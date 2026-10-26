//! Phase 4d — actor wrappers for the three network types + loss aggregator.
//!
//! Each network actor owns one instance of its network and handles a typed
//! request/response protocol. The sync [`crate::Brain`] facade is
//! unchanged; this module is purely additive — callers that want async
//! message-passing routing use [`BrainActorSystem::spawn`] and talk to
//! the returned handles.
//!
//! # Ownership
//!
//! Network state is **owned exclusively** by its actor. After
//! [`BrainActorSystem::spawn`] returns, the original [`crate::Brain`]
//! handle is no longer the sole authority on the network state —
//! callers should drive through the actor handles. A future 4e-style
//! integration can spawn the ensemble, drive multimodal training across
//! all three actors, read aggregated loss stats, and save the ensemble
//! without the sync facade being involved.
//!
//! # Loss aggregation
//!
//! Every training step emits a [`LossEvent`] onto the `"loss"` broadcast
//! topic. The [`LossAggregator`] actor subscribes to the topic and
//! maintains a per-source exponential moving average + most-recent value.
//! Callers `ask()` the aggregator for a snapshot.

use async_trait::async_trait;
use ndarray::Array1;
use nimcp_adaptive::AdaptiveNet;
use nimcp_core::{Error, Result};
use nimcp_lnn::{LnnNetwork, TrainParams};
use nimcp_scheduler::{Actor, ActorHandle, AskEnvelope, Context, Scheduler, TopicHandle};
use nimcp_snn::SnnNetwork;
use tokio::sync::broadcast;

// -------------------------------------------------------------------------
// Loss event + aggregator
// -------------------------------------------------------------------------

/// Which network produced a loss sample.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum LossSource {
    /// Adaptive MLP.
    Adaptive,
    /// SNN — reported as e.g. reward or average firing-rate deviation.
    /// The SNN actor chooses its own convention; the aggregator just
    /// tracks the scalar.
    Snn,
    /// LNN — sequence-MSE loss from `train_step_mse`.
    Lnn,
}

impl LossSource {
    /// Stable string name for logs / serialization.
    #[must_use]
    pub fn as_str(self) -> &'static str {
        match self {
            Self::Adaptive => "adaptive",
            Self::Snn => "snn",
            Self::Lnn => "lnn",
        }
    }
}

/// One loss sample. Published onto the `"loss"` broadcast topic by each
/// network actor on every training step.
#[derive(Debug, Clone, Copy)]
pub struct LossEvent {
    /// Which network produced the sample.
    pub source: LossSource,
    /// Scalar value — MSE, reward, or whatever the actor considers its
    /// training signal.
    pub value: f32,
    /// Monotonic step counter from the producing actor.
    pub step: u64,
}

/// Snapshot of the aggregator's per-source EMA + latest-value state.
#[derive(Debug, Clone, Copy, Default)]
pub struct LossStats {
    /// Most-recent adaptive loss, or `None` if the adaptive actor has
    /// not trained yet.
    pub adaptive_last: Option<f32>,
    /// EMA of adaptive losses (α = 0.1). `None` until first sample.
    pub adaptive_ema: Option<f32>,
    /// SNN latest value.
    pub snn_last: Option<f32>,
    /// SNN EMA.
    pub snn_ema: Option<f32>,
    /// LNN latest value.
    pub lnn_last: Option<f32>,
    /// LNN EMA.
    pub lnn_ema: Option<f32>,
    /// Total number of events aggregated.
    pub n_events: u64,
}

/// Aggregator actor: subscribes to the loss topic, maintains per-source
/// EMA + latest, answers [`LossStatsRequest`] asks.
pub struct LossAggregator {
    stats: LossStats,
    rx: broadcast::Receiver<LossEvent>,
    ema_alpha: f32,
}

impl LossAggregator {
    /// Construct — subscribes to `topic` so we start receiving events on
    /// the next actor spawn.
    #[must_use]
    pub fn new(topic: &TopicHandle<LossEvent>, ema_alpha: f32) -> Self {
        Self {
            stats: LossStats::default(),
            rx: topic.subscribe(),
            ema_alpha: ema_alpha.clamp(f32::EPSILON, 1.0),
        }
    }

    fn absorb(&mut self, ev: LossEvent) {
        let alpha = self.ema_alpha;
        match ev.source {
            LossSource::Adaptive => {
                self.stats.adaptive_last = Some(ev.value);
                self.stats.adaptive_ema = Some(match self.stats.adaptive_ema {
                    None => ev.value,
                    Some(prev) => (1.0 - alpha) * prev + alpha * ev.value,
                });
            }
            LossSource::Snn => {
                self.stats.snn_last = Some(ev.value);
                self.stats.snn_ema = Some(match self.stats.snn_ema {
                    None => ev.value,
                    Some(prev) => (1.0 - alpha) * prev + alpha * ev.value,
                });
            }
            LossSource::Lnn => {
                self.stats.lnn_last = Some(ev.value);
                self.stats.lnn_ema = Some(match self.stats.lnn_ema {
                    None => ev.value,
                    Some(prev) => (1.0 - alpha) * prev + alpha * ev.value,
                });
            }
        }
        self.stats.n_events = self.stats.n_events.saturating_add(1);
    }

    /// Drain every pending event from the broadcast receiver. `Lagged`
    /// means we fell behind; we just carry on with whatever's left.
    fn drain_pending(&mut self) {
        loop {
            match self.rx.try_recv() {
                Ok(ev) => self.absorb(ev),
                Err(broadcast::error::TryRecvError::Empty) => return,
                Err(broadcast::error::TryRecvError::Lagged(_)) => continue,
                Err(broadcast::error::TryRecvError::Closed) => return,
            }
        }
    }
}

/// Empty request payload — the aggregator ignores the request and just
/// returns the current stats. A stand-alone type (not `()`) so the
/// derive impls work cleanly.
#[derive(Debug, Clone, Copy, Default)]
pub struct LossStatsRequest;

#[async_trait]
impl Actor for LossAggregator {
    type Msg = AskEnvelope<LossStatsRequest, LossStats>;

    async fn handle(&mut self, msg: Self::Msg, _ctx: &mut Context) -> Result<()> {
        self.drain_pending();
        let stats = self.stats;
        let _ = msg.reply(stats);
        Ok(())
    }

    fn name(&self) -> &'static str {
        "loss_aggregator"
    }
}

// -------------------------------------------------------------------------
// Adaptive actor
// -------------------------------------------------------------------------

/// Typed request payload for the adaptive actor.
#[derive(Debug)]
pub enum AdaptiveRequest {
    /// Run one training step; reply is pre-update MSE loss.
    Learn {
        /// Input features (length = first layer width).
        features: Array1<f32>,
        /// Target (length = last layer width).
        target: Array1<f32>,
        /// Learning rate.
        lr: f32,
    },
    /// Pure forward pass.
    Predict {
        /// Input features.
        features: Array1<f32>,
    },
    /// Request the actor serialize its weights to an rkyv blob.
    SaveBytes,
    /// Replace weights from an rkyv blob.
    LoadBytes {
        /// Bytes previously returned by `SaveBytes`.
        bytes: Vec<u8>,
    },
}

/// Reply payload from the adaptive actor.
#[derive(Debug)]
pub enum AdaptiveReply {
    /// `Learn` → pre-update MSE.
    Loss(f32),
    /// `Predict` → output vector.
    Output(Array1<f32>),
    /// `SaveBytes` → rkyv-serialized weights.
    Bytes(Vec<u8>),
    /// `LoadBytes` outcome.
    Loaded(Result<()>),
}

/// Adaptive network actor.
pub struct AdaptiveActor {
    net: AdaptiveNet,
    loss_topic: TopicHandle<LossEvent>,
    step: u64,
}

impl AdaptiveActor {
    /// Construct with the network and a sender to the shared loss topic.
    #[must_use]
    pub fn new(net: AdaptiveNet, loss_topic: TopicHandle<LossEvent>) -> Self {
        Self {
            net,
            loss_topic,
            step: 0,
        }
    }
}

#[async_trait]
impl Actor for AdaptiveActor {
    type Msg = AskEnvelope<AdaptiveRequest, AdaptiveReply>;

    async fn handle(&mut self, msg: Self::Msg, _ctx: &mut Context) -> Result<()> {
        let (env, reply_tx) = (msg, ());
        let _ = reply_tx;
        // destructure into env to access request + reply channel
        let req_ref = env.request();
        let reply = match req_ref {
            AdaptiveRequest::Learn {
                features,
                target,
                lr,
            } => {
                let loss = self.net.learn(features, target, *lr);
                self.step = self.step.saturating_add(1);
                let _ = self.loss_topic.publish(LossEvent {
                    source: LossSource::Adaptive,
                    value: loss,
                    step: self.step,
                });
                AdaptiveReply::Loss(loss)
            }
            AdaptiveRequest::Predict { features } => {
                AdaptiveReply::Output(self.net.forward(features))
            }
            AdaptiveRequest::SaveBytes => {
                let bytes = self
                    .net
                    .save()
                    .map_err(|e| Error::Serialization(format!("adaptive save: {e:?}")))?;
                AdaptiveReply::Bytes(bytes)
            }
            AdaptiveRequest::LoadBytes { bytes } => {
                let r = self
                    .net
                    .load(bytes)
                    .map_err(|e| Error::Serialization(format!("adaptive load: {e:?}")));
                AdaptiveReply::Loaded(r)
            }
        };
        let _ = env.reply(reply);
        Ok(())
    }

    fn name(&self) -> &'static str {
        "adaptive_actor"
    }
}

// -------------------------------------------------------------------------
// LNN actor
// -------------------------------------------------------------------------

/// Typed request payload for the LNN actor.
#[derive(Debug)]
pub enum LnnRequest {
    /// Run the LNN over an input sequence; reply is the per-step readout.
    /// Resets hidden state at the start of the sequence.
    ForwardSequence {
        /// Input timesteps.
        inputs: Vec<Array1<f32>>,
    },
    /// Run one training step (MSE) on a sequence. Reply is `(loss, grad_norm)`.
    TrainStepMse {
        /// Input timesteps.
        inputs: Vec<Array1<f32>>,
        /// Target timesteps (same length).
        targets: Vec<Array1<f32>>,
        /// Hyperparameters.
        params: TrainParams,
    },
    /// Serialize the whole LNN as JSON bytes (round-trips weights +
    /// hyperparams, not transient state).
    SaveBytes,
    /// Replace the LNN from a JSON byte blob. Shape must match.
    LoadBytes {
        /// Bytes previously returned by `SaveBytes`.
        bytes: Vec<u8>,
    },
}

/// Reply payload from the LNN actor.
#[derive(Debug)]
pub enum LnnReply {
    /// `ForwardSequence` → per-step outputs.
    Outputs(Vec<Array1<f32>>),
    /// `TrainStepMse` → (loss, grad_norm).
    TrainStats(f32, f32),
    /// `SaveBytes` → serialized bytes.
    Bytes(Vec<u8>),
    /// `LoadBytes` outcome.
    Loaded(Result<()>),
}

/// LNN actor. Carries its own transient `LtcState` across forward calls.
pub struct LnnActor {
    net: LnnNetwork,
    state: Vec<nimcp_lnn::LtcState>,
    loss_topic: TopicHandle<LossEvent>,
    step: u64,
}

impl LnnActor {
    /// Construct from a network and a shared loss sender.
    #[must_use]
    pub fn new(net: LnnNetwork, loss_topic: TopicHandle<LossEvent>) -> Self {
        let state = net.new_state();
        Self {
            net,
            state,
            loss_topic,
            step: 0,
        }
    }
}

#[async_trait]
impl Actor for LnnActor {
    type Msg = AskEnvelope<LnnRequest, LnnReply>;

    async fn handle(&mut self, msg: Self::Msg, _ctx: &mut Context) -> Result<()> {
        let reply = match msg.request() {
            LnnRequest::ForwardSequence { inputs } => {
                self.state = self.net.new_state();
                let mut out = Vec::with_capacity(inputs.len());
                for u in inputs {
                    out.push(self.net.forward_step(&mut self.state, u));
                }
                LnnReply::Outputs(out)
            }
            LnnRequest::TrainStepMse {
                inputs,
                targets,
                params,
            } => {
                let (loss, gn) = nimcp_lnn::train_step_mse(&mut self.net, inputs, targets, params);
                self.step = self.step.saturating_add(1);
                let _ = self.loss_topic.publish(LossEvent {
                    source: LossSource::Lnn,
                    value: loss,
                    step: self.step,
                });
                LnnReply::TrainStats(loss, gn)
            }
            LnnRequest::SaveBytes => {
                let bytes = serde_json::to_vec(&self.net)
                    .map_err(|e| Error::Serialization(format!("lnn save: {e}")))?;
                LnnReply::Bytes(bytes)
            }
            LnnRequest::LoadBytes { bytes } => {
                let outcome: Result<()> = (|| {
                    let restored: LnnNetwork = serde_json::from_slice(bytes)
                        .map_err(|e| Error::Serialization(format!("lnn load: {e}")))?;
                    if restored.input_dim != self.net.input_dim
                        || restored.output_dim != self.net.output_dim
                        || restored.layers.len() != self.net.layers.len()
                    {
                        return Err(Error::Config(
                            "lnn load: shape mismatch with current actor".into(),
                        ));
                    }
                    self.net = restored;
                    self.state = self.net.new_state();
                    Ok(())
                })();
                LnnReply::Loaded(outcome)
            }
        };
        let _ = msg.reply(reply);
        Ok(())
    }

    fn name(&self) -> &'static str {
        "lnn_actor"
    }
}

// -------------------------------------------------------------------------
// SNN actor
// -------------------------------------------------------------------------

/// Typed request payload for the SNN actor.
#[derive(Debug)]
pub enum SnnRequest {
    /// One integration step. `external_i_syn` is per-population; the
    /// empty outer vec means "no external drive".
    Step {
        /// Per-population external drive. Use empty slices for pops with
        /// no external input.
        external_i_syn: Vec<Vec<f32>>,
        /// Reward signal (drives R-STDP modulation).
        reward: f32,
        /// Integration timestep, ms.
        dt_ms: f32,
    },
    /// Ask the actor for its current weight snapshot + rate EMAs.
    SnapshotBytes,
    /// Restore weights + rate EMAs from bytes produced by `SnapshotBytes`.
    LoadBytes {
        /// Serialized snapshot.
        bytes: Vec<u8>,
    },
}

/// Reply payload from the SNN actor.
#[derive(Debug)]
pub enum SnnReply {
    /// `Step` → total spikes this step + sequence index (monotonic step count).
    StepResult {
        /// Total spikes across every population in this step.
        n_spikes: u32,
        /// Actor-internal monotonic step counter.
        step: u64,
    },
    /// `SnapshotBytes` → JSON-serialized weight snapshot.
    Bytes(Vec<u8>),
    /// `LoadBytes` outcome.
    Loaded(Result<()>),
}

/// SNN actor.
pub struct SnnActor {
    net: SnnNetwork,
    loss_topic: TopicHandle<LossEvent>,
    step: u64,
}

impl SnnActor {
    /// Construct from a network and a shared loss sender.
    #[must_use]
    pub fn new(net: SnnNetwork, loss_topic: TopicHandle<LossEvent>) -> Self {
        Self {
            net,
            loss_topic,
            step: 0,
        }
    }
}

#[async_trait]
impl Actor for SnnActor {
    type Msg = AskEnvelope<SnnRequest, SnnReply>;

    async fn handle(&mut self, msg: Self::Msg, _ctx: &mut Context) -> Result<()> {
        let reply = match msg.request() {
            SnnRequest::Step {
                external_i_syn,
                reward,
                dt_ms,
            } => {
                // Convert `Vec<Vec<f32>>` to `Vec<&[f32]>` for the
                // underlying step signature.
                let slices: Vec<&[f32]> =
                    external_i_syn.iter().map(std::vec::Vec::as_slice).collect();
                let n_spikes = self.net.step(&slices, *reward, *dt_ms);
                self.step = self.step.saturating_add(1);
                // Report reward as the SNN "loss" channel value — it's the
                // scalar most directly analogous to adaptive MSE for
                // observability purposes.
                let _ = self.loss_topic.publish(LossEvent {
                    source: LossSource::Snn,
                    value: *reward,
                    step: self.step,
                });
                SnnReply::StepResult {
                    n_spikes,
                    step: self.step,
                }
            }
            SnnRequest::SnapshotBytes => {
                let snap = self.net.snapshot();
                let bytes = serde_json::to_vec(&snap)
                    .map_err(|e| Error::Serialization(format!("snn snapshot: {e}")))?;
                SnnReply::Bytes(bytes)
            }
            SnnRequest::LoadBytes { bytes } => {
                let outcome: Result<()> = (|| {
                    let snap: nimcp_snn::network::WeightSnapshot = serde_json::from_slice(bytes)
                        .map_err(|e| Error::Serialization(format!("snn load: {e}")))?;
                    if !self.net.restore(&snap) {
                        return Err(Error::Config("snn load: shape mismatch".into()));
                    }
                    Ok(())
                })();
                SnnReply::Loaded(outcome)
            }
        };
        let _ = msg.reply(reply);
        Ok(())
    }

    fn name(&self) -> &'static str {
        "snn_actor"
    }
}

// -------------------------------------------------------------------------
// Top-level actor system
// -------------------------------------------------------------------------

/// Handles returned by [`BrainActorSystem::spawn`] — one per spawned actor.
pub struct BrainActorHandles {
    /// Adaptive actor handle. Always present.
    pub adaptive: ActorHandle<AskEnvelope<AdaptiveRequest, AdaptiveReply>>,
    /// SNN actor handle. Present iff the brain was built with an SNN.
    pub snn: Option<ActorHandle<AskEnvelope<SnnRequest, SnnReply>>>,
    /// LNN actor handle. Present iff the brain was built with an LNN.
    pub lnn: Option<ActorHandle<AskEnvelope<LnnRequest, LnnReply>>>,
    /// Loss aggregator actor handle.
    pub loss: ActorHandle<AskEnvelope<LossStatsRequest, LossStats>>,
    /// Broadcast topic every actor publishes loss events onto. Exposed
    /// so external consumers (tests, dashboards) can subscribe too.
    pub loss_topic: TopicHandle<LossEvent>,
    /// Scheduler that owns the actors — keep alive for their lifetime.
    pub scheduler: Scheduler,
}

/// The three networks, passed in as-is to `spawn`.
pub struct BrainActorSystem {
    /// Adaptive network (required).
    pub adaptive: AdaptiveNet,
    /// Optional SNN.
    pub snn: Option<SnnNetwork>,
    /// Optional LNN.
    pub lnn: Option<LnnNetwork>,
}

impl BrainActorSystem {
    /// Spawn every actor on a new [`Scheduler`]. Returns handles to talk
    /// to them.
    ///
    /// The scheduler is embedded in [`BrainActorHandles`] — drop the
    /// handles only when you're done with every actor.
    pub fn spawn(self) -> BrainActorHandles {
        let mut scheduler = Scheduler::new(Default::default());

        // Loss topic — every actor publishes onto this; the aggregator
        // subscribes to its receiver at construction so no events are
        // missed once the actor loops start running.
        let loss_topic: TopicHandle<LossEvent> = scheduler.topic("loss");

        let aggregator = LossAggregator::new(&loss_topic, 0.1);
        let loss_handle = scheduler.spawn(aggregator);

        let adaptive_actor = AdaptiveActor::new(self.adaptive, loss_topic.clone());
        let adaptive_handle = scheduler.spawn(adaptive_actor);

        let snn_handle = self.snn.map(|snn| {
            let actor = SnnActor::new(snn, loss_topic.clone());
            scheduler.spawn(actor)
        });

        let lnn_handle = self.lnn.map(|lnn| {
            let actor = LnnActor::new(lnn, loss_topic.clone());
            scheduler.spawn(actor)
        });

        BrainActorHandles {
            adaptive: adaptive_handle,
            snn: snn_handle,
            lnn: lnn_handle,
            loss: loss_handle,
            loss_topic,
            scheduler,
        }
    }
}

// Re-export the ask trait for callers of this module. Callers use
// `brain::actors::AskExt::ask(handle, req).await` without depending on
// the scheduler crate directly.
pub use nimcp_scheduler::AskExt;

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use nimcp_adaptive::{Activation, AdaptiveConfig};
    use nimcp_lnn::{LnnConfig, LtcParams};
    use nimcp_plasticity::HomeostaticParams;
    use nimcp_snn::network::{EdgeSpec, PopulationSpec};
    use nimcp_snn::{LifParams, RstdpParams, SnnConfig};

    fn adaptive() -> AdaptiveNet {
        AdaptiveNet::new(AdaptiveConfig {
            layers: vec![4, 8, 2],
            rng_seed: 0x42,
            activation: Activation::Tanh,
        })
    }

    fn small_lnn() -> LnnNetwork {
        LnnNetwork::new(LnnConfig {
            input_dim: 3,
            output_dim: 1,
            layers: vec![LtcParams {
                n_in: 3,
                n_rec: 8,
                tau_init: 1.0,
                init_scale: 1.0,
            }],
            rng_seed: 0x11,
            dt_ms: 0.1,
            substrate: nimcp_lnn::LnnSubstrateCfg::default(),
            thalamic: None,
        })
        .expect("lnn build")
    }

    fn small_snn() -> SnnNetwork {
        SnnNetwork::new(SnnConfig {
            populations: vec![
                PopulationSpec {
                    name: "in".into(),
                    n_neurons: 32,
                    lif: LifParams::default(),
                    target_rate: 0.1,
                    homeostatic: HomeostaticParams::default(),
                    noise: nimcp_snn::NoiseConfig {
                        rate_hz: 0.0,
                        pulse_mv: 0.0,
                        kind: nimcp_snn::NoiseKind::default(),
                    },
                    depression: nimcp_snn::DepressionConfig {
                        inc: 0.0,
                        ..nimcp_snn::DepressionConfig::default()
                    },
                    adaptation_ahp: None,
                    adaptation_pump: None,
                    basket: None,
                    substrate: nimcp_snn::network::SnnSubstrateCfg::default(),
                },
                PopulationSpec {
                    name: "out".into(),
                    n_neurons: 32,
                    lif: LifParams::default(),
                    target_rate: 0.1,
                    homeostatic: HomeostaticParams::default(),
                    noise: nimcp_snn::NoiseConfig {
                        rate_hz: 0.0,
                        pulse_mv: 0.0,
                        kind: nimcp_snn::NoiseKind::default(),
                    },
                    depression: nimcp_snn::DepressionConfig {
                        inc: 0.0,
                        ..nimcp_snn::DepressionConfig::default()
                    },
                    adaptation_ahp: None,
                    adaptation_pump: None,
                    basket: None,
                    substrate: nimcp_snn::network::SnnSubstrateCfg::default(),
                },
            ],
            edges: vec![EdgeSpec {
                src: 0,
                dst: 1,
                fan_in: 8,
                weight_init: 1.0,
                weight_jitter: 0.2,
                rstdp: RstdpParams {
                    warmup_samples: 0,
                    w_max: 5.0,
                    ..RstdpParams::default()
                },
            }],
            rng_seed: 0x55,
            rate_ema_alpha: 0.05,
            reward_coupled_homeostatic: false,
            intrinsic_reward: nimcp_snn::IntrinsicRewardConfig::default(),
            thalamic: None,
            use_gpu_forward: false,
            device_assignment: Default::default(),
        })
        .expect("snn build")
    }

    #[tokio::test]
    async fn adaptive_actor_learn_and_predict() {
        let system = BrainActorSystem {
            adaptive: adaptive(),
            snn: None,
            lnn: None,
        };
        let handles = system.spawn();

        let x = Array1::from_vec(vec![0.1, 0.2, 0.3, 0.4]);
        let y = Array1::from_vec(vec![0.5, -0.5]);

        // Learn → loss reply.
        let reply = handles
            .adaptive
            .ask(AdaptiveRequest::Learn {
                features: x.clone(),
                target: y.clone(),
                lr: 0.05,
            })
            .await
            .expect("ask");
        match reply {
            AdaptiveReply::Loss(loss) => assert!(loss >= 0.0, "negative loss: {loss}"),
            other => panic!("expected Loss, got {other:?}"),
        }

        // Predict → output reply.
        let reply = handles
            .adaptive
            .ask(AdaptiveRequest::Predict {
                features: x.clone(),
            })
            .await
            .expect("ask");
        match reply {
            AdaptiveReply::Output(out) => assert_eq!(out.len(), 2),
            other => panic!("expected Output, got {other:?}"),
        }
    }

    #[tokio::test]
    async fn lnn_actor_trains_and_reports_loss() {
        let system = BrainActorSystem {
            adaptive: adaptive(),
            snn: None,
            lnn: Some(small_lnn()),
        };
        let handles = system.spawn();
        let lnn = handles.lnn.as_ref().expect("lnn spawned");

        let inputs: Vec<Array1<f32>> = (0..8)
            .map(|t| Array1::from_vec(vec![(t as f32 * 0.1).sin(), 0.2, -0.1]))
            .collect();
        let targets: Vec<Array1<f32>> = (0..8).map(|_| Array1::from_vec(vec![0.3])).collect();
        let params = TrainParams::default();

        let reply = lnn
            .ask(LnnRequest::TrainStepMse {
                inputs,
                targets,
                params,
            })
            .await
            .expect("ask");
        match reply {
            LnnReply::TrainStats(loss, gn) => {
                assert!(loss.is_finite());
                assert!(gn >= 0.0);
            }
            other => panic!("expected TrainStats, got {other:?}"),
        }
    }

    #[tokio::test]
    async fn snn_actor_steps_and_reports_step_count() {
        let system = BrainActorSystem {
            adaptive: adaptive(),
            snn: Some(small_snn()),
            lnn: None,
        };
        let handles = system.spawn();
        let snn = handles.snn.as_ref().expect("snn spawned");

        let drive: Vec<Vec<f32>> = vec![vec![500.0; 32], vec![]];
        for expected_step in 1..=5 {
            let reply = snn
                .ask(SnnRequest::Step {
                    external_i_syn: drive.clone(),
                    reward: 0.0,
                    dt_ms: 1.0,
                })
                .await
                .expect("ask");
            match reply {
                SnnReply::StepResult { step, .. } => {
                    assert_eq!(step, expected_step);
                }
                other => panic!("expected StepResult, got {other:?}"),
            }
        }
    }

    #[tokio::test]
    async fn loss_aggregator_sees_events_from_every_actor() {
        let system = BrainActorSystem {
            adaptive: adaptive(),
            snn: Some(small_snn()),
            lnn: Some(small_lnn()),
        };
        let handles = system.spawn();
        let lnn = handles.lnn.as_ref().expect("lnn spawned");
        let snn = handles.snn.as_ref().expect("snn spawned");

        // One training sample on each of the three networks.
        handles
            .adaptive
            .ask(AdaptiveRequest::Learn {
                features: Array1::from_vec(vec![0.1, 0.2, 0.3, 0.4]),
                target: Array1::from_vec(vec![0.5, -0.5]),
                lr: 0.05,
            })
            .await
            .expect("adaptive ask");
        lnn.ask(LnnRequest::TrainStepMse {
            inputs: vec![Array1::from_vec(vec![0.1, 0.2, 0.3])],
            targets: vec![Array1::from_vec(vec![0.5])],
            params: TrainParams::default(),
        })
        .await
        .expect("lnn ask");
        snn.ask(SnnRequest::Step {
            external_i_syn: vec![vec![500.0; 32], vec![]],
            reward: 0.2,
            dt_ms: 1.0,
        })
        .await
        .expect("snn ask");

        // Give the broadcast channel a tick to fan out before we ask the
        // aggregator to drain.
        tokio::task::yield_now().await;

        let stats = handles.loss.ask(LossStatsRequest).await.expect("loss ask");
        assert!(stats.adaptive_last.is_some(), "adaptive event missed");
        assert!(stats.lnn_last.is_some(), "lnn event missed");
        assert!(stats.snn_last.is_some(), "snn event missed");
        assert!(stats.n_events >= 3, "too few events: {}", stats.n_events);
    }

    #[tokio::test]
    async fn adaptive_actor_save_load_round_trip() {
        let system = BrainActorSystem {
            adaptive: adaptive(),
            snn: None,
            lnn: None,
        };
        let handles = system.spawn();

        let x = Array1::from_vec(vec![0.3, -0.2, 0.1, 0.7]);
        let y = Array1::from_vec(vec![-0.4, 0.6]);
        for _ in 0..10 {
            handles
                .adaptive
                .ask(AdaptiveRequest::Learn {
                    features: x.clone(),
                    target: y.clone(),
                    lr: 0.05,
                })
                .await
                .expect("learn");
        }
        let pred_a = match handles
            .adaptive
            .ask(AdaptiveRequest::Predict {
                features: x.clone(),
            })
            .await
            .expect("predict")
        {
            AdaptiveReply::Output(v) => v,
            _ => panic!("bad reply"),
        };
        let bytes = match handles
            .adaptive
            .ask(AdaptiveRequest::SaveBytes)
            .await
            .expect("save")
        {
            AdaptiveReply::Bytes(b) => b,
            _ => panic!("bad reply"),
        };

        // Spawn a fresh system, load bytes, verify prediction matches.
        let fresh = BrainActorSystem {
            adaptive: adaptive(),
            snn: None,
            lnn: None,
        };
        let h2 = fresh.spawn();
        match h2
            .adaptive
            .ask(AdaptiveRequest::LoadBytes { bytes })
            .await
            .expect("load")
        {
            AdaptiveReply::Loaded(r) => r.expect("load ok"),
            _ => panic!("bad reply"),
        };
        let pred_b = match h2
            .adaptive
            .ask(AdaptiveRequest::Predict { features: x })
            .await
            .expect("predict")
        {
            AdaptiveReply::Output(v) => v,
            _ => panic!("bad reply"),
        };
        for (a, b) in pred_a.iter().zip(pred_b.iter()) {
            assert!((a - b).abs() < 1e-6, "drift: {a} vs {b}");
        }
    }
}

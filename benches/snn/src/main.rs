//! Phase 3f bench — 1.8M-neuron SNN per-step latency.
//!
//! V2_PLAN Phase 3 exit criterion: "1.8M-neuron SNN runs at V1 throughput
//! rate." This binary builds an SNN at the V1-equivalent scale, runs a
//! warmup + measurement phase, and prints median per-step latency.
//!
//! # Configurations
//!
//! All sizes are CPU-friendly — no GPU path is exercised here. The three
//! named shapes trade off topology density against population count:
//!
//! | Name            | Populations | Neurons/pop | Fan-in | Total synapses |
//! |-----------------|-------------|-------------|--------|----------------|
//! | `small`         | 4           | 100_000     | 16     | ~6.4M          |
//! | `v1_target`     | 4           | 450_000     | 64     | ~115M          |
//! | `v1_hierarchy`  | 6           | 300_000     | 32     | ~58M           |
//!
//! `v1_target` is the Phase 3 exit configuration: 1.8M neurons total with
//! the fan-in density V1's lightweight SNN actually ran.
//!
//! # Methodology
//!
//! Each configuration runs `WARMUP_STEPS` untimed steps (to let page
//! faults settle and the rate-EMA warmup gate open), then `MEASURE_STEPS`
//! timed steps. Reports median + p95 + min + max in ms, plus throughput
//! as steps/sec and a derived "neurons-updated-per-second" figure —
//! that's the apples-to-apples number to compare against V1's reported
//! tick rate.
//!
//! Drive is a constant suprathreshold I_syn into population 0 only; the
//! rest of the network gets zero external drive and runs on propagated
//! spikes alone.

use std::time::Instant;

use nimcp_plasticity::HomeostaticParams;
use nimcp_snn::network::{EdgeSpec, PopulationSpec};
use nimcp_snn::{LifParams, RstdpParams, SnnConfig, SnnNetwork};

/// Steps run untimed before measurement begins.
const WARMUP_STEPS: usize = 20;
/// Timed steps for the latency distribution.
const MEASURE_STEPS: usize = 100;
/// Simulated timestep (ms) — matches V1's default.
const DT_MS: f32 = 1.0;

#[derive(Debug, Clone, Copy)]
struct Shape {
    name: &'static str,
    n_populations: usize,
    neurons_per_pop: u32,
    fan_in: u32,
}

const SHAPES: &[Shape] = &[
    Shape {
        name: "small",
        n_populations: 4,
        neurons_per_pop: 100_000,
        fan_in: 16,
    },
    Shape {
        name: "v1_hierarchy",
        n_populations: 6,
        neurons_per_pop: 300_000,
        fan_in: 32,
    },
    Shape {
        name: "v1_target",
        n_populations: 4,
        neurons_per_pop: 450_000,
        fan_in: 64,
    },
];

/// Whether the benchmark enables the Phase 3.5 stability mechanisms
/// (Poisson noise, adaptation AHP+pump, basket pool, short-term
/// depression, reward-coupled homeostatic). Controlled by the env var
/// `NIMCP_STABILITY=1`; default is off.
fn stability_on() -> bool {
    std::env::var("NIMCP_STABILITY")
        .ok()
        .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
        .unwrap_or(false)
}

fn build_config(shape: &Shape, seed: u64) -> SnnConfig {
    let lif = LifParams::default();
    let rstdp = RstdpParams {
        warmup_samples: 0, // benchmark doesn't care about plasticity correctness
        w_max: 10.0,
        ..RstdpParams::default()
    };

    let stability = stability_on();
    let populations: Vec<PopulationSpec> = (0..shape.n_populations)
        .map(|i| {
            if stability {
                PopulationSpec {
                    name: format!("pop_{i}"),
                    n_neurons: shape.neurons_per_pop,
                    lif,
                    target_rate: 0.05,
                    homeostatic: HomeostaticParams::default(),
                    // Master-tuned production defaults (20 Hz × 30 mV noise).
                    noise: nimcp_snn::NoiseConfig::default(),
                    depression: nimcp_snn::DepressionConfig::default(),
                    adaptation_ahp: Some(nimcp_snn::network::AdaptationCfg::default()),
                    adaptation_pump: Some(nimcp_snn::network::AdaptationCfg::pump_defaults()),
                    basket: Some(nimcp_snn::network::BasketCfg::default()),
                    substrate: nimcp_snn::network::SnnSubstrateCfg::default(),
                }
            } else {
                PopulationSpec {
                    name: format!("pop_{i}"),
                    n_neurons: shape.neurons_per_pop,
                    lif,
                    target_rate: 0.05,
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
                }
            }
        })
        .collect();

    // Feed-forward chain: pop_0 → pop_1 → ... → pop_{n-1}
    let edges: Vec<EdgeSpec> = (0..shape.n_populations - 1)
        .map(|i| EdgeSpec {
            src: i,
            dst: i + 1,
            fan_in: shape.fan_in,
            weight_init: 1.0,
            weight_jitter: 0.5,
            rstdp,
        })
        .collect();

    SnnConfig {
        populations,
        edges,
        rng_seed: seed,
        rate_ema_alpha: 0.01,
        reward_coupled_homeostatic: stability,
        intrinsic_reward: nimcp_snn::IntrinsicRewardConfig::default(),
        thalamic: None,
        // Opt-in GPU via NIMCP_GPU=1 env var. Only meaningful when
        // stability is off — the 9a GPU path falls back to CPU when
        // CPU-side prep (noise / adaptation / basket / substrate)
        // would have modified v_mem.
        use_gpu_forward: std::env::var("NIMCP_GPU").ok().as_deref() == Some("1"),
        device_assignment: Default::default(),
    }
}

fn percentile(sorted_ms: &[f64], p: f64) -> f64 {
    if sorted_ms.is_empty() {
        return f64::NAN;
    }
    let idx = ((sorted_ms.len() as f64 - 1.0) * p).round() as usize;
    sorted_ms[idx]
}

fn bench_shape(shape: &Shape) {
    let total_neurons = shape.n_populations as u64 * shape.neurons_per_pop as u64;
    let total_synapses = shape.n_populations.saturating_sub(1) as u64
        * shape.neurons_per_pop as u64
        * shape.fan_in as u64;

    println!(
        "\n=== {} — {} pops × {} neurons ({}M total), fan-in {} ({}M synapses) ===",
        shape.name,
        shape.n_populations,
        shape.neurons_per_pop,
        total_neurons / 1_000_000,
        shape.fan_in,
        total_synapses / 1_000_000,
    );

    let build_start = Instant::now();
    let config = build_config(shape, 0x1234_5678);
    let mut net = SnnNetwork::new(config).expect("build");
    let build_ms = build_start.elapsed().as_secs_f64() * 1e3;
    println!("  build: {build_ms:.1} ms");

    // Suprathreshold drive into pop 0 only.
    let drive: Vec<f32> = vec![1_000.0; shape.neurons_per_pop as usize];
    let empty: Vec<f32> = Vec::new();
    let mut slices: Vec<&[f32]> = Vec::with_capacity(shape.n_populations);
    slices.push(&drive);
    for _ in 1..shape.n_populations {
        slices.push(&empty);
    }

    // Warmup.
    for _ in 0..WARMUP_STEPS {
        net.step(&slices, 0.0, DT_MS);
    }

    // Measurement.
    let mut per_step_ms: Vec<f64> = Vec::with_capacity(MEASURE_STEPS);
    let mut total_spikes: u64 = 0;
    let measure_start = Instant::now();
    for _ in 0..MEASURE_STEPS {
        let t = Instant::now();
        let n_spikes = net.step(&slices, 0.0, DT_MS);
        per_step_ms.push(t.elapsed().as_secs_f64() * 1e3);
        total_spikes += n_spikes as u64;
    }
    let measure_ms = measure_start.elapsed().as_secs_f64() * 1e3;

    per_step_ms.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let median = percentile(&per_step_ms, 0.5);
    let p95 = percentile(&per_step_ms, 0.95);
    let min = per_step_ms.first().copied().unwrap_or(0.0);
    let max = per_step_ms.last().copied().unwrap_or(0.0);

    let steps_per_sec = MEASURE_STEPS as f64 / (measure_ms / 1e3);
    let neurons_per_sec = steps_per_sec * total_neurons as f64;
    let avg_spikes = total_spikes as f64 / MEASURE_STEPS as f64;

    println!("  per-step (ms):  median {median:.2}  p95 {p95:.2}  min {min:.2}  max {max:.2}");
    println!(
        "  throughput:     {steps_per_sec:.1} steps/sec, {:.1}M neuron-updates/sec",
        neurons_per_sec / 1e6
    );
    println!("  activity:       {avg_spikes:.0} spikes/step (avg)");
}

fn main() {
    println!("NIMCP V2 — Phase 3f SNN benchmark (CPU path)");
    println!("  warmup {WARMUP_STEPS} steps, measure {MEASURE_STEPS} steps, dt_ms = {DT_MS}");
    println!(
        "  stability mechanisms: {}",
        if stability_on() {
            "ON (noise + adaptation + basket + depression + reward-coupled homeostatic)"
        } else {
            "OFF (Phase 3 defaults)"
        }
    );

    for shape in SHAPES {
        bench_shape(shape);
    }
}

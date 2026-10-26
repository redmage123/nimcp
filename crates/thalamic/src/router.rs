//! Thalamic router — Hebbian route weights + channel registry.
//!
//! The router owns the shared state across channels:
//! - Per-`(source, dest)` Hebbian weight reinforced by coactivity.
//! - Registry of open channels (so the brain can broadcast attention
//!   changes across all sources).
//!
//! V1's full router had a priority queue + worker thread + signal
//! buffering. V2 Rust keeps the same logical contract but defers the
//! queue / thread model to the scheduler crate — each submit just
//! bumps Hebbian counters; the actual signal delivery happens in the
//! destination actor's mailbox.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::channel::ThalamicChannel;

/// Router error cases. `ChannelFull` mirrors V1's "too many open
/// channels" condition.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum ThalamicError {
    /// Attempt to open a channel for a source that already has one.
    #[error("source {0} already has an open channel")]
    DuplicateSource(u32),
    /// Too many channels open simultaneously.
    #[error("router at max_channels={0}")]
    ChannelFull(u32),
}

/// Tuning knobs.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct ThalamicRouterConfig {
    /// Max simultaneous open channels. Default `256`.
    pub max_channels: u32,
    /// Hebbian LR. Default `0.01` — one coactive submit bumps the
    /// route weight by ~1%.
    pub hebbian_lr: f32,
    /// Passive decay applied each tick to all Hebbian weights. Default
    /// `0.001` — slow forgetting.
    pub hebbian_decay: f32,
    /// Upper bound on Hebbian weight. Default `2.0` — reinforced
    /// routes carry up to 2× attention gain on top of the channel's
    /// attention_weights setting.
    pub hebbian_max: f32,
}

impl Default for ThalamicRouterConfig {
    fn default() -> Self {
        Self {
            max_channels: 256,
            hebbian_lr: 0.01,
            hebbian_decay: 0.001,
            hebbian_max: 2.0,
        }
    }
}

/// Shared thalamic state across all channels.
///
/// Channel handles are stored as `(source_id -> ThalamicChannel)` for
/// quick lookup during broadcast attention updates; adapters typically
/// hold their own copy via [`ThalamicRouter::open_channel`] and read/
/// write through that.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ThalamicRouter {
    config: ThalamicRouterConfig,
    /// All open channels, keyed by source_id.
    channels: HashMap<u32, ThalamicChannel>,
    /// Hebbian weights. Key = `(source_id, dest_id)`. Missing entry
    /// is treated as `1.0` — unreinforced routes pass through at
    /// attention × 1.0.
    hebbian: HashMap<(u32, u32), f32>,
}

impl ThalamicRouter {
    /// Construct an empty router.
    #[must_use]
    pub fn new(config: ThalamicRouterConfig) -> Self {
        Self {
            config,
            channels: HashMap::new(),
            hebbian: HashMap::new(),
        }
    }

    /// Open a channel for a source. Fails if source already open or
    /// router is at `max_channels`.
    pub fn open_channel(
        &mut self,
        source_id: u32,
        destinations: &[u32],
    ) -> Result<ThalamicChannel, ThalamicError> {
        if self.channels.len() >= self.config.max_channels as usize {
            return Err(ThalamicError::ChannelFull(self.config.max_channels));
        }
        if self.channels.contains_key(&source_id) {
            return Err(ThalamicError::DuplicateSource(source_id));
        }
        let ch = ThalamicChannel::new(source_id, destinations)
            .ok_or(ThalamicError::ChannelFull(destinations.len() as u32))?;
        self.channels.insert(source_id, ch);
        Ok(ch)
    }

    /// Close a source's channel. Returns true if a channel was removed.
    pub fn close_channel(&mut self, source_id: u32) -> bool {
        self.channels.remove(&source_id).is_some()
    }

    /// Read the effective gain for a source→dest route:
    ///
    /// ```text
    ///   effective = channel.attention_weights[dest] × hebbian[(src, dest)]
    /// ```
    ///
    /// Missing Hebbian entry = `1.0`. Missing channel = `0.0` (closed
    /// source cannot transmit). Result clamped to `[0, hebbian_max]`.
    #[must_use]
    pub fn effective_gain(&self, source_id: u32, dest_id: u32) -> f32 {
        let Some(ch) = self.channels.get(&source_id) else {
            return 0.0;
        };
        let attn = ch.get_gate(dest_id);
        let hebb = self
            .hebbian
            .get(&(source_id, dest_id))
            .copied()
            .unwrap_or(1.0);
        (attn * hebb).clamp(0.0, self.config.hebbian_max)
    }

    /// Override an attention gate. Updates both the router's registry
    /// and the caller's cached channel copy via the returned value.
    pub fn set_attention(&mut self, source_id: u32, dest_id: u32, gate: f32) -> bool {
        let Some(ch) = self.channels.get_mut(&source_id) else {
            return false;
        };
        ch.set_gate(dest_id, gate)
    }

    /// Per-step Hebbian update + decay.
    ///
    /// For each source that submitted signal this step (per its own
    /// `record_submit` count), reinforce the `(source, dest)` weight
    /// proportionally to submission intensity × the destination's
    /// attention weight. Then apply passive decay to every Hebbian
    /// entry.
    pub fn tick(&mut self) {
        let lr = self.config.hebbian_lr;
        let max = self.config.hebbian_max;
        let decay_factor = 1.0 - self.config.hebbian_decay;

        // Phase 1: reinforce from current channel submits.
        for (source_id, ch) in self.channels.iter_mut() {
            if ch.submits_this_step == 0 {
                ch.tick_stats();
                continue;
            }
            #[allow(clippy::cast_precision_loss)]
            let intensity = ch.submits_this_step as f32;
            for i in 0..ch.n_destinations as usize {
                let dest_id = ch.destinations[i];
                let attn = ch.attention_weights[i];
                let key = (*source_id, dest_id);
                let prev = self.hebbian.get(&key).copied().unwrap_or(1.0);
                let new = (prev + lr * intensity * attn).clamp(0.0, max);
                self.hebbian.insert(key, new);
            }
            ch.tick_stats();
        }

        // Phase 2: passive decay toward 1.0 (neutral) — not toward 0,
        // so unreinforced routes stay at normal gain.
        if self.config.hebbian_decay > 0.0 {
            for v in self.hebbian.values_mut() {
                *v = 1.0 + (*v - 1.0) * decay_factor;
            }
        }
    }

    /// Number of open channels (diagnostic).
    #[must_use]
    pub fn n_channels(&self) -> usize {
        self.channels.len()
    }

    /// Access a specific source's channel, if open.
    #[must_use]
    pub fn channel(&self, source_id: u32) -> Option<&ThalamicChannel> {
        self.channels.get(&source_id)
    }

    /// Record that a source submitted a signal — bumps the channel's
    /// `submits_this_step` counter so `tick()` can reinforce.
    pub fn record_submit(&mut self, source_id: u32) -> bool {
        let Some(ch) = self.channels.get_mut(&source_id) else {
            return false;
        };
        ch.record_submit();
        true
    }

    /// Current Hebbian weight for a route. Returns `1.0` (neutral) if
    /// never reinforced.
    #[must_use]
    pub fn hebbian_weight(&self, source_id: u32, dest_id: u32) -> f32 {
        self.hebbian
            .get(&(source_id, dest_id))
            .copied()
            .unwrap_or(1.0)
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn open_and_close_channel() {
        let mut r = ThalamicRouter::new(ThalamicRouterConfig::default());
        let _ch = r.open_channel(1, &[10, 20]).unwrap();
        assert_eq!(r.n_channels(), 1);
        assert!(r.close_channel(1));
        assert_eq!(r.n_channels(), 0);
    }

    #[test]
    fn duplicate_source_fails() {
        let mut r = ThalamicRouter::new(ThalamicRouterConfig::default());
        r.open_channel(1, &[10]).unwrap();
        assert_eq!(
            r.open_channel(1, &[10]).unwrap_err(),
            ThalamicError::DuplicateSource(1)
        );
    }

    #[test]
    fn max_channels_fails() {
        let cfg = ThalamicRouterConfig {
            max_channels: 2,
            ..Default::default()
        };
        let mut r = ThalamicRouter::new(cfg);
        r.open_channel(1, &[10]).unwrap();
        r.open_channel(2, &[10]).unwrap();
        assert!(matches!(
            r.open_channel(3, &[10]),
            Err(ThalamicError::ChannelFull(2))
        ));
    }

    #[test]
    fn effective_gain_product_of_attention_and_hebbian() {
        let mut r = ThalamicRouter::new(ThalamicRouterConfig::default());
        r.open_channel(1, &[10, 20]).unwrap();
        // Default: attention 1.0, hebbian 1.0 → gain 1.0.
        assert_eq!(r.effective_gain(1, 10), 1.0);

        r.set_attention(1, 10, 0.5);
        assert_eq!(r.effective_gain(1, 10), 0.5);
    }

    #[test]
    fn effective_gain_closed_source_is_zero() {
        let r = ThalamicRouter::new(ThalamicRouterConfig::default());
        assert_eq!(r.effective_gain(99, 1), 0.0);
    }

    #[test]
    fn effective_gain_unknown_dest_uses_channel_default() {
        let mut r = ThalamicRouter::new(ThalamicRouterConfig::default());
        r.open_channel(1, &[10]).unwrap();
        // dest 99 not in channel → get_gate returns 1.0 → gain 1.0.
        assert_eq!(r.effective_gain(1, 99), 1.0);
    }

    #[test]
    fn tick_reinforces_submitted_routes() {
        let cfg = ThalamicRouterConfig {
            hebbian_lr: 0.1,
            hebbian_decay: 0.0,
            ..Default::default()
        };
        let mut r = ThalamicRouter::new(cfg);
        r.open_channel(1, &[10]).unwrap();
        r.record_submit(1);
        r.record_submit(1);
        r.tick();
        // Initial 1.0 + 0.1 * 2 * 1.0 = 1.2
        assert!((r.hebbian_weight(1, 10) - 1.2).abs() < 1e-5);
    }

    #[test]
    fn tick_passive_decay_pulls_toward_one() {
        let cfg = ThalamicRouterConfig {
            hebbian_lr: 0.0,
            hebbian_decay: 0.5,
            ..Default::default()
        };
        let mut r = ThalamicRouter::new(cfg);
        r.open_channel(1, &[10]).unwrap();
        // Manually set a high weight.
        r.hebbian.insert((1, 10), 2.0);
        r.tick();
        // After decay 0.5: 1.0 + (2.0 - 1.0) * 0.5 = 1.5
        assert!((r.hebbian_weight(1, 10) - 1.5).abs() < 1e-5);
    }

    #[test]
    fn tick_caps_at_hebbian_max() {
        let cfg = ThalamicRouterConfig {
            hebbian_lr: 10.0,
            hebbian_decay: 0.0,
            hebbian_max: 2.0,
            ..Default::default()
        };
        let mut r = ThalamicRouter::new(cfg);
        r.open_channel(1, &[10]).unwrap();
        r.record_submit(1);
        r.tick();
        assert_eq!(r.hebbian_weight(1, 10), 2.0);
    }

    #[test]
    fn tick_resets_submit_counters() {
        let mut r = ThalamicRouter::new(ThalamicRouterConfig::default());
        r.open_channel(1, &[10]).unwrap();
        r.record_submit(1);
        r.tick();
        assert_eq!(r.channel(1).unwrap().submits_this_step, 0);
    }

    #[test]
    fn record_submit_on_unknown_source_fails_gracefully() {
        let mut r = ThalamicRouter::new(ThalamicRouterConfig::default());
        assert!(!r.record_submit(99));
    }
}

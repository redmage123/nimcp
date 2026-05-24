//! Working memory — a capacity-bounded, salience-decayed store of
//! feature vectors. Faithful port of V1's standalone `working_memory`
//! core (`src/cognitive/working_memory/`), which V1's own header
//! advertises as having no brain dependencies.
//!
//! V1 used parallel arrays; V2 uses a `Vec<WmItem>`. The salience decay
//! is exponential — `s ← s · exp(−Δt/τ)` — with an attention-refresh flag
//! that skips one decay tick, and eviction below `min_salience`. Adding to
//! a full store evicts the lowest-salience item.
//!
//! Time is **injected** (`now_ms` parameters) rather than read from a
//! clock, so the store is deterministic and serde round-trips exactly —
//! the brain supplies its own (possibly virtual) monotonic clock.

use serde::{Deserialize, Serialize};

/// Default slot count (V1 `working_memory_default_config`).
pub const DEFAULT_CAPACITY: usize = 7;
/// Default decay time constant, ms.
pub const DEFAULT_DECAY_TAU_MS: f32 = 1000.0;
/// Default eviction floor.
pub const DEFAULT_MIN_SALIENCE: f32 = 0.01;
/// Decay exponent floor — avoids `exp` underflow churn (V1 `MIN_DECAY_EXPONENT`).
const MIN_DECAY_EXPONENT: f32 = -80.0;

/// One working-memory item.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WmItem {
    /// Feature vector (deep-copied on add).
    pub data: Vec<f32>,
    /// Salience in `[0, 1]`.
    pub salience: f32,
    /// Last-touch time (ms), for decay.
    pub timestamp_ms: u64,
    /// Rehearsal flag — skips the next decay tick, then clears.
    pub refreshed: bool,
}

/// Capacity-bounded salience store.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkingMemory {
    items: Vec<WmItem>,
    capacity: usize,
    decay_tau_ms: f32,
    min_salience: f32,
    // Diagnostic counters.
    total_additions: u64,
    total_evictions: u64,
    total_refreshes: u64,
    total_decay_removals: u64,
}

impl Default for WorkingMemory {
    fn default() -> Self {
        Self::with_config(DEFAULT_CAPACITY, DEFAULT_DECAY_TAU_MS, DEFAULT_MIN_SALIENCE)
    }
}

impl WorkingMemory {
    /// New store with explicit config. `capacity` is floored at 1; the
    /// decay constant is floored at a small positive value.
    #[must_use]
    pub fn with_config(capacity: usize, decay_tau_ms: f32, min_salience: f32) -> Self {
        Self {
            items: Vec::new(),
            capacity: capacity.max(1),
            decay_tau_ms: decay_tau_ms.max(1.0),
            min_salience: min_salience.clamp(0.0, 1.0),
            total_additions: 0,
            total_evictions: 0,
            total_refreshes: 0,
            total_decay_removals: 0,
        }
    }

    /// Add a feature vector at `salience` (clamped to `[0, 1]`), stamped
    /// `now_ms`. If the store is full, the lowest-salience item is evicted
    /// first. Returns `true` (V1 returns `false` only on alloc failure).
    pub fn add(&mut self, data: &[f32], salience: f32, now_ms: u64) -> bool {
        if self.items.len() >= self.capacity {
            self.evict_lowest();
        }
        self.items.push(WmItem {
            data: data.to_vec(),
            salience: salience.clamp(0.0, 1.0),
            timestamp_ms: now_ms,
            refreshed: false,
        });
        self.total_additions += 1;
        true
    }

    fn evict_lowest(&mut self) {
        if let Some((idx, _)) = self
            .items
            .iter()
            .enumerate()
            .min_by(|(_, a), (_, b)| a.salience.total_cmp(&b.salience))
        {
            self.items.remove(idx);
            self.total_evictions += 1;
        }
    }

    /// Feature vector at `index` (read-only).
    #[must_use]
    pub fn get(&self, index: usize) -> Option<&[f32]> {
        self.items.get(index).map(|i| i.data.as_slice())
    }

    /// Salience at `index`.
    #[must_use]
    pub fn get_salience(&self, index: usize) -> Option<f32> {
        self.items.get(index).map(|i| i.salience)
    }

    /// Set salience at `index` (clamped). Returns `true` if found.
    pub fn set_salience(&mut self, index: usize, salience: f32) -> bool {
        if let Some(i) = self.items.get_mut(index) {
            i.salience = salience.clamp(0.0, 1.0);
            true
        } else {
            false
        }
    }

    /// Read-only view of all items (oldest-first by insertion).
    #[must_use]
    pub fn items(&self) -> &[WmItem] {
        &self.items
    }

    /// Active item count.
    #[must_use]
    pub fn size(&self) -> usize {
        self.items.len()
    }

    /// Slot capacity.
    #[must_use]
    pub fn capacity(&self) -> usize {
        self.capacity
    }

    #[must_use]
    pub fn is_full(&self) -> bool {
        self.items.len() >= self.capacity
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.items.is_empty()
    }

    /// `size / capacity`.
    #[must_use]
    pub fn utilization(&self) -> f32 {
        #[allow(clippy::cast_precision_loss)]
        {
            self.items.len() as f32 / self.capacity as f32
        }
    }

    /// Mark an item rehearsed (skips the next decay tick) and restamp it.
    /// Returns `true` if found.
    pub fn refresh(&mut self, index: usize, now_ms: u64) -> bool {
        if let Some(i) = self.items.get_mut(index) {
            i.refreshed = true;
            i.timestamp_ms = now_ms;
            self.total_refreshes += 1;
            true
        } else {
            false
        }
    }

    /// Exponential salience decay (V1 `working_memory_decay`): for each
    /// item not rehearsed this tick, `s ← s · exp(−Δt/τ)`; rehearsed items
    /// consume their flag and skip; items falling below `min_salience` are
    /// evicted. Returns the number evicted.
    pub fn decay(&mut self, now_ms: u64) -> u32 {
        let tau = self.decay_tau_ms;
        let min = self.min_salience;
        let before = self.items.len();
        for item in &mut self.items {
            if item.refreshed {
                item.refreshed = false;
                continue;
            }
            #[allow(clippy::cast_precision_loss)]
            let elapsed = now_ms.saturating_sub(item.timestamp_ms) as f32;
            let exponent = (-elapsed / tau).max(MIN_DECAY_EXPONENT);
            item.salience *= exponent.exp();
        }
        self.items.retain(|i| i.salience >= min);
        let removed = (before - self.items.len()) as u32;
        self.total_decay_removals += u64::from(removed);
        removed
    }

    /// Remove the item at `index`. Returns `true` if it existed.
    pub fn remove(&mut self, index: usize) -> bool {
        if index < self.items.len() {
            self.items.remove(index);
            true
        } else {
            false
        }
    }

    /// Drop all items.
    pub fn clear(&mut self) {
        self.items.clear();
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn add_and_get() {
        let mut wm = WorkingMemory::default();
        assert!(wm.add(&[1.0, 2.0, 3.0], 0.8, 0));
        assert_eq!(wm.size(), 1);
        assert_eq!(wm.get(0), Some([1.0, 2.0, 3.0].as_slice()));
        assert_eq!(wm.get_salience(0), Some(0.8));
    }

    #[test]
    fn salience_is_clamped() {
        let mut wm = WorkingMemory::default();
        wm.add(&[0.0], 5.0, 0);
        assert_eq!(wm.get_salience(0), Some(1.0));
        wm.set_salience(0, -3.0);
        assert_eq!(wm.get_salience(0), Some(0.0));
    }

    #[test]
    fn full_store_evicts_lowest_salience() {
        let mut wm = WorkingMemory::with_config(2, 1000.0, 0.01);
        wm.add(&[1.0], 0.9, 0);
        wm.add(&[2.0], 0.2, 0); // lowest
        wm.add(&[3.0], 0.5, 0); // evicts the 0.2 item
        assert_eq!(wm.size(), 2);
        let sals: Vec<f32> = wm.items().iter().map(|i| i.salience).collect();
        assert!(sals.contains(&0.9));
        assert!(sals.contains(&0.5));
        assert!(!sals.contains(&0.2));
        assert_eq!(wm.total_evictions, 1);
    }

    #[test]
    fn decay_reduces_salience() {
        let mut wm = WorkingMemory::with_config(4, 1000.0, 0.0);
        wm.add(&[1.0], 1.0, 0);
        // After one τ (1000 ms): s = 1.0 * exp(-1) ≈ 0.368.
        wm.decay(1000);
        let s = wm.get_salience(0).unwrap();
        assert!((s - (-1.0_f32).exp()).abs() < 1e-5, "got {s}");
    }

    #[test]
    fn decay_evicts_below_floor() {
        let mut wm = WorkingMemory::with_config(4, 1000.0, 0.5);
        wm.add(&[1.0], 0.6, 0);
        let removed = wm.decay(2000); // 0.6 * exp(-2) ≈ 0.081 < 0.5
        assert_eq!(removed, 1);
        assert!(wm.is_empty());
    }

    #[test]
    fn refresh_skips_one_decay_tick() {
        let mut wm = WorkingMemory::with_config(4, 1000.0, 0.0);
        wm.add(&[1.0], 1.0, 0);
        wm.refresh(0, 0);
        wm.decay(5000); // would normally crush it, but refreshed → skipped
        assert_eq!(wm.get_salience(0), Some(1.0));
        // Next tick decays normally.
        wm.decay(6000);
        assert!(wm.get_salience(0).unwrap() < 1.0);
    }

    #[test]
    fn remove_and_clear() {
        let mut wm = WorkingMemory::default();
        wm.add(&[1.0], 0.5, 0);
        wm.add(&[2.0], 0.5, 0);
        assert!(wm.remove(0));
        assert_eq!(wm.size(), 1);
        wm.clear();
        assert!(wm.is_empty());
        assert!(!wm.remove(0));
    }

    #[test]
    fn serde_round_trip() {
        let mut wm = WorkingMemory::with_config(5, 800.0, 0.05);
        wm.add(&[1.0, 2.0], 0.7, 100);
        let json = serde_json::to_string(&wm).unwrap();
        let back: WorkingMemory = serde_json::from_str(&json).unwrap();
        assert_eq!(back.size(), 1);
        assert_eq!(back.capacity(), 5);
        assert_eq!(back.get(0), Some([1.0, 2.0].as_slice()));
        assert_eq!(back.get_salience(0), Some(0.7));
    }
}

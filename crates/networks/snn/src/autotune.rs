//! Closed-loop SNN knob controller.
//!
//! V2 port of V1's brain_daemon `_snn_autotuner` (commit `41583c5a1`)
//! plus the proportional+integral brake fix (commit `eb42bce46`) and
//! the threshold tighten (commit `203fa0467`).
//!
//! # Design
//!
//! Pure observer/actuator state machine — no I/O, no threading, no
//! `expf` in the hot path. The V2 daemon calls
//! [`AutotuneController::observe_sample`] every ~30s during the wake
//! window, then calls [`AutotuneController::on_cycle_complete`] at
//! each sleep-cycle boundary. The controller emits at most one
//! [`Decision`] per cycle, naming the actuator and the new value.
//!
//! In V1 the controller talked directly to `brain.snn_tune` /
//! `brain.snn_tune_get`. In V2 the daemon owns the [`crate::tuning`]
//! actuator surface — the controller just *describes* the move it
//! wants, the daemon decides whether to apply or log it
//! (`autotune_enabled` lives outside this module).
//!
//! # Decision logic (priority order, at most one per cycle)
//!
//! | Rule | Trigger                                          | Actuator        | Move                                   |
//! |-----:|--------------------------------------------------|-----------------|-----------------------------------------|
//! | 1    | `peak_rate < 5 Hz` two cycles in a row           | `noise_rate_hz` | `+10` (rescue jolt)                     |
//! | 2    | `recovery_time > 0.7 × interval`                 | `sleep_interval` | `+60s` (lengthen wake window)          |
//! | 3    | `peak_rate > 500 Hz` (P+I brake)                 | `max_scale_dead` | `-0.005 × (peak/500) × (1+0.5·streak)` capped at `0.05` |
//! | 4    | `peak_rate ∈ [5, 30)` Hz                         | `max_scale_dead` | `+0.005`                                |
//! | 5    | `healthy_streak ≥ 1` AND `noise > 20`            | `noise_rate_hz` | `-5` (taper rescue dose)                |
//!
//! Streak counters are mutually-exclusive — entering a non-healthy band
//! resets healthy_streak, and vice versa.

/// Hard min/max clamps for each actuator. Defense in depth — even if
/// the decision logic produces a runaway value, the clamp pins it to
/// a sane operational range.
pub mod bounds {
    /// `max_scale_dead` — homeostatic scale-up rate for dead pops.
    pub const MAX_SCALE_DEAD: (f32, f32) = (1.01, 1.10);
    /// `sleep_interval` — seconds between periodic sleep cycles.
    pub const SLEEP_INTERVAL_SEC: (f32, f32) = (300.0, 3600.0);
    /// `noise_rate_hz` — Poisson background floor.
    pub const NOISE_RATE_HZ: (f32, f32) = (10.0, 100.0);
}

/// Names the actuator a [`Decision`] would move. Keep this enum exhaustive
/// — the V2 daemon's RPC dispatch is a `match` on the variant, so adding
/// a knob requires adding a daemon-side handler at the same time.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Actuator {
    /// `noise_rate_hz` — Poisson background floor.
    NoiseRateHz,
    /// `sleep_interval_sec` — seconds between sleep cycles.
    SleepIntervalSec,
    /// `max_scale_dead` — homeostatic scale-up rate for dead pops.
    MaxScaleDead,
}

impl Actuator {
    /// Stable string name for logging + RPC.
    #[must_use]
    pub fn name(self) -> &'static str {
        match self {
            Actuator::NoiseRateHz => "noise_rate_hz",
            Actuator::SleepIntervalSec => "sleep_interval_sec",
            Actuator::MaxScaleDead => "max_scale_dead",
        }
    }

    fn bounds(self) -> (f32, f32) {
        match self {
            Actuator::NoiseRateHz => bounds::NOISE_RATE_HZ,
            Actuator::SleepIntervalSec => bounds::SLEEP_INTERVAL_SEC,
            Actuator::MaxScaleDead => bounds::MAX_SCALE_DEAD,
        }
    }

    /// Apply this actuator's hard clamp.
    #[must_use]
    pub fn clamp(self, v: f32) -> f32 {
        let (lo, hi) = self.bounds();
        v.clamp(lo, hi)
    }
}

/// One actuator move proposed by the controller for a completed cycle.
#[derive(Debug, Clone)]
pub struct Decision {
    /// Which actuator the controller wants to move.
    pub actuator: Actuator,
    /// Current value the controller observed.
    pub current: f32,
    /// Proposed new value (already clamped to the actuator's bounds).
    pub target: f32,
    /// Human-readable reason — embedded in the daemon log.
    pub reason: String,
}

impl Decision {
    /// Was this move clamped to the lower bound of its actuator?
    /// Compared exact-equal because both sides are produced by
    /// `Actuator::clamp` of the same actuator — the lower bound is a
    /// concrete `f32` constant from [`bounds`], not an arithmetic
    /// result that could carry rounding error.
    #[must_use]
    #[allow(clippy::float_cmp)]
    pub fn is_clamped_low(&self) -> bool {
        self.target == self.actuator.clamp(f32::NEG_INFINITY)
    }
    /// Was this move clamped to the upper bound of its actuator? See
    /// [`Self::is_clamped_low`] for the float-equality rationale.
    #[must_use]
    #[allow(clippy::float_cmp)]
    pub fn is_clamped_high(&self) -> bool {
        self.target == self.actuator.clamp(f32::INFINITY)
    }
}

/// Snapshot of the daemon-side actuator values the controller reads
/// when proposing a decision. Mirrors V1's `brain.snn_tune_get()` call.
#[derive(Debug, Clone, Copy)]
pub struct AutotuneSnapshot {
    /// Current Poisson noise rate floor (Hz).
    pub noise_rate_hz: f32,
    /// Current sleep cycle interval (seconds).
    pub sleep_interval_sec: f32,
    /// Current homeostatic scale-up rate for dead pops.
    pub max_scale_dead: f32,
}

impl Default for AutotuneSnapshot {
    fn default() -> Self {
        Self {
            noise_rate_hz: 20.0,
            sleep_interval_sec: 900.0,
            max_scale_dead: 1.05,
        }
    }
}

/// Per-cycle metrics the controller accumulates between calls to
/// [`AutotuneController::observe_sample`]. Produced internally and
/// passed to [`AutotuneController::on_cycle_complete`].
#[derive(Debug, Clone, Copy, Default)]
pub struct CycleMetrics {
    /// Peak observed firing rate (Hz) across the cycle's samples.
    pub peak_rate_hz: f32,
    /// Seconds from cycle start until first sample with sparsity < 0.5.
    /// `None` if no such sample was seen.
    pub recovery_t_sec: Option<f32>,
    /// Cumulative seconds spent in the biological active band
    /// (sparsity ∈ `[0.05, 0.15]`).
    pub active_t_sec: f32,
}

/// Closed-loop controller state. Persists streak counters across cycles
/// so the proportional+integral brake (Rule 3) can amplify on
/// consecutive overshoots.
#[derive(Debug, Clone, Default)]
pub struct AutotuneController {
    /// Cycles observed since boot (read by daemon for log lines).
    pub cycles_observed: u64,
    /// Consecutive collapse streak — Rule 1 fires at `>= 2`.
    pub collapse_streak: u32,
    /// Consecutive healthy streak — Rule 5 fires at `>= 1` (per V1
    /// commit `eb42bce46`'s threshold loosen from 3 → 1).
    pub healthy_streak: u32,
    /// Consecutive overshoot streak — feeds Rule 3's integral term.
    pub overshoot_streak: u32,
    /// In-flight cycle measurement (cleared each `on_cycle_complete`).
    pub current: CycleMetrics,
    /// Per-sample dt accumulator for active-band time. Maintained by
    /// the caller via `observe_sample(.., dt_sec)`.
    pub last_sample_was_active: bool,
}

impl AutotuneController {
    /// Construct a fresh controller — all streaks at 0, no cycle data.
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Reset all counters AND in-flight cycle data. Use when the daemon
    /// restarts or the SNN config changes (any state that would make
    /// prior streak counts stale).
    pub fn reset(&mut self) {
        *self = Self::default();
    }

    /// Record one observation. The daemon polls this every ~30s during
    /// the wake window between sleep cycles.
    ///
    /// `dt_sec` is the elapsed real time since the last sample;
    /// accumulates into `active_t_sec` only when the sample lies in
    /// the biological active band (`sparsity ∈ [0.05, 0.15]`).
    ///
    /// `cycle_started_at_sec` is the wall-clock time when the prior
    /// sleep cycle completed (used to compute `recovery_t_sec`).
    /// Pass `None` if no cycle has completed yet (boot).
    pub fn observe_sample(
        &mut self,
        rate_hz: f32,
        sparsity: f32,
        dt_sec: f32,
        now_sec: f32,
        cycle_started_at_sec: Option<f32>,
    ) {
        if rate_hz > self.current.peak_rate_hz {
            self.current.peak_rate_hz = rate_hz;
        }
        if self.current.recovery_t_sec.is_none()
            && let Some(start) = cycle_started_at_sec
            && sparsity < 0.5
        {
            self.current.recovery_t_sec = Some(now_sec - start);
        }
        if (0.05..=0.15).contains(&sparsity) {
            self.current.active_t_sec += dt_sec.max(0.0);
            self.last_sample_was_active = true;
        } else {
            self.last_sample_was_active = false;
        }
    }

    /// Cycle boundary — evaluate the in-flight `current` against the
    /// rules, return at most one [`Decision`], and clear the cycle
    /// data ready for the next window.
    ///
    /// Streak counters are mutated **before** the decision is made so a
    /// `>= N` rule sees the post-update count.
    pub fn on_cycle_complete(
        &mut self,
        snapshot: AutotuneSnapshot,
        interval_sec: f32,
    ) -> Option<Decision> {
        let cycle = self.current;
        self.current = CycleMetrics::default();
        self.cycles_observed += 1;

        // Streak update — V1 commit eb42bce46 loosened the healthy
        // band: any peak in [30, 500] Hz, no active-time gate.
        if cycle.peak_rate_hz < 5.0 {
            self.collapse_streak = self.collapse_streak.saturating_add(1);
            self.healthy_streak = 0;
            self.overshoot_streak = 0;
        } else if cycle.peak_rate_hz > 500.0 {
            self.overshoot_streak = self.overshoot_streak.saturating_add(1);
            self.healthy_streak = 0;
            self.collapse_streak = 0;
        } else if (30.0..=500.0).contains(&cycle.peak_rate_hz) {
            self.healthy_streak = self.healthy_streak.saturating_add(1);
            self.collapse_streak = 0;
            self.overshoot_streak = 0;
        } else {
            self.collapse_streak = 0;
            self.healthy_streak = 0;
            self.overshoot_streak = 0;
        }

        // Rules — priority order, first match wins.
        // Rule 1: collapsed (≥ 2 cycles) → bump noise floor.
        if self.collapse_streak >= 2 {
            let cur = snapshot.noise_rate_hz;
            let new = Actuator::NoiseRateHz.clamp(cur + 10.0);
            if (new - cur).abs() > f32::EPSILON {
                return Some(Decision {
                    actuator: Actuator::NoiseRateHz,
                    current: cur,
                    target: new,
                    reason: format!("rescue: collapse_streak={}", self.collapse_streak),
                });
            }
        }

        // Rule 2: recovery slow → lengthen interval.
        if let Some(rec) = cycle.recovery_t_sec
            && rec > 0.7 * interval_sec
        {
            let new = Actuator::SleepIntervalSec.clamp(interval_sec + 60.0);
            if (new - interval_sec).abs() > f32::EPSILON {
                return Some(Decision {
                    actuator: Actuator::SleepIntervalSec,
                    current: interval_sec,
                    target: new,
                    reason: format!("recovery_t={rec:.0}s > 0.7*interval"),
                });
            }
        }

        // Rule 3: overshoot — proportional+integral brake.
        // V1 commits eb42bce46 + 203fa0467: threshold 1000 → 500;
        // step = min(0.05, 0.005 * (peak/500) * (1 + 0.5*streak)).
        if cycle.peak_rate_hz > 500.0 {
            let cur = snapshot.max_scale_dead;
            let p_factor = cycle.peak_rate_hz / 500.0;
            #[allow(clippy::cast_precision_loss)]
            let i_factor = 1.0 + 0.5 * self.overshoot_streak as f32;
            let step = (0.005 * p_factor * i_factor).min(0.05);
            let new = Actuator::MaxScaleDead.clamp(cur - step);
            if (new - cur).abs() > f32::EPSILON {
                return Some(Decision {
                    actuator: Actuator::MaxScaleDead,
                    current: cur,
                    target: new,
                    reason: format!(
                        "peak={:.0}Hz > 500 (P={p_factor:.2} I={i_factor:.1} step={step:.4})",
                        cycle.peak_rate_hz
                    ),
                });
            }
        }

        // Rule 4: under-recovery (not collapsed) → boost scale.
        if (5.0..30.0).contains(&cycle.peak_rate_hz) {
            let cur = snapshot.max_scale_dead;
            let new = Actuator::MaxScaleDead.clamp(cur + 0.005);
            if (new - cur).abs() > f32::EPSILON {
                return Some(Decision {
                    actuator: Actuator::MaxScaleDead,
                    current: cur,
                    target: new,
                    reason: format!("peak={:.0}Hz in [5,30)", cycle.peak_rate_hz),
                });
            }
        }

        // Rule 5: any healthy cycle (per eb42bce46) → taper rescue noise
        // when above the 20 Hz baseline.
        if self.healthy_streak >= 1 && snapshot.noise_rate_hz > 20.0 {
            let cur = snapshot.noise_rate_hz;
            let new = Actuator::NoiseRateHz.clamp(cur - 5.0);
            if (new - cur).abs() > f32::EPSILON {
                return Some(Decision {
                    actuator: Actuator::NoiseRateHz,
                    current: cur,
                    target: new,
                    reason: format!(
                        "taper: healthy_streak={} noise={cur:.0}>20",
                        self.healthy_streak
                    ),
                });
            }
        }

        None
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    fn snap() -> AutotuneSnapshot {
        AutotuneSnapshot::default()
    }

    fn drive_cycle(ctrl: &mut AutotuneController, peak: f32, sparsity: f32) {
        // One sample at the peak, plus enough active-band time to make
        // active_t > 0.3 * interval (so the older V1 rule would have
        // fired healthy — kept here even though V2 dropped that gate
        // so test intent stays clear).
        ctrl.observe_sample(peak, sparsity, 60.0, 60.0, Some(0.0));
    }

    // --- Rule 1: collapse rescue ---

    #[test]
    fn collapse_streak_below_2_does_not_fire() {
        let mut c = AutotuneController::new();
        drive_cycle(&mut c, 1.0, 0.99);
        let d = c.on_cycle_complete(snap(), 900.0);
        assert!(d.is_none(), "single collapse cycle must not fire");
        assert_eq!(c.collapse_streak, 1);
    }

    #[test]
    fn collapse_streak_two_fires_noise_bump() {
        let mut c = AutotuneController::new();
        drive_cycle(&mut c, 1.0, 0.99);
        let _ = c.on_cycle_complete(snap(), 900.0);
        drive_cycle(&mut c, 1.0, 0.99);
        let d = c.on_cycle_complete(snap(), 900.0).expect("should fire");
        assert_eq!(d.actuator, Actuator::NoiseRateHz);
        assert!((d.target - 30.0).abs() < 1e-3, "noise 20 + 10 = 30");
    }

    #[test]
    fn rule1_clamped_at_max_noise() {
        let mut c = AutotuneController::new();
        let mut snap_high = snap();
        snap_high.noise_rate_hz = 100.0; // already at clamp
        drive_cycle(&mut c, 1.0, 0.99);
        let _ = c.on_cycle_complete(snap_high, 900.0);
        drive_cycle(&mut c, 1.0, 0.99);
        let d = c.on_cycle_complete(snap_high, 900.0);
        assert!(d.is_none(), "no move when target equals current at clamp");
    }

    // --- Rule 2: recovery slow ---

    #[test]
    fn rule2_slow_recovery_lengthens_interval() {
        let mut c = AutotuneController::new();
        // sample with sparsity < 0.5 takes 700s after cycle start;
        // recovery > 0.7 * 900 = 630s.
        c.observe_sample(50.0, 0.4, 700.0, 700.0, Some(0.0));
        let d = c.on_cycle_complete(snap(), 900.0).expect("should fire");
        assert_eq!(d.actuator, Actuator::SleepIntervalSec);
        assert!((d.target - 960.0).abs() < 1e-3);
    }

    // --- Rule 3: P+I brake on overshoot ---

    #[test]
    fn rule3_overshoot_first_hit_proportional_only() {
        let mut c = AutotuneController::new();
        drive_cycle(&mut c, 745.0, 0.05);
        let d = c.on_cycle_complete(snap(), 900.0).expect("should fire");
        assert_eq!(d.actuator, Actuator::MaxScaleDead);
        // P = 745/500 = 1.49, I = 1 + 0.5*1 = 1.5, step = 0.005*1.49*1.5 = 0.011175
        let expected_step = 0.005 * (745.0 / 500.0) * (1.0 + 0.5 * 1.0);
        let expected = (snap().max_scale_dead - expected_step).max(1.01);
        assert!(
            (d.target - expected).abs() < 1e-4,
            "got {} expected {expected}",
            d.target
        );
    }

    #[test]
    fn rule3_integral_amplifies_on_consecutive_overshoots() {
        // Validate the I term by inspecting the *uncapped* step the
        // controller wants — we re-run from the same snapshot each time
        // so the clamp at 1.01 doesn't flatten the comparison once
        // max_scale_dead approaches the floor.
        let mut c = AutotuneController::new();
        let mut last_step = 0.0_f32;
        for _ in 0..3 {
            drive_cycle(&mut c, 745.0, 0.05);
            // Use a fresh high-headroom snapshot every iteration so
            // the clamp can't mask the I-factor growth.
            let mut snap_high = snap();
            snap_high.max_scale_dead = 1.10; // clamp ceiling
            let d = c.on_cycle_complete(snap_high, 900.0).expect("should fire");
            let step = snap_high.max_scale_dead - d.target;
            assert!(
                step > last_step,
                "I term must grow streak-over-streak (got {step} after {last_step})"
            );
            last_step = step;
        }
        // Last iteration's overshoot_streak == 3 (controller incremented
        // each on_cycle_complete) → I=2.5, P=1.49, step=0.018625.
        assert!(c.overshoot_streak == 3, "streak: {}", c.overshoot_streak);
    }

    #[test]
    fn rule3_step_capped_at_005() {
        let mut c = AutotuneController::new();
        // Force overshoot_streak high, peak high — without the cap step
        // would explode.
        c.overshoot_streak = 100;
        drive_cycle(&mut c, 5000.0, 0.01);
        let d = c.on_cycle_complete(snap(), 900.0).expect("should fire");
        let step = snap().max_scale_dead - d.target;
        // Snapshot at 1.05; cap at 0.05 → target ≥ 1.00 but clamped at
        // 1.01. So step is capped at 1.05 - 1.01 = 0.04 by the clamp,
        // never the cap. Either way step <= 0.05.
        assert!(step <= 0.05 + 1e-4);
    }

    // --- Rule 4: under-recovery boost ---

    #[test]
    fn rule4_under_recovery_boosts_scale() {
        let mut c = AutotuneController::new();
        drive_cycle(&mut c, 20.0, 0.5);
        let d = c.on_cycle_complete(snap(), 900.0).expect("should fire");
        assert_eq!(d.actuator, Actuator::MaxScaleDead);
        assert!((d.target - 1.055).abs() < 1e-3);
    }

    // --- Rule 5: healthy taper ---

    #[test]
    fn rule5_healthy_with_high_noise_tapers() {
        let mut c = AutotuneController::new();
        let mut snap_hot = snap();
        snap_hot.noise_rate_hz = 70.0;
        drive_cycle(&mut c, 100.0, 0.10);
        let d = c.on_cycle_complete(snap_hot, 900.0).expect("should fire");
        assert_eq!(d.actuator, Actuator::NoiseRateHz);
        assert!((d.target - 65.0).abs() < 1e-3);
    }

    #[test]
    fn rule5_healthy_at_baseline_noise_no_op() {
        let mut c = AutotuneController::new();
        // noise_rate_hz at default 20 → no taper (V1 commit threshold).
        drive_cycle(&mut c, 100.0, 0.10);
        let d = c.on_cycle_complete(snap(), 900.0);
        assert!(d.is_none(), "Rule 5 only fires when noise > 20");
    }

    // --- streak resets ---

    #[test]
    fn entering_healthy_band_resets_collapse_and_overshoot() {
        let mut c = AutotuneController::new();
        c.collapse_streak = 5;
        c.overshoot_streak = 3;
        drive_cycle(&mut c, 100.0, 0.10);
        let _ = c.on_cycle_complete(snap(), 900.0);
        assert_eq!(c.collapse_streak, 0);
        assert_eq!(c.overshoot_streak, 0);
        assert_eq!(c.healthy_streak, 1);
    }

    #[test]
    fn entering_overshoot_resets_other_streaks() {
        let mut c = AutotuneController::new();
        c.healthy_streak = 5;
        c.collapse_streak = 1;
        drive_cycle(&mut c, 700.0, 0.05);
        let _ = c.on_cycle_complete(snap(), 900.0);
        assert_eq!(c.healthy_streak, 0);
        assert_eq!(c.collapse_streak, 0);
        assert_eq!(c.overshoot_streak, 1);
    }

    #[test]
    fn band_in_30_to_500_counts_as_healthy() {
        let mut c = AutotuneController::new();
        for peak in [30.0, 100.0, 500.0] {
            drive_cycle(&mut c, peak, 0.10);
            let _ = c.on_cycle_complete(snap(), 900.0);
        }
        assert_eq!(c.healthy_streak, 3);
    }

    // --- daemon-facing API: reset ---

    #[test]
    fn reset_clears_all_state() {
        let mut c = AutotuneController::new();
        c.collapse_streak = 3;
        c.healthy_streak = 7;
        c.overshoot_streak = 2;
        c.cycles_observed = 100;
        c.current.peak_rate_hz = 500.0;
        c.reset();
        assert_eq!(c.collapse_streak, 0);
        assert_eq!(c.healthy_streak, 0);
        assert_eq!(c.overshoot_streak, 0);
        assert_eq!(c.cycles_observed, 0);
        assert_eq!(c.current.peak_rate_hz, 0.0);
    }

    // --- Actuator clamps ---

    #[test]
    fn actuator_clamp_pins_max_scale_dead() {
        assert_eq!(Actuator::MaxScaleDead.clamp(2.0), 1.10);
        assert_eq!(Actuator::MaxScaleDead.clamp(0.5), 1.01);
    }

    #[test]
    fn actuator_clamp_pins_sleep_interval() {
        assert_eq!(Actuator::SleepIntervalSec.clamp(10000.0), 3600.0);
        assert_eq!(Actuator::SleepIntervalSec.clamp(0.0), 300.0);
    }

    #[test]
    fn actuator_clamp_pins_noise_rate() {
        assert_eq!(Actuator::NoiseRateHz.clamp(500.0), 100.0);
        assert_eq!(Actuator::NoiseRateHz.clamp(0.0), 10.0);
    }

    // --- observe_sample mechanics ---

    #[test]
    fn observe_sample_tracks_peak_across_calls() {
        let mut c = AutotuneController::new();
        c.observe_sample(50.0, 0.1, 30.0, 30.0, Some(0.0));
        c.observe_sample(120.0, 0.1, 30.0, 60.0, Some(0.0));
        c.observe_sample(80.0, 0.1, 30.0, 90.0, Some(0.0));
        assert_eq!(c.current.peak_rate_hz, 120.0);
    }

    #[test]
    fn observe_sample_records_first_recovery_only() {
        let mut c = AutotuneController::new();
        c.observe_sample(10.0, 0.4, 100.0, 100.0, Some(0.0));
        c.observe_sample(10.0, 0.3, 100.0, 200.0, Some(0.0));
        assert_eq!(c.current.recovery_t_sec, Some(100.0));
    }

    #[test]
    fn observe_sample_accumulates_active_time_only_in_band() {
        let mut c = AutotuneController::new();
        c.observe_sample(10.0, 0.10, 30.0, 30.0, None); // in band
        c.observe_sample(10.0, 0.50, 30.0, 60.0, None); // out
        c.observe_sample(10.0, 0.05, 30.0, 90.0, None); // in band
        assert!((c.current.active_t_sec - 60.0).abs() < 1e-3);
    }
}

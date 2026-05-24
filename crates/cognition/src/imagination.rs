//! Imagination workspace — holds an active imagined-content vector +
//! vividness, evolvable by noise / goal-blend. The self-contained core of
//! V1's imagination engine (`src/cognitive/imagination/`); the
//! VAE/hippocampus/parietal scenario *generators* are out of scope.
//!
//! # What this is
//!
//! A small pool of [`Scenario`]s, each a latent content vector with a
//! vividness scalar. The cascade reads scenario `[0]` via
//! [`ImaginationEngine::copy_active_vector`] (V1's `copy_active_vector`).
//!
//! # The `semantic_buffer` gap
//!
//! In V1 the active vector is a *noise-seeded latent* — its `semantic_buffer`
//! (intended to hold language-space content) was declared but never
//! written, so the cascade blended raw noise. V2 closes that gap with
//! [`ImaginationEngine::stage_active_vector`]: a caller can stage real
//! semantic content (e.g. a comprehended scene) as the active vector, so
//! the imagination→content blend is meaningful rather than noise.

use serde::{Deserialize, Serialize};

/// Default latent width (V1 `IMAGINATION_DEFAULT_LATENT_DIM`).
pub const DEFAULT_LATENT_DIM: usize = 256;
/// Max concurrent scenarios (V1 `IMAGINATION_MAX_SCENARIOS`).
pub const MAX_SCENARIOS: usize = 8;
/// Default vividness for a fresh scenario.
pub const DEFAULT_VIVIDNESS: f32 = 0.7;
/// Default per-step creativity noise σ.
pub const DEFAULT_CREATIVITY_NOISE: f32 = 0.1;
/// Coherence below this rolls a step back (V1 `coherence_threshold`).
pub const DEFAULT_COHERENCE_THRESHOLD: f32 = 0.5;

/// How a scenario evolves on [`ImaginationEngine::step_scenario`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
pub enum ScenarioMode {
    /// Low-noise drift.
    #[default]
    Passive,
    /// High-noise free association.
    Creative,
    /// Low noise + blend toward a goal vector.
    Directed,
}

fn mode_noise_scale(mode: ScenarioMode) -> f32 {
    match mode {
        ScenarioMode::Passive => 2.0,
        ScenarioMode::Creative => 3.0,
        ScenarioMode::Directed => 0.5,
    }
}

/// One imagined scenario.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Scenario {
    /// Current latent / content vector (the active vector the cascade reads).
    pub latent_state: Vec<f32>,
    /// Previous step's latent (for coherence + rollback).
    pub latent_previous: Vec<f32>,
    pub vividness: f32,
    pub coherence: f32,
    pub novelty: f32,
    pub is_active: bool,
    pub is_paused: bool,
    pub mode: ScenarioMode,
    /// Optional target vector for `Directed` mode.
    pub goal: Option<Vec<f32>>,
}

/// Deterministic, serde-friendly xorshift64 (so the engine round-trips).
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
struct XorShift64 {
    state: u64,
}

impl XorShift64 {
    fn new(seed: u64) -> Self {
        Self {
            state: if seed == 0 { 0x9E37_79B9_7F4A_7C15 } else { seed },
        }
    }
    fn next_u64(&mut self) -> u64 {
        let mut x = self.state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.state = x;
        x
    }
    fn next_f32_unit(&mut self) -> f32 {
        #[allow(clippy::cast_precision_loss)]
        let v = (self.next_u64() >> 40) as f32;
        v / (1u32 << 24) as f32
    }
    /// Standard normal via Box–Muller.
    fn next_normal(&mut self) -> f32 {
        let u1 = self.next_f32_unit().max(1e-7);
        let u2 = self.next_f32_unit();
        (-2.0 * u1.ln()).sqrt() * (std::f32::consts::TAU * u2).cos()
    }
}

/// The imagination workspace engine.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ImaginationEngine {
    scenarios: Vec<Scenario>,
    latent_dim: usize,
    max_scenarios: usize,
    default_vividness: f32,
    creativity_noise: f32,
    coherence_threshold: f32,
    rng: XorShift64,
}

impl ImaginationEngine {
    /// New engine with the given latent width + RNG seed.
    #[must_use]
    pub fn new(latent_dim: usize, seed: u64) -> Self {
        Self {
            scenarios: Vec::new(),
            latent_dim: latent_dim.max(1),
            max_scenarios: MAX_SCENARIOS,
            default_vividness: DEFAULT_VIVIDNESS,
            creativity_noise: DEFAULT_CREATIVITY_NOISE,
            coherence_threshold: DEFAULT_COHERENCE_THRESHOLD,
            rng: XorShift64::new(seed),
        }
    }

    /// Latent width.
    #[must_use]
    pub fn latent_dim(&self) -> usize {
        self.latent_dim
    }

    /// Number of scenarios held.
    #[must_use]
    pub fn scenario_count(&self) -> usize {
        self.scenarios.len()
    }

    fn apply_noise(&mut self, target_len: usize, sigma: f32, out: &mut [f32]) {
        for v in out.iter_mut().take(target_len) {
            *v += self.rng.next_normal() * sigma;
        }
    }

    /// Begin a scenario: seed a fresh latent with small noise. Returns its
    /// index, or `None` if at `max_scenarios`. A `goal` (for `Directed`
    /// mode) is truncated / zero-padded to `latent_dim`.
    pub fn begin_scenario(&mut self, mode: ScenarioMode, goal: Option<&[f32]>) -> Option<usize> {
        if self.scenarios.len() >= self.max_scenarios {
            return None;
        }
        let dim = self.latent_dim;
        let mut latent = vec![0.0_f32; dim];
        let sigma = self.creativity_noise;
        self.apply_noise(dim, sigma, &mut latent);
        let goal = goal.map(|g| fit(g, dim));
        self.scenarios.push(Scenario {
            latent_previous: latent.clone(),
            latent_state: latent,
            vividness: self.default_vividness,
            coherence: 1.0,
            novelty: 0.0,
            is_active: true,
            is_paused: false,
            mode,
            goal,
        });
        Some(self.scenarios.len() - 1)
    }

    /// Stage real semantic content as the active scenario's vector (V2's
    /// fill for V1's never-wired `semantic_buffer`). Creates scenario `[0]`
    /// if none exists. The vector is fit to `latent_dim`; `vividness` is
    /// clamped to `[0, 1]`. After this, [`Self::copy_active_vector`] serves
    /// meaningful content rather than noise.
    pub fn stage_active_vector(&mut self, content: &[f32], vividness: f32) {
        let dim = self.latent_dim;
        let fitted = fit(content, dim);
        if let Some(sc) = self.scenarios.first_mut() {
            sc.latent_previous.clone_from(&sc.latent_state);
            sc.latent_state = fitted;
            sc.vividness = vividness.clamp(0.0, 1.0);
            sc.is_active = true;
            sc.is_paused = false;
        } else {
            self.scenarios.push(Scenario {
                latent_previous: fitted.clone(),
                latent_state: fitted,
                vividness: vividness.clamp(0.0, 1.0),
                coherence: 1.0,
                novelty: 0.0,
                is_active: true,
                is_paused: false,
                mode: ScenarioMode::Passive,
                goal: None,
            });
        }
    }

    /// Evolve a scenario one step: mode-scaled noise (+ goal blend in
    /// `Directed`), then a cosine-coherence check that rolls the step back
    /// when coherence drops below threshold. Returns the new coherence, or
    /// `None` if the index is invalid.
    pub fn step_scenario(&mut self, index: usize) -> Option<f32> {
        if index >= self.scenarios.len() {
            return None;
        }
        let dim = self.latent_dim;
        let sigma = self.creativity_noise * mode_noise_scale(self.scenarios[index].mode);
        let mode = self.scenarios[index].mode;
        let goal = self.scenarios[index].goal.clone();

        // Save previous, evolve a working copy.
        let prev = self.scenarios[index].latent_state.clone();
        let mut next = prev.clone();
        self.apply_noise(dim, sigma, &mut next);
        if mode == ScenarioMode::Directed {
            if let Some(g) = &goal {
                blend_into(&mut next, g, 0.9); // keep 0.9 self, pull 0.1 toward goal
            }
        }

        let coherence = coherence_of(&next, &prev);
        let sc = &mut self.scenarios[index];
        if coherence < self.coherence_threshold {
            // Roll back — the step diverged too far.
            sc.coherence = coherence;
            return Some(coherence);
        }
        sc.latent_previous = prev;
        sc.latent_state = next;
        sc.coherence = coherence;
        sc.novelty = 1.0 - coherence;
        Some(coherence)
    }

    /// End (deactivate + remove) a scenario. Returns `true` if it existed.
    pub fn end_scenario(&mut self, index: usize) -> bool {
        if index < self.scenarios.len() {
            self.scenarios.remove(index);
            true
        } else {
            false
        }
    }

    /// Drop all scenarios.
    pub fn reset(&mut self) {
        self.scenarios.clear();
    }

    /// Copy the active scenario's vector into `out` (V1 `copy_active_vector`).
    /// "Active" = scenario `[0]` exists and is `is_active && !is_paused`.
    /// Copies `min(len, out.len())` finite values and returns
    /// `(copied, vividness)`; `(0, 0.0)` when idle.
    #[must_use]
    pub fn copy_active_vector(&self, out: &mut [f32]) -> (usize, f32) {
        let Some(sc) = self.scenarios.first() else {
            return (0, 0.0);
        };
        if !sc.is_active || sc.is_paused {
            return (0, 0.0);
        }
        let n = sc.latent_state.len().min(out.len());
        for (dst, &src) in out.iter_mut().zip(sc.latent_state.iter()).take(n) {
            *dst = if src.is_finite() { src } else { 0.0 };
        }
        (n, sc.vividness)
    }

    /// Convenience: the active vector + vividness as a slice, or `None`
    /// when idle.
    #[must_use]
    pub fn active_vector(&self) -> Option<(&[f32], f32)> {
        let sc = self.scenarios.first()?;
        if sc.is_active && !sc.is_paused {
            Some((&sc.latent_state, sc.vividness))
        } else {
            None
        }
    }
}

/// Truncate / zero-pad `v` to length `dim`.
fn fit(v: &[f32], dim: usize) -> Vec<f32> {
    let mut out = vec![0.0_f32; dim];
    for (d, &s) in out.iter_mut().zip(v.iter()) {
        *d = s;
    }
    out
}

/// `dst ← α·dst + (1−α)·other` (element-wise, min-length).
fn blend_into(dst: &mut [f32], other: &[f32], alpha: f32) {
    for (d, &o) in dst.iter_mut().zip(other.iter()) {
        *d = alpha * *d + (1.0 - alpha) * o;
    }
}

/// Cosine similarity mapped to `[0, 1]` (V1 `compute_coherence`).
fn coherence_of(a: &[f32], b: &[f32]) -> f32 {
    let mut dot = 0.0_f32;
    let mut na = 0.0_f32;
    let mut nb = 0.0_f32;
    for (&x, &y) in a.iter().zip(b.iter()) {
        dot += x * y;
        na += x * x;
        nb += y * y;
    }
    let denom = na.sqrt() * nb.sqrt();
    if denom < 1e-8 {
        return 1.0; // two (near-)zero vectors are perfectly "coherent".
    }
    ((dot / denom) + 1.0) * 0.5
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn fresh_engine_is_idle() {
        let eng = ImaginationEngine::new(8, 1);
        assert_eq!(eng.scenario_count(), 0);
        let mut buf = [0.0_f32; 8];
        assert_eq!(eng.copy_active_vector(&mut buf), (0, 0.0));
        assert!(eng.active_vector().is_none());
    }

    #[test]
    fn begin_makes_active_scenario() {
        let mut eng = ImaginationEngine::new(8, 2);
        let idx = eng.begin_scenario(ScenarioMode::Creative, None).unwrap();
        assert_eq!(idx, 0);
        let mut buf = [0.0_f32; 8];
        let (n, viv) = eng.copy_active_vector(&mut buf);
        assert_eq!(n, 8);
        assert_eq!(viv, DEFAULT_VIVIDNESS);
        assert!(buf.iter().any(|&x| x != 0.0), "noise-seeded latent");
    }

    #[test]
    fn stage_active_vector_serves_semantic_content() {
        let mut eng = ImaginationEngine::new(4, 3);
        eng.stage_active_vector(&[0.1, 0.2, 0.3, 0.4], 0.9);
        let (vec, viv) = eng.active_vector().unwrap();
        assert_eq!(vec, &[0.1, 0.2, 0.3, 0.4]);
        assert_eq!(viv, 0.9);
        // Re-stage updates in place (no new scenario).
        eng.stage_active_vector(&[1.0, 1.0, 1.0, 1.0], 0.5);
        assert_eq!(eng.scenario_count(), 1);
        assert_eq!(eng.active_vector().unwrap().0, &[1.0, 1.0, 1.0, 1.0]);
    }

    #[test]
    fn stage_fits_dimension() {
        let mut eng = ImaginationEngine::new(3, 1);
        eng.stage_active_vector(&[1.0, 2.0, 3.0, 4.0, 5.0], 0.7); // truncated
        assert_eq!(eng.active_vector().unwrap().0, &[1.0, 2.0, 3.0]);
        eng.reset();
        eng.stage_active_vector(&[9.0], 0.7); // zero-padded
        assert_eq!(eng.active_vector().unwrap().0, &[9.0, 0.0, 0.0]);
    }

    #[test]
    fn copy_active_vector_finite_guard() {
        let mut eng = ImaginationEngine::new(4, 1);
        eng.stage_active_vector(&[f32::NAN, f32::INFINITY, 1.0, 2.0], 0.8);
        let mut buf = [0.0_f32; 4];
        let (n, _) = eng.copy_active_vector(&mut buf);
        assert_eq!(n, 4);
        assert_eq!(buf, [0.0, 0.0, 1.0, 2.0]);
    }

    #[test]
    fn step_keeps_coherent_evolution() {
        let mut eng = ImaginationEngine::new(16, 7);
        let idx = eng.begin_scenario(ScenarioMode::Passive, None).unwrap();
        let c = eng.step_scenario(idx).unwrap();
        assert!((0.0..=1.0).contains(&c));
    }

    #[test]
    fn end_and_reset() {
        let mut eng = ImaginationEngine::new(8, 1);
        eng.begin_scenario(ScenarioMode::Passive, None);
        eng.begin_scenario(ScenarioMode::Creative, None);
        assert_eq!(eng.scenario_count(), 2);
        assert!(eng.end_scenario(0));
        assert_eq!(eng.scenario_count(), 1);
        eng.reset();
        assert_eq!(eng.scenario_count(), 0);
    }

    #[test]
    fn max_scenarios_enforced() {
        let mut eng = ImaginationEngine::new(4, 1);
        for _ in 0..MAX_SCENARIOS {
            assert!(eng.begin_scenario(ScenarioMode::Passive, None).is_some());
        }
        assert!(eng.begin_scenario(ScenarioMode::Passive, None).is_none());
    }

    #[test]
    fn serde_round_trip() {
        let mut eng = ImaginationEngine::new(4, 5);
        eng.stage_active_vector(&[0.5, 0.5, 0.5, 0.5], 0.6);
        let json = serde_json::to_string(&eng).unwrap();
        let back: ImaginationEngine = serde_json::from_str(&json).unwrap();
        assert_eq!(back.active_vector().unwrap().0, &[0.5, 0.5, 0.5, 0.5]);
        assert_eq!(back.latent_dim(), 4);
    }
}

//! Quantum Monte Carlo — reduced to a seeded Metropolis sampler.
//!
//! V1's full quantum Monte Carlo is much larger (path-integral,
//! variational, diffusion MC variants). V2 keeps just the Metropolis
//! sampling primitive — given an energy function `E(x)`, propose
//! a step, accept with probability `min(1, exp(-ΔE / T))`.
//!
//! This generalises to any "MCMC over f32 state" problem; callers
//! supply the energy function + proposal distribution.

use rand::{Rng, SeedableRng};
use rand_chacha::ChaCha20Rng;
use thiserror::Error;

/// Sampler errors. Rare — construction sanity only.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum MetropolisError {
    /// Temperature must be strictly positive.
    #[error("temperature must be > 0, got {0}")]
    NonPositiveTemperature(String),
    /// Proposal step scale must be positive.
    #[error("proposal_sigma must be > 0, got {0}")]
    NonPositiveSigma(String),
}

/// Metropolis sampler state — 1D for the minimum-viable port.
/// Callers looking for multi-dim can run multiple samplers in parallel
/// or call `metropolis_step` directly with their own state.
#[derive(Debug, Clone)]
pub struct MetropolisState {
    /// Current sample.
    pub x: f32,
    /// Current energy `E(x)`.
    pub energy: f32,
    /// Temperature `T` (inverse of `β`). Higher T → more accepts.
    pub temperature: f32,
    /// Standard deviation of the Gaussian proposal.
    pub proposal_sigma: f32,
    /// Running count of accepted proposals.
    pub n_accept: u64,
    /// Running count of total proposals.
    pub n_total: u64,
    /// RNG.
    rng: ChaCha20Rng,
}

impl MetropolisState {
    /// New sampler. Energy of the initial `x` is evaluated via
    /// `energy_fn(x_init)` once at construction.
    pub fn new<F: Fn(f32) -> f32>(
        x_init: f32,
        temperature: f32,
        proposal_sigma: f32,
        seed: u64,
        energy_fn: F,
    ) -> Result<Self, MetropolisError> {
        if temperature.partial_cmp(&0.0) != Some(std::cmp::Ordering::Greater) {
            return Err(MetropolisError::NonPositiveTemperature(format!(
                "{temperature}"
            )));
        }
        if proposal_sigma.partial_cmp(&0.0) != Some(std::cmp::Ordering::Greater) {
            return Err(MetropolisError::NonPositiveSigma(format!(
                "{proposal_sigma}"
            )));
        }
        Ok(Self {
            x: x_init,
            energy: energy_fn(x_init),
            temperature,
            proposal_sigma,
            n_accept: 0,
            n_total: 0,
            rng: ChaCha20Rng::seed_from_u64(seed),
        })
    }

    /// Acceptance ratio over the run so far. Returns `0.0` before
    /// the first proposal.
    #[must_use]
    pub fn acceptance_ratio(&self) -> f32 {
        if self.n_total == 0 {
            return 0.0;
        }
        #[allow(clippy::cast_precision_loss)]
        let num = self.n_accept as f32;
        #[allow(clippy::cast_precision_loss)]
        let den = self.n_total as f32;
        num / den
    }
}

/// One Metropolis step: propose `x' = x + N(0, σ²)`, accept with
/// probability `min(1, exp(-ΔE / T))`. Mutates `state` in place.
/// Returns `true` if the proposal was accepted.
pub fn metropolis_step<F: Fn(f32) -> f32>(state: &mut MetropolisState, energy_fn: F) -> bool {
    // Propose — Gaussian via Box-Muller using two uniforms.
    let u1: f32 = state.rng.random::<f32>().max(1e-9);
    let u2: f32 = state.rng.random::<f32>();
    let normal = (-2.0 * u1.ln()).sqrt() * (2.0 * core::f32::consts::PI * u2).cos();
    let x_new = state.x + state.proposal_sigma * normal;
    let e_new = energy_fn(x_new);

    let de = e_new - state.energy;
    let accept = if de <= 0.0 {
        true
    } else {
        let prob = (-de / state.temperature).exp();
        state.rng.random::<f32>() < prob
    };

    state.n_total = state.n_total.saturating_add(1);
    if accept {
        state.n_accept = state.n_accept.saturating_add(1);
        state.x = x_new;
        state.energy = e_new;
    }
    accept
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn rejects_non_positive_temperature() {
        assert!(matches!(
            MetropolisState::new(0.0, 0.0, 0.1, 0, |_| 0.0),
            Err(MetropolisError::NonPositiveTemperature(_))
        ));
    }

    #[test]
    fn rejects_non_positive_sigma() {
        assert!(matches!(
            MetropolisState::new(0.0, 1.0, 0.0, 0, |_| 0.0),
            Err(MetropolisError::NonPositiveSigma(_))
        ));
    }

    /// Sampling a quadratic energy `E(x) = x²` at temperature 1 should
    /// concentrate mass near zero (Boltzmann distribution ∝ exp(-x²)).
    #[test]
    fn quadratic_energy_concentrates_near_zero() {
        let energy = |x: f32| x * x;
        let mut s = MetropolisState::new(5.0, 1.0, 0.5, 42, energy).unwrap();
        let mut samples: Vec<f32> = Vec::with_capacity(10_000);
        // Burn-in.
        for _ in 0..1000 {
            metropolis_step(&mut s, energy);
        }
        for _ in 0..10_000 {
            metropolis_step(&mut s, energy);
            samples.push(s.x);
        }
        #[allow(clippy::cast_precision_loss)]
        let mean: f32 = samples.iter().sum::<f32>() / samples.len() as f32;
        // Boltzmann mean for E = x² is 0 by symmetry.
        assert!(mean.abs() < 0.2, "mean {mean} not near 0");
    }

    #[test]
    fn high_temperature_accepts_almost_everything() {
        let energy = |x: f32| x * x * 1000.0;
        let mut s = MetropolisState::new(0.0, 1.0e10, 1.0, 7, energy).unwrap();
        for _ in 0..500 {
            metropolis_step(&mut s, energy);
        }
        assert!(
            s.acceptance_ratio() > 0.95,
            "high T should accept ~everything; got {}",
            s.acceptance_ratio()
        );
    }

    #[test]
    fn low_temperature_freezes_at_minimum() {
        // At T → 0 the sampler should accept only downhill moves,
        // so acceptance drops once we're near the bottom of the well.
        let energy = |x: f32| (x - 3.0) * (x - 3.0);
        let mut s = MetropolisState::new(0.0, 1.0e-3, 0.3, 7, energy).unwrap();
        for _ in 0..2000 {
            metropolis_step(&mut s, energy);
        }
        // Should have converged near x = 3.
        assert!((s.x - 3.0).abs() < 0.5, "didn't reach minimum; x = {}", s.x);
    }

    #[test]
    fn same_seed_same_trajectory() {
        let energy = |x: f32| x * x;
        let mut a = MetropolisState::new(1.0, 2.0, 0.3, 99, energy).unwrap();
        let mut b = MetropolisState::new(1.0, 2.0, 0.3, 99, energy).unwrap();
        for _ in 0..100 {
            metropolis_step(&mut a, energy);
            metropolis_step(&mut b, energy);
        }
        assert_eq!(a.x, b.x);
        assert_eq!(a.n_accept, b.n_accept);
    }

    #[test]
    fn acceptance_ratio_zero_before_first_step() {
        let s = MetropolisState::new(0.0, 1.0, 0.1, 0, |_| 0.0).unwrap();
        assert_eq!(s.acceptance_ratio(), 0.0);
    }
}

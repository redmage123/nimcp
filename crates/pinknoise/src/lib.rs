//! NIMCP V2 — 1/f (pink) noise generator.
//!
//! Port of V1's `src/plasticity/noise/nimcp_pink_noise.c`, reduced to
//! the core Voss-McCartney algorithm. V1 bundles a lot of monitoring +
//! multiscale + bridge machinery; this crate ships only the generator
//! itself. Monitoring (DFA-based `α` estimation) lives in the
//! companion fractal crate.
//!
//! # Algorithm
//!
//! Voss-McCartney pink noise sums `n_octaves` independent white-noise
//! sources, each updated at a different timescale. Source `k` is
//! refreshed every `2^k` samples; together they produce a spectrum
//! that falls off as roughly `1/f`.
//!
//! - Low `n_octaves` (4–6) gives a rough approximation, cheap per step.
//! - High `n_octaves` (12–16) gives a cleaner 1/f spectrum over 3–4
//!   decades of frequency.
//! - V1 default: `n_octaves = 16`, used for plasticity noise that has
//!   to look stationary over 10⁴+ steps.
//!
//! # Contract
//!
//! - Deterministic: same seed → bit-identical sample sequence.
//! - Output is centered near zero with amplitude ~±1 (per source the
//!   sum is ∈ [-n_octaves, +n_octaves]; [`PinkNoiseGen`] normalises
//!   by `n_octaves` so the typical output stays in [-1, 1]).
//! - Zero allocation after construction.

#![forbid(unsafe_code)]

use rand::Rng;
use rand::SeedableRng;
use rand_chacha::ChaCha20Rng;

/// Default octave count — V1's production value.
pub const DEFAULT_N_OCTAVES: u32 = 16;

/// Hard cap on octaves — at 31 the sample counter starts losing
/// resolution; we stop earlier to keep the update cadence exact.
pub const MAX_N_OCTAVES: u32 = 24;

/// Voss-McCartney pink-noise state. Each `step()` call advances the
/// generator by one sample and returns the next value in `[-1, 1]`
/// (approximate — small transients may exceed the range by a few %).
///
/// **Not serializable.** Pink-noise state is always reproducible
/// from `(n_octaves, seed, sample_count)` so checkpoints should
/// store that tuple and reconstruct on load rather than serialising
/// the internal RNG state.
#[derive(Debug, Clone)]
pub struct PinkNoiseGen {
    /// One octave's worth of state: the last-drawn white-noise value.
    /// Index `k` is updated every `2^k` samples.
    sources: Vec<f32>,
    /// Number of samples emitted — used to pick which sources update
    /// this tick. Wraps at `u64` horizon (≈10¹⁹ samples; irrelevant).
    counter: u64,
    /// RNG — seeded deterministically at construction.
    rng: ChaCha20Rng,
    /// Pre-computed `1 / n_octaves` so we avoid a divide on the hot path.
    inv_n_octaves: f32,
    /// Number of octaves — ranged `[1, MAX_N_OCTAVES]`.
    pub n_octaves: u32,
}

impl PinkNoiseGen {
    /// Construct a generator with `n_octaves` Voss-McCartney sources
    /// seeded by `seed`. Clamps `n_octaves` to `[1, MAX_N_OCTAVES]`.
    ///
    /// All sources are drawn from the RNG at construction so the first
    /// sample already has the target spectrum (no warmup transient).
    #[must_use]
    pub fn new(n_octaves: u32, seed: u64) -> Self {
        let n = n_octaves.clamp(1, MAX_N_OCTAVES);
        let mut rng = ChaCha20Rng::seed_from_u64(seed);
        let sources: Vec<f32> = (0..n).map(|_| rng.random::<f32>() * 2.0 - 1.0).collect();
        #[allow(clippy::cast_precision_loss)]
        let inv_n_octaves = 1.0 / (n as f32);
        Self {
            sources,
            counter: 0,
            rng,
            inv_n_octaves,
            n_octaves: n,
        }
    }

    /// Convenience: construct with [`DEFAULT_N_OCTAVES`].
    #[must_use]
    pub fn new_default(seed: u64) -> Self {
        Self::new(DEFAULT_N_OCTAVES, seed)
    }

    /// Advance one sample. Updates a subset of the sources (the
    /// lowest-order bit that flipped in the counter identifies which),
    /// then returns the sum divided by `n_octaves` so typical output
    /// is near `[-1, 1]`.
    pub fn step(&mut self) -> f32 {
        // Determine which sources to refresh. When bit k of `counter`
        // flips (on every 2^k increments), source k gets a fresh
        // white-noise value. The classic Voss trick: count the
        // number of trailing zeros in `counter + 1` to find the
        // highest bit that will change, and refresh sources [0..=k].
        self.counter = self.counter.wrapping_add(1);
        let k = self.counter.trailing_zeros().min(self.n_octaves - 1);
        for src in self.sources.iter_mut().take(k as usize + 1) {
            *src = self.rng.random::<f32>() * 2.0 - 1.0;
        }
        let sum: f32 = self.sources.iter().sum();
        sum * self.inv_n_octaves
    }

    /// Step `n` times and return the samples in a fresh `Vec`.
    /// Convenience wrapper for tests / benches.
    #[must_use]
    pub fn step_n(&mut self, n: usize) -> Vec<f32> {
        (0..n).map(|_| self.step()).collect()
    }

    /// Reset the counter + re-draw every source from the RNG. Use after
    /// `serde` round-trips if you want to discard any replay state.
    pub fn reset(&mut self) {
        self.counter = 0;
        for src in self.sources.iter_mut() {
            *src = self.rng.random::<f32>() * 2.0 - 1.0;
        }
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn construction_clamps_n_octaves() {
        let g = PinkNoiseGen::new(0, 0);
        assert_eq!(g.n_octaves, 1);
        let g = PinkNoiseGen::new(100, 0);
        assert_eq!(g.n_octaves, MAX_N_OCTAVES);
    }

    #[test]
    fn same_seed_same_sequence() {
        let mut a = PinkNoiseGen::new(8, 42);
        let mut b = PinkNoiseGen::new(8, 42);
        for _ in 0..200 {
            assert_eq!(a.step(), b.step());
        }
    }

    #[test]
    fn different_seeds_diverge() {
        let mut a = PinkNoiseGen::new(8, 1);
        let mut b = PinkNoiseGen::new(8, 2);
        let seq_a: Vec<f32> = (0..100).map(|_| a.step()).collect();
        let seq_b: Vec<f32> = (0..100).map(|_| b.step()).collect();
        assert_ne!(seq_a, seq_b);
    }

    #[test]
    fn output_in_amplitude_band() {
        // Typical output should stay within [-1.5, +1.5]; extreme
        // transients may briefly exceed but not by much.
        let mut g = PinkNoiseGen::new(8, 0x5EED);
        let samples = g.step_n(10_000);
        for &s in &samples {
            assert!(s.abs() < 2.0, "pink-noise sample {s} out of ±2 band");
        }
    }

    #[test]
    fn zero_mean_over_long_run() {
        let mut g = PinkNoiseGen::new(DEFAULT_N_OCTAVES, 7);
        let samples = g.step_n(50_000);
        #[allow(clippy::cast_precision_loss)]
        let mean: f32 = samples.iter().sum::<f32>() / samples.len() as f32;
        assert!(mean.abs() < 0.05, "long-run mean {mean} biased");
    }

    /// Coarse spectral sanity: partition the 50K samples into
    /// consecutive chunks and verify the variance of chunk means
    /// decreases sub-linearly with chunk size (1/f signature: low
    /// frequencies carry more power, so coarse chunks retain
    /// variance better than white noise).
    #[test]
    fn low_frequency_variance_survives_averaging() {
        let mut g = PinkNoiseGen::new(DEFAULT_N_OCTAVES, 0xF00D);
        let samples = g.step_n(50_000);

        let chunk_var = |chunk_size: usize| -> f32 {
            let mut means: Vec<f32> = Vec::with_capacity(samples.len() / chunk_size);
            for c in samples.chunks_exact(chunk_size) {
                #[allow(clippy::cast_precision_loss)]
                let m: f32 = c.iter().sum::<f32>() / c.len() as f32;
                means.push(m);
            }
            #[allow(clippy::cast_precision_loss)]
            let mu: f32 = means.iter().sum::<f32>() / means.len() as f32;
            #[allow(clippy::cast_precision_loss)]
            let v: f32 =
                means.iter().map(|&x| (x - mu) * (x - mu)).sum::<f32>() / means.len() as f32;
            v
        };

        let v_small = chunk_var(10);
        let v_large = chunk_var(100);
        // For white noise the ratio v_small/v_large = 10 (var ~ 1/N).
        // For 1/f, the ratio is much smaller — variance doesn't shrink
        // as aggressively because low-frequency power survives
        // averaging. Assert the ratio is below 10 (conservative bound).
        let ratio = v_small / v_large.max(1e-9);
        assert!(
            ratio < 10.0,
            "chunk-variance ratio {ratio} too high for 1/f (white noise = 10)"
        );
    }

    #[test]
    fn reset_resumes_from_fresh_state() {
        let mut g = PinkNoiseGen::new(8, 123);
        for _ in 0..500 {
            let _ = g.step();
        }
        let counter_before_reset = g.counter;
        g.reset();
        assert_eq!(g.counter, 0);
        assert!(counter_before_reset > 0);
    }

    #[test]
    fn step_n_matches_individual_steps() {
        let mut a = PinkNoiseGen::new(8, 999);
        let mut b = PinkNoiseGen::new(8, 999);
        let batch = a.step_n(50);
        for (i, &expected) in batch.iter().enumerate() {
            assert_eq!(b.step(), expected, "divergence at step {i}");
        }
    }
}

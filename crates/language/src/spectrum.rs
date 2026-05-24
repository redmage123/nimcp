//! Bigram FFT spectrum — a standalone "is grammar emerging?" diagnostic,
//! port of V1's `nimcp_bigram_spectrum.c`.
//!
//! Accumulates a `vocab_cap × vocab_cap` integer bigram-count matrix and,
//! on demand, takes a 2-D FFT (row-wise then column-wise 1-D passes) of
//! it. From the magnitude spectrum it derives three bounded metrics:
//!
//! - `peak_strength` — fraction of (non-DC) spectral energy in the single
//!   dominant frequency. Structure → a few strong peaks.
//! - `low_freq_concentration` — fraction of (non-DC) energy in the
//!   low-frequency quadrant. Grammatical structure concentrates there.
//! - `spectral_entropy` — normalized Shannon entropy of the (non-DC)
//!   magnitude distribution. High → flat/noisy; low → structured.
//!
//! This is **not** in the produce path; it is observed by the brain to
//! track whether sequence structure is forming. The DC bin (which is just
//! the total event count) is excluded from every metric.

use num_complex::Complex;
use rustfft::FftPlanner;
use serde::{Deserialize, Serialize};

/// Bounded spectral metrics, all in `[0, 1]`.
#[derive(Debug, Clone, Copy, Default, PartialEq, Serialize, Deserialize)]
pub struct BigramSpectralMetrics {
    pub peak_strength: f32,
    pub low_freq_concentration: f32,
    pub spectral_entropy: f32,
}

/// Square bigram-count matrix + cached spectral metrics.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BigramSpectrum {
    vocab_cap: usize,
    /// Row-major `vocab_cap × vocab_cap` counts.
    counts: Vec<u32>,
    total_events: u64,
    events_since_compute: u64,
    cached: Option<BigramSpectralMetrics>,
}

impl BigramSpectrum {
    /// New spectrum tracker. `vocab_cap` is clamped to `[2, 4096]`.
    #[must_use]
    pub fn new(vocab_cap: usize) -> Self {
        let cap = vocab_cap.clamp(2, 4096);
        Self {
            vocab_cap: cap,
            counts: vec![0; cap * cap],
            total_events: 0,
            events_since_compute: 0,
            cached: None,
        }
    }

    /// Capacity (rows == cols).
    #[must_use]
    pub fn vocab_cap(&self) -> usize {
        self.vocab_cap
    }

    /// Record a `(prev, next)` bigram. Ids `>= vocab_cap` are ignored.
    pub fn record(&mut self, prev: usize, next: usize) {
        if prev >= self.vocab_cap || next >= self.vocab_cap {
            return;
        }
        let cell = &mut self.counts[prev * self.vocab_cap + next];
        *cell = cell.saturating_add(1);
        self.total_events += 1;
        self.events_since_compute += 1;
    }

    /// Total recorded bigram events.
    #[must_use]
    pub fn total_events(&self) -> u64 {
        self.total_events
    }

    /// Last computed metrics, if any.
    #[must_use]
    pub fn cached_metrics(&self) -> Option<BigramSpectralMetrics> {
        self.cached
    }

    /// Clear counts + cache (keeps the capacity).
    pub fn reset(&mut self) {
        self.counts.iter_mut().for_each(|c| *c = 0);
        self.total_events = 0;
        self.events_since_compute = 0;
        self.cached = None;
    }

    /// Compute metrics now (caches the result). Returns the metrics.
    pub fn compute(&mut self) -> BigramSpectralMetrics {
        let m = self.compute_metrics();
        self.cached = Some(m);
        self.events_since_compute = 0;
        m
    }

    /// Recompute only if at least `min_delta_events` were recorded since
    /// the last compute; otherwise return the cached metrics (or compute
    /// once if never computed). Returns `(metrics, recomputed)`.
    pub fn maybe_compute(&mut self, min_delta_events: u64) -> (BigramSpectralMetrics, bool) {
        if self.cached.is_none() || self.events_since_compute >= min_delta_events {
            (self.compute(), true)
        } else {
            (self.cached.unwrap_or_default(), false)
        }
    }

    /// The 2-D FFT + metric extraction.
    fn compute_metrics(&self) -> BigramSpectralMetrics {
        let cap = self.vocab_cap;
        let n = cap.next_power_of_two();

        // Zero-padded N×N complex matrix (real = count).
        let mut mat = vec![Complex::<f32>::new(0.0, 0.0); n * n];
        for r in 0..cap {
            for c in 0..cap {
                #[allow(clippy::cast_precision_loss)]
                let v = self.counts[r * cap + c] as f32;
                mat[r * n + c] = Complex::new(v, 0.0);
            }
        }

        let mut planner = FftPlanner::<f32>::new();
        let fft = planner.plan_fft_forward(n);

        // Row pass.
        for r in 0..n {
            fft.process(&mut mat[r * n..r * n + n]);
        }
        // Column pass (gather stride-N column, transform, scatter back).
        let mut col = vec![Complex::<f32>::new(0.0, 0.0); n];
        for c in 0..n {
            for (r, slot) in col.iter_mut().enumerate() {
                *slot = mat[r * n + c];
            }
            fft.process(&mut col);
            for (r, &v) in col.iter().enumerate() {
                mat[r * n + c] = v;
            }
        }

        // Magnitudes, excluding the DC bin [0,0].
        let mut total = 0.0_f32;
        let mut peak = 0.0_f32;
        let mut low_freq = 0.0_f32;
        let quad = (n / 4).max(1);
        for r in 0..n {
            for c in 0..n {
                if r == 0 && c == 0 {
                    continue; // DC = total event count; not informative.
                }
                let mag = mat[r * n + c].norm();
                total += mag;
                if mag > peak {
                    peak = mag;
                }
                if r < quad && c < quad {
                    low_freq += mag;
                }
            }
        }

        if total < 1e-12 {
            return BigramSpectralMetrics::default();
        }

        // Shannon entropy of the magnitude distribution, normalized.
        let mut entropy = 0.0_f32;
        for r in 0..n {
            for c in 0..n {
                if r == 0 && c == 0 {
                    continue;
                }
                let p = mat[r * n + c].norm() / total;
                if p > 1e-12 {
                    entropy -= p * p.ln();
                }
            }
        }
        #[allow(clippy::cast_precision_loss)]
        let max_entropy = ((n * n - 1) as f32).ln();
        let spectral_entropy = if max_entropy > 0.0 {
            (entropy / max_entropy).clamp(0.0, 1.0)
        } else {
            0.0
        };

        BigramSpectralMetrics {
            peak_strength: (peak / total).clamp(0.0, 1.0),
            low_freq_concentration: (low_freq / total).clamp(0.0, 1.0),
            spectral_entropy,
        }
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn record_ignores_out_of_range() {
        let mut s = BigramSpectrum::new(4);
        s.record(0, 1);
        s.record(99, 1); // ignored
        s.record(1, 99); // ignored
        assert_eq!(s.total_events(), 1);
    }

    #[test]
    fn empty_spectrum_is_zero_metrics() {
        let mut s = BigramSpectrum::new(8);
        let m = s.compute();
        assert_eq!(m, BigramSpectralMetrics::default());
    }

    #[test]
    fn structured_corpus_has_lower_entropy_than_noise() {
        // Structured: a strict repeating cycle 0→1→2→…→0 — a single strong
        // off-DC spatial frequency → energy concentrated → low entropy.
        // Noise: random (i,j) bigrams → energy spread across many
        // frequencies → high entropy. (A *constant* matrix would be pure
        // DC, which we exclude — so the right contrast is structure vs
        // randomness, not structure vs uniform.)
        let cap = 8;
        let mut structured = BigramSpectrum::new(cap);
        for _ in 0..200 {
            for i in 0..cap {
                structured.record(i, (i + 1) % cap);
            }
        }
        let mut noise = BigramSpectrum::new(cap);
        let mut rng = crate::XorShift64::new(0xBEEF);
        for _ in 0..(200 * cap) {
            let i = (rng.next_u64() as usize) % cap;
            let j = (rng.next_u64() as usize) % cap;
            noise.record(i, j);
        }
        let ms = structured.compute();
        let mn = noise.compute();
        assert!(
            ms.spectral_entropy < mn.spectral_entropy,
            "structured entropy {} should be < noise {}",
            ms.spectral_entropy,
            mn.spectral_entropy
        );
        // Structured signal concentrates into a stronger single peak.
        assert!(
            ms.peak_strength > mn.peak_strength,
            "structured peak {} should exceed noise peak {}",
            ms.peak_strength,
            mn.peak_strength
        );
    }

    #[test]
    fn metrics_are_bounded() {
        let mut s = BigramSpectrum::new(16);
        for i in 0..16 {
            for _ in 0..(i + 1) {
                s.record(i, (i * 3) % 16);
            }
        }
        let m = s.compute();
        for v in [m.peak_strength, m.low_freq_concentration, m.spectral_entropy] {
            assert!((0.0..=1.0).contains(&v), "metric out of range: {v}");
        }
    }

    #[test]
    fn maybe_compute_respects_delta() {
        let mut s = BigramSpectrum::new(4);
        s.record(0, 1);
        let (_, ran1) = s.maybe_compute(10);
        assert!(ran1, "first call computes");
        s.record(1, 2);
        let (_, ran2) = s.maybe_compute(10);
        assert!(!ran2, "below delta → cached");
        for _ in 0..10 {
            s.record(2, 3);
        }
        let (_, ran3) = s.maybe_compute(10);
        assert!(ran3, "delta exceeded → recompute");
    }

    #[test]
    fn reset_clears() {
        let mut s = BigramSpectrum::new(4);
        s.record(0, 1);
        s.compute();
        s.reset();
        assert_eq!(s.total_events(), 0);
        assert!(s.cached_metrics().is_none());
    }

    #[test]
    fn serde_round_trip() {
        let mut s = BigramSpectrum::new(4);
        s.record(0, 1);
        s.record(1, 2);
        let json = serde_json::to_string(&s).unwrap();
        let back: BigramSpectrum = serde_json::from_str(&json).unwrap();
        assert_eq!(back.total_events(), 2);
        assert_eq!(back.vocab_cap(), 4);
    }
}

//! 1-D spectral convolution layer.
//!
//! Implements the FNO spectral mixing step:
//!   FFT(x) → multiply low-freq modes by learnable complex matrix →
//!   IFFT → take real part.
//!
//! Storage:
//! - `weight: Array3<Complex<f32>>` shape `[modes, in_channels, out_channels]`.
//!
//! Forward operates on `[batch, in_channels, length]` real tensors and
//! produces `[batch, out_channels, length]` real tensors.

use ndarray::{Array3, Axis};
use num_complex::Complex;
use rand::SeedableRng;
use rand::distr::{Distribution, Uniform};
use rand_chacha::ChaCha20Rng;
use rustfft::FftPlanner;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpectralConv1dLayer {
    pub in_channels: usize,
    pub out_channels: usize,
    /// Number of low-frequency modes to retain. Must satisfy
    /// `2 * modes <= length` for any input `length`.
    pub modes: usize,
    /// `[modes, in_channels, out_channels]` — complex per-mode mixing.
    pub weight_re: Array3<f32>,
    pub weight_im: Array3<f32>,
}

impl SpectralConv1dLayer {
    pub fn new(in_channels: usize, out_channels: usize, modes: usize, seed: u64) -> Self {
        assert!(
            in_channels > 0 && out_channels > 0 && modes > 0,
            "spectral: zero dim"
        );
        let mut rng = ChaCha20Rng::seed_from_u64(seed);
        let bound = 1.0_f32 / (in_channels as f32 * modes as f32);
        let dist = Uniform::new(-bound, bound).expect("scale > 0");

        let mut wr = Array3::<f32>::zeros((modes, in_channels, out_channels));
        let mut wi = Array3::<f32>::zeros((modes, in_channels, out_channels));
        for v in wr.iter_mut() {
            *v = dist.sample(&mut rng);
        }
        for v in wi.iter_mut() {
            *v = dist.sample(&mut rng);
        }
        Self {
            in_channels,
            out_channels,
            modes,
            weight_re: wr,
            weight_im: wi,
        }
    }

    /// Apply the spectral conv. `input` is `[N, in_channels, L]`;
    /// output is `[N, out_channels, L]`.
    ///
    /// # Panics
    /// If `input.shape()[1]` ≠ `in_channels` or `length < 2 * modes`.
    // Index loops deliberately mirror `W[k, ic, oc] · in[ic, k]` in the
    // spectral-convolution equation; iterator form obscures the math.
    #[allow(clippy::needless_range_loop)]
    pub fn forward(&self, input: &ndarray::Array3<f32>) -> ndarray::Array3<f32> {
        let (n, c_in, length) = input.dim();
        assert_eq!(c_in, self.in_channels, "spectral: input channel mismatch");
        assert!(
            length >= 2 * self.modes,
            "spectral: length {length} too short for modes {} (need >= {})",
            self.modes,
            2 * self.modes
        );

        // Plan the FFT once per call. The planner caches internally so
        // repeated calls with the same length are cheap.
        let mut planner = FftPlanner::<f32>::new();
        let fft_fwd = planner.plan_fft_forward(length);
        let fft_inv = planner.plan_fft_inverse(length);

        // Output-frequency buffer per (n, out_c). We accumulate into
        // this then apply IFFT per row.
        let mut output = ndarray::Array3::<f32>::zeros((n, self.out_channels, length));

        // Per-batch scratch: forward bins for every input channel +
        // accumulated bins for every output channel.
        let mut in_bins: Vec<Vec<Complex<f32>>> = (0..self.in_channels)
            .map(|_| vec![Complex::new(0.0, 0.0); length])
            .collect();
        let mut out_bins: Vec<Vec<Complex<f32>>> = (0..self.out_channels)
            .map(|_| vec![Complex::new(0.0, 0.0); length])
            .collect();

        for ni in 0..n {
            // Forward FFT each input channel.
            let batch_slab = input.index_axis(Axis(0), ni);
            for ic in 0..self.in_channels {
                let row = batch_slab.index_axis(Axis(0), ic);
                for (i, x) in row.iter().enumerate() {
                    in_bins[ic][i] = Complex::new(*x, 0.0);
                }
                fft_fwd.process(&mut in_bins[ic]);
            }

            // Zero the accumulator.
            for buf in out_bins.iter_mut() {
                for v in buf.iter_mut() {
                    *v = Complex::new(0.0, 0.0);
                }
            }

            // For each retained mode: out[oc, k] = Σ_ic W[k, ic, oc] · in[ic, k].
            for k in 0..self.modes {
                for ic in 0..self.in_channels {
                    let xk = in_bins[ic][k];
                    for oc in 0..self.out_channels {
                        let w =
                            Complex::new(self.weight_re[[k, ic, oc]], self.weight_im[[k, ic, oc]]);
                        out_bins[oc][k] += w * xk;
                    }
                }
            }
            // High-frequency bins (modes..length) stay zero by virtue
            // of the buffer reset above — that's the spectral truncation.

            // Inverse FFT each output channel; rustfft's inverse is
            // unscaled, so divide by length.
            let scale = 1.0_f32 / length as f32;
            for oc in 0..self.out_channels {
                fft_inv.process(&mut out_bins[oc]);
                for (i, c) in out_bins[oc].iter().enumerate() {
                    output[[ni, oc, i]] = c.re * scale;
                }
            }
        }

        output
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ndarray::Array3;

    #[test]
    fn fft_ifft_round_trip_recovers_input() {
        // Pure plumbing test — make sure rustfft + our scaling is right.
        let length = 32;
        let signal: Vec<f32> = (0..length).map(|i| (i as f32 * 0.3).sin()).collect();

        let mut planner = FftPlanner::<f32>::new();
        let fft_fwd = planner.plan_fft_forward(length);
        let fft_inv = planner.plan_fft_inverse(length);

        let mut buf: Vec<Complex<f32>> = signal.iter().map(|&v| Complex::new(v, 0.0)).collect();
        fft_fwd.process(&mut buf);
        fft_inv.process(&mut buf);

        let scale = 1.0_f32 / length as f32;
        for (i, c) in buf.iter().enumerate() {
            let recovered = c.re * scale;
            assert!(
                (recovered - signal[i]).abs() < 1e-4,
                "fft round-trip drift at {i}: {recovered} vs {}",
                signal[i]
            );
        }
    }

    #[test]
    fn spectral_conv_preserves_length_and_produces_finite() {
        let layer = SpectralConv1dLayer::new(3, 5, 4, 0xFEED);
        let length = 16;
        let input = Array3::from_shape_fn((2, 3, length), |(n, c, l)| {
            ((n + c) as f32 + l as f32 * 0.1).cos()
        });
        let out = layer.forward(&input);
        assert_eq!(out.dim(), (2, 5, length));
        for v in out.iter() {
            assert!(v.is_finite(), "spectral output not finite: {v}");
        }
    }

    #[test]
    fn spectral_conv_is_deterministic_under_seed() {
        let a = SpectralConv1dLayer::new(2, 4, 3, 0xC0DE);
        let b = SpectralConv1dLayer::new(2, 4, 3, 0xC0DE);
        assert_eq!(a.weight_re, b.weight_re);
        assert_eq!(a.weight_im, b.weight_im);
    }
}

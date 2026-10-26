//! 2-D convolutional layer (cross-correlation, no FFT).
//!
//! Storage:
//! - Weights `[out_channels, in_channels, kh, kw]`.
//! - Bias `[out_channels]`.
//!
//! Forward applies stride + zero padding via plain nested loops. LLVM
//! auto-vectorises the inner row × kernel multiply for typical kernel
//! sizes (3×3, 5×5, 7×7).
//!
//! Init: Xavier-uniform from a [`rand_chacha::ChaCha20Rng`] — same
//! convention as `LinearLayer` and the LNN, so deterministic-seed tests
//! work end-to-end.

use ndarray::{Array1, Array4};
use rand::SeedableRng;
use rand::distr::{Distribution, Uniform};
use rand_chacha::ChaCha20Rng;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Conv2dLayer {
    pub in_channels: usize,
    pub out_channels: usize,
    pub kh: usize,
    pub kw: usize,
    pub stride_h: usize,
    pub stride_w: usize,
    pub pad_h: usize,
    pub pad_w: usize,
    /// `[out_channels, in_channels, kh, kw]`.
    pub weight: Array4<f32>,
    /// `[out_channels]`.
    pub bias: Array1<f32>,
}

impl Conv2dLayer {
    /// Allocate + Xavier-initialise. `seed` drives the deterministic
    /// RNG; same `seed` → bit-identical weights across runs.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        in_channels: usize,
        out_channels: usize,
        kh: usize,
        kw: usize,
        stride_h: usize,
        stride_w: usize,
        pad_h: usize,
        pad_w: usize,
        seed: u64,
    ) -> Self {
        assert!(in_channels > 0 && out_channels > 0, "conv: zero channels");
        assert!(kh > 0 && kw > 0, "conv: zero kernel");
        assert!(stride_h > 0 && stride_w > 0, "conv: zero stride");

        let mut rng = ChaCha20Rng::seed_from_u64(seed);
        let fan_in = (in_channels * kh * kw) as f32;
        let fan_out = (out_channels * kh * kw) as f32;
        let bound = (6.0_f32 / (fan_in + fan_out)).sqrt();
        let dist = Uniform::new(-bound, bound).expect("xavier bound > 0");

        let mut w = Array4::<f32>::zeros((out_channels, in_channels, kh, kw));
        for v in w.iter_mut() {
            *v = dist.sample(&mut rng);
        }

        Self {
            in_channels,
            out_channels,
            kh,
            kw,
            stride_h,
            stride_w,
            pad_h,
            pad_w,
            weight: w,
            bias: Array1::<f32>::zeros(out_channels),
        }
    }

    /// Convenience constructor for the common `stride=1, pad=k/2` case.
    pub fn same_padding(in_channels: usize, out_channels: usize, k: usize, seed: u64) -> Self {
        Self::new(in_channels, out_channels, k, k, 1, 1, k / 2, k / 2, seed)
    }

    /// Output `(H_out, W_out)` for given input H/W under the layer's
    /// stride + padding.
    pub fn output_hw(&self, h_in: usize, w_in: usize) -> (usize, usize) {
        let h_out = (h_in + 2 * self.pad_h - self.kh) / self.stride_h + 1;
        let w_out = (w_in + 2 * self.pad_w - self.kw) / self.stride_w + 1;
        (h_out, w_out)
    }

    /// Cross-correlation forward. Input `[N, C_in, H, W]` →
    /// `[N, C_out, H_out, W_out]`.
    pub fn forward(&self, input: &Array4<f32>) -> Array4<f32> {
        let (n, c_in, h_in, w_in) = input.dim();
        assert_eq!(c_in, self.in_channels, "conv input channel mismatch");

        let h_padded = h_in + 2 * self.pad_h;
        let w_padded = w_in + 2 * self.pad_w;
        assert!(
            h_padded >= self.kh && w_padded >= self.kw,
            "conv: padded input smaller than kernel"
        );
        let (h_out, w_out) = self.output_hw(h_in, w_in);
        let mut out = Array4::<f32>::zeros((n, self.out_channels, h_out, w_out));

        for ni in 0..n {
            for oc in 0..self.out_channels {
                let bias = self.bias[oc];
                for oh in 0..h_out {
                    for ow in 0..w_out {
                        let mut acc = bias;
                        let h0 = oh * self.stride_h;
                        let w0 = ow * self.stride_w;
                        for ic in 0..self.in_channels {
                            for di in 0..self.kh {
                                // Effective input row, accounting for padding.
                                let ih = h0 + di;
                                if ih < self.pad_h || ih >= self.pad_h + h_in {
                                    continue;
                                }
                                let ih_in = ih - self.pad_h;
                                for dj in 0..self.kw {
                                    let iw = w0 + dj;
                                    if iw < self.pad_w || iw >= self.pad_w + w_in {
                                        continue;
                                    }
                                    let iw_in = iw - self.pad_w;
                                    acc += input[[ni, ic, ih_in, iw_in]]
                                        * self.weight[[oc, ic, di, dj]];
                                }
                            }
                        }
                        out[[ni, oc, oh, ow]] = acc;
                    }
                }
            }
        }
        out
    }
}

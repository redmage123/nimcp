//! Per-position linear mix — equivalent to a 1×1 1-D convolution.
//!
//! Mixes channels at each spatial position independently. Storage is a
//! dense `[out_channels, in_channels]` matrix; forward is a per-position
//! matmul.

use ndarray::{Array1, Array2, Array3, Axis};
use rand::SeedableRng;
use rand::distr::{Distribution, Uniform};
use rand_chacha::ChaCha20Rng;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LinearMixLayer {
    pub in_channels: usize,
    pub out_channels: usize,
    /// `[out_channels, in_channels]`.
    pub weight: Array2<f32>,
    /// `[out_channels]`.
    pub bias: Array1<f32>,
}

impl LinearMixLayer {
    pub fn new(in_channels: usize, out_channels: usize, seed: u64) -> Self {
        assert!(in_channels > 0 && out_channels > 0, "linear_mix: zero dim");
        let mut rng = ChaCha20Rng::seed_from_u64(seed);
        let bound = (6.0_f32 / (in_channels + out_channels) as f32).sqrt();
        let dist = Uniform::new(-bound, bound).expect("xavier bound > 0");
        let mut w = Array2::<f32>::zeros((out_channels, in_channels));
        for v in w.iter_mut() {
            *v = dist.sample(&mut rng);
        }
        Self {
            in_channels,
            out_channels,
            weight: w,
            bias: Array1::<f32>::zeros(out_channels),
        }
    }

    /// `input: [N, in_channels, L]` → `[N, out_channels, L]`.
    pub fn forward(&self, input: &Array3<f32>) -> Array3<f32> {
        let (n, c_in, length) = input.dim();
        assert_eq!(c_in, self.in_channels, "linear_mix: input channel mismatch");
        let mut out = Array3::<f32>::zeros((n, self.out_channels, length));

        for ni in 0..n {
            let in_slab = input.index_axis(Axis(0), ni); // [c_in, L]
            // Out[oc, l] = Σ_ic W[oc, ic] · in[ic, l] + b[oc].
            let mut out_slab = self.weight.dot(&in_slab); // [out_c, L]
            for (oc, mut row) in out_slab.rows_mut().into_iter().enumerate() {
                let b = self.bias[oc];
                for v in row.iter_mut() {
                    *v += b;
                }
            }
            out.index_axis_mut(Axis(0), ni).assign(&out_slab);
        }
        out
    }
}

//! Phase 11b-train — FNO backward pass + SGD.
//!
//! # Spectral conv backward — chain-rule derivation
//!
//! Forward (per batch element, ignoring `n` axis):
//!
//! ```text
//! X[ic, k]  = FFT_unscaled(in[ic, :])[k]                         (complex)
//! Y[oc, k]  = sum_ic W[k, ic, oc] · X[ic, k]   (k < modes; else 0)
//! out[oc, l] = Re( IFFT_unscaled(Y[oc, :])[l] ) / L              (real)
//! ```
//!
//! With real-valued upstream `grad_out[oc, l]`, treating the loss
//! `L` as a function of complex intermediates (Wirtinger calculus
//! collapsed to real/imag pairs):
//!
//! ```text
//! G[oc, k]      = FFT_unscaled(grad_out[oc, :])[k]               (complex)
//! dL/dY[oc, k]  = conj(G[oc, k]) / L              (only k < modes contributes)
//!
//! grad_W[k, ic, oc]  =  conj(X[ic, k]) · dL/dY[oc, k]            (complex)
//! grad_X[ic, k]      =  sum_oc conj(W[k, ic, oc]) · dL/dY[oc, k] (k < modes; else 0)
//!
//! grad_in[ic, l] = Re( IFFT_unscaled(grad_X[ic, :])[l] )         (real)
//! ```
//!
//! # Scope (Phase 11b-train)
//!
//! - MSE loss only (re-uses [`crate::network::FnoNetwork`] forward).
//! - Vanilla SGD; matches the [`nimcp_cnn`] convention.
//! - CPU only — GPU spectral backward is a follow-up phase.

use ndarray::{Array1, Array2, Array3};
use num_complex::Complex;
use rustfft::FftPlanner;

use crate::block::FnoBlock;
use crate::linear_mix::LinearMixLayer;
use crate::network::FnoNetwork;
use crate::spectral::SpectralConv1dLayer;

// ---------------------------------------------------------------------------
// LinearMix backward.
// ---------------------------------------------------------------------------

impl LinearMixLayer {
    /// Backward through a per-position 1×1 conv. Accumulates grads
    /// into `grad_weight` and `grad_bias`; returns `grad_input`.
    pub fn backward(
        &self,
        input: &Array3<f32>,
        grad_out: &Array3<f32>,
        grad_weight: &mut Array2<f32>,
        grad_bias: &mut Array1<f32>,
    ) -> Array3<f32> {
        let (n, c_in, length) = input.dim();
        debug_assert_eq!(grad_out.dim(), (n, self.out_channels, length));
        let mut grad_in = Array3::<f32>::zeros((n, c_in, length));

        for ni in 0..n {
            for l in 0..length {
                // grad_w[oc, ic] += grad_out[oc, l] * in[ic, l]
                // grad_b[oc]     += grad_out[oc, l]
                // grad_in[ic, l] += sum_oc W[oc, ic] * grad_out[oc, l]
                for oc in 0..self.out_channels {
                    let g = grad_out[[ni, oc, l]];
                    grad_bias[oc] += g;
                    for ic in 0..c_in {
                        grad_weight[[oc, ic]] += g * input[[ni, ic, l]];
                        grad_in[[ni, ic, l]] += self.weight[[oc, ic]] * g;
                    }
                }
            }
        }
        grad_in
    }
}

// ---------------------------------------------------------------------------
// Spectral conv backward.
// ---------------------------------------------------------------------------

impl SpectralConv1dLayer {
    /// Backward through a 1-D spectral conv. Returns `grad_input`;
    /// accumulates per-mode complex weight gradients into the
    /// supplied `grad_weight_re` / `grad_weight_im` buffers.
    pub fn backward(
        &self,
        input: &Array3<f32>,
        grad_out: &Array3<f32>,
        grad_weight_re: &mut ndarray::Array3<f32>,
        grad_weight_im: &mut ndarray::Array3<f32>,
    ) -> Array3<f32> {
        let (n, c_in, length) = input.dim();
        debug_assert_eq!(c_in, self.in_channels);
        debug_assert_eq!(grad_out.dim(), (n, self.out_channels, length));
        debug_assert!(length >= 2 * self.modes);

        let mut planner = FftPlanner::<f32>::new();
        let fft_fwd = planner.plan_fft_forward(length);
        let fft_inv = planner.plan_fft_inverse(length);
        let scale = 1.0_f32 / length as f32;

        let mut grad_in = Array3::<f32>::zeros(input.dim());

        // Per-batch scratch.
        let mut x_bins: Vec<Vec<Complex<f32>>> = (0..self.in_channels)
            .map(|_| vec![Complex::new(0.0, 0.0); length])
            .collect();
        let mut g_bins: Vec<Vec<Complex<f32>>> = (0..self.out_channels)
            .map(|_| vec![Complex::new(0.0, 0.0); length])
            .collect();
        let mut grad_x_bins: Vec<Vec<Complex<f32>>> = (0..self.in_channels)
            .map(|_| vec![Complex::new(0.0, 0.0); length])
            .collect();

        for ni in 0..n {
            // FFT each input channel (real → complex).
            for ic in 0..self.in_channels {
                for l in 0..length {
                    x_bins[ic][l] = Complex::new(input[[ni, ic, l]], 0.0);
                }
                fft_fwd.process(&mut x_bins[ic]);
            }
            // FFT each grad_out channel (real → complex).
            for oc in 0..self.out_channels {
                for l in 0..length {
                    g_bins[oc][l] = Complex::new(grad_out[[ni, oc, l]], 0.0);
                }
                fft_fwd.process(&mut g_bins[oc]);
            }
            // Reset grad_X accumulator.
            for buf in grad_x_bins.iter_mut() {
                for v in buf.iter_mut() {
                    *v = Complex::new(0.0, 0.0);
                }
            }

            // For each retained mode k, each (ic, oc):
            // dL/dY[oc, k] = conj(G[oc, k]) / L
            // grad_W[k, ic, oc] += conj(X[ic, k]) * dL/dY[oc, k]
            // grad_X[ic, k]     += conj(W[k, ic, oc]) * dL/dY[oc, k]
            for k in 0..self.modes {
                for oc in 0..self.out_channels {
                    let dl_dy = g_bins[oc][k].conj() * scale;
                    for ic in 0..self.in_channels {
                        let conj_x = x_bins[ic][k].conj();
                        let dgw = conj_x * dl_dy;
                        grad_weight_re[[k, ic, oc]] += dgw.re;
                        grad_weight_im[[k, ic, oc]] += dgw.im;
                        let conj_w = Complex::new(
                            self.weight_re[[k, ic, oc]],
                            -self.weight_im[[k, ic, oc]],
                        );
                        grad_x_bins[ic][k] += conj_w * dl_dy;
                    }
                }
            }

            // IFFT each grad_X_bins (high modes already zero) and take
            // the real part as grad_in.
            for ic in 0..self.in_channels {
                fft_inv.process(&mut grad_x_bins[ic]);
                for l in 0..length {
                    grad_in[[ni, ic, l]] = grad_x_bins[ic][l].re;
                }
            }
        }
        grad_in
    }
}

// ---------------------------------------------------------------------------
// Block backward — `out = tanh(spec(in) + mix(in))`.
// ---------------------------------------------------------------------------

/// Per-block gradient buffers.
#[derive(Debug, Clone)]
pub struct FnoBlockGrads {
    pub spec_w_re: ndarray::Array3<f32>,
    pub spec_w_im: ndarray::Array3<f32>,
    pub mix_w: Array2<f32>,
    pub mix_b: Array1<f32>,
}

impl FnoBlockGrads {
    pub fn zeros_for(block: &FnoBlock) -> Self {
        Self {
            spec_w_re: ndarray::Array3::<f32>::zeros(block.spectral.weight_re.dim()),
            spec_w_im: ndarray::Array3::<f32>::zeros(block.spectral.weight_im.dim()),
            mix_w: Array2::<f32>::zeros(block.linear_mix.weight.dim()),
            mix_b: Array1::<f32>::zeros(block.linear_mix.bias.len()),
        }
    }
}

impl FnoBlock {
    /// Backward through `tanh(spec(in) + mix(in))`. Returns `grad_input`.
    /// `cached_post_tanh` is the block's forward output (we re-use it
    /// for `tanh' = 1 - y²`, avoiding an extra forward pass).
    pub fn backward(
        &self,
        input: &Array3<f32>,
        cached_post_tanh: &Array3<f32>,
        grad_out: &Array3<f32>,
        grads: &mut FnoBlockGrads,
    ) -> Array3<f32> {
        // Step 1: through tanh — dL/d_pre = dL/d_out * (1 - y²).
        let mut grad_pre = Array3::<f32>::zeros(grad_out.dim());
        for ((g, gp), y) in grad_out
            .iter()
            .zip(grad_pre.iter_mut())
            .zip(cached_post_tanh.iter())
        {
            *gp = *g * (1.0 - y * y);
        }
        // Step 2: split into spectral + linear-mix branches (both
        // receive the full grad_pre because forward summed them).
        let g_in_spec = self.spectral.backward(
            input,
            &grad_pre,
            &mut grads.spec_w_re,
            &mut grads.spec_w_im,
        );
        let g_in_mix = self.linear_mix.backward(
            input,
            &grad_pre,
            &mut grads.mix_w,
            &mut grads.mix_b,
        );
        // Step 3: sum the two input gradients.
        let mut grad_in = g_in_spec;
        for (a, b) in grad_in.iter_mut().zip(g_in_mix.iter()) {
            *a += *b;
        }
        grad_in
    }
}

// ---------------------------------------------------------------------------
// Network-level gradients + train step.
// ---------------------------------------------------------------------------

/// Full FNO gradient bag.
#[derive(Debug, Clone)]
pub struct FnoGradients {
    pub input_proj_w: Array2<f32>,
    pub input_proj_b: Array1<f32>,
    pub blocks: Vec<FnoBlockGrads>,
    pub output_proj_w: Array2<f32>,
    pub output_proj_b: Array1<f32>,
}

impl FnoGradients {
    pub fn zeros_for(net: &FnoNetwork) -> Self {
        Self {
            input_proj_w: Array2::<f32>::zeros(net.input_proj.weight.dim()),
            input_proj_b: Array1::<f32>::zeros(net.input_proj.bias.len()),
            blocks: net.blocks.iter().map(FnoBlockGrads::zeros_for).collect(),
            output_proj_w: Array2::<f32>::zeros(net.output_proj.weight.dim()),
            output_proj_b: Array1::<f32>::zeros(net.output_proj.bias.len()),
        }
    }
}

/// Returns `(loss, grad_output)` for batched MSE between `output` and
/// `target`, both `[N, C, L]`.
pub fn mse_loss(output: &Array3<f32>, target: &Array3<f32>) -> (f32, Array3<f32>) {
    assert_eq!(output.dim(), target.dim(), "fno mse_loss shape mismatch");
    let (n, c, l) = output.dim();
    let denom = (n * c * l).max(1) as f32;
    let mut grad = Array3::<f32>::zeros((n, c, l));
    let mut loss = 0.0_f32;
    for ((og, tg), gg) in output.iter().zip(target.iter()).zip(grad.iter_mut()) {
        let d = *og - *tg;
        loss += d * d;
        *gg = 2.0 * d / denom;
    }
    loss /= denom;
    (loss, grad)
}

/// One forward + backward + SGD step. Returns `(loss, grad_norm)`.
pub fn train_step_mse(
    net: &mut FnoNetwork,
    input: &Array3<f32>,
    target: &Array3<f32>,
    lr: f32,
) -> (f32, f32) {
    let (loss, grads, grad_norm) = backward_grads(net, input, target);
    sgd_step(net, &grads, lr);
    (loss, grad_norm)
}

/// Phase 11-substrate — substrate-aware train step. Identical to
/// [`train_step_mse`] when `substrate_cfg` is disabled. When enabled:
///
/// - `lr_eff = lr × dend.plasticity_mod` (cadence-refreshed effects).
/// - Asymmetric LTP/LTD gating on each gradient component.
/// - A single plasticity event is debited from the substrate after the
///   step.
///
/// The forward used to compute gradients is the *un-modulated* forward —
/// substrate output-gain is an inference effect, not a training-target
/// distortion. Modulation enters training only through LR + LTP/LTD.
pub fn train_step_mse_modulated(
    net: &mut FnoNetwork,
    input: &Array3<f32>,
    target: &Array3<f32>,
    lr: f32,
) -> (f32, f32) {
    if !net.substrate_cfg.enabled {
        return train_step_mse(net, input, target, lr);
    }
    let should_recompute = net.substrate_effects.is_none()
        || net.substrate_tick_counter >= net.substrate_cfg.update_period;
    if should_recompute {
        net.recompute_substrate_effects();
        net.substrate_tick_counter = 0;
    } else {
        net.substrate_tick_counter = net.substrate_tick_counter.saturating_add(1);
    }

    let (loss, grads, grad_norm) = backward_grads(net, input, target);

    let lr_eff = crate::substrate_adapter::effective_lr(
        lr,
        net.substrate_effects.as_ref(),
        net.substrate_cfg.plasticity_mod_on,
    );
    let (ltp, ltd) = crate::substrate_adapter::ltp_ltd_gates(
        net.substrate_effects.as_ref(),
        net.substrate_cfg.ltp_ltd_asymmetry_on,
    );
    sgd_step_gated(net, &grads, lr_eff, ltp, ltd);

    if let Some(ref mut s) = net.substrate_state {
        nimcp_substrate::debit_activity(s, &net.substrate_cfg.dynamics, 0, 1);
    }

    (loss, grad_norm)
}

/// Forward + backward against an MSE target. Returns
/// `(loss, gradients, grad_norm)` without applying any update — shared
/// by [`train_step_mse`] and [`train_step_mse_modulated`].
fn backward_grads(
    net: &FnoNetwork,
    input: &Array3<f32>,
    target: &Array3<f32>,
) -> (f32, FnoGradients, f32) {
    // Forward — cache the output of every layer for backward.
    let h0 = net.input_proj.forward(input);
    let mut block_outs: Vec<Array3<f32>> = Vec::with_capacity(net.blocks.len());
    let mut h_curr = h0.clone();
    for block in &net.blocks {
        let next = block.forward(&h_curr);
        block_outs.push(next.clone());
        h_curr = next;
    }
    let output = net.output_proj.forward(&h_curr);

    let (loss, grad_out) = mse_loss(&output, target);
    let mut grads = FnoGradients::zeros_for(net);

    // Output projection backward.
    let g_h_last = net.output_proj.backward(
        &h_curr,
        &grad_out,
        &mut grads.output_proj_w,
        &mut grads.output_proj_b,
    );

    // Walk blocks in reverse.
    let mut g_h = g_h_last;
    for (i, block) in net.blocks.iter().enumerate().rev() {
        let block_input = if i == 0 { &h0 } else { &block_outs[i - 1] };
        let block_output = &block_outs[i];
        g_h = block.backward(block_input, block_output, &g_h, &mut grads.blocks[i]);
    }

    // Input projection backward.
    let _g_input = net.input_proj.backward(
        input,
        &g_h,
        &mut grads.input_proj_w,
        &mut grads.input_proj_b,
    );

    // Gradient norm (sum of squares across every accumulator).
    let mut sq = 0.0_f32;
    for v in grads.input_proj_w.iter().chain(grads.input_proj_b.iter()) {
        sq += v * v;
    }
    for v in grads.output_proj_w.iter().chain(grads.output_proj_b.iter()) {
        sq += v * v;
    }
    for bg in &grads.blocks {
        for v in bg.spec_w_re.iter().chain(bg.spec_w_im.iter()) {
            sq += v * v;
        }
        for v in bg.mix_w.iter().chain(bg.mix_b.iter()) {
            sq += v * v;
        }
    }
    let grad_norm = sq.sqrt();

    (loss, grads, grad_norm)
}

pub fn sgd_step(net: &mut FnoNetwork, grads: &FnoGradients, lr: f32) {
    for (w, g) in net.input_proj.weight.iter_mut().zip(grads.input_proj_w.iter()) {
        *w -= lr * *g;
    }
    for (b, g) in net.input_proj.bias.iter_mut().zip(grads.input_proj_b.iter()) {
        *b -= lr * *g;
    }
    for (w, g) in net.output_proj.weight.iter_mut().zip(grads.output_proj_w.iter()) {
        *w -= lr * *g;
    }
    for (b, g) in net.output_proj.bias.iter_mut().zip(grads.output_proj_b.iter()) {
        *b -= lr * *g;
    }
    for (block, bg) in net.blocks.iter_mut().zip(grads.blocks.iter()) {
        for (w, g) in block.spectral.weight_re.iter_mut().zip(bg.spec_w_re.iter()) {
            *w -= lr * *g;
        }
        for (w, g) in block.spectral.weight_im.iter_mut().zip(bg.spec_w_im.iter()) {
            *w -= lr * *g;
        }
        for (w, g) in block.linear_mix.weight.iter_mut().zip(bg.mix_w.iter()) {
            *w -= lr * *g;
        }
        for (b, g) in block.linear_mix.bias.iter_mut().zip(bg.mix_b.iter()) {
            *b -= lr * *g;
        }
    }
}

/// Like [`sgd_step`] but applies asymmetric LTP/LTD gating: a gradient
/// component is scaled by `ltp` when it would *raise* the weight (`g < 0`
/// under `param -= lr*g`) and by `ltd` when it would *lower* it.
/// `(ltp, ltd) = (1.0, 1.0)` is bit-identical to [`sgd_step`].
pub fn sgd_step_gated(net: &mut FnoNetwork, grads: &FnoGradients, lr: f32, ltp: f32, ltd: f32) {
    let gate = |g: f32| -> f32 {
        if g < 0.0 { g * ltp } else { g * ltd }
    };
    for (w, g) in net.input_proj.weight.iter_mut().zip(grads.input_proj_w.iter()) {
        *w -= lr * gate(*g);
    }
    for (b, g) in net.input_proj.bias.iter_mut().zip(grads.input_proj_b.iter()) {
        *b -= lr * gate(*g);
    }
    for (w, g) in net.output_proj.weight.iter_mut().zip(grads.output_proj_w.iter()) {
        *w -= lr * gate(*g);
    }
    for (b, g) in net.output_proj.bias.iter_mut().zip(grads.output_proj_b.iter()) {
        *b -= lr * gate(*g);
    }
    for (block, bg) in net.blocks.iter_mut().zip(grads.blocks.iter()) {
        for (w, g) in block.spectral.weight_re.iter_mut().zip(bg.spec_w_re.iter()) {
            *w -= lr * gate(*g);
        }
        for (w, g) in block.spectral.weight_im.iter_mut().zip(bg.spec_w_im.iter()) {
            *w -= lr * gate(*g);
        }
        for (w, g) in block.linear_mix.weight.iter_mut().zip(bg.mix_w.iter()) {
            *w -= lr * gate(*g);
        }
        for (b, g) in block.linear_mix.bias.iter_mut().zip(bg.mix_b.iter()) {
            *b -= lr * gate(*g);
        }
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::network::FnoConfig;

    /// Validate spectral grad_input via finite differences on a single
    /// pixel of the input. Uses small random network so the relationship
    /// is non-trivial but bounded.
    #[test]
    fn spectral_grad_input_matches_finite_difference() {
        let layer = SpectralConv1dLayer::new(2, 2, 3, 0xCAFE);
        let length = 8;
        let mut input = Array3::from_shape_fn((1, 2, length), |(_, c, l)| {
            ((c + 1) as f32 * l as f32 * 0.07).sin()
        });
        let target = Array3::from_shape_fn((1, 2, length), |(_, _, l)| (l as f32 * 0.1).cos());

        let out = layer.forward(&input);
        let (_, grad_out) = mse_loss(&out, &target);
        let mut gw_re = ndarray::Array3::<f32>::zeros(layer.weight_re.dim());
        let mut gw_im = ndarray::Array3::<f32>::zeros(layer.weight_im.dim());
        let g_in = layer.backward(&input, &grad_out, &mut gw_re, &mut gw_im);

        // Finite-difference along input[0, 0, 3].
        let eps = 1e-3_f32;
        input[[0, 0, 3]] += eps;
        let (l_plus, _) = mse_loss(&layer.forward(&input), &target);
        input[[0, 0, 3]] -= 2.0 * eps;
        let (l_minus, _) = mse_loss(&layer.forward(&input), &target);
        input[[0, 0, 3]] += eps;
        let num = (l_plus - l_minus) / (2.0 * eps);

        assert!(
            (g_in[[0, 0, 3]] - num).abs() < 1e-2,
            "spectral grad_input FD mismatch: analytic {} vs FD {}",
            g_in[[0, 0, 3]],
            num
        );
    }

    /// LinearMix grad_input via finite differences.
    #[test]
    fn linear_mix_grad_input_matches_finite_difference() {
        let layer = LinearMixLayer::new(3, 2, 0xBEEF);
        let length = 4;
        let mut input = Array3::from_shape_fn((1, 3, length), |(_, c, l)| (c + l) as f32 * 0.2);
        let target = Array3::<f32>::zeros((1, 2, length));

        let out = layer.forward(&input);
        let (_, grad_out) = mse_loss(&out, &target);
        let mut gw = Array2::<f32>::zeros(layer.weight.dim());
        let mut gb = Array1::<f32>::zeros(layer.bias.len());
        let g_in = layer.backward(&input, &grad_out, &mut gw, &mut gb);

        let eps = 1e-3_f32;
        input[[0, 1, 2]] += eps;
        let (l_plus, _) = mse_loss(&layer.forward(&input), &target);
        input[[0, 1, 2]] -= 2.0 * eps;
        let (l_minus, _) = mse_loss(&layer.forward(&input), &target);
        input[[0, 1, 2]] += eps;
        let num = (l_plus - l_minus) / (2.0 * eps);
        assert!(
            (g_in[[0, 1, 2]] - num).abs() < 1e-3,
            "linear_mix grad_input FD mismatch: analytic {} vs FD {}",
            g_in[[0, 1, 2]],
            num
        );
    }

    /// Full network: a small periodic regression — predict the input
    /// shifted by half a wavelength. Loss should drop substantially.
    #[test]
    fn fno_network_loss_decreases() {
        let cfg = FnoConfig {
            in_channels: 1,
            out_channels: 1,
            hidden_channels: 8,
            n_blocks: 2,
            modes: 4,
            rng_seed: 0xF11,
            substrate: Default::default(),
            thalamic: None,
        };
        let mut net = FnoNetwork::new(cfg).unwrap();
        let length = 16;
        let input = Array3::from_shape_fn((1, 1, length), |(_, _, l)| {
            (2.0 * std::f32::consts::PI * l as f32 / length as f32).sin()
        });
        let target = Array3::from_shape_fn((1, 1, length), |(_, _, l)| {
            (2.0 * std::f32::consts::PI * l as f32 / length as f32).cos()
        });

        let init_loss = mse_loss(&net.forward(&input), &target).0;

        let mut final_loss = f32::INFINITY;
        for _ in 0..400 {
            let (loss, _gn) = train_step_mse(&mut net, &input, &target, 0.05);
            final_loss = loss;
            if loss < 1e-3 {
                break;
            }
        }
        assert!(
            final_loss < 0.5 * init_loss,
            "fno did not learn: init={init_loss}, final={final_loss}"
        );
    }
}

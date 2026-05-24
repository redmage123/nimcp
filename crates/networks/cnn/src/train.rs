//! Phase 11a-train — CNN backward pass + SGD.
//!
//! # What's here
//!
//! - Per-layer `backward` implementations:
//!     - [`Conv2dLayer::backward`] — accumulates `grad_weight` /
//!       `grad_bias` and returns `grad_input`.
//!     - [`MaxPool2dLayer::backward`] — routes the upstream gradient
//!       to the argmax position of each pooling window.
//!     - [`LinearLayer::backward`] — straight matrix calculus.
//!     - [`ReluLayer::backward`] — gradient masked by `input > 0`.
//!     - [`FlattenLayer::backward`] — reshape only.
//! - [`CnnGradients`] — accumulated parameter gradients for every
//!   trainable layer in a [`CnnNetwork`].
//! - [`mse_loss`] — scalar MSE between output and target plus the
//!   gradient w.r.t. the output.
//! - [`sgd_step`] — applies `param ←- lr * grad / batch_size`.
//! - [`train_step_mse`] — one full forward → loss → backward → SGD
//!   step bundled together.
//!
//! # Scope (Phase 11a-train)
//!
//! - **MSE loss only.** Cross-entropy + softmax is a follow-up.
//! - **Vanilla SGD.** Adam / AdamW / momentum are out of scope; once
//!   the brain wires CNN training in, the LNN's `TrainParams` shape
//!   can be lifted into a shared `OptParams` later.
//! - **No CB/R-STDP analogues** — CNN doesn't have spike dynamics.
//! - **CPU-only.** GPU backward kernels land in 11a-gpu.

use ndarray::{Array1, Array2, Array3, Array4};

use crate::activation::ReluLayer;
use crate::conv::Conv2dLayer;
use crate::flatten::FlattenLayer;
use crate::linear::LinearLayer;
use crate::network::{CnnLayer, CnnNetwork};
use crate::pool::MaxPool2dLayer;

// ---------------------------------------------------------------------------
// Per-layer backward.
// ---------------------------------------------------------------------------

impl Conv2dLayer {
    /// Backward through a 2-D cross-correlation. Returns the gradient
    /// w.r.t. the layer's input. Accumulates the weight + bias
    /// gradients into the supplied buffers (caller zeros them between
    /// minibatches).
    pub fn backward(
        &self,
        input: &Array4<f32>,
        grad_out: &Array4<f32>,
        grad_weight: &mut Array4<f32>,
        grad_bias: &mut Array1<f32>,
    ) -> Array4<f32> {
        let (n, _c_in, h_in, w_in) = input.dim();
        let (_, _, h_out, w_out) = grad_out.dim();
        let mut grad_in = Array4::<f32>::zeros(input.dim());

        for ni in 0..n {
            for oc in 0..self.out_channels {
                for oh in 0..h_out {
                    for ow in 0..w_out {
                        let g = grad_out[[ni, oc, oh, ow]];
                        if g == 0.0 {
                            // Cheap skip — common for sparse gradients
                            // from dead ReLU positions, no math needed.
                            continue;
                        }
                        // Bias gradient: 1 per spatial position.
                        grad_bias[oc] += g;

                        let h0 = oh * self.stride_h;
                        let w0 = ow * self.stride_w;
                        for ic in 0..self.in_channels {
                            for di in 0..self.kh {
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
                                    // grad_w[oc, ic, di, dj] += g * input[ni, ic, ih_in, iw_in]
                                    grad_weight[[oc, ic, di, dj]] +=
                                        g * input[[ni, ic, ih_in, iw_in]];
                                    // grad_in[ni, ic, ih_in, iw_in] += g * w[oc, ic, di, dj]
                                    grad_in[[ni, ic, ih_in, iw_in]] +=
                                        g * self.weight[[oc, ic, di, dj]];
                                }
                            }
                        }
                    }
                }
            }
        }
        grad_in
    }
}

impl MaxPool2dLayer {
    /// Backward through max-pool — routes the upstream gradient back
    /// to the argmax position of each pooling window.
    pub fn backward(&self, input: &Array4<f32>, grad_out: &Array4<f32>) -> Array4<f32> {
        let (n, c, h_in, w_in) = input.dim();
        let (h_out, w_out) = self.output_hw(h_in, w_in);
        let mut grad_in = Array4::<f32>::zeros(input.dim());

        for ni in 0..n {
            for ci in 0..c {
                for oh in 0..h_out {
                    for ow in 0..w_out {
                        let h0 = oh * self.stride_h;
                        let w0 = ow * self.stride_w;
                        let mut best = f32::NEG_INFINITY;
                        let mut argmax = (0_usize, 0_usize);
                        for di in 0..self.kh {
                            for dj in 0..self.kw {
                                let v = input[[ni, ci, h0 + di, w0 + dj]];
                                if v > best {
                                    best = v;
                                    argmax = (di, dj);
                                }
                            }
                        }
                        grad_in[[ni, ci, h0 + argmax.0, w0 + argmax.1]] +=
                            grad_out[[ni, ci, oh, ow]];
                    }
                }
            }
        }
        grad_in
    }
}

impl ReluLayer {
    /// Backward through ReLU — passes the upstream gradient through
    /// only at positions where the input was positive.
    pub fn backward(&self, input: &Array4<f32>, grad_out: &Array4<f32>) -> Array4<f32> {
        let mut g = grad_out.clone();
        for (gi, ii) in g.iter_mut().zip(input.iter()) {
            if *ii <= 0.0 {
                *gi = 0.0;
            }
        }
        g
    }

    /// 2-D variant for the post-flatten path.
    pub fn backward_2d(&self, input: &Array2<f32>, grad_out: &Array2<f32>) -> Array2<f32> {
        let mut g = grad_out.clone();
        for (gi, ii) in g.iter_mut().zip(input.iter()) {
            if *ii <= 0.0 {
                *gi = 0.0;
            }
        }
        g
    }
}

impl FlattenLayer {
    /// Backward through flatten — reshape only.
    pub fn backward(&self, original_shape: (usize, usize, usize, usize), grad_out: &Array2<f32>) -> Array4<f32> {
        let raw: Vec<f32> = grad_out.iter().copied().collect();
        Array4::from_shape_vec(original_shape, raw)
            .expect("flatten backward: element count must match original shape")
    }
}

impl LinearLayer {
    /// Backward through a dense layer.
    /// - `grad_weight`: `[out, in]`, accumulates `grad_out^T · input`.
    /// - `grad_bias`: `[out]`, accumulates row-summed `grad_out`.
    /// Returns `grad_in = grad_out · W` (`[batch, in]`).
    pub fn backward(
        &self,
        input: &Array2<f32>,
        grad_out: &Array2<f32>,
        grad_weight: &mut Array2<f32>,
        grad_bias: &mut Array1<f32>,
    ) -> Array2<f32> {
        let (n, _) = input.dim();
        // grad_weight += grad_out^T @ input  => [out, in]
        let gw = grad_out.t().dot(input);
        for (g, dg) in grad_weight.iter_mut().zip(gw.iter()) {
            *g += *dg;
        }
        // grad_bias += sum_rows(grad_out) => [out]
        for (oc, b) in grad_bias.iter_mut().enumerate() {
            for ni in 0..n {
                *b += grad_out[[ni, oc]];
            }
        }
        // grad_in = grad_out @ W => [batch, in]
        grad_out.dot(&self.weight)
    }
}

// ---------------------------------------------------------------------------
// MSE loss + gradient.
// ---------------------------------------------------------------------------

/// Returns `(loss, grad_output)` for batched MSE between `output` and
/// `target`, both `[batch, out]`. Loss is averaged over batch and
/// outputs; gradient is `2 * (out - tgt) / (batch * out)`.
pub fn mse_loss(output: &Array2<f32>, target: &Array2<f32>) -> (f32, Array2<f32>) {
    assert_eq!(output.dim(), target.dim(), "mse_loss shape mismatch");
    let (n, k) = output.dim();
    let denom = (n * k).max(1) as f32;
    let mut grad = Array2::<f32>::zeros((n, k));
    let mut loss = 0.0_f32;
    for ((og, tg), gg) in output.iter().zip(target.iter()).zip(grad.iter_mut()) {
        let d = *og - *tg;
        loss += d * d;
        *gg = 2.0 * d / denom;
    }
    loss /= denom;
    (loss, grad)
}

// ---------------------------------------------------------------------------
// Network-level gradient bag + train step.
// ---------------------------------------------------------------------------

/// Per-layer gradient buffers, indexed in lock-step with
/// `network.layers`. Non-trainable layers (Pool / Relu / Flatten) hold
/// `None` to keep the index aligned.
#[derive(Debug, Clone)]
pub struct CnnGradients {
    pub conv_weight: Vec<Option<Array4<f32>>>,
    pub conv_bias: Vec<Option<Array1<f32>>>,
    pub linear_weight: Vec<Option<Array2<f32>>>,
    pub linear_bias: Vec<Option<Array1<f32>>>,
}

impl CnnGradients {
    pub fn zeros_for(net: &CnnNetwork) -> Self {
        let mut conv_weight: Vec<Option<Array4<f32>>> = Vec::with_capacity(net.layers.len());
        let mut conv_bias: Vec<Option<Array1<f32>>> = Vec::with_capacity(net.layers.len());
        let mut linear_weight: Vec<Option<Array2<f32>>> = Vec::with_capacity(net.layers.len());
        let mut linear_bias: Vec<Option<Array1<f32>>> = Vec::with_capacity(net.layers.len());

        for layer in &net.layers {
            match layer {
                CnnLayer::Conv(c) => {
                    conv_weight.push(Some(Array4::<f32>::zeros(c.weight.dim())));
                    conv_bias.push(Some(Array1::<f32>::zeros(c.bias.len())));
                    linear_weight.push(None);
                    linear_bias.push(None);
                }
                CnnLayer::Linear(l) => {
                    conv_weight.push(None);
                    conv_bias.push(None);
                    linear_weight.push(Some(Array2::<f32>::zeros(l.weight.dim())));
                    linear_bias.push(Some(Array1::<f32>::zeros(l.bias.len())));
                }
                _ => {
                    conv_weight.push(None);
                    conv_bias.push(None);
                    linear_weight.push(None);
                    linear_bias.push(None);
                }
            }
        }
        Self {
            conv_weight,
            conv_bias,
            linear_weight,
            linear_bias,
        }
    }
}

/// Cached forward activations for backward — at most one of `spatial`
/// / `flat` is `Some` per index, mirroring the forward pass.
#[derive(Debug)]
struct ForwardCache {
    spatial: Vec<Option<Array4<f32>>>,
    flat: Vec<Option<Array2<f32>>>,
}

fn forward_with_cache(net: &CnnNetwork, input: &Array4<f32>) -> (Array2<f32>, ForwardCache) {
    let mut spatial: Vec<Option<Array4<f32>>> = Vec::with_capacity(net.layers.len() + 1);
    let mut flat: Vec<Option<Array2<f32>>> = Vec::with_capacity(net.layers.len() + 1);
    spatial.push(Some(input.clone()));
    flat.push(None);

    let mut cur_spatial: Option<Array4<f32>> = Some(input.clone());
    let mut cur_flat: Option<Array2<f32>> = None;
    for layer in &net.layers {
        match layer {
            CnnLayer::Conv(c) => {
                let cur = cur_spatial.take().expect("conv after flatten");
                cur_spatial = Some(c.forward(&cur));
            }
            CnnLayer::Pool(p) => {
                let cur = cur_spatial.take().expect("pool after flatten");
                cur_spatial = Some(p.forward(&cur));
            }
            CnnLayer::Relu(r) => {
                if let Some(cur) = cur_spatial.take() {
                    cur_spatial = Some(r.forward(&cur));
                } else if let Some(cur) = cur_flat.take() {
                    cur_flat = Some(cur.mapv(|x| if x > 0.0 { x } else { 0.0 }));
                }
            }
            CnnLayer::Flatten(f) => {
                let cur = cur_spatial.take().expect("flatten without spatial");
                cur_flat = Some(f.forward(&cur));
            }
            CnnLayer::Linear(l) => {
                let cur = cur_flat.take().expect("linear without flatten");
                cur_flat = Some(l.forward(&cur));
            }
        }
        spatial.push(cur_spatial.clone());
        flat.push(cur_flat.clone());
    }

    let out = cur_flat.unwrap_or_else(|| {
        // Flatten implicitly if the network ended on a spatial layer.
        let s = cur_spatial.expect("forward produced no output");
        FlattenLayer::new().forward(&s)
    });
    (out, ForwardCache { spatial, flat })
}

/// One forward + backward + SGD step against an MSE target.
/// Returns `(loss, grad_norm)`.
pub fn train_step_mse(
    net: &mut CnnNetwork,
    input: &Array4<f32>,
    target: &Array2<f32>,
    lr: f32,
) -> (f32, f32) {
    let (loss, grads, grad_norm) = backward_grads(net, input, target);
    // SGD step — `param -= lr * grad`. The gradient already absorbed
    // the batch-mean denominator inside `mse_loss`.
    sgd_step(net, &grads, lr);
    (loss, grad_norm)
}

/// Phase 11-substrate — substrate-aware train step. Identical to
/// [`train_step_mse`] when `substrate_cfg` is disabled. When enabled:
///
/// - `lr_eff = lr × dend.plasticity_mod` (cadence-refreshed effects).
/// - Asymmetric LTP/LTD gating: positive gradient components scaled by
///   `ltp_capacity`, negative by `ltd_capacity`.
/// - Substrate ATP/ion/membrane debited by `n_plasticity = 1` after the
///   step (a learning event carries a metabolic cost).
///
/// The forward used to compute gradients here is the *un-modulated*
/// forward — substrate output-gain is an inference-path effect, not a
/// training-target distortion. Modulation enters training only through
/// the LR + LTP/LTD gates, matching the LNN adapter.
pub fn train_step_mse_modulated(
    net: &mut CnnNetwork,
    input: &Array4<f32>,
    target: &Array2<f32>,
    lr: f32,
) -> (f32, f32) {
    if !net.substrate_cfg.enabled {
        return train_step_mse(net, input, target, lr);
    }
    // Refresh cached effects on the configured cadence.
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

    // Debit a single plasticity event.
    if let Some(ref mut s) = net.substrate_state {
        nimcp_substrate::debit_activity(s, &net.substrate_cfg.dynamics, 0, 1);
    }

    (loss, grad_norm)
}

/// Forward + backward against an MSE target. Returns
/// `(loss, gradients, grad_norm)` without applying any update — shared
/// by [`train_step_mse`] and [`train_step_mse_modulated`].
fn backward_grads(
    net: &CnnNetwork,
    input: &Array4<f32>,
    target: &Array2<f32>,
) -> (f32, CnnGradients, f32) {
    let (output, cache) = forward_with_cache(net, input);
    let (loss, grad_out) = mse_loss(&output, target);

    let mut grads = CnnGradients::zeros_for(net);

    // Backward pass — walk layers in reverse, threading the upstream
    // gradient through. Like the forward, we carry both spatial and
    // flat representations and switch at Flatten.
    let mut g_spatial: Option<Array4<f32>> = None;
    let mut g_flat: Option<Array2<f32>> = Some(grad_out);

    for (idx, layer) in net.layers.iter().enumerate().rev() {
        let in_idx = idx; // the input to layer `idx` is cache[idx]; output is cache[idx+1].
        match layer {
            CnnLayer::Linear(l) => {
                let input_2d = cache.flat[in_idx]
                    .as_ref()
                    .expect("linear input must be flat");
                let grad = g_flat.take().expect("linear backward without flat grad");
                let gw = grads.linear_weight[idx].as_mut().expect("alloc'd above");
                let gb = grads.linear_bias[idx].as_mut().expect("alloc'd above");
                let g_in = l.backward(input_2d, &grad, gw, gb);
                g_flat = Some(g_in);
            }
            CnnLayer::Flatten(f) => {
                let input_4d = cache.spatial[in_idx]
                    .as_ref()
                    .expect("flatten input must be spatial");
                let grad = g_flat.take().expect("flatten backward without flat grad");
                let g_in = f.backward(input_4d.dim(), &grad);
                g_spatial = Some(g_in);
            }
            CnnLayer::Relu(r) => {
                if let Some(grad) = g_spatial.take() {
                    let input_4d = cache.spatial[in_idx]
                        .as_ref()
                        .expect("relu spatial input");
                    g_spatial = Some(r.backward(input_4d, &grad));
                } else if let Some(grad) = g_flat.take() {
                    let input_2d = cache.flat[in_idx].as_ref().expect("relu flat input");
                    g_flat = Some(r.backward_2d(input_2d, &grad));
                }
            }
            CnnLayer::Pool(p) => {
                let input_4d = cache.spatial[in_idx]
                    .as_ref()
                    .expect("pool spatial input");
                let grad = g_spatial.take().expect("pool backward without spatial grad");
                g_spatial = Some(p.backward(input_4d, &grad));
            }
            CnnLayer::Conv(c) => {
                let input_4d = cache.spatial[in_idx]
                    .as_ref()
                    .expect("conv spatial input");
                let grad = g_spatial.take().expect("conv backward without spatial grad");
                let gw = grads.conv_weight[idx].as_mut().expect("alloc'd above");
                let gb = grads.conv_bias[idx].as_mut().expect("alloc'd above");
                let g_in = c.backward(input_4d, &grad, gw, gb);
                g_spatial = Some(g_in);
            }
        }
    }

    // Compute gradient norm (sum of squares across every accumulated buffer).
    let mut sq_sum = 0.0_f32;
    for buf in grads.conv_weight.iter().flatten() {
        for v in buf.iter() {
            sq_sum += v * v;
        }
    }
    for buf in grads.conv_bias.iter().flatten() {
        for v in buf.iter() {
            sq_sum += v * v;
        }
    }
    for buf in grads.linear_weight.iter().flatten() {
        for v in buf.iter() {
            sq_sum += v * v;
        }
    }
    for buf in grads.linear_bias.iter().flatten() {
        for v in buf.iter() {
            sq_sum += v * v;
        }
    }
    let grad_norm = sq_sum.sqrt();

    (loss, grads, grad_norm)
}

/// Apply `param -= lr * grad` to every trainable layer.
pub fn sgd_step(net: &mut CnnNetwork, grads: &CnnGradients, lr: f32) {
    for (idx, layer) in net.layers.iter_mut().enumerate() {
        match layer {
            CnnLayer::Conv(c) => {
                if let (Some(gw), Some(gb)) =
                    (grads.conv_weight[idx].as_ref(), grads.conv_bias[idx].as_ref())
                {
                    for (w, g) in c.weight.iter_mut().zip(gw.iter()) {
                        *w -= lr * *g;
                    }
                    for (b, g) in c.bias.iter_mut().zip(gb.iter()) {
                        *b -= lr * *g;
                    }
                }
            }
            CnnLayer::Linear(l) => {
                if let (Some(gw), Some(gb)) = (
                    grads.linear_weight[idx].as_ref(),
                    grads.linear_bias[idx].as_ref(),
                ) {
                    for (w, g) in l.weight.iter_mut().zip(gw.iter()) {
                        *w -= lr * *g;
                    }
                    for (b, g) in l.bias.iter_mut().zip(gb.iter()) {
                        *b -= lr * *g;
                    }
                }
            }
            _ => {}
        }
    }
}

/// Like [`sgd_step`] but applies asymmetric LTP/LTD gating: each
/// gradient component is scaled by `ltp` when it would *decrease* the
/// weight (potentiation under `param -= lr*grad` means `grad < 0`) and
/// by `ltd` when it would *increase* it. `(ltp, ltd) = (1.0, 1.0)` makes
/// this bit-identical to [`sgd_step`].
pub fn sgd_step_gated(net: &mut CnnNetwork, grads: &CnnGradients, lr: f32, ltp: f32, ltd: f32) {
    let gate = |g: f32| -> f32 {
        // `param -= lr*g`: g < 0 raises the weight (LTP), g > 0 lowers it (LTD).
        if g < 0.0 { g * ltp } else { g * ltd }
    };
    for (idx, layer) in net.layers.iter_mut().enumerate() {
        match layer {
            CnnLayer::Conv(c) => {
                if let (Some(gw), Some(gb)) =
                    (grads.conv_weight[idx].as_ref(), grads.conv_bias[idx].as_ref())
                {
                    for (w, g) in c.weight.iter_mut().zip(gw.iter()) {
                        *w -= lr * gate(*g);
                    }
                    for (b, g) in c.bias.iter_mut().zip(gb.iter()) {
                        *b -= lr * gate(*g);
                    }
                }
            }
            CnnLayer::Linear(l) => {
                if let (Some(gw), Some(gb)) = (
                    grads.linear_weight[idx].as_ref(),
                    grads.linear_bias[idx].as_ref(),
                ) {
                    for (w, g) in l.weight.iter_mut().zip(gw.iter()) {
                        *w -= lr * gate(*g);
                    }
                    for (b, g) in l.bias.iter_mut().zip(gb.iter()) {
                        *b -= lr * gate(*g);
                    }
                }
            }
            _ => {}
        }
    }
}

// Suppress unused-import warning when tests only build the lib.
#[allow(dead_code)]
fn _force_array3_use(_: &Array3<f32>) {}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::network::{CnnConfig, CnnLayerSpec};
    use ndarray::{Array2, Array4};

    /// A tiny linear-only "CNN" — just Flatten + Linear — should fit
    /// a 2-input → 1-output identity in a few hundred SGD steps.
    /// This validates the linear backward + SGD path in isolation.
    #[test]
    fn linear_only_cnn_fits_simple_regression() {
        let cfg = CnnConfig {
            input_shape: (1, 1, 2),
            layers: vec![
                CnnLayerSpec::Flatten,
                CnnLayerSpec::Linear { out_features: 1 },
            ],
            rng_seed: 0xCFE,
            substrate: Default::default(),
            thalamic: None,
        };
        let mut net = CnnNetwork::new(cfg).unwrap();

        // Target: y = sum(x).
        let xs = vec![
            (vec![0.5_f32, 0.3], 0.8_f32),
            (vec![-0.2, 0.9], 0.7),
            (vec![0.1, -0.4], -0.3),
            (vec![-0.6, -0.1], -0.7),
        ];
        let mut last_loss = f32::INFINITY;
        for _ in 0..2000 {
            let mut total = 0.0;
            for (x, y) in &xs {
                let inp = Array4::from_shape_vec((1, 1, 1, 2), x.clone()).unwrap();
                let tgt = Array2::from_shape_vec((1, 1), vec![*y]).unwrap();
                let (loss, _gn) = train_step_mse(&mut net, &inp, &tgt, 0.05);
                total += loss;
            }
            last_loss = total / xs.len() as f32;
            if last_loss < 1e-3 {
                break;
            }
        }
        assert!(
            last_loss < 1e-2,
            "linear-only CNN did not fit regression: loss={last_loss}"
        );
    }

    /// Conv → ReLU → Flatten → Linear. Smoke-test convergence on a
    /// synthetic image classification: 3×3 patterns where the centre
    /// pixel sign decides the label. With ~500 SGD steps loss should
    /// drop noticeably below the random-init baseline.
    #[test]
    fn conv_cnn_loss_decreases() {
        let cfg = CnnConfig {
            input_shape: (1, 3, 3),
            layers: vec![
                CnnLayerSpec::Conv2d {
                    out_channels: 2,
                    kh: 3,
                    kw: 3,
                    stride: 1,
                    padding: 1,
                },
                CnnLayerSpec::Relu,
                CnnLayerSpec::Flatten,
                CnnLayerSpec::Linear { out_features: 1 },
            ],
            rng_seed: 0xC0,
            substrate: Default::default(),
            thalamic: None,
        };
        let mut net = CnnNetwork::new(cfg).unwrap();

        // Two patterns: centre +1 → label 1.0; centre -1 → label -1.0.
        // Surround pixels are random-fixed so the model has to rely on
        // the centre signal.
        let p_pos = vec![0.1, -0.2, 0.0, 0.3, 1.0, -0.1, 0.0, 0.2, -0.3];
        let p_neg = vec![0.1, -0.2, 0.0, 0.3, -1.0, -0.1, 0.0, 0.2, -0.3];
        let samples: Vec<(Array4<f32>, Array2<f32>)> = vec![
            (
                Array4::from_shape_vec((1, 1, 3, 3), p_pos.clone()).unwrap(),
                Array2::from_shape_vec((1, 1), vec![1.0]).unwrap(),
            ),
            (
                Array4::from_shape_vec((1, 1, 3, 3), p_neg.clone()).unwrap(),
                Array2::from_shape_vec((1, 1), vec![-1.0]).unwrap(),
            ),
        ];

        // Initial loss baseline.
        let mut init_loss = 0.0_f32;
        for (x, y) in &samples {
            let (out, _) = forward_with_cache(&net, x);
            let (l, _) = mse_loss(&out, y);
            init_loss += l;
        }
        init_loss /= samples.len() as f32;

        let mut final_loss = f32::INFINITY;
        for _ in 0..500 {
            let mut total = 0.0_f32;
            for (x, y) in &samples {
                let (loss, _) = train_step_mse(&mut net, x, y, 0.1);
                total += loss;
            }
            final_loss = total / samples.len() as f32;
            if final_loss < 1e-3 {
                break;
            }
        }
        assert!(
            final_loss < 0.5 * init_loss,
            "conv CNN loss did not decrease: init={init_loss}, final={final_loss}"
        );
    }

    /// Numerical-gradient sanity check on Conv2dLayer::backward —
    /// compare a single weight gradient component to centred finite
    /// differences. Ensures the analytic formula for grad_weight and
    /// the bias path is correct.
    #[test]
    fn conv_backward_matches_finite_difference() {
        let mut layer = Conv2dLayer::new(1, 1, 3, 3, 1, 1, 0, 0, 0xABCD);
        let input = Array4::from_shape_fn((1, 1, 5, 5), |(_, _, h, w)| (h + w) as f32 * 0.1);

        // Analytic gradient using a unit upstream gradient at one position.
        let mut grad_out = Array4::<f32>::zeros((1, 1, 3, 3));
        grad_out[[0, 0, 1, 1]] = 1.0;
        let mut gw = Array4::<f32>::zeros(layer.weight.dim());
        let mut gb = Array1::<f32>::zeros(layer.bias.len());
        let _ = layer.backward(&input, &grad_out, &mut gw, &mut gb);

        // Pick one weight position to perturb.
        let (oc, ic, di, dj) = (0_usize, 0_usize, 1_usize, 1_usize);
        let eps = 1e-3_f32;
        layer.weight[[oc, ic, di, dj]] += eps;
        let out_plus = layer.forward(&input);
        layer.weight[[oc, ic, di, dj]] -= 2.0 * eps;
        let out_minus = layer.forward(&input);
        layer.weight[[oc, ic, di, dj]] += eps;

        let scalar_plus = out_plus[[0, 0, 1, 1]];
        let scalar_minus = out_minus[[0, 0, 1, 1]];
        let num = (scalar_plus - scalar_minus) / (2.0 * eps);
        assert!(
            (gw[[oc, ic, di, dj]] - num).abs() < 1e-2,
            "conv grad_weight FD mismatch: analytic {} vs FD {}",
            gw[[oc, ic, di, dj]],
            num
        );
        assert!(gb[0] - 1.0 < 1e-6, "conv grad_bias should be 1 for a unit upstream");
    }

    #[test]
    fn maxpool_backward_routes_to_argmax() {
        let pool = MaxPool2dLayer::square(2);
        let input = Array4::from_shape_vec(
            (1, 1, 2, 2),
            vec![1.0_f32, 4.0, 3.0, 2.0],
        ).unwrap();
        let grad_out = Array4::from_shape_vec((1, 1, 1, 1), vec![5.0_f32]).unwrap();
        let g_in = pool.backward(&input, &grad_out);
        // argmax was at [0,0,0,1] (value 4.0); all gradient should land there.
        assert_eq!(g_in[[0, 0, 0, 1]], 5.0);
        assert_eq!(g_in[[0, 0, 0, 0]], 0.0);
        assert_eq!(g_in[[0, 0, 1, 0]], 0.0);
        assert_eq!(g_in[[0, 0, 1, 1]], 0.0);
    }
}

//! NIMCP V2 — convolutional neural network (Phase 11a).
//!
//! V1 has a CNN; V2 had none. This crate ships a fresh implementation
//! shaped to V2's `Network` trait surface (deterministic seed, serde
//! checkpoint, `forward` over an `ndarray` tensor) so the eventual GPU
//! port and `nimcp-brain` adapter drop in cleanly.
//!
//! # Scope (Phase 11a — CPU forward only)
//!
//! - Layers: [`Conv2dLayer`], [`MaxPool2dLayer`], [`LinearLayer`],
//!   [`ReluLayer`], [`FlattenLayer`].
//! - Storage: 4-D activations `[batch, channels, height, width]`,
//!   conv weights `[out_c, in_c, kh, kw]`, biases `[out_c]`.
//! - Forward: direct cross-correlation (no FFT) with stride + zero
//!   padding. Inner row × kernel multiply auto-vectorises under LLVM
//!   for the typical small (3×3, 5×5) kernel sizes.
//! - Init: Xavier-uniform from a [`rand_chacha::ChaCha20Rng`] seed,
//!   matching the LNN convention so deterministic tests work.
//!
//! # Out of scope here
//!
//! - GPU forward kernels (Phase 11a-gpu follow-up).
//! - Backward pass / training loop (Phase 11a-train follow-up).
//! - Substrate / thalamic adapters (the bundled 11-substrate phase).

#![forbid(unsafe_code)]
// Phase 11a — fields + small helpers are self-documenting via the
// crate-level explanation; per-field doc comments would be busywork.
#![allow(missing_docs)]

pub mod activation;
pub mod conv;
pub mod flatten;
pub mod linear;
pub mod network;
pub mod pool;
pub mod substrate_adapter;
pub mod train;

pub use activation::ReluLayer;
pub use conv::Conv2dLayer;
pub use flatten::FlattenLayer;
pub use linear::LinearLayer;
pub use network::{
    CnnConfig, CnnError, CnnLayerSpec, CnnNetwork, CnnSubstrateCfg, CnnThalamicCfg,
};
pub use pool::MaxPool2dLayer;
pub use train::{
    CnnGradients, mse_loss, sgd_step, sgd_step_gated, train_step_mse, train_step_mse_modulated,
};

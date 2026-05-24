//! NIMCP V2 — Fourier Neural Operator (Phase 11b).
//!
//! V2-shaped reimplementation of FNO. V1 had its own FNO module; this
//! crate ports the same operator into a clean Rust API on top of
//! `rustfft` (host-side FFT) + [`ndarray`].
//!
//! # Scope (Phase 11b — CPU forward only)
//!
//! - 1-D spectral convolution ([`SpectralConv1dLayer`]).
//! - Per-position linear mix branch ([`LinearMixLayer`]).
//! - Block fusing spectral + linear-mix + `tanh` ([`FnoBlock`]).
//! - Stack of blocks ([`FnoNetwork`]) with input/output channel
//!   projections.
//!
//! # Data layout
//!
//! All activations are 3-D `[batch, channels, length]` real tensors.
//! Per-block scratch is 3-D `[batch, channels, length]` complex (the
//! FFT bin count equals the spatial length — no downsampling here).
//!
//! Spectral weights are `[modes, in_channels, out_channels]` complex
//! — only the first `modes` low-frequency bins are mixed; the rest are
//! zeroed out before the inverse transform. This is FNO's
//! resolution-independence trick: a fixed `modes` truncation works on
//! any `length >= 2 * modes`.
//!
//! # Forward (per block)
//!
//! ```text
//! Y_spec = IFFT( W_complex · FFT(x), high modes := 0 )       // real part only
//! Y_mix  = Linear( x )                                        // per-position 1×1
//! out    = tanh( Y_spec + Y_mix )
//! ```
//!
//! # Init
//!
//! Both real + imaginary parts of the complex spectral weights draw
//! from a scaled `Uniform(-bound, bound)` with
//! `bound = 1 / (in_channels × modes)` — keeps the post-IFFT activation
//! in roughly the same scale as the input on a unit-variance signal.
//! Linear-mix weights use Xavier-uniform.

#![forbid(unsafe_code)]
#![allow(missing_docs)]

pub mod block;
pub mod linear_mix;
pub mod network;
pub mod spectral;
pub mod substrate_adapter;
pub mod train;

pub use block::FnoBlock;
pub use linear_mix::LinearMixLayer;
pub use network::{
    FnoConfig, FnoError, FnoNetwork, FnoSubstrateCfg, FnoThalamicCfg,
};
pub use spectral::SpectralConv1dLayer;
pub use train::{
    FnoBlockGrads, FnoGradients, mse_loss, sgd_step, sgd_step_gated, train_step_mse,
    train_step_mse_modulated,
};

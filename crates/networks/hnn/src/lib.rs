//! NIMCP V2 — Hamiltonian Neural Network (Phase 11c).
//!
//! Implements an MLP that learns a scalar Hamiltonian `H(q, p)`, paired
//! with a symplectic Euler integrator that uses `dH/dq` and `dH/dp`
//! (computed by hand-rolled reverse-mode autodiff over the MLP) to
//! evolve the canonical coordinates forward in time.
//!
//! # Why symplectic
//!
//! Symplectic Euler exactly preserves a *modified* Hamiltonian
//! `H_h = H + O(dt)` per step, which gives bounded (oscillating, not
//! drifting) energy error over long integrations — the defining
//! property of an HNN-shaped solver. A naive forward Euler step on the
//! same equations bleeds energy linearly in `t`.
//!
//! # Module layout
//!
//! - [`mlp`] — [`HamiltonianMlp`]: the scalar MLP `(q, p) → H` with
//!   forward + reverse-mode gradients to `q` and `p`.
//! - [`integrator`] — [`symplectic_euler_step`]: integrator generic
//!   over any closure returning `(H, dH/dq, dH/dp)`. Used both for the
//!   MLP and for analytic Hamiltonians in tests.
//! - [`network`] — [`HnnNetwork`]: top-level wrapper that owns the MLP,
//!   the (q, p) state, the timestep, and a `step()` method.

#![forbid(unsafe_code)]
#![allow(missing_docs)]

pub mod integrator;
pub mod mlp;
pub mod network;
pub mod substrate_adapter;

pub use integrator::symplectic_euler_step;
pub use mlp::{HamiltonianMlp, MlpActivation};
pub use network::{
    HnnConfig, HnnError, HnnNetwork, HnnSubstrateCfg, HnnThalamicCfg,
};

//! NIMCP V2 — geometry utilities.
//!
//! Port of V1's `utils/geometry/` — Poincaré disk (hyperbolic 2D) and
//! Lorentz-boost helpers. Used for embedding hierarchies in the
//! memory / concept modules (hyperbolic space can represent tree-like
//! structure with lower distortion than Euclidean).

#![forbid(unsafe_code)]

pub mod lorentz;
pub mod poincare;

pub use lorentz::{lorentz_boost, lorentz_factor};
pub use poincare::{
    POINCARE_CLIP_EPS, PoincarePoint, poincare_clip, poincare_conformal_factor, poincare_distance,
    poincare_norm,
};

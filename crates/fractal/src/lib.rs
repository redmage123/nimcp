//! NIMCP V2 — fractal utilities.
//!
//! Port of the subset of V1's fractal helpers that are network-agnostic
//! and actually used:
//!
//! - [`hilbert`] — 2D Hilbert-curve index ↔ `(x, y)` round trip.
//!   V1 uses this for memory addressing that preserves spatial
//!   locality. Standard bit-interleave algorithm.
//! - [`box_counting`] — fractal-dimension estimator. Occupancy-count
//!   at decreasing scales, fit on log-log axes → slope is `-D`.
//! - [`dfa`] — Detrended Fluctuation Analysis. Produces a scaling
//!   exponent `α`. For ideal 1/f (pink) noise `α ≈ 1.0`; α ≈ 0.5
//!   is white noise, α ≈ 1.5 is Brownian. V1's `pink_monitor` uses
//!   this to verify the 1/f output is actually 1/f.
//!
//! All three are pure functions — no state, no allocation beyond
//! documented outputs.

#![forbid(unsafe_code)]

pub mod box_counting;
pub mod dfa;
pub mod hilbert;

pub use box_counting::box_counting_dimension;
pub use dfa::{DfaError, dfa_alpha};
pub use hilbert::{hilbert_index_to_xy, hilbert_xy_to_index};

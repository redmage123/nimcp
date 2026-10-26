//! NIMCP V2 — quantum-inspired algorithms.
//!
//! Port of the subset of V1's `utils/quantum/` that is pure algorithm
//! (no GPU, no full physics). V2 keeps the ideas that proved useful
//! in V1's cognitive modules:
//!
//! - [`walk`] — discrete-time quantum walk on a finite graph.
//!   Amplitude vector over nodes + a coin register; produces a
//!   distribution with richer transient structure than a classical
//!   random walk. V1 uses this for exploration heuristics.
//! - [`qmc`] — quantum Monte Carlo, here reduced to a seeded
//!   Metropolis sampler that draws from an arbitrary energy
//!   function. V1's full QMC is much larger; V2 takes the core
//!   sampler only.
//!
//! All math is real-valued f32 in the minimum-viable port —
//! amplitudes are kept as `(re, im)` pairs explicitly rather than
//! pulling in a complex-number crate.

#![forbid(unsafe_code)]

pub mod qmc;
pub mod walk;

pub use qmc::{MetropolisError, MetropolisState, metropolis_step};
pub use walk::{Amp, QuantumWalker, WalkError, normalize_amplitudes, probabilities};

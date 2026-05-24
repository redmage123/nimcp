//! NIMCP V2 — cognitive subsystems.
//!
//! Functional-core ports of V1 cognitive modules that the language
//! cascade's content stage blends into its intent (V1 Tier-1 Steps D/E):
//!
//! - [`working_memory`] — a capacity-bounded, salience-decayed store of
//!   feature vectors (faithful port of V1's standalone `working_memory`
//!   core; V1's own header advertises "DEPENDENCIES: None").
//! - [`imagination`] — a scenario workspace holding an active
//!   imagined-content vector + vividness, evolvable by noise / goal-blend
//!   (the self-contained core of V1's imagination engine; the
//!   VAE/hippocampus scenario *generators* are out of scope).
//!
//! Reasoning is **not** here — its V2 functional core operates on the
//! grounded-language lexicon, so it lives in `nimcp-language`.

#![forbid(unsafe_code)]
#![allow(missing_docs)]

pub mod imagination;
pub mod working_memory;

pub use imagination::{ImaginationEngine, ScenarioMode};
pub use working_memory::WorkingMemory;

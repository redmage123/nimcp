//! NIMCP V2 — content-safety toxicity stack.
//!
//! A fresh Rust implementation of V1's near-standalone toxicity gate
//! (`src/security/nimcp_toxicity*.c`): a regex **pattern classifier**
//! (allowlist span-collect + toxic span-suppress + max-merge scoring), a
//! char-trigram-hashed **MLP** head, and a stage-graded **counterclaim
//! generator**. See `docs/V2_LANGUAGE_PLAN.md` (phases T1–T3).
//!
//! # Invariants carried from V1
//!
//! - **mark-not-filter**: toxic *training* data is logged / down-weighted,
//!   never silently dropped; toxic *output* is blocked with a counterclaim
//!   at a non-bypassable gate.
//! - **`would_block` is a hint**, not a delete order — scoring and policy
//!   are separated.
//! - **Allowlist span-suppress, not short-circuit**: an allowlist match
//!   suppresses overlapping toxic matches but does NOT clear the whole
//!   result (V1 2026-05-20 fix — `"X aren't subhuman. Kill all Y."` must
//!   still flag the second clause).

#![forbid(unsafe_code)]
#![allow(missing_docs)]

pub mod counterclaim;
pub mod ml;
pub mod pattern;
pub mod stack;

pub use counterclaim::{CounterclaimEngine, CounterclaimResult};
pub use ml::{MlClassifier, MlResult};
pub use pattern::{PatternClassifier, PatternRule, ToxicityResult};
pub use stack::ToxicityStack;

/// Default block threshold (V1 `0.7`): `max_score ≥ threshold` sets
/// `would_block`.
pub const DEFAULT_THRESHOLD: f32 = 0.7;

/// Affective-tag threshold (V1 `0.85`): only above this is training data
/// valence-tagged as toxic (mark-not-filter).
pub const TAG_THRESHOLD: f32 = 0.85;

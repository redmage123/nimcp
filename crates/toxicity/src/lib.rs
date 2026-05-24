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
//!
//! Phases T1–T3 populate the modules below. This is the L0 scaffold.

#![forbid(unsafe_code)]
#![allow(missing_docs)]

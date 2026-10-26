//! NIMCP V2 — thalamic router.
//!
//! Port of V1's `thalamic_router_t` / `thalamic_channel_t` reduced to
//! the minimum surface needed by the multi-network integration plan
//! (see `docs/design/substrate_thalamic_integration.md`).
//!
//! # Model
//!
//! Each *source* (typically a network, or a sub-region of one) holds
//! a [`ThalamicChannel`] — a compact bundle of up to
//! [`THALAMIC_MAX_DESTINATIONS`] (16) destinations plus their
//! attention weights in `[0, 1]` and a [`RelayMode`]. The
//! [`ThalamicRouter`] maintains per-`(source, destination)` Hebbian
//! route weights that reinforce when the source submits signal AND
//! the destination is later active.
//!
//! The network adapter contract:
//! 1. Each step: read `get_gate(dest_id)` per downstream destination →
//!    scales how strongly the source's output propagates.
//! 2. Submit output payloads via `submit(dest_id, payload, priority)`.
//! 3. Call `tick()` once per step → bumps Hebbian weights based on
//!    submit history.
//!
//! # Design notes
//!
//! - Fixed upper bound on destinations (16) matches V1 and keeps the
//!   channel `Copy`-cheap to pass around.
//! - Burst mode skips the priority queue — used when the source's
//!   output is "urgent" (high activation, salient signal).
//! - Attention weights and Hebbian routes are separate: attention is
//!   the *current* gate (fast), Hebbian is the *learned* route (slow,
//!   bumped each `tick`).
//! - Full-attention defaults: new channels have `attention_weights` of
//!   `1.0` for every destination — zero modulation at birth.

#![forbid(unsafe_code)]

pub mod channel;
pub mod router;

pub use channel::{RelayMode, THALAMIC_MAX_DESTINATIONS, ThalamicChannel, ThalamicSubmit};
pub use router::{ThalamicError, ThalamicRouter, ThalamicRouterConfig};

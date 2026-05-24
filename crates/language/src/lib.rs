//! NIMCP V2 — grounded language engine.
//!
//! A fresh Rust implementation of V1's `grounded_language` system (the
//! ~13 KLOC `grounded_language.c` core), shaped to V2 idioms: composed
//! modules instead of one ~80-field god-struct, `serde` checkpointing,
//! and a deterministic seeded RNG so tests are reproducible.
//!
//! # What lives here (built bottom-up; see `docs/V2_LANGUAGE_PLAN.md`)
//!
//! - [`concept`] — cross-modal concept registry (union-find over text /
//!   visual / audio fingerprints).
//! - [`lexicon`] — word↔concept lexicon with Hebbian bindings and
//!   per-word distributional context vectors.
//!
//! Later phases add distributional learning (SGNS), n-gram tables,
//! comprehend / produce, and persistence.
//!
//! # Design invariants
//!
//! - Deterministic: same seed → identical learning across runs/platforms.
//! - No `void*` / borrowed-pointer-rewire-at-init — V1's opaque
//!   attachment pointers become real Rust types when wired in `nimcp-brain`.
//! - One canonical persistence format (V1 had two divergent serializers).

#![forbid(unsafe_code)]
#![allow(missing_docs)]

pub mod cascade;
pub mod comprehend;
pub mod concept;
pub mod engine;
pub mod lexicon;
pub mod persistence;
pub mod phrase;
pub mod produce;
pub mod spectrum;
pub mod text;

pub use cascade::{CascadeConfig, Response};
pub use comprehend::ComprehensionResult;
pub use concept::{ConceptId, ConceptRegistry};
pub use engine::{GroundedLanguage, LanguageStats};
pub use persistence::{PersistError, FORMAT_MAGIC, FORMAT_VERSION};
pub use lexicon::{Lexicon, LexiconEntry, Modality, WordBinding, WordClass, MODALITY_COUNT};
pub use phrase::PhraseTable;
pub use produce::ProductionResult;
pub use spectrum::{BigramSpectralMetrics, BigramSpectrum};
pub use text::tokenize;

/// Part-of-speech expected after `prev` in a simple SVO skeleton (V1
/// `gl_pos_expected_next`). `None` when there's no expectation.
#[must_use]
pub fn pos_expected_next(prev: Option<lexicon::WordClass>) -> Option<lexicon::WordClass> {
    use lexicon::WordClass::{Adjective, Adverb, Noun, Verb};
    match prev {
        None => Some(Noun),           // sentence start → subject noun
        Some(Adjective) => Some(Noun),
        Some(Noun) => Some(Verb),
        Some(Verb) => Some(Noun),
        Some(Adverb) => Some(Verb),
        _ => None,
    }
}

/// Default semantic / distributional vector width (V1 `GL_SEMANTIC_DIM`).
pub const SEMANTIC_DIM: usize = 128;

/// Default Hebbian learning rate (V1 `GL_HEBBIAN_LR_DEFAULT`).
pub const HEBBIAN_LR_DEFAULT: f32 = 0.1;

/// Bindings below this strength are inert / pruned (V1 `GL_ASSOC_PRUNE_THRESHOLD`).
pub const ASSOC_PRUNE_THRESHOLD: f32 = 0.01;

/// One-shot fast-map binding strength (V1 `GL_FAST_MAP_THRESHOLD`).
pub const FAST_MAP_STRENGTH: f32 = 0.8;

/// Distributional context window half-width (V1 `GL_CONTEXT_WINDOW`).
pub const CONTEXT_WINDOW: usize = 7;

/// Frequency-subsampling threshold T (V1 `FREQ_SUBSAMPLE_T`). Words seen
/// more than T times are down-weighted by `sqrt(T / freq)` so that common
/// function words don't dominate the distributional update — the
/// word2vec subsampling that, with negative sampling, prevents the
/// embedding collapse V1 hit repeatedly.
pub const FREQ_SUBSAMPLE_T: f32 = 100.0;

/// Negative samples per center word (V1 `K_NEG`).
pub const K_NEG: usize = 5;

/// Bigram-rerank weight α (V1 `GL_BIGRAM_RERANK_ALPHA`). Produce adds
/// `α·ln(1 + bigram_freq(prev, cand))` to each candidate's cosine score —
/// re-orders *within* the top-K, cosine stays primary.
pub const BIGRAM_RERANK_ALPHA: f32 = 0.05;

/// Max distinct multi-word phrases retained (V1 `GL_MAX_PHRASES`).
/// Overflow evicts the least-frequent entry.
pub const MAX_PHRASES: usize = 512;

/// Negation scope: a cue (`not`, `no`, `never`, `n't`, …) negates this
/// many following words (V1 `GL_NEGATION_WINDOW`).
pub const NEGATION_WINDOW: usize = 3;

/// Discourse recency decay base — turn `d` steps back contributes
/// `0.6^d` to the running context vector (V1 `push_turn`).
pub const DISCOURSE_RECENCY: f32 = 0.6;

/// Discourse ring capacity (recent comprehended turns retained).
pub const DISCOURSE_CAPACITY: usize = 8;

/// Comprehension weights (V1): grounded concept features dominate,
/// distributional context next, NLP embedding least.
pub const W_CONCEPT_FEATURES: f32 = 0.6;
pub const W_DISTRIBUTIONAL: f32 = 0.3;

/// Produce candidate-pool size (V1 `GL_PRODUCE_TOPK`).
pub const PRODUCE_TOPK: usize = 32;

/// Produce scoring blend: `0.4·cos(intent, ctx) + 0.6·max_b strength·
/// cos(intent, concept_features)` (V1 `score_word_against_vector`).
pub const W_PRODUCE_CTX: f32 = 0.4;
pub const W_PRODUCE_BINDING: f32 = 0.6;

/// If the best candidate scores below this, produce shuffles the pool
/// (deterministically, seeded from the intent) to avoid stuck-on-one-word
/// collapse (V1 `GL_DIVERSITY_MIN_TOPSCORE`).
pub const DIVERSITY_MIN_TOPSCORE: f32 = 0.05;

/// Developmental confidence floor by stage — the **sole** production
/// length authority (V1 `gl_produce_confidence_floor`). Stage 0 emits one
/// word; later stages allow longer, lower-confidence continuations; stage
/// 4+ imposes no floor. Gated on raw cosine, never the bias-inflated score.
#[must_use]
pub fn produce_confidence_floor(stage: u32) -> f32 {
    match stage {
        0 => 1.0,
        1 => 0.30,
        2 => 0.15,
        3 => 0.05,
        _ => 0.0,
    }
}

/// Stage-scaled POS-transition bias weight (V1 Tier-1 Step C). Off at
/// stages 0–1 so early production isn't shoehorned into a grammar it
/// hasn't learned; ramps in at stage 2+.
#[must_use]
pub fn pos_bias_weight(stage: u32) -> f32 {
    match stage {
        0 | 1 => 0.0,
        2 => 0.08,
        3 => 0.12,
        _ => 0.15,
    }
}

/// Cascade content-intent combine weights (V1 content stage): the
/// comprehended prompt dominates; prior discourse context contributes
/// less. (The drive / goal / listener / episodic weights map to brain
/// subsystems V2 doesn't have yet — they enter as optional modulation.)
pub const CASCADE_W_PROMPT: f32 = 1.0;
pub const CASCADE_W_CONTEXT: f32 = 0.25;

/// Speech-repair perturbation fraction — each recurrent retry adds this
/// much Gaussian-ish noise to the content intent (V1 `REPAIR_NOISE_FRAC`).
pub const CASCADE_REPAIR_NOISE_FRAC: f32 = 0.1;

// -------------------------------------------------------------------------
// Shared math — ports of the V1 `static` helpers in grounded_language.c.
// -------------------------------------------------------------------------

/// Cosine similarity with the V1 epsilon guard (`1e-8`). Returns `0.0`
/// when either vector has (near-)zero norm or the lengths differ.
#[must_use]
pub fn cosine_similarity(a: &[f32], b: &[f32]) -> f32 {
    if a.len() != b.len() {
        return 0.0;
    }
    let mut dot = 0.0_f32;
    let mut na = 0.0_f32;
    let mut nb = 0.0_f32;
    for (&x, &y) in a.iter().zip(b.iter()) {
        dot += x * y;
        na += x * x;
        nb += y * y;
    }
    let denom = na.sqrt() * nb.sqrt();
    if denom < 1e-8 {
        0.0
    } else {
        dot / denom
    }
}

/// L2-normalize in place. No-op when the norm is below `1e-8` (avoids
/// dividing a numerically-zero vector — matches V1 `normalize_vector`).
pub fn normalize_vector(v: &mut [f32]) {
    let mut n = 0.0_f32;
    for &x in v.iter() {
        n += x * x;
    }
    let norm = n.sqrt();
    if norm < 1e-8 {
        return;
    }
    let inv = 1.0 / norm;
    for x in v.iter_mut() {
        *x *= inv;
    }
}

/// Deterministic `xorshift64` PRNG — the V1 lexicon RNG. Kept as a small
/// explicit type (rather than `rand`) so persisted `rng_state` round-trips
/// bit-for-bit and learning is reproducible from a seed.
#[derive(Debug, Clone, Copy, serde::Serialize, serde::Deserialize)]
pub struct XorShift64 {
    state: u64,
}

impl XorShift64 {
    /// Seed the generator. A zero seed is remapped (xorshift can't leave 0).
    #[must_use]
    pub fn new(seed: u64) -> Self {
        Self {
            state: if seed == 0 { 0x9E37_79B9_7F4A_7C15 } else { seed },
        }
    }

    /// Next raw 64-bit value (xorshift64, shifts 13/7/17 — the V1 variant).
    pub fn next_u64(&mut self) -> u64 {
        let mut x = self.state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.state = x;
        x
    }

    /// Next `f32` in `[0, 1)`.
    pub fn next_f32(&mut self) -> f32 {
        // Top 24 bits → [0,1) with 2^-24 resolution.
        #[allow(clippy::cast_precision_loss)]
        let v = (self.next_u64() >> 40) as f32;
        v / (1u32 << 24) as f32
    }

    /// Next `f32` in `[-half_range, half_range)`.
    pub fn next_centered(&mut self, half_range: f32) -> f32 {
        (self.next_f32() * 2.0 - 1.0) * half_range
    }
}

/// FNV-1a 32-bit hash over the lowercased bytes of `s` — the V1
/// `hash_word`. Exposed for fingerprinting / persistence parity.
#[must_use]
pub fn fnv1a_lower(s: &str) -> u32 {
    let mut h = 0x811c_9dc5_u32;
    for b in s.bytes() {
        let lb = b.to_ascii_lowercase();
        h ^= u32::from(lb);
        h = h.wrapping_mul(0x0100_0193);
    }
    h
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn cosine_identical_is_one() {
        let a = [1.0, 2.0, 3.0];
        assert!((cosine_similarity(&a, &a) - 1.0).abs() < 1e-6);
    }

    #[test]
    fn cosine_orthogonal_is_zero() {
        assert_eq!(cosine_similarity(&[1.0, 0.0], &[0.0, 1.0]), 0.0);
    }

    #[test]
    fn cosine_zero_vector_is_zero() {
        assert_eq!(cosine_similarity(&[0.0, 0.0], &[1.0, 1.0]), 0.0);
    }

    #[test]
    fn cosine_length_mismatch_is_zero() {
        assert_eq!(cosine_similarity(&[1.0], &[1.0, 2.0]), 0.0);
    }

    #[test]
    fn normalize_makes_unit_norm() {
        let mut v = [3.0, 4.0];
        normalize_vector(&mut v);
        let n = (v[0] * v[0] + v[1] * v[1]).sqrt();
        assert!((n - 1.0).abs() < 1e-6);
    }

    #[test]
    fn normalize_zero_is_noop() {
        let mut v = [0.0, 0.0];
        normalize_vector(&mut v);
        assert_eq!(v, [0.0, 0.0]);
    }

    #[test]
    fn xorshift_is_deterministic() {
        let mut a = XorShift64::new(42);
        let mut b = XorShift64::new(42);
        for _ in 0..100 {
            assert_eq!(a.next_u64(), b.next_u64());
        }
    }

    #[test]
    fn xorshift_f32_in_unit_range() {
        let mut r = XorShift64::new(7);
        for _ in 0..1000 {
            let v = r.next_f32();
            assert!((0.0..1.0).contains(&v), "out of range: {v}");
        }
    }

    #[test]
    fn xorshift_zero_seed_remapped() {
        let mut r = XorShift64::new(0);
        assert_ne!(r.next_u64(), 0);
    }

    #[test]
    fn fnv1a_is_case_insensitive() {
        assert_eq!(fnv1a_lower("Hello"), fnv1a_lower("hello"));
        assert_ne!(fnv1a_lower("hello"), fnv1a_lower("world"));
    }
}

//! Comprehension — text → semantic vector + activated concepts. Port of
//! V1's `grounded_language_comprehend` (core path; the opt-in V1 stages —
//! morphology, fuzzy match, WSD, coref, speech-act — are deferred and
//! will land as composable pipeline stages, per the plan).
//!
//! # Algorithm
//!
//! 1. Tokenize. Mark negation: a cue (`not`, `no`, `never`, `n't`, …)
//!    flips the *activation sign* of the next [`crate::NEGATION_WINDOW`]
//!    words — but never flips the semantic vector itself (V1 invariant).
//! 2. Per known word, over its bindings (strength ≥ prune threshold):
//!    - accumulate signed activation per concept;
//!    - integrate the semantic vector as a weighted sum of grounded
//!      concept features ([`crate::W_CONCEPT_FEATURES`]) + the word's
//!      distributional context vector ([`crate::W_DISTRIBUTIONAL`]),
//!      each divided by `word_count` so longer inputs don't inflate.
//! 3. `confidence = known / word_count`, `novelty = 1 − confidence`.
//! 4. Normalize the semantic vector; push it onto the discourse ring
//!    (recency-weighted context).

use serde::{Deserialize, Serialize};

use crate::concept::ConceptId;
use crate::engine::GroundedLanguage;
use crate::text::tokenize;
use crate::{ASSOC_PRUNE_THRESHOLD, NEGATION_WINDOW, W_CONCEPT_FEATURES, W_DISTRIBUTIONAL, normalize_vector};

/// Result of comprehending a text span.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComprehensionResult {
    /// Integrated, L2-normalized semantic vector (length = `semantic_dim`).
    pub semantic_vector: Vec<f32>,
    /// Concepts activated, with their (signed) activation levels.
    pub activated_concepts: Vec<ConceptId>,
    pub activation_levels: Vec<f32>,
    /// Fraction of input words that were known: `known / word_count`.
    pub comprehension_confidence: f32,
    /// `1 − comprehension_confidence`.
    pub novelty: f32,
}

impl ComprehensionResult {
    fn empty(dim: usize) -> Self {
        Self {
            semantic_vector: vec![0.0; dim],
            activated_concepts: Vec::new(),
            activation_levels: Vec::new(),
            comprehension_confidence: 0.0,
            novelty: 1.0,
        }
    }
}

/// Negation cues (whole-word). The contraction suffix `n't` is matched
/// separately.
const NEGATION_CUES: &[&str] = &[
    "not", "no", "never", "none", "nobody", "nothing", "neither", "nor", "without", "cannot",
];

fn is_negation_cue(word: &str) -> bool {
    NEGATION_CUES.contains(&word) || word.ends_with("n't")
}

impl GroundedLanguage {
    /// Comprehend a text span (see module docs). Mutates discourse state
    /// (pushes the turn vector) and bumps `stats.total_comprehensions`.
    pub fn comprehend(&mut self, text: &str) -> ComprehensionResult {
        let dim = self.lexicon.semantic_dim;
        let tokens = tokenize(text);
        if tokens.is_empty() {
            return ComprehensionResult::empty(dim);
        }
        self.stats.total_comprehensions += 1;

        // Negation marks: a cue negates the next NEGATION_WINDOW words.
        let mut negate = vec![false; tokens.len()];
        let mut remaining = 0usize;
        for (i, tok) in tokens.iter().enumerate() {
            if remaining > 0 {
                negate[i] = true;
                remaining -= 1;
            }
            if is_negation_cue(tok) {
                remaining = NEGATION_WINDOW;
            }
        }

        let mut sem = vec![0.0_f32; dim];
        let mut concepts: Vec<ConceptId> = Vec::new();
        let mut levels: Vec<f32> = Vec::new();
        let mut known = 0usize;
        #[allow(clippy::cast_precision_loss)]
        let inv_wc = 1.0 / tokens.len() as f32;

        for (i, tok) in tokens.iter().enumerate() {
            let Some(idx) = self.lexicon.find(tok) else {
                continue; // unknown word (subword fallback is a later stage)
            };
            known += 1;
            let polarity = if negate[i] { -1.0 } else { 1.0 };

            // Snapshot what we need (immutable borrow of the entry).
            let entry = self.lexicon.entry(idx);
            let ctx_init = entry.context_initialized;
            // (concept_id, strength) for bindings above the prune floor.
            let bindings: Vec<(ConceptId, f32)> = entry
                .bindings
                .iter()
                .filter(|b| b.strength >= ASSOC_PRUNE_THRESHOLD)
                .map(|b| (b.concept_id, b.strength))
                .collect();
            // Distributional term (0.3) — uses the word's context vector.
            if ctx_init {
                for (s, &c) in sem.iter_mut().zip(entry.context_vector.iter()) {
                    *s += c * W_DISTRIBUTIONAL * inv_wc;
                }
            }

            for (cid, strength) in bindings {
                let canon = self.concepts.canonical(cid);
                // Signed activation (negation flips the sign here only).
                merge_activation(&mut concepts, &mut levels, canon, polarity * strength);
                // Concept-feature term (0.6) — uses |strength| (negation
                // does NOT flip the semantic vector, only activations).
                if let Some(feat) = self.concept_features(canon) {
                    for (s, &f) in sem.iter_mut().zip(feat.iter()) {
                        *s += f * strength * W_CONCEPT_FEATURES * inv_wc;
                    }
                }
            }
        }

        #[allow(clippy::cast_precision_loss)]
        let confidence = known as f32 * inv_wc;
        normalize_vector(&mut sem);
        self.discourse.push_turn(&sem);

        ComprehensionResult {
            semantic_vector: sem,
            activated_concepts: concepts,
            activation_levels: levels,
            comprehension_confidence: confidence,
            novelty: 1.0 - confidence,
        }
    }
}

/// Accumulate `delta` onto `cid`'s activation, deduplicating concepts.
fn merge_activation(concepts: &mut Vec<ConceptId>, levels: &mut Vec<f32>, cid: ConceptId, delta: f32) {
    if let Some(pos) = concepts.iter().position(|&c| c == cid) {
        levels[pos] += delta;
    } else {
        concepts.push(cid);
        levels.push(delta);
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::lexicon::Modality;

    fn grounded_engine() -> GroundedLanguage {
        let mut gl = GroundedLanguage::new(8, 1);
        // Ground three words with distinct one-hot-ish feature vectors.
        gl.ground("dog", &[1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("cat", &[0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("runs", &[0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Motor);
        gl
    }

    #[test]
    fn empty_text_is_zero_confidence() {
        let mut gl = GroundedLanguage::new(8, 1);
        let r = gl.comprehend("   ");
        assert_eq!(r.comprehension_confidence, 0.0);
        assert_eq!(r.novelty, 1.0);
        assert!(r.semantic_vector.iter().all(|&x| x == 0.0));
    }

    #[test]
    fn known_words_activate_concepts_and_build_vector() {
        let mut gl = grounded_engine();
        let r = gl.comprehend("the dog runs");
        // "the" unknown, "dog"+"runs" known → confidence 2/3.
        assert!((r.comprehension_confidence - 2.0 / 3.0).abs() < 1e-6);
        assert_eq!(r.activated_concepts.len(), 2);
        let norm: f32 = r.semantic_vector.iter().map(|x| x * x).sum::<f32>().sqrt();
        assert!((norm - 1.0).abs() < 1e-5, "vector should be normalized");
    }

    #[test]
    fn all_known_is_full_confidence() {
        let mut gl = grounded_engine();
        let r = gl.comprehend("dog cat runs");
        assert!((r.comprehension_confidence - 1.0).abs() < 1e-6);
        assert_eq!(r.novelty, 0.0);
    }

    #[test]
    fn negation_flips_activation_sign_not_vector() {
        let mut gl = grounded_engine();
        let plain = gl.comprehend("dog");
        let negated = gl.comprehend("not dog");
        // Activation sign flips negative under negation.
        let dog = gl.concepts.intern_text("dog");
        let pa = plain.activation_levels[plain.activated_concepts.iter().position(|&c| c == dog).unwrap()];
        let na = negated.activation_levels[negated.activated_concepts.iter().position(|&c| c == dog).unwrap()];
        assert!(pa > 0.0, "plain activation positive");
        assert!(na < 0.0, "negated activation negative");
        // But the semantic vector keeps the concept's (positive) feature
        // signature — direction unchanged by negation.
        assert!(
            crate::cosine_similarity(&plain.semantic_vector, &negated.semantic_vector) > 0.99,
            "negation must not flip the semantic vector"
        );
    }

    #[test]
    fn discourse_context_builds_over_turns() {
        let mut gl = grounded_engine();
        assert_eq!(gl.discourse.depth(), 0);
        gl.comprehend("dog runs");
        gl.comprehend("cat runs");
        assert_eq!(gl.discourse.depth(), 2);
        let ctx = gl.discourse.context_vector();
        let norm: f32 = ctx.iter().map(|x| x * x).sum::<f32>().sqrt();
        assert!((norm - 1.0).abs() < 1e-5 || norm < 1e-6);
    }

    #[test]
    fn unknown_only_input_pushes_no_turn() {
        let mut gl = grounded_engine();
        let r = gl.comprehend("xyzzy plugh");
        assert_eq!(r.comprehension_confidence, 0.0);
        // Zero vector → discourse head doesn't advance (V1 gate).
        assert_eq!(gl.discourse.depth(), 0);
        // But the call still counts.
        assert_eq!(gl.stats.total_comprehensions, 1);
    }
}

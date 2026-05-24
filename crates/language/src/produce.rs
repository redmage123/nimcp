//! Production — semantic intent vector → text. Port of V1's
//! `grounded_language_produce` (the Option-B lexicon readout: decode
//! against the engine's own lexicon, since V1's SNN bridge decoder is a
//! stub).
//!
//! # Algorithm
//!
//! 1. Score every content word (Function / Pronoun dropped) against the
//!    intent: `0.4·cos(intent, ctx) + 0.6·max_b strength·cos(intent,
//!    concept_features)`, with a negative-valence damp. Keep the top
//!    [`crate::PRODUCE_TOPK`].
//! 2. If the best score is below [`crate::DIVERSITY_MIN_TOPSCORE`],
//!    deterministically shuffle the pool (seeded from the intent) so we
//!    don't emit the same word forever (V1 diversity fallback).
//! 3. Greedily emit unused candidates maximizing `cos + bigram_bias +
//!    pos_bias`, where:
//!    - `bigram_bias = α·ln(1 + freq(prev, cand))`;
//!    - `pos_bias = pos_weight(stage)·class_confidence` when the
//!      candidate's class is the [`crate::pos_expected_next`] of the
//!      previous word's class.
//! 4. Stop when the **raw cosine** of the best candidate falls below
//!    [`crate::produce_confidence_floor`] for the stage — but always emit
//!    at least `min_produce_words`. The floor (not a hard cap) is the sole
//!    length authority.

use serde::{Deserialize, Serialize};

use crate::engine::GroundedLanguage;
use crate::lexicon::WordClass;
use crate::{
    DIVERSITY_MIN_TOPSCORE, PRODUCE_TOPK, W_PRODUCE_BINDING, W_PRODUCE_CTX, XorShift64,
    cosine_similarity, pos_bias_weight, pos_expected_next, produce_confidence_floor,
};

/// Result of producing text from an intent vector.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProductionResult {
    pub text: String,
    pub words: Vec<String>,
    /// Top candidate's raw cosine score.
    pub fluency: f32,
    /// Mean raw cosine of the emitted words.
    pub relevance: f32,
    /// Placeholder (V1 also stubs this at 0.5 pending a real metric).
    pub creativity: f32,
    /// Copy of the driving intent.
    pub semantic_vector: Vec<f32>,
}

impl ProductionResult {
    fn empty(intent: &[f32]) -> Self {
        Self {
            text: String::new(),
            words: Vec::new(),
            fluency: 0.0,
            relevance: 0.0,
            creativity: 0.5,
            semantic_vector: intent.to_vec(),
        }
    }
}

impl GroundedLanguage {
    /// Score one lexicon entry against the intent (V1 `score_word_against_vector`).
    fn score_word(&self, idx: usize, intent: &[f32]) -> f32 {
        let entry = self.lexicon.entry(idx);
        let ctx_cos = if entry.context_initialized {
            cosine_similarity(intent, &entry.context_vector)
        } else {
            0.0
        };
        let mut binding_cos = 0.0_f32;
        for b in &entry.bindings {
            let canon = self.concepts.find_root(b.concept_id);
            if let Some(feat) = self.concept_features(canon) {
                let c = b.strength * cosine_similarity(intent, feat);
                if c > binding_cos {
                    binding_cos = c;
                }
            }
        }
        let mut raw = W_PRODUCE_CTX * ctx_cos + W_PRODUCE_BINDING * binding_cos;
        // Negative-valence damp: factor in [0.5, 1.0].
        if entry.valence < 0.0 {
            raw *= (1.0 + entry.valence * 0.5).clamp(0.5, 1.0);
        }
        raw
    }

    /// Build the top-K candidate pool: `(entry_idx, score)` sorted
    /// descending. Function / Pronoun words are excluded.
    fn candidate_pool(&self, intent: &[f32]) -> Vec<(usize, f32)> {
        let mut scored: Vec<(usize, f32)> = Vec::new();
        for (idx, entry) in self.lexicon.entries().iter().enumerate() {
            if matches!(entry.learned_class, WordClass::Function | WordClass::Pronoun) {
                continue;
            }
            scored.push((idx, self.score_word(idx, intent)));
        }
        scored.sort_by(|a, b| b.1.total_cmp(&a.1));
        scored.truncate(PRODUCE_TOPK);
        scored
    }

    /// Produce text from an `intent` vector at developmental `stage`,
    /// emitting at least `min_produce_words`.
    #[must_use]
    pub fn produce(&self, intent: &[f32], stage: u32, min_produce_words: usize) -> ProductionResult {
        if self.lexicon.vocab_count() == 0 {
            return ProductionResult::empty(intent);
        }
        let mut pool = self.candidate_pool(intent);
        if pool.is_empty() {
            return ProductionResult::empty(intent);
        }

        let fluency = pool[0].1;
        // Diversity fallback: stuck near zero → deterministic shuffle.
        if pool[0].1 < DIVERSITY_MIN_TOPSCORE {
            let mut rng = XorShift64::new(seed_from_intent(intent));
            fisher_yates(&mut pool, &mut rng);
        }

        let floor = produce_confidence_floor(stage);
        let pos_w = pos_bias_weight(stage);
        let min_words = min_produce_words.max(1);

        let mut emitted: Vec<usize> = Vec::new();
        let mut emitted_cos: Vec<f32> = Vec::new();
        let mut prev_idx: Option<usize> = None;
        let mut prev_class: Option<WordClass> = None;

        // Bounded by pool size (no word reused) → terminates.
        loop {
            let mut best: Option<usize> = None;
            let mut best_total = f32::NEG_INFINITY;
            let mut best_cos = 0.0_f32;
            for &(cand_idx, cand_cos) in &pool {
                if emitted.contains(&cand_idx) {
                    continue;
                }
                let mut total = cand_cos;
                if let Some(p) = prev_idx {
                    total += self.phrases.bigram_bias(p, cand_idx);
                }
                if pos_w > 0.0 {
                    let entry = self.lexicon.entry(cand_idx);
                    if pos_expected_next(prev_class) == Some(entry.learned_class) {
                        total += pos_w * entry.class_confidence;
                    }
                }
                if total > best_total {
                    best_total = total;
                    best = Some(cand_idx);
                    best_cos = cand_cos;
                }
            }
            let Some(best_idx) = best else {
                break; // pool exhausted
            };
            // Confidence-floor stop — gated on RAW cosine, after the
            // minimum word count. Always emits ≥ min_words.
            if emitted.len() >= min_words && best_cos < floor {
                break;
            }
            emitted.push(best_idx);
            emitted_cos.push(best_cos);
            prev_idx = Some(best_idx);
            prev_class = Some(self.lexicon.entry(best_idx).learned_class);
        }

        let words: Vec<String> = emitted
            .iter()
            .map(|&i| self.lexicon.entry(i).form.clone())
            .collect();
        let relevance = if emitted_cos.is_empty() {
            0.0
        } else {
            #[allow(clippy::cast_precision_loss)]
            let n = emitted_cos.len() as f32;
            emitted_cos.iter().sum::<f32>() / n
        };
        ProductionResult {
            text: words.join(" "),
            words,
            fluency,
            relevance,
            creativity: 0.5,
            semantic_vector: intent.to_vec(),
        }
    }
}

/// Fold an intent vector into a u64 seed (deterministic).
fn seed_from_intent(intent: &[f32]) -> u64 {
    let mut h = 0xcbf2_9ce4_8422_2325_u64;
    for &x in intent {
        h ^= u64::from(x.to_bits());
        h = h.wrapping_mul(0x0000_0100_0000_01b3);
    }
    h
}

/// In-place deterministic Fisher–Yates shuffle.
fn fisher_yates<T>(v: &mut [T], rng: &mut XorShift64) {
    let n = v.len();
    for i in (1..n).rev() {
        let j = (rng.next_u64() % (i as u64 + 1)) as usize;
        v.swap(i, j);
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::lexicon::Modality;

    /// Engine with three grounded words at distinct one-hot directions.
    fn grounded() -> GroundedLanguage {
        let mut gl = GroundedLanguage::new(4, 1);
        gl.ground("dog", &[1.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("cat", &[0.0, 1.0, 0.0, 0.0], Modality::Visual);
        gl.ground("run", &[0.0, 0.0, 1.0, 0.0], Modality::Motor);
        gl
    }

    #[test]
    fn empty_lexicon_produces_nothing() {
        let gl = GroundedLanguage::new(4, 1);
        let r = gl.produce(&[1.0, 0.0, 0.0, 0.0], 4, 1);
        assert!(r.text.is_empty());
        assert!(r.words.is_empty());
    }

    #[test]
    fn intent_aligned_word_is_emitted_first() {
        let gl = grounded();
        // Intent points at "cat".
        let r = gl.produce(&[0.0, 1.0, 0.0, 0.0], 4, 1);
        assert_eq!(r.words.first().map(String::as_str), Some("cat"));
        assert!(r.fluency > 0.5);
    }

    #[test]
    fn stage0_emits_exactly_one_word() {
        let gl = grounded();
        // Stage 0 floor = 1.0 → after the (min) first word, nothing else
        // clears the floor → exactly one word.
        let r = gl.produce(&[0.7, 0.7, 0.0, 0.0], 0, 1);
        assert_eq!(r.words.len(), 1);
    }

    #[test]
    fn higher_stage_emits_more_than_stage0() {
        let gl = grounded();
        let intent = [0.6, 0.5, 0.4, 0.0];
        let s0 = gl.produce(&intent, 0, 1).words.len();
        let s4 = gl.produce(&intent, 4, 1).words.len();
        assert_eq!(s0, 1);
        assert!(s4 >= s0, "stage 4 (no floor) should emit >= stage 0");
    }

    #[test]
    fn min_produce_words_is_respected_even_at_stage0() {
        let gl = grounded();
        // Stage 0 floor 1.0 would stop at 1 word, but min=2 forces 2.
        let r = gl.produce(&[0.0, 0.0, 1.0, 0.0], 0, 2);
        assert_eq!(r.words.len(), 2);
    }

    #[test]
    fn never_reuses_a_word() {
        let gl = grounded();
        let r = gl.produce(&[0.5, 0.5, 0.5, 0.0], 4, 1);
        let mut uniq = r.words.clone();
        uniq.sort();
        uniq.dedup();
        assert_eq!(uniq.len(), r.words.len(), "no word repeats");
    }

    #[test]
    fn diversity_fallback_still_emits_on_orthogonal_intent() {
        let gl = grounded();
        // Intent orthogonal to every grounded direction → all scores ~0
        // → diversity shuffle engages, but we still emit min words.
        let r = gl.produce(&[0.0, 0.0, 0.0, 1.0], 2, 1);
        assert!(!r.words.is_empty(), "must always emit at least min words");
    }

    #[test]
    fn deterministic() {
        let gl = grounded();
        let intent = [0.3, 0.6, 0.1, 0.0];
        let a = gl.produce(&intent, 3, 1);
        let b = gl.produce(&intent, 3, 1);
        assert_eq!(a.words, b.words);
    }
}

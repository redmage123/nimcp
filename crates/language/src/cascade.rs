//! Communication cascade — the production pipeline that turns an input
//! prompt into a response. A reduced, faithful port of V1's 15-stage
//! `communication_cascade.c`: the stages that actually generate output
//! (content → lexical → self-comprehension) are implemented here; the
//! drive / goal / listener / episodic / prosody / phonological stages map
//! to brain subsystems V2 doesn't have yet and enter later as optional
//! modulation hooks.
//!
//! # Pipeline
//!
//! 1. **Comprehend** the prompt (advances discourse).
//! 2. **Content**: build the intent vector — `1.0·prompt + 0.25·prior
//!    context`, normalized (V1 content-stage combine, reduced).
//! 3. **Lexical**: [`GroundedLanguage::produce`] the utterance, with the
//!    developmental confidence floor as the sole length authority.
//! 4. **Self-comprehension**: re-comprehend the utterance (without
//!    touching discourse) and measure `self_match = cos(intent, re-
//!    comprehended)`. `pe_total = 1 − self_match` (FEP prediction error).
//! 5. **Speech-repair (recurrent)**: if `recurrent_max_iters > 1`, retry
//!    with small deterministic perturbations of the intent and keep the
//!    best `self_match` (V1 perturbation-retry).

use serde::{Deserialize, Serialize};

use crate::engine::GroundedLanguage;
use crate::{
    CASCADE_REPAIR_NOISE_FRAC, CASCADE_W_CONTEXT, CASCADE_W_PROMPT, XorShift64, cosine_similarity,
    normalize_vector,
};

/// Cascade configuration.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct CascadeConfig {
    /// Developmental stage (drives the produce confidence floor + POS bias).
    pub stage: u32,
    /// Minimum words to emit (honored by every produce stop condition).
    pub min_produce_words: usize,
    /// Recurrent settling iterations (`1` = single pass; `>1` enables the
    /// speech-repair perturbation retry).
    pub recurrent_max_iters: usize,
}

impl Default for CascadeConfig {
    fn default() -> Self {
        Self {
            stage: 4,
            min_produce_words: 1,
            recurrent_max_iters: 1,
        }
    }
}

/// Cascade output.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Response {
    pub text: String,
    pub words: Vec<String>,
    /// Response confidence = `self_match` (falls back to `fluency`).
    pub confidence: f32,
    /// Top-candidate cosine at production.
    pub fluency: f32,
    /// Cosine(intent, re-comprehended own output) — the sensorimotor loop.
    pub self_match: f32,
    /// FEP prediction error: `1 − self_match`.
    pub pe_total: f32,
    /// Confidence of comprehending the *prompt*.
    pub comprehension_confidence: f32,
    /// Recurrent iterations actually run.
    pub iterations: usize,
}

impl GroundedLanguage {
    /// Run the cascade on `input`, producing a [`Response`].
    pub fn respond(&mut self, input: &str, cfg: &CascadeConfig) -> Response {
        // Prior discourse context BEFORE this prompt folds in.
        let ctx_before: Vec<f32> = self.discourse.context_vector().to_vec();

        // 1. Comprehend the prompt (advances discourse).
        let comp = self.comprehend(input);

        // 2. Content: blend prompt comprehension + prior context.
        let dim = self.lexicon.semantic_dim;
        let mut intent = vec![0.0_f32; dim];
        for (s, &p) in intent.iter_mut().zip(comp.semantic_vector.iter()) {
            *s += CASCADE_W_PROMPT * p;
        }
        if ctx_before.len() == dim {
            for (s, &c) in intent.iter_mut().zip(ctx_before.iter()) {
                *s += CASCADE_W_CONTEXT * c;
            }
        }
        normalize_vector(&mut intent);

        // 3-5. Lexical + self-comprehension, with optional recurrent repair.
        let max_iters = cfg.recurrent_max_iters.max(1);
        let mut best = self.run_once(&intent, cfg);
        let mut iterations = 1;
        if max_iters > 1 {
            let mut rng = XorShift64::new(seed_from(&intent));
            for _ in 1..max_iters {
                // Already a perfect echo → nothing to repair.
                if best.1 >= 0.999 {
                    break;
                }
                let mut perturbed = intent.clone();
                for v in &mut perturbed {
                    *v += rng.next_centered(CASCADE_REPAIR_NOISE_FRAC);
                }
                normalize_vector(&mut perturbed);
                let cand = self.run_once(&perturbed, cfg);
                iterations += 1;
                if cand.1 > best.1 {
                    best = cand;
                }
            }
        }

        let (prod, self_match) = best;
        let confidence = if self_match > 0.0 { self_match } else { prod.fluency };
        Response {
            text: prod.text,
            words: prod.words,
            confidence,
            fluency: prod.fluency,
            self_match,
            pe_total: 1.0 - self_match,
            comprehension_confidence: comp.comprehension_confidence,
            iterations,
        }
    }

    /// One produce + self-comprehend pass. Returns the production and the
    /// `self_match` (cosine of the intent vs the re-comprehended output).
    fn run_once(&mut self, intent: &[f32], cfg: &CascadeConfig) -> (crate::produce::ProductionResult, f32) {
        let prod = self.produce(intent, cfg.stage, cfg.min_produce_words);
        if prod.text.is_empty() {
            return (prod, 0.0);
        }
        let sc = self.comprehend_no_discourse(&prod.text);
        let self_match = cosine_similarity(intent, &sc.semantic_vector);
        (prod, self_match)
    }
}

/// Deterministic u64 seed from an intent vector.
fn seed_from(v: &[f32]) -> u64 {
    let mut h = 0xcbf2_9ce4_8422_2325_u64;
    for &x in v {
        h ^= u64::from(x.to_bits());
        h = h.wrapping_mul(0x0000_0100_0000_01b3);
    }
    h
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::lexicon::Modality;

    fn engine() -> GroundedLanguage {
        let mut gl = GroundedLanguage::new(8, 1);
        gl.ground("dog", &[1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("cat", &[0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("run", &[0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Motor);
        gl.ground("food", &[0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl
    }

    #[test]
    fn responds_with_text_to_known_prompt() {
        let mut gl = engine();
        let r = gl.respond("dog", &CascadeConfig::default());
        assert!(!r.text.is_empty());
        assert!(r.comprehension_confidence > 0.0);
        // Self-comprehension produced a finite match.
        assert!((0.0..=1.0).contains(&r.self_match));
        assert!((r.pe_total - (1.0 - r.self_match)).abs() < 1e-6);
    }

    #[test]
    fn stage0_response_is_one_word() {
        let mut gl = engine();
        let cfg = CascadeConfig { stage: 0, min_produce_words: 1, recurrent_max_iters: 1 };
        let r = gl.respond("dog cat", &cfg);
        assert_eq!(r.words.len(), 1, "stage-0 floor → one word");
    }

    #[test]
    fn unknown_prompt_still_safe() {
        let mut gl = engine();
        let r = gl.respond("zzz qqq", &CascadeConfig::default());
        // No discourse turn from an all-unknown prompt; response may be
        // empty or diversity-driven, but the call must not panic.
        assert!(r.comprehension_confidence == 0.0);
    }

    #[test]
    fn recurrent_keeps_best_and_counts_iters() {
        let mut gl = engine();
        let cfg = CascadeConfig { stage: 4, min_produce_words: 1, recurrent_max_iters: 5 };
        let r = gl.respond("dog", &cfg);
        assert!(r.iterations >= 1 && r.iterations <= 5);
        // Best self_match retained → confidence equals it when positive.
        if r.self_match > 0.0 {
            assert_eq!(r.confidence, r.self_match);
        }
    }

    #[test]
    fn self_comprehension_does_not_pollute_discourse() {
        let mut gl = engine();
        gl.respond("dog", &CascadeConfig::default());
        // Exactly one discourse turn (the prompt) — self-comp used the
        // no-discourse path.
        assert_eq!(gl.discourse.depth(), 1);
    }

    #[test]
    fn deterministic() {
        let mut a = engine();
        let mut b = engine();
        let cfg = CascadeConfig { stage: 3, min_produce_words: 2, recurrent_max_iters: 4 };
        let ra = a.respond("dog cat food", &cfg);
        let rb = b.respond("dog cat food", &cfg);
        assert_eq!(ra.words, rb.words);
        assert_eq!(ra.self_match, rb.self_match);
    }
}

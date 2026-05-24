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
    CASCADE_REPAIR_NOISE_FRAC, CASCADE_W_DISCOURSE_CONTINUITY, CASCADE_W_IMAGINATION,
    CASCADE_W_PROMPT, CASCADE_W_REASONING, CASCADE_W_WORKING_MEMORY, CASCADE_WM_SALIENCE_FLOOR,
    XorShift64, cosine_similarity, normalize_vector,
};

/// Add `scale · src` into `dst` element-wise, with V1's per-element
/// `isfinite` guard and min-length (truncation) semantics — a stray NaN /
/// Inf or a length mismatch never smears the intent.
fn blend(dst: &mut [f32], src: &[f32], scale: f32) {
    for (d, &s) in dst.iter_mut().zip(src.iter()) {
        if s.is_finite() {
            *d += scale * s;
        }
    }
}

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
    /// Opt-in: blend the reasoning-conclusion source into content intent
    /// (V1 `reason_in_content`, Tier-1 Step E 5e). Default OFF — zero cost
    /// / zero behavior change until a reasoning source is supplied AND this
    /// is enabled.
    pub reason_in_content: bool,
}

impl Default for CascadeConfig {
    fn default() -> Self {
        Self {
            stage: 4,
            min_produce_words: 1,
            recurrent_max_iters: 1,
            reason_in_content: false,
        }
    }
}

/// Cognitive/contextual sources blended into the content intent (V1
/// `cascade_stage_content` Steps D + E). The language crate stays free of
/// brain-subsystem dependencies: the caller (`nimcp-brain`, once it has
/// working memory / imagination / reasoning) supplies these vectors; the
/// discourse-continuity source is read natively from the engine's own ring.
///
/// All fields default to empty/`None` → the content build reduces to the
/// prompt + native discourse continuity, i.e. the dormant case.
#[derive(Debug, Clone, Copy, Default)]
pub struct ContentSources<'a> {
    /// Active working-memory items as `(feature_vector, salience)`. Items
    /// below [`crate::CASCADE_WM_SALIENCE_FLOOR`] are skipped; the rest are
    /// blended at `w_wm · salience` (V1 Step D).
    pub working_memory: &'a [(&'a [f32], f32)],
    /// Active imagined-scenario `(vector, vividness)`, blended at
    /// `w_imag · vividness` (V1 Step E 5d).
    pub imagination: Option<(&'a [f32], f32)>,
    /// Reasoning `(conclusion_vector, confidence)`, blended at
    /// `w_reason · confidence` — only when `CascadeConfig::reason_in_content`
    /// is set (V1 Step E 5e).
    pub reasoning: Option<(&'a [f32], f32)>,
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
    /// Run the cascade on `input` with no external cognitive sources
    /// (working memory / imagination / reasoning empty). Discourse
    /// continuity is still applied natively.
    pub fn respond(&mut self, input: &str, cfg: &CascadeConfig) -> Response {
        self.respond_with_sources(input, cfg, &ContentSources::default())
    }

    /// Run the cascade on `input`, blending the supplied cognitive
    /// [`ContentSources`] into the content intent (V1 `cascade_stage_content`
    /// Steps D + E). The caller supplies working-memory / imagination /
    /// reasoning vectors; discourse continuity is read from the engine.
    pub fn respond_with_sources(
        &mut self,
        input: &str,
        cfg: &CascadeConfig,
        sources: &ContentSources<'_>,
    ) -> Response {
        // 1. Comprehend the prompt (advances discourse → newest = back 1,
        //    so the prior exchange is back 2).
        let comp = self.comprehend(input);

        // 2. Content: prompt + discourse continuity + cognitive sources.
        let dim = self.lexicon.semantic_dim;
        let mut intent = vec![0.0_f32; dim];
        blend(&mut intent, &comp.semantic_vector, CASCADE_W_PROMPT);

        // 5c. Discourse continuity — the PRIOR turn (back = 2). No-ops on
        // the first turn. Read into a local to drop the borrow before the
        // mutable produce passes.
        if let Some(prior) = self.discourse.recent_turn_vector(2) {
            let prior = prior.to_vec();
            blend(&mut intent, &prior, CASCADE_W_DISCOURSE_CONTINUITY);
        }
        // 5b. Working memory — each salient item × salience.
        for (vec, salience) in sources.working_memory {
            if *salience >= CASCADE_WM_SALIENCE_FLOOR {
                blend(&mut intent, vec, CASCADE_W_WORKING_MEMORY * *salience);
            }
        }
        // 5d. Imagination — active scenario × vividness.
        if let Some((vec, vividness)) = sources.imagination {
            blend(&mut intent, vec, CASCADE_W_IMAGINATION * vividness);
        }
        // 5e. Reasoning — conclusion × confidence, gated default-OFF.
        if cfg.reason_in_content {
            if let Some((vec, confidence)) = sources.reasoning {
                blend(&mut intent, vec, CASCADE_W_REASONING * confidence);
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
        let cfg = CascadeConfig { stage: 0, min_produce_words: 1, recurrent_max_iters: 1, reason_in_content: false };
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
        let cfg = CascadeConfig { stage: 4, min_produce_words: 1, recurrent_max_iters: 5, reason_in_content: false };
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
        let cfg = CascadeConfig { stage: 3, min_produce_words: 2, recurrent_max_iters: 4, reason_in_content: false };
        let ra = a.respond("dog cat food", &cfg);
        let rb = b.respond("dog cat food", &cfg);
        assert_eq!(ra.words, rb.words);
        assert_eq!(ra.self_match, rb.self_match);
    }

    // --- Tier-1 Steps D + E: cognitive/discourse content sources ---

    #[test]
    fn discourse_continuity_uses_prior_turn() {
        let mut gl = engine();
        // First exchange establishes a prior turn.
        gl.respond("cat", &CascadeConfig::default());
        // Second turn: recent_turn_vector(2) is now the "cat" turn.
        let prior = gl.discourse.recent_turn_vector(1).map(<[f32]>::to_vec);
        assert!(prior.is_some());
        // The cascade must not panic and must still respond.
        let r = gl.respond("dog", &CascadeConfig::default());
        assert!(!r.text.is_empty());
        // After this turn, back=2 is the "cat" turn (continuity source).
        assert!(gl.discourse.recent_turn_vector(2).is_some());
    }

    #[test]
    fn working_memory_source_biases_production() {
        let mut gl = engine();
        // WM holds a strong "food" vector; prompt is empty-ish ("the").
        let food = [0.0_f32, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0];
        let wm: [(&[f32], f32); 1] = [(&food, 1.0)];
        let sources = ContentSources { working_memory: &wm, ..Default::default() };
        let cfg = CascadeConfig::default();
        let r = gl.respond_with_sources("the", &cfg, &sources);
        // With "the" unknown, WM is the dominant content source → "food".
        assert_eq!(r.words.first().map(String::as_str), Some("food"));
    }

    #[test]
    fn wm_below_salience_floor_is_ignored() {
        let mut gl = engine();
        let food = [0.0_f32, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0];
        // Salience 0.1 < floor 0.2 → contributes nothing.
        let wm: [(&[f32], f32); 1] = [(&food, 0.1)];
        let sources = ContentSources { working_memory: &wm, ..Default::default() };
        let plain = gl.respond("the", &CascadeConfig::default());
        let mut gl2 = engine();
        let gated = gl2.respond_with_sources("the", &CascadeConfig::default(), &sources);
        // Sub-floor WM doesn't change the (empty/diversity) outcome.
        assert_eq!(plain.words, gated.words);
    }

    #[test]
    fn reasoning_source_gated_by_flag() {
        let reason = [0.0_f32, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]; // points at "cat"
        let sources = ContentSources {
            reasoning: Some((&reason, 1.0)),
            ..Default::default()
        };
        // OFF: reasoning ignored.
        let mut gl_off = engine();
        let off = gl_off.respond_with_sources(
            "the",
            &CascadeConfig { reason_in_content: false, ..Default::default() },
            &sources,
        );
        // ON: reasoning drives intent toward "cat".
        let mut gl_on = engine();
        let on = gl_on.respond_with_sources(
            "the",
            &CascadeConfig { reason_in_content: true, ..Default::default() },
            &sources,
        );
        assert_eq!(on.words.first().map(String::as_str), Some("cat"));
        assert_ne!(off.words, on.words, "reason_in_content must gate the blend");
    }

    #[test]
    fn blend_skips_non_finite() {
        let mut gl = engine();
        let bad = [f32::NAN, f32::INFINITY, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0];
        let wm: [(&[f32], f32); 1] = [(&bad, 1.0)];
        let sources = ContentSources { working_memory: &wm, ..Default::default() };
        // Must not produce NaN/Inf in the response path.
        let r = gl.respond_with_sources("dog", &CascadeConfig::default(), &sources);
        assert!(r.confidence.is_finite());
        assert!(r.self_match.is_finite());
    }
}

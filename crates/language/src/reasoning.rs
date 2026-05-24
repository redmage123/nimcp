//! Reasoning — a lexicon-grounded functional core. V1's reasoning engine
//! delegates its intelligence to an engram + knowledge base V2 doesn't
//! have, so there is no semantic reasoning core to lift line-for-line.
//! What *is* portable is the **consumption contract** the cascade relies on
//! (V1 `cascade_prime_reasoning`): produce a `(conclusion_vector,
//! confidence)` from a query.
//!
//! V1's conclusion text embeds the query verbatim, so the cascade — which
//! averages the conclusion's content-word context vectors — effectively
//! blends the query's own content words scaled by a confidence number.
//! V2 reproduces that faithfully using only the lexicon:
//!
//! 1. Classify the query type from its first word ([`QueryType`]).
//! 2. For each content word (Function / Pronoun dropped), derive an
//!    evidence confidence from the lexicon signals V2 has
//!    (known? grounded? POS-confident? familiar?). Unknown → `0.15`
//!    (V1's "knowledge gap").
//! 3. Aggregate with V1's math: geometric mean (penalizes weak links) ×
//!    multi-source agreement factor `(1 − √variance)` × a small boost.
//! 4. Vectorize the conclusion = mean of the content words' context
//!    vectors (the cascade's own derivation, computed directly).

use serde::{Deserialize, Serialize};

use crate::engine::GroundedLanguage;
use crate::lexicon::WordClass;
use crate::text::tokenize;

/// Knowledge-gap confidence for an unknown content word (V1's 0.15).
const UNKNOWN_EVIDENCE: f32 = 0.15;

/// Query type, classified from the leading word (V1 `classify_query_type`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum QueryType {
    Factual,
    Causal,
    Procedural,
    Temporal,
    Spatial,
    Identity,
    Declarative,
}

impl QueryType {
    fn label(self) -> &'static str {
        match self {
            QueryType::Factual => "factual",
            QueryType::Causal => "causal",
            QueryType::Procedural => "procedural",
            QueryType::Temporal => "temporal",
            QueryType::Spatial => "spatial",
            QueryType::Identity => "identity",
            QueryType::Declarative => "declarative",
        }
    }
}

/// Classify a query by its first token (pure prefix match, V1-faithful).
#[must_use]
pub fn classify_query_type(query: &str) -> QueryType {
    let first = tokenize(query).into_iter().next().unwrap_or_default();
    match first.as_str() {
        "what" | "whats" => QueryType::Factual,
        "why" => QueryType::Causal,
        "how" => QueryType::Procedural,
        "when" => QueryType::Temporal,
        "where" => QueryType::Spatial,
        "which" | "who" | "whom" | "whose" => QueryType::Identity,
        _ => QueryType::Declarative,
    }
}

/// A reasoning conclusion.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReasoningConclusion {
    /// Templated conclusion text (query type + the query verbatim + a
    /// confidence label) — mirrors V1's `format_conclusion` shape.
    pub text: String,
    /// Overall confidence in `[0, 1]` (V1 geometric-mean aggregation).
    pub confidence: f32,
    /// Conclusion vector: mean of the content words' context vectors. This
    /// is the vector the cascade blends (V1 derives it by averaging the
    /// conclusion's content words; V2 computes it directly).
    pub vector: Vec<f32>,
    /// Classified query type.
    pub query_type: QueryType,
    /// Number of content words that contributed evidence.
    pub evidence_count: usize,
}

impl GroundedLanguage {
    /// Reason about `query`, producing a [`ReasoningConclusion`] from the
    /// lexicon (see module docs). Read-only; no discourse mutation.
    #[must_use]
    pub fn reason(&self, query: &str) -> ReasoningConclusion {
        let dim = self.lexicon.semantic_dim;
        let query_type = classify_query_type(query);
        let tokens = tokenize(query);

        let mut evidences: Vec<f32> = Vec::new();
        let mut vec_sum = vec![0.0_f32; dim];
        let mut vec_n = 0usize;

        for tok in &tokens {
            match self.lexicon.find(tok) {
                Some(idx) => {
                    let entry = self.lexicon.entry(idx);
                    // Drop closed-class words (same filter as produce/cascade).
                    if matches!(entry.learned_class, WordClass::Function | WordClass::Pronoun) {
                        continue;
                    }
                    evidences.push(word_evidence(
                        entry.context_initialized,
                        entry.class_confidence,
                        entry.frequency,
                    ));
                    if entry.context_initialized {
                        for (s, &c) in vec_sum.iter_mut().zip(entry.context_vector.iter()) {
                            *s += c;
                        }
                        vec_n += 1;
                    }
                }
                None => evidences.push(UNKNOWN_EVIDENCE),
            }
        }

        if vec_n > 0 {
            #[allow(clippy::cast_precision_loss)]
            let inv = 1.0 / vec_n as f32;
            for s in &mut vec_sum {
                *s *= inv;
            }
        }

        let confidence = aggregate_confidence(&evidences);
        let label = if confidence >= 0.66 {
            "high"
        } else if confidence >= 0.33 {
            "moderate"
        } else {
            "low"
        };
        let text = format!(
            "reasoning about {} query: \"{}\" — concluded with {} confidence",
            query_type.label(),
            query.trim(),
            label
        );

        ReasoningConclusion {
            text,
            confidence,
            vector: vec_sum,
            query_type,
            evidence_count: evidences.len(),
        }
    }
}

/// Per-word evidence confidence from V2 lexicon signals. Known + grounded
/// + POS-confident + familiar → high; bare-known → modest.
fn word_evidence(context_initialized: bool, class_confidence: f32, frequency: u32) -> f32 {
    let mut e = 0.4; // known baseline
    if context_initialized {
        e += 0.3;
    }
    e += 0.3 * class_confidence.clamp(0.0, 1.0);
    // Familiarity: more exposures → more confident, floored so a
    // just-seen word still counts.
    #[allow(clippy::cast_precision_loss)]
    let familiarity = (1.0 - (-(frequency as f32) / 20.0).exp()).max(0.2);
    (e * familiarity).clamp(0.0, 1.0)
}

/// Aggregate evidence confidences (V1 `phase_inference` + synthesis):
/// 0 → 0.15 ("I don't know"); 1 → `e·0.8`; ≥2 → geometric-mean ×
/// agreement `(1−√variance)` × small multi-source boost, clamped `[0,1]`.
fn aggregate_confidence(evidences: &[f32]) -> f32 {
    match evidences.len() {
        0 => UNKNOWN_EVIDENCE,
        1 => evidences[0] * 0.8,
        n => {
            // Geometric mean in log-domain (penalizes weak links).
            let log_sum: f32 = evidences.iter().map(|&e| e.max(0.001).ln()).sum();
            #[allow(clippy::cast_precision_loss)]
            let gm = (log_sum / n as f32).exp();
            // Agreement: low variance → sources concur.
            #[allow(clippy::cast_precision_loss)]
            let mean = evidences.iter().sum::<f32>() / n as f32;
            #[allow(clippy::cast_precision_loss)]
            let var = evidences.iter().map(|&e| (e - mean).powi(2)).sum::<f32>() / n as f32;
            let agreement = (1.0 - var.sqrt()).clamp(0.0, 1.0);
            #[allow(clippy::cast_precision_loss)]
            let boost = (1.0 + 0.05 * (n - 1) as f32).min(1.2);
            (gm * agreement * boost).clamp(0.0, 1.0)
        }
    }
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
        for _ in 0..30 {
            gl.learn_from_text("the dog and the cat run");
        }
        gl
    }

    #[test]
    fn classifies_query_types() {
        assert_eq!(classify_query_type("what is a dog"), QueryType::Factual);
        assert_eq!(classify_query_type("why does it run"), QueryType::Causal);
        assert_eq!(classify_query_type("how do cats hunt"), QueryType::Procedural);
        assert_eq!(classify_query_type("when did it happen"), QueryType::Temporal);
        assert_eq!(classify_query_type("where is the cat"), QueryType::Spatial);
        assert_eq!(classify_query_type("who let the dog out"), QueryType::Identity);
        assert_eq!(classify_query_type("the dog runs"), QueryType::Declarative);
        assert_eq!(classify_query_type(""), QueryType::Declarative);
    }

    #[test]
    fn known_words_yield_higher_confidence_than_unknown() {
        let gl = engine();
        let known = gl.reason("dog cat run");
        let unknown = gl.reason("xyzzy plugh frobnozz");
        assert!(
            known.confidence > unknown.confidence,
            "known {} should beat unknown {}",
            known.confidence,
            unknown.confidence
        );
        // A single unknown word is the bare knowledge-gap (× the 1-source
        // 0.8 factor); multiple agreeing gaps get a small boost, but never
        // approach a known answer.
        let single_unknown = gl.reason("xyzzy");
        assert!(single_unknown.confidence <= UNKNOWN_EVIDENCE + 1e-6);
        assert!(unknown.confidence < 0.25, "multi-gap stays low: {}", unknown.confidence);
    }

    #[test]
    fn conclusion_vector_is_mean_of_known_words() {
        let gl = engine();
        let c = gl.reason("dog cat");
        // dog≈[1,0,..], cat≈[0,1,..] after learning shifts them, but the
        // mean is a real averaged vector of the right dim, non-zero.
        assert_eq!(c.vector.len(), 8);
        assert!(c.vector.iter().any(|&x| x != 0.0));
        assert_eq!(c.evidence_count, 2);
    }

    #[test]
    fn empty_query_is_knowledge_gap() {
        let gl = engine();
        let c = gl.reason("");
        assert_eq!(c.confidence, UNKNOWN_EVIDENCE);
        assert!(c.vector.iter().all(|&x| x == 0.0));
        assert_eq!(c.evidence_count, 0);
    }

    #[test]
    fn conclusion_text_embeds_query_and_type() {
        let gl = engine();
        let c = gl.reason("what is a dog");
        assert!(c.text.contains("what is a dog"));
        assert!(c.text.contains("factual"));
        assert_eq!(c.query_type, QueryType::Factual);
    }

    #[test]
    fn confidence_bounded() {
        let gl = engine();
        for q in ["dog", "dog cat run", "what is a cat", "", "unknownword"] {
            let c = gl.reason(q);
            assert!((0.0..=1.0).contains(&c.confidence), "{q}: {}", c.confidence);
        }
    }

    #[test]
    fn deterministic() {
        let gl = engine();
        let a = gl.reason("dog cat run");
        let b = gl.reason("dog cat run");
        assert_eq!(a.confidence, b.confidence);
        assert_eq!(a.vector, b.vector);
    }
}

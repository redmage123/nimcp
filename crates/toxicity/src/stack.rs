//! [`ToxicityStack`] — the three engines composed into one gate, the
//! shape the brain wires in. Mirrors V1's `brain->toxicity_*` trio
//! (`nimcp_brain_init_safety_verify.c`).
//!
//! - `classify` runs the pattern classifier and the ML head and
//!   **max-merges** their `(harm, fairness)` (V1 ensemble): the gate is
//!   as strict as the stricter of the two.
//! - `counterclaim` delegates to the [`CounterclaimEngine`].
//!
//! The regex `PatternClassifier` and the `CounterclaimEngine` are rebuilt
//! from the bundled TSVs on construction (their content is static); only
//! the [`MlClassifier`] weights need persisting, via [`ToxicityStack::
//! ml_to_json`] / [`ToxicityStack::restore_ml_json`].

use crate::counterclaim::{CounterclaimEngine, CounterclaimResult};
use crate::ml::MlClassifier;
use crate::pattern::{PatternClassifier, RulesError, ToxicityResult};

/// The composed content-safety stack.
#[derive(Debug, Clone)]
pub struct ToxicityStack {
    pub pattern: PatternClassifier,
    pub ml: MlClassifier,
    pub counter: CounterclaimEngine,
}

impl ToxicityStack {
    /// Build from the bundled default rules/templates/anti-frames, with a
    /// fresh ML head seeded by `ml_seed`.
    pub fn with_defaults(ml_seed: u64) -> Result<Self, RulesError> {
        Ok(Self {
            pattern: PatternClassifier::with_default_rules()?,
            ml: MlClassifier::new(ml_seed),
            counter: CounterclaimEngine::with_defaults(),
        })
    }

    /// Classify `text`, max-merging the pattern + ML verdicts. When only
    /// the ML head crosses the threshold, the category is `"ml_classifier"`.
    #[must_use]
    pub fn classify(&self, text: &str) -> ToxicityResult {
        let mut r = self.pattern.classify(text);
        let ml = self.ml.predict(text);
        let pattern_harm = r.predicted_harm;
        r.predicted_harm = r.predicted_harm.max(ml.predicted_harm);
        r.fairness_violation = r.fairness_violation.max(ml.fairness_violation);
        r.max_score = r.predicted_harm.max(r.fairness_violation);
        let threshold = self.pattern.threshold();
        // Category attribution: if the ML head is what pushed it over and
        // the patterns alone wouldn't have flagged, label it.
        if r.max_score >= threshold && pattern_harm < threshold && r.matched_category.is_empty() {
            r.matched_category = "ml_classifier".to_string();
        }
        r.would_block = r.max_score >= threshold;
        r
    }

    /// Generate a counterclaim for blocked content.
    #[must_use]
    pub fn counterclaim(&self, toxic_text: &str, category: &str, stage: i32) -> CounterclaimResult {
        self.counter.generate(toxic_text, category, stage)
    }

    /// One mark-not-filter training step for the ML head against the
    /// pattern classifier's verdict (the teacher). Returns the pre-step
    /// MSE. Never mutates inputs / drops data.
    pub fn train_ml_from_pattern(&mut self, text: &str, lr: f32, dead_zone: f32) -> f32 {
        let teacher = self.pattern.classify(text);
        self.ml
            .train_step(text, teacher.predicted_harm, teacher.fairness_violation, lr, dead_zone)
    }

    /// Serialize just the ML weights (the only learned state).
    pub fn ml_to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string(&self.ml)
    }

    /// Restore ML weights from JSON (pattern + counterclaim are static).
    pub fn restore_ml_json(&mut self, json: &str) -> Result<(), serde_json::Error> {
        self.ml = serde_json::from_str(json)?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_build() {
        let s = ToxicityStack::with_defaults(1).unwrap();
        assert!(s.pattern.rule_count() >= 10);
        assert!(s.counter.template_count() >= 8);
    }

    #[test]
    fn pattern_match_blocks_even_with_untrained_ml() {
        let s = ToxicityStack::with_defaults(1).unwrap();
        let r = s.classify("kill all immigrants");
        assert!(r.would_block, "pattern alone must block");
        assert!(r.predicted_harm >= 0.9);
    }

    #[test]
    fn benign_passes() {
        let s = ToxicityStack::with_defaults(1).unwrap();
        let r = s.classify("the weather is lovely and the dog is happy");
        assert!(!r.would_block);
    }

    #[test]
    fn counterclaim_for_blocked() {
        let s = ToxicityStack::with_defaults(1).unwrap();
        let r = s.classify("muslims are subhuman");
        assert!(r.would_block);
        let cc = s.counterclaim("muslims are subhuman", &r.matched_category, 2);
        assert!(!cc.text.is_empty());
        assert!(cc.text.contains("muslims") || cc.source == "antiframe");
    }

    #[test]
    fn ml_weights_round_trip() {
        let mut s = ToxicityStack::with_defaults(2).unwrap();
        for _ in 0..30 {
            s.train_ml_from_pattern("kill all immigrants", 0.05, 0.0);
        }
        let json = s.ml_to_json().unwrap();
        let before = s.ml.predict("kill all immigrants").predicted_harm;
        let mut s2 = ToxicityStack::with_defaults(999).unwrap();
        s2.restore_ml_json(&json).unwrap();
        let after = s2.ml.predict("kill all immigrants").predicted_harm;
        assert!((before - after).abs() < 1e-6);
    }
}

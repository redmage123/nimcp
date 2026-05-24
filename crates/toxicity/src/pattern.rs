//! Regex pattern classifier — port of V1's `nimcp_toxicity.c`.
//!
//! Rules come from a TSV (`category \t harm_weight \t fairness_weight \t
//! regex \t description`). The `allowlist` category marks *anti-toxic*
//! constructions ("X are not subhuman"); its matches **suppress
//! overlapping** toxic matches but do not clear the whole verdict.
//!
//! # classify algorithm (V1, with the 2026-05-20 span-suppress fix)
//!
//! 1. Collect every allowlist match span; set `anti_toxic_signal = 1.0`
//!    if any matched.
//! 2. For each toxic rule, for each match: if the match is fully inside
//!    an allowlist span, skip it (disclaimer-covered); otherwise take the
//!    running `max` of `harm_weight` / `fairness_weight` and remember the
//!    best-scoring category.
//! 3. `max_score = max(harm, fairness)`; `would_block = max_score ≥
//!    threshold`. `would_block` is a hint, never a delete order.

use regex::Regex;
use serde::{Deserialize, Serialize};

use crate::DEFAULT_THRESHOLD;

/// Pseudo-regex marker: a rule whose pattern is
/// `__LOAD_FROM_FILE__<path>` sources extra patterns from an external
/// file (V1 slurs list). Skipped when the file is absent.
const LOAD_FROM_FILE_MARKER: &str = "__LOAD_FROM_FILE__";

/// Default bundled rule set (carried verbatim from V1 `data/safety/`).
const DEFAULT_RULES_TSV: &str = include_str!("../data/toxicity_rules.tsv");

/// One compiled pattern rule.
#[derive(Debug, Clone)]
pub struct PatternRule {
    pub category: String,
    pub harm_weight: f32,
    pub fairness_weight: f32,
    pub regex: Regex,
    pub description: String,
}

impl PatternRule {
    fn is_allowlist(&self) -> bool {
        self.category == "allowlist"
    }
}

/// Per-call classification result (V1 `toxicity_result_t`).
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct ToxicityResult {
    /// Max `fairness_weight` over matched toxic rules.
    pub fairness_violation: f32,
    /// Max `harm_weight` over matched toxic rules.
    pub predicted_harm: f32,
    /// `max(predicted_harm, fairness_violation)`.
    pub max_score: f32,
    /// `1.0` if any allowlist pattern matched (independent signal).
    pub anti_toxic_signal: f32,
    /// Category of the highest-scoring matched toxic rule.
    pub matched_category: String,
    /// Number of (non-suppressed) toxic matches.
    pub num_matches: u32,
    /// Hint: `max_score ≥ threshold`. NOT a delete instruction.
    pub would_block: bool,
}

/// Compiled rule set + block threshold.
#[derive(Debug, Clone)]
pub struct PatternClassifier {
    rules: Vec<PatternRule>,
    threshold: f32,
}

/// Loading error.
#[derive(Debug, thiserror::Error)]
pub enum RulesError {
    #[error("toxicity rules: regex compile error on line {line}: {source}")]
    Regex {
        line: usize,
        #[source]
        source: regex::Error,
    },
}

impl PatternClassifier {
    /// Build from the bundled default rule set.
    pub fn with_default_rules() -> Result<Self, RulesError> {
        Self::from_tsv(DEFAULT_RULES_TSV)
    }

    /// Build from a TSV string. Comment (`#`) and blank lines are skipped;
    /// `__LOAD_FROM_FILE__` marker rows are skipped (no external file).
    /// Patterns are compiled case-insensitively (V1 `REG_ICASE`).
    pub fn from_tsv(tsv: &str) -> Result<Self, RulesError> {
        let mut rules = Vec::new();
        for (i, line) in tsv.lines().enumerate() {
            let line = line.trim_end();
            if line.is_empty() || line.starts_with('#') {
                continue;
            }
            let cols: Vec<&str> = line.split('\t').collect();
            if cols.len() < 4 {
                continue; // malformed → skip (defensive, like V1)
            }
            let category = cols[0].trim().to_string();
            let harm_weight = cols[1].trim().parse().unwrap_or(0.0);
            let fairness_weight = cols[2].trim().parse().unwrap_or(0.0);
            let pat = cols[3].trim();
            if pat.starts_with(LOAD_FROM_FILE_MARKER) {
                continue; // external slurs file not shipped
            }
            let description = cols.get(4).map(|s| s.trim().to_string()).unwrap_or_default();
            let regex = regex::RegexBuilder::new(pat)
                .case_insensitive(true)
                .build()
                .map_err(|e| RulesError::Regex { line: i + 1, source: e })?;
            rules.push(PatternRule {
                category,
                harm_weight,
                fairness_weight,
                regex,
                description,
            });
        }
        Ok(Self {
            rules,
            threshold: DEFAULT_THRESHOLD,
        })
    }

    /// Number of compiled rules.
    #[must_use]
    pub fn rule_count(&self) -> usize {
        self.rules.len()
    }

    /// Current block threshold.
    #[must_use]
    pub fn threshold(&self) -> f32 {
        self.threshold
    }

    /// Set the block threshold (clamped to `[0, 1]`).
    pub fn set_threshold(&mut self, t: f32) {
        self.threshold = t.clamp(0.0, 1.0);
    }

    /// Classify `text` (see module docs).
    #[must_use]
    pub fn classify(&self, text: &str) -> ToxicityResult {
        let mut result = ToxicityResult::default();
        if text.is_empty() {
            return result;
        }

        // Pass 1 — collect allowlist spans.
        let mut allow_spans: Vec<(usize, usize)> = Vec::new();
        for rule in self.rules.iter().filter(|r| r.is_allowlist()) {
            for m in rule.regex.find_iter(text) {
                result.anti_toxic_signal = 1.0;
                allow_spans.push((m.start(), m.end()));
            }
        }

        // Pass 2 — toxic rules with span suppression.
        let mut best_score = 0.0_f32;
        for rule in self.rules.iter().filter(|r| !r.is_allowlist()) {
            for m in rule.regex.find_iter(text) {
                let (s, e) = (m.start(), m.end());
                // Suppress if fully inside an allowlist span.
                if allow_spans.iter().any(|&(as_, ae)| s >= as_ && e <= ae) {
                    continue;
                }
                result.num_matches += 1;
                if rule.harm_weight > result.predicted_harm {
                    result.predicted_harm = rule.harm_weight;
                }
                if rule.fairness_weight > result.fairness_violation {
                    result.fairness_violation = rule.fairness_weight;
                }
                let rule_score = rule.harm_weight.max(rule.fairness_weight);
                if rule_score > best_score {
                    best_score = rule_score;
                    result.matched_category = rule.category.clone();
                }
            }
        }

        result.max_score = result.predicted_harm.max(result.fairness_violation);
        result.would_block = result.max_score >= self.threshold;
        result
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn default_rules_compile() {
        let c = PatternClassifier::with_default_rules().unwrap();
        assert!(c.rule_count() >= 10, "expected the bundled rule set");
    }

    #[test]
    fn benign_text_is_clean() {
        let c = PatternClassifier::with_default_rules().unwrap();
        let r = c.classify("the dog runs in the park and it is a nice day");
        assert_eq!(r.num_matches, 0);
        assert_eq!(r.max_score, 0.0);
        assert!(!r.would_block);
    }

    #[test]
    fn dehumanization_is_flagged() {
        let c = PatternClassifier::with_default_rules().unwrap();
        let r = c.classify("muslims are subhuman");
        assert!(r.predicted_harm >= 0.9, "harm {}", r.predicted_harm);
        assert!(r.would_block);
        assert_eq!(r.matched_category, "dehumanization");
    }

    #[test]
    fn violence_call_is_flagged() {
        let c = PatternClassifier::with_default_rules().unwrap();
        let r = c.classify("kill all immigrants");
        assert!(r.would_block);
        assert!(r.predicted_harm >= 0.9);
    }

    #[test]
    fn allowlist_protects_anti_toxic_speech() {
        let c = PatternClassifier::with_default_rules().unwrap();
        // Anti-dehumanization construction: must NOT block, anti-toxic set.
        let r = c.classify("muslims are not subhuman");
        assert_eq!(r.anti_toxic_signal, 1.0);
        assert!(!r.would_block, "anti-toxic speech must not be blocked");
    }

    #[test]
    fn allowlist_suppresses_only_overlapping_clause() {
        // The 2026-05-20 fix: an allowlist disclaimer must NOT clear a
        // separate toxic clause later in the same input.
        let c = PatternClassifier::with_default_rules().unwrap();
        let r = c.classify("jews are not subhuman. kill all muslims.");
        assert_eq!(r.anti_toxic_signal, 1.0, "the disclaimer matched");
        assert!(
            r.would_block,
            "the second clause is a real threat and must still flag"
        );
    }

    #[test]
    fn threshold_is_settable_and_clamped() {
        let mut c = PatternClassifier::with_default_rules().unwrap();
        c.set_threshold(2.0);
        assert_eq!(c.threshold(), 1.0);
        c.set_threshold(-1.0);
        assert_eq!(c.threshold(), 0.0);
    }

    #[test]
    fn from_tsv_skips_comments_and_load_marker() {
        let tsv = "# comment\n\nallowlist\t0\t0\t\\bfoo\\b\tdesc\n\
                   toxic_generic\t0.8\t0.5\t\\bbar\\b\tdesc\n\
                   slurs_external\t0.9\t0.9\t__LOAD_FROM_FILE__/nope.txt\tmarker";
        let c = PatternClassifier::from_tsv(tsv).unwrap();
        assert_eq!(c.rule_count(), 2, "marker + comments skipped");
        let r = c.classify("bar");
        assert!((r.predicted_harm - 0.8).abs() < 1e-6);
        let clean = c.classify("foo");
        assert_eq!(clean.anti_toxic_signal, 1.0);
    }
}

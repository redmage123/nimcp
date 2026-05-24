//! Counterclaim generator — port of V1's `nimcp_toxicity_response.c`.
//!
//! Two tiers:
//! 1. **Template** (`category \t stage \t template \t notes`): pick the
//!    highest-stage row matching the toxic category (or the `*` wildcard)
//!    with `stage ≤ current_stage`; a non-wildcard beats a wildcard at the
//!    same stage. `{group}` is filled from the toxic text.
//! 2. **Anti-frame fallback** (`toxic_word \t counter_word`): when no
//!    template matches, swap toxic words for counter words (multi-word
//!    phrases first, then single words). Produces awkward-but-safe text.
//!
//! Stage-graded by design: stage 0 → holophrastic ("no"); stage 3 →
//! articulated refusal with reasoning.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

const DEFAULT_COUNTERCLAIMS_TSV: &str = include_str!("../data/toxicity_counterclaims.tsv");
const DEFAULT_ANTIFRAMES_TSV: &str = include_str!("../data/toxicity_antiframes.tsv");

/// Demographic group terms scanned to fill the `{group}` placeholder.
const GROUPS: &[&str] = &[
    "jews", "muslims", "christians", "hindus", "buddhists", "catholics", "blacks", "whites",
    "asians", "latinos", "hispanics", "arabs", "africans", "mexicans", "immigrants", "refugees",
    "women", "men", "girls", "gays", "lesbians", "trans", "queers", "disabled", "elderly",
];

/// A counterclaim template row.
#[derive(Debug, Clone, Serialize, Deserialize)]
struct Template {
    category: String,
    stage: i32,
    text: String,
}

/// Result of generating a counterclaim.
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct CounterclaimResult {
    /// The counterclaim text (empty if nothing matched).
    pub text: String,
    /// `"template"`, `"antiframe"`, or `""`.
    pub source: String,
    /// Stage of the matched template (`-1` if none).
    pub stage_matched: i32,
    /// Number of anti-frame word swaps applied.
    pub antiframe_swaps: u32,
}

/// Template + anti-frame engine.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CounterclaimEngine {
    templates: Vec<Template>,
    /// Single-word swaps (lowercased toxic → counter).
    single: HashMap<String, String>,
    /// Multi-word swaps (lowercased phrase → counter), longest-first.
    multi: Vec<(String, String)>,
}

/// Strip one layer of surrounding double quotes (V1 quotes multi-word
/// TSV fields).
fn unquote(s: &str) -> &str {
    let s = s.trim();
    s.strip_prefix('"').and_then(|x| x.strip_suffix('"')).unwrap_or(s)
}

impl CounterclaimEngine {
    /// Build from the bundled default counterclaim + anti-frame tables.
    #[must_use]
    pub fn with_defaults() -> Self {
        Self::from_tsv(DEFAULT_COUNTERCLAIMS_TSV, DEFAULT_ANTIFRAMES_TSV)
    }

    /// Build from counterclaim + anti-frame TSV strings.
    #[must_use]
    pub fn from_tsv(counterclaims: &str, antiframes: &str) -> Self {
        let mut templates = Vec::new();
        for line in counterclaims.lines() {
            let line = line.trim_end();
            if line.is_empty() || line.starts_with('#') {
                continue;
            }
            let c: Vec<&str> = line.split('\t').collect();
            if c.len() < 3 {
                continue;
            }
            let Ok(stage) = c[1].trim().parse::<i32>() else {
                continue;
            };
            templates.push(Template {
                category: c[0].trim().to_string(),
                stage,
                text: unquote(c[2]).to_string(),
            });
        }

        let mut single = HashMap::new();
        let mut multi = Vec::new();
        for line in antiframes.lines() {
            let line = line.trim_end();
            if line.is_empty() || line.starts_with('#') {
                continue;
            }
            let c: Vec<&str> = line.split('\t').collect();
            if c.len() < 2 {
                continue;
            }
            let toxic = unquote(c[0]).to_ascii_lowercase();
            let counter = unquote(c[1]).to_string();
            if toxic.contains(' ') {
                multi.push((toxic, counter));
            } else {
                single.insert(toxic, counter);
            }
        }
        // Longest phrase first so "a man's property" beats "property".
        multi.sort_by_key(|(phrase, _)| std::cmp::Reverse(phrase.len()));

        Self { templates, single, multi }
    }

    /// Number of templates / anti-frame swaps loaded.
    #[must_use]
    pub fn template_count(&self) -> usize {
        self.templates.len()
    }
    #[must_use]
    pub fn antiframe_count(&self) -> usize {
        self.single.len() + self.multi.len()
    }

    /// Find the best template for `category` at `current_stage`: highest
    /// stage `≤ current_stage`, non-wildcard beats wildcard on ties.
    fn best_template(&self, category: &str, current_stage: i32) -> Option<&Template> {
        let mut best: Option<&Template> = None;
        for t in &self.templates {
            let matches = t.category == category || t.category == "*";
            if !matches || t.stage > current_stage {
                continue;
            }
            best = match best {
                None => Some(t),
                Some(b) => {
                    let better_stage = t.stage > b.stage;
                    let tie_nonwild = t.stage == b.stage && b.category == "*" && t.category != "*";
                    if better_stage || tie_nonwild {
                        Some(t)
                    } else {
                        Some(b)
                    }
                }
            };
        }
        best
    }

    /// Extract the first demographic group mentioned in `text`, for the
    /// `{group}` placeholder. Defaults to `"people"`.
    fn extract_group(text: &str) -> String {
        let lower = text.to_ascii_lowercase();
        for g in GROUPS {
            // Whole-word-ish: bounded by non-alphanumerics.
            if lower.split(|c: char| !c.is_ascii_alphanumeric()).any(|w| w == *g) {
                return (*g).to_string();
            }
        }
        "people".to_string()
    }

    /// Anti-frame swap fallback. Multi-word phrases first (substring,
    /// case-insensitive), then single words (token-level). Returns the
    /// rewritten text + swap count.
    fn antiframe_swap(&self, text: &str) -> (String, u32) {
        let mut swaps = 0u32;
        let mut s = text.to_string();
        for (toxic, counter) in &self.multi {
            while let Some(pos) = s.to_ascii_lowercase().find(toxic) {
                s.replace_range(pos..pos + toxic.len(), counter);
                swaps += 1;
            }
        }
        // Single-word: token-level, preserving leading/trailing punctuation.
        let rebuilt: Vec<String> = s
            .split_whitespace()
            .map(|tok| {
                let core: String = tok.chars().filter(|c| c.is_ascii_alphanumeric()).collect();
                if let Some(rep) = self.single.get(&core.to_ascii_lowercase()) {
                    swaps += 1;
                    tok.replace(core.as_str(), rep)
                } else {
                    tok.to_string()
                }
            })
            .collect();
        (rebuilt.join(" "), swaps)
    }

    /// Generate a counterclaim against `toxic_text` for `matched_category`
    /// at `current_stage` (template tier first, then anti-frame fallback).
    #[must_use]
    pub fn generate(&self, toxic_text: &str, matched_category: &str, current_stage: i32) -> CounterclaimResult {
        if let Some(t) = self.best_template(matched_category, current_stage) {
            let text = t.text.replace("{group}", &Self::extract_group(toxic_text));
            return CounterclaimResult {
                text,
                source: "template".to_string(),
                stage_matched: t.stage,
                antiframe_swaps: 0,
            };
        }
        let (text, swaps) = self.antiframe_swap(toxic_text);
        if swaps > 0 {
            return CounterclaimResult {
                text,
                source: "antiframe".to_string(),
                stage_matched: -1,
                antiframe_swaps: swaps,
            };
        }
        CounterclaimResult::default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_load() {
        let e = CounterclaimEngine::with_defaults();
        assert!(e.template_count() >= 8);
        assert!(e.antiframe_count() >= 50);
    }

    #[test]
    fn stage0_is_holophrastic_refusal() {
        let e = CounterclaimEngine::with_defaults();
        let r = e.generate("muslims are subhuman", "dehumanization", 0);
        assert_eq!(r.source, "template");
        assert_eq!(r.stage_matched, 0);
        assert_eq!(r.text.to_lowercase(), "no");
    }

    #[test]
    fn higher_stage_picks_richer_template_and_fills_group() {
        let e = CounterclaimEngine::with_defaults();
        let r = e.generate("muslims are subhuman", "dehumanization", 2);
        assert_eq!(r.source, "template");
        assert_eq!(r.stage_matched, 2);
        // stage-2 dehumanization template is "{group} are people too".
        assert!(r.text.contains("muslims"), "group filled: {}", r.text);
        assert!(!r.text.contains("{group}"), "placeholder substituted");
    }

    #[test]
    fn caps_at_current_stage() {
        let e = CounterclaimEngine::with_defaults();
        // current_stage 1 → cannot use stage-2/3 templates.
        let r = e.generate("kill all jews", "violence_against_group", 1);
        assert!(r.stage_matched <= 1);
        assert_eq!(r.source, "template");
    }

    #[test]
    fn wildcard_fallback_when_no_category_template() {
        let e = CounterclaimEngine::with_defaults();
        // A category with no specific rows should fall to "*" if present.
        let r = e.generate("some toxic thing", "toxic_generic", 3);
        // Either a toxic_generic row or the "*" wildcard; must produce text.
        assert!(!r.text.is_empty());
    }

    #[test]
    fn antiframe_fallback_when_no_template() {
        // Engine with NO matching templates → forces anti-frame path.
        let e = CounterclaimEngine::from_tsv("", DEFAULT_ANTIFRAMES_TSV);
        let r = e.generate("they are vermin and subhuman", "dehumanization", 3);
        assert_eq!(r.source, "antiframe");
        assert!(r.antiframe_swaps >= 2, "swaps: {}", r.antiframe_swaps);
        assert!(r.text.contains("people"), "vermin→people: {}", r.text);
        assert!(r.text.contains("human"), "subhuman→human: {}", r.text);
    }

    #[test]
    fn multiword_antiframe_applied_before_single() {
        let e = CounterclaimEngine::from_tsv("", DEFAULT_ANTIFRAMES_TSV);
        let r = e.generate("round up the immigrants", "x", 3);
        assert_eq!(r.source, "antiframe");
        // "round up" → "welcome" (multi-word swap).
        assert!(r.text.to_lowercase().contains("welcome"), "got: {}", r.text);
    }

    #[test]
    fn no_match_yields_empty() {
        let e = CounterclaimEngine::from_tsv("", "");
        let r = e.generate("perfectly benign sentence", "none", 3);
        assert_eq!(r.source, "");
        assert!(r.text.is_empty());
    }

    #[test]
    fn serde_round_trip() {
        let e = CounterclaimEngine::with_defaults();
        let json = serde_json::to_string(&e).unwrap();
        let back: CounterclaimEngine = serde_json::from_str(&json).unwrap();
        assert_eq!(back.template_count(), e.template_count());
        assert_eq!(back.antiframe_count(), e.antiframe_count());
    }
}

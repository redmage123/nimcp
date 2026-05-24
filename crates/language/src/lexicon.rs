//! The lexicon — word forms, their Hebbian bindings to [`ConceptId`]s,
//! and per-word distributional context vectors. Port of V1's
//! `gl_lexicon_entry_t` + `lexicon_*` functions in `grounded_language.c`.
//!
//! # Model
//!
//! - A [`LexiconEntry`] is one word form. It owns a growable list of
//!   [`WordBinding`]s (word → concept, with per-modality strengths) plus
//!   a `context_vector` (the distributional embedding, learned in phase
//!   L2) and affect (`valence`, `arousal`) + an inferred [`WordClass`].
//! - [`Lexicon`] stores entries in a dense `Vec` (stable order → simple
//!   deterministic persistence) with a `HashMap` form→index for lookup.
//!   This replaces V1's hand-rolled open-addressing table; the FNV hash
//!   is still available via [`crate::fnv1a_lower`] for fingerprint parity.
//!
//! # Hebbian binding update (V1 `lexicon_bind`)
//!
//! On re-exposure of an existing `(word, concept)` pair:
//! `delta = lr·(1 − strength)·input;  strength ← min(1, strength + delta)`.
//! Per-modality strength accumulates `lr·input`. `confidence = 1 −
//! exp(−exposure / 5)` so it saturates toward 1 with repeated grounding.

use serde::{Deserialize, Serialize};

use crate::concept::ConceptId;
use crate::{HEBBIAN_LR_DEFAULT, fnv1a_lower};

/// Number of sensory modalities (V1 `GL_MODALITY_COUNT`).
pub const MODALITY_COUNT: usize = 6;

/// Sensory modality a grounding came through (V1 `gl_modality_t`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Modality {
    Visual = 0,
    Auditory = 1,
    Motor = 2,
    Emotional = 3,
    Spatial = 4,
    /// Cross-linguistic (word-to-word) grounding.
    Linguistic = 5,
}

/// Inferred part-of-speech class (V1 `gl_word_class_t`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
pub enum WordClass {
    #[default]
    Unknown = 0,
    Noun = 1,
    Verb = 2,
    Adjective = 3,
    Adverb = 4,
    /// Determiners, prepositions, conjunctions.
    Function = 5,
    /// Reference words.
    Pronoun = 6,
}

/// One word → concept binding with per-modality grounding strengths.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WordBinding {
    pub concept_id: ConceptId,
    /// Overall binding strength in `[0, 1]`.
    pub strength: f32,
    /// Per-modality grounding strength.
    pub modality_strength: [f32; MODALITY_COUNT],
    pub exposure_count: u32,
    /// `1 − exp(−exposure / 5)` — saturates toward 1.
    pub confidence: f32,
}

impl WordBinding {
    fn new(concept_id: ConceptId, input_strength: f32, modality: Modality) -> Self {
        let mut modality_strength = [0.0_f32; MODALITY_COUNT];
        modality_strength[modality as usize] = input_strength.clamp(0.0, 1.0);
        Self {
            concept_id,
            strength: input_strength.clamp(0.0, 1.0),
            modality_strength,
            exposure_count: 1,
            confidence: confidence_for(1),
        }
    }
}

/// `confidence = 1 − exp(−exposure / 5)` (V1).
fn confidence_for(exposure: u32) -> f32 {
    #[allow(clippy::cast_precision_loss)]
    let e = exposure as f32;
    1.0 - (-e / 5.0).exp()
}

/// One word form and everything learned about it.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LexiconEntry {
    /// Lowercased surface form.
    pub form: String,
    /// FNV-1a of the form (fingerprint parity with V1).
    pub form_hash: u32,
    pub bindings: Vec<WordBinding>,
    /// How often the form has been seen in text.
    pub frequency: u32,
    pub learned_class: WordClass,
    /// Confidence in `learned_class` (`[0, 1]`).
    pub class_confidence: f32,
    /// Distributional embedding (length = lexicon `semantic_dim`).
    pub context_vector: Vec<f32>,
    /// Whether `context_vector` has been seeded (phase L2).
    pub context_initialized: bool,
    /// Affective valence `[-1, 1]`.
    pub valence: f32,
    /// Affective arousal `[0, 1]`.
    pub arousal: f32,
}

impl LexiconEntry {
    fn new(form: String, semantic_dim: usize) -> Self {
        let form_hash = fnv1a_lower(&form);
        Self {
            form,
            form_hash,
            bindings: Vec::new(),
            frequency: 0,
            learned_class: WordClass::Unknown,
            class_confidence: 0.0,
            context_vector: vec![0.0; semantic_dim],
            context_initialized: false,
            valence: 0.0,
            arousal: 0.0,
        }
    }

    /// Strongest binding (highest `strength`), if any.
    #[must_use]
    pub fn best_binding(&self) -> Option<&WordBinding> {
        self.bindings
            .iter()
            .max_by(|a, b| a.strength.total_cmp(&b.strength))
    }
}

/// The lexicon: dense entries + form→index map.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Lexicon {
    entries: Vec<LexiconEntry>,
    /// Lowercased form → index into `entries`. Skipped in serde and
    /// rebuilt on load (it's a derived view of `entries`).
    #[serde(skip)]
    index: std::collections::HashMap<String, usize>,
    pub semantic_dim: usize,
    pub hebbian_lr: f32,
}

impl Lexicon {
    /// New empty lexicon with the given embedding width.
    #[must_use]
    pub fn new(semantic_dim: usize) -> Self {
        Self {
            entries: Vec::new(),
            index: std::collections::HashMap::new(),
            semantic_dim,
            hebbian_lr: HEBBIAN_LR_DEFAULT,
        }
    }

    /// Rebuild the form→index map from `entries` — call after a serde
    /// load (the map is `#[serde(skip)]`).
    pub fn reindex(&mut self) {
        self.index.clear();
        for (i, e) in self.entries.iter().enumerate() {
            self.index.insert(e.form.clone(), i);
        }
    }

    /// Look up an entry index by form (case-insensitive).
    #[must_use]
    pub fn find(&self, word: &str) -> Option<usize> {
        self.index.get(&word.to_ascii_lowercase()).copied()
    }

    /// True iff the form is in the lexicon.
    #[must_use]
    pub fn has_word(&self, word: &str) -> bool {
        self.find(word).is_some()
    }

    /// Get the entry index for a form, creating a zeroed entry if absent.
    pub fn find_or_create(&mut self, word: &str) -> usize {
        let key = word.to_ascii_lowercase();
        if let Some(&i) = self.index.get(&key) {
            return i;
        }
        let i = self.entries.len();
        self.entries
            .push(LexiconEntry::new(key.clone(), self.semantic_dim));
        self.index.insert(key, i);
        i
    }

    /// Find-or-create and bump the frequency counter (saturating).
    pub fn record_word(&mut self, word: &str) -> usize {
        let i = self.find_or_create(word);
        self.entries[i].frequency = self.entries[i].frequency.saturating_add(1);
        i
    }

    #[must_use]
    pub fn entry(&self, idx: usize) -> &LexiconEntry {
        &self.entries[idx]
    }

    pub fn entry_mut(&mut self, idx: usize) -> &mut LexiconEntry {
        &mut self.entries[idx]
    }

    #[must_use]
    pub fn entries(&self) -> &[LexiconEntry] {
        &self.entries
    }

    #[must_use]
    pub fn vocab_count(&self) -> usize {
        self.entries.len()
    }

    /// Hebbian bind: strengthen (or create) the `(word, concept)` link.
    ///
    /// Existing binding: `delta = lr·(1 − strength)·input`,
    /// `strength ← min(1, strength + delta)`, `modality_strength[m] +=
    /// lr·input`, `exposure += 1`, `confidence = 1 − exp(−exposure/5)`.
    /// New binding: created at `input_strength`.
    pub fn bind(&mut self, idx: usize, concept_id: ConceptId, input_strength: f32, modality: Modality) {
        let lr = self.hebbian_lr;
        let entry = &mut self.entries[idx];
        let input = input_strength.clamp(0.0, 1.0);
        if let Some(b) = entry.bindings.iter_mut().find(|b| b.concept_id == concept_id) {
            let delta = lr * (1.0 - b.strength) * input;
            b.strength = (b.strength + delta).min(1.0);
            b.modality_strength[modality as usize] =
                (b.modality_strength[modality as usize] + lr * input).min(1.0);
            b.exposure_count = b.exposure_count.saturating_add(1);
            b.confidence = confidence_for(b.exposure_count);
        } else {
            entry.bindings.push(WordBinding::new(concept_id, input, modality));
        }
    }

    /// One-shot strong grounding (V1 `grounded_language_fast_map`): bind
    /// the word to `concept_id` at [`crate::FAST_MAP_STRENGTH`] and seed
    /// its `context_vector` from `features` (truncated/zero-padded to
    /// `semantic_dim`). Returns the entry index.
    pub fn fast_map(
        &mut self,
        word: &str,
        concept_id: ConceptId,
        features: &[f32],
        modality: Modality,
    ) -> usize {
        let idx = self.find_or_create(word);
        self.bind(idx, concept_id, crate::FAST_MAP_STRENGTH, modality);
        let dim = self.semantic_dim;
        let entry = &mut self.entries[idx];
        for (dst, &src) in entry.context_vector.iter_mut().zip(features.iter()).take(dim) {
            *dst = src;
        }
        entry.context_initialized = true;
        idx
    }

    /// Keep only the `top_k` strongest bindings on an entry (V1
    /// `grounded_language_prune_bindings`). `top_k == 0` clears them.
    pub fn prune_bindings(&mut self, idx: usize, top_k: usize) {
        let b = &mut self.entries[idx].bindings;
        if b.len() <= top_k {
            return;
        }
        b.sort_by(|x, y| y.strength.total_cmp(&x.strength));
        b.truncate(top_k);
    }

    /// Drop bindings below [`crate::ASSOC_PRUNE_THRESHOLD`] across the
    /// whole lexicon. Returns the number removed.
    pub fn prune_weak(&mut self) -> usize {
        let thresh = crate::ASSOC_PRUNE_THRESHOLD;
        let mut removed = 0;
        for e in &mut self.entries {
            let before = e.bindings.len();
            e.bindings.retain(|b| b.strength >= thresh);
            removed += before - e.bindings.len();
        }
        removed
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn find_or_create_is_idempotent_and_case_insensitive() {
        let mut lex = Lexicon::new(8);
        let a = lex.find_or_create("Dog");
        let b = lex.find_or_create("dog");
        assert_eq!(a, b);
        assert_eq!(lex.vocab_count(), 1);
        assert_eq!(lex.entry(a).form, "dog");
        assert_eq!(lex.entry(a).context_vector.len(), 8);
    }

    #[test]
    fn record_word_bumps_frequency() {
        let mut lex = Lexicon::new(4);
        let i = lex.record_word("cat");
        lex.record_word("cat");
        assert_eq!(lex.entry(i).frequency, 2);
    }

    #[test]
    fn bind_creates_then_strengthens() {
        let mut lex = Lexicon::new(4);
        let i = lex.find_or_create("dog");
        let c = ConceptId(7);
        lex.bind(i, c, 0.5, Modality::Visual);
        let s1 = lex.entry(i).bindings[0].strength;
        assert_eq!(s1, 0.5);
        // Re-expose: delta = lr*(1-0.5)*0.5 = 0.1*0.25 = 0.025.
        lex.bind(i, c, 0.5, Modality::Visual);
        let s2 = lex.entry(i).bindings[0].strength;
        assert!((s2 - 0.525).abs() < 1e-5, "got {s2}");
        assert_eq!(lex.entry(i).bindings[0].exposure_count, 2);
        assert!(lex.entry(i).bindings[0].confidence > 0.0);
    }

    #[test]
    fn bind_strength_saturates_at_one() {
        let mut lex = Lexicon::new(4);
        let i = lex.find_or_create("x");
        let c = ConceptId(1);
        for _ in 0..500 {
            lex.bind(i, c, 1.0, Modality::Linguistic);
        }
        assert!(lex.entry(i).bindings[0].strength <= 1.0);
        assert!(lex.entry(i).bindings[0].strength > 0.99);
    }

    #[test]
    fn distinct_concepts_get_distinct_bindings() {
        let mut lex = Lexicon::new(4);
        let i = lex.find_or_create("bank");
        lex.bind(i, ConceptId(1), 0.6, Modality::Visual);
        lex.bind(i, ConceptId(2), 0.4, Modality::Linguistic);
        assert_eq!(lex.entry(i).bindings.len(), 2);
        assert_eq!(lex.entry(i).best_binding().unwrap().concept_id, ConceptId(1));
    }

    #[test]
    fn fast_map_strong_binds_and_seeds_vector() {
        let mut lex = Lexicon::new(4);
        let i = lex.fast_map("apple", ConceptId(3), &[0.1, 0.2, 0.3, 0.4], Modality::Visual);
        assert_eq!(lex.entry(i).bindings[0].strength, crate::FAST_MAP_STRENGTH);
        assert!(lex.entry(i).context_initialized);
        assert_eq!(lex.entry(i).context_vector, vec![0.1, 0.2, 0.3, 0.4]);
    }

    #[test]
    fn fast_map_truncates_overlong_features() {
        let mut lex = Lexicon::new(2);
        let i = lex.fast_map("z", ConceptId(0), &[1.0, 2.0, 3.0, 4.0], Modality::Motor);
        assert_eq!(lex.entry(i).context_vector, vec![1.0, 2.0]);
    }

    #[test]
    fn prune_bindings_keeps_top_k() {
        let mut lex = Lexicon::new(4);
        let i = lex.find_or_create("w");
        lex.bind(i, ConceptId(1), 0.9, Modality::Visual);
        lex.bind(i, ConceptId(2), 0.3, Modality::Visual);
        lex.bind(i, ConceptId(3), 0.6, Modality::Visual);
        lex.prune_bindings(i, 2);
        assert_eq!(lex.entry(i).bindings.len(), 2);
        let kept: Vec<_> = lex.entry(i).bindings.iter().map(|b| b.concept_id).collect();
        assert!(kept.contains(&ConceptId(1)));
        assert!(kept.contains(&ConceptId(3)));
    }

    #[test]
    fn prune_weak_drops_below_threshold() {
        let mut lex = Lexicon::new(4);
        let i = lex.find_or_create("w");
        lex.bind(i, ConceptId(1), 0.5, Modality::Visual);
        // strength 0.005 < ASSOC_PRUNE_THRESHOLD (0.01).
        lex.bind(i, ConceptId(2), 0.005, Modality::Visual);
        let removed = lex.prune_weak();
        assert_eq!(removed, 1);
        assert_eq!(lex.entry(i).bindings.len(), 1);
    }

    #[test]
    fn serde_round_trip_rebuilds_index() {
        let mut lex = Lexicon::new(4);
        let i = lex.record_word("dog");
        lex.bind(i, ConceptId(1), 0.7, Modality::Visual);
        let json = serde_json::to_string(&lex).unwrap();
        let mut back: Lexicon = serde_json::from_str(&json).unwrap();
        back.reindex();
        assert_eq!(back.vocab_count(), 1);
        assert_eq!(back.find("dog"), Some(0));
        assert_eq!(back.entry(0).bindings[0].strength, 0.7);
    }
}

//! The grounded-language engine — owns the lexicon, the concept
//! registry, the RNG, and (added in later phases) the n-gram tables,
//! comprehend / produce paths, and persistence.
//!
//! Phase L2 implements distributional learning: [`GroundedLanguage::
//! learn_from_text`] is a word2vec-style SGNS update (attraction to
//! context + frequency subsampling + negative sampling + L2-normalize).
//! This is the engine's first real learning path, and the design is the
//! root-cause fix for V1's repeated distributional collapse — the
//! subsample + negative-sampling terms are present from the start, not
//! bolted on after a collapse.

use serde::{Deserialize, Serialize};

use crate::concept::ConceptRegistry;
use crate::lexicon::Lexicon;
use crate::text::tokenize;
use crate::{CONTEXT_WINDOW, FREQ_SUBSAMPLE_T, K_NEG, SEMANTIC_DIM, XorShift64, normalize_vector};

/// Lightweight learning telemetry (a slice of V1's `gl_stats_t`).
#[derive(Debug, Clone, Copy, Default, Serialize, Deserialize)]
pub struct LanguageStats {
    /// Number of `learn_from_text` calls.
    pub learn_calls: u64,
    /// Total tokens consumed by learning.
    pub tokens_seen: u64,
    /// Number of distributional context-vector updates applied.
    pub context_updates: u64,
}

/// The grounded-language engine.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GroundedLanguage {
    pub lexicon: Lexicon,
    pub concepts: ConceptRegistry,
    rng: XorShift64,
    pub stats: LanguageStats,
}

impl GroundedLanguage {
    /// New engine with the given embedding width + RNG seed.
    #[must_use]
    pub fn new(semantic_dim: usize, rng_seed: u64) -> Self {
        Self {
            lexicon: Lexicon::new(semantic_dim),
            concepts: ConceptRegistry::new(),
            rng: XorShift64::new(rng_seed),
            stats: LanguageStats::default(),
        }
    }

    /// New engine with the default [`SEMANTIC_DIM`].
    #[must_use]
    pub fn with_seed(rng_seed: u64) -> Self {
        Self::new(SEMANTIC_DIM, rng_seed)
    }

    /// Rebuild derived state after a serde load (the lexicon's form→index
    /// map is `#[serde(skip)]`).
    pub fn reindex(&mut self) {
        self.lexicon.reindex();
    }

    /// Distributional learning over one text span (SGNS).
    ///
    /// For each center word, over a `±CONTEXT_WINDOW` window:
    /// - attract `center.ctx += lr · neighbor.ctx`, with
    ///   `lr = hebbian_lr · 0.1 · (1/dist) · freq_factor` and
    ///   `freq_factor = sqrt(T/freq)` for frequent neighbors (subsampling);
    /// - then draw `K_NEG` random negatives and repel
    ///   `center.ctx -= neg_lr · neg.ctx` (`neg_lr = hebbian_lr·0.1·0.25`);
    /// - L2-normalize the center vector.
    ///
    /// Uninitialized centers are seeded with small random noise (once the
    /// span is long enough to carry signal). Frequencies are bumped first
    /// so subsampling reflects the current span.
    pub fn learn_from_text(&mut self, text: &str) {
        let tokens = tokenize(text);
        if tokens.is_empty() {
            return;
        }
        self.stats.learn_calls += 1;
        self.stats.tokens_seen += tokens.len() as u64;

        // Record words (bump frequency) and capture their entry indices.
        let indices: Vec<usize> = tokens.iter().map(|t| self.lexicon.record_word(t)).collect();
        let word_count = indices.len();
        let dim = self.lexicon.semantic_dim;
        let lr_base = self.lexicon.hebbian_lr * 0.1;
        let neg_lr = self.lexicon.hebbian_lr * 0.1 * 0.25;
        let window = CONTEXT_WINDOW;

        for i in 0..word_count {
            let center_idx = indices[i];

            // Seed an uninitialized center with small noise (V1: only
            // once the span carries enough context).
            if !self.lexicon.entry(center_idx).context_initialized && word_count > 2 {
                for d in 0..dim {
                    let v = self.rng.next_centered(0.05);
                    self.lexicon.entry_mut(center_idx).context_vector[d] = v;
                }
                self.lexicon.entry_mut(center_idx).context_initialized = true;
            }

            // Accumulate the delta from neighbors + negatives using only
            // immutable reads, then apply once (avoids aliasing the same
            // Vec mutably + immutably).
            let mut acc = vec![0.0_f32; dim];
            let lo = i.saturating_sub(window);
            let hi = (i + window).min(word_count - 1);
            for (off, &neighbor_idx) in indices[lo..=hi].iter().enumerate() {
                let j = lo + off;
                if j == i {
                    continue;
                }
                if neighbor_idx == center_idx {
                    continue;
                }
                let ne = self.lexicon.entry(neighbor_idx);
                if !ne.context_initialized {
                    continue;
                }
                #[allow(clippy::cast_precision_loss)]
                let dist = (j as isize - i as isize).unsigned_abs() as f32;
                let dist_weight = 1.0 / dist;
                #[allow(clippy::cast_precision_loss)]
                let freq = ne.frequency as f32;
                let freq_factor = if freq > FREQ_SUBSAMPLE_T {
                    (FREQ_SUBSAMPLE_T / freq).sqrt()
                } else {
                    1.0
                };
                let lr = lr_base * dist_weight * freq_factor;
                for (a, &nv) in acc.iter_mut().zip(ne.context_vector.iter()) {
                    *a += lr * nv;
                }
            }

            // Negative sampling — repel from K random initialized words.
            let vocab = self.lexicon.vocab_count();
            let mut did_neg = false;
            if vocab > 16 {
                for _ in 0..K_NEG {
                    #[allow(clippy::cast_possible_truncation)]
                    let neg_idx = (self.rng.next_u64() % vocab as u64) as usize;
                    if neg_idx == center_idx {
                        continue;
                    }
                    let ne = self.lexicon.entry(neg_idx);
                    if !ne.context_initialized {
                        continue;
                    }
                    for (a, &nv) in acc.iter_mut().zip(ne.context_vector.iter()) {
                        *a -= neg_lr * nv;
                    }
                    did_neg = true;
                }
            }

            // Apply + normalize.
            let center = self.lexicon.entry_mut(center_idx);
            for (c, &a) in center.context_vector.iter_mut().zip(acc.iter()) {
                *c += a;
            }
            if did_neg {
                normalize_vector(&mut center.context_vector);
            }
            self.stats.context_updates += 1;
        }
    }

    /// Cosine similarity between two words' learned context vectors.
    /// Returns `0.0` if either word is unknown or uninitialized.
    #[must_use]
    pub fn word_similarity(&self, a: &str, b: &str) -> f32 {
        let (Some(ia), Some(ib)) = (self.lexicon.find(a), self.lexicon.find(b)) else {
            return 0.0;
        };
        let ea = self.lexicon.entry(ia);
        let eb = self.lexicon.entry(ib);
        if !ea.context_initialized || !eb.context_initialized {
            return 0.0;
        }
        crate::cosine_similarity(&ea.context_vector, &eb.context_vector)
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn empty_text_is_noop() {
        let mut gl = GroundedLanguage::with_seed(1);
        gl.learn_from_text("   ");
        assert_eq!(gl.stats.learn_calls, 0);
        assert_eq!(gl.lexicon.vocab_count(), 0);
    }

    #[test]
    fn learning_populates_lexicon() {
        let mut gl = GroundedLanguage::new(32, 42);
        gl.learn_from_text("the dog runs fast");
        assert_eq!(gl.lexicon.vocab_count(), 4);
        assert!(gl.stats.learn_calls == 1);
        assert!(gl.stats.tokens_seen == 4);
    }

    #[test]
    fn deterministic_under_seed() {
        let mut a = GroundedLanguage::new(16, 7);
        let mut b = GroundedLanguage::new(16, 7);
        for _ in 0..20 {
            a.learn_from_text("the cat sat on the mat");
            b.learn_from_text("the cat sat on the mat");
        }
        let ia = a.lexicon.find("cat").unwrap();
        let ib = b.lexicon.find("cat").unwrap();
        assert_eq!(
            a.lexicon.entry(ia).context_vector,
            b.lexicon.entry(ib).context_vector
        );
    }

    #[test]
    fn vectors_stay_normalized_after_negatives() {
        let mut gl = GroundedLanguage::new(32, 3);
        // Need vocab > 16 for negative sampling to engage.
        let corpus = "alpha beta gamma delta epsilon zeta eta theta iota kappa \
                      lambda mu nu xi omicron pi rho sigma tau upsilon phi chi";
        for _ in 0..30 {
            gl.learn_from_text(corpus);
        }
        // Every initialized vector should be ~unit norm (negatives → normalize).
        for e in gl.lexicon.entries() {
            if e.context_initialized {
                let n: f32 = e.context_vector.iter().map(|x| x * x).sum::<f32>().sqrt();
                assert!((n - 1.0).abs() < 1e-3 || n < 1e-6, "norm {n} for {}", e.form);
            }
        }
    }

    // The collapse regression: train on a narrow, repetitive corpus and
    // assert the learned embeddings DON'T all converge to one direction.
    // V1 collapsed here (pure attraction averaged everything together);
    // subsampling + negative sampling keeps content words separated.
    #[test]
    fn narrow_corpus_does_not_collapse() {
        let mut gl = GroundedLanguage::new(64, 11);
        let sentences = [
            "the dog chased the cat",
            "the cat saw the bird",
            "a bird flew over the dog",
            "the dog ate the food",
            "the cat liked the food",
            "a happy dog ran around",
            "the quick bird sang loud",
            "the lazy cat slept long",
            "dogs and cats are animals",
            "birds and fish are animals",
        ];
        for _ in 0..200 {
            for s in &sentences {
                gl.learn_from_text(s);
            }
        }
        // Content words should NOT be near-identical (collapse signature).
        let pairs = [("dog", "bird"), ("cat", "food"), ("dog", "food")];
        let mut max_sim = 0.0_f32;
        for (a, b) in pairs {
            let s = gl.word_similarity(a, b);
            max_sim = max_sim.max(s.abs());
        }
        assert!(
            max_sim < 0.97,
            "distributional collapse: unrelated content words at cos {max_sim}"
        );
        // And a word is perfectly similar to itself (sanity).
        assert!((gl.word_similarity("dog", "dog") - 1.0).abs() < 1e-4);
    }
}

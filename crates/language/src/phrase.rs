//! N-gram / phrase table — port of V1's `gl_phrase_t` + `_gl_track_phrases`
//! + the produce-time bigram reranker.
//!
//! Tracks bigram `(a, b)` and trigram `(a, b, c)` frequencies keyed by
//! lexicon **entry index** (stable + serde-round-trippable since the
//! lexicon `Vec` is append-only). The table is capped at
//! [`crate::MAX_PHRASES`] distinct entries; on overflow the
//! least-frequent entry is evicted (ties broken by the smaller key, for
//! determinism).
//!
//! The produce path consumes [`PhraseTable::bigram_bias`]:
//! `α·ln(1 + freq(prev, cand))` with `α = `[`crate::BIGRAM_RERANK_ALPHA`].

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use crate::{BIGRAM_RERANK_ALPHA, MAX_PHRASES};

/// Bigram/trigram frequency table over lexicon entry indices.
///
/// Serialized via a `Vec` wire form ([`PhraseWire`]) so it round-trips
/// through string-keyed formats like JSON (tuple-keyed maps can't be JSON
/// object keys). The wire form is sorted for deterministic output.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(into = "PhraseWire", from = "PhraseWire")]
pub struct PhraseTable {
    bigrams: HashMap<(u32, u32), u32>,
    trigrams: HashMap<(u32, u32, u32), u32>,
    /// Max distinct bigram entries before LFU eviction.
    cap: usize,
}

/// Serde wire form for [`PhraseTable`] (sorted vectors).
#[derive(Serialize, Deserialize)]
struct PhraseWire {
    /// `(a, b, freq)` bigram triples.
    bigrams: Vec<(u32, u32, u32)>,
    /// `(a, b, c, freq)` trigram quads.
    trigrams: Vec<(u32, u32, u32, u32)>,
    cap: usize,
}

impl From<PhraseTable> for PhraseWire {
    fn from(t: PhraseTable) -> Self {
        let mut bigrams: Vec<(u32, u32, u32)> =
            t.bigrams.iter().map(|(&(a, b), &f)| (a, b, f)).collect();
        bigrams.sort_unstable();
        let mut trigrams: Vec<(u32, u32, u32, u32)> =
            t.trigrams.iter().map(|(&(a, b, c), &f)| (a, b, c, f)).collect();
        trigrams.sort_unstable();
        Self { bigrams, trigrams, cap: t.cap }
    }
}

impl From<PhraseWire> for PhraseTable {
    fn from(w: PhraseWire) -> Self {
        let mut bigrams = HashMap::with_capacity(w.bigrams.len());
        for (a, b, f) in w.bigrams {
            bigrams.insert((a, b), f);
        }
        let mut trigrams = HashMap::with_capacity(w.trigrams.len());
        for (a, b, c, f) in w.trigrams {
            trigrams.insert((a, b, c), f);
        }
        Self { bigrams, trigrams, cap: w.cap.max(1) }
    }
}

impl Default for PhraseTable {
    fn default() -> Self {
        Self::new(MAX_PHRASES)
    }
}

impl PhraseTable {
    /// New table with the given bigram capacity.
    #[must_use]
    pub fn new(cap: usize) -> Self {
        Self {
            bigrams: HashMap::new(),
            trigrams: HashMap::new(),
            cap: cap.max(1),
        }
    }

    /// Slide over a token-index sequence, incrementing every adjacent
    /// bigram and trigram frequency.
    pub fn track(&mut self, ids: &[usize]) {
        for w in ids.windows(2) {
            self.bump_bigram(w[0] as u32, w[1] as u32);
        }
        for w in ids.windows(3) {
            let key = (w[0] as u32, w[1] as u32, w[2] as u32);
            *self.trigrams.entry(key).or_insert(0) += 1;
        }
    }

    fn bump_bigram(&mut self, a: u32, b: u32) {
        *self.bigrams.entry((a, b)).or_insert(0) += 1;
        // Insert-then-evict-global-min: a brand-new rare bigram won't
        // displace an established one — if it's the new global minimum it
        // is evicted right back out, leaving the table unchanged.
        if self.bigrams.len() > self.cap {
            self.evict_min_bigram();
        }
    }

    /// Evict the least-frequent bigram (smaller key wins ties → deterministic).
    fn evict_min_bigram(&mut self) {
        let victim = self
            .bigrams
            .iter()
            .min_by(|(ka, fa), (kb, fb)| fa.cmp(fb).then(ka.cmp(kb)))
            .map(|(k, _)| *k);
        if let Some(k) = victim {
            self.bigrams.remove(&k);
        }
    }

    /// Frequency of bigram `(prev, cand)`.
    #[must_use]
    pub fn bigram_freq(&self, prev: usize, cand: usize) -> u32 {
        self.bigrams
            .get(&(prev as u32, cand as u32))
            .copied()
            .unwrap_or(0)
    }

    /// Frequency of trigram `(a, b, c)`.
    #[must_use]
    pub fn trigram_freq(&self, a: usize, b: usize, c: usize) -> u32 {
        self.trigrams
            .get(&(a as u32, b as u32, c as u32))
            .copied()
            .unwrap_or(0)
    }

    /// Produce-time rerank bias: `α·ln(1 + freq(prev, cand))`.
    #[must_use]
    pub fn bigram_bias(&self, prev: usize, cand: usize) -> f32 {
        #[allow(clippy::cast_precision_loss)]
        let f = self.bigram_freq(prev, cand) as f32;
        BIGRAM_RERANK_ALPHA * (1.0 + f).ln()
    }

    /// Number of distinct bigrams retained.
    #[must_use]
    pub fn bigram_count(&self) -> usize {
        self.bigrams.len()
    }

    /// Number of distinct trigrams retained.
    #[must_use]
    pub fn trigram_count(&self) -> usize {
        self.trigrams.len()
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn track_counts_bigrams_and_trigrams() {
        let mut t = PhraseTable::default();
        // "the dog runs" → ids [0,1,2]
        t.track(&[0, 1, 2]);
        t.track(&[0, 1, 2]);
        assert_eq!(t.bigram_freq(0, 1), 2);
        assert_eq!(t.bigram_freq(1, 2), 2);
        assert_eq!(t.trigram_freq(0, 1, 2), 2);
        assert_eq!(t.bigram_freq(2, 0), 0);
    }

    #[test]
    fn bigram_bias_is_monotone_in_frequency() {
        let mut t = PhraseTable::default();
        for _ in 0..10 {
            t.track(&[5, 6]);
        }
        let b_hi = t.bigram_bias(5, 6);
        let b_lo = t.bigram_bias(5, 99);
        assert!(b_hi > b_lo);
        assert_eq!(b_lo, 0.0, "unseen bigram → ln(1)=0 bias");
        // α·ln(1+10).
        assert!((b_hi - BIGRAM_RERANK_ALPHA * 11.0_f32.ln()).abs() < 1e-6);
    }

    #[test]
    fn cap_evicts_least_frequent() {
        let mut t = PhraseTable::new(2);
        // (0,1) frequent, (2,3) frequent, (4,5) rare → (4,5) evicted.
        for _ in 0..5 {
            t.bump_bigram(0, 1);
        }
        for _ in 0..3 {
            t.bump_bigram(2, 3);
        }
        t.bump_bigram(4, 5); // overflow → evicts the min (the new freq-1 one)
        assert_eq!(t.bigram_count(), 2);
        assert_eq!(t.bigram_freq(0, 1), 5);
        assert_eq!(t.bigram_freq(2, 3), 3);
        assert_eq!(t.bigram_freq(4, 5), 0);
    }

    #[test]
    fn serde_round_trip() {
        let mut t = PhraseTable::default();
        t.track(&[1, 2, 3]);
        let json = serde_json::to_string(&t).unwrap();
        let back: PhraseTable = serde_json::from_str(&json).unwrap();
        assert_eq!(back.bigram_freq(1, 2), 1);
        assert_eq!(back.trigram_freq(1, 2, 3), 1);
    }
}

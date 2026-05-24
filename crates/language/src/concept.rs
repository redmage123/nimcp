//! Cross-modal concept registry — a union-find over text / visual /
//! audio fingerprints, port of V1's `nimcp_concept_registry.c`.
//!
//! A *concept* is a canonical identity that several modalities can point
//! at: the word "dog", a visual feature digest, and an audio digest can
//! all be bound to one [`ConceptId`]. Interning a fingerprint is
//! first-write-wins; [`ConceptRegistry::bind_modalities`] unions two ids
//! (the smaller root stays canonical), and [`ConceptRegistry::canonical`]
//! resolves any id to its root with path compression.
//!
//! Float fingerprints (visual / audio) are quantized to a `0.1` lattice
//! before hashing — a cheap LSH so near-identical feature vectors collide
//! onto the same concept (matches V1's `intern_visual` / `intern_audio`).

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use crate::fnv1a_lower;

/// Canonical concept identity. Raw value doubles as the index into the
/// union-find parent array.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct ConceptId(pub u32);

/// Cross-modal concept registry.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConceptRegistry {
    /// Union-find parent array, indexed by raw concept id.
    parent: Vec<u32>,
    /// Text fingerprint (FNV-1a of trimmed+lowercased form) → concept.
    text_map: HashMap<u32, u32>,
    /// Visual fingerprint (quantized-digest hash) → concept.
    visual_map: HashMap<u32, u32>,
    /// Audio fingerprint (quantized-digest hash) → concept.
    audio_map: HashMap<u32, u32>,
    /// Count of successful `bind_modalities` unions (diagnostic).
    modality_bindings: u64,
}

impl Default for ConceptRegistry {
    fn default() -> Self {
        Self::new()
    }
}

impl ConceptRegistry {
    /// Empty registry.
    #[must_use]
    pub fn new() -> Self {
        Self {
            parent: Vec::new(),
            text_map: HashMap::new(),
            visual_map: HashMap::new(),
            audio_map: HashMap::new(),
            modality_bindings: 0,
        }
    }

    /// Allocate a brand-new concept (its own union-find root).
    fn alloc(&mut self) -> ConceptId {
        let id = self.parent.len() as u32;
        self.parent.push(id);
        ConceptId(id)
    }

    /// Quantize a float fingerprint to a `0.1` lattice and FNV-fold it
    /// into a 32-bit digest. Near-identical vectors collide → same hash.
    fn quantize_hash(features: &[f32]) -> u32 {
        let mut h = 0x811c_9dc5_u32;
        for &x in features {
            // Round to nearest 0.1 bucket; clamp tames NaN/Inf to 0.
            #[allow(clippy::cast_possible_truncation)]
            let q = if x.is_finite() {
                (x * 10.0).round() as i32
            } else {
                0
            };
            for b in q.to_le_bytes() {
                h ^= u32::from(b);
                h = h.wrapping_mul(0x0100_0193);
            }
        }
        h
    }

    /// Intern a text form → canonical concept. Trims + lowercases first.
    pub fn intern_text(&mut self, text: &str) -> ConceptId {
        let key = fnv1a_lower(text.trim());
        if let Some(&raw) = self.text_map.get(&key) {
            return self.canonical(ConceptId(raw));
        }
        let id = self.alloc();
        self.text_map.insert(key, id.0);
        id
    }

    /// Intern a visual feature digest → canonical concept.
    pub fn intern_visual(&mut self, features: &[f32]) -> ConceptId {
        let key = Self::quantize_hash(features);
        if let Some(&raw) = self.visual_map.get(&key) {
            return self.canonical(ConceptId(raw));
        }
        let id = self.alloc();
        self.visual_map.insert(key, id.0);
        id
    }

    /// Intern an audio feature digest → canonical concept.
    pub fn intern_audio(&mut self, features: &[f32]) -> ConceptId {
        let key = Self::quantize_hash(features);
        if let Some(&raw) = self.audio_map.get(&key) {
            return self.canonical(ConceptId(raw));
        }
        let id = self.alloc();
        self.audio_map.insert(key, id.0);
        id
    }

    /// Resolve to the union-find root, compressing the path. Out-of-range
    /// ids resolve to themselves (defensive — a stale id from an old
    /// checkpoint never panics).
    pub fn canonical(&mut self, id: ConceptId) -> ConceptId {
        let n = self.parent.len() as u32;
        if id.0 >= n {
            return id;
        }
        // Find root.
        let mut root = id.0;
        while self.parent[root as usize] != root {
            root = self.parent[root as usize];
        }
        // Compress.
        let mut cur = id.0;
        while self.parent[cur as usize] != root {
            let next = self.parent[cur as usize];
            self.parent[cur as usize] = root;
            cur = next;
        }
        ConceptId(root)
    }

    /// Union two concepts so they share one canonical id. The numerically
    /// smaller root becomes the parent (stable, deterministic). Returns
    /// the resulting canonical id, or `None` if either id is out of range.
    pub fn bind_modalities(&mut self, a: ConceptId, b: ConceptId) -> Option<ConceptId> {
        let n = self.parent.len() as u32;
        if a.0 >= n || b.0 >= n {
            return None;
        }
        let ra = self.canonical(a).0;
        let rb = self.canonical(b).0;
        if ra == rb {
            return Some(ConceptId(ra));
        }
        let (keep, drop) = if ra < rb { (ra, rb) } else { (rb, ra) };
        self.parent[drop as usize] = keep;
        self.modality_bindings += 1;
        Some(ConceptId(keep))
    }

    /// Total interned referents (union-find slots, before merging).
    #[must_use]
    pub fn total_referents(&self) -> usize {
        self.parent.len()
    }

    /// Number of distinct canonical concepts after all unions.
    #[must_use]
    pub fn distinct_concepts(&mut self) -> usize {
        let n = self.parent.len() as u32;
        let mut roots = std::collections::HashSet::new();
        for raw in 0..n {
            roots.insert(self.canonical(ConceptId(raw)).0);
        }
        roots.len()
    }

    /// Count of successful modality unions (diagnostic).
    #[must_use]
    pub fn total_modality_bindings(&self) -> u64 {
        self.modality_bindings
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn intern_text_is_first_write_wins() {
        let mut r = ConceptRegistry::new();
        let a = r.intern_text("dog");
        let b = r.intern_text("dog");
        let c = r.intern_text("DOG  ");
        assert_eq!(a, b);
        assert_eq!(a, c, "trim + lowercase should collide");
        assert_eq!(r.total_referents(), 1);
    }

    #[test]
    fn distinct_text_concepts_differ() {
        let mut r = ConceptRegistry::new();
        let dog = r.intern_text("dog");
        let cat = r.intern_text("cat");
        assert_ne!(dog, cat);
        assert_eq!(r.total_referents(), 2);
    }

    #[test]
    fn visual_quantization_collides_near_vectors() {
        let mut r = ConceptRegistry::new();
        let a = r.intern_visual(&[0.50, 0.50, 0.50]);
        // Within the 0.1 lattice bucket → same digest.
        let b = r.intern_visual(&[0.52, 0.49, 0.51]);
        assert_eq!(a, b);
        // Outside the bucket → distinct.
        let c = r.intern_visual(&[0.9, 0.1, 0.2]);
        assert_ne!(a, c);
    }

    #[test]
    fn bind_modalities_unions_to_smaller_root() {
        let mut r = ConceptRegistry::new();
        let txt = r.intern_text("dog"); // id 0
        let vis = r.intern_visual(&[0.3, 0.3]); // id 1
        let canon = r.bind_modalities(txt, vis).unwrap();
        assert_eq!(canon, ConceptId(0), "smaller root canonical");
        assert_eq!(r.canonical(txt), r.canonical(vis));
        assert_eq!(r.distinct_concepts(), 1);
        assert_eq!(r.total_modality_bindings(), 1);
    }

    #[test]
    fn bind_same_concept_is_noop_union() {
        let mut r = ConceptRegistry::new();
        let a = r.intern_text("x");
        let canon = r.bind_modalities(a, a).unwrap();
        assert_eq!(canon, a);
        assert_eq!(r.total_modality_bindings(), 0, "self-union counts nothing");
    }

    #[test]
    fn bind_out_of_range_returns_none() {
        let mut r = ConceptRegistry::new();
        let a = r.intern_text("x");
        assert!(r.bind_modalities(a, ConceptId(999)).is_none());
    }

    #[test]
    fn stale_id_canonical_is_self() {
        let mut r = ConceptRegistry::new();
        assert_eq!(r.canonical(ConceptId(123)), ConceptId(123));
    }

    #[test]
    fn transitive_union_resolves_to_one_root() {
        let mut r = ConceptRegistry::new();
        let a = r.intern_text("a");
        let b = r.intern_text("b");
        let c = r.intern_text("c");
        r.bind_modalities(a, b);
        r.bind_modalities(b, c);
        let ca = r.canonical(a);
        assert_eq!(ca, r.canonical(b));
        assert_eq!(ca, r.canonical(c));
        assert_eq!(r.distinct_concepts(), 1);
    }

    #[test]
    fn serde_round_trip() {
        let mut r = ConceptRegistry::new();
        let a = r.intern_text("dog");
        let v = r.intern_visual(&[0.4, 0.4]);
        r.bind_modalities(a, v);
        let json = serde_json::to_string(&r).unwrap();
        let mut back: ConceptRegistry = serde_json::from_str(&json).unwrap();
        assert_eq!(back.total_referents(), 2);
        assert_eq!(back.canonical(a), back.canonical(v));
        assert_eq!(back.total_modality_bindings(), 1);
    }
}

//! Poincaré-disk embedding helpers for memory features.
//!
//! Alternative distance primitive for memory nodes: instead of
//! Euclidean / cosine on a full feature vector, project a vector into
//! the 2D Poincaré disk and use hyperbolic distance. This is useful
//! when the stored features are conceptually hierarchical, where
//! hyperbolic space represents trees with lower distortion than
//! Euclidean space.
//!
//! All three helpers are pure and additive — they don't touch
//! [`crate::ZLadder`] or any persistent state. Callers decide when to
//! substitute them for the default cosine-based
//! [`crate::query::QueryHit`] pipeline.
//!
//! # Clipping policy
//!
//! Every function clips its output into the open unit disk with
//! [`POINCARE_CLIP_EPS`] margin, so downstream
//! [`poincare_distance`] never sees a point on or outside the
//! boundary (where hyperbolic distance diverges).

use nimcp_geometry::{PoincarePoint, poincare_clip, poincare_distance};

/// Quick Poincaré-disk embedding of a feature vector.
///
/// Treats the first two components of `features` as `(x, y)`, clamps
/// them into the open unit disk with [`POINCARE_CLIP_EPS`] margin,
/// and returns the resulting point. Shorter vectors are padded with
/// zeros (so a single-component vector embeds on the x-axis; an empty
/// vector embeds at the origin).
///
/// Use this when the first two feature dimensions already encode a
/// hierarchy / 2D layout — e.g. an explicit `(parent_depth, sibling_offset)`.
///
/// See [`embed_unit_disk`] for the alternative that normalises
/// higher-dimensional vectors via their L2 norm.
///
/// [`POINCARE_CLIP_EPS`]: nimcp_geometry::POINCARE_CLIP_EPS
#[must_use]
pub fn embed_2d(features: &[f32]) -> PoincarePoint {
    let x = features.first().copied().unwrap_or(0.0);
    let y = features.get(1).copied().unwrap_or(0.0);
    let mut p = PoincarePoint::new(x, y);
    poincare_clip(&mut p);
    p
}

/// Hyperbolic distance between two feature vectors after applying
/// [`embed_2d`] to each. Symmetric, non-negative, zero iff the two
/// vectors embed to the same clipped point.
#[must_use]
pub fn hyperbolic_distance_2d(a: &[f32], b: &[f32]) -> f32 {
    let pa = embed_2d(a);
    let pb = embed_2d(b);
    poincare_distance(&pa, &pb)
}

/// Normalise any-length feature vector into the unit disk via its L2
/// norm.
///
/// Divides the first two components by `max(||features||₂, eps)`,
/// yielding a point whose Euclidean norm is at most 1, then clips
/// into the open disk. Features shorter than 2 components pad with
/// zeros before normalising.
///
/// Use this when feature vectors are high-dimensional but you want a
/// cheap 2D hyperbolic distance — it preserves the *direction* of
/// the first two components while scaling magnitudes to a bounded
/// range the Poincaré model can consume.
#[must_use]
pub fn embed_unit_disk(features: &[f32]) -> PoincarePoint {
    // Pad to length >= 2 conceptually by treating missing entries as 0.
    let x = features.first().copied().unwrap_or(0.0);
    let y = features.get(1).copied().unwrap_or(0.0);

    // L2 norm over the *full* feature vector (not just the first two
    // components) — matches how downstream hierarchical-embedding
    // code uses the global magnitude as the anchor.
    let norm_sq: f32 = features.iter().map(|v| v * v).sum();
    let norm = norm_sq.sqrt();

    // Avoid divide-by-zero for all-zero inputs; fall back to origin.
    let (nx, ny) = if norm > f32::EPSILON {
        (x / norm, y / norm)
    } else {
        (0.0, 0.0)
    };

    let mut p = PoincarePoint::new(nx, ny);
    poincare_clip(&mut p);
    p
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use nimcp_geometry::{POINCARE_CLIP_EPS, poincare_norm};

    #[test]
    fn short_features_pad_with_zero() {
        // Empty / 1-element inputs don't panic — they embed on the
        // x-axis (or at the origin).
        let p_empty = embed_2d(&[]);
        assert_eq!(p_empty, PoincarePoint::origin());

        let p_single = embed_2d(&[0.3]);
        assert!((p_single.x - 0.3).abs() < 1e-6);
        assert_eq!(p_single.y, 0.0);
    }

    #[test]
    fn embed_2d_clips_near_boundary() {
        // Input at radius 0.99999 must end up strictly inside the
        // disk after embedding.
        let p = embed_2d(&[0.99999, 0.0]);
        assert!(
            poincare_norm(&p) < 1.0,
            "clipped point should be strictly inside disk, got norm {}",
            poincare_norm(&p)
        );
        // And at least the advertised margin away from the boundary.
        assert!(poincare_norm(&p) <= 1.0 - POINCARE_CLIP_EPS + 1e-6);
    }

    #[test]
    fn embed_2d_clips_over_boundary() {
        // Input explicitly *outside* the disk must still clip safely.
        let p = embed_2d(&[2.5, 0.0]);
        assert!(poincare_norm(&p) < 1.0);
    }

    #[test]
    fn hyperbolic_distance_is_symmetric() {
        let a = [0.1, 0.2, 5.0];
        let b = [-0.3, 0.4, -1.0];
        let dab = hyperbolic_distance_2d(&a, &b);
        let dba = hyperbolic_distance_2d(&b, &a);
        assert!((dab - dba).abs() < 1e-5);
    }

    #[test]
    fn hyperbolic_distance_is_nonnegative_and_self_distance_zero() {
        let a = [0.25, -0.15];
        assert!(hyperbolic_distance_2d(&a, &a).abs() < 1e-5);

        let b = [0.4, 0.2];
        assert!(hyperbolic_distance_2d(&a, &b) >= 0.0);
    }

    #[test]
    fn unit_disk_embedding_of_all_zeros_is_origin() {
        let p = embed_unit_disk(&[0.0, 0.0, 0.0, 0.0]);
        assert_eq!(p, PoincarePoint::origin());
    }

    #[test]
    fn unit_disk_embedding_clips_large_vectors() {
        // Large raw components — after normalisation and clipping the
        // result is strictly inside the unit disk.
        let p = embed_unit_disk(&[1e6, 2e6, 3e6]);
        assert!(poincare_norm(&p) < 1.0);
        assert!(poincare_norm(&p) <= 1.0 - POINCARE_CLIP_EPS + 1e-6);
    }

    #[test]
    fn unit_disk_embedding_short_features_pads_with_zeros() {
        // 1-element feature — second component treated as 0.
        let p = embed_unit_disk(&[0.5]);
        assert_eq!(p.y, 0.0);
        assert!(poincare_norm(&p) < 1.0);
    }
}

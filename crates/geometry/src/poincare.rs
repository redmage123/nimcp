//! Poincaré disk model of 2D hyperbolic geometry.
//!
//! Points live in the open unit disk `{(x, y) : x² + y² < 1}`. The
//! hyperbolic distance between two points is
//!
//! ```text
//!     d(u, v) = acosh(1 + 2 · ||u - v||² / ((1 - ||u||²)(1 - ||v||²)))
//! ```
//!
//! Distances → ∞ as either point approaches the boundary. The
//! model preserves angles (conformal) but distorts lengths — two
//! points near the boundary at the same Euclidean distance are
//! hyperbolic-ally much further apart than two points near the origin.
//!
//! Port of V1's `utils/geometry/nimcp_hyperbolic.h`, minus the
//! gradient-descent embedding code (which is caller-side in V2).

use serde::{Deserialize, Serialize};

/// Epsilon from the disk boundary — `poincare_clip` leaves points
/// at least this far inside the unit disk so `1 - ||p||²` never
/// collapses to zero in distance computations.
pub const POINCARE_CLIP_EPS: f32 = 1.0e-5;

/// 2D point in the Poincaré disk.
#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct PoincarePoint {
    /// First coordinate.
    pub x: f32,
    /// Second coordinate.
    pub y: f32,
}

impl PoincarePoint {
    /// Construct a point. Does **not** clip — call
    /// [`poincare_clip`] if you got the point from an uncontrolled
    /// source (e.g., gradient step output).
    #[must_use]
    pub fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }

    /// Origin `(0, 0)`.
    #[must_use]
    pub fn origin() -> Self {
        Self { x: 0.0, y: 0.0 }
    }
}

/// Euclidean norm `||p||`.
#[must_use]
pub fn poincare_norm(p: &PoincarePoint) -> f32 {
    (p.x * p.x + p.y * p.y).sqrt()
}

/// Clip `p` into the open unit disk, leaving at least
/// [`POINCARE_CLIP_EPS`] margin from the boundary. Mutates in place.
pub fn poincare_clip(p: &mut PoincarePoint) {
    let r = poincare_norm(p);
    let r_max = 1.0 - POINCARE_CLIP_EPS;
    if r >= r_max && r > 0.0 {
        let s = r_max / r;
        p.x *= s;
        p.y *= s;
    }
}

/// Hyperbolic distance.
#[must_use]
pub fn poincare_distance(u: &PoincarePoint, v: &PoincarePoint) -> f32 {
    let dx = u.x - v.x;
    let dy = u.y - v.y;
    let d2 = dx * dx + dy * dy;
    let nu = u.x * u.x + u.y * u.y;
    let nv = v.x * v.x + v.y * v.y;
    // Keep denominator bounded away from zero.
    let denom = ((1.0 - nu).max(f32::EPSILON)) * ((1.0 - nv).max(f32::EPSILON));
    let arg = 1.0 + 2.0 * d2 / denom;
    // acosh(z) = ln(z + sqrt(z² − 1)) for z ≥ 1. Guard against z < 1
    // from rounding.
    let z = arg.max(1.0);
    (z + (z * z - 1.0).sqrt()).ln()
}

/// Conformal factor `(2 / (1 - ||p||²))²`. Scales the Euclidean
/// metric tensor to the hyperbolic one at `p`. Used by
/// Riemannian-gradient optimizers.
#[must_use]
pub fn poincare_conformal_factor(p: &PoincarePoint) -> f32 {
    let n2 = p.x * p.x + p.y * p.y;
    let denom = (1.0 - n2).max(f32::EPSILON);
    let c = 2.0 / denom;
    c * c
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn distance_to_self_is_zero() {
        let p = PoincarePoint::new(0.3, 0.4);
        let d = poincare_distance(&p, &p);
        assert!(d.abs() < 1e-5);
    }

    #[test]
    fn distance_is_symmetric() {
        let u = PoincarePoint::new(0.1, 0.2);
        let v = PoincarePoint::new(-0.3, 0.4);
        let duv = poincare_distance(&u, &v);
        let dvu = poincare_distance(&v, &u);
        assert!((duv - dvu).abs() < 1e-5);
    }

    #[test]
    fn distance_grows_toward_boundary() {
        // Two points at the same Euclidean offset, one near origin
        // and one near boundary. Hyperbolic distance should be larger
        // for the boundary pair.
        let near_origin = (PoincarePoint::new(0.0, 0.0), PoincarePoint::new(0.1, 0.0));
        let near_boundary = (PoincarePoint::new(0.85, 0.0), PoincarePoint::new(0.95, 0.0));
        let d1 = poincare_distance(&near_origin.0, &near_origin.1);
        let d2 = poincare_distance(&near_boundary.0, &near_boundary.1);
        assert!(
            d2 > d1,
            "boundary-side distance {d2} should exceed origin-side {d1}"
        );
    }

    #[test]
    fn distance_is_nonnegative() {
        let u = PoincarePoint::new(0.1, 0.3);
        let v = PoincarePoint::new(0.4, -0.2);
        assert!(poincare_distance(&u, &v) >= 0.0);
    }

    #[test]
    fn clip_keeps_inside_disk() {
        let mut p = PoincarePoint::new(0.99999, 0.0);
        poincare_clip(&mut p);
        assert!(poincare_norm(&p) < 1.0);
    }

    #[test]
    fn clip_no_op_for_interior() {
        let mut p = PoincarePoint::new(0.5, 0.3);
        let before = p;
        poincare_clip(&mut p);
        assert_eq!(p, before);
    }

    #[test]
    fn clip_origin_stable() {
        let mut p = PoincarePoint::origin();
        poincare_clip(&mut p);
        assert_eq!(p, PoincarePoint::origin());
    }

    #[test]
    fn conformal_factor_one_at_origin_is_four() {
        // At origin, (2 / (1 - 0))² = 4.
        let c = poincare_conformal_factor(&PoincarePoint::origin());
        assert!((c - 4.0).abs() < 1e-5);
    }

    #[test]
    fn conformal_factor_grows_toward_boundary() {
        let c_origin = poincare_conformal_factor(&PoincarePoint::origin());
        let c_edge = poincare_conformal_factor(&PoincarePoint::new(0.9, 0.0));
        assert!(c_edge > c_origin);
    }

    #[test]
    fn triangle_inequality() {
        // d(u, w) ≤ d(u, v) + d(v, w) in any metric.
        let u = PoincarePoint::new(-0.3, 0.0);
        let v = PoincarePoint::new(0.1, 0.2);
        let w = PoincarePoint::new(0.4, -0.1);
        let duv = poincare_distance(&u, &v);
        let dvw = poincare_distance(&v, &w);
        let duw = poincare_distance(&u, &w);
        assert!(duw <= duv + dvw + 1e-4);
    }
}

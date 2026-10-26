//! NIMCP V2 — ternary (-1/0/+1) logic + packed storage.
//!
//! Port of V1's `utils/ternary/` suite. Each value is a **trit** —
//! three-valued: negative / zero / positive — biologically grounded:
//!
//! - `Negative` — inhibitory, LTD, FORBID, reject, DISAGREE
//! - `Zero`     — silent / unknown, subthreshold, NEUTRAL, abstain
//! - `Positive` — excitatory, LTP, ALLOW, accept, AGREE
//!
//! # Storage
//!
//! - [`Trit`] is a packed `enum` fitting in one byte; a `Vec<Trit>` is
//!   the unpacked form (1 byte / trit).
//! - [`TritPacked`] is a 2-bit-per-trit packed vector — 4 trits per
//!   byte, matching V1's `TERNARY_PACK_2BIT` mode.
//! - V1 also has a base-243 mode (5 trits per byte) for maximum
//!   compression; we skip it here — the marginal saving (200KB vs
//!   256KB per 1M trits) isn't worth the encoding complexity for V2.
//!
//! # Logic
//!
//! Three-valued Kleene logic — `Unknown AND True = Unknown`. Matches
//! V1's `trit_and` / `trit_or` / `trit_not` semantics.
//!
//! # Conversion from floats
//!
//! `from_floats(xs, threshold)` quantizes via `|x| < threshold → 0`,
//! `x < -threshold → -1`, `x > threshold → +1`. Used for weight
//! ternarization (V1 integration target: SNN → 20× memory saving).

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Three-valued truth / weight token. `#[repr(i8)]` so `as i8`
/// matches the numeric value exactly.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Serialize, Deserialize)]
#[repr(i8)]
pub enum Trit {
    /// Negative / inhibitory / reject. Numeric `-1`.
    Negative = -1,
    /// Zero / silent / neutral / unknown. Numeric `0`. V1 default.
    #[default]
    Zero = 0,
    /// Positive / excitatory / accept. Numeric `+1`.
    Positive = 1,
}

impl Trit {
    /// Cast to a numeric `-1 / 0 / 1`.
    #[must_use]
    #[inline]
    pub fn as_i8(self) -> i8 {
        self as i8
    }

    /// Cast to `f32 ∈ {-1.0, 0.0, 1.0}`.
    #[must_use]
    #[inline]
    pub fn as_f32(self) -> f32 {
        f32::from(self as i8)
    }

    /// Quantize a float to the nearest trit, with a dead-zone around
    /// zero of width `2 · threshold`.
    #[must_use]
    pub fn from_float(x: f32, threshold: f32) -> Self {
        if x > threshold {
            Trit::Positive
        } else if x < -threshold {
            Trit::Negative
        } else {
            Trit::Zero
        }
    }
}

// =========================================================================
// Kleene three-valued logic
// =========================================================================

/// Kleene AND: `min(a, b)` on `{-1, 0, +1}`. `Unknown AND True = Unknown`.
#[must_use]
#[inline]
pub fn trit_and(a: Trit, b: Trit) -> Trit {
    match a.as_i8().min(b.as_i8()) {
        -1 => Trit::Negative,
        1 => Trit::Positive,
        _ => Trit::Zero,
    }
}

/// Kleene OR: `max(a, b)`.
#[must_use]
#[inline]
pub fn trit_or(a: Trit, b: Trit) -> Trit {
    match a.as_i8().max(b.as_i8()) {
        -1 => Trit::Negative,
        1 => Trit::Positive,
        _ => Trit::Zero,
    }
}

/// NOT: flip sign; `Zero` → `Zero`.
#[must_use]
#[inline]
pub fn trit_not(a: Trit) -> Trit {
    match a {
        Trit::Positive => Trit::Negative,
        Trit::Negative => Trit::Positive,
        Trit::Zero => Trit::Zero,
    }
}

/// Łukasiewicz implication: `a → b = min(1, 1 − a + b)` on the
/// `{-1, 0, +1}` truth set — recoded to trits:
/// | a \ b   | Neg | Zero | Pos |
/// |---------|-----|------|-----|
/// | **Neg** | Pos | Pos  | Pos |
/// | **Zero**| Zero| Pos  | Pos |
/// | **Pos** | Neg | Zero | Pos |
#[must_use]
#[inline]
pub fn trit_implies(a: Trit, b: Trit) -> Trit {
    // Simple table — cleaner than the arithmetic form at f32 precision.
    match (a, b) {
        (Trit::Negative, _) => Trit::Positive,
        (Trit::Zero, Trit::Negative) => Trit::Zero,
        (Trit::Zero, _) => Trit::Positive,
        (Trit::Positive, Trit::Negative) => Trit::Negative,
        (Trit::Positive, Trit::Zero) => Trit::Zero,
        (Trit::Positive, Trit::Positive) => Trit::Positive,
    }
}

// =========================================================================
// Vector helpers (unpacked — 1 byte / trit)
// =========================================================================

/// Quantize a slice of floats to a `Vec<Trit>`.
#[must_use]
pub fn from_floats(xs: &[f32], threshold: f32) -> Vec<Trit> {
    xs.iter().map(|&x| Trit::from_float(x, threshold)).collect()
}

/// Dequantize a slice of trits to `f32` values in `{-1.0, 0.0, 1.0}`.
#[must_use]
pub fn to_floats(trits: &[Trit]) -> Vec<f32> {
    trits.iter().map(|t| t.as_f32()).collect()
}

/// Element-wise [`trit_and`] over two equal-length slices.
/// Returns `None` on length mismatch.
#[must_use]
pub fn vec_and(a: &[Trit], b: &[Trit]) -> Option<Vec<Trit>> {
    if a.len() != b.len() {
        return None;
    }
    Some(
        a.iter()
            .zip(b.iter())
            .map(|(&x, &y)| trit_and(x, y))
            .collect(),
    )
}

/// Element-wise [`trit_or`].
#[must_use]
pub fn vec_or(a: &[Trit], b: &[Trit]) -> Option<Vec<Trit>> {
    if a.len() != b.len() {
        return None;
    }
    Some(
        a.iter()
            .zip(b.iter())
            .map(|(&x, &y)| trit_or(x, y))
            .collect(),
    )
}

/// Dot product of two ternary vectors, treating trits as `-1/0/+1`.
/// Returns `None` on length mismatch.
#[must_use]
pub fn dot(a: &[Trit], b: &[Trit]) -> Option<i64> {
    if a.len() != b.len() {
        return None;
    }
    Some(
        a.iter()
            .zip(b.iter())
            .map(|(&x, &y)| i64::from(x.as_i8()) * i64::from(y.as_i8()))
            .sum(),
    )
}

// =========================================================================
// Packed storage — 2 bits per trit, 4 trits per byte
// =========================================================================

/// Construction / access errors for packed storage.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum TernaryError {
    /// Requested index is past the end of the vector.
    #[error("trit index {index} out of bounds for len {len}")]
    OutOfBounds {
        /// Offending index.
        index: usize,
        /// Logical length.
        len: usize,
    },
    /// Length mismatch between two operands.
    #[error("length mismatch: {a} vs {b}")]
    LengthMismatch {
        /// Left operand length.
        a: usize,
        /// Right operand length.
        b: usize,
    },
}

/// 2-bit-per-trit packed vector. 4 trits fit in each byte. Trits
/// beyond `len` in the final byte are padding (read as `Zero`).
///
/// Encoding:
/// - `Zero` = `0b00`
/// - `Positive` = `0b01`
/// - `Negative` = `0b10`
/// - `0b11` = reserved / invalid (never written by this crate)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TritPacked {
    /// Logical number of trits (≤ `4 × data.len()`).
    pub len: usize,
    /// Packed bytes. `data.len() == ceil(len / 4)`.
    pub data: Vec<u8>,
}

const POS: u8 = 0b01;
const NEG: u8 = 0b10;

impl TritPacked {
    /// All-zero packed vector of logical length `len`.
    #[must_use]
    pub fn zeros(len: usize) -> Self {
        let bytes = len.div_ceil(4);
        Self {
            len,
            data: vec![0; bytes],
        }
    }

    /// Pack a slice of trits.
    #[must_use]
    pub fn from_trits(trits: &[Trit]) -> Self {
        let mut p = Self::zeros(trits.len());
        for (i, &t) in trits.iter().enumerate() {
            p.set(i, t).expect("i < len by construction");
        }
        p
    }

    /// Unpack to a freshly-allocated `Vec<Trit>`.
    #[must_use]
    pub fn to_trits(&self) -> Vec<Trit> {
        (0..self.len)
            .map(|i| self.get(i).unwrap_or(Trit::Zero))
            .collect()
    }

    /// Get the `i`-th trit. Returns `TernaryError::OutOfBounds` for
    /// `i >= len`.
    pub fn get(&self, i: usize) -> Result<Trit, TernaryError> {
        if i >= self.len {
            return Err(TernaryError::OutOfBounds {
                index: i,
                len: self.len,
            });
        }
        let byte = self.data[i / 4];
        let shift = (i % 4) * 2;
        match (byte >> shift) & 0b11 {
            POS => Ok(Trit::Positive),
            NEG => Ok(Trit::Negative),
            _ => Ok(Trit::Zero),
        }
    }

    /// Set the `i`-th trit.
    pub fn set(&mut self, i: usize, t: Trit) -> Result<(), TernaryError> {
        if i >= self.len {
            return Err(TernaryError::OutOfBounds {
                index: i,
                len: self.len,
            });
        }
        let bits: u8 = match t {
            Trit::Positive => POS,
            Trit::Negative => NEG,
            Trit::Zero => 0b00,
        };
        let byte_idx = i / 4;
        let shift = (i % 4) * 2;
        self.data[byte_idx] &= !(0b11 << shift);
        self.data[byte_idx] |= bits << shift;
        Ok(())
    }

    /// Bytes occupied by the backing buffer (excluding struct overhead).
    #[must_use]
    pub fn nbytes(&self) -> usize {
        self.data.len()
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn trit_as_i8() {
        assert_eq!(Trit::Negative.as_i8(), -1);
        assert_eq!(Trit::Zero.as_i8(), 0);
        assert_eq!(Trit::Positive.as_i8(), 1);
    }

    #[test]
    fn trit_as_f32() {
        assert_eq!(Trit::Negative.as_f32(), -1.0);
        assert_eq!(Trit::Zero.as_f32(), 0.0);
        assert_eq!(Trit::Positive.as_f32(), 1.0);
    }

    #[test]
    fn from_float_threshold() {
        assert_eq!(Trit::from_float(0.5, 0.3), Trit::Positive);
        assert_eq!(Trit::from_float(-0.5, 0.3), Trit::Negative);
        assert_eq!(Trit::from_float(0.1, 0.3), Trit::Zero);
        assert_eq!(Trit::from_float(0.0, 0.3), Trit::Zero);
    }

    #[test]
    fn kleene_and_truth_table() {
        for a in [Trit::Negative, Trit::Zero, Trit::Positive] {
            for b in [Trit::Negative, Trit::Zero, Trit::Positive] {
                let r = trit_and(a, b);
                let expected_num = a.as_i8().min(b.as_i8());
                assert_eq!(r.as_i8(), expected_num);
            }
        }
    }

    #[test]
    fn kleene_or_truth_table() {
        for a in [Trit::Negative, Trit::Zero, Trit::Positive] {
            for b in [Trit::Negative, Trit::Zero, Trit::Positive] {
                let r = trit_or(a, b);
                let expected_num = a.as_i8().max(b.as_i8());
                assert_eq!(r.as_i8(), expected_num);
            }
        }
    }

    #[test]
    fn trit_not_flips_sign() {
        assert_eq!(trit_not(Trit::Positive), Trit::Negative);
        assert_eq!(trit_not(Trit::Negative), Trit::Positive);
        assert_eq!(trit_not(Trit::Zero), Trit::Zero);
    }

    #[test]
    fn trit_implies_negative_is_tautology() {
        // False → anything = True. In Łukasiewicz: -1 → x = +1.
        for b in [Trit::Negative, Trit::Zero, Trit::Positive] {
            assert_eq!(trit_implies(Trit::Negative, b), Trit::Positive);
        }
    }

    #[test]
    fn from_floats_batch() {
        let v = from_floats(&[0.5, -0.1, 0.0, -0.9], 0.3);
        assert_eq!(
            v,
            vec![Trit::Positive, Trit::Zero, Trit::Zero, Trit::Negative]
        );
    }

    #[test]
    fn to_floats_batch() {
        let f = to_floats(&[Trit::Positive, Trit::Zero, Trit::Negative]);
        assert_eq!(f, vec![1.0, 0.0, -1.0]);
    }

    #[test]
    fn vec_and_length_check() {
        assert!(vec_and(&[Trit::Positive], &[Trit::Zero, Trit::Zero]).is_none());
        assert_eq!(
            vec_and(
                &[Trit::Positive, Trit::Negative],
                &[Trit::Negative, Trit::Positive]
            ),
            Some(vec![Trit::Negative, Trit::Negative])
        );
    }

    #[test]
    fn dot_product_matches_i64_math() {
        // [+1, -1, 0, +1] · [-1, -1, 0, +1] = -1 + 1 + 0 + 1 = 1
        let a = [Trit::Positive, Trit::Negative, Trit::Zero, Trit::Positive];
        let b = [Trit::Negative, Trit::Negative, Trit::Zero, Trit::Positive];
        assert_eq!(dot(&a, &b), Some(1));
    }

    #[test]
    fn packed_round_trip() {
        let trits = [
            Trit::Positive,
            Trit::Zero,
            Trit::Negative,
            Trit::Positive,
            Trit::Negative,
            Trit::Zero,
            Trit::Zero,
        ];
        let p = TritPacked::from_trits(&trits);
        for (i, &t) in trits.iter().enumerate() {
            assert_eq!(p.get(i).unwrap(), t);
        }
        let unpacked = p.to_trits();
        assert_eq!(unpacked, trits);
    }

    #[test]
    fn packed_layout_4_per_byte() {
        let p = TritPacked::zeros(8);
        assert_eq!(p.data.len(), 2);
        let p = TritPacked::zeros(5);
        assert_eq!(p.data.len(), 2);
        let p = TritPacked::zeros(4);
        assert_eq!(p.data.len(), 1);
        let p = TritPacked::zeros(0);
        assert_eq!(p.data.len(), 0);
    }

    #[test]
    fn packed_set_overwrites_existing() {
        let mut p = TritPacked::zeros(4);
        p.set(0, Trit::Positive).unwrap();
        p.set(0, Trit::Negative).unwrap();
        assert_eq!(p.get(0).unwrap(), Trit::Negative);
    }

    #[test]
    fn packed_out_of_bounds_errors() {
        let mut p = TritPacked::zeros(3);
        assert!(p.get(3).is_err());
        assert!(p.set(3, Trit::Positive).is_err());
    }

    #[test]
    fn packed_memory_footprint() {
        let p = TritPacked::zeros(1_000_000);
        // 1M trits @ 2 bits/trit = 250_000 bytes (ceil)
        assert_eq!(p.nbytes(), 250_000);
    }
}

//! Detrended Fluctuation Analysis (DFA).
//!
//! Estimates the **scaling exponent α** of a time series. The
//! algorithm integrates the (zero-mean) signal, partitions the
//! integrated series into windows of size `s`, subtracts a local
//! linear trend in each window, and measures the RMS fluctuation
//! `F(s)`. On log-log axes, `F(s) ~ s^α`:
//!
//! - `α ≈ 0.5` — uncorrelated (white) noise.
//! - `α ≈ 1.0` — 1/f (pink) noise.
//! - `α ≈ 1.5` — integrated white = Brownian / random walk.
//! - `α < 0.5` — anti-correlated.
//! - `α > 1.5` — long-range persistent.
//!
//! V1's `src/plasticity/noise/nimcp_pink_noise_monitor.c` uses DFA
//! to verify that a pink-noise generator is actually producing 1/f
//! output online. The V2 port keeps the core algorithm; the online
//! monitor wrapper is deferred.

use thiserror::Error;

/// DFA errors.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum DfaError {
    /// Input too short to cover the requested window range.
    #[error("input len {len} < min_window {min_window}")]
    TooShort {
        /// Input length.
        len: usize,
        /// Minimum window size requested.
        min_window: usize,
    },
    /// `min_window >= max_window`.
    #[error("window range invalid: min={min} max={max}")]
    BadRange {
        /// min.
        min: usize,
        /// max.
        max: usize,
    },
}

/// Compute the DFA scaling exponent `α` for `signal` using window
/// sizes spaced geometrically between `min_window` and `max_window`.
///
/// Typical usage: `dfa_alpha(&samples, 4, samples.len() / 4)`.
///
/// Returns `DfaError` on degenerate input.
pub fn dfa_alpha(signal: &[f32], min_window: usize, max_window: usize) -> Result<f32, DfaError> {
    if min_window < 4 || max_window <= min_window {
        return Err(DfaError::BadRange {
            min: min_window,
            max: max_window,
        });
    }
    if signal.len() < min_window {
        return Err(DfaError::TooShort {
            len: signal.len(),
            min_window,
        });
    }

    // Step 1: integrate (cumulative sum of zero-mean signal).
    #[allow(clippy::cast_precision_loss)]
    let mean: f32 = signal.iter().sum::<f32>() / signal.len() as f32;
    let mut y: Vec<f32> = Vec::with_capacity(signal.len());
    let mut acc = 0.0_f32;
    for &s in signal {
        acc += s - mean;
        y.push(acc);
    }

    // Step 2: for each window size s, compute F(s).
    let mut log_s: Vec<f32> = Vec::new();
    let mut log_f: Vec<f32> = Vec::new();

    // Geometric window spacing: 2× per step.
    let mut s = min_window;
    while s <= max_window && s <= y.len() {
        let f = window_fluctuation(&y, s);
        if f > 0.0 {
            #[allow(clippy::cast_precision_loss)]
            let ls = (s as f32).ln();
            log_s.push(ls);
            log_f.push(f.ln());
        }
        s = (s as f32 * 1.5) as usize;
        if s <= log_s.last().map_or(0, |_| 0) {
            break;
        }
    }

    if log_s.len() < 2 {
        return Ok(0.0);
    }

    // Step 3: least-squares slope of log F vs log s → α.
    #[allow(clippy::cast_precision_loss)]
    let n = log_s.len() as f32;
    let sx: f32 = log_s.iter().sum();
    let sy: f32 = log_f.iter().sum();
    let sxy: f32 = log_s.iter().zip(log_f.iter()).map(|(&x, &y)| x * y).sum();
    let sxx: f32 = log_s.iter().map(|&x| x * x).sum();
    let denom = n * sxx - sx * sx;
    if denom.abs() < 1e-9 {
        return Ok(0.0);
    }
    Ok((n * sxy - sx * sy) / denom)
}

/// For one window size `s`, compute the RMS fluctuation of the
/// detrended integrated series.
fn window_fluctuation(y: &[f32], s: usize) -> f32 {
    let n_win = y.len() / s;
    if n_win == 0 {
        return 0.0;
    }
    let mut total_sq: f64 = 0.0;
    for w in 0..n_win {
        let start = w * s;
        let window = &y[start..start + s];
        // Fit linear trend y = a*k + b (least squares on k in [0, s)).
        #[allow(clippy::cast_precision_loss)]
        let n_f = s as f32;
        #[allow(clippy::cast_precision_loss)]
        let sx: f32 = ((s - 1) * s / 2) as f32; // Σk
        #[allow(clippy::cast_precision_loss)]
        let sxx: f32 = ((s - 1) * s * (2 * s - 1) / 6) as f32; // Σk²
        let mut sy_v: f32 = 0.0;
        let mut sxy_v: f32 = 0.0;
        for (k, &v) in window.iter().enumerate() {
            sy_v += v;
            #[allow(clippy::cast_precision_loss)]
            let k_f = k as f32;
            sxy_v += k_f * v;
        }
        let denom = n_f * sxx - sx * sx;
        let (a, b) = if denom.abs() < 1e-9 {
            (0.0, sy_v / n_f)
        } else {
            let a = (n_f * sxy_v - sx * sy_v) / denom;
            let b = (sy_v - a * sx) / n_f;
            (a, b)
        };
        // Sum squared residuals.
        for (k, &v) in window.iter().enumerate() {
            #[allow(clippy::cast_precision_loss)]
            let k_f = k as f32;
            let resid = v - (a * k_f + b);
            total_sq += f64::from(resid) * f64::from(resid);
        }
    }
    let total_len = (n_win * s) as f64;
    ((total_sq / total_len).sqrt()) as f32
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn bad_range_errors() {
        assert!(matches!(
            dfa_alpha(&[0.0; 100], 10, 5),
            Err(DfaError::BadRange { .. })
        ));
        assert!(matches!(
            dfa_alpha(&[0.0; 100], 2, 20),
            Err(DfaError::BadRange { .. })
        ));
    }

    #[test]
    fn short_input_errors() {
        assert!(matches!(
            dfa_alpha(&[0.0; 5], 10, 20),
            Err(DfaError::TooShort { .. })
        ));
    }

    #[test]
    fn constant_signal_is_well_defined() {
        // Constant signal has zero fluctuation → α is undefined.
        // Our implementation returns 0.0 in that degenerate case.
        let s = vec![1.0_f32; 1000];
        let a = dfa_alpha(&s, 4, 200).unwrap_or(0.0);
        assert!(a.is_finite());
    }

    #[test]
    fn white_noise_alpha_near_half() {
        // Independent uniform draws — α should be close to 0.5.
        use rand::{Rng, SeedableRng};
        let mut rng = rand_chacha::ChaCha20Rng::seed_from_u64(42);
        let samples: Vec<f32> = (0..8192).map(|_| rng.random::<f32>() - 0.5).collect();
        let a = dfa_alpha(&samples, 8, 512).unwrap();
        assert!(
            (a - 0.5).abs() < 0.15,
            "white noise α should be ~0.5, got {a}"
        );
    }

    #[test]
    fn brownian_alpha_near_one_point_five() {
        // Random walk (integrated white noise) → α ≈ 1.5.
        use rand::{Rng, SeedableRng};
        let mut rng = rand_chacha::ChaCha20Rng::seed_from_u64(7);
        let mut walk = 0.0_f32;
        let samples: Vec<f32> = (0..8192)
            .map(|_| {
                walk += rng.random::<f32>() - 0.5;
                walk
            })
            .collect();
        let a = dfa_alpha(&samples, 8, 512).unwrap();
        assert!((a - 1.5).abs() < 0.25, "Brownian α should be ~1.5, got {a}");
    }
}

#[cfg(test)]
mod dep_glue {
    // Pull in rand_chacha for dfa tests even though it's not a direct
    // crate dep. This dev-only import lives in the test module so the
    // release binary has no such symbol.
    #[allow(unused_imports)]
    use rand_chacha as _;
}

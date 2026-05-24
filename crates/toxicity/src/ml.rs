//! Char-trigram MLP head — port of V1's `nimcp_toxicity_ml.c`.
//!
//! Featurizes text as a length-normalized bag of FNV-hashed character
//! trigrams (`bin = fnv1a32(trigram) % 1024`), then a 1024→256→64→2 MLP
//! (ReLU, ReLU, sigmoid) predicts `(harm, fairness)`. Trains online with
//! MSE + SGD-momentum and a dead-zone (no update when both errors are
//! already small). The pattern classifier supplies the teacher labels.

use rand::SeedableRng;
use rand::distr::{Distribution, Uniform};
use rand_chacha::ChaCha20Rng;
use serde::{Deserialize, Serialize};

const INPUT: usize = 1024;
const H1: usize = 256;
const H2: usize = 64;
const OUTPUT: usize = 2;
const NGRAM: usize = 3;
const MOMENTUM: f32 = 0.9;

/// Prediction from the ML head.
#[derive(Debug, Clone, Copy, Default, PartialEq, Serialize, Deserialize)]
pub struct MlResult {
    pub predicted_harm: f32,
    pub fairness_violation: f32,
    /// Certainty: how far the outputs sit from the 0.5 decision line.
    pub confidence: f32,
}

/// 1024→256→64→2 MLP with momentum buffers.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MlClassifier {
    // Row-major weights `[out][in]` + biases.
    w1: Vec<f32>,
    b1: Vec<f32>,
    w2: Vec<f32>,
    b2: Vec<f32>,
    w3: Vec<f32>,
    b3: Vec<f32>,
    // Momentum buffers (parallel to weights/biases).
    mw1: Vec<f32>,
    mb1: Vec<f32>,
    mw2: Vec<f32>,
    mb2: Vec<f32>,
    mw3: Vec<f32>,
    mb3: Vec<f32>,
    /// EMA of recent training loss.
    pub recent_loss_ema: f32,
    pub train_steps: u64,
}

fn xavier(rng: &mut ChaCha20Rng, n_in: usize, n_out: usize, len: usize) -> Vec<f32> {
    #[allow(clippy::cast_precision_loss)]
    let bound = (6.0_f32 / (n_in + n_out) as f32).sqrt();
    let dist = Uniform::new_inclusive(-bound, bound).expect("valid range");
    (0..len).map(|_| dist.sample(rng)).collect()
}

#[inline]
fn relu(x: f32) -> f32 {
    x.max(0.0)
}

#[inline]
fn sigmoid(x: f32) -> f32 {
    1.0 / (1.0 + (-x).exp())
}

/// `out[o] = act(W·x + b)`; `W` is row-major `[n_out][n_in]`.
fn dense(w: &[f32], b: &[f32], x: &[f32], n_in: usize, n_out: usize, act: fn(f32) -> f32) -> Vec<f32> {
    let mut out = vec![0.0_f32; n_out];
    for o in 0..n_out {
        let row = &w[o * n_in..o * n_in + n_in];
        let mut acc = b[o];
        for (wij, &xi) in row.iter().zip(x.iter()) {
            acc += wij * xi;
        }
        out[o] = act(acc);
    }
    out
}

impl MlClassifier {
    /// New randomly-initialized classifier.
    #[must_use]
    pub fn new(seed: u64) -> Self {
        let mut rng = ChaCha20Rng::seed_from_u64(seed);
        Self {
            w1: xavier(&mut rng, INPUT, H1, H1 * INPUT),
            b1: vec![0.0; H1],
            w2: xavier(&mut rng, H1, H2, H2 * H1),
            b2: vec![0.0; H2],
            w3: xavier(&mut rng, H2, OUTPUT, OUTPUT * H2),
            b3: vec![0.0; OUTPUT],
            mw1: vec![0.0; H1 * INPUT],
            mb1: vec![0.0; H1],
            mw2: vec![0.0; H2 * H1],
            mb2: vec![0.0; H2],
            mw3: vec![0.0; OUTPUT * H2],
            mb3: vec![0.0; OUTPUT],
            recent_loss_ema: 0.0,
            train_steps: 0,
        }
    }

    /// Length-normalized FNV-hashed char-trigram bag (length `INPUT`).
    fn featurize(text: &str) -> Vec<f32> {
        let mut x = vec![0.0_f32; INPUT];
        let bytes: Vec<u8> = text.to_ascii_lowercase().into_bytes();
        if bytes.len() < NGRAM {
            return x;
        }
        let mut count = 0.0_f32;
        for w in bytes.windows(NGRAM) {
            let mut h = 0x811c_9dc5_u32;
            for &b in w {
                h ^= u32::from(b);
                h = h.wrapping_mul(0x0100_0193);
            }
            x[(h as usize) % INPUT] += 1.0;
            count += 1.0;
        }
        if count > 0.0 {
            for v in &mut x {
                *v /= count;
            }
        }
        x
    }

    /// Forward, returning `(harm, fairness)` + cached activations.
    fn forward(&self, x: &[f32]) -> (Vec<f32>, Vec<f32>, Vec<f32>) {
        let h1 = dense(&self.w1, &self.b1, x, INPUT, H1, relu);
        let h2 = dense(&self.w2, &self.b2, &h1, H1, H2, relu);
        let y = dense(&self.w3, &self.b3, &h2, H2, OUTPUT, sigmoid);
        (h1, h2, y)
    }

    /// Predict `(harm, fairness)` for `text`.
    #[must_use]
    pub fn predict(&self, text: &str) -> MlResult {
        let x = Self::featurize(text);
        let (_, _, y) = self.forward(&x);
        let conf = ((y[0] - 0.5).abs() + (y[1] - 0.5).abs()).min(1.0);
        MlResult {
            predicted_harm: y[0],
            fairness_violation: y[1],
            confidence: conf,
        }
    }

    /// One SGD-momentum step toward `(target_harm, target_fairness)`.
    /// Skips the update (dead-zone) when both errors are `≤ dead_zone`.
    /// Returns the pre-step MSE.
    pub fn train_step(
        &mut self,
        text: &str,
        target_harm: f32,
        target_fairness: f32,
        lr: f32,
        dead_zone: f32,
    ) -> f32 {
        let x = Self::featurize(text);
        let (h1, h2, y) = self.forward(&x);
        let t = [target_harm, target_fairness];
        let err = [y[0] - t[0], y[1] - t[1]];
        let loss = 0.5 * (err[0] * err[0] + err[1] * err[1]);

        // Dead-zone: already close enough on both channels → no update.
        if err[0].abs() <= dead_zone && err[1].abs() <= dead_zone {
            return loss;
        }

        // Output layer: dz3 = (y - t) · y(1-y).
        let mut dz3 = [0.0_f32; OUTPUT];
        for o in 0..OUTPUT {
            dz3[o] = err[o] * y[o] * (1.0 - y[o]);
        }
        // Hidden-2 grad: dh2 = W3^T dz3; dz2 = dh2 · relu'(h2).
        let mut dz2 = vec![0.0_f32; H2];
        for (j, dz2j) in dz2.iter_mut().enumerate() {
            let mut acc = 0.0;
            for (o, &dz3o) in dz3.iter().enumerate() {
                acc += self.w3[o * H2 + j] * dz3o;
            }
            *dz2j = if h2[j] > 0.0 { acc } else { 0.0 };
        }
        // Hidden-1 grad.
        let mut dz1 = vec![0.0_f32; H1];
        for (k, dz1k) in dz1.iter_mut().enumerate() {
            let mut acc = 0.0;
            for (j, &dz2j) in dz2.iter().enumerate() {
                acc += self.w2[j * H1 + k] * dz2j;
            }
            *dz1k = if h1[k] > 0.0 { acc } else { 0.0 };
        }

        // Apply momentum-SGD: m = β·m + grad; w -= lr·m.
        apply_dense(&mut self.w3, &mut self.mw3, &mut self.b3, &mut self.mb3, &dz3, &h2, H2, OUTPUT, lr);
        apply_dense(&mut self.w2, &mut self.mw2, &mut self.b2, &mut self.mb2, &dz2, &h1, H1, H2, lr);
        apply_dense(&mut self.w1, &mut self.mw1, &mut self.b1, &mut self.mb1, &dz1, &x, INPUT, H1, lr);

        self.train_steps += 1;
        self.recent_loss_ema = if self.train_steps == 1 {
            loss
        } else {
            0.99 * self.recent_loss_ema + 0.01 * loss
        };
        loss
    }
}

/// Apply one momentum-SGD update to a dense layer: for each output `o`,
/// `m[o,i] = β·m[o,i] + dz[o]·x[i]; w[o,i] -= lr·m[o,i]` (bias uses `x≡1`).
#[allow(clippy::too_many_arguments)]
fn apply_dense(
    w: &mut [f32],
    mw: &mut [f32],
    b: &mut [f32],
    mb: &mut [f32],
    dz: &[f32],
    x: &[f32],
    n_in: usize,
    n_out: usize,
    lr: f32,
) {
    for o in 0..n_out {
        let base = o * n_in;
        for i in 0..n_in {
            let g = dz[o] * x[i];
            let m = MOMENTUM * mw[base + i] + g;
            mw[base + i] = m;
            w[base + i] -= lr * m;
        }
        let mbias = MOMENTUM * mb[o] + dz[o];
        mb[o] = mbias;
        b[o] -= lr * mbias;
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn predict_outputs_in_unit_range() {
        let c = MlClassifier::new(1);
        let r = c.predict("hello world this is a sentence");
        assert!((0.0..=1.0).contains(&r.predicted_harm));
        assert!((0.0..=1.0).contains(&r.fairness_violation));
    }

    #[test]
    fn featurize_is_length_normalized() {
        let x = MlClassifier::featurize("aaaa");
        let sum: f32 = x.iter().sum();
        // Two trigrams ("aaa","aaa") → same bin, normalized → sums to 1.
        assert!((sum - 1.0).abs() < 1e-5);
    }

    #[test]
    fn short_text_is_zero_features() {
        let x = MlClassifier::featurize("ab");
        assert!(x.iter().all(|&v| v == 0.0));
    }

    #[test]
    fn learns_to_separate_toxic_from_benign() {
        let mut c = MlClassifier::new(42);
        let toxic = "kill all of them they are vermin and subhuman scum";
        let benign = "the children played happily in the sunny green park";
        for _ in 0..400 {
            c.train_step(toxic, 1.0, 1.0, 0.05, 0.02);
            c.train_step(benign, 0.0, 0.0, 0.05, 0.02);
        }
        let pt = c.predict(toxic);
        let pb = c.predict(benign);
        assert!(
            pt.predicted_harm > pb.predicted_harm + 0.3,
            "toxic {} should outscore benign {}",
            pt.predicted_harm,
            pb.predicted_harm
        );
        assert!(pt.predicted_harm > 0.5);
        assert!(pb.predicted_harm < 0.5);
    }

    #[test]
    fn dead_zone_skips_update() {
        let mut c = MlClassifier::new(7);
        // Train to roughly fit first.
        for _ in 0..50 {
            c.train_step("benign text here", 0.0, 0.0, 0.05, 0.0);
        }
        let steps_before = c.train_steps;
        // Huge dead-zone → no update regardless of error.
        c.train_step("benign text here", 0.0, 0.0, 0.05, 1.0);
        assert_eq!(c.train_steps, steps_before, "dead-zone should skip");
    }

    #[test]
    fn serde_round_trip_preserves_prediction() {
        let mut c = MlClassifier::new(3);
        for _ in 0..20 {
            c.train_step("kill them all", 1.0, 1.0, 0.05, 0.0);
        }
        let json = serde_json::to_string(&c).unwrap();
        let back: MlClassifier = serde_json::from_str(&json).unwrap();
        let a = c.predict("kill them all");
        let b = back.predict("kill them all");
        assert_eq!(a.predicted_harm, b.predicted_harm);
    }
}

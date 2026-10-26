//! NIMCP V2 — Python bindings.
//!
//! Phase 1 surface:
//!
//! - `Brain(rng_seed, deterministic, layers, activation)` — construct.
//! - `brain.learn(x, y, lr) -> float` — one SGD step against MSE.
//! - `brain.predict(x) -> list[float]` — forward pass.
//! - `brain.save(path)` / `brain.load(path)` — rkyv checkpoint round-trip.
//!
//! Phase 6b surface (introspection):
//!
//! - `brain.stats() -> dict` — nested dict with one section per configured
//!   subsystem (`adaptive`, `snn`, `lnn`, `memory`). Sections for
//!   subsystems the brain wasn't built with are `None`.
//! - `brain.stats_json() -> str` — same payload as `stats()` but as the
//!   raw JSON string, for callers that want to log / pipe / save.
//!
//! Phase 7b surface (full ensemble + memory + ensemble checkpoint):
//!
//! - `Brain.from_json(config_json: str) -> Brain` — alternative constructor
//!   accepting a full `BrainConfig` as JSON. Use this to build a brain
//!   with SNN, LNN, and/or memory configured. Hand-writing the JSON is
//!   straightforward; the adapter shim builds it from harness configs.
//! - `brain.snn_step(drive, reward, dt_ms) -> int` — one SNN integration
//!   step. `drive` is `list[list[float]]` — one current vector per
//!   population. Returns total spikes this step.
//! - `brain.lnn_reset()` — clear LNN transient state.
//! - `brain.lnn_forward_step(input) -> list[float]` — one sample.
//! - `brain.lnn_forward_sequence(inputs) -> list[list[float]]` — multi-sample.
//! - `brain.lnn_train_step_mse(inputs, targets, lr, grad_clip) -> (loss, grad_norm)`
//! - `brain.memory_insert(id, features, t_ms, salience=0.0)`
//! - `brain.memory_mark_landmark(id, reason)`
//! - `brain.memory_query_all(query, k) -> list[dict]`
//! - `brain.memory_query_landmarks(query, k) -> list[dict]`
//! - `brain.memory_consolidate(dt_seconds) -> (promotions, demotions, evictions)`
//! - `brain.save_ensemble(dir)` / `brain.load_ensemble(dir)` — atomic
//!   directory checkpoint covering every configured subsystem.
//!
//! Language (L8) + cognition (C4/C5) surface — every method raises
//! `RuntimeError` on a brain built without `language` in its config:
//!
//! - `brain.has_language` / `brain.has_toxicity` — getters.
//! - `brain.language_ground(word, features, modality) -> int` — bind a word to
//!   a grounded feature vector; `modality` ∈ visual/auditory/motor/emotional/
//!   spatial/linguistic. Returns the concept id.
//! - `brain.language_learn(text)` — distributional + n-gram learning
//!   (mark-not-filter for toxic text).
//! - `brain.language_comprehend(text) -> (list[float], float)` — semantic
//!   vector + confidence; advances the discourse ring.
//! - `brain.language_respond(text) -> dict` — toxicity gate → cascade (with
//!   working memory / imagination / reasoning sources) → output gate.
//!   Keys: `text`, `confidence`, `blocked_for_toxicity`, `toxicity_category`.
//! - `brain.reason(query) -> dict` — lexicon conclusion enriched by Z-Ladder
//!   recall (`conclusion`, `lexicon_confidence`, `memory_hits`).
//! - `brain.reason_in_content` getter / `brain.set_reason_in_content(on)`.
//! - `brain.classify_toxicity(text) -> (harm, fairness, would_block)`.
//! - `brain.language_stats() -> dict` — learn/comprehend counters, vocab size,
//!   discourse depth.
//! - `brain.working_memory_add(data, salience)` / `brain.working_memory_items()
//!   -> list[dict]` — C5 working memory (semantic-dim vectors).
//! - `brain.imagination_stage(content, vividness)` /
//!   `brain.imagination_active() -> (list[float], float) | None`.
//! - `brain.cognition_now_ms` getter — virtual cognition clock.
//!
//! Built with `maturin develop` for editable installs.

#![allow(unsafe_code)] // pyo3 macros expand to unsafe
#![allow(unsafe_op_in_unsafe_fn)] // pyo3 macros
// PyO3 extractors produce Rust-owned values (String, Vec<T>) that we
// consume once and drop. clippy::needless_pass_by_value is a false
// positive for bindings.
#![allow(clippy::needless_pass_by_value)]

use ndarray::Array1;
use nimcp_adaptive::{Activation, AdaptiveConfig};
use nimcp_brain::{Backend, Brain, BrainConfig};
use nimcp_language::Modality;
use nimcp_lnn::TrainParams;
use nimcp_memory::MemoryNode;
use pyo3::exceptions::{PyRuntimeError, PyValueError};
use pyo3::prelude::*;
use std::path::PathBuf;

/// Python-visible brain handle.
#[pyclass(name = "Brain", module = "nimcp_v2")]
struct PyBrain {
    inner: Brain,
}

fn parse_activation(s: &str) -> PyResult<Activation> {
    match s.to_ascii_lowercase().as_str() {
        "relu" => Ok(Activation::Relu),
        "tanh" => Ok(Activation::Tanh),
        other => Err(PyValueError::new_err(format!(
            "unknown activation '{other}', expected 'relu' or 'tanh'"
        ))),
    }
}

fn parse_backend(s: &str) -> PyResult<Backend> {
    let lower = s.to_ascii_lowercase();
    match lower.as_str() {
        "cpu" => Ok(Backend::Cpu),
        "gpu" => Ok(Backend::Gpu),
        _ => {
            // "gpu:0,1" → MultiGpu over those ordinals.
            let Some(list) = lower.strip_prefix("gpu:") else {
                return Err(PyValueError::new_err(format!(
                    "unknown backend '{s}', expected 'cpu', 'gpu' or 'gpu:<ord>,<ord>,...'"
                )));
            };
            let devices = list
                .split(',')
                .map(|d| d.trim().parse::<u32>())
                .collect::<std::result::Result<Vec<u32>, _>>()
                .map_err(|e| {
                    PyValueError::new_err(format!("backend '{s}': bad device ordinal: {e}"))
                })?;
            if devices.is_empty() {
                return Err(PyValueError::new_err(format!(
                    "backend '{s}': device list is empty"
                )));
            }
            Ok(Backend::MultiGpu { devices })
        }
    }
}

fn backend_to_string(b: &Backend) -> String {
    match b {
        Backend::Cpu => "cpu".into(),
        Backend::Gpu => "gpu".into(),
        Backend::MultiGpu { devices } => format!(
            "gpu:{}",
            devices.iter().map(u32::to_string).collect::<Vec<_>>().join(",")
        ),
    }
}

fn parse_modality(s: &str) -> PyResult<Modality> {
    match s.to_ascii_lowercase().as_str() {
        "visual" => Ok(Modality::Visual),
        "auditory" => Ok(Modality::Auditory),
        "motor" => Ok(Modality::Motor),
        "emotional" => Ok(Modality::Emotional),
        "spatial" => Ok(Modality::Spatial),
        "linguistic" => Ok(Modality::Linguistic),
        other => Err(PyValueError::new_err(format!(
            "unknown modality '{other}', expected visual/auditory/motor/emotional/spatial/linguistic"
        ))),
    }
}

/// Serialize any `Serialize` value and hand it to Python as a native
/// object via `json.loads` — keeps the Rust→Python schema identical to
/// the daemon's JSON wire form.
fn to_py_json<'py, T: serde::Serialize>(py: Python<'py>, v: &T) -> PyResult<Bound<'py, PyAny>> {
    let json = serde_json::to_string(v).map_err(|e| PyRuntimeError::new_err(format!("encode: {e}")))?;
    py.import("json")?.call_method1("loads", (json,))
}

fn shape_err<E: std::fmt::Display>(e: E) -> PyErr {
    PyValueError::new_err(format!("{e}"))
}

fn rt_err<E: std::fmt::Display>(e: E) -> PyErr {
    PyRuntimeError::new_err(format!("{e}"))
}

#[pymethods]
impl PyBrain {
    /// Create a new brain.
    ///
    /// Args:
    ///     rng_seed (int): seed for deterministic init. Default: 0x5EED.
    ///     deterministic (bool): run in deterministic mode. Default: False.
    ///     layers (list[int]): MLP widths including input + output. Default: [64, 32, 10].
    ///     activation (str): 'relu' or 'tanh' for hidden activations. Default: 'relu'.
    ///     backend (str): execution backend: 'cpu' (default), 'gpu', or
    ///         'gpu:0,1' (Phase 10g — SNN populations spread over those
    ///         CUDA device ordinals). GPU backends force SNN's
    ///         `use_gpu_forward` on and call LNN's `enable_gpu()`. On a
    ///         CPU-only build (no `--features cuda`) they degrade to CPU
    ///         with a warning rather than failing.
    #[new]
    #[pyo3(signature = (
        rng_seed = 0x5EED,
        deterministic = false,
        layers = None,
        activation = "relu".to_string(),
        backend = "cpu".to_string(),
    ))]
    fn new(
        rng_seed: u64,
        deterministic: bool,
        layers: Option<Vec<usize>>,
        activation: String,
        backend: String,
    ) -> PyResult<Self> {
        let act = parse_activation(&activation)?;
        let backend_enum = parse_backend(&backend)?;
        let adaptive = AdaptiveConfig {
            layers: layers.unwrap_or_else(|| vec![64, 32, 10]),
            rng_seed,
            activation: act,
        };
        if adaptive.layers.len() < 2 {
            return Err(PyValueError::new_err(
                "layers must have at least 2 entries (input + output)",
            ));
        }
        let cfg = BrainConfig {
            rng_seed,
            deterministic,
            adaptive,
            backend: backend_enum,
            ..Default::default()
        };
        let inner = Brain::new(cfg).map_err(rt_err)?;
        Ok(Self { inner })
    }

    /// Currently-active backend, as a string: `"cpu"`, `"gpu"`, or
    /// `"gpu:0,1"`. Mirrors the `backend` arg passed to the constructor.
    #[getter]
    fn backend(&self) -> String {
        backend_to_string(&self.inner.config().backend)
    }

    /// One gradient step; returns pre-update MSE loss.
    ///
    /// Args:
    ///     features (list[float]): input vector, len == layers[0].
    ///     target   (list[float]): target vector, len == layers[-1].
    ///     lr       (float, optional): learning rate. Default: 0.01.
    #[pyo3(signature = (features, target, lr = 0.01))]
    fn learn(&mut self, features: Vec<f32>, target: Vec<f32>, lr: f32) -> PyResult<f32> {
        let cfg = self.inner.config();
        let expected_in = cfg.adaptive.layers[0];
        let expected_out = cfg.adaptive.layers[cfg.adaptive.layers.len() - 1];
        if features.len() != expected_in {
            return Err(shape_err(format!(
                "features len {} != input layer {}",
                features.len(),
                expected_in
            )));
        }
        if target.len() != expected_out {
            return Err(shape_err(format!(
                "target len {} != output layer {}",
                target.len(),
                expected_out
            )));
        }
        let x = Array1::from_vec(features);
        let y = Array1::from_vec(target);
        Ok(self.inner.learn(&x, &y, lr))
    }

    /// Forward pass.
    ///
    /// Args:
    ///     features (list[float]): input vector, len == layers[0].
    /// Returns:
    ///     list[float]: output of the (linear) final layer.
    fn predict(&self, features: Vec<f32>) -> PyResult<Vec<f32>> {
        let cfg = self.inner.config();
        let expected_in = cfg.adaptive.layers[0];
        if features.len() != expected_in {
            return Err(shape_err(format!(
                "features len {} != input layer {}",
                features.len(),
                expected_in
            )));
        }
        let x = Array1::from_vec(features);
        Ok(self.inner.predict(&x).to_vec())
    }

    /// Save the brain's weights to `path`. Phase 1: rkyv over adaptive net.
    fn save(&self, path: PathBuf) -> PyResult<()> {
        self.inner.save(&path).map_err(rt_err)
    }

    /// Reload weights from a previous `save`. Layer shapes must match the
    /// brain's current config.
    fn load(&mut self, path: PathBuf) -> PyResult<()> {
        self.inner.load(&path).map_err(rt_err)
    }

    /// Tuple of (input_dim, output_dim) read from config.
    fn shape(&self) -> (usize, usize) {
        let layers = &self.inner.config().adaptive.layers;
        (layers[0], layers[layers.len() - 1])
    }

    /// Comprehensive read-only stats snapshot across every configured
    /// subsystem. Returns a nested Python dict with sections
    /// `rng_seed`, `adaptive`, `snn`, `lnn`, `memory`. Sections for
    /// subsystems not configured on this brain are `None`.
    ///
    /// Cheap — linear in parameter / node count — but not free. Callers
    /// polling on a tight loop should throttle.
    ///
    /// Returns:
    ///     dict: see `nimcp_brain::stats::BrainStats` for the schema.
    fn stats<'py>(&self, py: Python<'py>) -> PyResult<Bound<'py, PyAny>> {
        let json = self.inner.stats_json().map_err(rt_err)?;
        let json_mod = py.import("json")?;
        json_mod.call_method1("loads", (json,))
    }

    /// Same payload as `stats()` but returned as a raw JSON string.
    /// Faster than `stats()` when the caller is going to write it to
    /// disk / a socket / a log without ever touching the structure.
    fn stats_json(&self) -> PyResult<String> {
        self.inner.stats_json().map_err(rt_err)
    }

    // -------------------------------------------------------------------------
    // Phase 7b — full-ensemble constructor + SNN / LNN / memory surface.
    // -------------------------------------------------------------------------

    /// Alternative constructor accepting a full `BrainConfig` as JSON.
    /// The Python caller builds a dict mirroring the `BrainConfig`
    /// struct and hands `json.dumps(cfg)`; use this path to enable
    /// SNN / LNN / memory on the brain.
    ///
    /// Args:
    ///     config_json (str): JSON serialization of `BrainConfig`.
    /// Returns:
    ///     Brain
    ///
    /// Raises:
    ///     ValueError: JSON didn't decode to a `BrainConfig`.
    ///     RuntimeError: Brain construction failed (shape / config errors).
    #[staticmethod]
    fn from_json(config_json: &str) -> PyResult<Self> {
        let cfg: BrainConfig = serde_json::from_str(config_json)
            .map_err(|e| PyValueError::new_err(format!("BrainConfig JSON decode: {e}")))?;
        let inner = Brain::new(cfg).map_err(rt_err)?;
        Ok(Self { inner })
    }

    /// One SNN integration step.
    ///
    /// Args:
    ///     drive  (list[list[float]]): external `I_syn` current per population.
    ///         Outer list length must equal the number of populations; each
    ///         inner list is either empty (no external drive for that
    ///         population) or length-equal to the population size.
    ///     reward (float): global reward signal for R-STDP.
    ///     dt_ms  (float): integration step in milliseconds.
    /// Returns:
    ///     int: total spikes emitted this step.
    fn snn_step(&mut self, drive: Vec<Vec<f32>>, reward: f32, dt_ms: f32) -> PyResult<u32> {
        let slices: Vec<&[f32]> = drive.iter().map(Vec::as_slice).collect();
        self.inner.snn_step(&slices, reward, dt_ms).map_err(rt_err)
    }

    /// Reset the LNN's transient state to zeros. No-op on brains without LNN.
    fn lnn_reset(&mut self) {
        self.inner.lnn_reset();
    }

    /// Step the LNN forward one sample; returns readout.
    /// State carries across calls; use `lnn_reset()` to start a fresh sequence.
    fn lnn_forward_step(&mut self, input: Vec<f32>) -> PyResult<Vec<f32>> {
        let x = Array1::from_vec(input);
        self.inner
            .lnn_forward_step(&x)
            .map(|out| out.to_vec())
            .map_err(rt_err)
    }

    /// Run the LNN over a full sequence (resets state first).
    /// Returns per-step readouts.
    fn lnn_forward_sequence(&mut self, inputs: Vec<Vec<f32>>) -> PyResult<Vec<Vec<f32>>> {
        let arrays: Vec<Array1<f32>> = inputs.into_iter().map(Array1::from_vec).collect();
        self.inner
            .lnn_forward_sequence(&arrays)
            .map(|outs| outs.into_iter().map(|a| a.to_vec()).collect())
            .map_err(rt_err)
    }

    /// One LNN training step (MSE over sequence). Returns `(loss, grad_norm)`.
    #[pyo3(signature = (inputs, targets, lr = 0.01, grad_clip = 1.0))]
    fn lnn_train_step_mse(
        &mut self,
        inputs: Vec<Vec<f32>>,
        targets: Vec<Vec<f32>>,
        lr: f32,
        grad_clip: f32,
    ) -> PyResult<(f32, f32)> {
        if inputs.len() != targets.len() {
            return Err(shape_err(format!(
                "len(inputs)={} != len(targets)={}",
                inputs.len(),
                targets.len()
            )));
        }
        let inputs_a: Vec<Array1<f32>> = inputs.into_iter().map(Array1::from_vec).collect();
        let targets_a: Vec<Array1<f32>> = targets.into_iter().map(Array1::from_vec).collect();
        let params = TrainParams { lr, grad_clip };
        self.inner
            .lnn_train_step_mse(&inputs_a, &targets_a, &params)
            .map_err(rt_err)
    }

    // -------------------------------------------------------------------------
    // Phase 11 — CNN / FNO / HNN.
    //
    // CNN forward expects a 4-D `[batch, channels, H, W]` input passed
    // as a nested list. The output is `[batch, output_dim]`. FNO is the
    // 3-D analogue (`[batch, channels, length]`). HNN exposes set-state
    // / step / energy.
    // -------------------------------------------------------------------------

    /// CNN forward pass.
    ///
    /// Args:
    ///     input: nested list shaped `[batch][channels][H][W]`.
    /// Returns:
    ///     list[list[float]]: `[batch][output_dim]`.
    fn cnn_predict(&self, input: Vec<Vec<Vec<Vec<f32>>>>) -> PyResult<Vec<Vec<f32>>> {
        let n = input.len();
        if n == 0 {
            return Err(shape_err("cnn_predict: empty batch"));
        }
        let c = input[0].len();
        if c == 0 {
            return Err(shape_err("cnn_predict: zero channels"));
        }
        let h = input[0][0].len();
        if h == 0 {
            return Err(shape_err("cnn_predict: zero height"));
        }
        let w = input[0][0][0].len();
        if w == 0 {
            return Err(shape_err("cnn_predict: zero width"));
        }

        let mut flat: Vec<f32> = Vec::with_capacity(n * c * h * w);
        for batch in &input {
            if batch.len() != c {
                return Err(shape_err("cnn_predict: ragged channel count"));
            }
            for chan in batch {
                if chan.len() != h {
                    return Err(shape_err("cnn_predict: ragged height"));
                }
                for row in chan {
                    if row.len() != w {
                        return Err(shape_err("cnn_predict: ragged width"));
                    }
                    flat.extend_from_slice(row);
                }
            }
        }
        let arr = ndarray::Array4::from_shape_vec((n, c, h, w), flat)
            .map_err(|e| shape_err(format!("cnn_predict shape: {e}")))?;
        let out = self.inner.cnn_predict(&arr).map_err(rt_err)?;
        let (rows, cols) = out.dim();
        let mut py_out: Vec<Vec<f32>> = Vec::with_capacity(rows);
        for r in 0..rows {
            let mut row = Vec::with_capacity(cols);
            for c in 0..cols {
                row.push(out[[r, c]]);
            }
            py_out.push(row);
        }
        Ok(py_out)
    }

    /// One CNN training step (MSE) — Phase 11a-train.
    ///
    /// Args:
    ///     input: nested list `[batch][channels][H][W]`.
    ///     target: nested list `[batch][output_dim]`.
    ///     lr: learning rate.
    /// Returns:
    ///     (loss, grad_norm)
    fn cnn_train_step_mse(
        &mut self,
        input: Vec<Vec<Vec<Vec<f32>>>>,
        target: Vec<Vec<f32>>,
        lr: f32,
    ) -> PyResult<(f32, f32)> {
        let n = input.len();
        if n == 0 {
            return Err(shape_err("cnn_train_step_mse: empty batch"));
        }
        let c = input[0].len();
        let h = input[0][0].len();
        let w = input[0][0][0].len();

        let mut flat: Vec<f32> = Vec::with_capacity(n * c * h * w);
        for batch in &input {
            if batch.len() != c {
                return Err(shape_err("cnn_train: ragged channel count"));
            }
            for chan in batch {
                if chan.len() != h {
                    return Err(shape_err("cnn_train: ragged height"));
                }
                for row in chan {
                    if row.len() != w {
                        return Err(shape_err("cnn_train: ragged width"));
                    }
                    flat.extend_from_slice(row);
                }
            }
        }
        let arr = ndarray::Array4::from_shape_vec((n, c, h, w), flat)
            .map_err(|e| shape_err(format!("cnn_train shape: {e}")))?;

        if target.len() != n {
            return Err(shape_err("cnn_train: target batch != input batch"));
        }
        let k = target[0].len();
        let mut t_flat: Vec<f32> = Vec::with_capacity(n * k);
        for row in &target {
            if row.len() != k {
                return Err(shape_err("cnn_train: ragged target"));
            }
            t_flat.extend_from_slice(row);
        }
        let tgt = ndarray::Array2::from_shape_vec((n, k), t_flat)
            .map_err(|e| shape_err(format!("cnn_train target shape: {e}")))?;

        self.inner
            .cnn_train_step_mse(&arr, &tgt, lr)
            .map_err(rt_err)
    }

    /// FNO forward pass.
    ///
    /// Args:
    ///     input: nested list shaped `[batch][channels][length]`.
    /// Returns:
    ///     list[list[list[float]]]: `[batch][out_channels][length]`.
    fn fno_predict(&self, input: Vec<Vec<Vec<f32>>>) -> PyResult<Vec<Vec<Vec<f32>>>> {
        let n = input.len();
        if n == 0 {
            return Err(shape_err("fno_predict: empty batch"));
        }
        let c = input[0].len();
        if c == 0 {
            return Err(shape_err("fno_predict: zero channels"));
        }
        let l = input[0][0].len();
        if l == 0 {
            return Err(shape_err("fno_predict: zero length"));
        }
        let mut flat: Vec<f32> = Vec::with_capacity(n * c * l);
        for batch in &input {
            if batch.len() != c {
                return Err(shape_err("fno_predict: ragged channel count"));
            }
            for chan in batch {
                if chan.len() != l {
                    return Err(shape_err("fno_predict: ragged length"));
                }
                flat.extend_from_slice(chan);
            }
        }
        let arr = ndarray::Array3::from_shape_vec((n, c, l), flat)
            .map_err(|e| shape_err(format!("fno_predict shape: {e}")))?;
        let out = self.inner.fno_predict(&arr).map_err(rt_err)?;
        let (n_o, c_o, l_o) = out.dim();
        let mut py_out: Vec<Vec<Vec<f32>>> = Vec::with_capacity(n_o);
        for ni in 0..n_o {
            let mut chans: Vec<Vec<f32>> = Vec::with_capacity(c_o);
            for ci in 0..c_o {
                let mut row = Vec::with_capacity(l_o);
                for li in 0..l_o {
                    row.push(out[[ni, ci, li]]);
                }
                chans.push(row);
            }
            py_out.push(chans);
        }
        Ok(py_out)
    }

    /// One FNO training step (MSE) — Phase 11b-train.
    fn fno_train_step_mse(
        &mut self,
        input: Vec<Vec<Vec<f32>>>,
        target: Vec<Vec<Vec<f32>>>,
        lr: f32,
    ) -> PyResult<(f32, f32)> {
        let n = input.len();
        if n == 0 {
            return Err(shape_err("fno_train: empty batch"));
        }
        let c = input[0].len();
        let l = input[0][0].len();
        let mut flat: Vec<f32> = Vec::with_capacity(n * c * l);
        for batch in &input {
            if batch.len() != c {
                return Err(shape_err("fno_train: ragged channel count"));
            }
            for chan in batch {
                if chan.len() != l {
                    return Err(shape_err("fno_train: ragged length"));
                }
                flat.extend_from_slice(chan);
            }
        }
        let in_arr = ndarray::Array3::from_shape_vec((n, c, l), flat)
            .map_err(|e| shape_err(format!("fno_train input shape: {e}")))?;

        if target.len() != n {
            return Err(shape_err("fno_train: target batch != input batch"));
        }
        let c_t = target[0].len();
        let l_t = target[0][0].len();
        let mut t_flat: Vec<f32> = Vec::with_capacity(n * c_t * l_t);
        for batch in &target {
            if batch.len() != c_t {
                return Err(shape_err("fno_train: ragged target channel count"));
            }
            for chan in batch {
                if chan.len() != l_t {
                    return Err(shape_err("fno_train: ragged target length"));
                }
                t_flat.extend_from_slice(chan);
            }
        }
        let tgt = ndarray::Array3::from_shape_vec((n, c_t, l_t), t_flat)
            .map_err(|e| shape_err(format!("fno_train target shape: {e}")))?;

        self.inner
            .fno_train_step_mse(&in_arr, &tgt, lr)
            .map_err(rt_err)
    }

    /// Set the HNN's `(q, p)` state.
    fn hnn_set_state(&mut self, q: Vec<f32>, p: Vec<f32>) -> PyResult<()> {
        let qa = Array1::from_vec(q);
        let pa = Array1::from_vec(p);
        self.inner.hnn_set_state(qa, pa).map_err(rt_err)
    }

    /// Advance the HNN one symplectic Euler step.
    /// Returns the Hamiltonian value at the start of the step.
    fn hnn_step(&mut self) -> PyResult<f32> {
        self.inner.hnn_step().map_err(rt_err)
    }

    /// Current HNN Hamiltonian value.
    fn hnn_energy(&self) -> PyResult<f32> {
        self.inner.hnn_energy().map_err(rt_err)
    }

    /// CNN configured?
    #[getter]
    fn has_cnn(&self) -> bool {
        self.inner.cnn().is_some()
    }
    /// FNO configured?
    #[getter]
    fn has_fno(&self) -> bool {
        self.inner.fno().is_some()
    }
    /// HNN configured?
    #[getter]
    fn has_hnn(&self) -> bool {
        self.inner.hnn().is_some()
    }

    /// Insert a new memory node into the Z-Ladder.
    ///
    /// Args:
    ///     id (int): caller-assigned unique ID.
    ///     features (list[float]): payload vector; round-trips
    ///         bit-for-bit through checkpoints.
    ///     t_ms (int): insertion time in milliseconds (used for age).
    ///     salience (float, optional): initial salience in [0, 1].
    ///         Higher values make Z0→Z1 promotion more likely.
    #[pyo3(signature = (id, features, t_ms, salience = 0.0))]
    fn memory_insert(
        &mut self,
        id: u64,
        features: Vec<f32>,
        t_ms: u64,
        salience: f32,
    ) -> PyResult<()> {
        let mut node = MemoryNode::new(id, features, t_ms);
        node.salience = salience.clamp(0.0, 1.0);
        self.inner.memory_insert(node).map_err(rt_err)
    }

    /// Promote a node to Z3 and protect it from demotion indefinitely.
    fn memory_mark_landmark(&mut self, id: u64, reason: &str) -> PyResult<()> {
        self.inner.memory_mark_landmark(id, reason).map_err(rt_err)
    }

    /// Top-`k` cosine similarity matches across every tier.
    ///
    /// Returns:
    ///     list[dict]: each entry is `{node_id, similarity, tier}`,
    ///     sorted descending by similarity.
    fn memory_query_all<'py>(
        &self,
        py: Python<'py>,
        query: Vec<f32>,
        k: usize,
    ) -> PyResult<Bound<'py, PyAny>> {
        let hits = self.inner.memory_query_all(&query, k).map_err(rt_err)?;
        let json = serde_json::to_string(&hits)
            .map_err(|e| PyRuntimeError::new_err(format!("QueryHit encode: {e}")))?;
        py.import("json")?.call_method1("loads", (json,))
    }

    /// Top-`k` cosine similarity matches across the landmark subset only.
    fn memory_query_landmarks<'py>(
        &self,
        py: Python<'py>,
        query: Vec<f32>,
        k: usize,
    ) -> PyResult<Bound<'py, PyAny>> {
        let hits = self
            .inner
            .memory_query_landmarks(&query, k)
            .map_err(rt_err)?;
        let json = serde_json::to_string(&hits)
            .map_err(|e| PyRuntimeError::new_err(format!("QueryHit encode: {e}")))?;
        py.import("json")?.call_method1("loads", (json,))
    }

    /// Run one consolidation tick: age-decay, promote / demote / evict.
    ///
    /// Args:
    ///     dt_seconds (float): wall-clock time since last consolidate.
    /// Returns:
    ///     tuple[int, int, int]: (total_promotions, total_demotions, total_evictions)
    ///     applied by this tick.
    ///
    /// Raises:
    ///     RuntimeError: no memory subsystem on this brain.
    fn memory_consolidate(&mut self, dt_seconds: f32) -> PyResult<(usize, usize, usize)> {
        let mem = self
            .inner
            .memory_mut()
            .ok_or_else(|| rt_err("memory not configured on this brain"))?;
        Ok(mem.consolidate(dt_seconds))
    }

    /// Atomic ensemble checkpoint. Writes `<dir>/{adaptive.rkyv,
    /// snn.json, lnn.json, memory.json, manifest.json}` via a temp dir
    /// + rename, so the old state survives any write failure.
    fn save_ensemble(&self, dir: PathBuf) -> PyResult<()> {
        self.inner.save_ensemble(&dir).map_err(rt_err)
    }

    /// Restore a full ensemble from a directory previously written by
    /// `save_ensemble()`. Shape mismatches (layer widths, SNN pop sizes,
    /// memory capacities) surface as `RuntimeError`.
    fn load_ensemble(&mut self, dir: PathBuf) -> PyResult<()> {
        self.inner.load_ensemble(&dir).map_err(rt_err)
    }

    // -------------------------------------------------------------------------
    // Language (L8) + cognition (C4/C5).
    // -------------------------------------------------------------------------

    /// Grounded-language subsystem configured?
    #[getter]
    fn has_language(&self) -> bool {
        self.inner.language().is_some()
    }

    /// Content-safety (toxicity) stack configured?
    #[getter]
    fn has_toxicity(&self) -> bool {
        self.inner.has_toxicity()
    }

    /// Ground `word` to a sensory feature vector through `modality`.
    ///
    /// Args:
    ///     word (str): the word form.
    ///     features (list[float]): grounded features, `semantic_dim` wide.
    ///     modality (str): visual / auditory / motor / emotional / spatial /
    ///         linguistic. Default: 'visual'.
    /// Returns:
    ///     int: the concept id the word was bound to.
    #[pyo3(signature = (word, features, modality = "visual".to_string()))]
    fn language_ground(&mut self, word: &str, features: Vec<f32>, modality: String) -> PyResult<u32> {
        let m = parse_modality(&modality)?;
        let gl = self
            .inner
            .language_mut()
            .ok_or_else(|| rt_err("language not configured on this brain"))?;
        Ok(gl.ground(word, &features, m).0)
    }

    /// Learn from a text span (distributional embeddings + phrase table).
    /// Toxic text is marked and logged, never dropped (mark-not-filter).
    fn language_learn(&mut self, text: &str) -> PyResult<()> {
        self.inner.language_learn(text).map_err(rt_err)
    }

    /// Comprehend text → `(semantic_vector, confidence)`. Advances the
    /// discourse ring like a heard utterance.
    fn language_comprehend(&mut self, text: &str) -> PyResult<(Vec<f32>, f32)> {
        self.inner
            .language_comprehend(text)
            .map(|(v, c)| (v.to_vec(), c))
            .map_err(rt_err)
    }

    /// Respond to a prompt through the full pipeline: must-run toxicity
    /// gate → cascade with working-memory / imagination / reasoning
    /// sources → defense-in-depth output gate; then feed working memory
    /// and the imagined scene for the next turn.
    ///
    /// Returns:
    ///     dict: `text`, `confidence`, `blocked_for_toxicity`,
    ///     `toxicity_category` (empty unless blocked).
    fn language_respond<'py>(&mut self, py: Python<'py>, text: &str) -> PyResult<Bound<'py, PyAny>> {
        let r = self.inner.language_respond(text).map_err(rt_err)?;
        let d = pyo3::types::PyDict::new(py);
        d.set_item("text", r.text)?;
        d.set_item("confidence", r.confidence)?;
        d.set_item("blocked_for_toxicity", r.blocked_for_toxicity)?;
        d.set_item("toxicity_category", r.toxicity_category)?;
        Ok(d.into_any())
    }

    /// Reason about `query`: the lexicon-grounded conclusion enriched by
    /// Z-Ladder recall (C4).
    ///
    /// Returns:
    ///     dict: `conclusion` (`text`, `confidence`, `vector`, `query_type`,
    ///     `evidence_count`), `lexicon_confidence`, `memory_hits`.
    fn reason<'py>(&mut self, py: Python<'py>, query: &str) -> PyResult<Bound<'py, PyAny>> {
        let r = self.inner.reason(query).map_err(rt_err)?;
        to_py_json(py, &r)
    }

    /// Whether the cascade blends the reasoning conclusion into its
    /// content intent (V1 `reason_in_content`).
    #[getter]
    fn reason_in_content(&self) -> bool {
        self.inner.reason_in_content()
    }

    /// Runtime toggle for the cascade reasoning blend.
    fn set_reason_in_content(&mut self, on: bool) -> PyResult<()> {
        if self.inner.language().is_none() {
            return Err(rt_err("language not configured on this brain"));
        }
        self.inner.set_reason_in_content(on);
        Ok(())
    }

    /// Classify text for toxicity → `(predicted_harm, fairness_violation,
    /// would_block)`. Raises if the toxicity stack is off.
    fn classify_toxicity(&self, text: &str) -> PyResult<(f32, f32, bool)> {
        self.inner.classify_toxicity(text).map_err(rt_err)
    }

    /// Language learning telemetry.
    ///
    /// Returns:
    ///     dict: `learn_calls`, `tokens_seen`, `context_updates`,
    ///     `total_comprehensions`, `groundings`, `vocab_count`,
    ///     `discourse_depth`.
    fn language_stats<'py>(&self, py: Python<'py>) -> PyResult<Bound<'py, PyAny>> {
        let gl = self
            .inner
            .language()
            .ok_or_else(|| rt_err("language not configured on this brain"))?;
        let d = to_py_json(py, &gl.stats)?;
        d.set_item("vocab_count", gl.lexicon.vocab_count())?;
        d.set_item("discourse_depth", gl.discourse.depth())?;
        Ok(d)
    }

    /// Add a semantic vector to working memory at `salience` (C5). It is
    /// blended into the next response's content intent at
    /// `0.25 × salience` while salience stays above 0.2, decaying one
    /// e-fold per turn by default.
    #[pyo3(signature = (data, salience = 0.5))]
    fn working_memory_add(&mut self, data: Vec<f32>, salience: f32) -> PyResult<()> {
        self.inner.working_memory_add(&data, salience).map_err(rt_err)
    }

    /// Current working-memory items, oldest first.
    ///
    /// Returns:
    ///     list[dict]: `data`, `salience`, `timestamp_ms`, `refreshed`.
    fn working_memory_items<'py>(&self, py: Python<'py>) -> PyResult<Bound<'py, PyAny>> {
        let wm = self
            .inner
            .working_memory()
            .ok_or_else(|| rt_err("working memory not configured on this brain"))?;
        to_py_json(py, &wm.items())
    }

    /// Stage `content` as the active imagined scene at `vividness` (C5);
    /// blended into the next response at `0.2 × vividness`.
    #[pyo3(signature = (content, vividness = 0.7))]
    fn imagination_stage(&mut self, content: Vec<f32>, vividness: f32) -> PyResult<()> {
        self.inner.imagination_stage(&content, vividness).map_err(rt_err)
    }

    /// The active imagined scene as `(vector, vividness)`, or `None` when
    /// idle. Raises if imagination is not configured.
    fn imagination_active(&self) -> PyResult<Option<(Vec<f32>, f32)>> {
        let im = self
            .inner
            .imagination()
            .ok_or_else(|| rt_err("imagination not configured on this brain"))?;
        Ok(im.active_vector().map(|(v, viv)| (v.to_vec(), viv)))
    }

    /// Virtual cognition clock in ms — advances by `cognition.turn_ms`
    /// per `language_respond`.
    #[getter]
    fn cognition_now_ms(&self) -> u64 {
        self.inner.cognition_now_ms()
    }

    /// String representation for Python repr.
    fn __repr__(&self) -> String {
        let cfg = self.inner.config();
        format!(
            "<Brain seed={} deterministic={} layers={:?} activation={:?}>",
            cfg.rng_seed, cfg.deterministic, cfg.adaptive.layers, cfg.adaptive.activation
        )
    }
}

/// Python module initializer.
#[pymodule]
fn nimcp_v2(_py: Python<'_>, m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_class::<PyBrain>()?;
    m.add("__version__", env!("CARGO_PKG_VERSION"))?;
    Ok(())
}

//! NIMCP Rust Bindings
//!
//! Safe Rust wrapper around the NIMCP C library (v2.6.3).
//! Uses only the public nimcp.h API via FFI.
//!
//! # Example
//! ```no_run
//! use nimcp::{Brain, BrainSize, BrainTask};
//!
//! nimcp::init().unwrap();
//! let mut brain = Brain::new("test", BrainSize::Tiny,
//!     BrainTask::Classification, 4, 2).unwrap();
//! brain.learn(&[1.0, 0.0, 0.5, 0.3], "cat", 0.9).unwrap();
//! let (label, confidence) = brain.predict(&[1.0, 0.0, 0.5, 0.3]).unwrap();
//! nimcp::shutdown();
//! ```

use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_float, c_int, c_uint, c_void};
use std::ptr;

// ============================================================================
// Error Type
// ============================================================================

#[derive(Debug, Clone, PartialEq)]
pub enum NimcpError {
    Generic(String),
    NullArg(String),
    Invalid(String),
    Memory(String),
    Io(String),
    Unknown(i32, String),
}

impl fmt::Display for NimcpError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NimcpError::Generic(m) => write!(f, "NIMCP error: {}", m),
            NimcpError::NullArg(m) => write!(f, "NIMCP null arg: {}", m),
            NimcpError::Invalid(m) => write!(f, "NIMCP invalid: {}", m),
            NimcpError::Memory(m) => write!(f, "NIMCP memory: {}", m),
            NimcpError::Io(m) => write!(f, "NIMCP I/O: {}", m),
            NimcpError::Unknown(c, m) => write!(f, "NIMCP error {}: {}", c, m),
        }
    }
}

impl std::error::Error for NimcpError {}

pub type Result<T> = std::result::Result<T, NimcpError>;

fn get_error_string() -> String {
    unsafe {
        let ptr = nimcp_get_error();
        if ptr.is_null() {
            "NIMCP error".to_string()
        } else {
            CStr::from_ptr(ptr).to_string_lossy().into_owned()
        }
    }
}

fn check_status(status: c_int) -> Result<()> {
    if status == 0 {
        return Ok(());
    }
    let msg = get_error_string();
    Err(match status {
        1000 => NimcpError::Generic(msg),
        1003 => NimcpError::NullArg(msg),
        1004 => NimcpError::Invalid(msg),
        2000 => NimcpError::Memory(msg),
        4000 => NimcpError::Io(msg),
        _ => NimcpError::Unknown(status, msg),
    })
}

// ============================================================================
// Enums
// ============================================================================

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum BrainSize {
    Tiny = 0,
    Small = 1,
    Medium = 2,
    Large = 3,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum BrainTask {
    Classification = 0,
    Regression = 1,
    PatternMatching = 2,
    Sequence = 3,
    Association = 4,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum NetworkType {
    Adaptive = 0,
    SNN = 1,
    LNN = 2,
    CNN = 3,
    Hybrid = 4,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum SNNTrainMethod {
    STDP = 0,
    RSTDP = 1,
    EProp = 2,
    Surrogate = 3,
    Homeostatic = 4,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum LNNTrainMethod {
    Adjoint = 0,
    BPTT = 1,
    RTRL = 2,
    EProp = 3,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum LossType {
    MSE = 0,
    CrossEntropy = 1,
    BinaryCE = 2,
    Huber = 3,
    MAE = 4,
    Focal = 5,
    KLDiv = 6,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum OptimizerType {
    SGD = 0,
    Momentum = 1,
    Adam = 2,
    AdamW = 3,
    RMSprop = 4,
    Adagrad = 5,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum SchedulerType {
    Constant = 0,
    Step = 1,
    Exponential = 2,
    Cosine = 3,
    WarmupCosine = 4,
    ReduceOnPlateau = 5,
    Cyclic = 6,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum CallbackEvent {
    StepComplete = 0,
    EpochComplete = 1,
    LossComputed = 2,
    WeightsUpdated = 3,
    LRChanged = 4,
    Convergence = 5,
    Divergence = 6,
    Checkpoint = 7,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum CallbackAction {
    Continue = 0,
    Stop = 1,
    Skip = 2,
    Rollback = 3,
    ReduceLR = 4,
    IncreaseLR = 5,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum CognitiveModule {
    None = 0,
    Perception = 1,
    WorkingMemory = 2,
    Executive = 3,
    TheoryOfMind = 4,
    Ethics = 5,
    Attention = 6,
    Emotion = 7,
    Salience = 8,
    Motor = 9,
    Language = 10,
    Metacognition = 11,
    Curiosity = 12,
    Introspection = 13,
    Predictive = 14,
    Consolidation = 15,
    EpisodicMemory = 16,
    SemanticMemory = 17,
    Wellbeing = 18,
    MentalHealth = 19,
    GoalMotivation = 20,
    CognitiveControl = 21,
    CustomStart = 100,
}

// ============================================================================
// FFI Structs
// ============================================================================

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct TrainingConfig {
    pub loss_type: c_int,
    pub optimizer_type: c_int,
    pub scheduler_type: c_int,
    pub learning_rate: f32,
    pub weight_decay: f32,
    pub momentum: f32,
    pub beta1: f32,
    pub beta2: f32,
    pub epsilon: f32,
    pub scheduler_step_size: u32,
    pub scheduler_gamma: f32,
    pub warmup_steps: u32,
    pub enable_gradient_clipping: bool,
    pub gradient_clip_value: f32,
    pub enable_biological_modulation: bool,
    pub biological_blend: f32,
    pub network_type: c_int,
    pub snn_method: c_int,
    pub snn_eligibility_tau: f32,
    pub snn_reward_tau: f32,
    pub snn_surrogate_beta: f32,
    pub lnn_method: c_int,
    pub lnn_bptt_truncation: u32,
    pub lnn_use_adjoint_checkpointing: bool,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct TrainingResult {
    pub loss: f32,
    pub learning_rate: f32,
    pub step: u32,
    pub early_stopped: bool,
    pub gradient_norm: f32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct CallbackConfig {
    pub enable_auto_checkpoint: bool,
    pub checkpoint_interval: u32,
    pub enable_early_stopping: bool,
    pub patience: u32,
    pub min_delta: f32,
    pub divergence_threshold: f32,
    pub log_interval: u32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct CallbackMetrics {
    pub step: u64,
    pub epoch: u64,
    pub loss: f32,
    pub loss_ema: f32,
    pub learning_rate: f32,
    pub gradient_norm: f32,
    pub step_time_us: u64,
    pub is_converging: bool,
    pub is_diverging: bool,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SnapshotInfo {
    pub name: [u8; 128],
    pub description: [u8; 512],
    pub timestamp: u64,
    pub file_size: u32,
    pub is_compressed: bool,
    pub is_encrypted: bool,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct BrainProbeData {
    pub task_name: [u8; 64],
    pub size: c_int,
    pub task: c_int,
    pub num_neurons: u32,
    pub num_synapses: u32,
    pub num_active_synapses: u32,
    pub total_inferences: u64,
    pub total_learning_steps: u64,
    pub avg_sparsity: f32,
    pub avg_inference_time_us: f32,
    pub current_learning_rate: f32,
    pub accuracy: f32,
    pub memory_bytes: usize,
    pub num_inputs: u32,
    pub num_outputs: u32,
    pub is_cow_clone: bool,
    pub cow_ref_count: u32,
    pub cow_shared_bytes: usize,
    pub cow_private_bytes: usize,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct Phasor {
    pub amplitude: f32,
    pub phase: f32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct AvatarState {
    pub mouth_open: f32,
    pub lip_round: f32,
    pub lip_upper: f32,
    pub lip_lower: f32,
    pub tongue_position: f32,
    pub current_viseme: u8,
    pub au1_inner_brow_raise: f32,
    pub au2_outer_brow_raise: f32,
    pub au4_brow_lower: f32,
    pub au5_upper_lid_raise: f32,
    pub au6_cheek_raise: f32,
    pub au7_lid_tighten: f32,
    pub au9_nose_wrinkle: f32,
    pub au10_upper_lip_raise: f32,
    pub au12_lip_corner_pull: f32,
    pub au15_lip_corner_drop: f32,
    pub au17_chin_raise: f32,
    pub au20_lip_stretch: f32,
    pub au23_lip_tighten: f32,
    pub au25_lips_part: f32,
    pub au26_jaw_drop: f32,
    pub au28_lip_suck: f32,
    pub valence: f32,
    pub arousal: f32,
    pub dominance: f32,
    pub emotion_id: u32,
    pub emotion_intensity: f32,
    pub gaze_x: f32,
    pub gaze_y: f32,
    pub head_pitch: f32,
    pub head_yaw: f32,
    pub head_roll: f32,
    pub blink: f32,
    pub pitch_hz: f32,
    pub speaking_rate: f32,
    pub volume: f32,
    pub timestamp_us: u64,
    pub is_speaking: bool,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct RubricData {
    pub internal_consistency: f32,
    pub confidence_calibration: f32,
    pub completeness: f32,
    pub reasoning_chain_quality: f32,
    pub epistemic_quality: f32,
    pub ethical_alignment: f32,
    pub tier1_score: f32,
    pub originality: f32,
    pub integration_depth: f32,
    pub communication_clarity: f32,
    pub engagement_quality: f32,
    pub empathetic_accuracy: f32,
    pub information_density: f32,
    pub tier2_score: f32,
    pub overall_score: f32,
    pub grade: u8,
    pub grade_modifier: u8,
    pub subsystems_available: u32,
    pub evaluation_time_us: u64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ImmuneMetrics {
    pub total_exceptions: u32,
    pub recovered_exceptions: u32,
    pub inflammation_level: f32,
    pub active_antibodies: u32,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct SynapseStats {
    pub total_synapses: u64,
    pub growth_since_last: i64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ExperienceResult {
    pub prediction_error: f32,
    pub attention_level: f32,
    pub learning_rate_used: f32,
    pub learning_applied: bool,
    pub synapse_formed: bool,
    pub reward_signal: f32,
    pub experience_id: u64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct ExperienceConfig {
    pub enabled: bool,
    pub base_learning_rate: f32,
    pub attention_threshold: f32,
    pub attention_lr_scale: f32,
    pub novelty_boost: f32,
    pub enable_hebbian: bool,
    pub enable_reward_learning: bool,
    pub enable_world_model_update: bool,
    pub enable_structural_plasticity: bool,
    pub synaptogenesis_threshold: f32,
    pub consolidation_interval: u32,
}

// ============================================================================
// FFI Structs — Memory Store / OOD Stats (for brain-handle wrappers)
// ============================================================================

#[repr(C)]
#[derive(Debug, Copy, Clone, Default)]
pub struct MemoryStoreStatsRaw {
    pub total_engrams: u64,
    pub total_concepts: u64,
    pub total_relations: u64,
    pub total_autobio: u64,
    pub total_writes: u64,
    pub total_reads: u64,
    pub cache_hits: u64,
    pub cache_misses: u64,
    pub write_buffer_flushes: u64,
    pub bloom_filter_hits: u64,
    pub avg_write_latency_ms: f32,
    pub avg_read_latency_ms: f32,
    pub db_size_bytes: u64,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Default)]
pub struct OodStatsRaw {
    pub total_checks: u64,
    pub ood_detected: u64,
    pub in_distribution: u64,
    pub avg_ood_score: f32,
    pub max_ood_score: f32,
    pub ood_rate: f32,
}

// ============================================================================
// Opaque Handle Types
// ============================================================================

#[repr(C)]
pub struct NimcpBrainHandle {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NimcpNetworkHandle {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NimcpEthicsHandle {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NimcpKnowledgeHandle {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NimcpSnapshotHandle {
    _private: [u8; 0],
}

// ============================================================================
// FFI Callback Type
// ============================================================================

type NativeCallbackFn = extern "C" fn(
    event: c_int,
    metrics: *const CallbackMetrics,
    user_data: *mut c_void,
) -> c_int;

// ============================================================================
// FFI Declarations
// ============================================================================

extern "C" {
    // --- Library lifecycle ---
    fn nimcp_init() -> c_int;
    fn nimcp_shutdown();
    fn nimcp_version() -> *const c_char;
    fn nimcp_version_int() -> c_int;
    fn nimcp_get_error() -> *const c_char;

    // --- Brain core ---
    fn nimcp_brain_create(
        name: *const c_char, size: c_int, task: c_int,
        num_inputs: c_uint, num_outputs: c_uint,
    ) -> *mut NimcpBrainHandle;
    fn nimcp_brain_destroy(brain: *mut NimcpBrainHandle);
    fn nimcp_brain_learn_example(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        num_features: c_uint, label: *const c_char, confidence: c_float,
    ) -> c_int;
    fn nimcp_brain_predict(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        num_features: c_uint, out_label: *mut c_char, out_confidence: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_infer(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        num_features: c_uint, outputs: *mut c_float, num_outputs: c_uint,
    ) -> c_int;
    fn nimcp_brain_save(brain: *mut NimcpBrainHandle, filepath: *const c_char) -> c_int;
    fn nimcp_brain_load(filepath: *const c_char) -> *mut NimcpBrainHandle;
    fn nimcp_brain_create_from_config(filepath: *const c_char) -> *mut NimcpBrainHandle;

    // --- Brain creation variants ---
    fn nimcp_brain_create_full(
        name: *const c_char, task: c_int,
        num_inputs: c_uint, num_outputs: c_uint, neuron_count: c_uint,
    ) -> *mut NimcpBrainHandle;
    fn nimcp_brain_create_with_neurons(
        name: *const c_char, task: c_int,
        num_inputs: c_uint, num_outputs: c_uint, neuron_count: c_uint,
    ) -> *mut NimcpBrainHandle;
    fn nimcp_brain_create_fast(
        name: *const c_char, task: c_int,
        num_inputs: c_uint, num_outputs: c_uint, neuron_count: c_uint,
    ) -> *mut NimcpBrainHandle;

    // --- Brain training ---
    fn nimcp_training_config_default() -> TrainingConfig;
    fn nimcp_brain_configure_training(
        brain: *mut NimcpBrainHandle, config: *const TrainingConfig,
    ) -> c_int;
    fn nimcp_brain_train_step(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        num_features: c_uint, targets: *const c_float, num_targets: c_uint,
        result: *mut TrainingResult,
    ) -> c_int;
    fn nimcp_brain_train_batch(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        targets: *const c_float, batch_size: c_uint,
        num_features: c_uint, num_targets: c_uint,
        result: *mut TrainingResult,
    ) -> c_int;
    fn nimcp_brain_get_training_stats(
        brain: *mut NimcpBrainHandle, total_steps: *mut u64,
        total_loss: *mut c_float, current_lr: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_step_scheduler(
        brain: *mut NimcpBrainHandle, validation_metric: c_float,
    ) -> c_float;

    // --- Brain callbacks ---
    fn nimcp_callback_config_default() -> CallbackConfig;
    fn nimcp_brain_enable_callbacks(
        brain: *mut NimcpBrainHandle, config: *const CallbackConfig,
    ) -> c_int;
    fn nimcp_brain_disable_callbacks(brain: *mut NimcpBrainHandle) -> c_int;
    fn nimcp_brain_register_callback(
        brain: *mut NimcpBrainHandle, event: c_int,
        callback: NativeCallbackFn, user_data: *mut c_void,
        name: *const c_char,
    ) -> c_uint;
    fn nimcp_brain_unregister_callback(
        brain: *mut NimcpBrainHandle, callback_id: c_uint,
    ) -> c_int;
    fn nimcp_brain_get_callback_stats(
        brain: *mut NimcpBrainHandle, total_fired: *mut u64,
        avg_time_us: *mut c_float, early_stops: *mut c_uint,
    ) -> c_int;

    // --- Brain resize ---
    fn nimcp_brain_resize(brain: *mut NimcpBrainHandle, count: c_uint) -> bool;
    fn nimcp_brain_auto_resize(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_get_neuron_count(brain: *mut NimcpBrainHandle) -> c_uint;
    fn nimcp_brain_get_utilization_metrics(
        brain: *mut NimcpBrainHandle, utilization: *mut c_float,
        saturation: *mut c_float,
    ) -> bool;

    // --- Brain named snapshots ---
    fn nimcp_brain_snapshot_save(
        brain: *mut NimcpBrainHandle, name: *const c_char,
        description: *const c_char,
    ) -> c_int;
    fn nimcp_brain_snapshot_restore(
        brain: *mut NimcpBrainHandle, name: *const c_char,
    ) -> *mut NimcpBrainHandle;
    fn nimcp_brain_snapshot_list(
        brain: *mut NimcpBrainHandle, infos: *mut SnapshotInfo,
        max_count: c_uint, out_count: *mut c_uint,
    ) -> c_int;
    fn nimcp_brain_snapshot_delete(
        brain: *mut NimcpBrainHandle, name: *const c_char,
    ) -> c_int;

    // --- Brain COW ---
    fn nimcp_brain_clone_cow(brain: *mut NimcpBrainHandle) -> *mut NimcpBrainHandle;
    fn nimcp_brain_snapshot_cow(brain: *mut NimcpBrainHandle) -> *mut NimcpSnapshotHandle;
    fn nimcp_brain_restore_cow(
        brain: *mut NimcpBrainHandle, snapshot: *mut NimcpSnapshotHandle,
    ) -> c_int;
    fn nimcp_brain_snapshot_destroy(snapshot: *mut NimcpSnapshotHandle);

    // --- Brain working memory ---
    fn nimcp_brain_working_memory_add(
        brain: *mut NimcpBrainHandle, data: *const c_float,
        size: c_uint, salience: c_float,
    ) -> c_int;
    fn nimcp_brain_working_memory_get(
        brain: *mut NimcpBrainHandle, index: c_uint, size_out: *mut c_uint,
    ) -> *const c_float;
    fn nimcp_brain_working_memory_stats(
        brain: *mut NimcpBrainHandle, current_size: *mut c_uint,
        capacity: *mut c_uint,
    ) -> c_int;
    fn nimcp_brain_working_memory_refresh(
        brain: *mut NimcpBrainHandle, index: c_uint,
    ) -> c_int;

    // --- Brain workspace ---
    fn nimcp_brain_workspace_compete(
        brain: *mut NimcpBrainHandle, module: c_int,
        content: *const c_float, content_dim: c_uint, strength: c_float,
    ) -> c_int;
    fn nimcp_brain_workspace_read(
        brain: *mut NimcpBrainHandle, content: *mut c_float,
        max_dim: c_uint, actual_dim: *mut c_uint,
        source_module: *mut c_int,
    ) -> c_int;
    fn nimcp_brain_workspace_subscribe(
        brain: *mut NimcpBrainHandle, module: c_int,
    ) -> c_int;
    fn nimcp_brain_workspace_unsubscribe(
        brain: *mut NimcpBrainHandle, module: c_int,
    ) -> c_int;
    fn nimcp_brain_workspace_has_broadcast(
        brain: *mut NimcpBrainHandle, has_broadcast: *mut bool,
    ) -> c_int;
    fn nimcp_brain_workspace_stats(
        brain: *mut NimcpBrainHandle, total_broadcasts: *mut c_uint,
        total_competitions: *mut c_uint, avg_strength: *mut c_float,
    ) -> c_int;

    // --- Brain oscillations ---
    fn nimcp_enable_complex_oscillations(
        brain: *mut NimcpBrainHandle, enable: bool,
    ) -> bool;
    fn nimcp_is_complex_oscillations_enabled(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_get_oscillation_phasor(
        brain: *mut NimcpBrainHandle, neuron_id: c_uint,
    ) -> Phasor;
    fn nimcp_get_phase_coherence(
        brain: *mut NimcpBrainHandle, neuron_ids: *const c_uint, count: c_uint,
    ) -> c_float;
    fn nimcp_get_pac_modulation(
        brain: *mut NimcpBrainHandle, theta_freq: c_float, gamma_freq: c_float,
    ) -> c_float;

    // --- Brain probe ---
    fn nimcp_brain_probe(
        brain: *mut NimcpBrainHandle, probe: *mut BrainProbeData,
    ) -> c_int;
    fn nimcp_brain_broadcast_probe(brain: *mut NimcpBrainHandle) -> c_int;

    // --- Cognitive decision ---
    fn nimcp_brain_decide_full(
        brain: *mut NimcpBrainHandle,
        features: *const c_float, num_features: c_uint,
        out_label: *mut c_char, out_confidence: *mut c_float,
        out_explanation: *mut c_char,
        out_output_vector: *mut c_float, out_output_size: *mut c_uint,
        out_num_active_neurons: *mut c_uint, out_sparsity: *mut c_float,
        out_inference_time_us: *mut u64,
    ) -> c_int;

    // --- Language production ---
    fn nimcp_brain_speak(
        brain: *mut NimcpBrainHandle,
        semantic_input: *const c_float, semantic_dim: c_uint,
        out_text: *mut c_char, text_max_len: c_uint,
        out_confidence: *mut c_float, out_fluency: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_generate_text(
        brain: *mut NimcpBrainHandle,
        prompt: *const c_char,
        semantic_input: *const c_float, semantic_dim: c_uint,
        out_text: *mut c_char, text_max_len: c_uint,
        out_confidence: *mut c_float, out_perplexity: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_comprehend(
        brain: *mut NimcpBrainHandle,
        text: *const c_char,
        out_semantic: *mut c_float, semantic_dim: c_uint,
        out_confidence: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_produce_text(
        brain: *mut NimcpBrainHandle,
        intent: *const c_float, intent_dim: c_uint,
        out_text: *mut c_char, text_max_len: c_uint,
        out_confidence: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_creative_blend(
        brain: *mut NimcpBrainHandle,
        vector_a: *const c_float, vector_b: *const c_float,
        vec_dim: c_uint, blend_ratio: c_float,
        out_text: *mut c_char, text_max: c_uint,
    ) -> c_int;
    fn nimcp_brain_grounded_respond(
        brain: *mut NimcpBrainHandle,
        input_text: *const c_char,
        out_response: *mut c_char, response_max: c_uint,
        out_confidence: *mut c_float,
    ) -> c_int;

    // --- Metrics getters ---
    fn nimcp_brain_get_accuracy(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_get_last_gradient_norm(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_get_last_loss(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_get_network_metrics(
        brain: *mut NimcpBrainHandle,
        ema_ann: *mut c_float, ema_cnn: *mut c_float,
        ema_snn: *mut c_float, ema_lnn: *mut c_float,
        ann_steps: *mut u64, cnn_steps: *mut u64,
        snn_steps: *mut u64, lnn_steps: *mut u64,
    ) -> bool;
    fn nimcp_brain_get_cognitive_stats(
        brain: *mut NimcpBrainHandle,
        out_stats: *mut c_uint, out_losses: *mut c_float,
        out_count: *mut c_uint,
    ) -> c_int;
    fn nimcp_brain_get_avatar_state(
        brain: *mut NimcpBrainHandle, state: *mut AvatarState,
    ) -> c_int;
    fn nimcp_brain_get_last_transcript(
        brain: *mut NimcpBrainHandle,
        out_entries: *mut [c_char; 256],
        out_saliences: *mut c_float,
        out_confidences: *mut c_float,
        out_modules: *mut *const c_char,
        max_entries: c_uint,
    ) -> c_uint;
    fn nimcp_brain_get_cortex_cnn_metrics(
        brain: *mut NimcpBrainHandle,
        out_types: *mut c_int, out_losses: *mut c_float,
        out_fwd_steps: *mut u64, out_bwd_steps: *mut u64,
        out_embed_norms: *mut c_float, out_count: *mut c_uint,
    ) -> c_int;

    // --- Vector / batch learning ---
    fn nimcp_brain_learn_vector(
        brain: *mut NimcpBrainHandle,
        features: *const c_float, num_features: c_uint,
        target: *const c_float, target_size: c_uint,
        label: *const c_char, confidence: c_float,
    ) -> c_int;
    fn nimcp_brain_learn_vector_batch(
        brain: *mut NimcpBrainHandle,
        features_array: *const *const c_float,
        targets_array: *const *const c_float,
        num_features: c_uint, target_size: c_uint,
        num_examples: c_uint, learning_rate: c_float,
    ) -> c_float;
    fn nimcp_brain_learn_batch(
        brain: *mut NimcpBrainHandle,
        features_array: *const *const c_float,
        num_features_array: *const c_uint,
        labels: *const *const c_char,
        confidences: *const c_float,
        num_examples: c_uint,
        losses_out: *mut c_float,
    ) -> c_int;

    // --- Knowledge / language learning ---
    fn nimcp_brain_learn_knowledge(
        brain: *mut NimcpBrainHandle, text: *const c_char, domain: c_int,
    ) -> c_int;
    fn nimcp_brain_learn_language(
        brain: *mut NimcpBrainHandle, text: *const c_char,
        out_loss: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_learn_language_pair(
        brain: *mut NimcpBrainHandle,
        input_text: *const c_char, target_text: *const c_char,
        learning_rate: c_float, out_loss: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_train_cognitive(
        brain: *mut NimcpBrainHandle,
        text: *const c_char, domain: c_int,
        target_text: *const c_char, learning_rate: c_float,
        out_loss: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_train_language(
        brain: *mut NimcpBrainHandle,
        input_text: *const c_char, target_text: *const c_char,
        learning_rate: c_float, out_loss: *mut c_float,
    ) -> c_int;

    // --- Fast / domain prediction ---
    fn nimcp_brain_predict_fast(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        num_features: c_uint, out_label: *mut c_char, out_confidence: *mut c_float,
    ) -> c_int;
    fn nimcp_brain_predict_in_domain(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        num_features: c_uint, domain_prefix: *const c_char,
        out_label: *mut c_char, out_confidence: *mut c_float,
    ) -> c_int;

    // --- Freeze / mode ---
    fn nimcp_brain_freeze(brain: *mut NimcpBrainHandle) -> c_int;
    fn nimcp_brain_is_frozen(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_enable_mixed_precision(
        brain: *mut NimcpBrainHandle, enable: bool,
    ) -> c_int;
    fn nimcp_brain_enable_gradient_checkpointing(
        brain: *mut NimcpBrainHandle, enable: bool, checkpoint_interval: c_uint,
    ) -> c_int;
    fn nimcp_brain_enable_hemispheric(
        brain: *mut NimcpBrainHandle, enable: bool,
    ) -> c_int;
    fn nimcp_brain_enable_recurrent(
        brain: *mut NimcpBrainHandle, enable: bool,
        max_iterations: c_uint, confidence_threshold: c_float,
        blend_alpha: c_float,
    ) -> c_int;
    fn nimcp_brain_enable_bptt(
        brain: *mut NimcpBrainHandle, enable: bool,
        window_size: c_uint, discount: c_float,
    ) -> c_int;
    fn nimcp_brain_enable_multi_network(brain: *mut NimcpBrainHandle) -> c_int;
    fn nimcp_brain_enable_biological_plasticity(
        brain: *mut NimcpBrainHandle, enabled: bool,
    ) -> c_int;
    fn nimcp_brain_set_fast_training(
        brain: *mut NimcpBrainHandle, enabled: bool,
    ) -> c_int;
    fn nimcp_brain_set_task_type(
        brain: *mut NimcpBrainHandle, task_type: *const c_char,
    ) -> c_int;
    fn nimcp_brain_set_training_mode(
        brain: *mut NimcpBrainHandle, active: bool,
    );
    fn nimcp_brain_set_network_ablation(
        brain: *mut NimcpBrainHandle,
        train_cnn: c_int, train_snn: c_int, train_lnn: c_int,
    );

    // Per-network training toggles (runtime-dynamic, no rebuild needed)
    fn nimcp_brain_set_train_ann(brain: *mut NimcpBrainHandle, enabled: bool);
    fn nimcp_brain_get_train_ann(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_set_train_cnn(brain: *mut NimcpBrainHandle, enabled: bool);
    fn nimcp_brain_get_train_cnn(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_set_train_snn(brain: *mut NimcpBrainHandle, enabled: bool);
    fn nimcp_brain_get_train_snn(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_set_train_lnn(brain: *mut NimcpBrainHandle, enabled: bool);
    fn nimcp_brain_get_train_lnn(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_set_snn_only_recovery(brain: *mut NimcpBrainHandle, enabled: bool);
    fn nimcp_brain_get_snn_only_recovery(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_set_ensemble_warmup_scale(brain: *mut NimcpBrainHandle, scale: c_float);
    fn nimcp_brain_get_ensemble_warmup_scale(brain: *mut NimcpBrainHandle) -> c_float;

    // --- Sensory / cortex ---
    fn nimcp_brain_submit_sensory(
        brain: *mut NimcpBrainHandle,
        modality: *const c_char,
        data: *const c_float, num_elements: c_uint,
        width: c_uint, height: c_uint, channels: c_uint,
        n_segments: c_uint,
    ) -> c_int;
    fn nimcp_brain_visual_cortex_process(
        brain: *mut NimcpBrainHandle,
        pixels: *const c_float, num_pixels: c_uint,
        width: c_uint, height: c_uint, channels: c_uint,
        out_features: *mut c_float, max_features: c_uint,
        out_feature_count: *mut c_uint,
    ) -> c_int;

    // --- Brain region probes ---
    fn nimcp_brain_medulla_get_arousal(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_bg_get_dopamine(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_sleep_get_pressure(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_substrate_get_health(
        brain: *mut NimcpBrainHandle,
        out_status: *mut c_char, max_len: c_uint,
    ) -> c_int;

    // --- Sub-network creation / stats ---
    fn nimcp_brain_lnn_create(
        brain: *mut NimcpBrainHandle,
        n_sensory: c_uint, n_inter: c_uint,
        n_command: c_uint, n_output: c_uint,
    ) -> c_int;
    fn nimcp_brain_lnn_get_stats(
        brain: *mut NimcpBrainHandle,
        out_forward_steps: *mut u64, out_backward_steps: *mut u64,
        out_ode_evals: *mut u64, out_avg_tau: *mut c_float,
        out_state_norm: *mut c_float, out_gradient_norm: *mut c_float,
        out_nan_count: *mut c_uint, out_inf_count: *mut c_uint,
    ) -> c_int;
    fn nimcp_brain_snn_get_stats(
        brain: *mut NimcpBrainHandle,
        out_total_steps: *mut u64, out_total_spikes: *mut u64,
        out_mean_firing_rate: *mut c_float, out_sparsity: *mut c_float,
        out_synchrony: *mut c_float, out_silent_neurons: *mut c_uint,
        out_hyperactive_neurons: *mut c_uint, out_health: *mut c_int,
        out_memory_bytes: *mut usize,
    ) -> c_int;
    fn nimcp_snn_set_input_scale(scale: c_float);
    fn nimcp_snn_get_input_scale() -> c_float;
    fn nimcp_brain_cnn_get_stats(
        brain: *mut NimcpBrainHandle,
        out_num_layers: *mut c_uint, out_num_parameters: *mut usize,
        out_num_labels: *mut c_uint, out_active: *mut bool,
    ) -> c_int;

    // --- Rubric ---
    fn nimcp_brain_rubric(
        brain: *mut NimcpBrainHandle, rubric: *mut RubricData,
    ) -> c_int;
    fn nimcp_brain_broadcast_rubric(brain: *mut NimcpBrainHandle) -> c_int;
    fn nimcp_brain_set_rubric_validation(
        brain: *mut NimcpBrainHandle, features: *const c_float,
        num_features: c_uint,
    ) -> c_int;
    fn nimcp_brain_get_rubric_training_stats(
        brain: *mut NimcpBrainHandle,
        eval_count: *mut u64, min_score: *mut c_float,
        max_score: *mut c_float, avg_score: *mut c_float,
        last_rubric: *mut RubricData,
    ) -> c_int;

    // --- Experience ---
    fn nimcp_brain_experience(
        brain: *mut NimcpBrainHandle,
        input: *const c_float, input_size: c_uint,
        output: *mut c_float, output_size: c_uint,
        teacher_reward: c_float,
        result: *mut ExperienceResult,
    ) -> c_int;
    fn nimcp_brain_experience_configure(
        brain: *mut NimcpBrainHandle,
        config: *const ExperienceConfig,
    ) -> c_int;
    fn nimcp_brain_experience_correct(
        brain: *mut NimcpBrainHandle,
        expected: *const c_float, expected_size: c_uint,
    ) -> c_float;
    fn nimcp_brain_experience_attend(
        brain: *mut NimcpBrainHandle,
        modality: *const c_char, strength: c_float,
    ) -> c_int;

    // --- Grounded language ---
    fn nimcp_brain_ground_word(
        brain: *mut NimcpBrainHandle,
        word: *const c_char,
        features: *const c_float, feature_dim: c_uint,
        modality: c_uint, attention: c_float,
    ) -> c_int;
    fn nimcp_brain_innate_hardwire(
        brain: *mut NimcpBrainHandle,
        config: *const c_void,
    ) -> c_int;

    // --- Cloud ---
    fn nimcp_brain_connect_cloud(
        brain: *mut NimcpBrainHandle, cloud_brain: *mut NimcpBrainHandle,
        confidence_threshold: c_float, enable_distillation: bool,
    ) -> c_int;
    fn nimcp_brain_disconnect_cloud(brain: *mut NimcpBrainHandle) -> c_int;
    fn nimcp_brain_distill_cloud_batch(
        brain: *mut NimcpBrainHandle, max_examples: c_uint,
    ) -> c_uint;
    fn nimcp_brain_get_cloud_stats(
        brain: *mut NimcpBrainHandle,
        total_queries: *mut u64, local_handled: *mut u64,
        cloud_escalated: *mut u64, distillation_steps: *mut u64,
    ) -> c_int;
    fn nimcp_brain_connect_collective(
        brain_a: *mut NimcpBrainHandle, brain_b: *mut NimcpBrainHandle,
        instance_id: c_uint,
    ) -> c_int;

    // --- Attention / hemispheric ---
    fn nimcp_brain_focus_attention(
        brain: *mut NimcpBrainHandle, modality: *const c_char,
    ) -> c_int;
    fn nimcp_brain_get_hemispheric_balance(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_get_callosum_transfers(brain: *mut NimcpBrainHandle) -> u64;
    fn nimcp_brain_get_lateralization(
        brain: *mut NimcpBrainHandle, domain: c_uint,
    ) -> c_float;
    fn nimcp_brain_shift_lateralization(
        brain: *mut NimcpBrainHandle, domain: c_uint, shift: c_float,
    ) -> c_int;
    fn nimcp_brain_get_recurrent_iterations(brain: *mut NimcpBrainHandle) -> c_uint;

    // --- Health probes ---
    fn nimcp_brain_get_immune_metrics(
        brain: *mut NimcpBrainHandle, out: *mut ImmuneMetrics,
    ) -> c_int;
    fn nimcp_brain_get_sparsity_ratio(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_get_synapse_stats(
        brain: *mut NimcpBrainHandle, out: *mut SynapseStats,
    ) -> c_int;
    fn nimcp_brain_get_active_neuron_count(brain: *mut NimcpBrainHandle) -> c_uint;
    fn nimcp_brain_get_neuron_utilization(brain: *mut NimcpBrainHandle) -> c_float;
    fn nimcp_brain_get_gpu_vram_used(brain: *mut NimcpBrainHandle) -> usize;
    fn nimcp_brain_get_memory_rss() -> usize;

    // --- Edge Brain ---
    fn nimcp_edge_brain_resize(
        brain: *mut NimcpBrainHandle, config: *const c_void,
    ) -> c_int;
    fn nimcp_edge_brain_resize_check(
        brain: *mut NimcpBrainHandle, config: *const c_void, report: *mut c_void,
    ) -> c_int;
    fn nimcp_edge_score_neuron_importance(
        brain: *mut NimcpBrainHandle, scores: *mut c_float, num_neurons: c_uint,
    ) -> c_int;
    fn nimcp_brain_distill(
        teacher: *mut NimcpBrainHandle, student: *mut *mut NimcpBrainHandle,
        config: *const c_void, report: *mut c_void,
    ) -> c_int;
    fn nimcp_brain_optimize_for_device(
        master: *mut NimcpBrainHandle, device: *const c_void,
        child: *mut *mut NimcpBrainHandle, report: *mut c_void,
    ) -> c_int;
    fn nimcp_brain_quantize(
        brain: *mut NimcpBrainHandle, config: *const c_void,
    ) -> c_int;
    fn nimcp_resize_config_default() -> [u8; 64];
    fn nimcp_distill_config_default() -> [u8; 64];
    fn nimcp_quantize_config_default() -> [u8; 64];
    fn nimcp_device_profile_default() -> [u8; 64];

    // --- Swarm Runtime ---
    fn nimcp_swarm_master_create(brain: *mut NimcpBrainHandle, config: *const c_void) -> *mut c_void;
    fn nimcp_swarm_master_destroy(master: *mut c_void);
    fn nimcp_swarm_master_start(master: *mut c_void) -> c_int;
    fn nimcp_swarm_master_stop(master: *mut c_void) -> c_int;
    fn nimcp_swarm_master_kick(master: *mut c_void, device_id: c_uint) -> c_int;
    fn nimcp_swarm_master_force_sync(master: *mut c_void) -> c_int;
    fn nimcp_swarm_master_get_peer_count(master: *const c_void) -> c_uint;
    fn nimcp_swarm_master_config_default() -> [u8; 64];
    fn nimcp_swarm_edge_create(brain: *mut NimcpBrainHandle, config: *const c_void) -> *mut c_void;
    fn nimcp_swarm_edge_destroy(rt: *mut c_void);
    fn nimcp_swarm_edge_start(rt: *mut c_void) -> c_int;
    fn nimcp_swarm_edge_stop(rt: *mut c_void) -> c_int;
    fn nimcp_swarm_edge_is_connected(rt: *const c_void) -> bool;
    fn nimcp_swarm_edge_submit_gradients(rt: *mut c_void, grads: *const c_float, n: c_uint) -> c_int;
    fn nimcp_swarm_edge_config_default() -> [u8; 64];

    // --- Sensor Hub ---
    fn nimcp_sensor_hub_create(max_sensors: c_uint) -> *mut c_void;
    fn nimcp_sensor_hub_destroy(hub: *mut c_void);
    fn nimcp_sensor_register(hub: *mut c_void, desc: *const c_void) -> c_int;
    fn nimcp_sensor_submit_reading(hub: *mut c_void, reading: *const c_void) -> c_int;
    fn nimcp_sensor_get_latest(hub: *mut c_void, sensor_id: c_uint, out: *mut c_void) -> c_int;
    fn nimcp_sensor_compose_feature_vector(hub: *mut c_void, features: *mut c_float, max: c_uint) -> c_int;
    fn nimcp_sensor_get_count(hub: *const c_void) -> c_uint;

    // --- Safety Watchdog ---
    fn nimcp_watchdog_create(config: *const c_void) -> *mut c_void;
    fn nimcp_watchdog_destroy(wd: *mut c_void);
    fn nimcp_watchdog_arm(wd: *mut c_void) -> c_int;
    fn nimcp_watchdog_disarm(wd: *mut c_void) -> c_int;
    fn nimcp_watchdog_heartbeat(wd: *mut c_void);
    fn nimcp_watchdog_validate_output(wd: *mut c_void, output: *mut c_float, n: c_uint) -> c_int;
    fn nimcp_watchdog_get_safe_output(wd: *mut c_void, output: *mut c_float, n: c_uint) -> c_int;
    fn nimcp_watchdog_estop(wd: *mut c_void);
    fn nimcp_watchdog_reset(wd: *mut c_void) -> c_int;
    fn nimcp_watchdog_get_state(wd: *mut c_void) -> c_int;
    fn nimcp_watchdog_state_name(state: c_int) -> *const c_char;
    fn nimcp_watchdog_config_default() -> [u8; 128];

    // --- ROS 2 Bridge ---
    fn nimcp_ros2_bridge_create(brain: *mut NimcpBrainHandle, config: *const c_void) -> *mut c_void;
    fn nimcp_ros2_bridge_destroy(bridge: *mut c_void);
    fn nimcp_ros2_bridge_start(bridge: *mut c_void) -> c_int;
    fn nimcp_ros2_bridge_stop(bridge: *mut c_void) -> c_int;
    fn nimcp_ros2_bridge_inject_sensor(bridge: *mut c_void, topic: *const c_char, data: *const c_float, count: c_uint) -> c_int;
    fn nimcp_ros2_bridge_get_last_cmd(bridge: *const c_void, data: *mut c_float, max: c_uint) -> c_int;
    fn nimcp_ros2_config_default() -> [u8; 256];

    // --- MAVLink Bridge ---
    fn nimcp_mavlink_bridge_create(config: *const c_void) -> *mut c_void;
    fn nimcp_mavlink_bridge_destroy(bridge: *mut c_void);
    fn nimcp_mavlink_bridge_connect(bridge: *mut c_void) -> c_int;
    fn nimcp_mavlink_bridge_disconnect(bridge: *mut c_void) -> c_int;
    fn nimcp_mavlink_bridge_start(bridge: *mut c_void) -> c_int;
    fn nimcp_mavlink_bridge_stop(bridge: *mut c_void) -> c_int;
    fn nimcp_mavlink_get_attitude(bridge: *const c_void, att: *mut [f32; 7]) -> c_int;
    fn nimcp_mavlink_get_position(bridge: *const c_void, pos: *mut [u8; 128]) -> c_int;
    fn nimcp_mavlink_get_battery(bridge: *const c_void, bat: *mut [f32; 5]) -> c_int;
    fn nimcp_mavlink_set_velocity(bridge: *mut c_void, vx: c_float, vy: c_float, vz: c_float, yr: c_float) -> c_int;
    fn nimcp_mavlink_arm(bridge: *mut c_void, arm: bool) -> c_int;
    fn nimcp_mavlink_takeoff(bridge: *mut c_void, alt: c_float) -> c_int;
    fn nimcp_mavlink_land(bridge: *mut c_void) -> c_int;
    fn nimcp_mavlink_goto(bridge: *mut c_void, lat: f64, lon: f64, alt: c_float) -> c_int;
    fn nimcp_mavlink_rtl(bridge: *mut c_void) -> c_int;
    fn nimcp_mavlink_compose_features(bridge: *const c_void, features: *mut c_float, max: c_uint) -> c_int;
    fn nimcp_mavlink_config_default() -> [u8; 512];

    // --- Memory Store / OOD / Audit (brain-handle wrappers) ---
    fn nimcp_brain_memory_store_stats(
        brain: *mut NimcpBrainHandle, stats: *mut MemoryStoreStatsRaw,
    ) -> c_int;
    fn nimcp_brain_memory_search_text(
        brain: *mut NimcpBrainHandle, query: *const c_char,
        max_results: c_uint, out_ids: *mut u64, out_count: *mut c_uint,
    ) -> c_int;
    fn nimcp_brain_memory_search_similar(
        brain: *mut NimcpBrainHandle, embedding: *const c_float,
        dim: c_uint, top_k: c_uint, out_ids: *mut u64,
        out_distances: *mut c_float, out_count: *mut c_uint,
    ) -> c_int;
    fn nimcp_brain_memory_is_healthy(brain: *mut NimcpBrainHandle) -> bool;
    fn nimcp_brain_ood_stats(
        brain: *mut NimcpBrainHandle, stats: *mut OodStatsRaw,
    ) -> c_int;
    fn nimcp_brain_audit_log(
        brain: *mut NimcpBrainHandle, description: *const c_char,
        severity: c_uint, details: *const c_char,
    ) -> c_int;
    fn nimcp_brain_audit_search(
        brain: *mut NimcpBrainHandle, min_severity: c_uint,
        max_results: c_uint, out_ids: *mut u64,
        out_severities: *mut c_float, out_count: *mut c_uint,
    ) -> c_int;

    // --- Network ---
    fn nimcp_network_create(
        num_inputs: c_uint, num_outputs: c_uint,
        num_hidden: c_uint, learning_rate: c_float,
    ) -> *mut NimcpNetworkHandle;
    fn nimcp_network_destroy(network: *mut NimcpNetworkHandle);
    fn nimcp_network_forward(
        network: *mut NimcpNetworkHandle, inputs: *const c_float,
        num_inputs: c_uint, outputs: *mut c_float, num_outputs: c_uint,
    ) -> c_int;
    fn nimcp_network_train(
        network: *mut NimcpNetworkHandle, inputs: *const c_float,
        num_inputs: c_uint, targets: *const c_float, num_targets: c_uint,
    ) -> c_int;

    // --- Ethics ---
    fn nimcp_ethics_create() -> *mut NimcpEthicsHandle;
    fn nimcp_ethics_destroy(ethics: *mut NimcpEthicsHandle);
    fn nimcp_ethics_check(
        ethics: *mut NimcpEthicsHandle, situation: *const c_float,
        num_features: c_uint, out_score: *mut c_float,
    ) -> c_int;

    // --- Knowledge ---
    fn nimcp_knowledge_create() -> *mut NimcpKnowledgeHandle;
    fn nimcp_knowledge_destroy(knowledge: *mut NimcpKnowledgeHandle);
    fn nimcp_knowledge_add_fact(
        knowledge: *mut NimcpKnowledgeHandle,
        subject: *const c_char, predicate: *const c_char, object: *const c_char,
    ) -> c_int;
    fn nimcp_knowledge_query(
        knowledge: *mut NimcpKnowledgeHandle, query: *const c_char,
        out_result: *mut c_char, max_result_len: c_uint,
    ) -> c_int;
}

// ============================================================================
// Library Lifecycle
// ============================================================================

pub fn init() -> Result<()> {
    check_status(unsafe { nimcp_init() })
}

pub fn shutdown() {
    unsafe { nimcp_shutdown() }
}

pub fn version() -> String {
    unsafe {
        let ptr = nimcp_version();
        if ptr.is_null() {
            "unknown".to_string()
        } else {
            CStr::from_ptr(ptr).to_string_lossy().into_owned()
        }
    }
}

pub fn version_int() -> i32 {
    unsafe { nimcp_version_int() }
}

pub fn training_config_default() -> TrainingConfig {
    unsafe { nimcp_training_config_default() }
}

pub fn callback_config_default() -> CallbackConfig {
    unsafe { nimcp_callback_config_default() }
}

/// Get process RSS in bytes (global, not per-brain)
pub fn get_memory_rss() -> usize {
    unsafe { nimcp_brain_get_memory_rss() }
}

// ============================================================================
// Safe Data Types
// ============================================================================

#[derive(Debug)]
pub struct Prediction {
    pub label: String,
    pub confidence: f32,
}

#[derive(Debug)]
pub struct TrainingStats {
    pub total_steps: u64,
    pub total_loss: f32,
    pub current_lr: f32,
}

#[derive(Debug)]
pub struct UtilizationMetrics {
    pub utilization: f32,
    pub saturation: f32,
}

#[derive(Debug)]
pub struct WorkspaceReadResult {
    pub content: Vec<f32>,
    pub actual_dim: u32,
    pub source_module: CognitiveModule,
}

#[derive(Debug)]
pub struct WorkspaceStats {
    pub total_broadcasts: u32,
    pub total_competitions: u32,
    pub avg_strength: f32,
}

#[derive(Debug)]
pub struct CallbackStats {
    pub total_fired: u64,
    pub avg_time_us: f32,
    pub early_stops: u32,
}

#[derive(Debug)]
pub struct WorkingMemoryStats {
    pub current_size: u32,
    pub capacity: u32,
}

#[derive(Debug)]
pub struct BrainProbe {
    pub task_name: String,
    pub size: BrainSize,
    pub task: BrainTask,
    pub num_neurons: u32,
    pub num_synapses: u32,
    pub num_active_synapses: u32,
    pub total_inferences: u64,
    pub total_learning_steps: u64,
    pub avg_sparsity: f32,
    pub avg_inference_time_us: f32,
    pub current_learning_rate: f32,
    pub accuracy: f32,
    pub memory_bytes: usize,
    pub num_inputs: u32,
    pub num_outputs: u32,
    pub is_cow_clone: bool,
    pub cow_ref_count: u32,
    pub cow_shared_bytes: usize,
    pub cow_private_bytes: usize,
}

/// Full decision result from decide_full()
#[derive(Debug)]
pub struct DecisionFull {
    pub label: String,
    pub confidence: f32,
    pub explanation: String,
    pub output_vector: Vec<f32>,
    pub num_active_neurons: u32,
    pub sparsity: f32,
    pub inference_time_us: u64,
}

/// Per-network training metrics
#[derive(Debug)]
pub struct NetworkMetrics {
    pub ema_ann: f32,
    pub ema_cnn: f32,
    pub ema_snn: f32,
    pub ema_lnn: f32,
    pub ann_steps: u64,
    pub cnn_steps: u64,
    pub snn_steps: u64,
    pub lnn_steps: u64,
}

/// LNN statistics
#[derive(Debug)]
pub struct LnnStats {
    pub forward_steps: u64,
    pub backward_steps: u64,
    pub ode_evals: u64,
    pub avg_tau: f32,
    pub state_norm: f32,
    pub gradient_norm: f32,
    pub nan_count: u32,
    pub inf_count: u32,
}

/// SNN statistics
#[derive(Debug)]
pub struct SnnStats {
    pub total_steps: u64,
    pub total_spikes: u64,
    pub mean_firing_rate: f32,
    pub sparsity: f32,
    pub synchrony: f32,
    pub silent_neurons: u32,
    pub hyperactive_neurons: u32,
    pub health: i32,
    pub memory_bytes: usize,
}

/// CNN statistics
#[derive(Debug)]
pub struct CnnStats {
    pub num_layers: u32,
    pub num_parameters: usize,
    pub num_labels: u32,
    pub active: bool,
}

/// Cloud inference statistics
#[derive(Debug)]
pub struct CloudStats {
    pub total_queries: u64,
    pub local_handled: u64,
    pub cloud_escalated: u64,
    pub distillation_steps: u64,
}

/// Rubric training statistics
#[derive(Debug)]
pub struct RubricTrainingStats {
    pub eval_count: u64,
    pub min_score: f32,
    pub max_score: f32,
    pub avg_score: f32,
    pub last_rubric: RubricData,
}

/// Per-cortex CNN metrics
#[derive(Debug)]
pub struct CortexCnnMetrics {
    pub types: Vec<i32>,
    pub losses: Vec<f32>,
    pub fwd_steps: Vec<u64>,
    pub bwd_steps: Vec<u64>,
    pub embed_norms: Vec<f32>,
}

/// Cognitive training stats per module
#[derive(Debug)]
pub struct CognitiveStats {
    pub step_counts: Vec<u32>,
    pub losses: Vec<f32>,
}

/// Memory store statistics
#[derive(Debug)]
pub struct MemoryStoreStats {
    pub total_engrams: u64,
    pub total_concepts: u64,
    pub total_relations: u64,
    pub total_autobio: u64,
    pub total_writes: u64,
    pub total_reads: u64,
    pub cache_hits: u64,
    pub cache_misses: u64,
    pub db_size_bytes: u64,
}

/// OOD detection statistics
#[derive(Debug)]
pub struct OodStatsResult {
    pub total_checks: u64,
    pub ood_detected: u64,
    pub in_distribution: u64,
    pub avg_ood_score: f32,
    pub ood_rate: f32,
}

/// Memory search result (id + distance)
#[derive(Debug)]
pub struct MemorySearchHit {
    pub id: u64,
    pub distance: f32,
}

/// Audit search result (id + severity)
#[derive(Debug)]
pub struct AuditSearchHit {
    pub id: u64,
    pub severity: f32,
}

// ============================================================================
// Brain
// ============================================================================

type CallbackBoxPtr =
    *mut Box<dyn Fn(CallbackEvent, &CallbackMetrics) -> CallbackAction>;

pub struct Brain {
    handle: *mut NimcpBrainHandle,
    _callback_ptrs: Vec<CallbackBoxPtr>,
    _swarm_master: *mut c_void,
    _swarm_edge: *mut c_void,
    _sensor_hub: *mut c_void,
    _watchdog: *mut c_void,
    _ros2_bridge: *mut c_void,
    _mavlink_bridge: *mut c_void,
}

impl Brain {
    pub fn new(
        name: &str, size: BrainSize, task: BrainTask,
        num_inputs: u32, num_outputs: u32,
    ) -> Result<Self> {
        let c_name = CString::new(name).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        unsafe {
            let handle = nimcp_brain_create(
                c_name.as_ptr(), size as c_int, task as c_int,
                num_inputs, num_outputs,
            );
            if handle.is_null() {
                Err(NimcpError::Generic(get_error_string()))
            } else {
                Ok(Brain { handle, _callback_ptrs: Vec::new(), _swarm_master: ptr::null_mut(), _swarm_edge: ptr::null_mut(), _sensor_hub: ptr::null_mut(), _watchdog: ptr::null_mut(), _ros2_bridge: ptr::null_mut(), _mavlink_bridge: ptr::null_mut() })
            }
        }
    }

    pub fn create_with_neurons(
        name: &str, task: BrainTask,
        num_inputs: u32, num_outputs: u32, neuron_count: u32,
    ) -> Result<Self> {
        let c_name = CString::new(name).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        unsafe {
            let handle = nimcp_brain_create_with_neurons(
                c_name.as_ptr(), task as c_int,
                num_inputs, num_outputs, neuron_count,
            );
            if handle.is_null() {
                Err(NimcpError::Generic(get_error_string()))
            } else {
                Ok(Brain { handle, _callback_ptrs: Vec::new(), _swarm_master: ptr::null_mut(), _swarm_edge: ptr::null_mut(), _sensor_hub: ptr::null_mut(), _watchdog: ptr::null_mut(), _ros2_bridge: ptr::null_mut(), _mavlink_bridge: ptr::null_mut() })
            }
        }
    }

    pub fn create_full(
        name: &str, task: BrainTask,
        num_inputs: u32, num_outputs: u32, neuron_count: u32,
    ) -> Result<Self> {
        let c_name = CString::new(name).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        unsafe {
            let handle = nimcp_brain_create_full(
                c_name.as_ptr(), task as c_int,
                num_inputs, num_outputs, neuron_count,
            );
            if handle.is_null() {
                Err(NimcpError::Generic(get_error_string()))
            } else {
                Ok(Brain { handle, _callback_ptrs: Vec::new(), _swarm_master: ptr::null_mut(), _swarm_edge: ptr::null_mut(), _sensor_hub: ptr::null_mut(), _watchdog: ptr::null_mut(), _ros2_bridge: ptr::null_mut(), _mavlink_bridge: ptr::null_mut() })
            }
        }
    }

    pub fn load(filepath: &str) -> Result<Self> {
        let c_path = CString::new(filepath).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        unsafe {
            let handle = nimcp_brain_load(c_path.as_ptr());
            if handle.is_null() {
                Err(NimcpError::Io(format!("Failed to load: {}", filepath)))
            } else {
                Ok(Brain { handle, _callback_ptrs: Vec::new(), _swarm_master: ptr::null_mut(), _swarm_edge: ptr::null_mut(), _sensor_hub: ptr::null_mut(), _watchdog: ptr::null_mut(), _ros2_bridge: ptr::null_mut(), _mavlink_bridge: ptr::null_mut() })
            }
        }
    }

    pub fn create_from_config(filepath: &str) -> Result<Self> {
        let c_path = CString::new(filepath).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        unsafe {
            let handle = nimcp_brain_create_from_config(c_path.as_ptr());
            if handle.is_null() {
                Err(NimcpError::Generic(get_error_string()))
            } else {
                Ok(Brain { handle, _callback_ptrs: Vec::new(), _swarm_master: ptr::null_mut(), _swarm_edge: ptr::null_mut(), _sensor_hub: ptr::null_mut(), _watchdog: ptr::null_mut(), _ros2_bridge: ptr::null_mut(), _mavlink_bridge: ptr::null_mut() })
            }
        }
    }

    // --- Core ---

    pub fn learn(&mut self, features: &[f32], label: &str, confidence: f32) -> Result<()> {
        let c_label = CString::new(label).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe {
            nimcp_brain_learn_example(
                self.handle, features.as_ptr(), features.len() as c_uint,
                c_label.as_ptr(), confidence,
            )
        })
    }

    pub fn predict(&self, features: &[f32]) -> Result<Prediction> {
        let mut label_buf = vec![0u8; 64];
        let mut confidence: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_predict(
                self.handle, features.as_ptr(), features.len() as c_uint,
                label_buf.as_mut_ptr() as *mut c_char, &mut confidence,
            )
        })?;
        let label = unsafe {
            CStr::from_ptr(label_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok(Prediction { label, confidence })
    }

    pub fn infer(&self, features: &[f32], outputs: &mut [f32]) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_infer(
                self.handle, features.as_ptr(), features.len() as c_uint,
                outputs.as_mut_ptr(), outputs.len() as c_uint,
            )
        })
    }

    pub fn save(&self, filepath: &str) -> Result<()> {
        let c_path = CString::new(filepath).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe { nimcp_brain_save(self.handle, c_path.as_ptr()) })
    }

    // --- Cognitive Decision ---

    pub fn decide_full(&self, features: &[f32], max_outputs: u32) -> Result<DecisionFull> {
        let mut label_buf = vec![0u8; 64];
        let mut confidence: f32 = 0.0;
        let mut explanation_buf = vec![0u8; 256];
        let mut output_vec = vec![0.0f32; max_outputs as usize];
        let mut output_size: u32 = max_outputs;
        let mut active_neurons: u32 = 0;
        let mut sparsity: f32 = 0.0;
        let mut inference_us: u64 = 0;
        check_status(unsafe {
            nimcp_brain_decide_full(
                self.handle, features.as_ptr(), features.len() as c_uint,
                label_buf.as_mut_ptr() as *mut c_char, &mut confidence,
                explanation_buf.as_mut_ptr() as *mut c_char,
                output_vec.as_mut_ptr(), &mut output_size,
                &mut active_neurons, &mut sparsity, &mut inference_us,
            )
        })?;
        let label = unsafe {
            CStr::from_ptr(label_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        let explanation = unsafe {
            CStr::from_ptr(explanation_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        output_vec.truncate(output_size as usize);
        Ok(DecisionFull {
            label, confidence, explanation, output_vector: output_vec,
            num_active_neurons: active_neurons, sparsity, inference_time_us: inference_us,
        })
    }

    // --- Language Production ---

    pub fn speak(
        &self, semantic_input: Option<&[f32]>, max_len: u32,
    ) -> Result<(String, f32, f32)> {
        let mut text_buf = vec![0u8; max_len as usize];
        let mut confidence: f32 = 0.0;
        let mut fluency: f32 = 0.0;
        let (sem_ptr, sem_dim) = match semantic_input {
            Some(s) => (s.as_ptr(), s.len() as c_uint),
            None => (ptr::null(), 0),
        };
        check_status(unsafe {
            nimcp_brain_speak(
                self.handle, sem_ptr, sem_dim,
                text_buf.as_mut_ptr() as *mut c_char, max_len,
                &mut confidence, &mut fluency,
            )
        })?;
        let text = unsafe {
            CStr::from_ptr(text_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok((text, confidence, fluency))
    }

    pub fn generate_text(
        &self, prompt: Option<&str>, semantic_input: Option<&[f32]>, max_len: u32,
    ) -> Result<(String, f32, f32)> {
        let mut text_buf = vec![0u8; max_len as usize];
        let mut confidence: f32 = 0.0;
        let mut perplexity: f32 = 0.0;
        let c_prompt = prompt.map(|p| CString::new(p).unwrap());
        let prompt_ptr = c_prompt.as_ref().map_or(ptr::null(), |s| s.as_ptr());
        let (sem_ptr, sem_dim) = match semantic_input {
            Some(s) => (s.as_ptr(), s.len() as c_uint),
            None => (ptr::null(), 0),
        };
        check_status(unsafe {
            nimcp_brain_generate_text(
                self.handle, prompt_ptr, sem_ptr, sem_dim,
                text_buf.as_mut_ptr() as *mut c_char, max_len,
                &mut confidence, &mut perplexity,
            )
        })?;
        let text = unsafe {
            CStr::from_ptr(text_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok((text, confidence, perplexity))
    }

    pub fn comprehend(&self, text: &str, semantic_dim: u32) -> Result<(Vec<f32>, f32)> {
        let c_text = CString::new(text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let mut semantic = vec![0.0f32; semantic_dim as usize];
        let mut confidence: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_comprehend(
                self.handle, c_text.as_ptr(),
                semantic.as_mut_ptr(), semantic_dim, &mut confidence,
            )
        })?;
        Ok((semantic, confidence))
    }

    pub fn produce_text(&self, intent: &[f32], max_len: u32) -> Result<(String, f32)> {
        let mut text_buf = vec![0u8; max_len as usize];
        let mut confidence: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_produce_text(
                self.handle, intent.as_ptr(), intent.len() as c_uint,
                text_buf.as_mut_ptr() as *mut c_char, max_len,
                &mut confidence,
            )
        })?;
        let text = unsafe {
            CStr::from_ptr(text_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok((text, confidence))
    }

    pub fn creative_blend(
        &self, vector_a: &[f32], vector_b: &[f32], blend_ratio: f32, max_len: u32,
    ) -> Result<String> {
        assert_eq!(vector_a.len(), vector_b.len(), "vectors must have same dimension");
        let mut text_buf = vec![0u8; max_len as usize];
        check_status(unsafe {
            nimcp_brain_creative_blend(
                self.handle, vector_a.as_ptr(), vector_b.as_ptr(),
                vector_a.len() as c_uint, blend_ratio,
                text_buf.as_mut_ptr() as *mut c_char, max_len,
            )
        })?;
        let text = unsafe {
            CStr::from_ptr(text_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok(text)
    }

    pub fn grounded_respond(&self, input_text: &str, max_len: u32) -> Result<(String, f32)> {
        let c_input = CString::new(input_text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let mut resp_buf = vec![0u8; max_len as usize];
        let mut confidence: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_grounded_respond(
                self.handle, c_input.as_ptr(),
                resp_buf.as_mut_ptr() as *mut c_char, max_len,
                &mut confidence,
            )
        })?;
        let text = unsafe {
            CStr::from_ptr(resp_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok((text, confidence))
    }

    // --- Metrics Getters ---

    pub fn get_accuracy(&self) -> f32 {
        unsafe { nimcp_brain_get_accuracy(self.handle) }
    }

    pub fn get_last_gradient_norm(&self) -> f32 {
        unsafe { nimcp_brain_get_last_gradient_norm(self.handle) }
    }

    pub fn get_last_loss(&self) -> f32 {
        unsafe { nimcp_brain_get_last_loss(self.handle) }
    }

    pub fn get_network_metrics(&self) -> Option<NetworkMetrics> {
        let mut ema_ann: f32 = 0.0;
        let mut ema_cnn: f32 = 0.0;
        let mut ema_snn: f32 = 0.0;
        let mut ema_lnn: f32 = 0.0;
        let mut ann_steps: u64 = 0;
        let mut cnn_steps: u64 = 0;
        let mut snn_steps: u64 = 0;
        let mut lnn_steps: u64 = 0;
        let ok = unsafe {
            nimcp_brain_get_network_metrics(
                self.handle,
                &mut ema_ann, &mut ema_cnn, &mut ema_snn, &mut ema_lnn,
                &mut ann_steps, &mut cnn_steps, &mut snn_steps, &mut lnn_steps,
            )
        };
        if ok {
            Some(NetworkMetrics {
                ema_ann, ema_cnn, ema_snn, ema_lnn,
                ann_steps, cnn_steps, snn_steps, lnn_steps,
            })
        } else {
            None
        }
    }

    pub fn get_cognitive_stats(&self) -> Result<CognitiveStats> {
        let mut stats = [0u32; 13];
        let mut losses = [0.0f32; 13];
        let mut count: u32 = 0;
        check_status(unsafe {
            nimcp_brain_get_cognitive_stats(
                self.handle, stats.as_mut_ptr(), losses.as_mut_ptr(), &mut count,
            )
        })?;
        let n = count as usize;
        Ok(CognitiveStats {
            step_counts: stats[..n].to_vec(),
            losses: losses[..n].to_vec(),
        })
    }

    pub fn get_avatar_state(&self) -> Result<AvatarState> {
        let mut state = std::mem::MaybeUninit::<AvatarState>::zeroed();
        check_status(unsafe {
            nimcp_brain_get_avatar_state(self.handle, state.as_mut_ptr())
        })?;
        Ok(unsafe { state.assume_init() })
    }

    pub fn get_last_transcript(&self, max_entries: u32) -> Vec<(String, f32, f32)> {
        let mut entries: Vec<[c_char; 256]> = vec![[0i8; 256]; max_entries as usize];
        let mut saliences = vec![0.0f32; max_entries as usize];
        let mut confidences = vec![0.0f32; max_entries as usize];
        let mut modules = vec![ptr::null::<c_char>(); max_entries as usize];
        let count = unsafe {
            nimcp_brain_get_last_transcript(
                self.handle, entries.as_mut_ptr(), saliences.as_mut_ptr(),
                confidences.as_mut_ptr(), modules.as_mut_ptr(), max_entries,
            )
        };
        let mut result = Vec::new();
        for i in 0..count as usize {
            let text = unsafe {
                CStr::from_ptr(entries[i].as_ptr())
                    .to_string_lossy().into_owned()
            };
            result.push((text, saliences[i], confidences[i]));
        }
        result
    }

    pub fn get_cortex_cnn_metrics(&self) -> Result<CortexCnnMetrics> {
        let mut types = [0i32; 4];
        let mut losses = [0.0f32; 4];
        let mut fwd_steps = [0u64; 4];
        let mut bwd_steps = [0u64; 4];
        let mut embed_norms = [0.0f32; 4];
        let mut count: u32 = 0;
        check_status(unsafe {
            nimcp_brain_get_cortex_cnn_metrics(
                self.handle, types.as_mut_ptr(), losses.as_mut_ptr(),
                fwd_steps.as_mut_ptr(), bwd_steps.as_mut_ptr(),
                embed_norms.as_mut_ptr(), &mut count,
            )
        })?;
        let n = count as usize;
        Ok(CortexCnnMetrics {
            types: types[..n].to_vec(),
            losses: losses[..n].to_vec(),
            fwd_steps: fwd_steps[..n].to_vec(),
            bwd_steps: bwd_steps[..n].to_vec(),
            embed_norms: embed_norms[..n].to_vec(),
        })
    }

    // --- Vector / Batch Learning ---

    pub fn learn_vector(
        &mut self, features: &[f32], target: &[f32],
        label: Option<&str>, confidence: f32,
    ) -> Result<()> {
        let c_label = label.map(|l| CString::new(l).unwrap());
        let label_ptr = c_label.as_ref().map_or(ptr::null(), |s| s.as_ptr());
        check_status(unsafe {
            nimcp_brain_learn_vector(
                self.handle, features.as_ptr(), features.len() as c_uint,
                target.as_ptr(), target.len() as c_uint,
                label_ptr, confidence,
            )
        })
    }

    pub fn learn_vector_batch(
        &mut self, features: &[&[f32]], targets: &[&[f32]],
        learning_rate: f32,
    ) -> f32 {
        assert_eq!(features.len(), targets.len());
        let num = features.len() as c_uint;
        let num_features = features.first().map_or(0, |f| f.len()) as c_uint;
        let target_size = targets.first().map_or(0, |t| t.len()) as c_uint;
        let feat_ptrs: Vec<*const c_float> = features.iter().map(|f| f.as_ptr()).collect();
        let tgt_ptrs: Vec<*const c_float> = targets.iter().map(|t| t.as_ptr()).collect();
        unsafe {
            nimcp_brain_learn_vector_batch(
                self.handle, feat_ptrs.as_ptr(), tgt_ptrs.as_ptr(),
                num_features, target_size, num, learning_rate,
            )
        }
    }

    pub fn learn_batch(
        &mut self, features: &[&[f32]], labels: &[&str],
        confidences: Option<&[f32]>,
    ) -> Result<Vec<f32>> {
        assert_eq!(features.len(), labels.len());
        let num = features.len() as c_uint;
        let feat_ptrs: Vec<*const c_float> = features.iter().map(|f| f.as_ptr()).collect();
        let feat_lens: Vec<c_uint> = features.iter().map(|f| f.len() as c_uint).collect();
        let c_labels: Vec<CString> = labels.iter()
            .map(|l| CString::new(*l).unwrap()).collect();
        let label_ptrs: Vec<*const c_char> = c_labels.iter().map(|l| l.as_ptr()).collect();
        let conf_ptr = confidences.map_or(ptr::null(), |c| c.as_ptr());
        let mut losses = vec![0.0f32; features.len()];
        check_status(unsafe {
            nimcp_brain_learn_batch(
                self.handle, feat_ptrs.as_ptr(), feat_lens.as_ptr(),
                label_ptrs.as_ptr(), conf_ptr, num, losses.as_mut_ptr(),
            )
        })?;
        Ok(losses)
    }

    // --- Knowledge / Language Learning ---

    pub fn learn_knowledge(&mut self, text: &str, domain: i32) -> Result<()> {
        let c_text = CString::new(text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe {
            nimcp_brain_learn_knowledge(self.handle, c_text.as_ptr(), domain)
        })
    }

    pub fn learn_language(&mut self, text: &str) -> Result<f32> {
        let c_text = CString::new(text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let mut loss: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_learn_language(self.handle, c_text.as_ptr(), &mut loss)
        })?;
        Ok(loss)
    }

    pub fn learn_language_pair(
        &mut self, input_text: &str, target_text: &str, learning_rate: f32,
    ) -> Result<f32> {
        let c_input = CString::new(input_text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let c_target = CString::new(target_text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let mut loss: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_learn_language_pair(
                self.handle, c_input.as_ptr(), c_target.as_ptr(),
                learning_rate, &mut loss,
            )
        })?;
        Ok(loss)
    }

    pub fn train_cognitive(
        &mut self, text: &str, domain: i32,
        target_text: Option<&str>, learning_rate: f32,
    ) -> Result<f32> {
        let c_text = CString::new(text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let c_target = target_text.map(|t| CString::new(t).unwrap());
        let target_ptr = c_target.as_ref().map_or(ptr::null(), |s| s.as_ptr());
        let mut loss: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_train_cognitive(
                self.handle, c_text.as_ptr(), domain,
                target_ptr, learning_rate, &mut loss,
            )
        })?;
        Ok(loss)
    }

    pub fn train_language(
        &mut self, input_text: &str, target_text: &str, learning_rate: f32,
    ) -> Result<f32> {
        let c_input = CString::new(input_text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let c_target = CString::new(target_text).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let mut loss: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_train_language(
                self.handle, c_input.as_ptr(), c_target.as_ptr(),
                learning_rate, &mut loss,
            )
        })?;
        Ok(loss)
    }

    // --- Fast / Domain Prediction ---

    pub fn predict_fast(&self, features: &[f32]) -> Result<Prediction> {
        let mut label_buf = vec![0u8; 64];
        let mut confidence: f32 = 0.0;
        check_status(unsafe {
            nimcp_brain_predict_fast(
                self.handle, features.as_ptr(), features.len() as c_uint,
                label_buf.as_mut_ptr() as *mut c_char, &mut confidence,
            )
        })?;
        let label = unsafe {
            CStr::from_ptr(label_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok(Prediction { label, confidence })
    }

    pub fn predict_in_domain(
        &self, features: &[f32], domain_prefix: Option<&str>,
    ) -> Result<Prediction> {
        let mut label_buf = vec![0u8; 64];
        let mut confidence: f32 = 0.0;
        let c_prefix = domain_prefix.map(|d| CString::new(d).unwrap());
        let prefix_ptr = c_prefix.as_ref().map_or(ptr::null(), |s| s.as_ptr());
        check_status(unsafe {
            nimcp_brain_predict_in_domain(
                self.handle, features.as_ptr(), features.len() as c_uint,
                prefix_ptr, label_buf.as_mut_ptr() as *mut c_char, &mut confidence,
            )
        })?;
        let label = unsafe {
            CStr::from_ptr(label_buf.as_ptr() as *const c_char)
                .to_string_lossy().into_owned()
        };
        Ok(Prediction { label, confidence })
    }

    // --- Freeze / Mode ---

    pub fn freeze(&mut self) -> Result<()> {
        check_status(unsafe { nimcp_brain_freeze(self.handle) })
    }

    pub fn is_frozen(&self) -> bool {
        unsafe { nimcp_brain_is_frozen(self.handle) }
    }

    pub fn enable_mixed_precision(&mut self, enable: bool) -> Result<()> {
        check_status(unsafe { nimcp_brain_enable_mixed_precision(self.handle, enable) })
    }

    pub fn enable_gradient_checkpointing(
        &mut self, enable: bool, checkpoint_interval: u32,
    ) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_enable_gradient_checkpointing(self.handle, enable, checkpoint_interval)
        })
    }

    pub fn enable_hemispheric(&mut self, enable: bool) -> Result<()> {
        check_status(unsafe { nimcp_brain_enable_hemispheric(self.handle, enable) })
    }

    pub fn enable_recurrent(
        &mut self, enable: bool, max_iterations: u32,
        confidence_threshold: f32, blend_alpha: f32,
    ) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_enable_recurrent(
                self.handle, enable, max_iterations, confidence_threshold, blend_alpha,
            )
        })
    }

    pub fn enable_bptt(&mut self, enable: bool, window_size: u32, discount: f32) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_enable_bptt(self.handle, enable, window_size, discount)
        })
    }

    pub fn enable_multi_network(&mut self) -> Result<()> {
        check_status(unsafe { nimcp_brain_enable_multi_network(self.handle) })
    }

    pub fn enable_biological_plasticity(&mut self, enabled: bool) -> Result<()> {
        check_status(unsafe { nimcp_brain_enable_biological_plasticity(self.handle, enabled) })
    }

    pub fn set_fast_training(&mut self, enabled: bool) -> Result<()> {
        check_status(unsafe { nimcp_brain_set_fast_training(self.handle, enabled) })
    }

    pub fn set_task_type(&mut self, task_type: &str) -> Result<()> {
        let c_task = CString::new(task_type).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe { nimcp_brain_set_task_type(self.handle, c_task.as_ptr()) })
    }

    pub fn set_training_mode(&mut self, active: bool) {
        unsafe { nimcp_brain_set_training_mode(self.handle, active) }
    }

    pub fn set_network_ablation(&mut self, train_cnn: i32, train_snn: i32, train_lnn: i32) {
        unsafe { nimcp_brain_set_network_ablation(self.handle, train_cnn, train_snn, train_lnn) }
    }

    // --- Per-network training toggles (runtime-dynamic, no rebuild required) ---

    pub fn set_train_ann(&mut self, enabled: bool) {
        unsafe { nimcp_brain_set_train_ann(self.handle, enabled) }
    }
    pub fn get_train_ann(&self) -> bool {
        unsafe { nimcp_brain_get_train_ann(self.handle) }
    }

    pub fn set_train_cnn(&mut self, enabled: bool) {
        unsafe { nimcp_brain_set_train_cnn(self.handle, enabled) }
    }
    pub fn get_train_cnn(&self) -> bool {
        unsafe { nimcp_brain_get_train_cnn(self.handle) }
    }

    pub fn set_train_snn(&mut self, enabled: bool) {
        unsafe { nimcp_brain_set_train_snn(self.handle, enabled) }
    }
    pub fn get_train_snn(&self) -> bool {
        unsafe { nimcp_brain_get_train_snn(self.handle) }
    }

    pub fn set_train_lnn(&mut self, enabled: bool) {
        unsafe { nimcp_brain_set_train_lnn(self.handle, enabled) }
    }
    pub fn get_train_lnn(&self) -> bool {
        unsafe { nimcp_brain_get_train_lnn(self.handle) }
    }

    /// Convenience preset: freeze ANN/CNN/LNN while keeping SNN training.
    /// Used for SNN-only recovery after large BPTT behavior changes.
    pub fn set_snn_only_recovery(&mut self, enabled: bool) {
        unsafe { nimcp_brain_set_snn_only_recovery(self.handle, enabled) }
    }
    pub fn get_snn_only_recovery(&self) -> bool {
        unsafe { nimcp_brain_get_snn_only_recovery(self.handle) }
    }

    /// Ensemble warmup scale [0.0, 1.0] — probabilistic gate on non-SNN
    /// training. 1.0 = full-rate (default), 0.0 = fully frozen,
    /// intermediate = Monte-Carlo skip. Used by the daemon plateau
    /// detector to ramp ANN/CNN/LNN back in gradually after snn-only
    /// recovery. Out-of-range values are clamped on the C side.
    pub fn set_ensemble_warmup_scale(&mut self, scale: f32) {
        unsafe { nimcp_brain_set_ensemble_warmup_scale(self.handle, scale as c_float) }
    }
    pub fn get_ensemble_warmup_scale(&self) -> f32 {
        unsafe { nimcp_brain_get_ensemble_warmup_scale(self.handle) as f32 }
    }

    // --- Sensory / Cortex ---

    pub fn submit_sensory(
        &mut self, modality: &str, data: &[f32],
        width: u32, height: u32, channels: u32, n_segments: u32,
    ) -> Result<()> {
        let c_mod = CString::new(modality).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe {
            nimcp_brain_submit_sensory(
                self.handle, c_mod.as_ptr(), data.as_ptr(), data.len() as c_uint,
                width, height, channels, n_segments,
            )
        })
    }

    pub fn visual_cortex_process(
        &mut self, pixels: &[f32],
        width: u32, height: u32, channels: u32, max_features: u32,
    ) -> Result<Vec<f32>> {
        let mut features = vec![0.0f32; max_features as usize];
        let mut count: u32 = 0;
        check_status(unsafe {
            nimcp_brain_visual_cortex_process(
                self.handle, pixels.as_ptr(), pixels.len() as c_uint,
                width, height, channels, features.as_mut_ptr(), max_features, &mut count,
            )
        })?;
        features.truncate(count as usize);
        Ok(features)
    }

    // --- Brain Region Probes ---

    pub fn medulla_get_arousal(&self) -> f32 {
        unsafe { nimcp_brain_medulla_get_arousal(self.handle) }
    }

    pub fn bg_get_dopamine(&self) -> f32 {
        unsafe { nimcp_brain_bg_get_dopamine(self.handle) }
    }

    pub fn sleep_get_pressure(&self) -> f32 {
        unsafe { nimcp_brain_sleep_get_pressure(self.handle) }
    }

    pub fn substrate_get_health(&self) -> Result<String> {
        let mut buf = vec![0u8; 64];
        check_status(unsafe {
            nimcp_brain_substrate_get_health(self.handle, buf.as_mut_ptr() as *mut c_char, 64)
        })?;
        let text = unsafe {
            CStr::from_ptr(buf.as_ptr() as *const c_char).to_string_lossy().into_owned()
        };
        Ok(text)
    }

    // --- Sub-network Creation / Stats ---

    pub fn lnn_create(
        &mut self, n_sensory: u32, n_inter: u32, n_command: u32, n_output: u32,
    ) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_lnn_create(self.handle, n_sensory, n_inter, n_command, n_output)
        })
    }

    pub fn lnn_get_stats(&self) -> Result<LnnStats> {
        let (mut fwd, mut bwd, mut ode) = (0u64, 0u64, 0u64);
        let (mut tau, mut state, mut grad) = (0.0f32, 0.0f32, 0.0f32);
        let (mut nan, mut inf) = (0u32, 0u32);
        check_status(unsafe {
            nimcp_brain_lnn_get_stats(
                self.handle, &mut fwd, &mut bwd, &mut ode,
                &mut tau, &mut state, &mut grad, &mut nan, &mut inf,
            )
        })?;
        Ok(LnnStats {
            forward_steps: fwd, backward_steps: bwd, ode_evals: ode,
            avg_tau: tau, state_norm: state, gradient_norm: grad,
            nan_count: nan, inf_count: inf,
        })
    }

    pub fn snn_get_stats(&self) -> Result<SnnStats> {
        let (mut steps, mut spikes) = (0u64, 0u64);
        let (mut rate, mut sparsity, mut sync) = (0.0f32, 0.0f32, 0.0f32);
        let (mut silent, mut hyper) = (0u32, 0u32);
        let mut health: c_int = 0;
        let mut mem: usize = 0;
        check_status(unsafe {
            nimcp_brain_snn_get_stats(
                self.handle, &mut steps, &mut spikes,
                &mut rate, &mut sparsity, &mut sync,
                &mut silent, &mut hyper, &mut health, &mut mem,
            )
        })?;
        Ok(SnnStats {
            total_steps: steps, total_spikes: spikes,
            mean_firing_rate: rate, sparsity, synchrony: sync,
            silent_neurons: silent, hyperactive_neurons: hyper,
            health, memory_bytes: mem,
        })
    }

    pub fn snn_set_input_scale(&self, scale: f32) {
        unsafe { nimcp_snn_set_input_scale(scale) }
    }
    pub fn snn_get_input_scale(&self) -> f32 {
        unsafe { nimcp_snn_get_input_scale() }
    }

    pub fn cnn_get_stats(&self) -> Result<CnnStats> {
        let (mut layers, mut labels) = (0u32, 0u32);
        let mut params: usize = 0;
        let mut active: bool = false;
        check_status(unsafe {
            nimcp_brain_cnn_get_stats(self.handle, &mut layers, &mut params, &mut labels, &mut active)
        })?;
        Ok(CnnStats { num_layers: layers, num_parameters: params, num_labels: labels, active })
    }

    // --- Rubric ---

    pub fn rubric(&self) -> Result<RubricData> {
        let mut r = std::mem::MaybeUninit::<RubricData>::zeroed();
        check_status(unsafe { nimcp_brain_rubric(self.handle, r.as_mut_ptr()) })?;
        Ok(unsafe { r.assume_init() })
    }

    pub fn broadcast_rubric(&self) -> Result<()> {
        check_status(unsafe { nimcp_brain_broadcast_rubric(self.handle) })
    }

    pub fn set_rubric_validation(&mut self, features: &[f32]) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_set_rubric_validation(
                self.handle, features.as_ptr(), features.len() as c_uint,
            )
        })
    }

    pub fn get_rubric_training_stats(&self) -> Result<RubricTrainingStats> {
        let (mut eval_count, mut min_s, mut max_s, mut avg_s) = (0u64, 0.0f32, 0.0f32, 0.0f32);
        let mut last = std::mem::MaybeUninit::<RubricData>::zeroed();
        check_status(unsafe {
            nimcp_brain_get_rubric_training_stats(
                self.handle, &mut eval_count, &mut min_s, &mut max_s, &mut avg_s,
                last.as_mut_ptr(),
            )
        })?;
        Ok(RubricTrainingStats {
            eval_count, min_score: min_s, max_score: max_s, avg_score: avg_s,
            last_rubric: unsafe { last.assume_init() },
        })
    }

    // --- Experience ---

    pub fn experience(
        &mut self, input: &[f32], output: &mut [f32], teacher_reward: f32,
    ) -> Result<ExperienceResult> {
        let mut result = std::mem::MaybeUninit::<ExperienceResult>::zeroed();
        check_status(unsafe {
            nimcp_brain_experience(
                self.handle, input.as_ptr(), input.len() as c_uint,
                output.as_mut_ptr(), output.len() as c_uint,
                teacher_reward, result.as_mut_ptr(),
            )
        })?;
        Ok(unsafe { result.assume_init() })
    }

    pub fn experience_configure(&mut self, config: &ExperienceConfig) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_experience_configure(self.handle, config as *const ExperienceConfig)
        })
    }

    pub fn experience_correct(&mut self, expected: &[f32]) -> f32 {
        unsafe {
            nimcp_brain_experience_correct(self.handle, expected.as_ptr(), expected.len() as c_uint)
        }
    }

    pub fn experience_attend(&mut self, modality: &str, strength: f32) -> Result<()> {
        let c_mod = CString::new(modality).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe { nimcp_brain_experience_attend(self.handle, c_mod.as_ptr(), strength) })
    }

    // --- Grounded Language ---

    pub fn ground_word(
        &mut self, word: &str, features: &[f32], modality: u32, attention: f32,
    ) -> Result<()> {
        let c_word = CString::new(word).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe {
            nimcp_brain_ground_word(
                self.handle, c_word.as_ptr(),
                features.as_ptr(), features.len() as c_uint, modality, attention,
            )
        })
    }

    pub fn innate_hardwire(&mut self, config: *const c_void) -> Result<()> {
        check_status(unsafe { nimcp_brain_innate_hardwire(self.handle, config) })
    }

    // --- Cloud ---

    pub fn connect_cloud(
        &mut self, cloud: &Brain, confidence_threshold: f32, enable_distillation: bool,
    ) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_connect_cloud(
                self.handle, cloud.handle, confidence_threshold, enable_distillation,
            )
        })
    }

    pub fn disconnect_cloud(&mut self) -> Result<()> {
        check_status(unsafe { nimcp_brain_disconnect_cloud(self.handle) })
    }

    pub fn distill_cloud_batch(&mut self, max_examples: u32) -> u32 {
        unsafe { nimcp_brain_distill_cloud_batch(self.handle, max_examples) }
    }

    pub fn get_cloud_stats(&self) -> Result<CloudStats> {
        let (mut total, mut local, mut escalated, mut distill) = (0u64, 0u64, 0u64, 0u64);
        check_status(unsafe {
            nimcp_brain_get_cloud_stats(
                self.handle, &mut total, &mut local, &mut escalated, &mut distill,
            )
        })?;
        Ok(CloudStats {
            total_queries: total, local_handled: local,
            cloud_escalated: escalated, distillation_steps: distill,
        })
    }

    pub fn connect_collective(&mut self, other: &Brain, instance_id: u32) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_connect_collective(self.handle, other.handle, instance_id)
        })
    }

    // --- Attention / Hemispheric ---

    pub fn focus_attention(&mut self, modality: &str) -> Result<()> {
        let c_mod = CString::new(modality).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe { nimcp_brain_focus_attention(self.handle, c_mod.as_ptr()) })
    }

    pub fn get_hemispheric_balance(&self) -> f32 {
        unsafe { nimcp_brain_get_hemispheric_balance(self.handle) }
    }

    pub fn get_callosum_transfers(&self) -> u64 {
        unsafe { nimcp_brain_get_callosum_transfers(self.handle) }
    }

    pub fn get_lateralization(&self, domain: u32) -> f32 {
        unsafe { nimcp_brain_get_lateralization(self.handle, domain) }
    }

    pub fn shift_lateralization(&mut self, domain: u32, shift: f32) -> Result<()> {
        check_status(unsafe { nimcp_brain_shift_lateralization(self.handle, domain, shift) })
    }

    pub fn get_recurrent_iterations(&self) -> u32 {
        unsafe { nimcp_brain_get_recurrent_iterations(self.handle) }
    }

    // --- Health Probes ---

    pub fn get_immune_metrics(&self) -> Result<ImmuneMetrics> {
        let mut m = std::mem::MaybeUninit::<ImmuneMetrics>::zeroed();
        check_status(unsafe { nimcp_brain_get_immune_metrics(self.handle, m.as_mut_ptr()) })?;
        Ok(unsafe { m.assume_init() })
    }

    pub fn get_sparsity_ratio(&self) -> f32 {
        unsafe { nimcp_brain_get_sparsity_ratio(self.handle) }
    }

    pub fn get_synapse_stats(&self) -> Result<SynapseStats> {
        let mut s = std::mem::MaybeUninit::<SynapseStats>::zeroed();
        check_status(unsafe { nimcp_brain_get_synapse_stats(self.handle, s.as_mut_ptr()) })?;
        Ok(unsafe { s.assume_init() })
    }

    pub fn get_active_neuron_count(&self) -> u32 {
        unsafe { nimcp_brain_get_active_neuron_count(self.handle) }
    }

    pub fn get_neuron_utilization(&self) -> f32 {
        unsafe { nimcp_brain_get_neuron_utilization(self.handle) }
    }

    pub fn get_gpu_vram_used(&self) -> usize {
        unsafe { nimcp_brain_get_gpu_vram_used(self.handle) }
    }

    // --- Training Pipeline ---

    pub fn configure_training(&mut self, config: &TrainingConfig) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_configure_training(self.handle, config as *const TrainingConfig)
        })
    }

    pub fn train_step(&mut self, features: &[f32], targets: &[f32]) -> Result<TrainingResult> {
        let mut result = std::mem::MaybeUninit::<TrainingResult>::zeroed();
        check_status(unsafe {
            nimcp_brain_train_step(
                self.handle, features.as_ptr(), features.len() as c_uint,
                targets.as_ptr(), targets.len() as c_uint, result.as_mut_ptr(),
            )
        })?;
        Ok(unsafe { result.assume_init() })
    }

    pub fn train_batch(
        &mut self, features: &[f32], targets: &[f32],
        batch_size: u32, num_features: u32, num_targets: u32,
    ) -> Result<TrainingResult> {
        let mut result = std::mem::MaybeUninit::<TrainingResult>::zeroed();
        check_status(unsafe {
            nimcp_brain_train_batch(
                self.handle, features.as_ptr(), targets.as_ptr(),
                batch_size, num_features, num_targets, result.as_mut_ptr(),
            )
        })?;
        Ok(unsafe { result.assume_init() })
    }

    pub fn get_training_stats(&self) -> Result<TrainingStats> {
        let (mut steps, mut loss, mut lr) = (0u64, 0.0f32, 0.0f32);
        check_status(unsafe {
            nimcp_brain_get_training_stats(self.handle, &mut steps, &mut loss, &mut lr)
        })?;
        Ok(TrainingStats { total_steps: steps, total_loss: loss, current_lr: lr })
    }

    pub fn step_scheduler(&mut self, validation_metric: f32) -> f32 {
        unsafe { nimcp_brain_step_scheduler(self.handle, validation_metric) }
    }

    // --- Callbacks ---

    pub fn enable_callbacks(&mut self, config: &CallbackConfig) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_enable_callbacks(self.handle, config as *const CallbackConfig)
        })
    }

    pub fn disable_callbacks(&mut self) -> Result<()> {
        check_status(unsafe { nimcp_brain_disable_callbacks(self.handle) })
    }

    pub fn register_callback<F>(
        &mut self, event: CallbackEvent, callback: F, name: Option<&str>,
    ) -> Result<u32>
    where
        F: Fn(CallbackEvent, &CallbackMetrics) -> CallbackAction + 'static,
    {
        let boxed: Box<dyn Fn(CallbackEvent, &CallbackMetrics) -> CallbackAction> =
            Box::new(callback);
        let raw = Box::into_raw(Box::new(boxed));

        extern "C" fn trampoline(
            event: c_int, metrics: *const CallbackMetrics, user_data: *mut c_void,
        ) -> c_int {
            let cb = unsafe {
                &*(user_data
                    as *const Box<dyn Fn(CallbackEvent, &CallbackMetrics) -> CallbackAction>)
            };
            let evt = unsafe { std::mem::transmute::<c_int, CallbackEvent>(event) };
            let m = if metrics.is_null() {
                &CallbackMetrics {
                    step: 0, epoch: 0, loss: 0.0, loss_ema: 0.0,
                    learning_rate: 0.0, gradient_norm: 0.0, step_time_us: 0,
                    is_converging: false, is_diverging: false,
                }
            } else {
                unsafe { &*metrics }
            };
            cb(evt, m) as c_int
        }

        let c_name = name.map(|n| CString::new(n).unwrap());
        let name_ptr = c_name.as_ref().map_or(ptr::null(), |s| s.as_ptr());

        let id = unsafe {
            nimcp_brain_register_callback(
                self.handle, event as c_int, trampoline,
                raw as *mut c_void, name_ptr,
            )
        };

        if id == 0 {
            let _ = unsafe { Box::from_raw(raw) };
            return Err(NimcpError::Generic("Failed to register callback".into()));
        }

        self._callback_ptrs.push(raw);
        Ok(id)
    }

    pub fn unregister_callback(&mut self, callback_id: u32) -> Result<()> {
        check_status(unsafe { nimcp_brain_unregister_callback(self.handle, callback_id) })
    }

    pub fn get_callback_stats(&self) -> Result<CallbackStats> {
        let (mut fired, mut avg, mut early) = (0u64, 0.0f32, 0u32);
        check_status(unsafe {
            nimcp_brain_get_callback_stats(self.handle, &mut fired, &mut avg, &mut early)
        })?;
        Ok(CallbackStats { total_fired: fired, avg_time_us: avg, early_stops: early })
    }

    // --- Resize ---

    pub fn resize(&mut self, neuron_count: u32) -> bool {
        unsafe { nimcp_brain_resize(self.handle, neuron_count) }
    }

    pub fn auto_resize(&mut self) -> bool {
        unsafe { nimcp_brain_auto_resize(self.handle) }
    }

    pub fn get_neuron_count(&self) -> u32 {
        unsafe { nimcp_brain_get_neuron_count(self.handle) }
    }

    pub fn get_utilization_metrics(&self) -> Option<UtilizationMetrics> {
        let (mut util, mut sat) = (0.0f32, 0.0f32);
        let ok = unsafe { nimcp_brain_get_utilization_metrics(self.handle, &mut util, &mut sat) };
        if ok { Some(UtilizationMetrics { utilization: util, saturation: sat }) } else { None }
    }

    // --- Named Snapshots ---

    pub fn snapshot_save(&self, name: &str, description: Option<&str>) -> Result<()> {
        let c_name = CString::new(name).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let c_desc = description.map(|d| CString::new(d).unwrap());
        let desc_ptr = c_desc.as_ref().map_or(ptr::null(), |s| s.as_ptr());
        check_status(unsafe { nimcp_brain_snapshot_save(self.handle, c_name.as_ptr(), desc_ptr) })
    }

    pub fn snapshot_restore(&self, name: &str) -> Result<Brain> {
        let c_name = CString::new(name).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        unsafe {
            let h = nimcp_brain_snapshot_restore(self.handle, c_name.as_ptr());
            if h.is_null() {
                Err(NimcpError::Generic("Failed to restore snapshot".into()))
            } else {
                Ok(Brain { handle: h, _callback_ptrs: Vec::new(), _swarm_master: ptr::null_mut(), _swarm_edge: ptr::null_mut(), _sensor_hub: ptr::null_mut(), _watchdog: ptr::null_mut(), _ros2_bridge: ptr::null_mut(), _mavlink_bridge: ptr::null_mut() })
            }
        }
    }

    pub fn snapshot_delete(&self, name: &str) -> Result<()> {
        let c_name = CString::new(name).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe { nimcp_brain_snapshot_delete(self.handle, c_name.as_ptr()) })
    }

    // --- COW ---

    pub fn clone_cow(&self) -> Result<Brain> {
        unsafe {
            let h = nimcp_brain_clone_cow(self.handle);
            if h.is_null() {
                Err(NimcpError::Generic("Failed to clone brain".into()))
            } else {
                Ok(Brain { handle: h, _callback_ptrs: Vec::new(), _swarm_master: ptr::null_mut(), _swarm_edge: ptr::null_mut(), _sensor_hub: ptr::null_mut(), _watchdog: ptr::null_mut(), _ros2_bridge: ptr::null_mut(), _mavlink_bridge: ptr::null_mut() })
            }
        }
    }

    pub fn snapshot_cow(&self) -> Result<BrainSnapshot> {
        unsafe {
            let h = nimcp_brain_snapshot_cow(self.handle);
            if h.is_null() {
                Err(NimcpError::Generic("Failed to create snapshot".into()))
            } else {
                Ok(BrainSnapshot { handle: h })
            }
        }
    }

    pub fn restore_cow(&mut self, snapshot: &BrainSnapshot) -> Result<()> {
        check_status(unsafe { nimcp_brain_restore_cow(self.handle, snapshot.handle) })
    }

    // --- Working Memory ---

    pub fn working_memory_add(&mut self, data: &[f32], salience: f32) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_working_memory_add(
                self.handle, data.as_ptr(), data.len() as c_uint, salience,
            )
        })
    }

    pub fn working_memory_get(&self, index: u32) -> Option<Vec<f32>> {
        let mut size: u32 = 0;
        let ptr = unsafe { nimcp_brain_working_memory_get(self.handle, index, &mut size) };
        if ptr.is_null() || size == 0 {
            None
        } else {
            let mut result = vec![0.0f32; size as usize];
            unsafe { ptr::copy_nonoverlapping(ptr, result.as_mut_ptr(), size as usize) };
            Some(result)
        }
    }

    pub fn working_memory_stats(&self) -> Result<WorkingMemoryStats> {
        let (mut cur, mut cap) = (0u32, 0u32);
        check_status(unsafe {
            nimcp_brain_working_memory_stats(self.handle, &mut cur, &mut cap)
        })?;
        Ok(WorkingMemoryStats { current_size: cur, capacity: cap })
    }

    pub fn working_memory_refresh(&mut self, index: u32) -> Result<()> {
        check_status(unsafe { nimcp_brain_working_memory_refresh(self.handle, index) })
    }

    // --- Workspace ---

    pub fn workspace_compete(
        &mut self, module: CognitiveModule, content: &[f32], strength: f32,
    ) -> Result<()> {
        check_status(unsafe {
            nimcp_brain_workspace_compete(
                self.handle, module as c_int, content.as_ptr(),
                content.len() as c_uint, strength,
            )
        })
    }

    pub fn workspace_read(&self, max_dim: u32) -> Result<WorkspaceReadResult> {
        let mut buf = vec![0.0f32; max_dim as usize];
        let (mut actual_dim, mut source) = (0u32, 0i32);
        check_status(unsafe {
            nimcp_brain_workspace_read(
                self.handle, buf.as_mut_ptr(), max_dim, &mut actual_dim, &mut source,
            )
        })?;
        buf.truncate(actual_dim as usize);
        Ok(WorkspaceReadResult {
            content: buf, actual_dim,
            source_module: unsafe { std::mem::transmute::<c_int, CognitiveModule>(source) },
        })
    }

    pub fn workspace_subscribe(&mut self, module: CognitiveModule) -> Result<()> {
        check_status(unsafe { nimcp_brain_workspace_subscribe(self.handle, module as c_int) })
    }

    pub fn workspace_unsubscribe(&mut self, module: CognitiveModule) -> Result<()> {
        check_status(unsafe { nimcp_brain_workspace_unsubscribe(self.handle, module as c_int) })
    }

    pub fn workspace_has_broadcast(&self) -> Result<bool> {
        let mut has: bool = false;
        check_status(unsafe { nimcp_brain_workspace_has_broadcast(self.handle, &mut has) })?;
        Ok(has)
    }

    pub fn workspace_stats(&self) -> Result<WorkspaceStats> {
        let (mut bc, mut comp, mut avg) = (0u32, 0u32, 0.0f32);
        check_status(unsafe {
            nimcp_brain_workspace_stats(self.handle, &mut bc, &mut comp, &mut avg)
        })?;
        Ok(WorkspaceStats { total_broadcasts: bc, total_competitions: comp, avg_strength: avg })
    }

    // --- Oscillations ---

    pub fn enable_oscillations(&mut self, enable: bool) -> bool {
        unsafe { nimcp_enable_complex_oscillations(self.handle, enable) }
    }

    pub fn is_oscillations_enabled(&self) -> bool {
        unsafe { nimcp_is_complex_oscillations_enabled(self.handle) }
    }

    pub fn get_phasor(&self, neuron_id: u32) -> Phasor {
        unsafe { nimcp_get_oscillation_phasor(self.handle, neuron_id) }
    }

    pub fn get_phase_coherence(&self, neuron_ids: &[u32]) -> f32 {
        unsafe {
            nimcp_get_phase_coherence(self.handle, neuron_ids.as_ptr(), neuron_ids.len() as c_uint)
        }
    }

    pub fn get_pac_modulation(&self, theta_freq: f32, gamma_freq: f32) -> f32 {
        unsafe { nimcp_get_pac_modulation(self.handle, theta_freq, gamma_freq) }
    }

    // --- Probe ---

    pub fn probe(&self) -> Result<BrainProbe> {
        let mut raw = std::mem::MaybeUninit::<BrainProbeData>::zeroed();
        check_status(unsafe { nimcp_brain_probe(self.handle, raw.as_mut_ptr()) })?;
        let raw = unsafe { raw.assume_init() };
        let task_name = {
            let end = raw.task_name.iter().position(|&b| b == 0).unwrap_or(64);
            String::from_utf8_lossy(&raw.task_name[..end]).into_owned()
        };
        Ok(BrainProbe {
            task_name,
            size: unsafe { std::mem::transmute::<c_int, BrainSize>(raw.size) },
            task: unsafe { std::mem::transmute::<c_int, BrainTask>(raw.task) },
            num_neurons: raw.num_neurons, num_synapses: raw.num_synapses,
            num_active_synapses: raw.num_active_synapses,
            total_inferences: raw.total_inferences,
            total_learning_steps: raw.total_learning_steps,
            avg_sparsity: raw.avg_sparsity,
            avg_inference_time_us: raw.avg_inference_time_us,
            current_learning_rate: raw.current_learning_rate,
            accuracy: raw.accuracy, memory_bytes: raw.memory_bytes,
            num_inputs: raw.num_inputs, num_outputs: raw.num_outputs,
            is_cow_clone: raw.is_cow_clone, cow_ref_count: raw.cow_ref_count,
            cow_shared_bytes: raw.cow_shared_bytes, cow_private_bytes: raw.cow_private_bytes,
        })
    }

    pub fn broadcast_probe(&self) -> Result<()> {
        check_status(unsafe { nimcp_brain_broadcast_probe(self.handle) })
    }

    // --- Edge Brain ---

    /// Score neuron importance (activity, connectivity, weight magnitude).
    pub fn edge_score_importance(&self, num_neurons: u32) -> Vec<f32> {
        let mut scores = vec![0.0f32; num_neurons as usize];
        unsafe {
            nimcp_edge_score_neuron_importance(
                self.handle, scores.as_mut_ptr(), num_neurons);
        }
        scores
    }

    /// Resize brain neuron count at runtime.
    /// mode: 0=contract, 1=expand, 2=rebalance
    pub fn edge_resize(&mut self, target_neurons: u32, mode: i32,
                       knowledge_transfer: bool) -> Result<()> {
        unsafe {
            let mut config = nimcp_resize_config_default();
            // Set target_neuron_count at offset 0 (u32)
            let ptr = config.as_mut_ptr() as *mut u32;
            *ptr = target_neurons;
            // Set mode at offset 4 (i32)
            let mode_ptr = (config.as_mut_ptr() as *mut i32).add(1);
            *mode_ptr = mode;
            // Set knowledge_transfer at offset 8 (bool)
            let kt_ptr = config.as_mut_ptr().add(8) as *mut bool;
            *kt_ptr = knowledge_transfer;
            check_status(nimcp_edge_brain_resize(
                self.handle, config.as_ptr() as *const c_void))
        }
    }

    /// Dry-run resize feasibility check.
    pub fn edge_resize_check(&self, target_neurons: u32) -> Result<(bool, u32, u32, f32)> {
        unsafe {
            let mut config = nimcp_resize_config_default();
            let ptr = config.as_mut_ptr() as *mut u32;
            *ptr = target_neurons;
            let mut report = [0u8; 512];
            check_status(nimcp_edge_brain_resize_check(
                self.handle, config.as_ptr() as *const c_void,
                report.as_mut_ptr() as *mut c_void))?;
            let feasible = *(report.as_ptr() as *const bool);
            let neurons_before = *((report.as_ptr() as *const u32).add(1));
            let neurons_after = *((report.as_ptr() as *const u32).add(2));
            let ram_delta = *((report.as_ptr() as *const f32).add(3));
            Ok((feasible, neurons_before, neurons_after, ram_delta))
        }
    }

    /// Distill brain into a smaller student.
    pub fn edge_distill(&self, target_neurons: u32, temperature: f32,
                        steps: u32, include_snn: bool,
                        include_lnn: bool, include_cnn: bool) -> Result<()> {
        unsafe {
            let mut config = nimcp_distill_config_default();
            let ptr = config.as_mut_ptr();
            *(ptr as *mut u32) = target_neurons;
            *((ptr as *mut f32).add(1)) = temperature;
            *((ptr as *mut u32).add(2)) = steps;
            *(ptr.add(12) as *mut bool) = include_snn;
            *(ptr.add(13) as *mut bool) = include_lnn;
            *(ptr.add(14) as *mut bool) = include_cnn;
            let mut student: *mut NimcpBrainHandle = ptr::null_mut();
            let mut report = [0u8; 256];
            check_status(nimcp_brain_distill(
                self.handle, &mut student,
                config.as_ptr() as *const c_void,
                report.as_mut_ptr() as *mut c_void))
        }
    }

    /// Auto-optimize brain for a target device.
    pub fn edge_optimize_for_device(&mut self, ram_mb: u32, cpu_cores: u32,
                                     has_camera: bool, has_imu: bool,
                                     has_motor_control: bool, has_network: bool,
                                     role: i32) -> Result<()> {
        unsafe {
            let mut profile = nimcp_device_profile_default();
            let ptr = profile.as_mut_ptr();
            *(ptr as *mut u32) = ram_mb;
            *((ptr as *mut u32).add(1)) = cpu_cores;
            *(ptr.add(8) as *mut bool) = has_camera;
            *(ptr.add(9) as *mut bool) = has_imu;
            *(ptr.add(10) as *mut bool) = has_motor_control;
            *(ptr.add(11) as *mut bool) = has_network;
            *((ptr as *mut i32).add(3)) = role;
            let mut child: *mut NimcpBrainHandle = ptr::null_mut();
            let mut report = [0u8; 256];
            check_status(nimcp_brain_optimize_for_device(
                self.handle, profile.as_ptr() as *const c_void,
                &mut child, report.as_mut_ptr() as *mut c_void))
        }
    }

    /// Quantize brain weights in-place.
    pub fn edge_quantize(&mut self, precision: i32,
                         calibration_samples: u32) -> Result<()> {
        unsafe {
            let mut config = nimcp_quantize_config_default();
            let ptr = config.as_mut_ptr();
            *(ptr as *mut i32) = precision;
            *((ptr as *mut u32).add(1)) = calibration_samples;
            check_status(nimcp_brain_quantize(
                self.handle, config.as_ptr() as *const c_void))
        }
    }

    // --- Swarm / Sensor / Watchdog / ROS2 / MAVLink ---

    /// Create a swarm master. Returns opaque handle stored internally.
    pub fn swarm_master_create(&mut self, device_id: u32, listen_port: u16) -> Result<()> {
        unsafe {
            let mut cfg = nimcp_swarm_master_config_default();
            *(cfg.as_mut_ptr() as *mut u32) = device_id;
            *((cfg.as_mut_ptr() as *mut u16).add(2)) = listen_port;
            let master = nimcp_swarm_master_create(self.handle, cfg.as_ptr() as *const c_void);
            if master.is_null() { return Err(NimcpError::Generic("failed to create swarm master".into())); }
            self._swarm_master = master;
            Ok(())
        }
    }
    pub fn swarm_master_destroy(&mut self) { unsafe { if !self._swarm_master.is_null() { nimcp_swarm_master_destroy(self._swarm_master); self._swarm_master = ptr::null_mut(); } } }
    pub fn swarm_master_start(&self) -> i32 { if self._swarm_master.is_null() { return -1; } unsafe { nimcp_swarm_master_start(self._swarm_master) } }
    pub fn swarm_master_stop(&self) -> i32 { if self._swarm_master.is_null() { return -1; } unsafe { nimcp_swarm_master_stop(self._swarm_master) } }
    pub fn swarm_master_kick(&self, device_id: u32) -> i32 { if self._swarm_master.is_null() { return -1; } unsafe { nimcp_swarm_master_kick(self._swarm_master, device_id) } }
    pub fn swarm_master_force_sync(&self) -> i32 { if self._swarm_master.is_null() { return -1; } unsafe { nimcp_swarm_master_force_sync(self._swarm_master) } }
    pub fn swarm_master_get_peer_count(&self) -> u32 { if self._swarm_master.is_null() { return 0; } unsafe { nimcp_swarm_master_get_peer_count(self._swarm_master) } }

    pub fn swarm_edge_create(&mut self, device_id: u32) -> Result<()> {
        unsafe {
            let mut cfg = nimcp_swarm_edge_config_default();
            *(cfg.as_mut_ptr() as *mut u32) = device_id;
            let edge = nimcp_swarm_edge_create(self.handle, cfg.as_ptr() as *const c_void);
            if edge.is_null() { return Err(NimcpError::Generic("failed to create swarm edge".into())); }
            self._swarm_edge = edge;
            Ok(())
        }
    }
    pub fn swarm_edge_destroy(&mut self) { unsafe { if !self._swarm_edge.is_null() { nimcp_swarm_edge_destroy(self._swarm_edge); self._swarm_edge = ptr::null_mut(); } } }
    pub fn swarm_edge_start(&self) -> i32 { if self._swarm_edge.is_null() { return -1; } unsafe { nimcp_swarm_edge_start(self._swarm_edge) } }
    pub fn swarm_edge_stop(&self) -> i32 { if self._swarm_edge.is_null() { return -1; } unsafe { nimcp_swarm_edge_stop(self._swarm_edge) } }
    pub fn swarm_edge_is_connected(&self) -> bool { if self._swarm_edge.is_null() { return false; } unsafe { nimcp_swarm_edge_is_connected(self._swarm_edge) } }
    pub fn swarm_edge_submit_gradients(&self, gradients: &[f32]) -> i32 { if self._swarm_edge.is_null() { return -1; } unsafe { nimcp_swarm_edge_submit_gradients(self._swarm_edge, gradients.as_ptr(), gradients.len() as c_uint) } }

    pub fn sensor_hub_create(&mut self, max_sensors: u32) -> Result<()> {
        unsafe { let hub = nimcp_sensor_hub_create(max_sensors); if hub.is_null() { return Err(NimcpError::Generic("failed to create sensor hub".into())); } self._sensor_hub = hub; Ok(()) }
    }
    pub fn sensor_hub_destroy(&mut self) { unsafe { if !self._sensor_hub.is_null() { nimcp_sensor_hub_destroy(self._sensor_hub); self._sensor_hub = ptr::null_mut(); } } }
    pub fn sensor_get_count(&self) -> u32 { if self._sensor_hub.is_null() { return 0; } unsafe { nimcp_sensor_get_count(self._sensor_hub) } }
    pub fn sensor_compose_features(&self, max_features: u32) -> Vec<f32> {
        if self._sensor_hub.is_null() { return vec![]; }
        let mut features = vec![0.0f32; max_features as usize];
        let count = unsafe { nimcp_sensor_compose_feature_vector(self._sensor_hub, features.as_mut_ptr(), max_features) };
        if count < 0 { return vec![]; }
        features.truncate(count as usize);
        features
    }

    pub fn watchdog_create(&mut self) -> Result<()> {
        unsafe { let cfg = nimcp_watchdog_config_default(); let wd = nimcp_watchdog_create(cfg.as_ptr() as *const c_void); if wd.is_null() { return Err(NimcpError::Generic("failed to create watchdog".into())); } self._watchdog = wd; Ok(()) }
    }
    pub fn watchdog_destroy(&mut self) { unsafe { if !self._watchdog.is_null() { nimcp_watchdog_destroy(self._watchdog); self._watchdog = ptr::null_mut(); } } }
    pub fn watchdog_arm(&self) -> i32 { if self._watchdog.is_null() { return -1; } unsafe { nimcp_watchdog_arm(self._watchdog) } }
    pub fn watchdog_disarm(&self) -> i32 { if self._watchdog.is_null() { return -1; } unsafe { nimcp_watchdog_disarm(self._watchdog) } }
    pub fn watchdog_heartbeat(&self) { if !self._watchdog.is_null() { unsafe { nimcp_watchdog_heartbeat(self._watchdog) } } }
    pub fn watchdog_validate_output(&self, output: &mut [f32]) -> bool { if self._watchdog.is_null() { return false; } unsafe { nimcp_watchdog_validate_output(self._watchdog, output.as_mut_ptr(), output.len() as c_uint) == 0 } }
    pub fn watchdog_get_safe_output(&self, num: u32) -> Vec<f32> { if self._watchdog.is_null() { return vec![]; } let mut out = vec![0.0f32; num as usize]; unsafe { nimcp_watchdog_get_safe_output(self._watchdog, out.as_mut_ptr(), num); } out }
    pub fn watchdog_estop(&self) { if !self._watchdog.is_null() { unsafe { nimcp_watchdog_estop(self._watchdog) } } }
    pub fn watchdog_reset(&self) -> i32 { if self._watchdog.is_null() { return -1; } unsafe { nimcp_watchdog_reset(self._watchdog) } }
    pub fn watchdog_get_state(&self) -> String { if self._watchdog.is_null() { return "NONE".into(); } unsafe { let s = nimcp_watchdog_get_state(self._watchdog); CStr::from_ptr(nimcp_watchdog_state_name(s)).to_string_lossy().into_owned() } }

    pub fn ros2_bridge_create(&mut self) -> Result<()> {
        unsafe { let cfg = nimcp_ros2_config_default(); let br = nimcp_ros2_bridge_create(self.handle, cfg.as_ptr() as *const c_void); if br.is_null() { return Err(NimcpError::Generic("failed to create ROS2 bridge".into())); } self._ros2_bridge = br; Ok(()) }
    }
    pub fn ros2_bridge_destroy(&mut self) { unsafe { if !self._ros2_bridge.is_null() { nimcp_ros2_bridge_destroy(self._ros2_bridge); self._ros2_bridge = ptr::null_mut(); } } }
    pub fn ros2_bridge_start(&self) -> i32 { if self._ros2_bridge.is_null() { return -1; } unsafe { nimcp_ros2_bridge_start(self._ros2_bridge) } }
    pub fn ros2_bridge_stop(&self) -> i32 { if self._ros2_bridge.is_null() { return -1; } unsafe { nimcp_ros2_bridge_stop(self._ros2_bridge) } }
    pub fn ros2_bridge_inject_sensor(&self, topic: &str, data: &[f32]) -> i32 { if self._ros2_bridge.is_null() { return -1; } let ct = CString::new(topic).unwrap_or_default(); unsafe { nimcp_ros2_bridge_inject_sensor(self._ros2_bridge, ct.as_ptr(), data.as_ptr(), data.len() as c_uint) } }
    pub fn ros2_bridge_get_last_cmd(&self, max: u32) -> Vec<f32> { if self._ros2_bridge.is_null() { return vec![]; } let mut data = vec![0.0f32; max as usize]; let got = unsafe { nimcp_ros2_bridge_get_last_cmd(self._ros2_bridge, data.as_mut_ptr(), max) }; if got < 0 { return vec![]; } data.truncate(got as usize); data }

    pub fn mavlink_create(&mut self) -> Result<()> {
        unsafe { let cfg = nimcp_mavlink_config_default(); let br = nimcp_mavlink_bridge_create(cfg.as_ptr() as *const c_void); if br.is_null() { return Err(NimcpError::Generic("failed to create MAVLink bridge".into())); } self._mavlink_bridge = br; Ok(()) }
    }
    pub fn mavlink_destroy(&mut self) { unsafe { if !self._mavlink_bridge.is_null() { nimcp_mavlink_bridge_destroy(self._mavlink_bridge); self._mavlink_bridge = ptr::null_mut(); } } }
    pub fn mavlink_connect(&self) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_bridge_connect(self._mavlink_bridge) } }
    pub fn mavlink_disconnect(&self) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_bridge_disconnect(self._mavlink_bridge) } }
    pub fn mavlink_start(&self) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_bridge_start(self._mavlink_bridge) } }
    pub fn mavlink_stop(&self) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_bridge_stop(self._mavlink_bridge) } }
    pub fn mavlink_set_velocity(&self, vx: f32, vy: f32, vz: f32, yaw_rate: f32) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_set_velocity(self._mavlink_bridge, vx, vy, vz, yaw_rate) } }
    pub fn mavlink_arm(&self, arm: bool) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_arm(self._mavlink_bridge, arm) } }
    pub fn mavlink_takeoff(&self, alt: f32) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_takeoff(self._mavlink_bridge, alt) } }
    pub fn mavlink_land(&self) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_land(self._mavlink_bridge) } }
    pub fn mavlink_goto(&self, lat: f64, lon: f64, alt: f32) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_goto(self._mavlink_bridge, lat, lon, alt) } }
    pub fn mavlink_rtl(&self) -> i32 { if self._mavlink_bridge.is_null() { return -1; } unsafe { nimcp_mavlink_rtl(self._mavlink_bridge) } }
    pub fn mavlink_compose_features(&self) -> Vec<f32> { if self._mavlink_bridge.is_null() { return vec![]; } let mut features = vec![0.0f32; 14]; let count = unsafe { nimcp_mavlink_compose_features(self._mavlink_bridge, features.as_mut_ptr(), 14) }; if count < 0 { return vec![]; } features.truncate(count as usize); features }

    // --- Memory Store ---

    /// Get memory store statistics.
    pub fn memory_store_stats(&self) -> Option<MemoryStoreStats> {
        let mut raw = MemoryStoreStatsRaw::default();
        let ret = unsafe { nimcp_brain_memory_store_stats(self.handle, &mut raw) };
        if ret != 0 { return None; }
        Some(MemoryStoreStats {
            total_engrams: raw.total_engrams,
            total_concepts: raw.total_concepts,
            total_relations: raw.total_relations,
            total_autobio: raw.total_autobio,
            total_writes: raw.total_writes,
            total_reads: raw.total_reads,
            cache_hits: raw.cache_hits,
            cache_misses: raw.cache_misses,
            db_size_bytes: raw.db_size_bytes,
        })
    }

    /// Search memory by text (FTS5 on engram labels).
    pub fn memory_search_text(&self, query: &str, max_results: u32) -> Vec<u64> {
        let c_query = CString::new(query).unwrap_or_default();
        let mut ids = vec![0u64; max_results as usize];
        let mut count = 0u32;
        unsafe {
            nimcp_brain_memory_search_text(
                self.handle, c_query.as_ptr(), max_results,
                ids.as_mut_ptr(), &mut count);
        }
        ids.truncate(count as usize);
        ids
    }

    /// Search memory by embedding similarity.
    pub fn memory_search_similar(&self, embedding: &[f32],
                                  top_k: u32) -> Vec<MemorySearchHit> {
        let mut ids = vec![0u64; top_k as usize];
        let mut distances = vec![0.0f32; top_k as usize];
        let mut count = 0u32;
        unsafe {
            nimcp_brain_memory_search_similar(
                self.handle, embedding.as_ptr(),
                embedding.len() as c_uint, top_k,
                ids.as_mut_ptr(), distances.as_mut_ptr(), &mut count);
        }
        (0..count as usize).map(|i| MemorySearchHit {
            id: ids[i], distance: distances[i]
        }).collect()
    }

    /// Check if memory store is healthy.
    pub fn memory_is_healthy(&self) -> bool {
        unsafe { nimcp_brain_memory_is_healthy(self.handle) }
    }

    // --- OOD Detection ---

    /// Get OOD detector statistics.
    pub fn ood_stats(&self) -> Option<OodStatsResult> {
        let mut raw = OodStatsRaw::default();
        let ret = unsafe { nimcp_brain_ood_stats(self.handle, &mut raw) };
        if ret != 0 { return None; }
        Some(OodStatsResult {
            total_checks: raw.total_checks,
            ood_detected: raw.ood_detected,
            in_distribution: raw.in_distribution,
            avg_ood_score: raw.avg_ood_score,
            ood_rate: raw.ood_rate,
        })
    }

    // --- Audit Trail ---

    /// Log an audit event.
    pub fn audit_log(&self, description: &str, severity: u32,
                     details: &str) -> i32 {
        let c_desc = CString::new(description).unwrap_or_default();
        let c_details = CString::new(details).unwrap_or_default();
        unsafe {
            nimcp_brain_audit_log(
                self.handle, c_desc.as_ptr(), severity, c_details.as_ptr())
        }
    }

    /// Search audit trail by minimum severity.
    pub fn audit_search(&self, min_severity: u32,
                        max_results: u32) -> Vec<AuditSearchHit> {
        let mut ids = vec![0u64; max_results as usize];
        let mut severities = vec![0.0f32; max_results as usize];
        let mut count = 0u32;
        unsafe {
            nimcp_brain_audit_search(
                self.handle, min_severity, max_results,
                ids.as_mut_ptr(), severities.as_mut_ptr(), &mut count);
        }
        (0..count as usize).map(|i| AuditSearchHit {
            id: ids[i], severity: severities[i]
        }).collect()
    }
}

impl Drop for Brain {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { nimcp_brain_destroy(self.handle) }
        }
        for ptr in self._callback_ptrs.drain(..) {
            if !ptr.is_null() {
                let _ = unsafe { Box::from_raw(ptr) };
            }
        }
    }
}

// ============================================================================
// BrainSnapshot (COW)
// ============================================================================

pub struct BrainSnapshot {
    handle: *mut NimcpSnapshotHandle,
}

impl Drop for BrainSnapshot {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { nimcp_brain_snapshot_destroy(self.handle) }
        }
    }
}

// ============================================================================
// Network
// ============================================================================

pub struct Network {
    handle: *mut NimcpNetworkHandle,
    num_outputs: usize,
}

impl Network {
    pub fn new(
        num_inputs: u32, num_outputs: u32, num_hidden: u32, learning_rate: f32,
    ) -> Result<Self> {
        unsafe {
            let handle = nimcp_network_create(num_inputs, num_outputs, num_hidden, learning_rate);
            if handle.is_null() {
                Err(NimcpError::Generic(get_error_string()))
            } else {
                Ok(Network { handle, num_outputs: num_outputs as usize })
            }
        }
    }

    pub fn forward(&self, inputs: &[f32]) -> Result<Vec<f32>> {
        let mut outputs = vec![0.0f32; self.num_outputs];
        check_status(unsafe {
            nimcp_network_forward(
                self.handle, inputs.as_ptr(), inputs.len() as c_uint,
                outputs.as_mut_ptr(), outputs.len() as c_uint,
            )
        })?;
        Ok(outputs)
    }

    pub fn train(&mut self, inputs: &[f32], targets: &[f32]) -> Result<()> {
        check_status(unsafe {
            nimcp_network_train(
                self.handle, inputs.as_ptr(), inputs.len() as c_uint,
                targets.as_ptr(), targets.len() as c_uint,
            )
        })
    }
}

impl Drop for Network {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { nimcp_network_destroy(self.handle) }
        }
    }
}

// ============================================================================
// Ethics
// ============================================================================

pub struct Ethics {
    handle: *mut NimcpEthicsHandle,
}

impl Ethics {
    pub fn new() -> Result<Self> {
        unsafe {
            let handle = nimcp_ethics_create();
            if handle.is_null() {
                Err(NimcpError::Generic(get_error_string()))
            } else {
                Ok(Ethics { handle })
            }
        }
    }

    pub fn check(&self, situation: &[f32]) -> Result<f32> {
        let mut score: f32 = 0.0;
        check_status(unsafe {
            nimcp_ethics_check(self.handle, situation.as_ptr(), situation.len() as c_uint, &mut score)
        })?;
        Ok(score)
    }
}

impl Drop for Ethics {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { nimcp_ethics_destroy(self.handle) }
        }
    }
}

// ============================================================================
// KnowledgeGraph
// ============================================================================

pub struct KnowledgeGraph {
    handle: *mut NimcpKnowledgeHandle,
}

impl KnowledgeGraph {
    pub fn new() -> Result<Self> {
        unsafe {
            let handle = nimcp_knowledge_create();
            if handle.is_null() {
                Err(NimcpError::Generic(get_error_string()))
            } else {
                Ok(KnowledgeGraph { handle })
            }
        }
    }

    pub fn add_fact(&mut self, subject: &str, predicate: &str, object: &str) -> Result<()> {
        let c_sub = CString::new(subject).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let c_pred = CString::new(predicate).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let c_obj = CString::new(object).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        check_status(unsafe {
            nimcp_knowledge_add_fact(self.handle, c_sub.as_ptr(), c_pred.as_ptr(), c_obj.as_ptr())
        })
    }

    pub fn query(&self, query: &str, max_len: u32) -> Result<String> {
        let c_query = CString::new(query).map_err(|e| NimcpError::Invalid(e.to_string()))?;
        let mut buf = vec![0u8; max_len as usize];
        check_status(unsafe {
            nimcp_knowledge_query(
                self.handle, c_query.as_ptr(), buf.as_mut_ptr() as *mut c_char, max_len,
            )
        })?;
        let result = unsafe {
            CStr::from_ptr(buf.as_ptr() as *const c_char).to_string_lossy().into_owned()
        };
        Ok(result)
    }
}

impl Drop for KnowledgeGraph {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { nimcp_knowledge_destroy(self.handle) }
        }
    }
}

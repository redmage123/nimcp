/**
 * @file nimcp.hpp
 * @brief Idiomatic C++20 bindings for NIMCP
 * @version 2.6.4
 *
 * Single-header C++ wrapper providing RAII classes, strongly-typed enums,
 * exception-based error handling, and std::function callbacks over the
 * NIMCP C API (nimcp.h).
 *
 * Usage:
 *   #include <nimcp.hpp>
 *
 *   nimcp::Library lib;  // RAII init/shutdown
 *   nimcp::Brain brain("test", nimcp::BrainSize::Tiny,
 *                      nimcp::TaskType::Classification, 4, 2);
 *   brain.learn({1.0f, 0.0f, 0.5f, 0.3f}, "cat", 0.9f);
 *   auto [label, confidence] = brain.predict({1.0f, 0.0f, 0.5f, 0.3f});
 */

#ifndef NIMCP_HPP
#define NIMCP_HPP

#include <nimcp.h>
#include <edge/nimcp_edge.h>
#include <edge/nimcp_swarm_runtime.h>
#include <edge/nimcp_sensor.h>
#include <edge/nimcp_safety_watchdog.h>
#include <edge/nimcp_ros2_bridge.h>
#include <edge/nimcp_mavlink_bridge.h>
#include <memory/nimcp_memory_store.h>
#include <cognitive/nimcp_ood_detector.h>

// Forward declarations for brain-handle bridge functions
// (defined in nimcp_part_bindings.c, linked into libnimcp)
extern "C" {
int nimcp_brain_memory_store_stats(nimcp_brain_t brain, nimcp_memory_store_stats_t* stats);
int nimcp_brain_memory_search_text(nimcp_brain_t brain, const char* query,
    uint32_t max_results, uint64_t* out_ids, uint32_t* out_count);
int nimcp_brain_memory_search_similar(nimcp_brain_t brain, const float* embedding,
    uint32_t dim, uint32_t top_k, uint64_t* out_ids, float* out_distances,
    uint32_t* out_count);
bool nimcp_brain_memory_is_healthy(nimcp_brain_t brain);
int nimcp_brain_ood_stats(nimcp_brain_t brain, nimcp_ood_stats_t* stats);
int nimcp_brain_audit_log(nimcp_brain_t brain, const char* description,
    uint32_t severity, const char* details);
int nimcp_brain_audit_search(nimcp_brain_t brain, uint32_t min_severity,
    uint32_t max_results, uint64_t* out_ids, float* out_severities,
    uint32_t* out_count);
}

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nimcp {

// ============================================================================
// Exception Hierarchy
// ============================================================================

class Exception : public std::runtime_error {
public:
    explicit Exception(nimcp_status_t code, const char* msg)
        : std::runtime_error(msg), code_(code) {}

    nimcp_status_t code() const noexcept { return code_; }

private:
    nimcp_status_t code_;
};

class NullArgError : public Exception {
public:
    explicit NullArgError(const char* msg)
        : Exception(NIMCP_ERROR_NULL_ARG, msg) {}
};

class InvalidError : public Exception {
public:
    explicit InvalidError(const char* msg)
        : Exception(NIMCP_ERROR_INVALID, msg) {}
};

class MemoryError : public Exception {
public:
    explicit MemoryError(const char* msg)
        : Exception(NIMCP_ERROR_MEMORY, msg) {}
};

class IOError : public Exception {
public:
    explicit IOError(const char* msg)
        : Exception(NIMCP_ERROR_IO, msg) {}
};

inline void check_status(nimcp_status_t s) {
    if (s == NIMCP_OK) return;
    const char* msg = nimcp_get_error();
    if (!msg || msg[0] == '\0') msg = "NIMCP error";
    switch (s) {
    case NIMCP_ERROR_NULL_ARG: throw NullArgError(msg);
    case NIMCP_ERROR_INVALID:  throw InvalidError(msg);
    case NIMCP_ERROR_MEMORY:   throw MemoryError(msg);
    case NIMCP_ERROR_IO:       throw IOError(msg);
    default:                   throw Exception(s, msg);
    }
}

// ============================================================================
// Strongly-typed Enum Classes
// ============================================================================

enum class BrainSize : int {
    Tiny   = NIMCP_BRAIN_TINY,
    Small  = NIMCP_BRAIN_SMALL,
    Medium = NIMCP_BRAIN_MEDIUM,
    Large  = NIMCP_BRAIN_LARGE,
};

enum class TaskType : int {
    Classification  = NIMCP_TASK_CLASSIFICATION,
    Regression      = NIMCP_TASK_REGRESSION,
    PatternMatching = NIMCP_TASK_PATTERN_MATCHING,
    Sequence        = NIMCP_TASK_SEQUENCE,
    Association     = NIMCP_TASK_ASSOCIATION,
};

enum class NetworkType : int {
    Adaptive = NIMCP_NETWORK_ADAPTIVE,
    SNN      = NIMCP_NETWORK_SNN,
    LNN      = NIMCP_NETWORK_LNN,
    CNN      = NIMCP_NETWORK_CNN,
    Hybrid   = NIMCP_NETWORK_HYBRID,
};

enum class SNNTrainMethod : int {
    STDP        = NIMCP_SNN_TRAIN_STDP,
    RSTDP       = NIMCP_SNN_TRAIN_R_STDP,
    EProp       = NIMCP_SNN_TRAIN_EPROP,
    Surrogate   = NIMCP_SNN_TRAIN_SURROGATE,
    Homeostatic = NIMCP_SNN_TRAIN_HOMEOSTATIC,
};

enum class LNNTrainMethod : int {
    Adjoint = NIMCP_LNN_TRAIN_ADJOINT,
    BPTT    = NIMCP_LNN_TRAIN_BPTT,
    RTRL    = NIMCP_LNN_TRAIN_RTRL,
    EProp   = NIMCP_LNN_TRAIN_EPROP,
};

enum class LossType : int {
    MSE          = NIMCP_API_LOSS_MSE,
    CrossEntropy = NIMCP_API_LOSS_CROSS_ENTROPY,
    BinaryCE     = NIMCP_API_LOSS_BINARY_CE,
    Huber        = NIMCP_API_LOSS_HUBER,
    MAE          = NIMCP_API_LOSS_MAE,
    Focal        = NIMCP_API_LOSS_FOCAL,
    KLDiv        = NIMCP_API_LOSS_KL_DIV,
};

enum class OptimizerType : int {
    SGD      = NIMCP_API_OPT_SGD,
    Momentum = NIMCP_API_OPT_MOMENTUM,
    Adam     = NIMCP_API_OPT_ADAM,
    AdamW    = NIMCP_API_OPT_ADAMW,
    RMSprop  = NIMCP_API_OPT_RMSPROP,
    Adagrad  = NIMCP_API_OPT_ADAGRAD,
};

enum class SchedulerType : int {
    Constant        = NIMCP_API_SCHED_CONSTANT,
    Step            = NIMCP_API_SCHED_STEP,
    Exponential     = NIMCP_API_SCHED_EXPONENTIAL,
    Cosine          = NIMCP_API_SCHED_COSINE,
    WarmupCosine    = NIMCP_API_SCHED_WARMUP_COSINE,
    ReduceOnPlateau = NIMCP_API_SCHED_REDUCE_ON_PLATEAU,
    Cyclic          = NIMCP_API_SCHED_CYCLIC,
};

enum class CallbackEvent : int {
    StepComplete   = NIMCP_CB_STEP_COMPLETE,
    EpochComplete  = NIMCP_CB_EPOCH_COMPLETE,
    LossComputed   = NIMCP_CB_LOSS_COMPUTED,
    WeightsUpdated = NIMCP_CB_WEIGHTS_UPDATED,
    LRChanged      = NIMCP_CB_LR_CHANGED,
    Convergence    = NIMCP_CB_CONVERGENCE,
    Divergence     = NIMCP_CB_DIVERGENCE,
    Checkpoint     = NIMCP_CB_CHECKPOINT,
};

enum class CallbackAction : int {
    Continue   = NIMCP_CB_ACTION_CONTINUE,
    Stop       = NIMCP_CB_ACTION_STOP,
    Skip       = NIMCP_CB_ACTION_SKIP,
    Rollback   = NIMCP_CB_ACTION_ROLLBACK,
    ReduceLR   = NIMCP_CB_ACTION_REDUCE_LR,
    IncreaseLR = NIMCP_CB_ACTION_INCREASE_LR,
};

enum class CognitiveModule : int {
    None             = NIMCP_MODULE_NONE,
    Perception       = NIMCP_MODULE_PERCEPTION,
    WorkingMemory    = NIMCP_MODULE_WORKING_MEMORY,
    Executive        = NIMCP_MODULE_EXECUTIVE,
    TheoryOfMind     = NIMCP_MODULE_THEORY_OF_MIND,
    Ethics           = NIMCP_MODULE_ETHICS,
    Attention        = NIMCP_MODULE_ATTENTION,
    Emotion          = NIMCP_MODULE_EMOTION,
    Salience         = NIMCP_MODULE_SALIENCE,
    Motor            = NIMCP_MODULE_MOTOR,
    Language         = NIMCP_MODULE_LANGUAGE,
    Metacognition    = NIMCP_MODULE_METACOGNITION,
    Curiosity        = NIMCP_MODULE_CURIOSITY,
    Introspection    = NIMCP_MODULE_INTROSPECTION,
    Predictive       = NIMCP_MODULE_PREDICTIVE,
    Consolidation    = NIMCP_MODULE_CONSOLIDATION,
    EpisodicMemory   = NIMCP_MODULE_EPISODIC_MEMORY,
    SemanticMemory   = NIMCP_MODULE_SEMANTIC_MEMORY,
    Wellbeing        = NIMCP_MODULE_WELLBEING,
    MentalHealth     = NIMCP_MODULE_MENTAL_HEALTH,
    GoalMotivation   = NIMCP_MODULE_GOAL_MOTIVATION,
    CognitiveControl = NIMCP_MODULE_COGNITIVE_CONTROL,
    CustomStart      = NIMCP_MODULE_CUSTOM_START,
};

// ============================================================================
// C++ Config/Result Structs
// ============================================================================

struct TrainingConfig {
    LossType loss_type           = LossType::CrossEntropy;
    OptimizerType optimizer_type = OptimizerType::Adam;
    SchedulerType scheduler_type = SchedulerType::Cosine;

    float learning_rate = 0.001f;
    float weight_decay  = 0.0f;
    float momentum      = 0.9f;
    float beta1         = 0.9f;
    float beta2         = 0.999f;
    float epsilon       = 1e-8f;

    uint32_t scheduler_step_size = 100;
    float scheduler_gamma        = 0.1f;
    uint32_t warmup_steps        = 0;

    bool enable_gradient_clipping = false;
    float gradient_clip_value     = 1.0f;

    bool enable_biological_modulation = true;
    float biological_blend            = 0.5f;

    NetworkType network_type = NetworkType::Adaptive;

    SNNTrainMethod snn_method    = SNNTrainMethod::STDP;
    float snn_eligibility_tau    = 20.0f;
    float snn_reward_tau         = 100.0f;
    float snn_surrogate_beta     = 5.0f;

    LNNTrainMethod lnn_method          = LNNTrainMethod::Adjoint;
    uint32_t lnn_bptt_truncation       = 100;
    bool lnn_use_adjoint_checkpointing = true;

    nimcp_training_config_t to_c() const {
        nimcp_training_config_t c{};
        c.loss_type      = static_cast<nimcp_api_loss_t>(loss_type);
        c.optimizer_type = static_cast<nimcp_api_optimizer_t>(optimizer_type);
        c.scheduler_type = static_cast<nimcp_api_scheduler_t>(scheduler_type);
        c.learning_rate  = learning_rate;
        c.weight_decay   = weight_decay;
        c.momentum       = momentum;
        c.beta1          = beta1;
        c.beta2          = beta2;
        c.epsilon        = epsilon;
        c.scheduler_step_size = scheduler_step_size;
        c.scheduler_gamma     = scheduler_gamma;
        c.warmup_steps        = warmup_steps;
        c.enable_gradient_clipping  = enable_gradient_clipping;
        c.gradient_clip_value       = gradient_clip_value;
        c.enable_biological_modulation = enable_biological_modulation;
        c.biological_blend             = biological_blend;
        c.network_type    = static_cast<nimcp_network_type_t>(network_type);
        c.snn_method      = static_cast<nimcp_snn_train_method_t>(snn_method);
        c.snn_eligibility_tau = snn_eligibility_tau;
        c.snn_reward_tau      = snn_reward_tau;
        c.snn_surrogate_beta  = snn_surrogate_beta;
        c.lnn_method          = static_cast<nimcp_lnn_train_method_t>(lnn_method);
        c.lnn_bptt_truncation = lnn_bptt_truncation;
        c.lnn_use_adjoint_checkpointing = lnn_use_adjoint_checkpointing;
        return c;
    }

    static TrainingConfig from_default() {
        auto c = nimcp_training_config_default();
        TrainingConfig cfg;
        cfg.loss_type      = static_cast<LossType>(c.loss_type);
        cfg.optimizer_type = static_cast<OptimizerType>(c.optimizer_type);
        cfg.scheduler_type = static_cast<SchedulerType>(c.scheduler_type);
        cfg.learning_rate  = c.learning_rate;
        cfg.weight_decay   = c.weight_decay;
        cfg.momentum       = c.momentum;
        cfg.beta1          = c.beta1;
        cfg.beta2          = c.beta2;
        cfg.epsilon        = c.epsilon;
        cfg.scheduler_step_size = c.scheduler_step_size;
        cfg.scheduler_gamma     = c.scheduler_gamma;
        cfg.warmup_steps        = c.warmup_steps;
        cfg.enable_gradient_clipping  = c.enable_gradient_clipping;
        cfg.gradient_clip_value       = c.gradient_clip_value;
        cfg.enable_biological_modulation = c.enable_biological_modulation;
        cfg.biological_blend             = c.biological_blend;
        cfg.network_type    = static_cast<NetworkType>(c.network_type);
        cfg.snn_method      = static_cast<SNNTrainMethod>(c.snn_method);
        cfg.snn_eligibility_tau = c.snn_eligibility_tau;
        cfg.snn_reward_tau      = c.snn_reward_tau;
        cfg.snn_surrogate_beta  = c.snn_surrogate_beta;
        cfg.lnn_method          = static_cast<LNNTrainMethod>(c.lnn_method);
        cfg.lnn_bptt_truncation = c.lnn_bptt_truncation;
        cfg.lnn_use_adjoint_checkpointing = c.lnn_use_adjoint_checkpointing;
        return cfg;
    }
};

struct CallbackConfig {
    bool enable_auto_checkpoint  = false;
    uint32_t checkpoint_interval = 1000;
    bool enable_early_stopping   = false;
    uint32_t patience            = 10;
    float min_delta              = 0.0001f;
    float divergence_threshold   = 2.0f;
    uint32_t log_interval        = 0;

    nimcp_callback_config_t to_c() const {
        nimcp_callback_config_t c{};
        c.enable_auto_checkpoint = enable_auto_checkpoint;
        c.checkpoint_interval    = checkpoint_interval;
        c.enable_early_stopping  = enable_early_stopping;
        c.patience               = patience;
        c.min_delta              = min_delta;
        c.divergence_threshold   = divergence_threshold;
        c.log_interval           = log_interval;
        return c;
    }

    static CallbackConfig from_default() {
        auto c = nimcp_callback_config_default();
        CallbackConfig cfg;
        cfg.enable_auto_checkpoint = c.enable_auto_checkpoint;
        cfg.checkpoint_interval    = c.checkpoint_interval;
        cfg.enable_early_stopping  = c.enable_early_stopping;
        cfg.patience               = c.patience;
        cfg.min_delta              = c.min_delta;
        cfg.divergence_threshold   = c.divergence_threshold;
        cfg.log_interval           = c.log_interval;
        return cfg;
    }
};

struct TrainingResult {
    float loss           = 0.0f;
    float learning_rate  = 0.0f;
    uint32_t step        = 0;
    bool early_stopped   = false;
    float gradient_norm  = 0.0f;

    static TrainingResult from_c(const nimcp_training_result_t& c) {
        return {c.loss, c.learning_rate, c.step, c.early_stopped, c.gradient_norm};
    }
};

struct CallbackMetrics {
    uint64_t step         = 0;
    uint64_t epoch        = 0;
    float loss            = 0.0f;
    float loss_ema        = 0.0f;
    float learning_rate   = 0.0f;
    float gradient_norm   = 0.0f;
    uint64_t step_time_us = 0;
    bool is_converging    = false;
    bool is_diverging     = false;

    static CallbackMetrics from_c(const nimcp_callback_metrics_t& c) {
        return {c.step, c.epoch, c.loss, c.loss_ema, c.learning_rate,
                c.gradient_norm, c.step_time_us, c.is_converging, c.is_diverging};
    }
};

struct SnapshotInfo {
    std::string name;
    std::string description;
    uint64_t timestamp    = 0;
    uint32_t file_size    = 0;
    bool is_compressed    = false;
    bool is_encrypted     = false;

    static SnapshotInfo from_c(const nimcp_brain_snapshot_info_t& c) {
        SnapshotInfo info;
        info.name          = c.name;
        info.description   = c.description;
        info.timestamp     = c.timestamp;
        info.file_size     = c.file_size;
        info.is_compressed = c.is_compressed;
        info.is_encrypted  = c.is_encrypted;
        return info;
    }
};

struct BrainProbe {
    std::string task_name;
    BrainSize size       = BrainSize::Tiny;
    TaskType task        = TaskType::Classification;
    uint32_t num_neurons  = 0;
    uint32_t num_synapses = 0;
    uint32_t num_active_synapses = 0;

    uint64_t total_inferences     = 0;
    uint64_t total_learning_steps = 0;

    float avg_sparsity          = 0.0f;
    float avg_inference_time_us = 0.0f;
    float current_learning_rate = 0.0f;

    float accuracy      = 0.0f;
    size_t memory_bytes = 0;

    uint32_t num_inputs  = 0;
    uint32_t num_outputs = 0;

    bool is_cow_clone       = false;
    uint32_t cow_ref_count  = 0;
    size_t cow_shared_bytes  = 0;
    size_t cow_private_bytes = 0;

    static BrainProbe from_c(const nimcp_brain_probe_t& c) {
        BrainProbe p;
        p.task_name            = c.task_name;
        p.size                 = static_cast<BrainSize>(c.size);
        p.task                 = static_cast<TaskType>(c.task);
        p.num_neurons          = c.num_neurons;
        p.num_synapses         = c.num_synapses;
        p.num_active_synapses  = c.num_active_synapses;
        p.total_inferences     = c.total_inferences;
        p.total_learning_steps = c.total_learning_steps;
        p.avg_sparsity         = c.avg_sparsity;
        p.avg_inference_time_us = c.avg_inference_time_us;
        p.current_learning_rate = c.current_learning_rate;
        p.accuracy             = c.accuracy;
        p.memory_bytes         = c.memory_bytes;
        p.num_inputs           = c.num_inputs;
        p.num_outputs          = c.num_outputs;
        p.is_cow_clone         = c.is_cow_clone;
        p.cow_ref_count        = c.cow_ref_count;
        p.cow_shared_bytes     = c.cow_shared_bytes;
        p.cow_private_bytes    = c.cow_private_bytes;
        return p;
    }
};

struct Phasor {
    float amplitude = 0.0f;
    float phase     = 0.0f;
};

struct Prediction {
    std::string label;
    float confidence = 0.0f;
};

struct TrainingStats {
    uint64_t total_steps = 0;
    float total_loss     = 0.0f;
    float current_lr     = 0.0f;
};

struct CallbackStats {
    uint64_t total_fired  = 0;
    float avg_time_us     = 0.0f;
    uint32_t early_stops  = 0;
};

struct UtilizationMetrics {
    float utilization = 0.0f;
    float saturation  = 0.0f;
};

struct WorkingMemoryStats {
    uint32_t size     = 0;
    uint32_t capacity = 0;
};

struct WorkspaceReadResult {
    std::vector<float> content;
    CognitiveModule source = CognitiveModule::None;
};

struct WorkspaceStats {
    uint32_t total_broadcasts  = 0;
    uint32_t total_competitions = 0;
    float avg_strength          = 0.0f;
};

// ============================================================================
// RAII Handle Wrapper Base
// ============================================================================

namespace detail {

template <typename Handle, auto Deleter>
class HandleWrapper {
public:
    explicit HandleWrapper(Handle h) noexcept : handle_(h) {}
    ~HandleWrapper() { if (handle_) Deleter(handle_); }

    HandleWrapper(HandleWrapper&& o) noexcept : handle_(o.handle_) {
        o.handle_ = nullptr;
    }

    HandleWrapper& operator=(HandleWrapper&& o) noexcept {
        if (this != &o) {
            if (handle_) Deleter(handle_);
            handle_ = o.handle_;
            o.handle_ = nullptr;
        }
        return *this;
    }

    HandleWrapper(const HandleWrapper&) = delete;
    HandleWrapper& operator=(const HandleWrapper&) = delete;

    Handle get() const noexcept { return handle_; }
    explicit operator bool() const noexcept { return handle_ != nullptr; }

protected:
    Handle handle_;
};

} // namespace detail

// ============================================================================
// Callback Trampoline
// ============================================================================

namespace detail {

using CppCallbackFn = std::function<CallbackAction(CallbackEvent, const CallbackMetrics&)>;

inline nimcp_callback_action_t callback_trampoline(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t* metrics,
    void* user_data)
{
    auto* fn = static_cast<CppCallbackFn*>(user_data);
    auto cpp_metrics = CallbackMetrics::from_c(*metrics);
    auto action = (*fn)(static_cast<CallbackEvent>(event), cpp_metrics);
    return static_cast<nimcp_callback_action_t>(action);
}

} // namespace detail

// ============================================================================
// Brain Class
// ============================================================================

class Brain : public detail::HandleWrapper<nimcp_brain_t, nimcp_brain_destroy> {
    using Base = detail::HandleWrapper<nimcp_brain_t, nimcp_brain_destroy>;

public:
    // --- Construction ---

    Brain(std::string_view name, BrainSize size, TaskType task,
          uint32_t num_inputs, uint32_t num_outputs)
        : Base(nimcp_brain_create(
              std::string(name).c_str(),
              static_cast<nimcp_brain_size_t>(size),
              static_cast<nimcp_brain_task_t>(task),
              num_inputs, num_outputs))
    {
        if (!handle_) throw MemoryError("Failed to create brain");
    }

    static Brain from_config(std::string_view config_path) {
        auto h = nimcp_brain_create_from_config(std::string(config_path).c_str());
        if (!h) throw IOError("Failed to create brain from config");
        return Brain(h);
    }

    static Brain load(std::string_view filepath) {
        auto h = nimcp_brain_load(std::string(filepath).c_str());
        if (!h) throw IOError("Failed to load brain");
        return Brain(h);
    }

    void save(std::string_view filepath) const {
        check_status(nimcp_brain_save(handle_, std::string(filepath).c_str()));
    }

    static Brain create_with_neurons(std::string_view name, TaskType task,
                                     uint32_t num_inputs, uint32_t num_outputs,
                                     uint32_t neuron_count) {
        auto h = nimcp_brain_create_with_neurons(
            std::string(name).c_str(),
            static_cast<nimcp_brain_task_t>(task),
            num_inputs, num_outputs, neuron_count);
        if (!h) throw MemoryError("Failed to create brain with neurons");
        return Brain(h);
    }

    static Brain create_full(std::string_view name, TaskType task,
                              uint32_t num_inputs, uint32_t num_outputs,
                              uint32_t neuron_count) {
        auto h = nimcp_brain_create_full(
            std::string(name).c_str(),
            static_cast<nimcp_brain_task_t>(task),
            num_inputs, num_outputs, neuron_count);
        if (!h) throw MemoryError("Failed to create full brain");
        return Brain(h);
    }

    // --- Learning / Inference ---

    void learn(std::span<const float> features, std::string_view label,
               float confidence = 1.0f) {
        check_status(nimcp_brain_learn_example(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            std::string(label).c_str(), confidence));
    }

    Prediction predict(std::span<const float> features) const {
        char label_buf[NIMCP_MAX_LABEL_SIZE]{};
        float confidence = 0.0f;
        check_status(nimcp_brain_predict(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            label_buf, &confidence));
        return {label_buf, confidence};
    }

    std::vector<float> infer(std::span<const float> features,
                             uint32_t num_outputs) const {
        std::vector<float> outputs(num_outputs);
        check_status(nimcp_brain_infer(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            outputs.data(), num_outputs));
        return outputs;
    }

    // --- Extended Learning ---

    void learn_vector(std::span<const float> features,
                      std::span<const float> target,
                      std::string_view label = "",
                      float confidence = 1.0f) {
        check_status(nimcp_brain_learn_vector(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            target.data(), static_cast<uint32_t>(target.size()),
            label.empty() ? nullptr : std::string(label).c_str(),
            confidence));
    }

    float learn_vector_batch(const float** features_array,
                             const float** targets_array,
                             uint32_t num_features, uint32_t target_size,
                             uint32_t num_examples, float learning_rate) {
        return nimcp_brain_learn_vector_batch(
            handle_, features_array, targets_array,
            num_features, target_size, num_examples, learning_rate);
    }

    void learn_batch(const float** features_array,
                     const uint32_t* num_features_array,
                     const char** labels,
                     const float* confidences,
                     uint32_t num_examples,
                     float* losses_out = nullptr) {
        check_status(nimcp_brain_learn_batch(
            handle_, features_array, num_features_array,
            labels, confidences, num_examples, losses_out));
    }

    void learn_knowledge(std::string_view text, int domain) {
        check_status(nimcp_brain_learn_knowledge(
            handle_, std::string(text).c_str(), domain));
    }

    void learn_language(std::string_view text, float* out_loss = nullptr) {
        check_status(nimcp_brain_learn_language(
            handle_, std::string(text).c_str(), out_loss));
    }

    void learn_language_pair(std::string_view input_text,
                             std::string_view target_text,
                             float learning_rate = 0.0f,
                             float* out_loss = nullptr) {
        check_status(nimcp_brain_learn_language_pair(
            handle_,
            std::string(input_text).c_str(),
            std::string(target_text).c_str(),
            learning_rate, out_loss));
    }

    // --- Extended Prediction ---

    Prediction predict_fast(std::span<const float> features) const {
        char label_buf[NIMCP_MAX_LABEL_SIZE]{};
        float confidence = 0.0f;
        check_status(nimcp_brain_predict_fast(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            label_buf, &confidence));
        return {label_buf, confidence};
    }

    Prediction predict_in_domain(std::span<const float> features,
                                 std::string_view domain_prefix) const {
        char label_buf[NIMCP_MAX_LABEL_SIZE]{};
        float confidence = 0.0f;
        check_status(nimcp_brain_predict_in_domain(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            domain_prefix.empty() ? nullptr : std::string(domain_prefix).c_str(),
            label_buf, &confidence));
        return {label_buf, confidence};
    }

    // --- Full Decision ---

    struct DecisionResult {
        std::string label;
        float confidence = 0.0f;
        std::string explanation;
        std::vector<float> output_vector;
        uint32_t num_active_neurons = 0;
        float sparsity = 0.0f;
        uint64_t inference_time_us = 0;
    };

    DecisionResult decide_full(std::span<const float> features,
                               uint32_t max_outputs = 256) const {
        DecisionResult r;
        char label_buf[NIMCP_MAX_LABEL_SIZE]{};
        char explanation_buf[1024]{};
        r.output_vector.resize(max_outputs);
        uint32_t output_size = max_outputs;
        check_status(nimcp_brain_decide_full(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            label_buf, &r.confidence,
            explanation_buf,
            r.output_vector.data(), &output_size,
            &r.num_active_neurons, &r.sparsity,
            &r.inference_time_us));
        r.label = label_buf;
        r.explanation = explanation_buf;
        r.output_vector.resize(output_size);
        return r;
    }

    // --- Language / Speech ---

    struct SpeakResult {
        std::string text;
        float confidence = 0.0f;
        float fluency = 0.0f;
    };

    SpeakResult speak(std::span<const float> semantic_input = {},
                      uint32_t text_max_len = 1024) const {
        SpeakResult r;
        std::vector<char> buf(text_max_len, '\0');
        check_status(nimcp_brain_speak(
            handle_,
            semantic_input.empty() ? nullptr : semantic_input.data(),
            static_cast<uint32_t>(semantic_input.size()),
            buf.data(), text_max_len,
            &r.confidence, &r.fluency));
        r.text = buf.data();
        return r;
    }

    struct GenerateTextResult {
        std::string text;
        float confidence = 0.0f;
        float perplexity = 0.0f;
    };

    GenerateTextResult generate_text(std::string_view prompt = "",
                                     std::span<const float> semantic_input = {},
                                     uint32_t text_max_len = 1024) const {
        GenerateTextResult r;
        std::vector<char> buf(text_max_len, '\0');
        check_status(nimcp_brain_generate_text(
            handle_,
            prompt.empty() ? nullptr : std::string(prompt).c_str(),
            semantic_input.empty() ? nullptr : semantic_input.data(),
            static_cast<uint32_t>(semantic_input.size()),
            buf.data(), text_max_len,
            &r.confidence, &r.perplexity));
        r.text = buf.data();
        return r;
    }

    struct ComprehendResult {
        std::vector<float> semantic;
        float confidence = 0.0f;
    };

    ComprehendResult comprehend(std::string_view text,
                                uint32_t semantic_dim = 128) const {
        ComprehendResult r;
        r.semantic.resize(semantic_dim);
        check_status(nimcp_brain_comprehend(
            handle_, std::string(text).c_str(),
            r.semantic.data(), semantic_dim, &r.confidence));
        return r;
    }

    struct ProduceTextResult {
        std::string text;
        float confidence = 0.0f;
    };

    ProduceTextResult produce_text(std::span<const float> intent,
                                   uint32_t text_max_len = 1024) const {
        ProduceTextResult r;
        std::vector<char> buf(text_max_len, '\0');
        check_status(nimcp_brain_produce_text(
            handle_, intent.data(), static_cast<uint32_t>(intent.size()),
            buf.data(), text_max_len, &r.confidence));
        r.text = buf.data();
        return r;
    }

    std::string creative_blend(std::span<const float> vector_a,
                               std::span<const float> vector_b,
                               float blend_ratio,
                               uint32_t text_max = 1024) {
        std::vector<char> buf(text_max, '\0');
        check_status(nimcp_brain_creative_blend(
            handle_, vector_a.data(), vector_b.data(),
            static_cast<uint32_t>(vector_a.size()),
            blend_ratio, buf.data(), text_max));
        return std::string(buf.data());
    }

    struct GroundedRespondResult {
        std::string response;
        float confidence = 0.0f;
    };

    GroundedRespondResult grounded_respond(std::string_view input_text,
                                           uint32_t response_max = 1024) const {
        GroundedRespondResult r;
        std::vector<char> buf(response_max, '\0');
        check_status(nimcp_brain_grounded_respond(
            handle_, std::string(input_text).c_str(),
            buf.data(), response_max, &r.confidence));
        r.response = buf.data();
        return r;
    }

    void ground_word(std::string_view word,
                     std::span<const float> features,
                     uint32_t modality, float attention) {
        check_status(nimcp_brain_ground_word(
            handle_, std::string(word).c_str(),
            features.data(), static_cast<uint32_t>(features.size()),
            modality, attention));
    }

    // --- Cognitive Training ---

    void train_cognitive(std::string_view text, int domain,
                         std::string_view target_text = "",
                         float learning_rate = 0.0f,
                         float* out_loss = nullptr) {
        check_status(nimcp_brain_train_cognitive(
            handle_, std::string(text).c_str(), domain,
            target_text.empty() ? nullptr : std::string(target_text).c_str(),
            learning_rate, out_loss));
    }

    void train_language(std::string_view input_text,
                        std::string_view target_text,
                        float learning_rate = 0.0f,
                        float* out_loss = nullptr) {
        check_status(nimcp_brain_train_language(
            handle_,
            std::string(input_text).c_str(),
            std::string(target_text).c_str(),
            learning_rate, out_loss));
    }

    // --- Training Pipeline ---

    void configure_training(const TrainingConfig& config) {
        auto c = config.to_c();
        check_status(nimcp_brain_configure_training(handle_, &c));
    }

    TrainingResult train_step(std::span<const float> features,
                              std::span<const float> targets) {
        nimcp_training_result_t result{};
        check_status(nimcp_brain_train_step(
            handle_, features.data(), static_cast<uint32_t>(features.size()),
            targets.data(), static_cast<uint32_t>(targets.size()), &result));
        return TrainingResult::from_c(result);
    }

    TrainingResult train_batch(std::span<const float> features,
                               std::span<const float> targets,
                               uint32_t batch_size,
                               uint32_t num_features,
                               uint32_t num_targets) {
        nimcp_training_result_t result{};
        check_status(nimcp_brain_train_batch(
            handle_, features.data(), targets.data(),
            batch_size, num_features, num_targets, &result));
        return TrainingResult::from_c(result);
    }

    TrainingStats get_training_stats() const {
        TrainingStats stats{};
        check_status(nimcp_brain_get_training_stats(
            handle_, &stats.total_steps, &stats.total_loss, &stats.current_lr));
        return stats;
    }

    float step_scheduler(float validation_metric = 0.0f) {
        return nimcp_brain_step_scheduler(handle_, validation_metric);
    }

    // --- Metrics / Stats Getters ---

    float get_accuracy() const {
        return nimcp_brain_get_accuracy(handle_);
    }

    float get_last_gradient_norm() const {
        return nimcp_brain_get_last_gradient_norm(handle_);
    }

    float get_last_loss() const {
        return nimcp_brain_get_last_loss(handle_);
    }

    struct NetworkMetrics {
        float ema_ann = 0.0f, ema_cnn = 0.0f, ema_snn = 0.0f, ema_lnn = 0.0f;
        uint64_t ann_steps = 0, cnn_steps = 0, snn_steps = 0, lnn_steps = 0;
    };

    std::optional<NetworkMetrics> get_network_metrics() const {
        NetworkMetrics m;
        bool ok = nimcp_brain_get_network_metrics(
            handle_, &m.ema_ann, &m.ema_cnn, &m.ema_snn, &m.ema_lnn,
            &m.ann_steps, &m.cnn_steps, &m.snn_steps, &m.lnn_steps);
        if (!ok) return std::nullopt;
        return m;
    }

    struct CognitiveStats {
        std::vector<uint32_t> step_counts;
        std::vector<float> losses;
        uint32_t module_count = 0;
    };

    CognitiveStats get_cognitive_stats() const {
        CognitiveStats s;
        s.step_counts.resize(13);
        s.losses.resize(13);
        check_status(nimcp_brain_get_cognitive_stats(
            handle_, s.step_counts.data(), s.losses.data(), &s.module_count));
        s.step_counts.resize(s.module_count);
        s.losses.resize(s.module_count);
        return s;
    }

    nimcp_avatar_state_t get_avatar_state() const {
        nimcp_avatar_state_t state{};
        check_status(nimcp_brain_get_avatar_state(handle_, &state));
        return state;
    }

    struct TranscriptEntry {
        std::string summary;
        float salience = 0.0f;
        float confidence = 0.0f;
        std::string module_name;
    };

    std::vector<TranscriptEntry> get_last_transcript(uint32_t max_entries = 32) const {
        std::vector<std::array<char, 256>> entries(max_entries);
        std::vector<float> saliences(max_entries);
        std::vector<float> confidences(max_entries);
        std::vector<const char*> modules(max_entries, nullptr);
        uint32_t count = nimcp_brain_get_last_transcript(
            handle_,
            reinterpret_cast<char(*)[256]>(entries.data()),
            saliences.data(), confidences.data(),
            modules.data(), max_entries);
        std::vector<TranscriptEntry> result;
        result.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            TranscriptEntry e;
            e.summary = entries[i].data();
            e.salience = saliences[i];
            e.confidence = confidences[i];
            e.module_name = modules[i] ? modules[i] : "";
            result.push_back(std::move(e));
        }
        return result;
    }

    struct CortexCnnMetrics {
        std::vector<int> types;
        std::vector<float> losses;
        std::vector<uint64_t> fwd_steps;
        std::vector<uint64_t> bwd_steps;
        std::vector<float> embed_norms;
        uint32_t count = 0;
    };

    CortexCnnMetrics get_cortex_cnn_metrics() const {
        CortexCnnMetrics m;
        m.types.resize(4);
        m.losses.resize(4);
        m.fwd_steps.resize(4);
        m.bwd_steps.resize(4);
        m.embed_norms.resize(4);
        check_status(nimcp_brain_get_cortex_cnn_metrics(
            handle_, m.types.data(), m.losses.data(),
            m.fwd_steps.data(), m.bwd_steps.data(),
            m.embed_norms.data(), &m.count));
        m.types.resize(m.count);
        m.losses.resize(m.count);
        m.fwd_steps.resize(m.count);
        m.bwd_steps.resize(m.count);
        m.embed_norms.resize(m.count);
        return m;
    }

    // --- Experience API ---

    void experience(std::span<const float> input,
                    std::span<float> output,
                    float teacher_reward,
                    brain_experience_result_t* result = nullptr) {
        check_status(nimcp_brain_experience(
            handle_, input.data(), static_cast<uint32_t>(input.size()),
            output.data(), static_cast<uint32_t>(output.size()),
            teacher_reward, result));
    }

    void experience_configure(const brain_experience_config_t* config) {
        check_status(nimcp_brain_experience_configure(handle_, config));
    }

    float experience_correct(std::span<const float> expected) {
        return nimcp_brain_experience_correct(
            handle_, expected.data(), static_cast<uint32_t>(expected.size()));
    }

    void experience_attend(std::string_view modality, float strength) {
        check_status(nimcp_brain_experience_attend(
            handle_, std::string(modality).c_str(), strength));
    }

    // --- Rubric ---

    nimcp_rubric_t rubric() const {
        nimcp_rubric_t r{};
        check_status(nimcp_brain_rubric(handle_, &r));
        return r;
    }

    // --- Callbacks ---

    void enable_callbacks(const CallbackConfig& config) {
        auto c = config.to_c();
        check_status(nimcp_brain_enable_callbacks(handle_, &c));
    }

    void enable_callbacks() {
        check_status(nimcp_brain_enable_callbacks(handle_, nullptr));
    }

    void disable_callbacks() {
        check_status(nimcp_brain_disable_callbacks(handle_));
    }

    uint32_t register_callback(
        CallbackEvent event,
        detail::CppCallbackFn fn,
        std::string_view name = "")
    {
        auto* fn_ptr = new detail::CppCallbackFn(std::move(fn));
        owned_callbacks_.push_back(fn_ptr);
        uint32_t id = nimcp_brain_register_callback(
            handle_,
            static_cast<nimcp_callback_event_t>(event),
            detail::callback_trampoline,
            fn_ptr,
            name.empty() ? nullptr : std::string(name).c_str());
        if (id == 0) {
            owned_callbacks_.pop_back();
            delete fn_ptr;
            throw Exception(NIMCP_ERROR, "Failed to register callback");
        }
        return id;
    }

    void unregister_callback(uint32_t callback_id) {
        check_status(nimcp_brain_unregister_callback(handle_, callback_id));
    }

    CallbackStats get_callback_stats() const {
        CallbackStats stats{};
        check_status(nimcp_brain_get_callback_stats(
            handle_, &stats.total_fired, &stats.avg_time_us, &stats.early_stops));
        return stats;
    }

    // --- Resizing ---

    bool resize(uint32_t new_neuron_count) {
        return nimcp_brain_resize(handle_, new_neuron_count);
    }

    bool auto_resize() {
        return nimcp_brain_auto_resize(handle_);
    }

    uint32_t neuron_count() const {
        return nimcp_brain_get_neuron_count(handle_);
    }

    UtilizationMetrics utilization_metrics() const {
        UtilizationMetrics m{};
        nimcp_brain_get_utilization_metrics(handle_, &m.utilization, &m.saturation);
        return m;
    }

    // --- Named Snapshots ---

    void snapshot_save(std::string_view name,
                       std::string_view description = "") {
        check_status(nimcp_brain_snapshot_save(
            handle_, std::string(name).c_str(),
            description.empty() ? nullptr : std::string(description).c_str()));
    }

    Brain snapshot_restore(std::string_view name) {
        auto h = nimcp_brain_snapshot_restore(handle_, std::string(name).c_str());
        if (!h) throw IOError("Failed to restore snapshot");
        return Brain(h);
    }

    std::vector<SnapshotInfo> snapshot_list(uint32_t max_count = 64) const {
        std::vector<nimcp_brain_snapshot_info_t> c_infos(max_count);
        uint32_t count = 0;
        check_status(nimcp_brain_snapshot_list(
            handle_, c_infos.data(), max_count, &count));
        std::vector<SnapshotInfo> result;
        result.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            result.push_back(SnapshotInfo::from_c(c_infos[i]));
        }
        return result;
    }

    void snapshot_delete(std::string_view name) {
        check_status(nimcp_brain_snapshot_delete(
            handle_, std::string(name).c_str()));
    }

    // --- Probe ---

    BrainProbe probe() const {
        nimcp_brain_probe_t c{};
        check_status(nimcp_brain_probe(handle_, &c));
        return BrainProbe::from_c(c);
    }

    void broadcast_probe() {
        check_status(nimcp_brain_broadcast_probe(handle_));
    }

    // --- COW (Copy-on-Write) ---

    Brain clone() const {
        auto h = nimcp_brain_clone_cow(handle_);
        if (!h) throw MemoryError("Failed to clone brain");
        return Brain(h);
    }

    // --- Working Memory ---

    void working_memory_add(std::span<const float> data, float salience) {
        check_status(nimcp_brain_working_memory_add(
            handle_, data.data(), static_cast<uint32_t>(data.size()), salience));
    }

    std::optional<std::vector<float>> working_memory_get(uint32_t index) const {
        uint32_t size = 0;
        const float* data = nimcp_brain_working_memory_get(handle_, index, &size);
        if (!data) return std::nullopt;
        return std::vector<float>(data, data + size);
    }

    WorkingMemoryStats working_memory_stats() const {
        WorkingMemoryStats s{};
        check_status(nimcp_brain_working_memory_stats(handle_, &s.size, &s.capacity));
        return s;
    }

    void working_memory_refresh(uint32_t index) {
        check_status(nimcp_brain_working_memory_refresh(handle_, index));
    }

    // --- Global Workspace ---

    void workspace_compete(CognitiveModule module,
                           std::span<const float> content,
                           float strength) {
        check_status(nimcp_brain_workspace_compete(
            handle_,
            static_cast<nimcp_cognitive_module_t>(module),
            content.data(), static_cast<uint32_t>(content.size()),
            strength));
    }

    std::optional<WorkspaceReadResult> workspace_read(uint32_t max_dim = 256) const {
        std::vector<float> content(max_dim);
        uint32_t actual_dim = 0;
        nimcp_cognitive_module_t source = NIMCP_MODULE_NONE;
        auto s = nimcp_brain_workspace_read(
            handle_, content.data(), max_dim, &actual_dim, &source);
        if (s != NIMCP_OK) return std::nullopt;
        content.resize(actual_dim);
        return WorkspaceReadResult{std::move(content),
                                   static_cast<CognitiveModule>(source)};
    }

    void workspace_subscribe(CognitiveModule module) {
        check_status(nimcp_brain_workspace_subscribe(
            handle_, static_cast<nimcp_cognitive_module_t>(module)));
    }

    void workspace_unsubscribe(CognitiveModule module) {
        check_status(nimcp_brain_workspace_unsubscribe(
            handle_, static_cast<nimcp_cognitive_module_t>(module)));
    }

    bool workspace_has_broadcast() const {
        bool has = false;
        check_status(nimcp_brain_workspace_has_broadcast(handle_, &has));
        return has;
    }

    WorkspaceStats workspace_stats() const {
        WorkspaceStats s{};
        check_status(nimcp_brain_workspace_stats(
            handle_, &s.total_broadcasts, &s.total_competitions, &s.avg_strength));
        return s;
    }

    // --- Oscillations ---

    bool enable_oscillations(bool enable) {
        return nimcp_enable_complex_oscillations(handle_, enable);
    }

    bool oscillations_enabled() const {
        return nimcp_is_complex_oscillations_enabled(handle_);
    }

    Phasor get_phasor(uint32_t neuron_id) const {
        auto c = nimcp_get_oscillation_phasor(handle_, neuron_id);
        return {c.amplitude, c.phase};
    }

    float phase_coherence(std::span<const uint32_t> neuron_ids) const {
        return nimcp_get_phase_coherence(
            handle_, neuron_ids.data(),
            static_cast<uint32_t>(neuron_ids.size()));
    }

    float pac_modulation(float theta_freq, float gamma_freq) const {
        return nimcp_get_pac_modulation(handle_, theta_freq, gamma_freq);
    }

    // --- Freeze / Configuration ---

    void freeze() {
        check_status(nimcp_brain_freeze(handle_));
    }

    bool is_frozen() const {
        return nimcp_brain_is_frozen(handle_);
    }

    void enable_mixed_precision(bool enable) {
        check_status(nimcp_brain_enable_mixed_precision(handle_, enable));
    }

    void enable_gradient_checkpointing(bool enable,
                                       uint32_t checkpoint_interval = 0) {
        check_status(nimcp_brain_enable_gradient_checkpointing(
            handle_, enable, checkpoint_interval));
    }

    void enable_hemispheric(bool enable) {
        check_status(nimcp_brain_enable_hemispheric(handle_, enable));
    }

    void enable_recurrent(bool enable, uint32_t max_iterations = 3,
                          float confidence_threshold = 0.9f,
                          float blend_alpha = 0.5f) {
        check_status(nimcp_brain_enable_recurrent(
            handle_, enable, max_iterations,
            confidence_threshold, blend_alpha));
    }

    void enable_bptt(bool enable, uint32_t window_size = 8,
                     float discount = 0.95f) {
        check_status(nimcp_brain_enable_bptt(
            handle_, enable, window_size, discount));
    }

    void enable_multi_network() {
        check_status(nimcp_brain_enable_multi_network(handle_));
    }

    void enable_biological_plasticity(bool enabled) {
        check_status(nimcp_brain_enable_biological_plasticity(handle_, enabled));
    }

    void set_fast_training(bool enabled) {
        check_status(nimcp_brain_set_fast_training(handle_, enabled));
    }

    void set_task_type(std::string_view task_type) {
        check_status(nimcp_brain_set_task_type(
            handle_, std::string(task_type).c_str()));
    }

    void set_training_mode(bool active) {
        nimcp_brain_set_training_mode(handle_, active);
    }

    void set_network_ablation(int train_cnn, int train_snn, int train_lnn) {
        nimcp_brain_set_network_ablation(handle_, train_cnn, train_snn, train_lnn);
    }

    // --- Per-network training toggles (runtime-dynamic, no rebuild required) ---

    void set_train_ann(bool enabled) { nimcp_brain_set_train_ann(handle_, enabled); }
    bool get_train_ann() const { return nimcp_brain_get_train_ann(handle_); }

    void set_train_cnn(bool enabled) { nimcp_brain_set_train_cnn(handle_, enabled); }
    bool get_train_cnn() const { return nimcp_brain_get_train_cnn(handle_); }

    void set_train_snn(bool enabled) { nimcp_brain_set_train_snn(handle_, enabled); }
    bool get_train_snn() const { return nimcp_brain_get_train_snn(handle_); }

    void set_train_lnn(bool enabled) { nimcp_brain_set_train_lnn(handle_, enabled); }
    bool get_train_lnn() const { return nimcp_brain_get_train_lnn(handle_); }

    /** Convenience preset: freeze ANN/CNN/LNN while keeping SNN training. */
    void set_snn_only_recovery(bool enabled) {
        nimcp_brain_set_snn_only_recovery(handle_, enabled);
    }
    bool get_snn_only_recovery() const {
        return nimcp_brain_get_snn_only_recovery(handle_);
    }

    /** Ensemble warmup scale [0.0, 1.0] — probabilistic gate on non-SNN
     *  training. 1.0 = full-rate (default), 0.0 = fully frozen,
     *  intermediate = Monte-Carlo skip. Clamped to [0.0, 1.0] on the C side. */
    void set_ensemble_warmup_scale(float scale) {
        nimcp_brain_set_ensemble_warmup_scale(handle_, scale);
    }
    float get_ensemble_warmup_scale() const {
        return nimcp_brain_get_ensemble_warmup_scale(handle_);
    }

    // --- Sensory / Brain Regions ---

    void submit_sensory(std::string_view modality,
                        std::span<const float> data,
                        uint32_t width, uint32_t height,
                        uint32_t channels, uint32_t n_segments) {
        check_status(nimcp_brain_submit_sensory(
            handle_, std::string(modality).c_str(),
            data.data(), static_cast<uint32_t>(data.size()),
            width, height, channels, n_segments));
    }

    struct VisualCortexResult {
        std::vector<float> features;
        uint32_t feature_count = 0;
    };

    VisualCortexResult visual_cortex_process(std::span<const float> pixels,
                                             uint32_t width, uint32_t height,
                                             uint32_t channels,
                                             uint32_t max_features = 256) const {
        VisualCortexResult r;
        r.features.resize(max_features);
        check_status(nimcp_brain_visual_cortex_process(
            handle_, pixels.data(), static_cast<uint32_t>(pixels.size()),
            width, height, channels,
            r.features.data(), max_features, &r.feature_count));
        r.features.resize(r.feature_count);
        return r;
    }

    float medulla_get_arousal() const {
        return nimcp_brain_medulla_get_arousal(handle_);
    }

    float bg_get_dopamine() const {
        return nimcp_brain_bg_get_dopamine(handle_);
    }

    float sleep_get_pressure() const {
        return nimcp_brain_sleep_get_pressure(handle_);
    }

    std::string substrate_get_health() const {
        char buf[64]{};
        check_status(nimcp_brain_substrate_get_health(handle_, buf, sizeof(buf)));
        return std::string(buf);
    }

    // --- Sub-network Management ---

    void lnn_create(uint32_t n_sensory, uint32_t n_inter,
                    uint32_t n_command, uint32_t n_output) {
        check_status(nimcp_brain_lnn_create(
            handle_, n_sensory, n_inter, n_command, n_output));
    }

    struct LnnStats {
        uint64_t forward_steps = 0, backward_steps = 0, ode_evals = 0;
        float avg_tau = 0.0f, state_norm = 0.0f, gradient_norm = 0.0f;
        uint32_t nan_count = 0, inf_count = 0;
    };

    LnnStats lnn_get_stats() const {
        LnnStats s;
        check_status(nimcp_brain_lnn_get_stats(
            handle_, &s.forward_steps, &s.backward_steps,
            &s.ode_evals, &s.avg_tau, &s.state_norm,
            &s.gradient_norm, &s.nan_count, &s.inf_count));
        return s;
    }

    struct SnnStats {
        uint64_t total_steps = 0, total_spikes = 0;
        float mean_firing_rate = 0.0f, sparsity = 0.0f, synchrony = 0.0f;
        uint32_t silent_neurons = 0, hyperactive_neurons = 0;
        int health = 0;
        size_t memory_bytes = 0;
    };

    SnnStats snn_get_stats() const {
        SnnStats s;
        check_status(nimcp_brain_snn_get_stats(
            handle_, &s.total_steps, &s.total_spikes,
            &s.mean_firing_rate, &s.sparsity, &s.synchrony,
            &s.silent_neurons, &s.hyperactive_neurons,
            &s.health, &s.memory_bytes));
        return s;
    }

    static void snn_set_input_scale(float scale) {
        nimcp_snn_set_input_scale(scale);
    }
    static float snn_get_input_scale() {
        return nimcp_snn_get_input_scale();
    }

    struct CnnStats {
        uint32_t num_layers = 0;
        size_t num_parameters = 0;
        uint32_t num_labels = 0;
        bool active = false;
    };

    CnnStats cnn_get_stats() const {
        CnnStats s;
        check_status(nimcp_brain_cnn_get_stats(
            handle_, &s.num_layers, &s.num_parameters,
            &s.num_labels, &s.active));
        return s;
    }

    // --- Edge Brain ---

    struct EdgeResizeResult {
        int status = 0;
        uint32_t target_neurons = 0;
        std::string mode;
    };

    EdgeResizeResult edge_resize(uint32_t target_neurons,
                                  std::string_view mode = "contract",
                                  bool knowledge_transfer = true) {
        nimcp_resize_config_t config = nimcp_resize_config_default();
        config.target_neuron_count = target_neurons;
        config.enable_knowledge_transfer = knowledge_transfer;
        if (mode == "expand") config.mode = 1;
        else if (mode == "rebalance") config.mode = 2;
        else config.mode = 0;
        int ret = nimcp_edge_brain_resize(handle_, &config);
        return {ret, target_neurons, std::string(mode)};
    }

    struct ResizeCheckReport {
        bool feasible = false;
        uint32_t neurons_before = 0;
        uint32_t neurons_after = 0;
        float ram_delta_mb = 0.0f;
        std::string reason;
    };

    ResizeCheckReport edge_resize_check(uint32_t target_neurons) {
        nimcp_resize_config_t config = nimcp_resize_config_default();
        config.target_neuron_count = target_neurons;
        config.mode = 0;
        nimcp_resize_report_t report{};
        nimcp_edge_brain_resize_check(handle_, &config, &report);
        return {report.feasible, report.neurons_before,
                report.neurons_after, report.estimated_ram_delta_mb,
                report.reason};
    }

    struct DistillReport {
        int status = 0;
        float accuracy_retention = 0.0f;
        uint32_t neurons_selected = 0;
        float compression_ratio = 0.0f;
        float teacher_loss = 0.0f;
        float student_loss = 0.0f;
        uint32_t steps_trained = 0;
    };

    DistillReport edge_distill(uint32_t target_neurons,
                                float temperature = 2.0f,
                                uint32_t steps = 5000,
                                bool include_snn = false,
                                bool include_lnn = false,
                                bool include_cnn = true) {
        nimcp_distill_config_t config = nimcp_distill_config_default();
        config.target_neurons = target_neurons;
        config.temperature = temperature;
        config.distillation_steps = steps;
        config.include_snn = include_snn;
        config.include_lnn = include_lnn;
        config.include_cnn = include_cnn;
        nimcp_distill_report_t report{};
        nimcp_brain_t student = nullptr;
        int ret = nimcp_brain_distill(handle_, &student, &config, &report);
        return {ret, report.accuracy_retention, report.neurons_selected,
                report.compression_ratio, report.teacher_loss,
                report.student_loss, report.steps_trained};
    }

    struct DeviceOptReport {
        int status = 0;
        uint32_t neuron_count = 0;
        uint32_t subsystems_enabled = 0;
        float estimated_ram_mb = 0.0f;
        float estimated_inference_ms = 0.0f;
        float accuracy_retention = 0.0f;
    };

    DeviceOptReport edge_optimize_for_device(uint32_t ram_mb,
                                              uint32_t cpu_cores = 2,
                                              bool has_camera = false,
                                              bool has_imu = false,
                                              bool has_motor_control = false,
                                              bool has_network = true,
                                              std::string_view role = "general") {
        nimcp_device_profile_t profile = nimcp_device_profile_default();
        profile.ram_mb = ram_mb;
        profile.cpu_cores = cpu_cores;
        profile.has_camera = has_camera;
        profile.has_imu = has_imu;
        profile.has_motor_control = has_motor_control;
        profile.has_network = has_network;
        if (role == "sensor") profile.role = 1;
        else if (role == "actuator") profile.role = 2;
        else if (role == "coordinator") profile.role = 3;
        else profile.role = 0;
        nimcp_optimization_report_t report{};
        nimcp_brain_t child = nullptr;
        int ret = nimcp_brain_optimize_for_device(handle_, &profile, &child, &report);
        return {ret, report.neuron_count, report.subsystems_enabled,
                report.estimated_ram_mb, report.estimated_inference_ms,
                report.accuracy_retention};
    }

    struct QuantizeResult {
        int status = 0;
        std::string precision;
    };

    QuantizeResult edge_quantize(std::string_view precision = "int8_symmetric",
                                  uint32_t calibration_samples = 100) {
        nimcp_quantize_config_t config = nimcp_quantize_config_default();
        config.calibration_samples = calibration_samples;
        if (precision == "fp16") config.weight_precision = 1;
        else if (precision == "int8_affine") config.weight_precision = 2;
        else if (precision == "int4") config.weight_precision = 3;
        else if (precision == "ternary") config.weight_precision = 4;
        else config.weight_precision = 0;
        int ret = nimcp_brain_quantize(handle_, &config);
        return {ret, std::string(precision)};
    }

    std::vector<float> edge_score_importance(uint32_t num_neurons = 1000) {
        std::vector<float> scores(num_neurons, 0.0f);
        nimcp_edge_score_neuron_importance(handle_, scores.data(), num_neurons);
        return scores;
    }

    // --- Swarm Master ---

    bool swarm_master_create(uint32_t device_id = 1, uint16_t listen_port = 9200) {
        nimcp_master_config_t cfg = nimcp_swarm_master_config_default();
        cfg.device_id = device_id; cfg.listen_port = listen_port;
        swarm_master_ = nimcp_swarm_master_create(handle_, &cfg);
        return swarm_master_ != nullptr;
    }
    void swarm_master_destroy() { if (swarm_master_) { nimcp_swarm_master_destroy(swarm_master_); swarm_master_ = nullptr; } }
    int swarm_master_start() { return swarm_master_ ? nimcp_swarm_master_start(swarm_master_) : -1; }
    int swarm_master_stop() { return swarm_master_ ? nimcp_swarm_master_stop(swarm_master_) : -1; }
    int swarm_master_kick(uint32_t device_id) { return swarm_master_ ? nimcp_swarm_master_kick(swarm_master_, device_id) : -1; }
    int swarm_master_force_sync() { return swarm_master_ ? nimcp_swarm_master_force_sync(swarm_master_) : -1; }
    uint32_t swarm_master_get_peer_count() const { return swarm_master_ ? nimcp_swarm_master_get_peer_count(swarm_master_) : 0; }

    // --- Swarm Edge ---

    bool swarm_edge_create(uint32_t device_id = 2) {
        nimcp_edge_runtime_config_t cfg = nimcp_swarm_edge_config_default();
        cfg.device_id = device_id;
        swarm_edge_ = nimcp_swarm_edge_create(handle_, &cfg);
        return swarm_edge_ != nullptr;
    }
    void swarm_edge_destroy() { if (swarm_edge_) { nimcp_swarm_edge_destroy(swarm_edge_); swarm_edge_ = nullptr; } }
    int swarm_edge_start() { return swarm_edge_ ? nimcp_swarm_edge_start(swarm_edge_) : -1; }
    int swarm_edge_stop() { return swarm_edge_ ? nimcp_swarm_edge_stop(swarm_edge_) : -1; }
    bool swarm_edge_is_connected() const { return swarm_edge_ && nimcp_swarm_edge_is_connected(swarm_edge_); }
    int swarm_edge_submit_gradients(std::span<const float> grads) { return swarm_edge_ ? nimcp_swarm_edge_submit_gradients(swarm_edge_, grads.data(), static_cast<uint32_t>(grads.size())) : -1; }

    // --- Sensor Hub ---

    bool sensor_hub_create(uint32_t max_sensors = 32) { sensor_hub_ = nimcp_sensor_hub_create(max_sensors); return sensor_hub_ != nullptr; }
    void sensor_hub_destroy() { if (sensor_hub_) { nimcp_sensor_hub_destroy(sensor_hub_); sensor_hub_ = nullptr; } }
    uint32_t sensor_get_count() const { return sensor_hub_ ? nimcp_sensor_get_count(sensor_hub_) : 0; }
    std::vector<float> sensor_compose_features(uint32_t max_features = 1024) {
        if (!sensor_hub_) return {};
        std::vector<float> features(max_features, 0.0f);
        int count = nimcp_sensor_compose_feature_vector(sensor_hub_, features.data(), max_features);
        if (count < 0) return {};
        features.resize(static_cast<size_t>(count));
        return features;
    }

    // --- Safety Watchdog ---

    bool watchdog_create(uint32_t timeout_ms = 500, float max_magnitude = 1.0f) {
        nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
        cfg.timeout_ms = timeout_ms; cfg.validation.max_output_magnitude = max_magnitude;
        watchdog_ = nimcp_watchdog_create(&cfg);
        return watchdog_ != nullptr;
    }
    void watchdog_destroy() { if (watchdog_) { nimcp_watchdog_destroy(watchdog_); watchdog_ = nullptr; } }
    int watchdog_arm() { return watchdog_ ? nimcp_watchdog_arm(watchdog_) : -1; }
    int watchdog_disarm() { return watchdog_ ? nimcp_watchdog_disarm(watchdog_) : -1; }
    void watchdog_heartbeat() { if (watchdog_) nimcp_watchdog_heartbeat(watchdog_); }
    bool watchdog_validate_output(std::span<float> output) { return watchdog_ && nimcp_watchdog_validate_output(watchdog_, output.data(), static_cast<uint32_t>(output.size())) == 0; }
    std::vector<float> watchdog_get_safe_output(uint32_t num_outputs = 32) {
        if (!watchdog_) return {};
        std::vector<float> out(num_outputs, 0.0f);
        nimcp_watchdog_get_safe_output(watchdog_, out.data(), num_outputs);
        return out;
    }
    void watchdog_estop() { if (watchdog_) nimcp_watchdog_estop(watchdog_); }
    int watchdog_reset() { return watchdog_ ? nimcp_watchdog_reset(watchdog_) : -1; }
    std::string watchdog_get_state() const { if (!watchdog_) return "NONE"; return nimcp_watchdog_state_name(nimcp_watchdog_get_state(watchdog_)); }

    // --- ROS 2 Bridge ---

    bool ros2_bridge_create(std::string_view node_name = "nimcp_brain") {
        nimcp_ros2_config_t cfg = nimcp_ros2_config_default();
        cfg.node_name = std::string(node_name).c_str();
        ros2_bridge_ = nimcp_ros2_bridge_create(handle_, &cfg);
        return ros2_bridge_ != nullptr;
    }
    void ros2_bridge_destroy() { if (ros2_bridge_) { nimcp_ros2_bridge_destroy(ros2_bridge_); ros2_bridge_ = nullptr; } }
    int ros2_bridge_start() { return ros2_bridge_ ? nimcp_ros2_bridge_start(ros2_bridge_) : -1; }
    int ros2_bridge_stop() { return ros2_bridge_ ? nimcp_ros2_bridge_stop(ros2_bridge_) : -1; }
    int ros2_bridge_inject_sensor(std::string_view topic, std::span<const float> data) { return ros2_bridge_ ? nimcp_ros2_bridge_inject_sensor(ros2_bridge_, std::string(topic).c_str(), data.data(), static_cast<uint32_t>(data.size())) : -1; }
    std::vector<float> ros2_bridge_get_last_cmd(uint32_t max_count = 32) {
        if (!ros2_bridge_) return {};
        std::vector<float> data(max_count, 0.0f);
        int got = nimcp_ros2_bridge_get_last_cmd(ros2_bridge_, data.data(), max_count);
        if (got < 0) return {};
        data.resize(static_cast<size_t>(got));
        return data;
    }

    // --- MAVLink Bridge ---

    bool mavlink_create(std::string_view conn_string = "udp:14550") {
        nimcp_mavlink_config_t cfg = nimcp_mavlink_config_default();
        std::strncpy(cfg.connection_string, std::string(conn_string).c_str(), sizeof(cfg.connection_string) - 1);
        mavlink_bridge_ = nimcp_mavlink_bridge_create(&cfg);
        return mavlink_bridge_ != nullptr;
    }
    void mavlink_destroy() { if (mavlink_bridge_) { nimcp_mavlink_bridge_destroy(mavlink_bridge_); mavlink_bridge_ = nullptr; } }
    int mavlink_connect() { return mavlink_bridge_ ? nimcp_mavlink_bridge_connect(mavlink_bridge_) : -1; }
    int mavlink_disconnect() { return mavlink_bridge_ ? nimcp_mavlink_bridge_disconnect(mavlink_bridge_) : -1; }
    int mavlink_start() { return mavlink_bridge_ ? nimcp_mavlink_bridge_start(mavlink_bridge_) : -1; }
    int mavlink_stop() { return mavlink_bridge_ ? nimcp_mavlink_bridge_stop(mavlink_bridge_) : -1; }
    int mavlink_set_velocity(float vx, float vy, float vz, float yaw_rate) { return mavlink_bridge_ ? nimcp_mavlink_set_velocity(mavlink_bridge_, vx, vy, vz, yaw_rate) : -1; }
    int mavlink_arm(bool arm = true) { return mavlink_bridge_ ? nimcp_mavlink_arm(mavlink_bridge_, arm) : -1; }
    int mavlink_takeoff(float altitude = 5.0f) { return mavlink_bridge_ ? nimcp_mavlink_takeoff(mavlink_bridge_, altitude) : -1; }
    int mavlink_land() { return mavlink_bridge_ ? nimcp_mavlink_land(mavlink_bridge_) : -1; }
    int mavlink_goto(double lat, double lon, float alt = 10.0f) { return mavlink_bridge_ ? nimcp_mavlink_goto(mavlink_bridge_, lat, lon, alt) : -1; }
    int mavlink_rtl() { return mavlink_bridge_ ? nimcp_mavlink_rtl(mavlink_bridge_) : -1; }
    std::vector<float> mavlink_compose_features() {
        if (!mavlink_bridge_) return {};
        std::vector<float> features(NIMCP_MAVLINK_FEATURE_COUNT, 0.0f);
        int count = nimcp_mavlink_compose_features(mavlink_bridge_, features.data(), NIMCP_MAVLINK_FEATURE_COUNT);
        if (count < 0) return {};
        features.resize(static_cast<size_t>(count));
        return features;
    }

    // --- Memory Store ---

    struct MemoryStoreStats {
        uint64_t total_engrams = 0, total_concepts = 0;
        uint64_t total_relations = 0, total_autobio = 0;
        uint64_t total_writes = 0, total_reads = 0;
        uint64_t cache_hits = 0, cache_misses = 0;
        uint64_t db_size_bytes = 0;
    };

    std::optional<MemoryStoreStats> memory_store_stats() const {
        nimcp_memory_store_stats_t s{};
        int ret = nimcp_brain_memory_store_stats(handle_, &s);
        if (ret != 0) return std::nullopt;
        return MemoryStoreStats{s.total_engrams, s.total_concepts,
                                s.total_relations, s.total_autobio,
                                s.total_writes, s.total_reads,
                                s.cache_hits, s.cache_misses,
                                s.db_size_bytes};
    }

    std::vector<uint64_t> memory_search_text(std::string_view query,
                                              uint32_t max_results = 10) const {
        std::vector<uint64_t> ids(max_results);
        uint32_t count = 0;
        nimcp_brain_memory_search_text(
            handle_, std::string(query).c_str(), max_results,
            ids.data(), &count);
        ids.resize(count);
        return ids;
    }

    struct MemorySearchResult {
        uint64_t id = 0;
        float distance = 0.0f;
    };

    std::vector<MemorySearchResult> memory_search_similar(
        std::span<const float> embedding, uint32_t top_k = 5) const {
        std::vector<uint64_t> ids(top_k);
        std::vector<float> distances(top_k);
        uint32_t count = 0;
        nimcp_brain_memory_search_similar(
            handle_, embedding.data(),
            static_cast<uint32_t>(embedding.size()),
            top_k, ids.data(), distances.data(), &count);
        std::vector<MemorySearchResult> result(count);
        for (uint32_t i = 0; i < count; ++i)
            result[i] = {ids[i], distances[i]};
        return result;
    }

    bool memory_is_healthy() const {
        return nimcp_brain_memory_is_healthy(handle_);
    }

    // --- OOD Detection ---

    struct OodStats {
        uint64_t total_checks = 0, ood_detected = 0, in_distribution = 0;
        float avg_ood_score = 0.0f, ood_rate = 0.0f;
    };

    std::optional<OodStats> ood_stats() const {
        nimcp_ood_stats_t s{};
        int ret = nimcp_brain_ood_stats(handle_, &s);
        if (ret != 0) return std::nullopt;
        return OodStats{s.total_checks, s.ood_detected,
                        s.in_distribution, s.avg_ood_score, s.ood_rate};
    }

    // --- Audit Trail ---

    int audit_log(std::string_view description, uint32_t severity = 0,
                  std::string_view details = "") {
        return nimcp_brain_audit_log(
            handle_, std::string(description).c_str(),
            severity, std::string(details).c_str());
    }

    struct AuditResult {
        uint64_t id = 0;
        float severity = 0.0f;
    };

    std::vector<AuditResult> audit_search(uint32_t min_severity = 0,
                                           uint32_t max_results = 100) const {
        std::vector<uint64_t> ids(max_results);
        std::vector<float> severities(max_results);
        uint32_t count = 0;
        nimcp_brain_audit_search(
            handle_, min_severity, max_results,
            ids.data(), severities.data(), &count);
        std::vector<AuditResult> result(count);
        for (uint32_t i = 0; i < count; ++i)
            result[i] = {ids[i], severities[i]};
        return result;
    }

    // --- Cloud Connectivity ---

    void connect_cloud(Brain& cloud_brain,
                       float confidence_threshold,
                       bool enable_distillation) {
        check_status(nimcp_brain_connect_cloud(
            handle_, cloud_brain.get(),
            confidence_threshold, enable_distillation));
    }

    void disconnect_cloud() {
        check_status(nimcp_brain_disconnect_cloud(handle_));
    }

    // Move constructor/assignment use base class defaults
    Brain(Brain&&) noexcept = default;
    Brain& operator=(Brain&&) noexcept = default;

    ~Brain() {
        for (auto* fn : owned_callbacks_) delete fn;
    }

private:
    explicit Brain(nimcp_brain_t h) noexcept : Base(h) {}
    std::vector<detail::CppCallbackFn*> owned_callbacks_;
    nimcp_swarm_master_t* swarm_master_ = nullptr;
    nimcp_swarm_edge_runtime_t* swarm_edge_ = nullptr;
    nimcp_sensor_hub_t* sensor_hub_ = nullptr;
    nimcp_safety_watchdog_t* watchdog_ = nullptr;
    nimcp_ros2_bridge_t* ros2_bridge_ = nullptr;
    nimcp_mavlink_bridge_t* mavlink_bridge_ = nullptr;

    friend class BrainSnapshot;
};

// ============================================================================
// BrainSnapshot Class (COW snapshots)
// ============================================================================

class BrainSnapshot
    : public detail::HandleWrapper<nimcp_brain_snapshot_t,
                                   nimcp_brain_snapshot_destroy> {
    using Base = detail::HandleWrapper<nimcp_brain_snapshot_t,
                                      nimcp_brain_snapshot_destroy>;

public:
    explicit BrainSnapshot(const Brain& brain)
        : Base(nimcp_brain_snapshot_cow(brain.get()))
    {
        if (!handle_) throw MemoryError("Failed to create snapshot");
    }

    void restore_to(Brain& brain) const {
        check_status(nimcp_brain_restore_cow(brain.get(), handle_));
    }

    BrainSnapshot(BrainSnapshot&&) noexcept = default;
    BrainSnapshot& operator=(BrainSnapshot&&) noexcept = default;

private:
    explicit BrainSnapshot(nimcp_brain_snapshot_t h) noexcept : Base(h) {}
};

// ============================================================================
// Network Class
// ============================================================================

class Network
    : public detail::HandleWrapper<nimcp_network_t, nimcp_network_destroy> {
    using Base = detail::HandleWrapper<nimcp_network_t, nimcp_network_destroy>;

public:
    Network(uint32_t num_inputs, uint32_t num_outputs,
            uint32_t num_hidden, float learning_rate)
        : Base(nimcp_network_create(num_inputs, num_outputs,
                                    num_hidden, learning_rate))
    {
        if (!handle_) throw MemoryError("Failed to create network");
    }

    std::vector<float> forward(std::span<const float> inputs,
                               uint32_t num_outputs) {
        std::vector<float> outputs(num_outputs);
        check_status(nimcp_network_forward(
            handle_, inputs.data(), static_cast<uint32_t>(inputs.size()),
            outputs.data(), num_outputs));
        return outputs;
    }

    void train(std::span<const float> inputs,
               std::span<const float> targets) {
        check_status(nimcp_network_train(
            handle_, inputs.data(), static_cast<uint32_t>(inputs.size()),
            targets.data(), static_cast<uint32_t>(targets.size())));
    }

    Network(Network&&) noexcept = default;
    Network& operator=(Network&&) noexcept = default;
};

// ============================================================================
// Ethics Class
// ============================================================================

class Ethics
    : public detail::HandleWrapper<nimcp_ethics_t, nimcp_ethics_destroy> {
    using Base = detail::HandleWrapper<nimcp_ethics_t, nimcp_ethics_destroy>;

public:
    Ethics() : Base(nimcp_ethics_create()) {
        if (!handle_) throw MemoryError("Failed to create ethics module");
    }

    float check(std::span<const float> situation) const {
        float score = 0.0f;
        check_status(nimcp_ethics_check(
            handle_, situation.data(),
            static_cast<uint32_t>(situation.size()), &score));
        return score;
    }

    Ethics(Ethics&&) noexcept = default;
    Ethics& operator=(Ethics&&) noexcept = default;
};

// ============================================================================
// KnowledgeGraph Class
// ============================================================================

class KnowledgeGraph
    : public detail::HandleWrapper<nimcp_knowledge_t, nimcp_knowledge_destroy> {
    using Base = detail::HandleWrapper<nimcp_knowledge_t, nimcp_knowledge_destroy>;

public:
    KnowledgeGraph() : Base(nimcp_knowledge_create()) {
        if (!handle_) throw MemoryError("Failed to create knowledge graph");
    }

    void add_fact(std::string_view subject, std::string_view predicate,
                  std::string_view object) {
        check_status(nimcp_knowledge_add_fact(
            handle_,
            std::string(subject).c_str(),
            std::string(predicate).c_str(),
            std::string(object).c_str()));
    }

    std::string query(std::string_view q, uint32_t max_len = 1024) const {
        std::vector<char> buf(max_len, '\0');
        check_status(nimcp_knowledge_query(
            handle_, std::string(q).c_str(), buf.data(), max_len));
        return std::string(buf.data());
    }

    KnowledgeGraph(KnowledgeGraph&&) noexcept = default;
    KnowledgeGraph& operator=(KnowledgeGraph&&) noexcept = default;
};

// ============================================================================
// Free Functions
// ============================================================================

inline void init() {
    check_status(nimcp_init());
}

inline void shutdown() {
    nimcp_shutdown();
}

inline const char* version() {
    return nimcp_version();
}

inline int version_int() {
    return nimcp_version_int();
}

inline const char* get_error() {
    return nimcp_get_error();
}

// ============================================================================
// Library RAII Guard
// ============================================================================

class Library {
public:
    Library() { init(); }
    ~Library() { shutdown(); }

    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    Library(Library&&) = delete;
    Library& operator=(Library&&) = delete;
};

} // namespace nimcp

#endif // NIMCP_HPP

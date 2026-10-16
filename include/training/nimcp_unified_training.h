/**
 * @file nimcp_unified_training.h
 * @brief Unified Training Manager — single training infrastructure for all network types
 *
 * WHAT: Vtable-based training interface that LNN, SNN, CNN, and Adaptive networks plug into
 * WHY:  Eliminates duplicated anti-collapse logic, enables cross-network gradient flow,
 *       provides single composite loss and shared optimizer across all active networks
 * HOW:  Each network type implements nimcp_trainable_network_ops_t; the unified manager
 *       owns gradient management, diversity loss, normalization, and optimizer stepping
 *
 * @author NIMCP Development Team
 * @date 2026-03-11
 */

#ifndef NIMCP_UNIFIED_TRAINING_H
#define NIMCP_UNIFIED_TRAINING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of trainable networks in the unified manager */
#define NIMCP_UTM_MAX_NETWORKS      8

/** Maximum number of cross-network bridges */
#define NIMCP_UTM_MAX_BRIDGES       16

/** Default diversity buffer size (larger buffer = more comparisons = stronger signal) */
#define NIMCP_UTM_DEFAULT_DIVERSITY_BUFFER  32

/** Default gradient target norm (0.0 = sentinel for adaptive mode) */
#define NIMCP_UTM_DEFAULT_GRADIENT_TARGET   0.0f

/** Default diversity loss weight — must be strong enough to counteract collapse
 *  at scale. 0.1 was insufficient for 2M neuron / 4096 output networks. */
#define NIMCP_UTM_DEFAULT_DIVERSITY_WEIGHT  0.5f

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_unified_training_manager nimcp_unified_training_manager_t;
typedef struct nimcp_cross_network_bridge nimcp_cross_network_bridge_t;

/* External types (opaque) */
typedef struct nimcp_optimizer_context nimcp_optimizer_context_t;
typedef struct nimcp_gradient_manager_ctx nimcp_gradient_manager_ctx_t;
typedef struct nimcp_loss_context nimcp_loss_context_t;
#ifndef TPB_CONTEXT_TYPEDEF
#define TPB_CONTEXT_TYPEDEF
typedef struct tpb_context tpb_context_t;
#endif

/* Training middleware opaque handles */
typedef struct tcb_context tcb_context_t;
typedef struct checkpoint_mgr_s checkpoint_mgr_t;
typedef struct training_diagnoser training_diagnoser_t;
typedef struct nimcp_regularization_ctx nimcp_regularization_ctx_t;
typedef struct cl_ctx_s cl_ctx_t;
typedef struct adv_ctx_s adv_ctx_t;

/* GPU opaque handles */
typedef struct nimcp_checkpoint_ctx nimcp_checkpoint_ctx_t;

/* Information geometry opaque handles */
typedef struct nimcp_fisher_info_struct* nimcp_fisher_info_t;
typedef struct nimcp_natural_gradient_struct* nimcp_natural_gradient_t;
typedef struct nimcp_neural_manifold_struct* nimcp_neural_manifold_t;

//=============================================================================
// Network Type Enum
//=============================================================================

typedef enum nimcp_trainable_type {
    NIMCP_TRAINABLE_ADAPTIVE = 0,   /**< Adaptive/ANN backbone */
    NIMCP_TRAINABLE_CNN,            /**< Convolutional neural network */
    NIMCP_TRAINABLE_SNN,            /**< Spiking neural network */
    NIMCP_TRAINABLE_LNN,            /**< Liquid neural network */
    NIMCP_TRAINABLE_CUSTOM,         /**< User-defined network type */
    NIMCP_TRAINABLE_TYPE_COUNT
} nimcp_trainable_type_t;

//=============================================================================
// Parameter Group (for unified optimizer)
//=============================================================================

typedef struct nimcp_utm_param_group {
    float* params;              /**< Parameter array (owned by network) */
    float* gradients;           /**< Gradient array (owned by network) */
    size_t count;               /**< Number of parameters */
    float lr_scale;             /**< LR multiplier relative to base (1.0 = default) */
    float weight_decay;         /**< Group-specific weight decay (0 = use global) */
    const char* name;           /**< Human-readable name (e.g. "lnn_tau", "cnn_dense_0") */
} nimcp_utm_param_group_t;

//=============================================================================
// Trainable Network Interface (vtable)
//=============================================================================

/**
 * @brief Operations that every trainable network must implement
 *
 * Each network type (LNN, SNN, CNN, Adaptive) provides an adapter that
 * implements these operations. The unified manager calls them polymorphically.
 */
typedef struct nimcp_trainable_network_ops {
    /** Human-readable name (e.g. "LNN", "SNN_eProp") */
    const char* name;

    /** Network type tag */
    nimcp_trainable_type_t type;

    /**
     * @brief Forward pass: input → output, caching activations internally
     * @param ctx       Network-specific context
     * @param input     Input feature vector
     * @param input_dim Input dimension
     * @param output    Output buffer (caller-allocated, size = get_output_dim())
     * @param output_dim Output dimension
     * @return 0 on success, negative on error
     */
    int (*forward)(void* ctx, const float* input, uint32_t input_dim,
                   float* output, uint32_t output_dim);

    /**
     * @brief Backward pass: dL/dOutput → accumulate dL/dParams, optionally produce dL/dInput
     *
     * dL/dInput is required for cross-network gradient flow. If the network is
     * the first in the chain, dl_dinput may be NULL (no upstream to propagate to).
     *
     * @param ctx           Network-specific context
     * @param dl_doutput    Gradient of loss w.r.t. this network's output
     * @param output_dim    Output dimension
     * @param dl_dinput     [out] Gradient of loss w.r.t. this network's input (may be NULL)
     * @param input_dim     Input dimension
     * @return 0 on success, negative on error
     */
    int (*backward)(void* ctx, const float* dl_doutput, uint32_t output_dim,
                    float* dl_dinput, uint32_t input_dim);

    /**
     * @brief Export parameter groups for unified optimizer
     *
     * The network returns pointers to its internal parameter and gradient arrays,
     * grouped by learning rate requirements (e.g., tau params at 0.1x, biases at 2.0x).
     *
     * @param ctx        Network-specific context
     * @param groups     [out] Array of param groups (caller frees array, not contents)
     * @param num_groups [out] Number of groups
     * @return 0 on success, negative on error
     */
    int (*get_param_groups)(void* ctx, nimcp_utm_param_group_t** groups, uint32_t* num_groups);

    /**
     * @brief Zero all accumulated gradients
     * @param ctx Network-specific context
     * @return 0 on success, negative on error
     */
    int (*zero_grad)(void* ctx);

    /** @brief Get output dimensionality */
    uint32_t (*get_output_dim)(void* ctx);

    /** @brief Get input dimensionality */
    uint32_t (*get_input_dim)(void* ctx);

    /**
     * @brief Compute network-specific auxiliary loss (e.g., SNN spike regularization)
     *
     * This is added to the composite loss. Return 0.0f if no auxiliary loss.
     *
     * @param ctx Network-specific context
     * @return Auxiliary loss value
     */
    float (*compute_auxiliary_loss)(void* ctx);

    /**
     * @brief Destroy the adapter context (not the underlying network)
     * @param ctx Network-specific adapter context
     */
    void (*destroy)(void* ctx);

    /**
     * @brief Phase 4: Sync cached params back to underlying network after optimizer step
     *
     * Called after UTM's AdamW updates param groups. Only needed when the adapter
     * caches a copy of params (LNN, SNN). CNN modifies tensors in-place → no-op.
     * NULL is valid (no sync needed).
     *
     * @param ctx Network-specific adapter context
     * @return 0 on success, negative on error
     */
    int (*sync_params)(void* ctx);

} nimcp_trainable_network_ops_t;

/**
 * @brief A registered trainable network (ops vtable + context)
 */
typedef struct nimcp_trainable_network {
    const nimcp_trainable_network_ops_t* ops;
    void* ctx;                          /**< Adapter-owned context */
    float loss_weight;                  /**< Weight in composite loss (default 1.0) */
    bool enabled;                       /**< Can be temporarily disabled */
} nimcp_trainable_network_t;

//=============================================================================
// Cross-Network Bridge
//=============================================================================

typedef enum nimcp_bridge_type {
    NIMCP_BRIDGE_LINEAR = 0,            /**< Learnable linear projection */
    NIMCP_BRIDGE_RATE_TO_SPIKE,         /**< ANN → SNN: rate-to-spike encoding */
    NIMCP_BRIDGE_SPIKE_TO_RATE,         /**< SNN → ANN: spike-to-rate decoding */
    NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE,   /**< LNN → SNN */
    NIMCP_BRIDGE_IDENTITY               /**< Same-dimensionality passthrough */
} nimcp_bridge_type_t;

/**
 * @brief Differentiable bridge between two networks
 *
 * Enables gradient flow across network type boundaries.
 */
struct nimcp_cross_network_bridge {
    uint32_t source_idx;            /**< Index into manager's networks[] */
    uint32_t target_idx;            /**< Index into manager's networks[] */
    nimcp_bridge_type_t type;

    /* Learnable transform (for LINEAR bridge) */
    float* transform_weights;       /**< [target_dim x source_dim] */
    float* transform_bias;          /**< [target_dim] or NULL */
    float* weight_grad;             /**< Gradient for transform_weights */
    float* bias_grad;               /**< Gradient for transform_bias */
    uint32_t source_dim;
    uint32_t target_dim;

    /* Cached for backward pass */
    float* last_source_output;      /**< Cached source output */
    float* last_target_input;       /**< Cached transformed output */

    /* B7: Configurable bridge parameters (instead of #define constants) */
    float surrogate_beta;           /**< SuperSpike sharpness (default: 1.0) */
    float spike_rate_alpha;         /**< Spike-to-rate smoothing factor (default: 0.3) */
    float spike_gain;               /**< Rate-to-spike sigmoid gain (default: 5.0) */
    float spike_threshold;          /**< Spike threshold (default: 0.5) */

    bool enabled;

    /* Persistent GPU device buffers for bridge GEMV (avoids alloc/free per step) */
    void* gpu_d_W;          /**< Device: weight matrix [target_dim x source_dim] */
    void* gpu_d_x;          /**< Device: input vector [source_dim] */
    void* gpu_d_y;          /**< Device: output vector [target_dim] */
    uint32_t gpu_buf_rows;  /**< Allocated rows (target_dim) */
    uint32_t gpu_buf_cols;  /**< Allocated cols (source_dim) */
};

//=============================================================================
// Anti-Collapse Configuration
//=============================================================================

typedef struct nimcp_anti_collapse_config {
    float diversity_loss_weight;        /**< Weight for diversity penalty (default: 0.1) */
    uint32_t diversity_buffer_size;     /**< Ring buffer size (default: 16) */
    bool use_gradient_normalization;    /**< Normalize to target norm (default: true) */
    float gradient_target_norm;         /**< Target norm (0.0 = adaptive, else fixed) */
    float gradient_clip_value;          /**< Fallback clip value if normalization off (default: 5.0) */
    bool adaptive_gradient_target;     /**< Use adaptive target based on EMA of gradient norms (default: true) */
} nimcp_anti_collapse_config_t;

//=============================================================================
// Learning Rate Schedule
//=============================================================================

typedef enum nimcp_lr_schedule_type {
    NIMCP_LR_SCHEDULE_CONSTANT = 0,
    NIMCP_LR_SCHEDULE_COSINE,
    NIMCP_LR_SCHEDULE_STEP
} nimcp_lr_schedule_type_t;

typedef struct nimcp_lr_schedule_config {
    nimcp_lr_schedule_type_t type;
    uint64_t warmup_steps;          /**< Linear warmup steps (default: 1000) */
    uint64_t total_steps;           /**< Total training steps for cosine schedule (default: 100000) */
    float min_lr_ratio;             /**< Minimum LR as ratio of base (default: 0.01) */
    float step_decay_factor;        /**< Decay factor for step schedule (default: 0.5) */
    uint64_t step_decay_interval;   /**< Steps between decays (default: 10000) */
} nimcp_lr_schedule_config_t;

//=============================================================================
// Unified Training Manager Configuration
//=============================================================================

typedef struct nimcp_unified_training_config {
    /* Optimizer */
    uint32_t optimizer_type;            /**< nimcp_optimizer_type_t */
    float learning_rate;                /**< Base learning rate */
    float weight_decay;                 /**< Global weight decay */

    /* Anti-collapse */
    nimcp_anti_collapse_config_t anti_collapse;

    /* Loss */
    uint32_t loss_type;                 /**< nimcp_loss_type_t */

    /* Composite loss */
    bool use_composite_loss;            /**< Combine losses across networks (default: true) */

    /* Execution */
    bool enable_cross_network_gradients;/**< Enable gradient flow through bridges */

    /* Mini-batching */
    uint32_t batch_size;                /**< Accumulate gradients over N samples (default: 1) */

    /* Unified optimizer control */
    bool unified_optimizer;             /**< When true, adapters skip internal optimizer step (default: false) */

    /* GPU acceleration */
    bool enable_mixed_precision;        /**< Enable FP16/AMP via autocast (default: false) */
    bool enable_sparse_training;        /**< Enable sparse GPU gradient ops (default: false) */

    /* LR Schedule */
    nimcp_lr_schedule_config_t lr_schedule;

    /* DFA health monitoring */
    uint32_t loss_history_size;             /**< Ring buffer size (default: 256, 0=disabled) */
    uint32_t health_check_interval;         /**< Run DFA every N steps (default: 64) */
    bool dfa_auto_adjust_lr;                /**< Auto-adjust LR based on health (default: true) */
    bool enable_pink_noise_dfa_feedback;    /**< DFA health → pink noise amplitude (default: true) */

    /* Quantum annealing for plateau escape */
    bool enable_quantum_anneal;             /**< Anneal weights on plateau (default: true) */
    uint32_t plateau_anneal_threshold;      /**< Consecutive PLATEAU checks before trigger (default: 3) */

    /* Quantum Shannon bottleneck detection */
    uint32_t bottleneck_check_interval;     /**< Check bridges every N steps (default: 128, 0=disabled) */

    /* Natural gradient optimizer */
    bool enable_natural_gradient;           /**< Use NG instead of AdamW where practical (default: true) */
    uint32_t fisher_update_interval;        /**< Recompute Fisher every N steps (default: 16) */
    uint32_t natural_grad_max_params;       /**< Max param group size for NG (default: 4096) */

    /* Manifold tracking */
    bool enable_manifold_tracking;          /**< Track output manifold dimensionality (default: true) */

    /* Item 2: Bridge params in AdamW optimizer (instead of simple SGD) */
    bool bridge_params_in_optimizer;        /**< Include bridge weights in AdamW (default: true) */

    /* Item 3: Automatic Mixed Precision */
    bool enable_amp;                        /**< Enable AMP via training AMP API (default: true) */

    /* Item 6: Curriculum learning */
    bool enable_curriculum;                 /**< Enable curriculum sample weighting (default: true) */
    uint32_t curriculum_num_samples;        /**< Number of samples in curriculum (default: 1024) */

    /* Item 7: Knowledge distillation */
    bool enable_knowledge_distillation;     /**< Enable KD loss composition (default: true) */
    float kd_loss_weight;                   /**< Weight of KD loss in composite (default: 0.3) */

    /* Item 8: Neuromodulator-gated LR */
    bool enable_neuromod_lr;                /**< Modulate LR via neuromodulator levels (default: true) */

    /* Item 9: EMA / Polyak averaging */
    bool enable_ema;                        /**< Shadow parameter EMA for inference (default: true) */
    float ema_decay;                        /**< EMA decay factor (default: 0.999) */

    /* Item 10: Middleware LR scheduler (cosine warm restarts) */
    bool enable_lr_scheduler;               /**< Use middleware scheduler instead of built-in (default: true) */

    /* Item 11: Riemannian SGD */
    bool enable_riemannian_sgd;             /**< Use Riemannian gradient for small groups (default: true) */
    uint32_t riemannian_max_params;         /**< Max params for Riemannian SGD (default: 2048) */

    /* Item 12: Contrastive loss */
    bool enable_contrastive_loss;           /**< Cross-network contrastive loss (default: true) */
    float contrastive_loss_weight;          /**< Weight in composite (default: 0.1) */
    float contrastive_margin;               /**< Contrastive margin (default: 1.0) */

    /* Item 13: Per-network LR */
    float per_network_lr[NIMCP_UTM_MAX_NETWORKS]; /**< Per-network LR (0 = use global) */

    /* Item 14: Early stopping */
    bool enable_early_stopping;             /**< Enable early stopping (default: true) */
    uint32_t early_stopping_patience;       /**< Steps without improvement (default: 50) */
    float early_stopping_min_delta;         /**< Minimum improvement threshold (default: 1e-5) */

    /* 15-item gap analysis: Extended pipeline config */

    /* Gap 1: AMP autocast lifecycle (uses enable_amp above) */

    /* Gap 2: Gradient manager — NaN/Inf detection + sanitization */
    bool enable_gradient_manager;           /**< Wire gradient health check before optimizer (default: true) */

    /* Gap 3: Middleware optimizer — replaces inline AdamW with serializable state */
    bool enable_middleware_optimizer;        /**< Use nimcp_optimizer_step_group instead of inline (default: true) */

    /* Gap 4: Training callbacks */
    bool enable_training_callbacks;         /**< Fire TCB events on step complete/divergence (default: true) */

    /* Gap 5: Checkpoint manager */
    bool enable_checkpoint_manager;         /**< Auto-save optimizer/EMA/scheduler state (default: true) */
    uint32_t checkpoint_interval;           /**< Steps between checkpoint saves (default: 1000) */

    /* Gap 6: Training diagnosis — replace hardcoded DFA→LR with root-cause analysis */
    bool enable_training_diagnosis;         /**< Use diagnoser for LR adjustment (default: true) */

    /* Gap 7: Regularization — L1/ElasticNet + label smoothing */
    bool enable_regularization;             /**< Apply L1 gradient penalty (default: true) */
    float l1_lambda;                        /**< L1 regularization strength (default: 1e-5) */
    float label_smoothing;                  /**< Label smoothing factor (default: 0.1) */

    /* Gap 8: Bridge real AdamW — actual Adam momentum for bridge params */
    /* (uses bridge_params_in_optimizer above; adds real momentum state) */

    /* Gap 9: Continual learning (EWC) */
    bool enable_continual_learning;         /**< Add EWC penalty to prevent forgetting (default: true) */
    float ewc_lambda;                       /**< EWC penalty weight (default: 0.1) */

    /* Gap 10: Adversarial training (FGSM/PGD augmentation) — requires forward_fn */
    bool enable_adversarial_training;       /**< Enable adversarial augmentation (default: false) */

    /* Gap 11: Fused GPU inference ops — CUDA-gated */
    bool enable_fused_inference;            /**< Use CUDA graph capture for fused ops (default: true) */

    /* Gap 13: EMA → inference swap */
    bool enable_ema_inference_swap;         /**< Swap EMA params before inference (default: true) */

    /* Gap 14: GPU gradient checkpointing — CUDA-gated */
    bool enable_gradient_checkpointing;     /**< Activation memory recovery (default: true) */

    /* Gap 15: Cross-network gradients default (changed from false to true) */
    /* (uses enable_cross_network_gradients above — default changed) */

} nimcp_unified_training_config_t;

//=============================================================================
// Anti-Collapse State (shared, not per-network)
//=============================================================================

typedef struct nimcp_anti_collapse_state {
    float* diversity_buffer;            /**< [buffer_size × output_dim] */
    uint32_t buffer_pos;
    uint32_t buffer_count;
    uint32_t output_dim;
    nimcp_anti_collapse_config_t config;
    float ema_gradient_norm;            /**< EMA of gradient norms for adaptive target */
    float ema_alpha;                    /**< EMA smoothing factor (default: 0.01) */
} nimcp_anti_collapse_state_t;

//=============================================================================
// Training Health (DFA-based monitoring)
//=============================================================================

typedef enum nimcp_training_health {
    NIMCP_TRAINING_HEALTH_UNKNOWN = 0,
    NIMCP_TRAINING_HEALTH_OPTIMAL,      /**< DFA α ≈ 0.8-1.2, pink-noise-like */
    NIMCP_TRAINING_HEALTH_NOISY,        /**< DFA α < 0.6, white-noise-like */
    NIMCP_TRAINING_HEALTH_DRIFTING,     /**< DFA α > 1.3, brown-noise-like */
    NIMCP_TRAINING_HEALTH_OSCILLATING,  /**< DFA α < 0.3, anti-persistent */
    NIMCP_TRAINING_HEALTH_PLATEAU       /**< Hurst H > 0.8, persistent stagnation */
} nimcp_training_health_t;

//=============================================================================
// Unified Training Manager
//=============================================================================

struct nimcp_unified_training_manager {
    /* Registered networks */
    nimcp_trainable_network_t networks[NIMCP_UTM_MAX_NETWORKS];
    uint32_t num_networks;

    /* Cross-network bridges */
    nimcp_cross_network_bridge_t bridges[NIMCP_UTM_MAX_BRIDGES];
    uint32_t num_bridges;

    /* Per-network anti-collapse state (prevents diversity buffer sharing) */
    nimcp_anti_collapse_state_t anti_collapse[NIMCP_UTM_MAX_NETWORKS];

    /* Composite loss tracking */
    float last_composite_loss;
    float per_network_loss[NIMCP_UTM_MAX_NETWORKS];

    /* Training state */
    uint64_t step_count;
    float current_lr;
    uint32_t batch_accumulation_count;  /**< Samples accumulated in current batch */

    /* Configuration */
    nimcp_unified_training_config_t config;

    /* Phase 5: Plasticity bridge for backprop gating (optional, may be NULL) */
    tpb_context_t* plasticity_bridge;

    /* GPU acceleration context (optional, may be NULL) */
    void* gpu_ctx;                  /**< nimcp_gpu_training_ctx_t* (not owned) */

    /* AdamW optimizer state per param group */
    float** adam_m;                 /**< First moment vectors (one per param group) */
    float** adam_v;                 /**< Second moment vectors (one per param group) */
    size_t* adam_sizes;             /**< Size of each moment vector */
    uint32_t adam_num_groups;       /**< Number of allocated moment groups */
    float adam_beta1_t;             /**< beta1^t for bias correction */
    float adam_beta2_t;             /**< beta2^t for bias correction */

    /* Fractal-aware training (optional) */
    float fractal_lr_multiplier[NIMCP_UTM_MAX_NETWORKS]; /**< Per-network LR scale from centrality (default: 1.0) */
    bool fractal_enabled;                                  /**< Enable fractal-aware LR scaling */

    /* DFA health monitoring */
    float* loss_history;                    /**< Ring buffer of recent losses */
    uint32_t loss_history_size;             /**< Buffer capacity (default: 256) */
    uint32_t loss_history_pos;              /**< Current write position */
    uint32_t loss_history_count;            /**< Filled count */
    uint32_t health_check_interval;         /**< Run DFA every N steps (default: 64) */
    nimcp_training_health_t training_health; /**< Last computed health status */
    float dfa_exponent;                     /**< Last DFA alpha */
    float hurst_exponent;                   /**< Last Hurst exponent */
    float lacunarity_value;                 /**< Last lacunarity */
    bool is_multifractal;                   /**< Last multifractal analysis result */
    float multifractal_width;               /**< Spectrum width (0 = monofractal) */
    bool dfa_auto_adjust_lr;                /**< Auto-adjust LR based on health */

    /* Quantum annealing for plateau escape */
    uint32_t plateau_consecutive_count;     /**< How many consecutive PLATEAU checks */
    uint32_t plateau_anneal_threshold;      /**< Trigger annealing after N consecutive */
    bool enable_quantum_anneal;             /**< Enable quantum annealing */

    /* Quantum Shannon bottleneck detection */
    float bridge_bottleneck_severity[NIMCP_UTM_MAX_BRIDGES]; /**< 0=no bottleneck, 1=severe */
    uint32_t bottleneck_check_interval;     /**< Check every N steps */

    /* Natural gradient optimizer (optional, per-network) */
    nimcp_natural_gradient_t natural_grad[NIMCP_UTM_MAX_NETWORKS];
    nimcp_fisher_info_t fisher[NIMCP_UTM_MAX_NETWORKS];
    bool natural_gradient_enabled;
    uint32_t fisher_update_interval;        /**< Recompute Fisher every N steps */
    uint32_t natural_grad_max_params;       /**< Max param group size for NG */

    /* Phase coherence (cross-network health) */
    float cross_network_coherence;          /**< Phase coherence (0=diverse, 1=collapsed) */
    float* per_network_loss_history;        /**< [loss_history_size × num_networks] ring buffer */

    /* Manifold tracking */
    nimcp_neural_manifold_t output_manifold[NIMCP_UTM_MAX_NETWORKS];
    uint32_t manifold_intrinsic_dim[NIMCP_UTM_MAX_NETWORKS];
    bool manifold_tracking_enabled;

    /* Item 2: Bridge params in optimizer */
    bool bridge_params_in_optimizer;

    /* Item 3: AMP context (amp_ctx_t*, lazy-created) */
    void* amp_ctx;

    /* Item 6: Curriculum context (curriculum_ctx_t*, lazy-created) */
    void* curriculum_ctx;

    /* Item 7: KD context (kd_ctx_t*, lazy-created) */
    void* kd_ctx;
    float kd_loss_weight;

    /* Item 8: Neuromod adapter (nimcp_neuromod_pool_adapter_t, set externally) */
    void* neuromod_adapter;
    float neuromod_lr_scale;                /**< Last computed neuromod LR multiplier */

    /* Item 9: EMA / Polyak averaging */
    float** ema_params;                     /**< Shadow parameter copies [num_groups] */
    size_t* ema_sizes;                      /**< Size of each shadow copy */
    uint32_t ema_num_groups;
    float ema_decay;
    bool ema_enabled;

    /* Item 10: Middleware LR scheduler (nimcp_lr_scheduler_ctx_t*, lazy-created) */
    void* lr_scheduler_ctx;

    /* Item 11: Riemannian SGD */
    void* riemannian_metric;                /**< riemannian_metric_t* (lazy-created) */
    uint32_t riemannian_max_params;
    bool riemannian_enabled;

    /* Item 13: Per-network LR overrides */
    float per_network_lr[NIMCP_UTM_MAX_NETWORKS]; /**< 0 = use global */

    /* Item 14: Early stopping */
    bool early_stopping_enabled;
    uint32_t early_stopping_patience;
    float early_stopping_min_delta;
    float early_stopping_best_loss;
    uint32_t early_stopping_counter;
    bool early_stopped;                     /**< True when training should stop */

    /* 15-item gap analysis: Extended pipeline state */

    /* Gap 2: Gradient manager */
    nimcp_gradient_manager_ctx_t* gradient_manager;  /**< NaN/Inf detection (lazy-created) */

    /* Gap 3: Middleware optimizer */
    nimcp_optimizer_context_t* middleware_optimizer;  /**< Serializable AdamW (lazy-created) */

    /* Gap 4: Training callbacks */
    tcb_context_t* tcb_ctx;                         /**< Callback event system (lazy-created) */

    /* Gap 5: Checkpoint manager */
    checkpoint_mgr_t* checkpoint_mgr;               /**< Checkpoint save/restore (lazy-created) */

    /* Gap 6: Training diagnosis */
    training_diagnoser_t* diagnoser;                /**< Root-cause analysis (lazy-created) */
    float previous_loss;                            /**< Loss from previous step for diagnosis */
    float previous_grad_norm;                       /**< Grad norm from previous step */

    /* Gap 7: Regularization */
    nimcp_regularization_ctx_t* regularization_ctx;  /**< L1/ElasticNet/label smoothing (lazy-created) */

    /* Gap 8: Bridge AdamW real momentum state */
    float** bridge_adam_m;                          /**< First moment for bridge weights */
    float** bridge_adam_v;                          /**< Second moment for bridge weights */
    size_t* bridge_adam_sizes;                      /**< Size of each bridge moment vector */
    uint32_t bridge_adam_num;                       /**< Number of bridge moment arrays */

    /* Gap 9: Continual learning */
    cl_ctx_t* cl_ctx;                               /**< EWC/replay (lazy-created, caller-settable) */
    float ewc_lambda;                               /**< EWC penalty weight */

    /* Gap 10: Adversarial training */
    adv_ctx_t* adv_ctx;                             /**< Adversarial augmentation (lazy-created) */

    /* Gap 14: GPU gradient checkpointing */
    nimcp_checkpoint_ctx_t* grad_checkpoint_ctx;    /**< Activation checkpointing (CUDA-gated) */

    /* Gap 13: EMA inference swap tracking */
    bool ema_swapped_in;                            /**< True when EMA params are loaded for inference */

    /* Pink noise ↔ DFA closed-loop feedback */
    void* pink_noise;                               /**< neuromod_pink_noise_t* (set externally) */
    bool pink_noise_dfa_feedback;                   /**< Enable DFA → pink noise amplitude modulation */
    float pink_noise_base_amplitude[4];             /**< Original amplitudes [DA, 5-HT, ACh, NE] */
    bool pink_noise_base_captured;                  /**< True once base amplitudes are captured */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create a unified training manager with default config
 * @param config Configuration (NULL for defaults)
 * @return New manager, or NULL on failure
 */
nimcp_unified_training_manager_t* nimcp_utm_create(
    const nimcp_unified_training_config_t* config);

/**
 * @brief Destroy the unified training manager
 * Destroys adapters and bridges but NOT the underlying networks.
 */
void nimcp_utm_destroy(nimcp_unified_training_manager_t* mgr);

/**
 * @brief Get default configuration
 */
void nimcp_utm_default_config(nimcp_unified_training_config_t* config);

//=============================================================================
// Network Registration
//=============================================================================

/**
 * @brief Register a trainable network with the manager
 * @param mgr        Manager
 * @param ops        Vtable (must outlive the registration)
 * @param ctx        Adapter context (manager takes ownership)
 * @param loss_weight Weight in composite loss (1.0 = equal weight)
 * @return Network index (>=0) on success, negative on error
 */
int nimcp_utm_register_network(nimcp_unified_training_manager_t* mgr,
                               const nimcp_trainable_network_ops_t* ops,
                               void* ctx,
                               float loss_weight);

/**
 * @brief Enable/disable a registered network
 */
int nimcp_utm_set_network_enabled(nimcp_unified_training_manager_t* mgr,
                                  uint32_t network_idx, bool enabled);

/**
 * @brief Set the plasticity bridge for backprop gating (Phase 5)
 *
 * When set, the manager will suppress biological plasticity during the backward
 * pass to prevent STDP/BCM interference with gradient-based training.
 *
 * @param mgr  Manager
 * @param tpb  Plasticity bridge context (not owned, must outlive manager; NULL to disable)
 */
void nimcp_utm_set_plasticity_bridge(nimcp_unified_training_manager_t* mgr,
                                      tpb_context_t* tpb);

//=============================================================================
// Bridge API
//=============================================================================

/**
 * @brief Add a cross-network bridge
 * @param mgr         Manager
 * @param source_idx  Source network index
 * @param target_idx  Target network index
 * @param type        Bridge type
 * @return Bridge index (>=0) on success, negative on error
 */
int nimcp_utm_add_bridge(nimcp_unified_training_manager_t* mgr,
                         uint32_t source_idx, uint32_t target_idx,
                         nimcp_bridge_type_t type);

//=============================================================================
// Training Step
//=============================================================================

/**
 * @brief Result of a unified training step
 */
typedef struct nimcp_utm_step_result {
    float composite_loss;               /**< Weighted sum of all network losses */
    float diversity_loss;               /**< Anti-collapse diversity penalty */
    float per_network_loss[NIMCP_UTM_MAX_NETWORKS]; /**< Per-network losses */
    float gradient_norm;                /**< Global gradient norm before normalization */
    float gradient_scale;               /**< Scale factor applied to gradients */
    uint64_t step;                      /**< Current step count */

    /* Fractal health monitoring */
    nimcp_training_health_t training_health; /**< Last computed health status */
    float dfa_exponent;                 /**< Last DFA alpha */
    float hurst_exponent;               /**< Last Hurst H */
    float lacunarity_value;             /**< Last lacunarity */
    bool is_multifractal;               /**< Last multifractal result */

    /* Cross-network coherence */
    float cross_network_coherence;      /**< Phase coherence (0=diverse, 1=collapsed) */

    /* Manifold tracking */
    uint32_t manifold_intrinsic_dim[NIMCP_UTM_MAX_NETWORKS]; /**< 0 = not computed */

    /* Extended pipeline fields */
    bool early_stopped;                     /**< True if early stopping triggered */
    float neuromod_lr_scale;                /**< Neuromod LR multiplier applied this step */
    float scheduled_lr;                     /**< LR after scheduling this step */
    float contrastive_loss;                 /**< Cross-network contrastive loss */
    float kd_loss;                          /**< Knowledge distillation loss */

    /* 15-item gap analysis result fields */
    bool gradient_healthy;                  /**< True if no NaN/Inf detected */
    uint64_t gradients_sanitized;           /**< Count of NaN/Inf values replaced */
    float ewc_penalty;                      /**< Continual learning EWC penalty this step */
    float l1_penalty;                       /**< L1 regularization penalty this step */
    bool diagnosis_reduce_lr;               /**< Diagnoser recommends reducing LR */
    float diagnosis_lr_factor;              /**< Recommended LR factor from diagnoser */
} nimcp_utm_step_result_t;

/**
 * @brief Execute one unified training step
 *
 * 1. Zero all gradients
 * 2. Forward pass through networks (topology order)
 * 3. Compute composite loss + diversity loss
 * 4. Backward pass (reverse order, with bridge gradient flow)
 * 5. Gradient normalization / clipping
 * 6. Optimizer step across all parameter groups
 *
 * @param mgr        Manager
 * @param input      Input features
 * @param input_dim  Input dimension
 * @param target     Target output
 * @param target_dim Target dimension
 * @param result     [out] Step result (may be NULL)
 * @return 0 on success, negative on error
 */
int nimcp_utm_step(nimcp_unified_training_manager_t* mgr,
                   const float* input, uint32_t input_dim,
                   const float* target, uint32_t target_dim,
                   nimcp_utm_step_result_t* result);

/**
 * @brief Forward-only inference through registered networks (no backward/optimizer)
 *
 * When enable_fused_inference is true, skips gradient checkpointing and
 * backward-related allocations for faster inference throughput.
 *
 * @param mgr      Training manager
 * @param input    Input data
 * @param input_dim Input dimensionality
 * @param output   [out] Blended output from all networks
 * @param output_dim Output buffer size
 * @return 0 on success, negative on error
 */
int nimcp_utm_forward_only(nimcp_unified_training_manager_t* mgr,
                           const float* input, uint32_t input_dim,
                           float* output, uint32_t output_dim);

/**
 * @brief Compute scheduled learning rate for current step
 */
float nimcp_utm_get_scheduled_lr(const nimcp_unified_training_manager_t* mgr);

//=============================================================================
// Anti-Collapse API (standalone, usable without full manager)
//=============================================================================

/**
 * @brief Initialize anti-collapse state
 */
int nimcp_anti_collapse_init(nimcp_anti_collapse_state_t* state,
                             const nimcp_anti_collapse_config_t* config);

/**
 * @brief Destroy anti-collapse state
 */
void nimcp_anti_collapse_destroy(nimcp_anti_collapse_state_t* state);

/**
 * @brief Compute diversity loss and add gradient contribution
 *
 * Updates the ring buffer with the current output, computes average cosine
 * similarity against recent outputs, and adds diversity gradient to grad_output.
 *
 * @param state       Anti-collapse state
 * @param output      Current network output
 * @param grad_output [in/out] Loss gradient — diversity gradient is ADDED to this
 * @param dim         Output dimensionality
 * @return Diversity loss value (cosine similarity penalty)
 */
float nimcp_anti_collapse_diversity_loss(nimcp_anti_collapse_state_t* state,
                                         const float* output,
                                         float* grad_output,
                                         uint32_t dim);

/**
 * @brief Apply gradient normalization or clipping
 *
 * If use_gradient_normalization: normalizes to target_norm (preserves direction)
 * Else: clips to gradient_clip_value (only scales down)
 *
 * @param config      Anti-collapse config
 * @param gradients   Array of gradient arrays
 * @param sizes       Array of gradient counts per array
 * @param num_arrays  Number of gradient arrays
 * @return Scale factor applied (1.0 if no scaling needed)
 */
float nimcp_anti_collapse_normalize_gradients(
    nimcp_anti_collapse_state_t* state,
    float** gradients, const size_t* sizes, uint32_t num_arrays);

//=============================================================================
// Adapter Creation Helpers
//=============================================================================

/* Forward declarations of adapter creation functions.
 * Each returns a vtable + context pair. The caller registers them via
 * nimcp_utm_register_network(). */

struct cnn_trainer_s;
struct snn_backprop_ctx_s;
struct lnn_training_ctx_s;
struct neural_network_struct;

/**
 * @brief Create adapter for CNN trainer
 * @param trainer Existing CNN trainer (not owned, must outlive adapter)
 * @param[out] ops  Vtable pointer
 * @param[out] ctx  Adapter context
 * @return 0 on success
 */
int nimcp_trainable_cnn_create(struct cnn_trainer_s* trainer,
                               const nimcp_trainable_network_ops_t** ops,
                               void** ctx);

/**
 * @brief Create adapter for SNN backprop context
 */
int nimcp_trainable_snn_create(struct snn_backprop_ctx_s* snn_ctx,
                               const nimcp_trainable_network_ops_t** ops,
                               void** ctx);

/** @brief Set managed_by_utm flag on SNN adapter (enables UTM optimizer for SNN weights) */
void nimcp_trainable_snn_set_managed(void* ctx, bool managed);

/** @brief Set input/output dimensions on SNN adapter */
void nimcp_trainable_snn_set_dims(void* ctx, uint32_t input_dim, uint32_t output_dim);

/**
 * @brief Create adapter for LNN training context
 */
int nimcp_trainable_lnn_create(struct lnn_training_ctx_s* lnn_ctx,
                               const nimcp_trainable_network_ops_t** ops,
                               void** ctx);

/** @brief Set managed_by_utm flag on LNN adapter (enables UTM optimizer for LNN params) */
void nimcp_trainable_lnn_set_managed(void* ctx, bool managed);

/** @brief Set input/output dimensions on LNN adapter */
void nimcp_trainable_lnn_set_dims(void* ctx, uint32_t input_dim, uint32_t output_dim);

/**
 * @brief Create adapter for Adaptive (backbone) network
 */
int nimcp_trainable_adaptive_create(struct neural_network_struct* network,
                                    const nimcp_trainable_network_ops_t** ops,
                                    void** ctx);

/** @brief Set input/output dimensions on Adaptive adapter */
void nimcp_trainable_adaptive_set_dims(void* ctx, uint32_t input_dim, uint32_t output_dim);

/** @brief Set input/output dimensions on CNN adapter */
void nimcp_trainable_cnn_set_dims(void* ctx, uint32_t input_dim, uint32_t output_dim);

//=============================================================================
// Cross-Network Bridge Forward/Backward (specialized implementations)
//=============================================================================

/** @brief Rate-to-spike forward: continuous [0,1] → soft spike probabilities (ANN→SNN) */
void bridge_rate_to_spike_forward(const nimcp_cross_network_bridge_t* b,
                                   const float* source_output,
                                   float* target_input);

/** @brief Rate-to-spike backward: surrogate gradient through sigmoid threshold */
void bridge_rate_to_spike_backward(const nimcp_cross_network_bridge_t* b,
                                    const float* dl_dtarget,
                                    float* dl_dsource);

/** @brief Spike-to-rate forward: spikes → continuous rates via exponential smoothing (SNN→ANN) */
void bridge_spike_to_rate_forward(const nimcp_cross_network_bridge_t* b,
                                   const float* source_output,
                                   float* target_input);

/** @brief Spike-to-rate backward: surrogate gradient through spike boundary */
void bridge_spike_to_rate_backward(const nimcp_cross_network_bridge_t* b,
                                    const float* dl_dtarget,
                                    float* dl_dsource);

/** @brief Continuous-to-spike forward: ODE states → spike probabilities (LNN→SNN) */
void bridge_continuous_to_spike_forward(const nimcp_cross_network_bridge_t* b,
                                         const float* source_output,
                                         float* target_input);

/** @brief Continuous-to-spike backward: chain rule through tanh + sigmoid + surrogate */
void bridge_continuous_to_spike_backward(const nimcp_cross_network_bridge_t* b,
                                          const float* dl_dtarget,
                                          float* dl_dsource);

//=============================================================================
// Fractal-Aware Training API
//=============================================================================

/** @brief Set per-network fractal LR multiplier (hub centrality) */
void nimcp_utm_set_fractal_lr(nimcp_unified_training_manager_t* mgr,
                               uint32_t net_idx, float scale);

/** @brief Query current training health (from most recent DFA check) */
nimcp_training_health_t nimcp_utm_get_health(const nimcp_unified_training_manager_t* mgr);

/** @brief Query last DFA exponent */
float nimcp_utm_get_dfa_exponent(const nimcp_unified_training_manager_t* mgr);

/** @brief Enable/disable natural gradient optimizer for a specific network */
void nimcp_utm_set_natural_gradient(nimcp_unified_training_manager_t* mgr,
                                     uint32_t net_idx, bool enabled);

//=============================================================================
// Extended Pipeline API (Items 1-14)
//=============================================================================

/** @brief Set AMP context (caller-owned, must outlive manager) */
void nimcp_utm_set_amp(nimcp_unified_training_manager_t* mgr, void* amp_ctx);

/** @brief Set curriculum context (caller-owned, must outlive manager) */
void nimcp_utm_set_curriculum(nimcp_unified_training_manager_t* mgr, void* curriculum_ctx);

/** @brief Set knowledge distillation context (caller-owned, must outlive manager) */
void nimcp_utm_set_kd(nimcp_unified_training_manager_t* mgr, void* kd_ctx);

/** @brief Set neuromodulator pool adapter (caller-owned, must outlive manager) */
void nimcp_utm_set_neuromod(nimcp_unified_training_manager_t* mgr, void* neuromod_adapter);

/** @brief Set pink noise system for DFA-driven amplitude feedback (caller-owned) */
void nimcp_utm_set_pink_noise(nimcp_unified_training_manager_t* mgr, void* pink_noise);

/** @brief Set per-network learning rate (0 = use global) */
void nimcp_utm_set_per_network_lr(nimcp_unified_training_manager_t* mgr,
                                    uint32_t net_idx, float lr);

/** @brief Check if early stopping has triggered */
bool nimcp_utm_is_early_stopped(const nimcp_unified_training_manager_t* mgr);

/** @brief Enable/disable early stopping at runtime. Disabling does NOT
 *  clear an already-tripped early_stopped flag — call reset for that. */
void nimcp_utm_set_early_stopping_enabled(nimcp_unified_training_manager_t* mgr,
                                          bool enabled);

/** @brief Clear an already-tripped early-stop. Resets early_stopped=false,
 *  counter=0, best_loss=+inf so the next composite loss starts fresh.
 *  Use between curriculum phases (e.g. after Sensory Enrichment) to
 *  unblock training when the prior phase plateaued legitimately. */
void nimcp_utm_reset_early_stopping(nimcp_unified_training_manager_t* mgr);

/** @brief Get EMA-averaged parameters for a param group (copies out) */
int nimcp_utm_get_ema_params(const nimcp_unified_training_manager_t* mgr,
                               uint32_t group_idx, float* out_params, size_t count);

//=============================================================================
// Extended Pipeline API (15-Item Gap Analysis)
//=============================================================================

/** @brief Set continual learning context (caller-owned, must outlive manager) */
void nimcp_utm_set_cl(nimcp_unified_training_manager_t* mgr, cl_ctx_t* cl_ctx);

/** @brief Set adversarial training context (caller-owned, must outlive manager) */
void nimcp_utm_set_adversarial(nimcp_unified_training_manager_t* mgr, adv_ctx_t* adv_ctx);

/** @brief Set training callback context (caller-owned, must outlive manager) */
void nimcp_utm_set_callbacks(nimcp_unified_training_manager_t* mgr, tcb_context_t* tcb_ctx);

/** @brief Set checkpoint manager (caller-owned, must outlive manager) */
void nimcp_utm_set_checkpoint_mgr(nimcp_unified_training_manager_t* mgr, checkpoint_mgr_t* ckpt_mgr);

/** @brief Swap EMA parameters in for inference (swaps live ↔ EMA) */
int nimcp_utm_swap_to_ema(nimcp_unified_training_manager_t* mgr);

/** @brief Swap live parameters back after inference (reverses EMA swap) */
int nimcp_utm_swap_from_ema(nimcp_unified_training_manager_t* mgr);

/** @brief Get gradient health status from last step */
bool nimcp_utm_gradients_healthy(const nimcp_unified_training_manager_t* mgr);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_UNIFIED_TRAINING_H */

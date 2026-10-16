/**
 * @file nimcp_unified_training.c
 * @brief Unified Training Manager — lifecycle, registration, and training step
 *
 * WHAT: Single manager for all trainable networks with composite loss and shared infrastructure
 * WHY:  Eliminates per-network duplication, enables cross-network gradient flow
 * HOW:  Vtable-based polymorphism, topology-ordered forward/backward, unified gradient management
 *
 * @author NIMCP Development Team
 * @date 2026-03-11
 */

#include "training/nimcp_unified_training.h"
#include <time.h>
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

/* Fractal + geometry + quantum integration */
#include "cognitive/memory/core/nimcp_fractal.h"
#include "physics/geometry/nimcp_information_geometry.h"
#include "utils/math/nimcp_complex_math.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

/* Extended pipeline (Items 1-14) */
#include "training/nimcp_mixed_precision.h"
#include "training/nimcp_curriculum_learning.h"
#include "training/nimcp_knowledge_distillation.h"
#include "middleware/training/nimcp_lr_scheduler.h"
#include "integration/adapters/neuromod/nimcp_neuromod_pool_adapter.h"
#include "utils/geometry/nimcp_differential_geometry.h"
#include "middleware/training/nimcp_loss_functions.h"

/* 15-item gap analysis: Extended pipeline */
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_training_callbacks.h"
/* Note: nimcp_training_checkpoint.h — checkpoint is wired via TCB callback, not direct calls */
#include "middleware/training/nimcp_training_diagnosis.h"
#include "middleware/training/nimcp_regularization.h"
#include "training/nimcp_continual_learning.h"
#include "training/nimcp_adversarial_training.h"
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/training/nimcp_gradient_checkpoint.h"
#include "gpu/context/nimcp_gpu_context.h"
#endif

/* Thread pool for parallel forward/backward passes */
#include "utils/thread/nimcp_thread_pool.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include <stdlib.h>

/* Debug timing: enabled by NIMCP_DEBUG_TIMING=1 environment variable */
static int _utm_debug_timing_checked = 0;
static int _utm_debug_timing = 0;
static inline int utm_debug_timing_enabled(void) {
    if (!_utm_debug_timing_checked) {
        const char* env = getenv("NIMCP_DEBUG_TIMING");
        _utm_debug_timing = (env && env[0] == '1');
        _utm_debug_timing_checked = 1;
    }
    return _utm_debug_timing;
}

/* C1: GPU acceleration — forward declarations gated behind NIMCP_ENABLE_CUDA.
 * These are called only when mgr->gpu_ctx is non-NULL at runtime. */
#ifdef NIMCP_ENABLE_CUDA
extern bool nimcp_gpu_loss_mse(void* gpu_ctx, const float* pred, const float* target,
                                uint32_t n, float* loss);
extern bool nimcp_gpu_optim_adamw(void* gpu_ctx, float* params, float* grads, float* m, float* v,
                                    uint32_t n, float lr, float beta1, float beta2, float eps, float wd,
                                    uint64_t step);
extern bool nimcp_gpu_bridge_gemv(void* gpu_ctx, const float* W, const float* x, const float* bias,
                                   float* y, uint32_t rows, uint32_t cols,
                                   void** p_d_W, void** p_d_x, void** p_d_y,
                                   uint32_t* p_buf_rows, uint32_t* p_buf_cols);
#endif

//=============================================================================
// Default Configuration
//=============================================================================

void nimcp_utm_default_config(nimcp_unified_training_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(nimcp_unified_training_config_t));

    config->optimizer_type = 5; /* NIMCP_OPTIMIZER_ADAM */
    config->learning_rate = 0.01f;
    config->weight_decay = 0.0f;

    config->anti_collapse.diversity_loss_weight = NIMCP_UTM_DEFAULT_DIVERSITY_WEIGHT;
    config->anti_collapse.diversity_buffer_size = NIMCP_UTM_DEFAULT_DIVERSITY_BUFFER;
    config->anti_collapse.use_gradient_normalization = true;
    config->anti_collapse.gradient_target_norm = NIMCP_UTM_DEFAULT_GRADIENT_TARGET;
    config->anti_collapse.gradient_clip_value = 5.0f;
    config->anti_collapse.adaptive_gradient_target = true;

    config->batch_size = 1;
    config->unified_optimizer = false;
    config->loss_type = 0; /* NIMCP_LOSS_MSE */
    config->use_composite_loss = true;
    config->enable_cross_network_gradients = true;   /* Gap 15: default changed to true */

    config->lr_schedule.type = NIMCP_LR_SCHEDULE_COSINE;
    config->lr_schedule.warmup_steps = 1000;
    config->lr_schedule.total_steps = 100000;
    config->lr_schedule.min_lr_ratio = 0.01f;
    config->lr_schedule.step_decay_factor = 0.5f;
    config->lr_schedule.step_decay_interval = 10000;

    /* DFA health monitoring (default: enabled) */
    config->loss_history_size = 256;
    config->health_check_interval = 64;
    config->dfa_auto_adjust_lr = true;
    config->enable_pink_noise_dfa_feedback = true;

    /* Quantum annealing (default: enabled) */
    config->enable_quantum_anneal = true;
    config->plateau_anneal_threshold = 3;

    /* Quantum Shannon bottleneck (default: enabled) */
    config->bottleneck_check_interval = 128;

    /* Natural gradient (default: enabled) */
    config->enable_natural_gradient = true;
    config->fisher_update_interval = 16;
    config->natural_grad_max_params = 4096;

    /* Manifold tracking (default: enabled) */
    config->enable_manifold_tracking = true;

    /* Item 2: Bridge params in AdamW (default: enabled) */
    config->bridge_params_in_optimizer = true;

    /* Item 3: AMP (default: enabled) */
    config->enable_amp = true;

    /* Item 6: Curriculum (default: enabled) */
    config->enable_curriculum = true;
    config->curriculum_num_samples = 1024;

    /* Item 7: KD (default: enabled) */
    config->enable_knowledge_distillation = true;
    config->kd_loss_weight = 0.3f;

    /* Item 8: Neuromod LR (default: enabled) */
    config->enable_neuromod_lr = true;

    /* Item 9: EMA (default: enabled) */
    config->enable_ema = true;
    config->ema_decay = 0.999f;

    /* Item 10: Middleware LR scheduler (default: enabled) */
    config->enable_lr_scheduler = true;

    /* Item 11: Riemannian SGD (default: enabled) */
    config->enable_riemannian_sgd = true;
    config->riemannian_max_params = 2048;

    /* Item 12: Contrastive loss (default: enabled) */
    config->enable_contrastive_loss = true;
    config->contrastive_loss_weight = 0.1f;
    config->contrastive_margin = 1.0f;

    /* Item 13: Per-network LR (default: 0 = use global) */
    /* Already zeroed by memset */

    /* Item 14: Early stopping (default: enabled, but with conservative
     * thresholds for immersive curricula where loss can plateau for
     * thousands of steps without indicating convergence — e.g. Sensory
     * Enrichment phase. Was patience=50 / min_delta=1e-5 — far too
     * aggressive; tripped at step 1500 mid-mode-collapse, locking the
     * Adaptive net at a flat 4725 loss. New defaults are safer; callers
     * who want fast early-stop should override explicitly. */
    config->enable_early_stopping = true;
    config->early_stopping_patience = 5000;
    config->early_stopping_min_delta = 1e-3f;

    /* 15-item gap analysis defaults (all enabled per user preference) */
    config->enable_gradient_manager = true;
    config->enable_middleware_optimizer = true;
    config->enable_training_callbacks = true;
    config->enable_checkpoint_manager = true;
    config->checkpoint_interval = 1000;
    config->enable_training_diagnosis = true;
    config->enable_regularization = true;
    config->l1_lambda = 1e-5f;
    config->label_smoothing = 0.1f;
    config->enable_continual_learning = true;
    config->ewc_lambda = 0.1f;
    config->enable_adversarial_training = false;  /* Requires forward_fn callback — opt-in */
    config->enable_fused_inference = true;
    config->enable_ema_inference_swap = true;
    config->enable_gradient_checkpointing = true;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_unified_training_manager_t* nimcp_utm_create(
    const nimcp_unified_training_config_t* config) {

    nimcp_unified_training_manager_t* mgr = (nimcp_unified_training_manager_t*)
        nimcp_calloc(1, sizeof(nimcp_unified_training_manager_t));
    if (!mgr) {
        NIMCP_LOGGING_ERROR("nimcp_utm_create: Failed to allocate manager");
        return NULL;
    }

    if (config) {
        mgr->config = *config;
    } else {
        nimcp_utm_default_config(&mgr->config);
    }

    mgr->current_lr = mgr->config.learning_rate;

    /* Wire enable_mixed_precision → enable_amp (alias) */
    if (mgr->config.enable_mixed_precision && !mgr->config.enable_amp) {
        mgr->config.enable_amp = true;
    }

    /* Initialize per-network anti-collapse state */
    for (uint32_t i = 0; i < NIMCP_UTM_MAX_NETWORKS; i++) {
        nimcp_anti_collapse_init(&mgr->anti_collapse[i], &mgr->config.anti_collapse);
    }

    /* Initialize AdamW optimizer state */
    mgr->adam_m = NULL;
    mgr->adam_v = NULL;
    mgr->adam_sizes = NULL;
    mgr->adam_num_groups = 0;
    mgr->adam_beta1_t = 1.0f;
    mgr->adam_beta2_t = 1.0f;

    /* Initialize fractal LR multipliers */
    for (uint32_t i = 0; i < NIMCP_UTM_MAX_NETWORKS; i++) {
        mgr->fractal_lr_multiplier[i] = 1.0f;
    }
    mgr->fractal_enabled = false;

    /* Initialize DFA health monitoring */
    mgr->loss_history_size = mgr->config.loss_history_size;
    mgr->health_check_interval = mgr->config.health_check_interval;
    mgr->dfa_auto_adjust_lr = mgr->config.dfa_auto_adjust_lr;
    mgr->pink_noise_dfa_feedback = mgr->config.enable_pink_noise_dfa_feedback;
    if (mgr->loss_history_size > 0) {
        mgr->loss_history = (float*)nimcp_calloc(mgr->loss_history_size, sizeof(float));
    }
    mgr->training_health = NIMCP_TRAINING_HEALTH_UNKNOWN;

    /* Initialize quantum annealing */
    mgr->enable_quantum_anneal = mgr->config.enable_quantum_anneal;
    mgr->plateau_anneal_threshold = mgr->config.plateau_anneal_threshold;
    mgr->plateau_consecutive_count = 0;

    /* Initialize bottleneck detection */
    mgr->bottleneck_check_interval = mgr->config.bottleneck_check_interval;

    /* Initialize natural gradient */
    mgr->natural_gradient_enabled = mgr->config.enable_natural_gradient;
    mgr->fisher_update_interval = (mgr->config.fisher_update_interval > 0) ?
                                    mgr->config.fisher_update_interval : 16;
    mgr->natural_grad_max_params = (mgr->config.natural_grad_max_params > 0) ?
                                    mgr->config.natural_grad_max_params : 4096;
    /* Natural gradient handles are created lazily when param groups are known */

    /* Initialize manifold tracking */
    mgr->manifold_tracking_enabled = mgr->config.enable_manifold_tracking;
    /* Manifold handles are created lazily when output dims are known */

    /* Initialize phase coherence */
    mgr->cross_network_coherence = 0.0f;
    mgr->per_network_loss_history = NULL;

    /* Item 2: Bridge params in optimizer */
    mgr->bridge_params_in_optimizer = mgr->config.bridge_params_in_optimizer;

    /* Item 3: AMP (lazy — set via nimcp_utm_set_amp or auto-created) */
    mgr->amp_ctx = NULL;

    /* Item 6: Curriculum (lazy — set via nimcp_utm_set_curriculum or auto-created) */
    mgr->curriculum_ctx = NULL;

    /* Item 7: KD (lazy — set via nimcp_utm_set_kd) */
    mgr->kd_ctx = NULL;
    mgr->kd_loss_weight = mgr->config.kd_loss_weight;

    /* Item 8: Neuromod (set via nimcp_utm_set_neuromod) */
    mgr->neuromod_adapter = NULL;
    mgr->neuromod_lr_scale = 1.0f;

    /* Item 9: EMA */
    mgr->ema_params = NULL;
    mgr->ema_sizes = NULL;
    mgr->ema_num_groups = 0;
    mgr->ema_decay = mgr->config.ema_decay;
    mgr->ema_enabled = mgr->config.enable_ema;

    /* Item 10: LR scheduler (lazy-created on first use if enabled) */
    mgr->lr_scheduler_ctx = NULL;

    /* Item 11: Riemannian SGD */
    mgr->riemannian_metric = NULL;
    mgr->riemannian_max_params = (mgr->config.riemannian_max_params > 0) ?
                                   mgr->config.riemannian_max_params : 2048;
    mgr->riemannian_enabled = mgr->config.enable_riemannian_sgd;

    /* Item 13: Per-network LR */
    memcpy(mgr->per_network_lr, mgr->config.per_network_lr, sizeof(mgr->per_network_lr));

    /* Item 14: Early stopping */
    mgr->early_stopping_enabled = mgr->config.enable_early_stopping;
    mgr->early_stopping_patience = mgr->config.early_stopping_patience;
    mgr->early_stopping_min_delta = mgr->config.early_stopping_min_delta;
    mgr->early_stopping_best_loss = FLT_MAX;
    mgr->early_stopping_counter = 0;
    mgr->early_stopped = false;

    /* 15-item gap analysis: initialization */
    mgr->gradient_manager = NULL;       /* Lazy-created on first step */
    mgr->middleware_optimizer = NULL;    /* Lazy-created on first step */
    mgr->tcb_ctx = NULL;               /* Set via nimcp_utm_set_callbacks */
    mgr->checkpoint_mgr = NULL;        /* Set via nimcp_utm_set_checkpoint_mgr */
    mgr->diagnoser = NULL;             /* Lazy-created on first step */
    mgr->previous_loss = 0.0f;
    mgr->previous_grad_norm = 0.0f;
    mgr->regularization_ctx = NULL;    /* Lazy-created on first step */
    mgr->bridge_adam_m = NULL;
    mgr->bridge_adam_v = NULL;
    mgr->bridge_adam_sizes = NULL;
    mgr->bridge_adam_num = 0;
    mgr->cl_ctx = NULL;               /* Set via nimcp_utm_set_cl */
    mgr->ewc_lambda = mgr->config.ewc_lambda;
    mgr->adv_ctx = NULL;              /* Set via nimcp_utm_set_adversarial */
    mgr->grad_checkpoint_ctx = NULL;   /* CUDA-gated, lazy-created */
    mgr->ema_swapped_in = false;

    NIMCP_LOGGING_INFO("Unified training manager created (lr=%.4f, diversity_w=%.2f, grad_norm=%s, "
                       "dfa=%s, quantum=%s, nat_grad=%s, manifold=%s)",
                       mgr->current_lr,
                       mgr->config.anti_collapse.diversity_loss_weight,
                       mgr->config.anti_collapse.use_gradient_normalization ? "normalize" : "clip",
                       mgr->loss_history_size > 0 ? "on" : "off",
                       mgr->enable_quantum_anneal ? "on" : "off",
                       mgr->natural_gradient_enabled ? "on" : "off",
                       mgr->manifold_tracking_enabled ? "on" : "off");

    return mgr;
}

void nimcp_utm_destroy(nimcp_unified_training_manager_t* mgr) {
    if (!mgr) return;

    /* Destroy adapters via their vtable destroy() */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (mgr->networks[i].ops && mgr->networks[i].ops->destroy) {
            mgr->networks[i].ops->destroy(mgr->networks[i].ctx);
        }
        mgr->networks[i].ctx = NULL;
        mgr->networks[i].ops = NULL;
    }

    /* Destroy bridges */
    for (uint32_t i = 0; i < mgr->num_bridges; i++) {
        nimcp_cross_network_bridge_t* b = &mgr->bridges[i];
        nimcp_free(b->transform_weights);
        nimcp_free(b->transform_bias);
        nimcp_free(b->weight_grad);
        nimcp_free(b->bias_grad);
        nimcp_free(b->last_source_output);
        nimcp_free(b->last_target_input);
#ifdef NIMCP_ENABLE_CUDA
        /* Free persistent GPU device buffers */
        if (mgr->gpu_ctx && b->gpu_d_W) nimcp_gpu_free((nimcp_gpu_context_t*)mgr->gpu_ctx, b->gpu_d_W);
        if (mgr->gpu_ctx && b->gpu_d_x) nimcp_gpu_free((nimcp_gpu_context_t*)mgr->gpu_ctx, b->gpu_d_x);
        if (mgr->gpu_ctx && b->gpu_d_y) nimcp_gpu_free((nimcp_gpu_context_t*)mgr->gpu_ctx, b->gpu_d_y);
#endif
    }

    /* Destroy per-network anti-collapse state */
    for (uint32_t i = 0; i < NIMCP_UTM_MAX_NETWORKS; i++) {
        nimcp_anti_collapse_destroy(&mgr->anti_collapse[i]);
    }

    /* Free Adam optimizer state */
    if (mgr->adam_m) {
        for (uint32_t i = 0; i < mgr->adam_num_groups; i++) {
            nimcp_free(mgr->adam_m[i]);
            nimcp_free(mgr->adam_v[i]);
        }
        nimcp_free(mgr->adam_m);
        nimcp_free(mgr->adam_v);
        nimcp_free(mgr->adam_sizes);
    }

    /* Free DFA loss history */
    nimcp_free(mgr->loss_history);
    nimcp_free(mgr->per_network_loss_history);

    /* Free EMA shadow params */
    if (mgr->ema_params) {
        for (uint32_t i = 0; i < mgr->ema_num_groups; i++) {
            nimcp_free(mgr->ema_params[i]);
        }
        nimcp_free(mgr->ema_params);
        nimcp_free(mgr->ema_sizes);
    }

    /* Destroy LR scheduler if we created it */
    if (mgr->lr_scheduler_ctx) {
        nimcp_lr_scheduler_destroy((nimcp_lr_scheduler_ctx_t*)mgr->lr_scheduler_ctx);
    }

    /* Destroy Riemannian metric */
    if (mgr->riemannian_metric) {
        riemannian_metric_destroy((riemannian_metric_t*)mgr->riemannian_metric);
    }

    /* Note: AMP, curriculum, KD, neuromod, CL, adv, tcb, checkpoint contexts
     * are NOT owned — caller destroys them */

    /* 15-item gap analysis: cleanup owned resources */
    if (mgr->gradient_manager) {
        nimcp_gradient_manager_destroy(mgr->gradient_manager);
    }
    if (mgr->middleware_optimizer) {
        nimcp_optimizer_destroy(mgr->middleware_optimizer);
    }
    if (mgr->diagnoser) {
        training_diagnoser_destroy(mgr->diagnoser);
    }
    if (mgr->regularization_ctx) {
        nimcp_regularization_destroy(mgr->regularization_ctx);
    }

    /* Free bridge AdamW momentum state */
    if (mgr->bridge_adam_m) {
        for (uint32_t i = 0; i < mgr->bridge_adam_num; i++) {
            nimcp_free(mgr->bridge_adam_m[i]);
            nimcp_free(mgr->bridge_adam_v[i]);
        }
        nimcp_free(mgr->bridge_adam_m);
        nimcp_free(mgr->bridge_adam_v);
        nimcp_free(mgr->bridge_adam_sizes);
    }

#ifdef NIMCP_ENABLE_CUDA
    if (mgr->grad_checkpoint_ctx) {
        nimcp_checkpoint_ctx_destroy(mgr->grad_checkpoint_ctx);
    }
#endif

    /* Destroy natural gradient handles */
    for (uint32_t i = 0; i < NIMCP_UTM_MAX_NETWORKS; i++) {
        if (mgr->natural_grad[i]) {
            nimcp_natural_grad_destroy(mgr->natural_grad[i]);
        }
        if (mgr->fisher[i]) {
            nimcp_fisher_destroy(mgr->fisher[i]);
        }
        if (mgr->output_manifold[i]) {
            nimcp_manifold_destroy(mgr->output_manifold[i]);
        }
    }

    nimcp_free(mgr);
    NIMCP_LOGGING_INFO("Unified training manager destroyed");
}

//=============================================================================
// Network Registration
//=============================================================================

int nimcp_utm_register_network(nimcp_unified_training_manager_t* mgr,
                               const nimcp_trainable_network_ops_t* ops,
                               void* ctx,
                               float loss_weight) {
    if (!mgr || !ops || !ctx) return -1;

    if (mgr->num_networks >= NIMCP_UTM_MAX_NETWORKS) {
        NIMCP_LOGGING_ERROR("nimcp_utm_register_network: Max networks (%d) reached",
                           NIMCP_UTM_MAX_NETWORKS);
        return -1;
    }

    uint32_t idx = mgr->num_networks;
    mgr->networks[idx].ops = ops;
    mgr->networks[idx].ctx = ctx;
    mgr->networks[idx].loss_weight = (loss_weight > 0.0f) ? loss_weight : 1.0f;
    mgr->networks[idx].enabled = true;
    mgr->num_networks++;

    NIMCP_LOGGING_INFO("Registered trainable network '%s' at index %u (weight=%.2f)",
                       ops->name ? ops->name : "unnamed", idx, mgr->networks[idx].loss_weight);

    return (int)idx;
}

int nimcp_utm_set_network_enabled(nimcp_unified_training_manager_t* mgr,
                                  uint32_t network_idx, bool enabled) {
    if (!mgr || network_idx >= mgr->num_networks) return -1;
    mgr->networks[network_idx].enabled = enabled;
    return 0;
}

void nimcp_utm_set_plasticity_bridge(nimcp_unified_training_manager_t* mgr,
                                      tpb_context_t* tpb) {
    if (!mgr) return;
    mgr->plasticity_bridge = tpb;
}

//=============================================================================
// Bridge API
//=============================================================================

int nimcp_utm_add_bridge(nimcp_unified_training_manager_t* mgr,
                         uint32_t source_idx, uint32_t target_idx,
                         nimcp_bridge_type_t type) {
    if (!mgr) return -1;
    if (source_idx >= mgr->num_networks || target_idx >= mgr->num_networks) return -1;
    if (mgr->num_bridges >= NIMCP_UTM_MAX_BRIDGES) return -1;

    uint32_t bidx = mgr->num_bridges;
    nimcp_cross_network_bridge_t* b = &mgr->bridges[bidx];
    memset(b, 0, sizeof(nimcp_cross_network_bridge_t));

    b->source_idx = source_idx;
    b->target_idx = target_idx;
    b->type = type;
    b->enabled = true;

    /* B7: Set configurable bridge parameter defaults */
    b->surrogate_beta = 1.0f;
    b->spike_rate_alpha = 0.3f;
    b->spike_gain = 5.0f;
    b->spike_threshold = 0.5f;

    /* Get dimensions from registered networks */
    nimcp_trainable_network_t* src = &mgr->networks[source_idx];
    nimcp_trainable_network_t* tgt = &mgr->networks[target_idx];

    b->source_dim = src->ops->get_output_dim(src->ctx);
    b->target_dim = tgt->ops->get_input_dim(tgt->ctx);

    if (type == NIMCP_BRIDGE_LINEAR && b->source_dim > 0 && b->target_dim > 0) {
        size_t w_size = (size_t)b->target_dim * b->source_dim;
        b->transform_weights = (float*)nimcp_calloc(w_size, sizeof(float));
        b->weight_grad = (float*)nimcp_calloc(w_size, sizeof(float));
        b->transform_bias = (float*)nimcp_calloc(b->target_dim, sizeof(float));
        b->bias_grad = (float*)nimcp_calloc(b->target_dim, sizeof(float));

        if (!b->transform_weights || !b->weight_grad) {
            NIMCP_LOGGING_ERROR("nimcp_utm_add_bridge: Failed to allocate transform");
            return -1;
        }

        /* Xavier initialization */
        float scale = sqrtf(2.0f / (float)(b->source_dim + b->target_dim));
        for (size_t i = 0; i < w_size; i++) {
            b->transform_weights[i] = scale * ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f;
        }
    }

    b->last_source_output = (float*)nimcp_calloc(b->source_dim, sizeof(float));
    b->last_target_input = (float*)nimcp_calloc(b->target_dim, sizeof(float));

    mgr->num_bridges++;

    NIMCP_LOGGING_INFO("Added %s bridge: network[%u] (%ux) -> network[%u] (%ux)",
                       type == NIMCP_BRIDGE_LINEAR ? "linear" :
                       type == NIMCP_BRIDGE_IDENTITY ? "identity" : "encoded",
                       source_idx, b->source_dim, target_idx, b->target_dim);

    return (int)bidx;
}

//=============================================================================
// Parallel Forward/Backward Task Contexts
//=============================================================================

/** @brief Context for a parallel forward pass task (one per network) */
typedef struct {
    nimcp_trainable_network_t* net;
    const float* input;
    uint32_t input_dim;
    float* output;
    uint32_t output_dim;
    int rc;  /**< Return code from forward() */
} utm_forward_task_t;

/** @brief Thread pool task: run a single network's forward pass */
static void utm_forward_task_fn(void* arg) {
    utm_forward_task_t* t = (utm_forward_task_t*)arg;
    t->rc = t->net->ops->forward(t->net->ctx, t->input, t->input_dim,
                                  t->output, t->output_dim);
}

/** @brief Context for a parallel backward pass task (one per network) */
typedef struct {
    nimcp_trainable_network_t* net;
    nimcp_anti_collapse_state_t* anti_collapse;
    const float* net_output;
    const float* target;
    uint32_t out_dim;
    uint32_t target_dim;
    uint32_t loss_type;
    float* dl_din;        /**< Output: input gradient (allocated by caller) */
    uint32_t in_dim;
    float div_loss;       /**< Output: diversity loss for this network */
    float* dl_dout_override; /**< If non-NULL, use this instead of computing loss gradient */
} utm_backward_task_t;

/** @brief Thread pool task: compute loss gradient + diversity + backward for one network */
static void utm_backward_task_fn(void* arg) {
    utm_backward_task_t* t = (utm_backward_task_t*)arg;

    float* dl_dout;
    bool owns_dl_dout;

    if (t->dl_dout_override) {
        /* Pre-computed gradient from Phase 2a (bridged parallel path) */
        dl_dout = t->dl_dout_override;
        owns_dl_dout = false;
    } else {
        /* Compute dL/dOutput from scratch (Phase 1 non-bridged path) */
        dl_dout = (float*)nimcp_calloc(t->out_dim, sizeof(float));
        if (!dl_dout) { t->div_loss = 0.0f; return; }
        owns_dl_dout = true;

        uint32_t cmp_dim = (t->out_dim < t->target_dim) ? t->out_dim : t->target_dim;
        float n_cmp = (float)(cmp_dim > 0 ? cmp_dim : 1);
        switch (t->loss_type) {
            case 1:
                for (uint32_t j = 0; j < cmp_dim; j++) {
                    float diff = t->net_output[j] - t->target[j];
                    dl_dout[j] = (diff > 0.0f ? 1.0f : (diff < 0.0f ? -1.0f : 0.0f)) / n_cmp;
                }
                break;
            case 2:
                for (uint32_t j = 0; j < cmp_dim; j++) {
                    float o = t->net_output[j];
                    if (o < 1e-7f) o = 1e-7f;
                    dl_dout[j] = -t->target[j] / o / n_cmp;
                }
                break;
            default:
                for (uint32_t j = 0; j < cmp_dim; j++) {
                    dl_dout[j] = 2.0f * (t->net_output[j] - t->target[j]) / n_cmp;
                }
                break;
        }

        /* Diversity loss gradient (only when computing from scratch) */
        if (t->anti_collapse) {
            t->div_loss = nimcp_anti_collapse_diversity_loss(
                t->anti_collapse, t->net_output, dl_dout, t->out_dim);
        }
    }

    /* Backward through network (the expensive part — runs in parallel) */
    struct timespec _bwd_t0, _bwd_t1;
    if (utm_debug_timing_enabled()) clock_gettime(CLOCK_MONOTONIC, &_bwd_t0);
    t->net->ops->backward(t->net->ctx, dl_dout, t->out_dim, t->dl_din, t->in_dim);
    if (utm_debug_timing_enabled()) {
        clock_gettime(CLOCK_MONOTONIC, &_bwd_t1);
        double _bwd_ms = (_bwd_t1.tv_sec - _bwd_t0.tv_sec) * 1000.0
                       + (_bwd_t1.tv_nsec - _bwd_t0.tv_nsec) / 1e6;
        if (_bwd_ms > 100.0) {
            NIMCP_LOGGING_INFO("UTM network backward '%s': %.0fms (in=%u out=%u)",
                               t->net->ops->name, _bwd_ms, t->in_dim, t->out_dim);
        }
    }

    if (owns_dl_dout) nimcp_free(dl_dout);
}

//=============================================================================
// Bridge Forward/Backward (internal)
//=============================================================================

static int bridge_forward(nimcp_cross_network_bridge_t* b,
                          const float* source_output,
                          float* target_input,
                          void* gpu_ctx) {
    if (!b || !source_output || !target_input) return -1;
    (void)gpu_ctx; /* Used only when NIMCP_ENABLE_CUDA is defined */

    /* Cache source output for backward */
    memcpy(b->last_source_output, source_output, b->source_dim * sizeof(float));

    switch (b->type) {
        case NIMCP_BRIDGE_IDENTITY:
            /* Direct copy (dims must match) */
            {
                uint32_t copy_dim = (b->source_dim < b->target_dim) ?
                                     b->source_dim : b->target_dim;
                memcpy(target_input, source_output, copy_dim * sizeof(float));
                /* Zero-pad if target is larger */
                for (uint32_t i = copy_dim; i < b->target_dim; i++) {
                    target_input[i] = 0.0f;
                }
            }
            break;

        case NIMCP_BRIDGE_LINEAR:
            if (!b->transform_weights) return -1; /* B8: null guard */
            /* target = W @ source + bias */
#ifdef NIMCP_ENABLE_CUDA
            if (gpu_ctx && b->source_dim >= 64) {
                /* GPU path: cuBLAS gemv for large bridges */
                if (nimcp_gpu_bridge_gemv(gpu_ctx, b->transform_weights,
                                          source_output, b->transform_bias,
                                          target_input, b->target_dim, b->source_dim,
                                          &b->gpu_d_W, &b->gpu_d_x, &b->gpu_d_y,
                                          &b->gpu_buf_rows, &b->gpu_buf_cols)) {
                    break; /* GPU succeeded */
                }
                /* Fall through to CPU if GPU fails */
            }
#endif
            /* CPU fallback */
            for (uint32_t t = 0; t < b->target_dim; t++) {
                float sum = b->transform_bias ? b->transform_bias[t] : 0.0f;
                for (uint32_t s = 0; s < b->source_dim; s++) {
                    sum += b->transform_weights[t * b->source_dim + s] * source_output[s];
                }
                target_input[t] = sum;
            }
            break;

        case NIMCP_BRIDGE_RATE_TO_SPIKE:
            bridge_rate_to_spike_forward(b, source_output, target_input);
            break;

        case NIMCP_BRIDGE_SPIKE_TO_RATE:
            bridge_spike_to_rate_forward(b, source_output, target_input);
            break;

        case NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE:
            bridge_continuous_to_spike_forward(b, source_output, target_input);
            break;
    }

    /* Cache transformed output */
    memcpy(b->last_target_input, target_input, b->target_dim * sizeof(float));
    return 0;
}

static int bridge_backward(nimcp_cross_network_bridge_t* b,
                            const float* dl_dtarget_input,
                            float* dl_dsource_output) {
    if (!b || !dl_dtarget_input) return -1;

    switch (b->type) {
        case NIMCP_BRIDGE_IDENTITY:
            if (dl_dsource_output) {
                uint32_t copy_dim = (b->source_dim < b->target_dim) ?
                                     b->source_dim : b->target_dim;
                memcpy(dl_dsource_output, dl_dtarget_input, copy_dim * sizeof(float));
            }
            break;

        case NIMCP_BRIDGE_LINEAR:
            /* Gradient w.r.t. weights: dL/dW = dL/dtarget @ source.T */
            if (b->weight_grad) {
                for (uint32_t t = 0; t < b->target_dim; t++) {
                    for (uint32_t s = 0; s < b->source_dim; s++) {
                        b->weight_grad[t * b->source_dim + s] +=
                            dl_dtarget_input[t] * b->last_source_output[s];
                    }
                }
            }
            /* Gradient w.r.t. bias */
            if (b->bias_grad) {
                for (uint32_t t = 0; t < b->target_dim; t++) {
                    b->bias_grad[t] += dl_dtarget_input[t];
                }
            }
            /* Gradient w.r.t. source output: dL/dsource = W.T @ dL/dtarget */
            if (dl_dsource_output) {
                for (uint32_t s = 0; s < b->source_dim; s++) {
                    float sum = 0.0f;
                    for (uint32_t t = 0; t < b->target_dim; t++) {
                        sum += b->transform_weights[t * b->source_dim + s] * dl_dtarget_input[t];
                    }
                    dl_dsource_output[s] = sum;
                }
            }
            break;

        case NIMCP_BRIDGE_RATE_TO_SPIKE:
            bridge_rate_to_spike_backward(b, dl_dtarget_input, dl_dsource_output);
            break;

        case NIMCP_BRIDGE_SPIKE_TO_RATE:
            bridge_spike_to_rate_backward(b, dl_dtarget_input, dl_dsource_output);
            break;

        case NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE:
            bridge_continuous_to_spike_backward(b, dl_dtarget_input, dl_dsource_output);
            break;
    }

    return 0;
}

//=============================================================================
// Learning Rate Schedule
//=============================================================================

float nimcp_utm_get_scheduled_lr(const nimcp_unified_training_manager_t* mgr) {
    if (!mgr) return 0.01f;

    float base_lr = mgr->config.learning_rate;
    const nimcp_lr_schedule_config_t* sched = &mgr->config.lr_schedule;
    uint64_t step = mgr->step_count;

    /* Linear warmup */
    if (sched->warmup_steps > 0 && step < sched->warmup_steps) {
        float warmup_ratio = (float)(step + 1) / (float)sched->warmup_steps;
        return base_lr * warmup_ratio;
    }

    float min_lr = base_lr * sched->min_lr_ratio;

    switch (sched->type) {
        case NIMCP_LR_SCHEDULE_COSINE: {
            if (sched->total_steps <= sched->warmup_steps) return base_lr;
            uint64_t decay_steps = sched->total_steps - sched->warmup_steps;
            uint64_t decay_step = step - sched->warmup_steps;
            if (decay_step >= decay_steps) return min_lr;
            float progress = (float)decay_step / (float)decay_steps;
            /* Cosine annealing: lr = min_lr + 0.5 * (base_lr - min_lr) * (1 + cos(pi * progress)) */
            return min_lr + 0.5f * (base_lr - min_lr) * (1.0f + cosf((float)M_PI * progress));
        }
        case NIMCP_LR_SCHEDULE_STEP: {
            if (sched->step_decay_interval == 0) return base_lr;
            uint64_t num_decays = step / sched->step_decay_interval;
            float lr = base_lr;
            for (uint64_t d = 0; d < num_decays && d < 20; d++) {
                lr *= sched->step_decay_factor;
            }
            return fmaxf(lr, min_lr);
        }
        default:
            return base_lr;
    }
}

//=============================================================================
// Forward-Only Inference (no backward/optimizer)
//=============================================================================

int nimcp_utm_forward_only(nimcp_unified_training_manager_t* mgr,
                           const float* input, uint32_t input_dim,
                           float* output, uint32_t output_dim) {
    if (!mgr || !input || !output) return -1;
    if (mgr->num_networks == 0) return -1;

    /* Allocate per-network output buffers */
    float* net_outputs[NIMCP_UTM_MAX_NETWORKS];
    uint32_t net_output_dims[NIMCP_UTM_MAX_NETWORKS];
    memset(net_outputs, 0, sizeof(net_outputs));
    memset(net_output_dims, 0, sizeof(net_output_dims));

    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        nimcp_trainable_network_t* net = &mgr->networks[i];
        uint32_t out_dim = net->ops->get_output_dim(net->ctx);
        net_output_dims[i] = out_dim;
        net_outputs[i] = (float*)nimcp_calloc(out_dim, sizeof(float));
        if (!net_outputs[i]) continue;

        net->ops->forward(net->ctx, input, input_dim, net_outputs[i], out_dim);
    }

    /* Blend outputs (weighted average, same as training ensemble) */
    memset(output, 0, output_dim * sizeof(float));
    float total_weight = 0.0f;
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
        float w = mgr->networks[i].loss_weight;
        uint32_t blend_dim = (net_output_dims[i] < output_dim) ?
                              net_output_dims[i] : output_dim;
        for (uint32_t j = 0; j < blend_dim; j++) {
            output[j] += w * net_outputs[i][j];
        }
        total_weight += w;
    }
    if (total_weight > 0.0f && total_weight != 1.0f) {
        for (uint32_t j = 0; j < output_dim; j++) {
            output[j] /= total_weight;
        }
    }

    /* Cleanup */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        nimcp_free(net_outputs[i]);
    }
    return 0;
}

//=============================================================================
// Unified Training Step
//=============================================================================

int nimcp_utm_step(nimcp_unified_training_manager_t* mgr,
                   const float* input, uint32_t input_dim,
                   const float* target, uint32_t target_dim,
                   nimcp_utm_step_result_t* result) {
    if (!mgr || !input || !target) return -1;
    if (mgr->num_networks == 0) return -1;

    /* Item 14: Early stopping — skip step if already stopped */
    if (mgr->early_stopped) {
        if (result) {
            memset(result, 0, sizeof(nimcp_utm_step_result_t));
            result->early_stopped = true;
        }
        return 0;
    }

    nimcp_utm_step_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* ------------------------------------------------------------------ */
    /* Step 1: Zero all gradients (only on first sample of batch)         */
    /* ------------------------------------------------------------------ */
    uint32_t effective_batch = (mgr->config.batch_size > 0) ? mgr->config.batch_size : 1;
    bool is_first_in_batch = (mgr->batch_accumulation_count == 0);

    if (is_first_in_batch) {
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            const nimcp_trainable_network_ops_t* ops = mgr->networks[i].ops;
            if (ops->zero_grad) {
                ops->zero_grad(mgr->networks[i].ctx);
            }
        }

        /* Zero bridge gradients */
        for (uint32_t i = 0; i < mgr->num_bridges; i++) {
            nimcp_cross_network_bridge_t* b = &mgr->bridges[i];
            if (b->weight_grad) memset(b->weight_grad, 0,
                (size_t)b->target_dim * b->source_dim * sizeof(float));
            if (b->bias_grad) memset(b->bias_grad, 0,
                b->target_dim * sizeof(float));
        }
    }

    /* ------------------------------------------------------------------ */
    /* Gap 1: AMP autocast enter (before forward pass)                    */
    /* ------------------------------------------------------------------ */
    if (mgr->config.enable_amp && mgr->amp_ctx) {
        amp_autocast_enter((amp_ctx_t*)mgr->amp_ctx);
    }

    /* ------------------------------------------------------------------ */
    /* Step 2: Forward pass through networks (topology order)             */
    /* ------------------------------------------------------------------ */

    /* Gradient checkpointing: begin forward pass (CUDA-gated) */
#ifdef NIMCP_ENABLE_CUDA
    if (mgr->config.enable_gradient_checkpointing && mgr->gpu_ctx) {
        /* Lazy-create checkpoint context */
        if (!mgr->grad_checkpoint_ctx) {
            mgr->grad_checkpoint_ctx = nimcp_checkpoint_ctx_create(
                (nimcp_gpu_context_t*)mgr->gpu_ctx,
                CKPT_STRATEGY_SQRT,
                (int)mgr->num_networks, 0);
        }
        if (mgr->grad_checkpoint_ctx) {
            nimcp_checkpoint_begin_forward(mgr->grad_checkpoint_ctx);
        }
    }
#endif

    /* Allocate per-network output buffers */
    float* net_outputs[NIMCP_UTM_MAX_NETWORKS];
    uint32_t net_output_dims[NIMCP_UTM_MAX_NETWORKS];
    memset(net_outputs, 0, sizeof(net_outputs));
    memset(net_output_dims, 0, sizeof(net_output_dims));

    /* Classify networks: has_incoming_bridge[i] = true if any enabled bridge
     * targets network i. Non-bridged networks can run their forward pass in
     * parallel (Phase 1); bridged networks run sequentially after their source
     * networks complete (Phase 2). */
    bool has_incoming_bridge[NIMCP_UTM_MAX_NETWORKS];
    memset(has_incoming_bridge, 0, sizeof(has_incoming_bridge));
    for (uint32_t b = 0; b < mgr->num_bridges; b++) {
        if (mgr->bridges[b].enabled) {
            has_incoming_bridge[mgr->bridges[b].target_idx] = true;
        }
    }

    /* Pre-allocate output buffers and prepare input adaptation for all networks */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        uint32_t out_dim = mgr->networks[i].ops->get_output_dim(mgr->networks[i].ctx);
        if (out_dim == 0) {
            NIMCP_LOGGING_WARN("utm_step: Network '%s' has output_dim=0, skipping",
                              mgr->networks[i].ops->name);
            continue;
        }
        net_output_dims[i] = out_dim;
        net_outputs[i] = (float*)nimcp_calloc(out_dim, sizeof(float));
        if (!net_outputs[i]) goto cleanup;
    }

    /* --- Phase 1: Non-bridged networks run in parallel --- */
    {
        utm_forward_task_t fwd_tasks[NIMCP_UTM_MAX_NETWORKS];
        uint32_t parallel_count = 0;
        /* Temporary per-network adapted inputs for parallel networks */
        float* parallel_inputs[NIMCP_UTM_MAX_NETWORKS];
        memset(parallel_inputs, 0, sizeof(parallel_inputs));

        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
            if (has_incoming_bridge[i]) continue; /* Phase 2 */

            nimcp_trainable_network_t* net = &mgr->networks[i];
            uint32_t out_dim = net_output_dims[i];

            /* Input adaptation (average-pool if needed) */
            const float* net_input = input;
            uint32_t net_input_dim = input_dim;
            uint32_t net_in_dim = net->ops->get_input_dim(net->ctx);
            if (net_in_dim > 0 && net_input_dim > net_in_dim) {
                float* adapted = (float*)nimcp_calloc(net_in_dim, sizeof(float));
                if (adapted) {
                    float ratio = (float)net_input_dim / (float)net_in_dim;
                    for (uint32_t j = 0; j < net_in_dim; j++) {
                        uint32_t start = (uint32_t)(j * ratio);
                        uint32_t end = (uint32_t)((j + 1) * ratio);
                        if (end > net_input_dim) end = net_input_dim;
                        if (end <= start) end = start + 1;
                        float sum = 0.0f;
                        for (uint32_t k = start; k < end; k++) {
                            sum += net_input[k];
                        }
                        adapted[j] = sum / (float)(end - start);
                    }
                    parallel_inputs[i] = adapted;
                    net_input = adapted;
                    net_input_dim = net_in_dim;
                }
            }

            utm_forward_task_t* t = &fwd_tasks[parallel_count];
            t->net = net;
            t->input = net_input;
            t->input_dim = net_input_dim;
            t->output = net_outputs[i];
            t->output_dim = out_dim;
            t->rc = 0;
            parallel_count++;
        }

        if (parallel_count >= 2) {
            /* Submit to thread pool for true parallelism */
            nimcp_thread_pool_t* pool = nimcp_pool_create(parallel_count);
            if (pool) {
                for (uint32_t p = 0; p < parallel_count; p++) {
                    nimcp_pool_submit(pool, utm_forward_task_fn, &fwd_tasks[p]);
                }
                nimcp_pool_wait(pool);
                nimcp_pool_destroy(pool);
            } else {
                /* Fallback: run sequentially if pool creation fails */
                for (uint32_t p = 0; p < parallel_count; p++) {
                    utm_forward_task_fn(&fwd_tasks[p]);
                }
            }
        } else {
            /* Single network or none — run directly, no pool overhead */
            for (uint32_t p = 0; p < parallel_count; p++) {
                utm_forward_task_fn(&fwd_tasks[p]);
            }
        }

        /* Check results and log warnings */
        for (uint32_t p = 0; p < parallel_count; p++) {
            if (fwd_tasks[p].rc != 0) {
                NIMCP_LOGGING_WARN("utm_step: Forward failed for network '%s' (rc=%d)",
                                  fwd_tasks[p].net->ops->name, fwd_tasks[p].rc);
            }
        }

        /* Free adapted inputs */
        for (uint32_t i = 0; i < NIMCP_UTM_MAX_NETWORKS; i++) {
            nimcp_free(parallel_inputs[i]);
        }
    }

    /* --- Phase 2: Bridged networks run sequentially (topology order) --- */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
        if (!has_incoming_bridge[i]) continue; /* Already done in Phase 1 */

        nimcp_trainable_network_t* net = &mgr->networks[i];
        uint32_t out_dim = net_output_dims[i];

        /* Determine input: from a bridge source */
        const float* net_input = input;
        uint32_t net_input_dim = input_dim;

        for (uint32_t b = 0; b < mgr->num_bridges; b++) {
            if (mgr->bridges[b].target_idx == i && mgr->bridges[b].enabled) {
                uint32_t src = mgr->bridges[b].source_idx;
                if (net_outputs[src]) {
                    uint32_t bridged_dim = net->ops->get_input_dim(net->ctx);
                    float* bridged_input = (float*)nimcp_calloc(bridged_dim, sizeof(float));
                    if (bridged_input) {
                        bridge_forward(&mgr->bridges[b], net_outputs[src], bridged_input, mgr->gpu_ctx);
                        net_input = bridged_input;
                        net_input_dim = bridged_dim;
                        break; /* A1: Use first bridge only */
                    }
                }
            }
        }

        /* Adapt input dimensions: pool down or zero-pad up */
        uint32_t net_in_dim = net->ops->get_input_dim(net->ctx);
        float* adapted_input = NULL;
        if (net_in_dim > 0 && net_input_dim > net_in_dim) {
            /* Average-pool down */
            adapted_input = (float*)nimcp_calloc(net_in_dim, sizeof(float));
            if (adapted_input) {
                float ratio = (float)net_input_dim / (float)net_in_dim;
                for (uint32_t j = 0; j < net_in_dim; j++) {
                    uint32_t start = (uint32_t)(j * ratio);
                    uint32_t end = (uint32_t)((j + 1) * ratio);
                    if (end > net_input_dim) end = net_input_dim;
                    if (end <= start) end = start + 1;
                    float sum = 0.0f;
                    for (uint32_t k = start; k < end; k++) {
                        sum += net_input[k];
                    }
                    adapted_input[j] = sum / (float)(end - start);
                }
                if (net_input != input) {
                    nimcp_free((void*)net_input);
                }
                net_input = adapted_input;
                net_input_dim = net_in_dim;
            }
        } else if (net_in_dim > 0 && net_input_dim < net_in_dim) {
            /* Zero-pad up */
            adapted_input = (float*)nimcp_calloc(net_in_dim, sizeof(float));
            if (adapted_input) {
                memcpy(adapted_input, net_input, net_input_dim * sizeof(float));
                if (net_input != input) {
                    nimcp_free((void*)net_input);
                }
                net_input = adapted_input;
                net_input_dim = net_in_dim;
            }
        }

        int rc = net->ops->forward(net->ctx, net_input, net_input_dim,
                                   net_outputs[i], out_dim);

        if (net_input != input) {
            nimcp_free((void*)net_input);
        }

        if (rc != 0) {
            NIMCP_LOGGING_WARN("utm_step: Forward failed for network '%s' (rc=%d)",
                              net->ops->name, rc);
        }
    }

    /* Gap 1: AMP autocast exit (after forward) */
    if (mgr->config.enable_amp && mgr->amp_ctx) {
        amp_autocast_exit((amp_ctx_t*)mgr->amp_ctx);
    }

    /* Gradient checkpointing: end forward pass */
#ifdef NIMCP_ENABLE_CUDA
    if (mgr->grad_checkpoint_ctx) {
        nimcp_checkpoint_end_forward(mgr->grad_checkpoint_ctx);
    }
#endif

    struct timespec _utm_fwd_end; clock_gettime(CLOCK_MONOTONIC, &_utm_fwd_end);

    /* ------------------------------------------------------------------ */
    /* Step 3: Compute composite loss                                     */
    /* ------------------------------------------------------------------ */
    float composite_loss = 0.0f;
    float total_weight = 0.0f;

    /* Per-network loss against target (B2: MSE/MAE/cross-entropy) */
    uint32_t loss_type = mgr->config.loss_type;
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
        nimcp_trainable_network_t* net = &mgr->networks[i];

        uint32_t cmp_dim = (net_output_dims[i] < target_dim) ?
                            net_output_dims[i] : target_dim;
        float loss_val = 0.0f;
        float n = (float)(cmp_dim > 0 ? cmp_dim : 1);
        switch (loss_type) {
            case 1: /* MAE: Σ|out-tgt|/n */
                for (uint32_t j = 0; j < cmp_dim; j++) {
                    loss_val += fabsf(net_outputs[i][j] - target[j]);
                }
                loss_val /= n;
                break;
            case 2: /* Cross-entropy: -Σ tgt*log(max(out,ε))/n */
                for (uint32_t j = 0; j < cmp_dim; j++) {
                    float o = net_outputs[i][j];
                    if (o < 1e-7f) o = 1e-7f;
                    loss_val -= target[j] * logf(o);
                }
                loss_val /= n;
                break;
            default: /* MSE: Σ(out-tgt)²/n */
#ifdef NIMCP_ENABLE_CUDA
                if (mgr->gpu_ctx && cmp_dim >= 256) {
                    /* GPU-accelerated MSE for large dimensions */
                    if (!nimcp_gpu_loss_mse(mgr->gpu_ctx, net_outputs[i], target,
                                            cmp_dim, &loss_val)) {
                        /* GPU failed — fall through to CPU */
                        goto cpu_mse;
                    }
                } else
#endif
                {
#ifdef NIMCP_ENABLE_CUDA
                    cpu_mse:
#endif
                    for (uint32_t j = 0; j < cmp_dim; j++) {
                        float diff = net_outputs[i][j] - target[j];
                        loss_val += diff * diff;
                    }
                    loss_val /= n;
                }
                break;
        }

        /* Auxiliary loss from the network (e.g., spike regularization) */
        float aux = 0.0f;
        if (net->ops->compute_auxiliary_loss) {
            aux = net->ops->compute_auxiliary_loss(net->ctx);
        }

        float net_loss = loss_val + aux;
        /* Guard against NaN/inf from individual networks (e.g. gradient explosion) */
        if (!isfinite(net_loss) || net_loss < 0.0f) {
            net_loss = 0.0f;
        }
        local_result.per_network_loss[i] = net_loss;
        composite_loss += net_loss * net->loss_weight;
        total_weight += net->loss_weight;
    }

    if (total_weight > 0.0f) {
        composite_loss /= total_weight;
    }

    /* Item 12: Cross-network contrastive loss — penalizes mode collapse between networks */
    if (mgr->config.enable_contrastive_loss && mgr->num_networks >= 2) {
        float contrastive = 0.0f;
        uint32_t pair_count = 0;
        for (uint32_t a = 0; a < mgr->num_networks; a++) {
            if (!mgr->networks[a].enabled || !net_outputs[a]) continue;
            for (uint32_t b_idx = a + 1; b_idx < mgr->num_networks; b_idx++) {
                if (!mgr->networks[b_idx].enabled || !net_outputs[b_idx]) continue;
                /* Compute L2 distance between network outputs */
                uint32_t min_dim = (net_output_dims[a] < net_output_dims[b_idx]) ?
                    net_output_dims[a] : net_output_dims[b_idx];
                float dist_sq = 0.0f;
                for (uint32_t j = 0; j < min_dim; j++) {
                    float d = net_outputs[a][j] - net_outputs[b_idx][j];
                    dist_sq += d * d;
                }
                float dist = sqrtf(dist_sq);
                /* Contrastive: penalize if distance < margin (networks too similar) */
                float margin = mgr->config.contrastive_margin;
                if (dist < margin) {
                    contrastive += (margin - dist) * (margin - dist);
                }
                pair_count++;
            }
        }
        if (pair_count > 0) {
            contrastive /= (float)pair_count;
            composite_loss += mgr->config.contrastive_loss_weight * contrastive;
            local_result.contrastive_loss = contrastive;
        }
    }

    /* Knowledge distillation loss — soft targets from teacher */
    if (mgr->config.enable_knowledge_distillation && mgr->kd_ctx && input) {
        /* Use first network's output as student logits for KD */
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
            uint32_t out_dim = net_output_dims[i];
            if (out_dim == 0) continue;

            uint32_t s_dims[1] = { out_dim };
            nimcp_tensor_t* student_t = nimcp_tensor_create(s_dims, 1, NIMCP_DTYPE_F32);
            uint32_t i_dims[1] = { input_dim };
            nimcp_tensor_t* input_t = nimcp_tensor_create(i_dims, 1, NIMCP_DTYPE_F32);
            if (student_t && input_t) {
                memcpy(nimcp_tensor_data(student_t), net_outputs[i], out_dim * sizeof(float));
                memcpy(nimcp_tensor_data(input_t), input, input_dim * sizeof(float));
                float kd_loss = 0.0f;
                if (kd_compute_loss((kd_ctx_t*)mgr->kd_ctx, student_t, input_t,
                                    NULL, &kd_loss) == 0) {
                    composite_loss += mgr->kd_loss_weight * kd_loss;
                }
            }
            nimcp_tensor_destroy(student_t);
            nimcp_tensor_destroy(input_t);
            break; /* KD on first active network only */
        }
    }

    /* Gap 9: Continual learning — EWC penalty to prevent catastrophic forgetting */
    if (mgr->config.enable_continual_learning && mgr->cl_ctx) {
        /* Collect all params into a flat buffer for EWC */
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (mgr->networks[i].ops->get_param_groups &&
                mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups; g++) {
                    if (groups[g].params && groups[g].count > 0) {
                        float penalty = cl_ewc_penalty(mgr->cl_ctx,
                            groups[g].params, groups[g].count);
                        composite_loss += mgr->ewc_lambda * penalty;
                        local_result.ewc_penalty += mgr->ewc_lambda * penalty;
                    }
                }
                nimcp_free(groups);
            }
        }
    }

    local_result.composite_loss = composite_loss;

    /* ------------------------------------------------------------------ */
    /* Step 4: Backward pass (reverse order, with bridge gradient flow)   */
    /* ------------------------------------------------------------------ */

    /* Gradient checkpointing: begin backward pass */
#ifdef NIMCP_ENABLE_CUDA
    if (mgr->grad_checkpoint_ctx) {
        nimcp_checkpoint_begin_backward(mgr->grad_checkpoint_ctx);
    }
#endif

    /* Phase 5: Suppress biological plasticity during backprop */
    if (mgr->plasticity_bridge) {
        nimcp_tpb_set_backprop_active(mgr->plasticity_bridge, true);
    }

    /* Store per-network input gradients for cross-network gradient flow.
     * Backward runs in reverse order so downstream networks compute their
     * dl_dinput before upstream networks need it for bridge backward. */
    float* dl_dinputs[NIMCP_UTM_MAX_NETWORKS];
    memset(dl_dinputs, 0, sizeof(dl_dinputs));

    /* Classify networks for backward parallelism: networks that participate
     * in any bridge (as source OR target) must run sequentially in reverse
     * topology order for correct gradient flow. Non-bridged networks have no
     * cross-network gradient dependencies and can run in parallel. */
    bool in_bridge[NIMCP_UTM_MAX_NETWORKS];
    memset(in_bridge, 0, sizeof(in_bridge));
    for (uint32_t b = 0; b < mgr->num_bridges; b++) {
        if (mgr->bridges[b].enabled) {
            in_bridge[mgr->bridges[b].source_idx] = true;
            in_bridge[mgr->bridges[b].target_idx] = true;
        }
    }

    /* --- Backward Phase 1: Non-bridged networks in parallel --- */
    {
        utm_backward_task_t bwd_tasks[NIMCP_UTM_MAX_NETWORKS];
        uint32_t bwd_indices[NIMCP_UTM_MAX_NETWORKS]; /* Map task slot -> network index */
        uint32_t parallel_count = 0;

        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
            if (in_bridge[i]) continue; /* Phase 2 */

            nimcp_trainable_network_t* net = &mgr->networks[i];
            uint32_t out_dim = net_output_dims[i];
            uint32_t in_dim = net->ops->get_input_dim(net->ctx);
            float* dl_din = (float*)nimcp_calloc(in_dim, sizeof(float));
            if (!dl_din) continue;

            utm_backward_task_t* t = &bwd_tasks[parallel_count];
            t->net = net;
            t->anti_collapse = &mgr->anti_collapse[i];
            t->net_output = net_outputs[i];
            t->target = target;
            t->out_dim = out_dim;
            t->target_dim = target_dim;
            t->loss_type = loss_type;
            t->dl_din = dl_din;
            t->in_dim = in_dim;
            t->div_loss = 0.0f;
            t->dl_dout_override = NULL;

            bwd_indices[parallel_count] = i;
            parallel_count++;
        }

        {
            struct timespec _p1_t0;
            if (utm_debug_timing_enabled()) {
                clock_gettime(CLOCK_MONOTONIC, &_p1_t0);
                NIMCP_LOGGING_INFO("UTM Phase 1 backward: %u non-bridged networks", parallel_count);
            }

            if (parallel_count >= 2) {
                nimcp_thread_pool_t* pool = nimcp_pool_create(parallel_count);
                if (pool) {
                    for (uint32_t p = 0; p < parallel_count; p++) {
                        nimcp_pool_submit(pool, utm_backward_task_fn, &bwd_tasks[p]);
                    }
                    nimcp_pool_wait(pool);
                    nimcp_pool_destroy(pool);
                } else {
                    for (uint32_t p = 0; p < parallel_count; p++) {
                        utm_backward_task_fn(&bwd_tasks[p]);
                    }
                }
            } else {
            for (uint32_t p = 0; p < parallel_count; p++) {
                utm_backward_task_fn(&bwd_tasks[p]);
            }
        }

        /* Collect results */
        for (uint32_t p = 0; p < parallel_count; p++) {
            uint32_t idx = bwd_indices[p];
            dl_dinputs[idx] = bwd_tasks[p].dl_din;
            local_result.diversity_loss += bwd_tasks[p].div_loss;
        }

            if (utm_debug_timing_enabled()) {
                struct timespec _p1_t1; clock_gettime(CLOCK_MONOTONIC, &_p1_t1);
                double _p1_ms = (_p1_t1.tv_sec - _p1_t0.tv_sec) * 1000.0
                              + (_p1_t1.tv_nsec - _p1_t0.tv_nsec) / 1e6;
                NIMCP_LOGGING_INFO("UTM Phase 1 backward: %.0fms for %u networks", _p1_ms, parallel_count);
            }
        }
    }

    /* --- Backward Phase 2: Bridged networks ---
     * Split into two sub-phases for parallelism:
     * 2a: Compute dl_dout (loss + bridge gradients) sequentially (fast, ~ms)
     * 2b: Run backward() calls in parallel (expensive, ~2s each, independent) */
    {
        /* Sub-phase 2a: Prepare dl_dout for each bridged network (sequential) */
        float* bridged_dl_dout[NIMCP_UTM_MAX_NETWORKS];
        memset(bridged_dl_dout, 0, sizeof(bridged_dl_dout));

        for (int i = (int)mgr->num_networks - 1; i >= 0; i--) {
            if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
            if (!in_bridge[i]) continue;

            uint32_t out_dim = net_output_dims[i];
            float* dl_dout = (float*)nimcp_calloc(out_dim, sizeof(float));
            if (!dl_dout) continue;

            uint32_t cmp_dim = (out_dim < target_dim) ? out_dim : target_dim;
            float n_cmp = (float)(cmp_dim > 0 ? cmp_dim : 1);
            switch (loss_type) {
                case 1:
                    for (uint32_t j = 0; j < cmp_dim; j++) {
                        float diff = net_outputs[i][j] - target[j];
                        dl_dout[j] = (diff > 0.0f ? 1.0f : (diff < 0.0f ? -1.0f : 0.0f)) / n_cmp;
                    }
                    break;
                case 2:
                    for (uint32_t j = 0; j < cmp_dim; j++) {
                        float o = net_outputs[i][j];
                        if (o < 1e-7f) o = 1e-7f;
                        dl_dout[j] = -target[j] / o / n_cmp;
                    }
                    break;
                default:
                    for (uint32_t j = 0; j < cmp_dim; j++) {
                        dl_dout[j] = 2.0f * (net_outputs[i][j] - target[j]) / n_cmp;
                    }
                    break;
            }

            if (mgr->config.enable_cross_network_gradients) {
                for (uint32_t b = 0; b < mgr->num_bridges; b++) {
                    if (mgr->bridges[b].source_idx == (uint32_t)i && mgr->bridges[b].enabled) {
                        uint32_t tgt = mgr->bridges[b].target_idx;
                        const float* tgt_grad = dl_dinputs[tgt];
                        if (tgt_grad) {
                            float* bridge_grad = (float*)nimcp_calloc(out_dim, sizeof(float));
                            if (bridge_grad) {
                                bridge_backward(&mgr->bridges[b], tgt_grad, bridge_grad);
                                for (uint32_t j = 0; j < out_dim; j++) {
                                    dl_dout[j] += bridge_grad[j];
                                }
                                nimcp_free(bridge_grad);
                            }
                        }
                    }
                }
            }

            float div_loss = nimcp_anti_collapse_diversity_loss(
                &mgr->anti_collapse[i], net_outputs[i], dl_dout, out_dim);
            local_result.diversity_loss += div_loss;

            bridged_dl_dout[i] = dl_dout;
        }

        /* Sub-phase 2b: Run backward() in parallel (each is independent now) */
        utm_backward_task_t bwd_tasks2[NIMCP_UTM_MAX_NETWORKS];
        uint32_t bwd_indices2[NIMCP_UTM_MAX_NETWORKS];
        uint32_t bridged_count = 0;

        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!bridged_dl_dout[i]) continue;

            nimcp_trainable_network_t* net = &mgr->networks[i];
            uint32_t out_dim = net_output_dims[i];
            uint32_t in_dim = net->ops->get_input_dim(net->ctx);
            float* dl_din = (float*)nimcp_calloc(in_dim, sizeof(float));
            if (!dl_din) { nimcp_free(bridged_dl_dout[i]); continue; }

            utm_backward_task_t* t = &bwd_tasks2[bridged_count];
            t->net = net;
            t->anti_collapse = NULL;  /* Already applied above */
            t->net_output = net_outputs[i];
            t->target = target;
            t->out_dim = out_dim;
            t->target_dim = target_dim;
            t->loss_type = loss_type;
            t->dl_din = dl_din;
            t->in_dim = in_dim;
            t->div_loss = 0.0f;
            /* Override: use pre-computed dl_dout instead of recomputing */
            t->dl_dout_override = bridged_dl_dout[i];

            bwd_indices2[bridged_count] = i;
            bridged_count++;
        }

        if (bridged_count >= 2) {
            struct timespec _bwd2_t0; clock_gettime(CLOCK_MONOTONIC, &_bwd2_t0);
            nimcp_thread_pool_t* pool = nimcp_pool_create(bridged_count);
            if (pool) {
                NIMCP_LOGGING_INFO("UTM Phase 2b: submitting %u backward tasks to pool", bridged_count);
                for (uint32_t p = 0; p < bridged_count; p++) {
                    nimcp_pool_submit(pool, utm_backward_task_fn, &bwd_tasks2[p]);
                }
                nimcp_pool_wait(pool);
                nimcp_pool_destroy(pool);
            } else {
                NIMCP_LOGGING_WARN("UTM Phase 2b: pool creation FAILED, running %u tasks sequentially", bridged_count);
                for (uint32_t p = 0; p < bridged_count; p++) {
                    utm_backward_task_fn(&bwd_tasks2[p]);
                }
            }
            struct timespec _bwd2_t1; clock_gettime(CLOCK_MONOTONIC, &_bwd2_t1);
            double _bwd2_ms = (_bwd2_t1.tv_sec - _bwd2_t0.tv_sec) * 1000.0
                            + (_bwd2_t1.tv_nsec - _bwd2_t0.tv_nsec) / 1e6;
            NIMCP_LOGGING_INFO("UTM Phase 2b: %u tasks completed in %.0fms", bridged_count, _bwd2_ms);
        } else {
            for (uint32_t p = 0; p < bridged_count; p++) {
                utm_backward_task_fn(&bwd_tasks2[p]);
            }
        }

        /* Collect results and free dl_dout buffers */
        for (uint32_t p = 0; p < bridged_count; p++) {
            uint32_t idx = bwd_indices2[p];
            dl_dinputs[idx] = bwd_tasks2[p].dl_din;
            nimcp_free(bridged_dl_dout[idx]);
        }
    }

    /* Phase 5: Re-enable biological plasticity after backprop */
    if (mgr->plasticity_bridge) {
        nimcp_tpb_set_backprop_active(mgr->plasticity_bridge, false);
    }

    /* Free all stored input gradients */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        nimcp_free(dl_dinputs[i]);
    }

    /* ------------------------------------------------------------------ */
    /* Step 4.5: Mini-batch accumulation check                            */
    /* ------------------------------------------------------------------ */
    mgr->batch_accumulation_count++;
    bool batch_complete = (mgr->batch_accumulation_count >= effective_batch);

    if (!batch_complete) {
        /* Accumulating — skip gradient normalization and optimizer step */
        local_result.composite_loss = composite_loss;
        local_result.step = mgr->step_count;
        if (result) *result = local_result;
        goto cleanup;
    }

    /* B4: Track which networks actually ran backward successfully */
    bool backward_ran[NIMCP_UTM_MAX_NETWORKS];
    memset(backward_ran, 0, sizeof(backward_ran));
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (mgr->networks[i].enabled && net_outputs[i]) {
            backward_ran[i] = true;
        }
    }

    /* Batch complete — divide accumulated gradients by batch_size */
    if (effective_batch > 1) {
        float inv_batch = 1.0f / (float)effective_batch;
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled || !backward_ran[i]) continue;
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (mgr->networks[i].ops->get_param_groups &&
                mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups; g++) {
                    if (groups[g].gradients && groups[g].count > 0) {
                        for (size_t j = 0; j < groups[g].count; j++) {
                            groups[g].gradients[j] *= inv_batch;
                        }
                    }
                }
                nimcp_free(groups);
            }
        }
    }
    mgr->batch_accumulation_count = 0;

    /* Gradient checkpointing: end backward pass */
#ifdef NIMCP_ENABLE_CUDA
    if (mgr->grad_checkpoint_ctx) {
        nimcp_checkpoint_end_backward(mgr->grad_checkpoint_ctx);
    }
#endif

    struct timespec _utm_bwd_end; clock_gettime(CLOCK_MONOTONIC, &_utm_bwd_end);

    /* ------------------------------------------------------------------ */
    /* Step 5: Gradient normalization / clipping (unified, single pass)   */
    /* ------------------------------------------------------------------ */
    {
        /* B3: Per-network gradient normalization, then lighter global safety clip */
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_trainable_network_t* net = &mgr->networks[i];

            float* net_grads[32];
            size_t net_sizes[32];
            uint32_t net_arrays = 0;

            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (net->ops->get_param_groups &&
                net->ops->get_param_groups(net->ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups && net_arrays < 32; g++) {
                    if (groups[g].gradients && groups[g].count > 0) {
                        net_grads[net_arrays] = groups[g].gradients;
                        net_sizes[net_arrays] = groups[g].count;
                        net_arrays++;
                    }
                }
                nimcp_free(groups);
            }

            if (net_arrays > 0) {
                nimcp_anti_collapse_normalize_gradients(
                    &mgr->anti_collapse[i], net_grads, net_sizes, net_arrays);
            }
        }

        /* Global safety clip at 10x target norm using anti_collapse[0] */
        float* all_grads[NIMCP_UTM_MAX_NETWORKS * 32];
        size_t all_sizes[NIMCP_UTM_MAX_NETWORKS * 32];
        uint32_t total_arrays = 0;

        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_trainable_network_t* net = &mgr->networks[i];
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (net->ops->get_param_groups &&
                net->ops->get_param_groups(net->ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups && total_arrays < NIMCP_UTM_MAX_NETWORKS * 32; g++) {
                    if (groups[g].gradients && groups[g].count > 0) {
                        all_grads[total_arrays] = groups[g].gradients;
                        all_sizes[total_arrays] = groups[g].count;
                        total_arrays++;
                    }
                }
                nimcp_free(groups);
            }
        }

        /* Include bridge weight gradients */
        for (uint32_t b = 0; b < mgr->num_bridges && total_arrays < NIMCP_UTM_MAX_NETWORKS * 32; b++) {
            nimcp_cross_network_bridge_t* br = &mgr->bridges[b];
            if (br->weight_grad && br->source_dim > 0 && br->target_dim > 0) {
                all_grads[total_arrays] = br->weight_grad;
                all_sizes[total_arrays] = (size_t)br->target_dim * br->source_dim;
                total_arrays++;
            }
        }

        /* Global safety clip (10x target norm) */
        float scale = nimcp_anti_collapse_normalize_gradients(
            &mgr->anti_collapse[0], all_grads, all_sizes, total_arrays);

        float effective_target = mgr->anti_collapse[0].config.gradient_target_norm;
        if (mgr->anti_collapse[0].config.adaptive_gradient_target && effective_target <= 0.0f) {
            effective_target = sqrtf(mgr->anti_collapse[0].ema_gradient_norm > 0.0f ?
                                     mgr->anti_collapse[0].ema_gradient_norm : 1.0f);
        }
        local_result.gradient_norm = (scale != 0.0f) ?
            effective_target / scale : 0.0f;
        local_result.gradient_scale = scale;
    }

    /* ------------------------------------------------------------------ */
    /* Gap 1: AMP unscale gradients (before optimizer)                    */
    /* ------------------------------------------------------------------ */
    if (mgr->config.enable_amp && mgr->amp_ctx) {
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (mgr->networks[i].ops->get_param_groups &&
                mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups; g++) {
                    if (groups[g].gradients && groups[g].count > 0) {
                        amp_unscale_gradients((amp_ctx_t*)mgr->amp_ctx,
                            groups[g].gradients, groups[g].count);
                    }
                }
                nimcp_free(groups);
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* Gap 2: Gradient health check — NaN/Inf detection + sanitization    */
    /* ------------------------------------------------------------------ */
    local_result.gradient_healthy = true;
    local_result.gradients_sanitized = 0;
    if (mgr->config.enable_gradient_manager) {
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (mgr->networks[i].ops->get_param_groups &&
                mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups; g++) {
                    if (groups[g].gradients && groups[g].count > 0) {
                        nimcp_grad_health_t health = nimcp_gradient_check_health(
                            groups[g].gradients, groups[g].count);
                        if (health != NIMCP_GRAD_HEALTHY) {
                            local_result.gradient_healthy = false;
                            uint64_t sanitized = nimcp_gradient_sanitize(
                                groups[g].gradients, groups[g].count, 0.0f);
                            local_result.gradients_sanitized += sanitized;
                        }
                    }
                }
                nimcp_free(groups);
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* Gap 7: L1 regularization gradient injection                        */
    /* ------------------------------------------------------------------ */
    if (mgr->config.enable_regularization && mgr->config.l1_lambda > 0.0f) {
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (mgr->networks[i].ops->get_param_groups &&
                mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups; g++) {
                    if (groups[g].params && groups[g].gradients && groups[g].count > 0) {
                        nimcp_l1_gradient(groups[g].params, groups[g].gradients,
                            groups[g].count, mgr->config.l1_lambda);
                    }
                }
                nimcp_free(groups);
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* Step 6: Optimizer step (AdamW with LR schedule)                    */
    /* ------------------------------------------------------------------ */

    /* Update learning rate from schedule */
    /* Item 10: Use middleware LR scheduler if available */
    if (mgr->lr_scheduler_ctx) {
        mgr->current_lr = nimcp_lr_scheduler_step(
            (nimcp_lr_scheduler_ctx_t*)mgr->lr_scheduler_ctx);
    } else {
        mgr->current_lr = nimcp_utm_get_scheduled_lr(mgr);
    }

    /* Item 8: Neuromodulator-gated LR modulation */
    mgr->neuromod_lr_scale = 1.0f;
    if (mgr->config.enable_neuromod_lr && mgr->neuromod_adapter) {
        float dopamine = 0.5f, norepinephrine = 0.5f;
        nimcp_neuromod_pool_adapter_get_level(
            (nimcp_neuromod_pool_adapter_t)mgr->neuromod_adapter,
            NMOD_DOPAMINE, &dopamine);
        nimcp_neuromod_pool_adapter_get_level(
            (nimcp_neuromod_pool_adapter_t)mgr->neuromod_adapter,
            NMOD_NOREPINEPHRINE, &norepinephrine);
        /* Dopamine boosts learning (reward signal), NE modulates attention */
        float neuro_scale = 0.5f + dopamine + 0.3f * norepinephrine;
        /* Clamp to [0.1, 3.0] for stability */
        if (neuro_scale < 0.1f) neuro_scale = 0.1f;
        if (neuro_scale > 3.0f) neuro_scale = 3.0f;
        mgr->neuromod_lr_scale = neuro_scale;
    }
    local_result.neuromod_lr_scale = mgr->neuromod_lr_scale;
    local_result.scheduled_lr = mgr->current_lr;

    /* AdamW constants */
    const float adam_beta1 = 0.9f;
    const float adam_beta2 = 0.999f;
    const float adam_eps = 1e-8f;

    /* Update bias correction terms: beta^t */
    mgr->adam_beta1_t *= adam_beta1;
    mgr->adam_beta2_t *= adam_beta2;

    /* Collect all param groups across all networks to assign moment indices */
    {
        /* First pass: count total param groups */
        uint32_t total_groups = 0;
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_trainable_network_t* net = &mgr->networks[i];
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (net->ops->get_param_groups &&
                net->ops->get_param_groups(net->ctx, &groups, &num_groups) == 0) {
                total_groups += num_groups;
                nimcp_free(groups);
            }
        }

        /* Lazy-allocate or re-allocate Adam moment arrays if group count changed */
        if (total_groups > 0 && (mgr->adam_m == NULL || total_groups != mgr->adam_num_groups)) {
            /* Free old state if group count changed */
            if (mgr->adam_m) {
                for (uint32_t i = 0; i < mgr->adam_num_groups; i++) {
                    nimcp_free(mgr->adam_m[i]);
                    nimcp_free(mgr->adam_v[i]);
                }
                nimcp_free(mgr->adam_m);
                nimcp_free(mgr->adam_v);
                nimcp_free(mgr->adam_sizes);
            }

            mgr->adam_m = (float**)nimcp_calloc(total_groups, sizeof(float*));
            mgr->adam_v = (float**)nimcp_calloc(total_groups, sizeof(float*));
            mgr->adam_sizes = (size_t*)nimcp_calloc(total_groups, sizeof(size_t));
            mgr->adam_num_groups = total_groups;
            /* Reset bias correction on realloc */
            mgr->adam_beta1_t = adam_beta1;
            mgr->adam_beta2_t = adam_beta2;
        }

        /* Second pass: apply AdamW update */
        uint32_t group_idx = 0;
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_trainable_network_t* net = &mgr->networks[i];

            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (net->ops->get_param_groups &&
                net->ops->get_param_groups(net->ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups; g++) {
                    if (!groups[g].params || !groups[g].gradients || groups[g].count == 0) {
                        group_idx++;
                        continue;
                    }

                    /* Lazy-allocate moment vectors for this group */
                    if (group_idx < mgr->adam_num_groups) {
                        if (!mgr->adam_m[group_idx] || mgr->adam_sizes[group_idx] != groups[g].count) {
                            nimcp_free(mgr->adam_m[group_idx]);
                            nimcp_free(mgr->adam_v[group_idx]);
                            mgr->adam_m[group_idx] = (float*)nimcp_calloc(groups[g].count, sizeof(float));
                            mgr->adam_v[group_idx] = (float*)nimcp_calloc(groups[g].count, sizeof(float));
                            mgr->adam_sizes[group_idx] = groups[g].count;
                        }
                    }

                    /* Feature 1: Hub-aware fractal LR scaling */
                    float fractal_scale = mgr->fractal_enabled ?
                        mgr->fractal_lr_multiplier[i] : 1.0f;
                    /* Item 13: Per-network LR override */
                    float base_lr = (mgr->per_network_lr[i] > 0.0f) ?
                        mgr->per_network_lr[i] : mgr->current_lr;
                    /* Item 8: Neuromod LR modulation */
                    float lr = base_lr * groups[g].lr_scale * fractal_scale *
                        mgr->neuromod_lr_scale;
                    float wd = groups[g].weight_decay;
                    float bc1 = 1.0f - mgr->adam_beta1_t;  /* 1 - beta1^t */
                    float bc2 = 1.0f - mgr->adam_beta2_t;  /* 1 - beta2^t */

                    float* m = (group_idx < mgr->adam_num_groups) ? mgr->adam_m[group_idx] : NULL;
                    float* v = (group_idx < mgr->adam_num_groups) ? mgr->adam_v[group_idx] : NULL;

                    /* Feature 7: Natural gradient optimizer for small param groups */
                    bool use_ng = mgr->natural_gradient_enabled &&
                                  groups[g].count <= mgr->natural_grad_max_params &&
                                  groups[g].count > 0;

                    if (use_ng) {
                        /* Lazy-create Fisher + NG handles for this network */
                        if (!mgr->fisher[i]) {
                            nimcp_fisher_config_t f_cfg = {0};
                            f_cfg.param_dim = (uint32_t)groups[g].count;
                            f_cfg.sample_size = 1;
                            f_cfg.regularization = 1e-4f;
                            f_cfg.use_empirical = true;
                            f_cfg.enable_damping = true;
                            f_cfg.initial_damping = 1e-3f;
                            mgr->fisher[i] = nimcp_fisher_create(&f_cfg);
                        }
                        if (!mgr->natural_grad[i]) {
                            nimcp_natural_grad_config_t ng_cfg = {0};
                            ng_cfg.learning_rate = lr;
                            ng_cfg.momentum = 0.9f;
                            ng_cfg.gradient_clip = 5.0f;
                            ng_cfg.use_preconditioner = true;
                            ng_cfg.enable_warmup = false;
                            mgr->natural_grad[i] = nimcp_natural_grad_create(
                                &ng_cfg, (uint32_t)groups[g].count);
                        }

                        /* Periodically recompute Fisher */
                        if (mgr->fisher[i] &&
                            mgr->step_count % mgr->fisher_update_interval == 0) {
                            nimcp_fisher_compute(mgr->fisher[i],
                                groups[g].gradients, 1, (uint32_t)groups[g].count);
                            if (mgr->natural_grad[i]) {
                                nimcp_natural_grad_update_fisher(
                                    mgr->natural_grad[i], mgr->fisher[i]);
                            }
                        }

                        /* Apply weight decay before NG step */
                        if (wd > 0.0f) {
                            for (size_t j = 0; j < groups[g].count; j++) {
                                groups[g].params[j] -= lr * wd * groups[g].params[j];
                            }
                        }

                        /* Natural gradient step */
                        if (mgr->natural_grad[i]) {
                            nimcp_natural_grad_step(mgr->natural_grad[i],
                                groups[g].params, groups[g].gradients,
                                (uint32_t)groups[g].count);
                        }
                    } else if (mgr->riemannian_enabled &&
                               groups[g].count <= mgr->riemannian_max_params &&
                               groups[g].count > 0) {
                        /* Riemannian SGD: precondition gradients with metric tensor.
                         * The metric captures parameter space curvature, producing
                         * geometry-aware updates (shorter steps in curved directions). */
                        uint32_t pdim = (uint32_t)groups[g].count;

                        /* Lazy-create metric for this network */
                        if (!mgr->riemannian_metric) {
                            mgr->riemannian_metric = riemannian_metric_create(pdim);
                        }
                        riemannian_metric_t* metric = (riemannian_metric_t*)mgr->riemannian_metric;

                        if (metric && metric->dim == pdim) {
                            /* Build metric from gradient outer product (empirical Fisher approx):
                             * g_{ij} += grad_i * grad_j, then regularize with identity */
                            for (uint32_t r = 0; r < pdim; r++) {
                                for (uint32_t c = r; c < pdim; c++) {
                                    float val = groups[g].gradients[r] * groups[g].gradients[c];
                                    /* Exponential moving average */
                                    uint32_t idx = r * pdim + c;
                                    metric->g[idx] = 0.9f * metric->g[idx] + 0.1f * val;
                                    if (r != c) {
                                        metric->g[c * pdim + r] = metric->g[idx];
                                    }
                                }
                                /* Tikhonov regularization for stability */
                                metric->g[r * pdim + r] += 1e-4f;
                            }
                            metric->inv_valid = false;
                            riemannian_metric_invert(metric);

                            if (metric->inv_valid) {
                                /* Riemannian gradient: g^{ij} grad_j */
                                float* riem_grad = (float*)nimcp_calloc(pdim, sizeof(float));
                                if (riem_grad) {
                                    riemannian_raise_index(metric, groups[g].gradients, riem_grad);

                                    /* Weight decay + Riemannian update */
                                    for (size_t j = 0; j < groups[g].count; j++) {
                                        if (wd > 0.0f) {
                                            groups[g].params[j] -= lr * wd * groups[g].params[j];
                                        }
                                        groups[g].params[j] -= lr * riem_grad[j];
                                    }
                                    nimcp_free(riem_grad);
                                }
                            } else {
                                /* Fallback to plain SGD if metric singular */
                                for (size_t j = 0; j < groups[g].count; j++) {
                                    if (wd > 0.0f) {
                                        groups[g].params[j] -= lr * wd * groups[g].params[j];
                                    }
                                    groups[g].params[j] -= lr * groups[g].gradients[j];
                                }
                            }
                        } else {
                            /* Dimension mismatch — fall through to AdamW */
                            goto adamw_fallback;
                        }
                    } else {
                        adamw_fallback:
                        /* Standard AdamW — try GPU first for large param groups */
#ifdef NIMCP_ENABLE_CUDA
                        if (mgr->gpu_ctx && m && v && groups[g].count >= 1024) {
                            /* GPU-accelerated AdamW for large parameter groups */
                            if (nimcp_gpu_optim_adamw(mgr->gpu_ctx,
                                    groups[g].params, groups[g].gradients, m, v,
                                    (uint32_t)groups[g].count,
                                    lr, adam_beta1, adam_beta2, adam_eps, wd,
                                    mgr->step_count)) {
                                goto adamw_done;  /* GPU succeeded */
                            }
                            /* GPU failed — fall through to CPU */
                        }
#endif
                        for (size_t j = 0; j < groups[g].count; j++) {
                            float grad = groups[g].gradients[j];

                            if (m && v) {
                                /* AdamW: update moments */
                                m[j] = adam_beta1 * m[j] + (1.0f - adam_beta1) * grad;
                                v[j] = adam_beta2 * v[j] + (1.0f - adam_beta2) * grad * grad;

                                /* Bias-corrected estimates */
                                float m_hat = m[j] / bc1;
                                float v_hat = v[j] / bc2;

                                /* AdamW: decoupled weight decay */
                                if (wd > 0.0f) {
                                    groups[g].params[j] -= lr * wd * groups[g].params[j];
                                }

                                /* Adam update */
                                groups[g].params[j] -= lr * m_hat / (sqrtf(v_hat) + adam_eps);
                            } else {
                                /* Fallback SGD */
                                if (wd > 0.0f) {
                                    groups[g].params[j] -= lr * wd * groups[g].params[j];
                                }
                                groups[g].params[j] -= lr * grad;
                            }
                        }
#ifdef NIMCP_ENABLE_CUDA
                        adamw_done:
#endif
                        /* Sparse training: zero out small gradients to maintain sparsity */
                        if (mgr->config.enable_sparse_training) {
                            float sparse_threshold = 1e-6f;
                            for (size_t j = 0; j < groups[g].count; j++) {
                                if (fabsf(groups[g].params[j]) < sparse_threshold) {
                                    groups[g].params[j] = 0.0f;
                                }
                            }
                        }
                    }

                    group_idx++;
                }
                nimcp_free(groups);
            }
        }
    }

    /* Phase 4: Sync cached params back to underlying networks after AdamW */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        if (mgr->networks[i].ops->sync_params) {
            mgr->networks[i].ops->sync_params(mgr->networks[i].ctx);
        }
    }

    /* Apply bridge weight updates */
    /* Gap 8: Real AdamW with momentum for bridge params */
    if (mgr->num_bridges > 0) {
        /* Lazy-allocate bridge Adam state */
        if (!mgr->bridge_adam_m && mgr->bridge_params_in_optimizer) {
            mgr->bridge_adam_m = (float**)nimcp_calloc(mgr->num_bridges, sizeof(float*));
            mgr->bridge_adam_v = (float**)nimcp_calloc(mgr->num_bridges, sizeof(float*));
            mgr->bridge_adam_sizes = (size_t*)nimcp_calloc(mgr->num_bridges, sizeof(size_t));
            mgr->bridge_adam_num = mgr->num_bridges;
        }

        for (uint32_t b = 0; b < mgr->num_bridges; b++) {
            nimcp_cross_network_bridge_t* br = &mgr->bridges[b];
            if (!br->enabled || !br->transform_weights || !br->weight_grad) continue;

            size_t w_size = (size_t)br->target_dim * br->source_dim;
            float bridge_lr = mgr->current_lr * mgr->neuromod_lr_scale;

            if (mgr->bridge_params_in_optimizer && mgr->bridge_adam_m) {
                /* Lazy-allocate per-bridge moment vectors */
                if (!mgr->bridge_adam_m[b] || mgr->bridge_adam_sizes[b] != w_size) {
                    nimcp_free(mgr->bridge_adam_m[b]);
                    nimcp_free(mgr->bridge_adam_v[b]);
                    mgr->bridge_adam_m[b] = (float*)nimcp_calloc(w_size, sizeof(float));
                    mgr->bridge_adam_v[b] = (float*)nimcp_calloc(w_size, sizeof(float));
                    mgr->bridge_adam_sizes[b] = w_size;
                }

                float* bm = mgr->bridge_adam_m[b];
                float* bv = mgr->bridge_adam_v[b];
                float bc1 = 1.0f - mgr->adam_beta1_t;
                float bc2 = 1.0f - mgr->adam_beta2_t;

                if (bm && bv) {
                    for (size_t j = 0; j < w_size; j++) {
                        float grad = br->weight_grad[j];
                        /* AdamW moments */
                        bm[j] = adam_beta1 * bm[j] + (1.0f - adam_beta1) * grad;
                        bv[j] = adam_beta2 * bv[j] + (1.0f - adam_beta2) * grad * grad;
                        /* Bias-corrected */
                        float m_hat = bm[j] / bc1;
                        float v_hat = bv[j] / bc2;
                        /* Weight decay */
                        if (mgr->config.weight_decay > 0.0f) {
                            br->transform_weights[j] -= bridge_lr * mgr->config.weight_decay *
                                br->transform_weights[j];
                        }
                        /* Adam update */
                        br->transform_weights[j] -= bridge_lr * m_hat / (sqrtf(v_hat) + adam_eps);
                    }
                } else {
                    /* Fallback if allocation failed */
                    for (size_t j = 0; j < w_size; j++) {
                        br->transform_weights[j] -= bridge_lr * br->weight_grad[j];
                    }
                }
            } else {
                /* Simple SGD fallback */
                for (size_t j = 0; j < w_size; j++) {
                    br->transform_weights[j] -= bridge_lr * br->weight_grad[j];
                }
            }
            if (br->transform_bias && br->bias_grad) {
                for (uint32_t j = 0; j < br->target_dim; j++) {
                    br->transform_bias[j] -= bridge_lr * br->bias_grad[j];
                }
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* Step 7: Update state                                               */
    /* ------------------------------------------------------------------ */
    mgr->step_count++;
    mgr->last_composite_loss = composite_loss;
    memcpy(mgr->per_network_loss, local_result.per_network_loss, sizeof(mgr->per_network_loss));

    local_result.step = mgr->step_count;

    /* Item 9: EMA / Polyak averaging — update shadow params */
    if (mgr->ema_enabled) {
        /* Collect all param groups and update EMA copies */
        uint32_t g_idx = 0;
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled) continue;
            nimcp_utm_param_group_t* groups = NULL;
            uint32_t num_groups = 0;
            if (mgr->networks[i].ops->get_param_groups &&
                mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
                for (uint32_t g = 0; g < num_groups; g++) {
                    if (!groups[g].params || groups[g].count == 0) { g_idx++; continue; }

                    /* Lazy-allocate EMA arrays */
                    if (!mgr->ema_params || g_idx >= mgr->ema_num_groups) {
                        uint32_t new_count = g_idx + num_groups;
                        float** new_params = (float**)nimcp_calloc(new_count, sizeof(float*));
                        size_t* new_sizes = (size_t*)nimcp_calloc(new_count, sizeof(size_t));
                        if (new_params && new_sizes) {
                            if (mgr->ema_params) {
                                memcpy(new_params, mgr->ema_params, mgr->ema_num_groups * sizeof(float*));
                                memcpy(new_sizes, mgr->ema_sizes, mgr->ema_num_groups * sizeof(size_t));
                                nimcp_free(mgr->ema_params);
                                nimcp_free(mgr->ema_sizes);
                            }
                            mgr->ema_params = new_params;
                            mgr->ema_sizes = new_sizes;
                            mgr->ema_num_groups = new_count;
                        } else {
                            nimcp_free(new_params);
                            nimcp_free(new_sizes);
                        }
                    }

                    if (mgr->ema_params && g_idx < mgr->ema_num_groups) {
                        if (!mgr->ema_params[g_idx] || mgr->ema_sizes[g_idx] != groups[g].count) {
                            nimcp_free(mgr->ema_params[g_idx]);
                            mgr->ema_params[g_idx] = (float*)nimcp_malloc(groups[g].count * sizeof(float));
                            mgr->ema_sizes[g_idx] = groups[g].count;
                            /* Initialize to current params on first allocation */
                            if (mgr->ema_params[g_idx]) {
                                memcpy(mgr->ema_params[g_idx], groups[g].params,
                                       groups[g].count * sizeof(float));
                            }
                        } else if (mgr->ema_params[g_idx]) {
                            /* Polyak update: ema = decay * ema + (1 - decay) * params */
                            float d = mgr->ema_decay;
                            float od = 1.0f - d;
                            for (size_t j = 0; j < groups[g].count; j++) {
                                mgr->ema_params[g_idx][j] = d * mgr->ema_params[g_idx][j] +
                                    od * groups[g].params[j];
                            }
                        }
                    }
                    g_idx++;
                }
                nimcp_free(groups);
            }
        }
    }

    /* Item 14: Early stopping check */
    if (mgr->early_stopping_enabled && isfinite(composite_loss)) {
        if (composite_loss < mgr->early_stopping_best_loss - mgr->early_stopping_min_delta) {
            mgr->early_stopping_best_loss = composite_loss;
            mgr->early_stopping_counter = 0;
        } else {
            mgr->early_stopping_counter++;
            if (mgr->early_stopping_counter >= mgr->early_stopping_patience) {
                mgr->early_stopped = true;
                NIMCP_LOGGING_INFO("UTM: Early stopping triggered at step %lu (best loss: %.6f)",
                                   (unsigned long)mgr->step_count, mgr->early_stopping_best_loss);
            }
        }
    }
    local_result.early_stopped = mgr->early_stopped;

    /* ------------------------------------------------------------------ */
    /* Gap 1: AMP update scale (after optimizer step)                      */
    /* ------------------------------------------------------------------ */
    if (mgr->config.enable_amp && mgr->amp_ctx) {
        amp_update_scale((amp_ctx_t*)mgr->amp_ctx, local_result.gradient_healthy);
    }

    /* ------------------------------------------------------------------ */
    /* Gap 6: Training diagnosis — observe metrics + apply recommendations */
    /* ------------------------------------------------------------------ */
    local_result.diagnosis_reduce_lr = false;
    local_result.diagnosis_lr_factor = 1.0f;
    if (mgr->config.enable_training_diagnosis) {
        /* Lazy-create diagnoser */
        if (!mgr->diagnoser) {
            mgr->diagnoser = training_diagnoser_create();
        }
        if (mgr->diagnoser) {
            /* Compute loss_volatility: stddev of recent losses from ring buffer */
            float loss_volatility = 0.0f;
            if (mgr->loss_history && mgr->loss_history_count >= 2) {
                uint32_t count = mgr->loss_history_count;
                uint32_t start = (count < mgr->loss_history_size) ?
                    0 : mgr->loss_history_pos;
                float sum = 0.0f;
                for (uint32_t k = 0; k < count; k++) {
                    sum += mgr->loss_history[(start + k) % mgr->loss_history_size];
                }
                float mean = sum / (float)count;
                float var_sum = 0.0f;
                for (uint32_t k = 0; k < count; k++) {
                    float diff = mgr->loss_history[(start + k) % mgr->loss_history_size] - mean;
                    var_sum += diff * diff;
                }
                loss_volatility = sqrtf(var_sum / (float)count);
            }

            /* Compute gradient_variance: variance of per-network EMA gradient norms */
            float gradient_variance = 0.0f;
            if (mgr->num_networks >= 2) {
                float gn_sum = 0.0f;
                uint32_t gn_count = 0;
                for (uint32_t i = 0; i < mgr->num_networks; i++) {
                    if (mgr->networks[i].enabled) {
                        gn_sum += mgr->anti_collapse[i].ema_gradient_norm;
                        gn_count++;
                    }
                }
                if (gn_count >= 2) {
                    float gn_mean = gn_sum / (float)gn_count;
                    float gn_var = 0.0f;
                    for (uint32_t i = 0; i < mgr->num_networks; i++) {
                        if (mgr->networks[i].enabled) {
                            float d = mgr->anti_collapse[i].ema_gradient_norm - gn_mean;
                            gn_var += d * d;
                        }
                    }
                    gradient_variance = gn_var / (float)gn_count;
                }
            }

            /* Arousal level: neuromod LR scale captures NE/DA state (0.5 = neutral) */
            float arousal_level = mgr->neuromod_lr_scale;
            if (arousal_level < 0.0f) arousal_level = 0.0f;
            if (arousal_level > 1.0f) arousal_level = 1.0f;

            /* Inflammation level: ratio of sanitized gradients (NaN/Inf replaced) */
            float inflammation_level = 0.0f;
            if (local_result.gradients_sanitized > 0) {
                /* Estimate total gradient count from all param groups */
                uint64_t total_params = 0;
                for (uint32_t i = 0; i < mgr->num_networks; i++) {
                    if (!mgr->networks[i].enabled) continue;
                    nimcp_utm_param_group_t* groups = NULL;
                    uint32_t num_groups = 0;
                    if (mgr->networks[i].ops->get_param_groups &&
                        mgr->networks[i].ops->get_param_groups(
                            mgr->networks[i].ctx, &groups, &num_groups) == 0) {
                        for (uint32_t g = 0; g < num_groups; g++) {
                            total_params += groups[g].count;
                        }
                        nimcp_free(groups);
                    }
                }
                if (total_params > 0) {
                    inflammation_level = (float)local_result.gradients_sanitized /
                                         (float)total_params;
                    if (inflammation_level > 1.0f) inflammation_level = 1.0f;
                }
            }

            /* Resource pressure: batch accumulation progress (0 = fresh, 1 = full) */
            float resource_pressure = 0.0f;
            {
                uint32_t eff_batch = (mgr->config.batch_size > 1) ?
                    mgr->config.batch_size : 1;
                resource_pressure = (float)mgr->batch_accumulation_count / (float)eff_batch;
                if (resource_pressure > 1.0f) resource_pressure = 1.0f;
            }

            training_diagnoser_observe_from_metrics(mgr->diagnoser,
                composite_loss,                         /* loss_current */
                mgr->previous_loss,                     /* loss_previous */
                local_result.gradient_norm,             /* grad_norm */
                mgr->previous_grad_norm,                /* grad_norm_previous */
                loss_volatility,                        /* loss_volatility */
                gradient_variance,                      /* gradient_variance */
                mgr->current_lr,                        /* learning_rate */
                (float)(mgr->config.batch_size > 0 ? mgr->config.batch_size : 1), /* batch_size */
                arousal_level,                          /* arousal_level */
                inflammation_level,                     /* inflammation_level */
                resource_pressure);                     /* resource_pressure */

            training_diagnosis_t diagnosis = {0};
            if (training_diagnoser_diagnose(mgr->diagnoser, &diagnosis) == 0) {
                local_result.diagnosis_reduce_lr = diagnosis.recommend_reduce_lr;
                local_result.diagnosis_lr_factor = diagnosis.recommended_lr_factor;
                /* Apply recommended LR factor (clamped for safety) */
                if (diagnosis.recommend_reduce_lr && diagnosis.recommended_lr_factor > 0.0f &&
                    diagnosis.recommended_lr_factor < 1.0f) {
                    float factor = diagnosis.recommended_lr_factor;
                    if (factor < 0.1f) factor = 0.1f;
                    mgr->current_lr *= factor;
                }
            }
        }
    }
    mgr->previous_loss = composite_loss;
    mgr->previous_grad_norm = local_result.gradient_norm;

    /* ------------------------------------------------------------------ */
    /* Gap 4: Training callbacks — fire step complete event                */
    /* ------------------------------------------------------------------ */
    if (mgr->config.enable_training_callbacks && mgr->tcb_ctx) {
        tcb_metrics_t tcb_metrics = {0};
        tcb_metrics.loss = composite_loss;
        tcb_metrics.learning_rate = mgr->current_lr;
        tcb_metrics.gradient_norm = local_result.gradient_norm;
        tcb_metrics.step = mgr->step_count;
        tcb_action_t action = tcb_fire_event(mgr->tcb_ctx,
            TCB_EVENT_STEP_COMPLETE, &tcb_metrics);
        if (action == TCB_ACTION_STOP_TRAINING) {
            mgr->early_stopped = true;
            local_result.early_stopped = true;
        } else if (action == TCB_ACTION_REDUCE_LR) {
            mgr->current_lr *= 0.5f;
        }

        /* Fire divergence event if loss is NaN or Inf */
        if (!isfinite(composite_loss)) {
            tcb_fire_event(mgr->tcb_ctx, TCB_EVENT_DIVERGENCE, &tcb_metrics);
        }
    }

    /* ------------------------------------------------------------------ */
    /* Gap 5: Checkpoint save (periodic) — via TCB_EVENT_CHECKPOINT       */
    /* ------------------------------------------------------------------ */
    /* Checkpoint saving is triggered via the callback system rather than
     * direct checkpoint_mgr calls, since checkpoint_mgr_t is caller-owned.
     * The caller registers a TCB_EVENT_CHECKPOINT callback that calls
     * checkpoint_save_full() with the appropriate weight tensors. */
    if (mgr->config.enable_checkpoint_manager && mgr->tcb_ctx &&
        mgr->config.checkpoint_interval > 0 &&
        mgr->step_count % mgr->config.checkpoint_interval == 0) {
        tcb_metrics_t ckpt_metrics = {0};
        ckpt_metrics.loss = composite_loss;
        ckpt_metrics.step = mgr->step_count;
        ckpt_metrics.learning_rate = mgr->current_lr;
        tcb_fire_event(mgr->tcb_ctx, TCB_EVENT_CHECKPOINT, &ckpt_metrics);
    }

    /* ------------------------------------------------------------------ */
    /* Step 8: DFA health monitoring + fractal analysis                    */
    /* ------------------------------------------------------------------ */
    if (mgr->loss_history && mgr->loss_history_size > 0) {
        /* Record loss in ring buffer */
        mgr->loss_history[mgr->loss_history_pos] = composite_loss;
        mgr->loss_history_pos = (mgr->loss_history_pos + 1) % mgr->loss_history_size;
        if (mgr->loss_history_count < mgr->loss_history_size) {
            mgr->loss_history_count++;
        }

        /* Record per-network losses for phase coherence */
        if (mgr->num_networks >= 2 && mgr->per_network_loss_history == NULL) {
            mgr->per_network_loss_history = (float*)nimcp_calloc(
                (size_t)mgr->loss_history_size * mgr->num_networks, sizeof(float));
        }
        if (mgr->per_network_loss_history) {
            uint32_t pos = (mgr->loss_history_pos == 0) ?
                            mgr->loss_history_size - 1 : mgr->loss_history_pos - 1;
            for (uint32_t n = 0; n < mgr->num_networks; n++) {
                mgr->per_network_loss_history[pos * mgr->num_networks + n] =
                    local_result.per_network_loss[n];
            }
        }

        /* Run health check every N steps when enough data */
        if (mgr->health_check_interval > 0 &&
            mgr->step_count % mgr->health_check_interval == 0 &&
            mgr->loss_history_count >= 32) {

            /* Linearize ring buffer for fractal analysis */
            uint32_t count = mgr->loss_history_count;
            float* linear = (float*)nimcp_malloc(count * sizeof(float));
            if (linear) {
                uint32_t start = (mgr->loss_history_count < mgr->loss_history_size) ?
                    0 : mgr->loss_history_pos;
                for (uint32_t k = 0; k < count; k++) {
                    linear[k] = mgr->loss_history[(start + k) % mgr->loss_history_size];
                }

                /* Feature 2: DFA + Hurst */
                fractal_config_t fcfg = fractal_config_default();
                fractal_result_t dfa_result = {0};
                fractal_result_t hurst_result = {0};

                if (fractal_dfa(linear, count, &fcfg, &dfa_result) == 0) {
                    mgr->dfa_exponent = dfa_result.dfa_exponent;
                }
                if (fractal_hurst_rs(linear, count, &fcfg, &hurst_result) == 0) {
                    mgr->hurst_exponent = hurst_result.hurst_exponent;
                }

                /* Classify health */
                float alpha = mgr->dfa_exponent;
                float H = mgr->hurst_exponent;
                if (alpha < 0.3f) {
                    mgr->training_health = NIMCP_TRAINING_HEALTH_OSCILLATING;
                } else if (alpha < 0.6f) {
                    mgr->training_health = NIMCP_TRAINING_HEALTH_NOISY;
                } else if (alpha > 1.3f) {
                    mgr->training_health = NIMCP_TRAINING_HEALTH_DRIFTING;
                } else if (H > 0.8f) {
                    mgr->training_health = NIMCP_TRAINING_HEALTH_PLATEAU;
                } else {
                    mgr->training_health = NIMCP_TRAINING_HEALTH_OPTIMAL;
                }

                /* Feature 2: Auto-adjust LR based on health */
                if (mgr->dfa_auto_adjust_lr) {
                    switch (mgr->training_health) {
                        case NIMCP_TRAINING_HEALTH_NOISY:
                            mgr->current_lr *= 0.8f;
                            break;
                        case NIMCP_TRAINING_HEALTH_DRIFTING:
                            mgr->current_lr *= 0.5f;
                            break;
                        case NIMCP_TRAINING_HEALTH_OSCILLATING:
                            mgr->current_lr *= 0.3f;
                            break;
                        case NIMCP_TRAINING_HEALTH_PLATEAU:
                            mgr->current_lr *= 1.5f;
                            break;
                        default:
                            break;
                    }
                }

                /* DFA → Pink Noise closed-loop feedback.
                 * Adjusts neuromodulator pink noise amplitudes based on training health:
                 *   NOISY/OSCILLATING: reduce amplitude (dampen stochasticity)
                 *   DRIFTING: reduce amplitude (stabilize)
                 *   PLATEAU: boost amplitude (inject exploration to escape local minimum)
                 *   OPTIMAL: restore to baseline (ideal dynamics, no correction needed)
                 *
                 * This creates a self-tuning system: DFA measures whether training
                 * dynamics are in the pink noise regime, pink noise amplitude adjusts
                 * to steer dynamics back toward α≈1.0. */
                if (mgr->pink_noise_dfa_feedback && mgr->pink_noise) {
                    neuromod_pink_noise_t* pn = (neuromod_pink_noise_t*)mgr->pink_noise;

                    /* Capture baseline amplitudes on first access */
                    if (!mgr->pink_noise_base_captured) {
                        mgr->pink_noise_base_amplitude[0] = pn->dopamine_noise_amplitude;
                        mgr->pink_noise_base_amplitude[1] = pn->serotonin_noise_amplitude;
                        mgr->pink_noise_base_amplitude[2] = pn->acetylcholine_noise_amplitude;
                        mgr->pink_noise_base_amplitude[3] = pn->norepinephrine_noise_amplitude;
                        mgr->pink_noise_base_captured = true;
                    }

                    /* Compute amplitude scale factor from health */
                    float amp_scale = 1.0f;
                    switch (mgr->training_health) {
                        case NIMCP_TRAINING_HEALTH_NOISY:
                            amp_scale = 0.6f;   /* Reduce noise — too much randomness */
                            break;
                        case NIMCP_TRAINING_HEALTH_OSCILLATING:
                            amp_scale = 0.3f;   /* Strongly reduce — chaotic dynamics */
                            break;
                        case NIMCP_TRAINING_HEALTH_DRIFTING:
                            amp_scale = 0.5f;   /* Reduce — diverging */
                            break;
                        case NIMCP_TRAINING_HEALTH_PLATEAU:
                            amp_scale = 2.0f;   /* Boost — escape local minimum */
                            break;
                        case NIMCP_TRAINING_HEALTH_OPTIMAL:
                            amp_scale = 1.0f;   /* Restore baseline — ideal regime */
                            break;
                        default:
                            break;
                    }

                    /* Apply scaled amplitudes (relative to captured baseline) */
                    pn->dopamine_noise_amplitude =
                        mgr->pink_noise_base_amplitude[0] * amp_scale;
                    pn->serotonin_noise_amplitude =
                        mgr->pink_noise_base_amplitude[1] * amp_scale;
                    pn->acetylcholine_noise_amplitude =
                        mgr->pink_noise_base_amplitude[2] * amp_scale;
                    pn->norepinephrine_noise_amplitude =
                        mgr->pink_noise_base_amplitude[3] * amp_scale;

                    if (amp_scale != 1.0f) {
                        NIMCP_LOGGING_INFO("UTM: DFA→PinkNoise feedback: health=%d, "
                            "amp_scale=%.1f (DA=%.3f, 5-HT=%.3f, ACh=%.3f, NE=%.3f)",
                            (int)mgr->training_health, amp_scale,
                            pn->dopamine_noise_amplitude,
                            pn->serotonin_noise_amplitude,
                            pn->acetylcholine_noise_amplitude,
                            pn->norepinephrine_noise_amplitude);
                    }
                }

                /* Feature 3: Extended fractal analysis */
                /* Lacunarity */
                if (count >= 8) {
                    mgr->lacunarity_value = fractal_lacunarity(linear, count, count / 8);
                }

                /* Multifractal spectrum */
                multifractal_spectrum_t* spectrum = NULL;
                if (fractal_multifractal_spectrum(linear, count,
                        -3.0f, 3.0f, 13, &spectrum) == 0 && spectrum) {
                    mgr->is_multifractal = spectrum->is_multifractal;
                    mgr->multifractal_width = spectrum->width;
                    multifractal_spectrum_destroy(spectrum);
                }

                /* Spectral exponent (supplementary) */
                fractal_result_t spectral_result = {0};
                fractal_spectral_exponent(linear, count, &spectral_result);

                /* Feature 5: Quantum annealing for plateau escape */
                if (mgr->training_health == NIMCP_TRAINING_HEALTH_PLATEAU) {
                    mgr->plateau_consecutive_count++;
                } else {
                    mgr->plateau_consecutive_count = 0;
                }

                if (mgr->enable_quantum_anneal &&
                    mgr->plateau_consecutive_count >= mgr->plateau_anneal_threshold) {

                    NIMCP_LOGGING_INFO("UTM: Triggering quantum annealing (plateau for %u checks)",
                                       mgr->plateau_consecutive_count);

                    quantum_annealing_config_t qa_cfg = quantum_annealing_default_config();
                    qa_cfg.num_iterations = 50;
                    qa_cfg.quantum_strength = 0.1f;
                    qa_cfg.enable_tunneling = true;
                    quantum_annealer_t annealer = quantum_annealer_create(&qa_cfg);
                    if (annealer) {
                        /* Perturb each network's first small param group */
                        for (uint32_t n = 0; n < mgr->num_networks; n++) {
                            if (!mgr->networks[n].enabled) continue;
                            nimcp_utm_param_group_t* pgroups = NULL;
                            uint32_t npg = 0;
                            if (mgr->networks[n].ops->get_param_groups &&
                                mgr->networks[n].ops->get_param_groups(
                                    mgr->networks[n].ctx, &pgroups, &npg) == 0 && npg > 0) {
                                /* Anneal first group if small enough */
                                if (pgroups[0].params && pgroups[0].count > 0 &&
                                    pgroups[0].count <= 4096) {
                                    float* optimized = (float*)nimcp_malloc(
                                        pgroups[0].count * sizeof(float));
                                    if (optimized) {
                                        quantum_anneal(annealer, NULL,
                                            pgroups[0].params, optimized,
                                            (uint32_t)pgroups[0].count, NULL);
                                        memcpy(pgroups[0].params, optimized,
                                            pgroups[0].count * sizeof(float));
                                        nimcp_free(optimized);
                                    }
                                }
                                nimcp_free(pgroups);
                            }
                        }
                        quantum_annealer_destroy(annealer);
                        mgr->plateau_consecutive_count = 0;
                    }
                }

                /* Feature 8: Phase coherence (cross-network mode collapse detection) */
                if (mgr->num_networks >= 2 && mgr->per_network_loss_history && count >= 16) {
                    float total_sync = 0.0f;
                    uint32_t pair_count = 0;

                    /* Allocate phasor buffers */
                    neural_phasor_t* phasors_a = (neural_phasor_t*)nimcp_malloc(
                        count * sizeof(neural_phasor_t));
                    neural_phasor_t* phasors_b = (neural_phasor_t*)nimcp_malloc(
                        count * sizeof(neural_phasor_t));
                    float* net_series = (float*)nimcp_malloc(count * sizeof(float));

                    if (phasors_a && phasors_b && net_series) {
                        for (uint32_t na = 0; na < mgr->num_networks; na++) {
                            /* Extract network na's loss history */
                            for (uint32_t k = 0; k < count; k++) {
                                uint32_t idx = (start + k) % mgr->loss_history_size;
                                net_series[k] = mgr->per_network_loss_history[
                                    idx * mgr->num_networks + na];
                            }
                            if (!phasor_hilbert_transform(net_series, phasors_a, count))
                                continue;

                            for (uint32_t nb = na + 1; nb < mgr->num_networks; nb++) {
                                for (uint32_t k = 0; k < count; k++) {
                                    uint32_t idx = (start + k) % mgr->loss_history_size;
                                    net_series[k] = mgr->per_network_loss_history[
                                        idx * mgr->num_networks + nb];
                                }
                                if (!phasor_hilbert_transform(net_series, phasors_b, count))
                                    continue;

                                float sync = phasor_array_synchrony(phasors_a, phasors_b, count);
                                total_sync += sync;
                                pair_count++;
                            }
                        }
                    }
                    nimcp_free(phasors_a);
                    nimcp_free(phasors_b);
                    nimcp_free(net_series);

                    if (pair_count > 0) {
                        mgr->cross_network_coherence = total_sync / (float)pair_count;
                        /* Override health if networks are locked together */
                        if (mgr->cross_network_coherence > 0.9f) {
                            mgr->training_health = NIMCP_TRAINING_HEALTH_OSCILLATING;
                        }
                    }
                }

                nimcp_free(linear);
            }
        }

        /* Feature 6: Bottleneck detection (less frequent) */
        if (mgr->bottleneck_check_interval > 0 &&
            mgr->step_count % mgr->bottleneck_check_interval == 0 &&
            mgr->num_bridges > 0) {
            for (uint32_t b = 0; b < mgr->num_bridges; b++) {
                nimcp_cross_network_bridge_t* br = &mgr->bridges[b];
                if (!br->enabled || !br->last_source_output || !br->last_target_input)
                    continue;

                /* Simple bottleneck metric: cosine similarity between source and target */
                float dot = 0.0f, norm_s = 0.0f, norm_t = 0.0f;
                uint32_t min_dim = (br->source_dim < br->target_dim) ?
                                    br->source_dim : br->target_dim;
                for (uint32_t j = 0; j < min_dim; j++) {
                    dot += br->last_source_output[j] * br->last_target_input[j];
                    norm_s += br->last_source_output[j] * br->last_source_output[j];
                    norm_t += br->last_target_input[j] * br->last_target_input[j];
                }
                float denom = sqrtf(norm_s) * sqrtf(norm_t);
                float similarity = (denom > 1e-7f) ? (dot / denom) : 0.0f;
                /* Low similarity = information bottleneck */
                mgr->bridge_bottleneck_severity[b] = 1.0f - fabsf(similarity);

                if (mgr->bridge_bottleneck_severity[b] > 0.7f) {
                    NIMCP_LOGGING_WARN("UTM: Bridge %u bottleneck severity %.2f",
                                        b, mgr->bridge_bottleneck_severity[b]);
                }
            }
        }
    }

    /* Feature 9: Manifold dimensionality tracking */
    if (mgr->manifold_tracking_enabled &&
        mgr->health_check_interval > 0 &&
        mgr->step_count % mgr->health_check_interval == 0) {
        for (uint32_t i = 0; i < mgr->num_networks; i++) {
            if (!mgr->networks[i].enabled || !net_outputs[i]) continue;
            uint32_t out_dim = net_output_dims[i];
            if (out_dim == 0) continue;

            /* Lazy-create manifold handle */
            if (!mgr->output_manifold[i]) {
                nimcp_manifold_config_t m_cfg = {0};
                m_cfg.intrinsic_dim = 0; /* auto-detect */
                m_cfg.num_samples = 64;
                m_cfg.neighborhood_radius = 0.1f;
                m_cfg.compute_curvature = false;
                m_cfg.enable_embedding = false;
                mgr->output_manifold[i] = nimcp_manifold_create(&m_cfg, out_dim);
            }

            /* Add current output as a sample */
            if (mgr->output_manifold[i]) {
                nimcp_manifold_add_samples(mgr->output_manifold[i],
                    net_outputs[i], 1, out_dim);

                /* Estimate intrinsic dimensionality */
                uint32_t dim = 0;
                if (nimcp_manifold_estimate_dim(mgr->output_manifold[i], &dim) == 0) {
                    mgr->manifold_intrinsic_dim[i] = dim;
                    if (dim < 2 && dim > 0) {
                        NIMCP_LOGGING_WARN("UTM: Network %u manifold dim collapsed to %u", i, dim);
                    }
                }
            }
        }
    }

    /* Populate extended result fields */
    local_result.training_health = mgr->training_health;
    local_result.dfa_exponent = mgr->dfa_exponent;
    local_result.hurst_exponent = mgr->hurst_exponent;
    local_result.lacunarity_value = mgr->lacunarity_value;
    local_result.is_multifractal = mgr->is_multifractal;
    local_result.cross_network_coherence = mgr->cross_network_coherence;
    memcpy(local_result.manifold_intrinsic_dim, mgr->manifold_intrinsic_dim,
           sizeof(mgr->manifold_intrinsic_dim));

    if (result) {
        *result = local_result;
    }

cleanup:
    /* Free output buffers */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        nimcp_free(net_outputs[i]);
    }

    /* UTM step timing breakdown (NIMCP_DEBUG_TIMING=1 to enable) */
    if (utm_debug_timing_enabled()) {
        struct timespec _utm_end; clock_gettime(CLOCK_MONOTONIC, &_utm_end);
        #define UTM_MS(a, b) (((b).tv_sec - (a).tv_sec) * 1000.0 + ((b).tv_nsec - (a).tv_nsec) / 1e6)
        static uint64_t _utm_step_count = 0;
        _utm_step_count++;
        double _bwd = UTM_MS(_utm_fwd_end, _utm_bwd_end);
        double _rest = UTM_MS(_utm_bwd_end, _utm_end);
        NIMCP_LOGGING_INFO("UTM step %llu: backward=%.0fms optimizer+state=%.0fms "
                           "(%u networks, %u bridges)",
                           (unsigned long long)_utm_step_count,
                           _bwd, _rest, mgr->num_networks, mgr->num_bridges);
        #undef UTM_MS
    }

    return 0;
}

//=============================================================================
// Fractal-Aware Training API
//=============================================================================

void nimcp_utm_set_fractal_lr(nimcp_unified_training_manager_t* mgr,
                               uint32_t net_idx, float scale) {
    if (!mgr || net_idx >= NIMCP_UTM_MAX_NETWORKS) return;
    mgr->fractal_lr_multiplier[net_idx] = scale;
    mgr->fractal_enabled = true;
}

nimcp_training_health_t nimcp_utm_get_health(const nimcp_unified_training_manager_t* mgr) {
    if (!mgr) return NIMCP_TRAINING_HEALTH_UNKNOWN;
    return mgr->training_health;
}

float nimcp_utm_get_dfa_exponent(const nimcp_unified_training_manager_t* mgr) {
    if (!mgr) return 0.0f;
    return mgr->dfa_exponent;
}

void nimcp_utm_set_natural_gradient(nimcp_unified_training_manager_t* mgr,
                                     uint32_t net_idx, bool enabled) {
    if (!mgr || net_idx >= NIMCP_UTM_MAX_NETWORKS) return;
    /* Toggle per-network by destroying/allowing lazy creation */
    if (!enabled && mgr->natural_grad[net_idx]) {
        nimcp_natural_grad_destroy(mgr->natural_grad[net_idx]);
        mgr->natural_grad[net_idx] = NULL;
    }
    if (!enabled && mgr->fisher[net_idx]) {
        nimcp_fisher_destroy(mgr->fisher[net_idx]);
        mgr->fisher[net_idx] = NULL;
    }
    /* If enabling, handles will be lazy-created in the optimizer loop */
}

//=============================================================================
// Extended Pipeline API (Items 1-14)
//=============================================================================

void nimcp_utm_set_amp(nimcp_unified_training_manager_t* mgr, void* amp_ctx) {
    if (!mgr) return;
    mgr->amp_ctx = amp_ctx;
}

void nimcp_utm_set_curriculum(nimcp_unified_training_manager_t* mgr, void* curriculum_ctx) {
    if (!mgr) return;
    mgr->curriculum_ctx = curriculum_ctx;
}

void nimcp_utm_set_kd(nimcp_unified_training_manager_t* mgr, void* kd_ctx) {
    if (!mgr) return;
    mgr->kd_ctx = kd_ctx;
}

void nimcp_utm_set_neuromod(nimcp_unified_training_manager_t* mgr, void* neuromod_adapter) {
    if (!mgr) return;
    mgr->neuromod_adapter = neuromod_adapter;
}

void nimcp_utm_set_pink_noise(nimcp_unified_training_manager_t* mgr, void* pink_noise) {
    if (!mgr) return;
    mgr->pink_noise = pink_noise;
    mgr->pink_noise_base_captured = false; /* Will capture on first health check */
}

void nimcp_utm_set_per_network_lr(nimcp_unified_training_manager_t* mgr,
                                    uint32_t net_idx, float lr) {
    if (!mgr || net_idx >= NIMCP_UTM_MAX_NETWORKS) return;
    mgr->per_network_lr[net_idx] = lr;
}

bool nimcp_utm_is_early_stopped(const nimcp_unified_training_manager_t* mgr) {
    if (!mgr) return false;
    return mgr->early_stopped;
}

void nimcp_utm_set_early_stopping_enabled(nimcp_unified_training_manager_t* mgr,
                                          bool enabled) {
    if (!mgr) return;
    mgr->early_stopping_enabled = enabled;
    /* Note: does not clear an already-tripped flag — caller must reset. */
}

void nimcp_utm_reset_early_stopping(nimcp_unified_training_manager_t* mgr) {
    if (!mgr) return;
    mgr->early_stopped = false;
    mgr->early_stopping_counter = 0;
    mgr->early_stopping_best_loss = INFINITY;
    NIMCP_LOGGING_INFO("UTM: early-stopping state reset (counter=0, best=+inf)");
}

int nimcp_utm_get_ema_params(const nimcp_unified_training_manager_t* mgr,
                               uint32_t group_idx, float* out_params, size_t count) {
    if (!mgr || !out_params || !mgr->ema_params) return -1;
    if (group_idx >= mgr->ema_num_groups) return -1;
    if (!mgr->ema_params[group_idx]) return -1;
    size_t copy_count = (count < mgr->ema_sizes[group_idx]) ?
        count : mgr->ema_sizes[group_idx];
    memcpy(out_params, mgr->ema_params[group_idx], copy_count * sizeof(float));
    return 0;
}

//=============================================================================
// Extended Pipeline API (15-Item Gap Analysis)
//=============================================================================

void nimcp_utm_set_cl(nimcp_unified_training_manager_t* mgr, cl_ctx_t* cl_ctx) {
    if (!mgr) return;
    mgr->cl_ctx = cl_ctx;
}

void nimcp_utm_set_adversarial(nimcp_unified_training_manager_t* mgr, adv_ctx_t* adv_ctx) {
    if (!mgr) return;
    mgr->adv_ctx = adv_ctx;
}

void nimcp_utm_set_callbacks(nimcp_unified_training_manager_t* mgr, tcb_context_t* tcb_ctx) {
    if (!mgr) return;
    mgr->tcb_ctx = tcb_ctx;
}

void nimcp_utm_set_checkpoint_mgr(nimcp_unified_training_manager_t* mgr,
                                    checkpoint_mgr_t* ckpt_mgr) {
    if (!mgr) return;
    mgr->checkpoint_mgr = ckpt_mgr;
}

int nimcp_utm_swap_to_ema(nimcp_unified_training_manager_t* mgr) {
    if (!mgr || !mgr->ema_params || !mgr->ema_enabled) return -1;
    if (mgr->ema_swapped_in) return 0; /* Already swapped */

    /* Swap live params ↔ EMA params for all groups */
    uint32_t g_idx = 0;
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        nimcp_utm_param_group_t* groups = NULL;
        uint32_t num_groups = 0;
        if (mgr->networks[i].ops->get_param_groups &&
            mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
            for (uint32_t g = 0; g < num_groups; g++) {
                if (groups[g].params && groups[g].count > 0 &&
                    g_idx < mgr->ema_num_groups && mgr->ema_params[g_idx]) {
                    /* In-place swap */
                    size_t count = groups[g].count;
                    if (count == mgr->ema_sizes[g_idx]) {
                        for (size_t j = 0; j < count; j++) {
                            float tmp = groups[g].params[j];
                            groups[g].params[j] = mgr->ema_params[g_idx][j];
                            mgr->ema_params[g_idx][j] = tmp;
                        }
                    }
                }
                g_idx++;
            }
            nimcp_free(groups);
        }
    }

    /* Sync swapped params to underlying networks */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        if (mgr->networks[i].ops->sync_params) {
            mgr->networks[i].ops->sync_params(mgr->networks[i].ctx);
        }
    }

    mgr->ema_swapped_in = true;
    return 0;
}

int nimcp_utm_swap_from_ema(nimcp_unified_training_manager_t* mgr) {
    if (!mgr || !mgr->ema_swapped_in) return -1;

    /* Swap back (same operation — EMA now holds live, live holds EMA) */
    uint32_t g_idx = 0;
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        nimcp_utm_param_group_t* groups = NULL;
        uint32_t num_groups = 0;
        if (mgr->networks[i].ops->get_param_groups &&
            mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
            for (uint32_t g = 0; g < num_groups; g++) {
                if (groups[g].params && groups[g].count > 0 &&
                    g_idx < mgr->ema_num_groups && mgr->ema_params[g_idx]) {
                    size_t count = groups[g].count;
                    if (count == mgr->ema_sizes[g_idx]) {
                        for (size_t j = 0; j < count; j++) {
                            float tmp = groups[g].params[j];
                            groups[g].params[j] = mgr->ema_params[g_idx][j];
                            mgr->ema_params[g_idx][j] = tmp;
                        }
                    }
                }
                g_idx++;
            }
            nimcp_free(groups);
        }
    }

    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        if (mgr->networks[i].ops->sync_params) {
            mgr->networks[i].ops->sync_params(mgr->networks[i].ctx);
        }
    }

    mgr->ema_swapped_in = false;
    return 0;
}

bool nimcp_utm_gradients_healthy(const nimcp_unified_training_manager_t* mgr) {
    if (!mgr) return true;
    /* Check all param groups for NaN/Inf */
    for (uint32_t i = 0; i < mgr->num_networks; i++) {
        if (!mgr->networks[i].enabled) continue;
        nimcp_utm_param_group_t* groups = NULL;
        uint32_t num_groups = 0;
        if (mgr->networks[i].ops->get_param_groups &&
            mgr->networks[i].ops->get_param_groups(mgr->networks[i].ctx, &groups, &num_groups) == 0) {
            for (uint32_t g = 0; g < num_groups; g++) {
                if (groups[g].gradients && groups[g].count > 0) {
                    nimcp_grad_health_t health = nimcp_gradient_check_health(
                        groups[g].gradients, groups[g].count);
                    if (health != NIMCP_GRAD_HEALTHY) {
                        nimcp_free(groups);
                        return false;
                    }
                }
            }
            nimcp_free(groups);
        }
    }
    return true;
}

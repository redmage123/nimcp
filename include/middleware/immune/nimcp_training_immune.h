/**
 * @file nimcp_training_immune.h
 * @brief Training-Immune System Integration for NIMCP
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and training pipeline
 * WHY:  Model biological learning suppression during illness and immune response
 *       to training instabilities
 * HOW:  Immune inflammation modulates learning rate; training divergence triggers
 *       immune response
 *
 * BIOLOGICAL BASIS:
 * ==================
 * During illness, the body suppresses learning and memory consolidation to:
 * 1. Conserve metabolic energy for immune response (fever = high energy cost)
 * 2. Prevent encoding corrupted/noisy sensory information during impaired states
 * 3. Protect neural plasticity mechanisms from inflammation-induced damage
 *
 * This is observed as:
 * - Reduced synaptic plasticity during infection (cytokine-mediated)
 * - Impaired memory formation during fever states
 * - Slower learning rates during inflammatory conditions
 *
 * NIMCP MAPPING:
 * ==============
 * ```
 * BIOLOGICAL MECHANISM              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Fever/inflammation severity    → Learning rate reduction factor
 * Cytokine IL-1β, IL-6, TNF-α    → Inflammation level (0-4)
 * Synaptic plasticity suppression→ Training step modulation
 * Immune activation energy cost  → Gradient scaling reduction
 * Training divergence/instability→ Antigen presentation (threat)
 * Loss explosion                 → Acute immune response trigger
 * Gradient norm explosion        → Inflammation escalation
 * ```
 *
 * ARCHITECTURE:
 * =============
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    TRAINING IMMUNE BRIDGE                        │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  IMMUNE → TRAINING (Inflammation modulates learning)            │
 * │  ┌────────────────┐           ┌─────────────────────────┐      │
 * │  │ Brain Immune   │           │   Training Pipeline     │      │
 * │  │ Inflammation   │──fever──→│   Learning Rate         │      │
 * │  │   Level (0-4)  │  factor  │   Modulation            │      │
 * │  └────────────────┘           └─────────────────────────┘      │
 * │         │                              │                        │
 * │         │ NONE      → LR × 1.0         │                        │
 * │         │ LOCAL     → LR × 0.95        │                        │
 * │         │ REGIONAL  → LR × 0.80        │                        │
 * │         │ SYSTEMIC  → LR × 0.50        │                        │
 * │         │ STORM     → LR × 0.10        │                        │
 * │         │                              │                        │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │         Gradient Manager Integration                        │ │
 * │  │  - Scale gradients by immune factor                        │ │
 * │  │  - Increase numerical stability during inflammation        │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                                                                  │
 * │  TRAINING → IMMUNE (Instabilities trigger response)             │
 * │  ┌─────────────────────────┐       ┌──────────────────┐        │
 * │  │   Training Metrics      │       │  Brain Immune    │        │
 * │  │   Loss, Grad Norm, etc. │─detect→│  Antigen         │        │
 * │  │   Divergence Detection  │ threat │  Presentation    │        │
 * │  └─────────────────────────┘       └──────────────────┘        │
 * │         │                                   │                   │
 * │         │ Loss NaN/Inf      → SEVERITY 10   │                   │
 * │         │ Loss explosion    → SEVERITY 8    │                   │
 * │         │ Grad explosion    → SEVERITY 6    │                   │
 * │         │ Loss plateau      → SEVERITY 3    │                   │
 * │         │                                   │                   │
 * │         └────────────→ Immune Response ─────┘                   │
 * │                       (Isolation, Recovery)                     │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * INTEGRATION POINTS:
 * ===================
 * 1. Optimizer: Adjust learning rate based on inflammation
 * 2. Gradient Manager: Scale gradients during immune response
 * 3. Training Callbacks: Monitor for training divergence
 * 4. Loss Functions: Detect anomalous loss values
 *
 * USAGE EXAMPLE:
 * ==============
 * ```c
 * // Create training immune bridge
 * training_immune_config_t config = training_immune_default_config();
 * training_immune_system_t* ti = training_immune_create(&config);
 *
 * // Connect to brain immune and training components
 * training_immune_connect_brain_immune(ti, brain_immune);
 * training_immune_connect_optimizer(ti, optimizer);
 * training_immune_connect_gradient_manager(ti, grad_mgr);
 * training_immune_connect_callbacks(ti, callbacks);
 *
 * // Start monitoring
 * training_immune_start(ti);
 *
 * // Training loop
 * for (int step = 0; step < max_steps; step++) {
 *     // Update metrics
 *     training_immune_update_metrics(ti, loss, grad_norm, lr);
 *
 *     // Check for divergence (auto-triggers immune response)
 *     training_immune_check_stability(ti);
 *
 *     // Get modulated learning rate
 *     float effective_lr = training_immune_get_effective_lr(ti, base_lr);
 *
 *     // Apply to optimizer
 *     nimcp_optimizer_set_lr(optimizer, effective_lr);
 *
 *     // ... perform training step ...
 * }
 * ```
 *
 * GOTCHAS:
 * ========
 * - Learning rate modulation is multiplicative, not additive
 * - Inflammation reduction is gradual (uses EMA)
 * - Immune response has minimum duration before LR restoration
 * - Gradient scaling affects both forward and backward passes
 *
 * NIMCP STANDARDS:
 * ================
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRAINING_IMMUNE_H
#define NIMCP_TRAINING_IMMUNE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Core dependencies */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_training_callbacks.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TRAINING_IMMUNE_MODULE_NAME      "training_immune"
#define TRAINING_IMMUNE_MAX_HISTORY      100  /**< Max training metric history */

/* Fever/inflammation learning rate reduction factors */
#define TRAINING_IMMUNE_LR_FACTOR_NONE      1.00f  /**< No inflammation */
#define TRAINING_IMMUNE_LR_FACTOR_LOCAL     0.95f  /**< Local inflammation */
#define TRAINING_IMMUNE_LR_FACTOR_REGIONAL  0.80f  /**< Regional inflammation */
#define TRAINING_IMMUNE_LR_FACTOR_SYSTEMIC  0.50f  /**< Systemic inflammation */
#define TRAINING_IMMUNE_LR_FACTOR_STORM     0.10f  /**< Cytokine storm */

/* Training divergence detection thresholds */
#define TRAINING_IMMUNE_LOSS_EXPLOSION_RATIO     10.0f  /**< Loss increase ratio for explosion */
#define TRAINING_IMMUNE_GRAD_EXPLOSION_THRESHOLD 100.0f /**< Gradient norm for explosion */
#define TRAINING_IMMUNE_LOSS_PLATEAU_STEPS       50     /**< Steps without improvement */

/* Immune response delays */
#define TRAINING_IMMUNE_MIN_RESPONSE_DURATION_MS 5000   /**< Min immune response time */
#define TRAINING_IMMUNE_INFLAMMATION_EMA_ALPHA   0.1f   /**< EMA smoothing for inflammation */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct training_immune_system training_immune_system_t;
typedef struct perception_training_bridge perception_training_bridge_t;
typedef struct cortical_training_bridge cortical_training_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Training instability types that trigger immune response
 *
 * BIOLOGICAL BASIS:
 * Different training failures map to different threat types
 */
typedef enum {
    TRAINING_INSTABILITY_NONE = 0,      /**< Stable training */
    TRAINING_INSTABILITY_LOSS_NAN,      /**< Loss is NaN */
    TRAINING_INSTABILITY_LOSS_INF,      /**< Loss is infinite */
    TRAINING_INSTABILITY_LOSS_EXPLOSION,/**< Loss increased rapidly */
    TRAINING_INSTABILITY_GRAD_EXPLOSION,/**< Gradients exploded */
    TRAINING_INSTABILITY_GRAD_VANISHING,/**< Gradients vanished */
    TRAINING_INSTABILITY_LOSS_PLATEAU,  /**< Loss stopped improving */
    TRAINING_INSTABILITY_OSCILLATION,   /**< Loss oscillating wildly */
    /* Perception-based instabilities */
    TRAINING_INSTABILITY_PERCEPTION_COLLAPSE,   /**< All modality confidences near 0 */
    /* Cortical-based instabilities */
    TRAINING_INSTABILITY_CORTICAL_EXPLOSION,    /**< Free energy explosion */
    TRAINING_INSTABILITY_PREDICTION_FAILURE,    /**< Burst rate collapse */
    TRAINING_INSTABILITY_COUNT
} training_instability_type_t;

/**
 * @brief Training immune system phase
 */
typedef enum {
    TRAINING_IMMUNE_PHASE_HEALTHY = 0,  /**< Normal training, no immune response */
    TRAINING_IMMUNE_PHASE_MONITORING,   /**< Watching for instabilities */
    TRAINING_IMMUNE_PHASE_RESPONDING,   /**< Active immune response */
    TRAINING_IMMUNE_PHASE_RECOVERING,   /**< Inflammation resolving */
    TRAINING_IMMUNE_PHASE_RESOLVED      /**< Back to normal */
} training_immune_phase_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Training metrics snapshot for immune system
 *
 * WHAT: Current training state for divergence detection
 * WHY:  Immune system needs training context to assess health
 */
typedef struct {
    uint64_t step;                      /**< Current training step */
    uint64_t epoch;                     /**< Current epoch */

    /* Loss metrics */
    float loss;                         /**< Current loss value */
    float loss_prev;                    /**< Previous loss */
    float loss_min;                     /**< Minimum loss seen */
    float loss_ema;                     /**< Exponential moving average */
    float loss_variance;                /**< Loss variance */

    /* Gradient metrics */
    float grad_norm;                    /**< Gradient L2 norm */
    float grad_max;                     /**< Max gradient value */
    uint32_t grad_clips;                /**< Gradients clipped count */

    /* Learning rate */
    float learning_rate;                /**< Base learning rate */
    float effective_lr;                 /**< After immune modulation */

    /* Flags */
    bool has_nan;                       /**< NaN detected */
    bool has_inf;                       /**< Inf detected */
    bool is_exploding;                  /**< Gradient explosion */
    bool is_vanishing;                  /**< Gradient vanishing */
    bool is_plateau;                    /**< Loss plateau */

    uint64_t timestamp_ms;              /**< Metric timestamp */
} training_immune_metrics_t;

/**
 * @brief Training instability event
 *
 * WHAT: Detected training problem that triggers immune response
 * WHY:  Map training failures to immune antigens
 */
typedef struct {
    uint32_t event_id;                  /**< Unique event ID */
    training_instability_type_t type;   /**< Type of instability */
    uint32_t severity;                  /**< Severity (1-10) */
    float confidence;                   /**< Detection confidence (0-1) */

    /* Metrics at time of detection */
    training_immune_metrics_t metrics;

    /* Immune response */
    uint32_t antigen_id;                /**< Brain immune antigen ID */
    bool immune_triggered;              /**< Whether immune response activated */

    uint64_t detection_time_ms;         /**< When detected */
} training_instability_event_t;

/**
 * @brief Configuration for training immune system
 */
typedef struct {
    /* Immune modulation settings */
    bool enable_lr_modulation;          /**< Enable learning rate modulation */
    bool enable_grad_scaling;           /**< Enable gradient scaling */
    float min_lr_factor;                /**< Minimum LR factor (safety) */

    /* Custom LR factors per inflammation level */
    float lr_factor_local;              /**< LR factor for local inflammation */
    float lr_factor_regional;           /**< LR factor for regional inflammation */
    float lr_factor_systemic;           /**< LR factor for systemic inflammation */
    float lr_factor_storm;              /**< LR factor for cytokine storm */

    /* Divergence detection thresholds */
    float loss_explosion_ratio;         /**< Loss increase ratio for explosion */
    float grad_explosion_threshold;     /**< Gradient norm for explosion */
    float grad_vanishing_threshold;     /**< Gradient norm for vanishing */
    uint32_t plateau_steps;             /**< Steps without improvement = plateau */
    float plateau_delta;                /**< Min improvement to reset plateau */

    /* Immune response settings */
    bool enable_auto_immune_response;   /**< Auto-trigger immune on divergence */
    uint64_t min_response_duration_ms;  /**< Min immune response time */
    float inflammation_ema_alpha;       /**< EMA smoothing for inflammation */

    /* Monitoring */
    uint32_t history_size;              /**< Training history buffer size */
    bool enable_logging;                /**< Enable detailed logging */

    /* Integration handles (set via connect functions) */
    bool has_brain_immune;              /**< Brain immune connected */
    bool has_optimizer;                 /**< Optimizer connected */
    bool has_gradient_manager;          /**< Gradient manager connected */
    bool has_callbacks;                 /**< Training callbacks connected */
    bool has_perception_training;       /**< Perception training connected */
    bool has_cortical_training;         /**< Cortical training connected */

    /* Perception/Cortical instability thresholds */
    float perception_collapse_threshold;/**< Perception confidence for collapse (default: 0.1) */
    float cortical_fe_explosion_threshold; /**< Free energy explosion threshold (default: 100.0) */
    float cortical_burst_collapse_threshold; /**< Burst rate collapse threshold (default: 0.1) */
} training_immune_config_t;

/**
 * @brief Training immune system statistics
 */
typedef struct {
    /* Instability detections */
    uint64_t total_instabilities;
    uint64_t instabilities_by_type[TRAINING_INSTABILITY_COUNT];

    /* Immune responses */
    uint64_t immune_responses_triggered;
    uint64_t total_immune_duration_ms;
    float avg_immune_duration_ms;

    /* Learning rate modulation */
    uint64_t lr_modulations;
    float avg_lr_reduction_factor;
    float min_effective_lr;

    /* Health metrics */
    uint64_t healthy_steps;
    uint64_t inflamed_steps;
    float time_in_inflammation_pct;

    /* Current state */
    training_immune_phase_t current_phase;
    brain_inflammation_level_t current_inflammation;
    float current_lr_factor;

    /* Perception/Cortical integration stats */
    bool perception_training_connected;
    bool cortical_training_connected;
    uint64_t perception_collapse_count;
    uint64_t cortical_explosion_count;
    uint64_t prediction_failure_count;
} training_immune_stats_t;

/* ============================================================================
 * Main System Structure
 * ============================================================================ */

/**
 * @brief Training immune system state
 */
struct training_immune_system {
    training_immune_config_t config;    /**< Configuration */
    training_immune_phase_t phase;      /**< Current phase */

    /* Integration handles */
    brain_immune_system_t* brain_immune;         /**< Brain immune system */
    nimcp_optimizer_context_t* optimizer;        /**< Optimizer */
    nimcp_gradient_manager_ctx_t* grad_manager;  /**< Gradient manager */
    tcb_context_t* callbacks;                     /**< Training callbacks */
    perception_training_bridge_t* perception_training; /**< Perception training bridge */
    cortical_training_bridge_t* cortical_training;     /**< Cortical training bridge */

    /* Current metrics */
    training_immune_metrics_t current_metrics;
    training_immune_metrics_t* history;          /**< Metric history */
    size_t history_count;
    size_t history_capacity;
    size_t history_index;

    /* Instability tracking */
    training_instability_event_t* events;        /**< Instability events */
    size_t event_count;
    size_t event_capacity;
    uint32_t next_event_id;

    /* Immune state */
    brain_inflammation_level_t inflammation;     /**< Current inflammation */
    float inflammation_ema;                      /**< Smoothed inflammation */
    float current_lr_factor;                     /**< Current LR multiplier */
    uint64_t immune_response_start_ms;           /**< When immune response began */
    bool in_immune_response;                     /**< Active immune response */

    /* Plateau detection */
    float best_loss;                             /**< Best loss seen */
    uint32_t steps_without_improvement;          /**< Plateau counter */

    /* Statistics */
    training_immune_stats_t stats;

    /* Thread safety */
    void* mutex;                                 /**< Platform mutex */

    /* State */
    bool running;                                /**< System active */
    uint64_t start_time_ms;                      /**< System start time */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int training_immune_default_config(training_immune_config_t* config);

/**
 * @brief Create training immune system
 *
 * WHAT: Initialize training-immune integration
 * WHY:  Set up bidirectional coordination
 * HOW:  Allocate state, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @return New system or NULL on failure
 */
training_immune_system_t* training_immune_create(
    const training_immune_config_t* config
);

/**
 * @brief Destroy training immune system
 *
 * WHAT: Clean up resources
 * WHY:  Proper resource deallocation
 * HOW:  Free buffers, unregister callbacks
 *
 * @param system System to destroy
 */
void training_immune_destroy(training_immune_system_t* system);

/**
 * @brief Start training immune monitoring
 *
 * WHAT: Activate monitoring and modulation
 * WHY:  Begin immune-training coordination
 * HOW:  Register with connected components
 *
 * @param system Training immune system
 * @return 0 on success
 */
int training_immune_start(training_immune_system_t* system);

/**
 * @brief Stop training immune system
 *
 * WHAT: Deactivate system
 * WHY:  Graceful shutdown
 * HOW:  Unregister handlers, complete pending responses
 *
 * @param system Training immune system
 * @return 0 on success
 */
int training_immune_stop(training_immune_system_t* system);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Link to brain immune for inflammation monitoring
 * WHY:  Receive inflammation state to modulate learning
 * HOW:  Store handle, register inflammation callback
 *
 * @param system Training immune system
 * @param brain_immune Brain immune system
 * @return 0 on success
 */
int training_immune_connect_brain_immune(
    training_immune_system_t* system,
    brain_immune_system_t* brain_immune
);

/**
 * @brief Connect to optimizer
 *
 * WHAT: Link to optimizer for LR modulation
 * WHY:  Apply learning rate adjustments
 * HOW:  Store handle for LR updates
 *
 * @param system Training immune system
 * @param optimizer Optimizer context
 * @return 0 on success
 */
int training_immune_connect_optimizer(
    training_immune_system_t* system,
    nimcp_optimizer_context_t* optimizer
);

/**
 * @brief Connect to gradient manager
 *
 * WHAT: Link to gradient manager for scaling
 * WHY:  Adjust gradient processing during inflammation
 * HOW:  Store handle, apply scaling factors
 *
 * @param system Training immune system
 * @param grad_manager Gradient manager context
 * @return 0 on success
 */
int training_immune_connect_gradient_manager(
    training_immune_system_t* system,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Connect to training callbacks
 *
 * WHAT: Link to callback system for monitoring
 * WHY:  Auto-detect training instabilities
 * HOW:  Register divergence detection callbacks
 *
 * @param system Training immune system
 * @param callbacks Training callback context
 * @return 0 on success
 */
int training_immune_connect_callbacks(
    training_immune_system_t* system,
    tcb_context_t* callbacks
);

/**
 * @brief Connect to perception training bridge
 *
 * WHAT: Link to perception training for instability detection
 * WHY:  Perception collapse threatens training integrity
 * HOW:  Store handle, monitor perception effects for collapse
 *
 * BIOLOGICAL BASIS: Sensory deprivation or collapse threatens learning.
 * Total perceptual failure is an immune threat requiring response.
 *
 * @param system Training immune system
 * @param perception_training Perception training bridge (NULL to disconnect)
 * @return 0 on success
 */
int training_immune_connect_perception_training(
    training_immune_system_t* system,
    perception_training_bridge_t* perception_training
);

/**
 * @brief Connect to cortical training bridge
 *
 * WHAT: Link to cortical training for instability detection
 * WHY:  Free energy explosion and burst collapse threaten training
 * HOW:  Store handle, monitor cortical effects for anomalies
 *
 * BIOLOGICAL BASIS: Cortical instability (free energy explosion, burst
 * collapse) indicates neural dysfunction requiring immune intervention.
 *
 * @param system Training immune system
 * @param cortical_training Cortical training bridge (NULL to disconnect)
 * @return 0 on success
 */
int training_immune_connect_cortical_training(
    training_immune_system_t* system,
    cortical_training_bridge_t* cortical_training
);

/* ============================================================================
 * Immune → Training: Inflammation Modulates Learning
 * ============================================================================ */

/**
 * @brief Update inflammation state from brain immune
 *
 * WHAT: Receive inflammation level from brain immune
 * WHY:  Reflect immune state in learning modulation
 * HOW:  Update internal state, compute LR factor
 *
 * @param system Training immune system
 * @param inflammation Current inflammation level
 * @return 0 on success
 */
int training_immune_update_inflammation(
    training_immune_system_t* system,
    brain_inflammation_level_t inflammation
);

/**
 * @brief Get effective learning rate (after immune modulation)
 *
 * WHAT: Compute learning rate adjusted for inflammation
 * WHY:  Reduce learning during immune response
 * HOW:  Multiply base LR by inflammation-dependent factor
 *
 * @param system Training immune system
 * @param base_lr Base learning rate
 * @return Effective learning rate
 */
float training_immune_get_effective_lr(
    const training_immune_system_t* system,
    float base_lr
);

/**
 * @brief Get current learning rate reduction factor
 *
 * WHAT: Get multiplicative factor for LR
 * WHY:  Allow external components to apply modulation
 * HOW:  Return current factor based on inflammation
 *
 * @param system Training immune system
 * @return LR factor (0-1, where 1.0 = no reduction)
 */
float training_immune_get_lr_factor(
    const training_immune_system_t* system
);

/**
 * @brief Apply immune modulation to optimizer
 *
 * WHAT: Update optimizer LR based on inflammation
 * WHY:  Automatic LR adjustment during immune response
 * HOW:  Get effective LR, set via optimizer API
 *
 * @param system Training immune system
 * @return 0 on success
 */
int training_immune_apply_lr_modulation(
    training_immune_system_t* system
);

/**
 * @brief Get gradient scaling factor for inflammation
 *
 * WHAT: Get gradient scaling factor based on immune state
 * WHY:  Increase numerical stability during inflammation
 * HOW:  Return scaling factor for gradient manager
 *
 * @param system Training immune system
 * @return Gradient scaling factor
 */
float training_immune_get_gradient_scale(
    const training_immune_system_t* system
);

/* ============================================================================
 * Training → Immune: Instability Detection
 * ============================================================================ */

/**
 * @brief Update training metrics
 *
 * WHAT: Record current training state
 * WHY:  Track metrics for divergence detection
 * HOW:  Store metrics in history buffer
 *
 * @param system Training immune system
 * @param loss Current loss value
 * @param grad_norm Gradient norm
 * @param learning_rate Current learning rate
 * @return 0 on success
 */
int training_immune_update_metrics(
    training_immune_system_t* system,
    float loss,
    float grad_norm,
    float learning_rate
);

/**
 * @brief Check training stability
 *
 * WHAT: Detect training divergence/instability
 * WHY:  Auto-trigger immune response to instabilities
 * HOW:  Analyze metrics for explosion, plateau, etc.
 *
 * @param system Training immune system
 * @return Detected instability type (NONE if stable)
 */
training_instability_type_t training_immune_check_stability(
    training_immune_system_t* system
);

/**
 * @brief Report training instability
 *
 * WHAT: Manually report instability event
 * WHY:  Allow external detection to trigger immune response
 * HOW:  Create event, present as antigen to brain immune
 *
 * @param system Training immune system
 * @param type Instability type
 * @param severity Severity (1-10)
 * @param event_id Output: instability event ID
 * @return 0 on success
 */
int training_immune_report_instability(
    training_immune_system_t* system,
    training_instability_type_t type,
    uint32_t severity,
    uint32_t* event_id
);

/**
 * @brief Trigger immune response to training instability
 *
 * WHAT: Present training instability as immune antigen
 * WHY:  Activate brain immune system for training problem
 * HOW:  Convert instability to epitope, present to brain immune
 *
 * @param system Training immune system
 * @param event_id Instability event ID
 * @return 0 on success
 */
int training_immune_trigger_immune_response(
    training_immune_system_t* system,
    uint32_t event_id
);

/**
 * @brief Check perception/cortical stability
 *
 * WHAT: Detect instabilities from connected perception/cortical bridges
 * WHY:  Auto-trigger immune response to perception/cortical failures
 * HOW:  Query perception/cortical effects, check against thresholds
 *
 * RETURNS: Detected instability type if any (may be PERCEPTION_COLLAPSE,
 *          CORTICAL_EXPLOSION, or PREDICTION_FAILURE)
 *
 * @param system Training immune system
 * @return Detected instability type (NONE if stable)
 */
training_instability_type_t training_immune_check_perception_cortical_stability(
    training_immune_system_t* system
);

/**
 * @brief Get perception sensitivity factor based on inflammation
 *
 * WHAT: Compute perception sensitivity modulation from immune state
 * WHY:  Inflammation reduces perception sensitivity to conserve resources
 * HOW:  Maps inflammation level to sensitivity factor [0.3-1.0]
 *
 * BIOLOGICAL BASIS: During illness (inflammation), sensory sensitivity
 * decreases as resources are conserved for immune response.
 *
 * @param system Training immune system
 * @return Perception sensitivity factor [0.3-1.0]
 */
float training_immune_get_perception_sensitivity(
    const training_immune_system_t* system
);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get current phase
 *
 * @param system Training immune system
 * @return Current phase
 */
training_immune_phase_t training_immune_get_phase(
    const training_immune_system_t* system
);

/**
 * @brief Get current inflammation level
 *
 * @param system Training immune system
 * @return Current inflammation level
 */
brain_inflammation_level_t training_immune_get_inflammation(
    const training_immune_system_t* system
);

/**
 * @brief Check if in immune response
 *
 * @param system Training immune system
 * @return true if active immune response
 */
bool training_immune_is_responding(
    const training_immune_system_t* system
);

/**
 * @brief Get training immune statistics
 *
 * @param system Training immune system
 * @param stats Output statistics
 * @return 0 on success
 */
int training_immune_get_stats(
    const training_immune_system_t* system,
    training_immune_stats_t* stats
);

/**
 * @brief Get current metrics
 *
 * @param system Training immune system
 * @param metrics Output current metrics
 * @return 0 on success
 */
int training_immune_get_current_metrics(
    const training_immune_system_t* system,
    training_immune_metrics_t* metrics
);

/**
 * @brief Get instability event by ID
 *
 * @param system Training immune system
 * @param event_id Event ID
 * @return Event or NULL if not found
 */
const training_instability_event_t* training_immune_get_event(
    const training_immune_system_t* system,
    uint32_t event_id
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* training_immune_phase_to_string(training_immune_phase_t phase);
const char* training_instability_type_to_string(training_instability_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_IMMUNE_H */

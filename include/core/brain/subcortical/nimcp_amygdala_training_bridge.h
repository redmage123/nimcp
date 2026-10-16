/**
 * @file nimcp_amygdala_training_bridge.h
 * @brief Amygdala-Training Integration Bridge for NIMCP
 * @version 1.0.0
 * @date 2025-12-22
 *
 * WHAT: Bidirectional integration between amygdala (emotional state) and training pipeline
 * WHY:  Model biological emotion-learning coupling and training-induced stress responses
 * HOW:  Amygdala arousal modulates learning rate via Yerkes-Dodson; training instabilities
 *       trigger amygdala threat responses
 *
 * BIOLOGICAL BASIS:
 * ==================
 * Emotional arousal has a well-established inverted-U relationship with learning
 * (Yerkes-Dodson Law):
 * 1. Low arousal → Poor learning (insufficient engagement/motivation)
 * 2. Moderate arousal → Optimal learning (alertness + focus)
 * 3. High arousal → Impaired learning (anxiety disrupts executive function)
 *
 * This manifests as:
 * - Norepinephrine release modulates synaptic plasticity (inverted-U)
 * - Fear/anxiety can enhance simple conditioning but impairs complex learning
 * - Stress triggers amygdala activation, which can suppress hippocampal LTP
 * - Training failures/instabilities induce cognitive stress responses
 *
 * NIMCP MAPPING:
 * ==============
 * ```
 * BIOLOGICAL MECHANISM              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Amygdala arousal/fear level    → Learning rate modulation (Yerkes-Dodson)
 * Moderate arousal (optimal)     → LR multiplier = 1.0 (at arousal=0.5)
 * Low arousal (disengagement)    → LR multiplier < 1.0 (at arousal=0.0)
 * High arousal (anxiety)         → LR multiplier < 1.0 (at arousal=1.0)
 * Fear conditioning enhancement  → Threat-related learning boost
 * Training divergence/failure    → Amygdala threat response
 * Loss explosion/NaN             → High fear activation
 * ```
 *
 * ARCHITECTURE:
 * =============
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                  AMYGDALA-TRAINING BRIDGE                        │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  AMYGDALA → TRAINING (Yerkes-Dodson LR modulation)              │
 * │  ┌────────────────┐           ┌─────────────────────────┐      │
 * │  │   Amygdala     │           │   Training Pipeline     │      │
 * │  │   Arousal      │─arousal──→│   Learning Rate         │      │
 * │  │   Level (0-1)  │  factor   │   Modulation            │      │
 * │  └────────────────┘           └─────────────────────────┘      │
 * │         │                              │                        │
 * │         │ Arousal=0.0 → LR × 0.50      │                        │
 * │         │ Arousal=0.5 → LR × 1.00      │ (optimal)              │
 * │         │ Arousal=1.0 → LR × 0.50      │                        │
 * │         │                              │                        │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │    Threat Learning Enhancement                              │ │
 * │  │  - Fear context → 2x learning boost for threat patterns    │ │
 * │  │  - Simple conditioning enhanced under stress               │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                                                                  │
 * │  TRAINING → AMYGDALA (Instability triggers threat)              │
 * │  ┌─────────────────────────┐       ┌──────────────────┐        │
 * │  │   Training Metrics      │       │    Amygdala      │        │
 * │  │   Loss, Grad Norm, etc. │─detect→│    Threat        │        │
 * │  │   Divergence Detection  │ threat │    Response      │        │
 * │  └─────────────────────────┘       └──────────────────┘        │
 * │         │                                   │                   │
 * │         │ Loss NaN/Inf      → FEAR +0.7     │                   │
 * │         │ Loss explosion    → FEAR +0.5     │                   │
 * │         │ Grad explosion    → FEAR +0.3     │                   │
 * │         │ Training plateau  → ANXIETY +0.1  │                   │
 * │         │                                   │                   │
 * │         └────────────→ Amygdala State ─────┘                    │
 * │                       (Fear, Anxiety)                           │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * YERKES-DODSON CURVE:
 * ====================
 * Learning rate modulation follows inverted-U:
 * ```
 * lr_factor = 1.0 - 4.0 * (arousal - 0.5)^2
 *
 * Arousal  | LR Factor | Effect
 * ---------|-----------|----------------------------------
 * 0.0      | 0.00      | No learning (too disengaged)
 * 0.25     | 0.75      | Sub-optimal (insufficient arousal)
 * 0.5      | 1.00      | Optimal (peak performance)
 * 0.75     | 0.75      | Sub-optimal (anxiety interfering)
 * 1.0      | 0.00      | Panic (executive function collapse)
 * ```
 *
 * INTEGRATION POINTS:
 * ===================
 * 1. Optimizer: Adjust learning rate based on amygdala arousal
 * 2. Training callbacks: Monitor for training divergence
 * 3. Amygdala: Trigger threat response on training failures
 * 4. Fear conditioning: Enhanced learning for threat-related patterns
 *
 * USAGE EXAMPLE:
 * ==============
 * ```c
 * // Create amygdala-training bridge
 * amygdala_training_config_t config = amygdala_training_default_config();
 * amygdala_training_bridge_t* bridge = amygdala_training_create(&config);
 *
 * // Connect to amygdala and training components
 * amygdala_training_connect_amygdala(bridge, amygdala);
 * amygdala_training_connect_training(bridge, training_ctx);
 * amygdala_training_connect_optimizer(bridge, optimizer);
 *
 * // Training loop
 * for (int step = 0; step < max_steps; step++) {
 *     // Update bridge from amygdala state
 *     amygdala_training_update(bridge);
 *
 *     // Get arousal-modulated learning rate
 *     float lr_multiplier = amygdala_training_get_lr_multiplier(bridge);
 *     float effective_lr = base_lr * lr_multiplier;
 *
 *     // Apply to optimizer
 *     nimcp_optimizer_set_lr(optimizer, effective_lr);
 *
 *     // ... perform training step ...
 *
 *     // Detect instabilities and trigger amygdala response
 *     if (isnan(loss)) {
 *         amygdala_training_on_instability(bridge, TRAINING_INSTABILITY_LOSS_NAN, 10);
 *     }
 * }
 * ```
 *
 * GOTCHAS:
 * ========
 * - Yerkes-Dodson curve is symmetric around arousal=0.5
 * - LR modulation is multiplicative, not additive
 * - Threat learning boost only applies when fear level > threshold
 * - Amygdala fear response is cumulative (decays over time)
 * - Zero arousal → zero learning (not just reduced)
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

#ifndef NIMCP_AMYGDALA_TRAINING_BRIDGE_H
#define NIMCP_AMYGDALA_TRAINING_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Core dependencies */
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "middleware/immune/nimcp_training_immune.h"
#include "middleware/training/nimcp_optimizers.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define AMYGDALA_TRAINING_MODULE_NAME      "amygdala_training"

/* Yerkes-Dodson curve parameters */
#define AMYGDALA_TRAINING_OPTIMAL_AROUSAL   0.5f   /**< Peak learning arousal */
#define AMYGDALA_TRAINING_CURVE_SHARPNESS   4.0f   /**< Inverted-U sharpness */

/* Threat learning enhancement */
#define AMYGDALA_TRAINING_THREAT_LEARNING_BOOST  2.0f   /**< LR multiplier for threat patterns */
#define AMYGDALA_TRAINING_THREAT_FEAR_THRESHOLD  0.4f   /**< Fear level for threat boost */

/* Instability fear responses */
#define AMYGDALA_TRAINING_FEAR_NAN           0.7f   /**< Fear increase for NaN */
#define AMYGDALA_TRAINING_FEAR_INF           0.7f   /**< Fear increase for Inf */
#define AMYGDALA_TRAINING_FEAR_EXPLOSION     0.5f   /**< Fear increase for explosion */
#define AMYGDALA_TRAINING_FEAR_GRAD_EXPLOSION 0.3f   /**< Fear increase for gradient explosion */
#define AMYGDALA_TRAINING_ANXIETY_PLATEAU    0.1f   /**< Anxiety increase for plateau */

/* State decay */
#define AMYGDALA_TRAINING_FEAR_DECAY_RATE    0.01f  /**< Fear decay per update */
#define AMYGDALA_TRAINING_MIN_LR_FACTOR      0.0f   /**< Minimum LR factor (safety) */

/* Bio-async */
#define AMYGDALA_TRAINING_BIO_INBOX_CAPACITY 32     /**< Message queue size */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct amygdala_training_bridge amygdala_training_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Amygdala-training integration phase
 */
typedef enum {
    AMYGDALA_TRAINING_PHASE_INACTIVE = 0,  /**< Bridge not active */
    AMYGDALA_TRAINING_PHASE_MONITORING,    /**< Monitoring amygdala state */
    AMYGDALA_TRAINING_PHASE_MODULATING,    /**< Actively modulating training */
    AMYGDALA_TRAINING_PHASE_RESPONDING     /**< Responding to instability */
} amygdala_training_phase_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Configuration for amygdala-training bridge
 */
typedef struct {
    /* Yerkes-Dodson parameters */
    float optimal_arousal;            /**< Arousal for peak learning (default: 0.5) */
    float curve_sharpness;            /**< Inverted-U sharpness (default: 4.0) */
    bool enable_yerkes_dodson;        /**< Enable arousal-LR modulation */

    /* Threat learning */
    float threat_learning_boost;      /**< LR multiplier for threat patterns (default: 2.0) */
    float threat_fear_threshold;      /**< Fear level for threat boost (default: 0.4) */
    bool enable_threat_learning;      /**< Enable threat learning enhancement */

    /* Instability responses */
    float fear_nan;                   /**< Fear increase for NaN (default: 0.7) */
    float fear_inf;                   /**< Fear increase for Inf (default: 0.7) */
    float fear_explosion;             /**< Fear increase for loss explosion (default: 0.5) */
    float fear_grad_explosion;        /**< Fear increase for grad explosion (default: 0.3) */
    float anxiety_plateau;            /**< Anxiety increase for plateau (default: 0.1) */
    bool enable_instability_response; /**< Enable training instability responses */

    /* State decay */
    float fear_decay_rate;            /**< Fear decay per update (default: 0.01) */
    float min_lr_factor;              /**< Minimum LR factor (default: 0.0) */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    uint32_t bio_inbox_capacity;      /**< Message queue size */

    /* Logging */
    bool enable_logging;              /**< Enable detailed logging */
} amygdala_training_config_t;

/**
 * @brief Amygdala-training bridge state
 */
struct amygdala_training_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Connected systems */
    amygdala_t* amygdala;                      /**< Amygdala instance */
    void* training_system;                      /**< Training context (opaque) */
    nimcp_optimizer_context_t* optimizer;       /**< Optimizer instance */

    /* Configuration */
    amygdala_training_config_t config;

    /* Current state */
    amygdala_training_phase_t phase;
    float arousal_level;                        /**< Current arousal [0-1] */
    float fear_level;                           /**< Current fear [0-1] */
    float anxiety_level;                        /**< Current anxiety [0-1] */
    float lr_modulation;                        /**< Current LR multiplier */
    float threat_learning_boost_active;         /**< Active threat boost */

    /* Instability tracking */
    bool instability_detected;
    uint32_t instability_count;
    uint64_t last_instability_ms;

    /* Connection state */
    bool amygdala_connected;
    bool training_connected;
    bool optimizer_connected;

    /* Statistics */
    uint64_t total_updates;
    uint64_t total_instabilities;
    float avg_arousal;
    float avg_lr_modulation;

    /* Timestamps */
    uint64_t creation_time_ms;
    uint64_t last_update_ms;
};

/**
 * @brief Amygdala-training statistics
 */
typedef struct {
    /* Current state */
    amygdala_training_phase_t current_phase;
    float current_arousal;
    float current_fear;
    float current_anxiety;
    float current_lr_modulation;
    float current_threat_boost;

    /* Counts */
    uint64_t total_updates;
    uint64_t total_instabilities;
    uint64_t nan_detections;
    uint64_t inf_detections;
    uint64_t explosion_detections;
    uint64_t plateau_detections;

    /* Averages */
    float avg_arousal;
    float avg_lr_modulation;
    float min_lr_modulation;
    float max_lr_modulation;

    /* Connection status */
    bool amygdala_connected;
    bool training_connected;
    bool optimizer_connected;
    bool bio_async_connected;

    /* Timing */
    uint64_t uptime_ms;
} amygdala_training_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with Yerkes-Dodson parameters
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int amygdala_training_default_config(amygdala_training_config_t* config);

/**
 * @brief Create amygdala-training bridge
 *
 * WHAT: Initialize amygdala-training integration
 * WHY:  Set up bidirectional coordination
 * HOW:  Allocate state, initialize mutex, register bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
amygdala_training_bridge_t* amygdala_training_create(
    const amygdala_training_config_t* config
);

/**
 * @brief Destroy amygdala-training bridge
 *
 * WHAT: Clean up resources
 * WHY:  Proper resource deallocation
 * HOW:  Free mutex, unregister bio-async, free structure
 *
 * @param bridge Bridge to destroy
 */
void amygdala_training_destroy(amygdala_training_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to amygdala
 *
 * WHAT: Link to amygdala for emotional state monitoring
 * WHY:  Receive arousal/fear/anxiety to modulate learning
 * HOW:  Store handle, enable state queries
 *
 * @param bridge Amygdala-training bridge
 * @param amygdala Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_training_connect_amygdala(
    amygdala_training_bridge_t* bridge,
    amygdala_t* amygdala
);

/**
 * @brief Connect to training system
 *
 * WHAT: Link to training context for coordination
 * WHY:  Enable training pipeline modulation
 * HOW:  Store opaque handle
 *
 * @param bridge Amygdala-training bridge
 * @param training Training context (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_training_connect_training(
    amygdala_training_bridge_t* bridge,
    void* training
);

/**
 * @brief Connect to optimizer
 *
 * WHAT: Link to optimizer for LR modulation
 * WHY:  Apply arousal-based learning rate adjustments
 * HOW:  Store handle for LR updates
 *
 * @param bridge Amygdala-training bridge
 * @param optimizer Optimizer context
 * @return 0 on success, error code on failure
 */
int amygdala_training_connect_optimizer(
    amygdala_training_bridge_t* bridge,
    nimcp_optimizer_context_t* optimizer
);

/**
 * @brief Disconnect amygdala
 *
 * @param bridge Amygdala-training bridge
 * @return 0 on success
 */
int amygdala_training_disconnect_amygdala(amygdala_training_bridge_t* bridge);

/**
 * @brief Disconnect training
 *
 * @param bridge Amygdala-training bridge
 * @return 0 on success
 */
int amygdala_training_disconnect_training(amygdala_training_bridge_t* bridge);

/**
 * @brief Disconnect optimizer
 *
 * @param bridge Amygdala-training bridge
 * @return 0 on success
 */
int amygdala_training_disconnect_optimizer(amygdala_training_bridge_t* bridge);

/* ============================================================================
 * Amygdala → Training: Arousal Modulates Learning
 * ============================================================================ */

/**
 * @brief Update bridge state from amygdala
 *
 * WHAT: Read amygdala state and compute LR modulation
 * WHY:  Reflect emotional state in learning parameters
 * HOW:  Query amygdala arousal, apply Yerkes-Dodson curve
 *
 * BIOLOGICAL BASIS: Inverted-U arousal-performance relationship.
 * Moderate arousal → optimal learning. Low/high arousal → impaired.
 *
 * @param bridge Amygdala-training bridge
 * @return 0 on success, error code on failure
 */
int amygdala_training_update(amygdala_training_bridge_t* bridge);

/**
 * @brief Get learning rate multiplier from arousal
 *
 * WHAT: Compute Yerkes-Dodson LR factor
 * WHY:  Apply arousal-dependent learning modulation
 * HOW:  lr_factor = 1.0 - 4.0 * (arousal - 0.5)^2
 *
 * YERKES-DODSON CURVE:
 * Arousal=0.0 → factor=0.0, Arousal=0.5 → factor=1.0, Arousal=1.0 → factor=0.0
 *
 * @param bridge Amygdala-training bridge
 * @return LR multiplier [0-1]
 */
float amygdala_training_get_lr_multiplier(const amygdala_training_bridge_t* bridge);

/**
 * @brief Get threat learning boost
 *
 * WHAT: Get LR boost for threat-related patterns
 * WHY:  Fear enhances simple conditioning
 * HOW:  Return boost if fear > threshold, else 1.0
 *
 * BIOLOGICAL BASIS: Fear/stress enhances amygdala-dependent conditioning
 * while impairing hippocampus-dependent complex learning.
 *
 * @param bridge Amygdala-training bridge
 * @return Threat learning boost multiplier [1.0-2.0]
 */
float amygdala_training_get_threat_boost(const amygdala_training_bridge_t* bridge);

/**
 * @brief Apply arousal modulation to optimizer
 *
 * WHAT: Update optimizer LR based on arousal
 * WHY:  Automatic LR adjustment during training
 * HOW:  Get LR multiplier, set via optimizer API
 *
 * @param bridge Amygdala-training bridge
 * @param base_lr Base learning rate (before modulation)
 * @return 0 on success, error code on failure
 */
int amygdala_training_apply_lr_modulation(
    amygdala_training_bridge_t* bridge,
    float base_lr
);

/* ============================================================================
 * Training → Amygdala: Instability Triggers Threat Response
 * ============================================================================ */

/**
 * @brief Report training instability to amygdala
 *
 * WHAT: Trigger amygdala threat response for training failure
 * WHY:  Training instabilities are cognitive stressors
 * HOW:  Increase amygdala fear/anxiety based on instability type
 *
 * BIOLOGICAL BASIS: Cognitive failures (prediction errors, confusion)
 * activate amygdala threat circuits, inducing stress response.
 *
 * SEVERITY MAPPING:
 * - NaN/Inf → +0.7 fear (severe threat)
 * - Loss explosion → +0.5 fear (moderate threat)
 * - Grad explosion → +0.3 fear (mild threat)
 * - Plateau → +0.1 anxiety (chronic stress)
 *
 * @param bridge Amygdala-training bridge
 * @param instability_type Type of instability (from training_immune)
 * @param severity Severity (1-10)
 * @return 0 on success, error code on failure
 */
int amygdala_training_on_instability(
    amygdala_training_bridge_t* bridge,
    int instability_type,
    float severity
);

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with inter-module messaging
 * WHY:  Enable async communication
 * HOW:  Register module with BIO_MODULE_IMMUNE_AMYGDALA_TRAINING
 *
 * @param bridge Amygdala-training bridge
 * @return 0 on success, error code on failure
 */
int amygdala_training_connect_bio_async(amygdala_training_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from messaging
 * WHY:  Clean shutdown
 *
 * @param bridge Amygdala-training bridge
 * @return 0 on success, error code on failure
 */
int amygdala_training_disconnect_bio_async(amygdala_training_bridge_t* bridge);

/**
 * @brief Check if bio-async connected
 *
 * @param bridge Amygdala-training bridge
 * @return true if connected
 */
bool amygdala_training_is_bio_async_connected(const amygdala_training_bridge_t* bridge);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get current phase
 *
 * @param bridge Amygdala-training bridge
 * @return Current phase
 */
amygdala_training_phase_t amygdala_training_get_phase(
    const amygdala_training_bridge_t* bridge
);

/**
 * @brief Get current arousal level
 *
 * @param bridge Amygdala-training bridge
 * @return Current arousal [0-1]
 */
float amygdala_training_get_arousal(const amygdala_training_bridge_t* bridge);

/**
 * @brief Get current fear level
 *
 * @param bridge Amygdala-training bridge
 * @return Current fear [0-1]
 */
float amygdala_training_get_fear(const amygdala_training_bridge_t* bridge);

/**
 * @brief Get current anxiety level
 *
 * @param bridge Amygdala-training bridge
 * @return Current anxiety [0-1]
 */
float amygdala_training_get_anxiety(const amygdala_training_bridge_t* bridge);

/**
 * @brief Get amygdala-training statistics
 *
 * @param bridge Amygdala-training bridge
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int amygdala_training_get_stats(
    const amygdala_training_bridge_t* bridge,
    amygdala_training_stats_t* stats
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* amygdala_training_phase_to_string(amygdala_training_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMYGDALA_TRAINING_BRIDGE_H */

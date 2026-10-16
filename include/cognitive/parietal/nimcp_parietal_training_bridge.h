/**
 * @file nimcp_parietal_training_bridge.h
 * @brief Parietal Lobe - Training System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: Bidirectional bridge connecting parietal lobe to training system
 * WHY:  Enable sensorimotor learning, spatial skill refinement, and
 *       numerical processing improvement through training feedback
 * HOW:  Register parietal as training module, provide learning callbacks,
 *       route training events to plasticity systems
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    Parietal-Training Bridge                                |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |   Training System     |       |   Parietal Lobe       |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Loss Functions        |       | Spatial Processing    |                |
 * |  | Optimizers            |       | Numerical Cognition   |                |
 * |  | LR Schedulers         |       | Visuospatial Skills   |                |
 * |  | Gradient Management   |       | Attention Control     |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Parietal Learning Callback Router               |            |
 * |  |  training_event → parietal_module → learning_signal      |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Parietal-Plasticity Integration                 |            |
 * |  |  learning_signal → STDP/BCM/Reward → weight_update       |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * LEARNING DOMAINS:
 * - Coordinate Transform Refinement: Learn accurate reference frame conversions
 * - Reaching Accuracy: Improve motor-visual coordination
 * - Attention Optimization: Learn efficient spatial attention allocation
 * - Numerical Processing: Refine magnitude representations
 * - Mental Rotation: Improve spatial visualization skills
 *
 * @see nimcp_parietal.h
 * @see nimcp_brain_training_integration.h
 * @see nimcp_parietal_plasticity_bridge.h
 */

#ifndef NIMCP_PARIETAL_TRAINING_BRIDGE_H
#define NIMCP_PARIETAL_TRAINING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Brain training context */
#ifndef NIMCP_BRAIN_TRAINING_INTEGRATION_H
struct nimcp_brain_training_ctx;
typedef struct nimcp_brain_training_ctx nimcp_brain_training_ctx_t;
#endif

/* Parietal cortex adapter */
#ifndef NIMCP_PARIETAL_ADAPTER_H
struct parietal_adapter;
typedef struct parietal_adapter parietal_adapter_t;
#endif
/* Type alias for compatibility */
typedef parietal_adapter_t parietal_cortex_adapter_t;

/* Parietal plasticity bridge */
#ifndef NIMCP_PARIETAL_PLASTICITY_BRIDGE_H
struct parietal_plasticity_bridge;
typedef struct parietal_plasticity_bridge parietal_plasticity_bridge_t;
#endif

/* Bio-async router */
#ifndef NIMCP_BIO_ROUTER_H
typedef struct bio_router_struct* bio_router_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PARIETAL_TRAINING_BRIDGE_VERSION    "1.0.0"
#define PARIETAL_TRAINING_BRIDGE_MAGIC      0x50545242  /* 'PTRB' */

/** Bio-async module ID for parietal-training bridge */
#define BIO_MODULE_PARIETAL_TRAINING        0x0D62

/** Maximum learning domains */
#define PARIETAL_TRAIN_MAX_DOMAINS          8

/** Default learning rate for parietal adaptation */
#define PARIETAL_TRAIN_DEFAULT_LR           0.001f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Parietal learning domains
 */
typedef enum {
    PARIETAL_DOMAIN_COORDINATE_TRANSFORM = 0, /**< Reference frame conversions */
    PARIETAL_DOMAIN_REACHING_ACCURACY,        /**< Motor-visual coordination */
    PARIETAL_DOMAIN_ATTENTION_ALLOCATION,     /**< Spatial attention control */
    PARIETAL_DOMAIN_NUMERICAL_MAGNITUDE,      /**< Number sense refinement */
    PARIETAL_DOMAIN_MENTAL_ROTATION,          /**< Spatial visualization */
    PARIETAL_DOMAIN_MULTISENSORY_BINDING,     /**< Cross-modal integration */
    PARIETAL_DOMAIN_BODY_SCHEMA,              /**< Proprioceptive representation */
    PARIETAL_DOMAIN_PATTERN_DETECTION,        /**< Mathematical patterns */
    PARIETAL_DOMAIN_COUNT
} parietal_learning_domain_t;

/**
 * @brief Training event response types
 */
typedef enum {
    PARIETAL_TRAIN_RESPONSE_NONE = 0,         /**< No action needed */
    PARIETAL_TRAIN_RESPONSE_UPDATE_WEIGHTS,   /**< Update synaptic weights */
    PARIETAL_TRAIN_RESPONSE_ADJUST_THRESHOLD, /**< Adjust activation thresholds */
    PARIETAL_TRAIN_RESPONSE_MODULATE_GAIN,    /**< Modulate processing gain */
    PARIETAL_TRAIN_RESPONSE_CONSOLIDATE       /**< Consolidate learned patterns */
} parietal_train_response_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    PARIETAL_TRAIN_STATE_UNINITIALIZED = 0,
    PARIETAL_TRAIN_STATE_INITIALIZED,
    PARIETAL_TRAIN_STATE_CONNECTED,
    PARIETAL_TRAIN_STATE_LEARNING,
    PARIETAL_TRAIN_STATE_PAUSED,
    PARIETAL_TRAIN_STATE_ERROR
} parietal_train_state_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Domain-specific learning configuration
 */
typedef struct {
    bool enabled;                    /**< Enable learning in this domain */
    float learning_rate;             /**< Domain-specific learning rate */
    float momentum;                  /**< Momentum for weight updates */
    float weight_decay;              /**< L2 regularization strength */
    float min_confidence;            /**< Minimum confidence for updates */
    bool use_reward_modulation;      /**< Enable reward-modulated learning */
} parietal_domain_config_t;

/**
 * @brief Parietal-Training bridge configuration
 */
typedef struct {
    /* Global learning parameters */
    float base_learning_rate;        /**< Base learning rate */
    float learning_rate_decay;       /**< LR decay per epoch */
    float min_learning_rate;         /**< Minimum learning rate */

    /* Domain-specific configurations */
    parietal_domain_config_t domains[PARIETAL_DOMAIN_COUNT];

    /* Training integration */
    bool register_with_training;     /**< Register as training module */
    bool receive_loss_signals;       /**< Receive loss feedback */
    bool receive_gradient_signals;   /**< Receive gradient information */

    /* Plasticity integration */
    bool connect_to_plasticity;      /**< Connect to plasticity bridge */
    bool enable_stdp_learning;       /**< Enable STDP-based learning */
    bool enable_bcm_learning;        /**< Enable BCM stabilization */
    bool enable_homeostatic;         /**< Enable homeostatic regulation */

    /* Event handling */
    bool batch_weight_updates;       /**< Batch updates for efficiency */
    uint32_t update_batch_size;      /**< Number of events per batch */
    uint32_t update_interval_ms;     /**< Minimum update interval */

    /* Bio-async integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bio_router_t bio_router;         /**< Bio-async router (optional) */

    /* Monitoring */
    bool verbose_logging;            /**< Enable verbose output */
    bool track_learning_progress;    /**< Track per-domain progress */
} parietal_training_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Per-domain learning statistics
 */
typedef struct {
    uint64_t training_events;        /**< Events received */
    uint64_t weight_updates;         /**< Weight updates performed */
    float total_delta;               /**< Cumulative weight change */
    float avg_loss;                  /**< Average loss in domain */
    float improvement_rate;          /**< Learning improvement rate */
    uint64_t last_update_time_ms;    /**< Last update timestamp */
} parietal_domain_stats_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Global statistics */
    uint64_t total_events;           /**< Total training events */
    uint64_t total_updates;          /**< Total weight updates */
    uint64_t epochs_processed;       /**< Epochs processed */
    uint64_t batches_processed;      /**< Batches processed */

    /* Loss tracking */
    float current_loss;              /**< Current loss value */
    float best_loss;                 /**< Best loss achieved */
    float loss_improvement;          /**< Loss improvement from start */

    /* Domain-specific statistics */
    parietal_domain_stats_t domain_stats[PARIETAL_DOMAIN_COUNT];

    /* Performance */
    uint64_t total_time_us;          /**< Total processing time */
    uint64_t avg_update_time_us;     /**< Average update time */

    /* Connection status */
    bool training_connected;         /**< Training system connected */
    bool plasticity_connected;       /**< Plasticity bridge connected */
    bool bio_async_connected;        /**< Bio-async connected */
} parietal_training_stats_t;

/* ============================================================================
 * Learning Signal
 * ============================================================================ */

/**
 * @brief Learning signal from training to parietal
 */
typedef struct {
    parietal_learning_domain_t domain; /**< Target domain */
    parietal_train_response_t response; /**< Suggested response */

    /* Training context */
    uint64_t epoch;                  /**< Current epoch */
    uint64_t batch;                  /**< Current batch */
    float loss_value;                /**< Loss value */
    float loss_delta;                /**< Loss change from previous */
    float learning_rate;             /**< Current learning rate */

    /* Gradient information */
    float gradient_norm;             /**< Gradient magnitude */
    float gradient_direction[8];     /**< Gradient direction per domain */

    /* Reward modulation */
    float reward_signal;             /**< External reward signal */
    float prediction_error;          /**< Reward prediction error */

    /* Timing */
    uint64_t timestamp_us;           /**< Signal timestamp */
} parietal_learning_signal_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Learning event callback
 */
typedef void (*parietal_learning_callback_t)(
    const parietal_learning_signal_t* signal,
    void* user_data
);

/**
 * @brief Weight update callback (for external monitoring)
 */
typedef void (*parietal_weight_update_callback_t)(
    parietal_learning_domain_t domain,
    float weight_delta,
    void* user_data
);

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque parietal-training bridge handle
 */
typedef struct parietal_training_bridge parietal_training_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int parietal_training_default_config(parietal_training_config_t* config);

/**
 * @brief Create parietal-training bridge
 *
 * @param config Configuration (NULL for defaults)
 * @param parietal Parietal cortex adapter (required)
 * @param training Training context (can be NULL, connect later)
 * @return Bridge handle or NULL on failure
 */
parietal_training_bridge_t* parietal_training_create(
    const parietal_training_config_t* config,
    parietal_cortex_adapter_t* parietal,
    nimcp_brain_training_ctx_t* training
);

/**
 * @brief Destroy parietal-training bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void parietal_training_destroy(parietal_training_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to training context
 *
 * @param bridge Bridge handle
 * @param training Training context
 * @return 0 on success, -1 on error
 */
int parietal_training_connect(
    parietal_training_bridge_t* bridge,
    nimcp_brain_training_ctx_t* training
);

/**
 * @brief Disconnect from training context
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int parietal_training_disconnect(parietal_training_bridge_t* bridge);

/**
 * @brief Connect to plasticity bridge
 *
 * @param bridge Bridge handle
 * @param plasticity Plasticity bridge
 * @return 0 on success, -1 on error
 */
int parietal_training_connect_plasticity(
    parietal_training_bridge_t* bridge,
    parietal_plasticity_bridge_t* plasticity
);

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int parietal_training_connect_bio_async(
    parietal_training_bridge_t* bridge,
    bio_router_t router
);

/* ============================================================================
 * Learning API
 * ============================================================================ */

/**
 * @brief Process learning signal
 *
 * @param bridge Bridge handle
 * @param signal Learning signal
 * @return Response type
 */
parietal_train_response_t parietal_training_process_signal(
    parietal_training_bridge_t* bridge,
    const parietal_learning_signal_t* signal
);

/**
 * @brief Trigger weight update for domain
 *
 * @param bridge Bridge handle
 * @param domain Learning domain
 * @param learning_rate Learning rate to use
 * @return 0 on success, -1 on error
 */
int parietal_training_update_weights(
    parietal_training_bridge_t* bridge,
    parietal_learning_domain_t domain,
    float learning_rate
);

/**
 * @brief Apply batch updates
 *
 * @param bridge Bridge handle
 * @return Number of updates applied, -1 on error
 */
int parietal_training_flush_batch(parietal_training_bridge_t* bridge);

/**
 * @brief Set domain learning rate
 *
 * @param bridge Bridge handle
 * @param domain Learning domain
 * @param learning_rate New learning rate
 * @return 0 on success, -1 on error
 */
int parietal_training_set_domain_lr(
    parietal_training_bridge_t* bridge,
    parietal_learning_domain_t domain,
    float learning_rate
);

/**
 * @brief Enable/disable domain learning
 *
 * @param bridge Bridge handle
 * @param domain Learning domain
 * @param enabled Enable flag
 * @return 0 on success, -1 on error
 */
int parietal_training_set_domain_enabled(
    parietal_training_bridge_t* bridge,
    parietal_learning_domain_t domain,
    bool enabled
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set learning callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, -1 on error
 */
int parietal_training_set_learning_callback(
    parietal_training_bridge_t* bridge,
    parietal_learning_callback_t callback,
    void* user_data
);

/**
 * @brief Set weight update callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, -1 on error
 */
int parietal_training_set_update_callback(
    parietal_training_bridge_t* bridge,
    parietal_weight_update_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
parietal_train_state_t parietal_training_get_state(
    const parietal_training_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int parietal_training_get_stats(
    const parietal_training_bridge_t* bridge,
    parietal_training_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void parietal_training_reset_stats(parietal_training_bridge_t* bridge);

/**
 * @brief Check if connected to training
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool parietal_training_is_connected(const parietal_training_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get domain name
 *
 * @param domain Domain type
 * @return String name (static)
 */
const char* parietal_training_domain_name(parietal_learning_domain_t domain);

/**
 * @brief Get response name
 *
 * @param response Response type
 * @return String name (static)
 */
const char* parietal_training_response_name(parietal_train_response_t response);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* parietal_training_state_name(parietal_train_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* parietal_training_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PARIETAL_TRAINING_BRIDGE_H */

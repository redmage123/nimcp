/**
 * @file nimcp_snn_training_bridge.h
 * @brief SNN-Training Integration Bridge
 *
 * WHAT: Bidirectional bridge between SNN training and training pipeline
 * WHY:  Enable coordinated learning with immune, plasticity, cognitive systems
 * HOW:  Integrate STDP/eProp/R-STDP with training subsystems
 *
 * BIOLOGICAL BASIS:
 * - Learning rates modulated by neuroimmune state (fever suppresses LTP)
 * - Homeostatic plasticity maintains stable network activity
 * - Metaplasticity: learning rules adapt based on history
 * - Sleep-dependent consolidation enhances offline learning
 *
 * INTEGRATION:
 * - SNN Training → Immune: Training instability triggers immune response
 * - Immune → SNN Training: Inflammation reduces learning rate
 * - SNN Training → Plasticity: Coordinates STDP with global plasticity
 * - SNN Training → Cognitive: Training state affects attention/memory
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_TRAINING_BRIDGE_H
#define NIMCP_SNN_TRAINING_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_training.h"
#include "async/nimcp_bio_async.h"

/* Forward declarations for connected systems */
struct training_immune_system_s;
struct training_plasticity_bridge_s;
struct cognitive_training_bridge_s;
struct snn_sleep_bridge_s;

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Training instability type
 *
 * WHAT: Category of detected training issue
 * WHY:  Different instabilities require different interventions
 * HOW:  Pattern-based classification
 */
typedef enum {
    SNN_TRAIN_STABLE = 0,           /**< Training is stable */
    SNN_TRAIN_WEIGHT_EXPLOSION,     /**< Weights growing unbounded */
    SNN_TRAIN_WEIGHT_COLLAPSE,      /**< Weights collapsing to zero */
    SNN_TRAIN_RATE_EXPLOSION,       /**< Firing rates too high */
    SNN_TRAIN_RATE_COLLAPSE,        /**< Firing rates too low (dead neurons) */
    SNN_TRAIN_GRADIENT_EXPLOSION,   /**< Gradients too large */
    SNN_TRAIN_GRADIENT_VANISHING,   /**< Gradients too small */
    SNN_TRAIN_OSCILLATION,          /**< Unstable oscillatory dynamics */
    SNN_TRAIN_SATURATION            /**< Neurons saturated */
} snn_training_instability_t;

/**
 * @brief Training modulation source
 *
 * WHAT: Which system is modulating training
 * WHY:  Track sources of learning rate changes
 * HOW:  Bitfield for multiple simultaneous sources
 */
typedef enum {
    SNN_MODULATION_NONE = 0,
    SNN_MODULATION_IMMUNE = 1,          /**< Immune/inflammation */
    SNN_MODULATION_HOMEOSTATIC = 2,     /**< Homeostatic plasticity */
    SNN_MODULATION_METAPLASTICITY = 4,  /**< Metaplasticity (BCM-like) */
    SNN_MODULATION_SLEEP = 8,           /**< Sleep/consolidation */
    SNN_MODULATION_ATTENTION = 16,      /**< Attentional gating */
    SNN_MODULATION_REWARD = 32          /**< Reward modulation */
} snn_training_modulation_t;

/**
 * @brief SNN training bridge configuration
 *
 * WHAT: Parameters for training integration
 * WHY:  Control how training interacts with other systems
 * HOW:  Thresholds, rates, and enable flags
 */
typedef struct snn_training_bridge_config_s {
    /* Instability detection */
    float weight_explosion_threshold;   /**< Max weight magnitude */
    float weight_collapse_threshold;    /**< Min weight variance */
    float rate_explosion_threshold;     /**< Max firing rate (Hz) */
    float rate_collapse_threshold;      /**< Min firing rate (Hz) */
    float gradient_explosion_threshold; /**< Max gradient norm */
    float gradient_vanishing_threshold; /**< Min gradient norm */

    /* Modulation parameters */
    float immune_modulation_strength;   /**< How much immune affects LR [0,1] */
    float homeostatic_modulation_strength; /**< Homeostatic LR effect [0,1] */
    float sleep_consolidation_boost;    /**< LTP boost during sleep [1,2] */
    float attention_gating_strength;    /**< Attention modulation [0,1] */

    /* Metaplasticity (BCM-like) */
    bool enable_metaplasticity;         /**< Enable sliding threshold */
    float metaplasticity_tau;           /**< Time constant (ms) */
    float bcm_theta_init;               /**< Initial BCM threshold */

    /* Consolidation */
    bool enable_offline_consolidation;  /**< Enable sleep-like consolidation */
    float consolidation_rate;           /**< Synaptic strengthening rate */
    float replay_probability;           /**< Probability of replay */

    /* Integration enables */
    bool enable_immune_integration;     /**< Connect to training-immune */
    bool enable_plasticity_integration; /**< Connect to training-plasticity */
    bool enable_cognitive_integration;  /**< Connect to cognitive-training */
    bool enable_sleep_integration;      /**< Connect to sleep bridge */

    /* Update timing */
    float update_interval_ms;           /**< How often to check state */
    float instability_check_interval_ms; /**< How often to check stability */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} snn_training_bridge_config_t;

/**
 * @brief Training metrics state
 *
 * WHAT: Current training metrics
 * WHY:  Track learning progress and stability
 * HOW:  Running statistics
 */
typedef struct snn_training_metrics_s {
    /* Weight statistics */
    float weight_mean;                  /**< Mean weight value */
    float weight_std;                   /**< Weight standard deviation */
    float weight_max;                   /**< Maximum weight */
    float weight_min;                   /**< Minimum weight */

    /* Firing rate statistics */
    float rate_mean;                    /**< Mean firing rate (Hz) */
    float rate_std;                     /**< Rate standard deviation */
    float rate_max;                     /**< Maximum rate */
    float rate_min;                     /**< Minimum rate */

    /* Gradient statistics (if applicable) */
    float gradient_norm;                /**< Gradient L2 norm */
    float gradient_max;                 /**< Maximum gradient */

    /* Learning dynamics */
    float effective_lr;                 /**< Current effective learning rate */
    float lr_modulation_factor;         /**< Combined modulation [0,1] */
    uint32_t modulation_sources;        /**< Active modulation sources (bitfield) */

    /* Stability */
    snn_training_instability_t instability_type; /**< Current instability */
    uint32_t instability_count;         /**< Total instabilities detected */
    float stability_score;              /**< Overall stability [0,1] */
} snn_training_metrics_t;

/**
 * @brief SNN training bridge state
 *
 * WHAT: Current state of training bridge
 * WHY:  Track integration status
 * HOW:  Connection status and statistics
 */
typedef struct snn_training_bridge_state_s {
    /* Current metrics */
    snn_training_metrics_t metrics;     /**< Current training metrics */

    /* Metaplasticity state */
    float bcm_theta;                    /**< Current BCM threshold */
    float metaplasticity_level;         /**< Metaplasticity level [0,1] */

    /* Consolidation state */
    bool consolidation_active;          /**< Currently consolidating */
    float consolidation_progress;       /**< Consolidation progress [0,1] */
    uint32_t replay_count;              /**< Total replay events */

    /* Integration status */
    bool immune_connected;              /**< Connected to immune system */
    bool plasticity_connected;          /**< Connected to plasticity */
    bool cognitive_connected;           /**< Connected to cognitive */
    bool sleep_connected;               /**< Connected to sleep bridge */

    /* Statistics */
    uint32_t update_count;              /**< Total updates */
    uint32_t weight_updates;            /**< Total weight updates */
    uint32_t modulation_events;         /**< Times LR was modulated */
    float avg_effective_lr;             /**< Average effective LR */
    float total_delta_w;                /**< Total weight change */
} snn_training_bridge_state_t;

/**
 * @brief SNN training bridge structure
 *
 * WHAT: Context for SNN-training integration
 * WHY:  Coordinate training with other systems
 * HOW:  Store references and state
 */
typedef struct snn_training_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Core components */
    snn_network_t* snn;                 /**< SNN network */
    snn_training_ctx_t* training_ctx;   /**< Training context */
    snn_training_bridge_config_t config; /**< Bridge configuration */
    snn_training_bridge_state_t state;  /**< Current state */

    /* Connected systems (optional) */
    struct training_immune_system_s* immune_system;
    struct training_plasticity_bridge_s* plasticity_bridge;
    struct cognitive_training_bridge_s* cognitive_bridge;
    struct snn_sleep_bridge_s* sleep_bridge;

    /* Base learning rate (before modulation) */
    float base_lr;                      /**< Base learning rate */

    /* Rate estimation (for homeostatic) */
    float* neuron_rates;                /**< Per-neuron rate estimates */
    uint32_t n_neurons;                 /**< Number of neurons tracked */

    /* Timing */
    float last_update_time;             /**< Last update timestamp (ms) */
    float last_stability_check;         /**< Last stability check (ms) */
    float total_time;                   /**< Total time (ms) */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_training_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize training bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_training_bridge_config_default(snn_training_bridge_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN training bridge
 *
 * WHAT: Initialize training integration bridge
 * WHY:  Enable coordinated learning
 * HOW:  Allocate context, initialize state
 *
 * @param config Bridge configuration
 * @param snn SNN network
 * @param training_ctx Training context (STDP, eProp, etc.)
 * @return Bridge instance or NULL on failure
 */
snn_training_bridge_t* snn_training_bridge_create(
    const snn_training_bridge_config_t* config,
    snn_network_t* snn,
    snn_training_ctx_t* training_ctx
);

/**
 * @brief Destroy SNN training bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy
 */
void snn_training_bridge_destroy(snn_training_bridge_t* bridge);

//=============================================================================
// System Connections
//=============================================================================

/**
 * @brief Connect to training-immune system
 *
 * WHAT: Enable immune modulation of learning
 * WHY:  Model fever-induced LTP suppression
 * HOW:  Register for inflammation updates
 *
 * @param bridge Training bridge
 * @param immune Training-immune system
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_connect_immune(
    snn_training_bridge_t* bridge,
    struct training_immune_system_s* immune
);

/**
 * @brief Connect to training-plasticity bridge
 *
 * WHAT: Coordinate with global plasticity
 * WHY:  Unified plasticity management
 * HOW:  Share plasticity state
 *
 * @param bridge Training bridge
 * @param plasticity Plasticity bridge
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_connect_plasticity(
    snn_training_bridge_t* bridge,
    struct training_plasticity_bridge_s* plasticity
);

/**
 * @brief Connect to cognitive training bridge
 *
 * WHAT: Enable cognitive state modulation
 * WHY:  Attention affects learning
 * HOW:  Receive attention/arousal signals
 *
 * @param bridge Training bridge
 * @param cognitive Cognitive training bridge
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_connect_cognitive(
    snn_training_bridge_t* bridge,
    struct cognitive_training_bridge_s* cognitive
);

/**
 * @brief Connect to sleep bridge
 *
 * WHAT: Enable sleep-dependent consolidation
 * WHY:  Sleep consolidates memories
 * HOW:  Receive sleep stage signals, trigger replay
 *
 * @param bridge Training bridge
 * @param sleep Sleep bridge
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_connect_sleep(
    snn_training_bridge_t* bridge,
    struct snn_sleep_bridge_s* sleep
);

//=============================================================================
// Bio-async Functions
//=============================================================================

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed training coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_connect_bio_async(snn_training_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_training_bridge_disconnect_bio_async(snn_training_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_training_bridge_is_bio_async_connected(const snn_training_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Main update loop for training integration
 * WHY:  Coordinate all training aspects
 * HOW:  Update metrics, check stability, apply modulation
 *
 * @param bridge Bridge to update
 * @param dt Time step in milliseconds
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_update(snn_training_bridge_t* bridge, float dt);

/**
 * @brief Perform training step
 *
 * WHAT: Execute one training step with full integration
 * WHY:  Apply learning with all modulations
 * HOW:  Compute modulated LR, apply STDP/eProp/etc.
 *
 * @param bridge Bridge instance
 * @param dt Time step (ms)
 * @return Number of weight updates
 */
uint32_t snn_training_bridge_train_step(
    snn_training_bridge_t* bridge,
    float dt
);

//=============================================================================
// Instability Detection
//=============================================================================

/**
 * @brief Check training stability
 *
 * WHAT: Detect training instabilities
 * WHY:  Prevent divergence or collapse
 * HOW:  Check metrics against thresholds
 *
 * @param bridge Bridge instance
 * @return Detected instability type (STABLE if none)
 */
snn_training_instability_t snn_training_bridge_check_stability(
    snn_training_bridge_t* bridge
);

/**
 * @brief Handle detected instability
 *
 * WHAT: Respond to training instability
 * WHY:  Recover from divergence
 * HOW:  Reduce LR, reset weights, trigger immune
 *
 * @param bridge Bridge instance
 * @param instability Type of instability
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_handle_instability(
    snn_training_bridge_t* bridge,
    snn_training_instability_t instability
);

//=============================================================================
// Learning Rate Modulation
//=============================================================================

/**
 * @brief Get effective learning rate
 *
 * WHAT: Compute modulated learning rate
 * WHY:  Apply all modulation factors
 * HOW:  base_lr * immune_factor * homeostatic_factor * ...
 *
 * @param bridge Bridge instance
 * @return Effective learning rate
 */
float snn_training_bridge_get_effective_lr(const snn_training_bridge_t* bridge);

/**
 * @brief Set base learning rate
 *
 * WHAT: Set unmodulated learning rate
 * WHY:  Allow LR scheduling
 * HOW:  Update base_lr field
 *
 * @param bridge Bridge instance
 * @param base_lr New base learning rate
 */
void snn_training_bridge_set_base_lr(snn_training_bridge_t* bridge, float base_lr);

/**
 * @brief Get modulation factor from specific source
 *
 * WHAT: Get individual modulation component
 * WHY:  Debug/analyze modulation
 * HOW:  Query specific system
 *
 * @param bridge Bridge instance
 * @param source Modulation source
 * @return Modulation factor [0,1] or 1.0 if not connected
 */
float snn_training_bridge_get_modulation_factor(
    const snn_training_bridge_t* bridge,
    snn_training_modulation_t source
);

//=============================================================================
// Metaplasticity
//=============================================================================

/**
 * @brief Update BCM threshold (metaplasticity)
 *
 * WHAT: Adjust sliding threshold based on activity
 * WHY:  Prevent runaway LTP or LTD
 * HOW:  BCM sliding threshold rule
 *
 * @param bridge Bridge instance
 * @param dt Time step (ms)
 * @return New BCM theta value
 */
float snn_training_bridge_update_bcm_theta(
    snn_training_bridge_t* bridge,
    float dt
);

/**
 * @brief Get metaplasticity level
 *
 * WHAT: Query current metaplasticity state
 * WHY:  Monitor adaptation level
 * HOW:  Normalized theta value
 *
 * @param bridge Bridge instance
 * @return Metaplasticity level [0,1]
 */
float snn_training_bridge_get_metaplasticity_level(
    const snn_training_bridge_t* bridge
);

//=============================================================================
// Consolidation
//=============================================================================

/**
 * @brief Trigger memory consolidation
 *
 * WHAT: Start offline consolidation process
 * WHY:  Strengthen important synapses
 * HOW:  Boost weights for recently active synapses
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_trigger_consolidation(snn_training_bridge_t* bridge);

/**
 * @brief Trigger spike sequence replay
 *
 * WHAT: Replay recent activity patterns
 * WHY:  Consolidate episodic memories
 * HOW:  Reactivate stored sequences
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int snn_training_bridge_trigger_replay(snn_training_bridge_t* bridge);

/**
 * @brief Check if consolidation is active
 *
 * @param bridge Bridge instance
 * @return true if consolidating
 */
bool snn_training_bridge_is_consolidating(const snn_training_bridge_t* bridge);

//=============================================================================
// Metrics and Statistics
//=============================================================================

/**
 * @brief Update training metrics
 *
 * WHAT: Compute current training statistics
 * WHY:  Monitor learning health
 * HOW:  Analyze weights, rates, gradients
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int snn_training_bridge_update_metrics(snn_training_bridge_t* bridge);

/**
 * @brief Get training metrics
 *
 * WHAT: Query current metrics
 * WHY:  Monitor training state
 * HOW:  Copy current metrics
 *
 * @param bridge Bridge instance
 * @param metrics Output metrics (copied)
 * @return 0 on success
 */
int snn_training_bridge_get_metrics(
    const snn_training_bridge_t* bridge,
    snn_training_metrics_t* metrics
);

/**
 * @brief Get bridge state
 *
 * WHAT: Query full bridge state
 * WHY:  Debugging and monitoring
 * HOW:  Copy state structure
 *
 * @param bridge Bridge to query
 * @param state Output state (copied)
 * @return 0 on success
 */
int snn_training_bridge_get_state(
    const snn_training_bridge_t* bridge,
    snn_training_bridge_state_t* state
);

/**
 * @brief Get statistics
 *
 * WHAT: Get training statistics summary
 * WHY:  Monitor training progress
 * HOW:  Return key counters
 *
 * @param bridge Bridge to query
 * @param update_count Output: total updates
 * @param weight_updates Output: total weight updates
 * @param avg_effective_lr Output: average effective LR
 * @return 0 on success
 */
int snn_training_bridge_get_stats(
    const snn_training_bridge_t* bridge,
    uint32_t* update_count,
    uint32_t* weight_updates,
    float* avg_effective_lr
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge to reset
 */
void snn_training_bridge_reset_stats(snn_training_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get stability score
 *
 * @param bridge Bridge to query
 * @return Stability score [0,1]
 */
float snn_training_bridge_get_stability_score(const snn_training_bridge_t* bridge);

/**
 * @brief Get current instability type
 *
 * @param bridge Bridge to query
 * @return Current instability (STABLE if none)
 */
snn_training_instability_t snn_training_bridge_get_instability(
    const snn_training_bridge_t* bridge
);

/**
 * @brief Get consolidation progress
 *
 * @param bridge Bridge to query
 * @return Consolidation progress [0,1]
 */
float snn_training_bridge_get_consolidation_progress(
    const snn_training_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_TRAINING_BRIDGE_H */

/**
 * @file nimcp_hypothalamus_training_bridge.h
 * @brief Bridge between hypothalamus drives and training system for homeostatic learning
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional bridge connecting hypothalamic drives to the training integration hub,
 *       enabling drive-modulated learning and homeostatic stability regulation.
 *
 * WHY: Training systems benefit from biological homeostatic principles:
 *      - Curiosity drive modulates exploration (higher LR, broader sampling)
 *      - Safety drive constrains learning (cautious updates, gradient clipping)
 *      - Competence drive tracks mastery and adjusts difficulty
 *      - Fatigue drive signals need for consolidation phases
 *      - Homeostatic loss regulation maintains "healthy" training range
 *      This implements Byrnes' "Steering Subsystem" concept for safe adaptive learning.
 *
 * HOW: The bridge:
 *      1. Subscribes to training events (loss, gradients, LR changes, epochs)
 *      2. Computes homeostatic deviations (loss vs setpoint = "core temperature")
 *      3. Updates hypothalamic drives based on training state
 *      4. Publishes modulation signals back to training hub
 *      5. Coordinates with hypothalamus orchestrator for unified drive response
 *
 * DESIGN PATTERNS:
 * - Bridge: Connects two distinct systems (hypothalamus <-> training)
 * - Observer: Subscribes to training events
 * - Strategy: Different modulation strategies per drive type
 * - Mediator: Routes between orchestrator and training hub
 *
 * KEY INTEGRATIONS:
 * 1. Loss → Homeostatic deviation → Safety/Curiosity drive balance
 * 2. Gradient norm → Stress detection → Safety drive activation
 * 3. Epoch progress → Competence tracking → Difficulty modulation
 * 4. Training duration → Fatigue accumulation → Consolidation triggers
 * 5. LR changes → Exploration/exploitation balance → Curiosity drive
 *
 * THREAD SAFETY: All functions are thread-safe. Uses internal synchronization.
 *
 * PERFORMANCE:
 * - Event handling: O(1) per event
 * - Modulation computation: O(drives) where drives = active drive count
 * - Drive update: O(1)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPOTHALAMUS_TRAINING_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_TRAINING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

/* Hypothalamus orchestrator (from nimcp_hypothalamus_orchestrator.h) */
typedef struct hypo_orchestrator_struct* hypo_orchestrator_t;

/* Training integration hub (from nimcp_training_integration_hub.h) */
typedef struct training_integration_hub_struct* training_integration_hub_t;

/* ============================================================================
 * OPAQUE TYPE DECLARATIONS
 * ============================================================================ */

/**
 * WHAT: Opaque handle to hypothalamus-training bridge
 * WHY: Encapsulation - hide internal implementation details
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct hypo_training_bridge_struct hypo_training_bridge_t;

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Module ID for registration with training hub */
#define HYPO_TRAINING_BRIDGE_MODULE_ID      0x3001

/** Maximum loss history entries for trend analysis */
#define HYPO_TRAINING_MAX_LOSS_HISTORY      256

/** Maximum gradient history entries */
#define HYPO_TRAINING_MAX_GRADIENT_HISTORY  128

/** Default homeostatic loss setpoint */
#define HYPO_TRAINING_DEFAULT_LOSS_SETPOINT 0.5f

/** Default loss tolerance band (sigma) */
#define HYPO_TRAINING_DEFAULT_LOSS_TOLERANCE 0.1f

/** Default fatigue accumulation rate per epoch */
#define HYPO_TRAINING_DEFAULT_FATIGUE_RATE  0.02f

/** Default consolidation threshold (fatigue level) */
#define HYPO_TRAINING_DEFAULT_CONSOLIDATION_THRESHOLD 0.8f

/** Minimum precision for modulation */
#define HYPO_TRAINING_MIN_PRECISION         0.1f

/** Maximum precision for modulation */
#define HYPO_TRAINING_MAX_PRECISION         2.0f

/** Default curiosity-exploration LR multiplier */
#define HYPO_TRAINING_DEFAULT_CURIOSITY_LR_MULT 1.5f

/** Default safety LR reduction factor */
#define HYPO_TRAINING_DEFAULT_SAFETY_LR_MULT 0.5f

/* ============================================================================
 * DRIVE MODULATION TYPES
 * ============================================================================ */

/**
 * WHAT: Types of training modulations the bridge can apply
 * WHY: Categorize modulation effects for routing
 * HOW: Each type maps to specific training parameters
 */
typedef enum {
    HYPO_TRAIN_MOD_LEARNING_RATE = 0,   /**< Modulate learning rate */
    HYPO_TRAIN_MOD_BATCH_SIZE,          /**< Modulate batch size */
    HYPO_TRAIN_MOD_GRADIENT_CLIP,       /**< Modulate gradient clipping */
    HYPO_TRAIN_MOD_CURRICULUM_DIFF,     /**< Modulate curriculum difficulty */
    HYPO_TRAIN_MOD_SAMPLE_PRIORITY,     /**< Modulate sample priority weights */
    HYPO_TRAIN_MOD_CHECKPOINT_FREQ,     /**< Modulate checkpoint frequency */
    HYPO_TRAIN_MOD_MULTI_TASK_WEIGHT,   /**< Modulate multi-task weights */
    HYPO_TRAIN_MOD_REPLAY_PRIORITY,     /**< Modulate replay buffer priority */
    HYPO_TRAIN_MOD_COUNT                /**< Total modulation types */
} hypo_training_modulation_type_t;

/**
 * WHAT: Training state assessment from hypothalamic perspective
 * WHY: Categorize training health for response selection
 * HOW: Maps loss deviation to categorical state
 */
typedef enum {
    HYPO_TRAIN_STATE_HEALTHY = 0,       /**< Loss in healthy range */
    HYPO_TRAIN_STATE_IMPROVING,         /**< Loss decreasing toward setpoint */
    HYPO_TRAIN_STATE_PLATEAU,           /**< Loss stagnant */
    HYPO_TRAIN_STATE_DIVERGING,         /**< Loss increasing away from setpoint */
    HYPO_TRAIN_STATE_UNSTABLE,          /**< High variance in loss */
    HYPO_TRAIN_STATE_CRITICAL           /**< Loss far from setpoint or NaN/Inf */
} hypo_training_state_t;

/**
 * WHAT: Consolidation phase type
 * WHY: Different consolidation strategies
 * HOW: Maps fatigue level to consolidation action
 */
typedef enum {
    HYPO_CONSOL_NONE = 0,               /**< No consolidation needed */
    HYPO_CONSOL_MINI_REST,              /**< Brief pause, reduce LR */
    HYPO_CONSOL_CHECKPOINT,             /**< Save checkpoint, validate */
    HYPO_CONSOL_REPLAY,                 /**< Memory replay phase */
    HYPO_CONSOL_FULL_REST               /**< Full consolidation cycle */
} hypo_consolidation_type_t;

/* ============================================================================
 * CONFIGURATION STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Configuration for drive-to-training modulation mappings
 * WHY: Customize how drives affect training parameters
 * HOW: Per-drive weight and limit configuration
 */
typedef struct {
    /* Curiosity drive configuration */
    float curiosity_lr_multiplier;      /**< LR multiplier when curiosity high */
    float curiosity_exploration_weight; /**< Weight for exploration actions */
    bool curiosity_enables_random_search; /**< Allow random sampling */

    /* Safety drive configuration */
    float safety_lr_reduction;          /**< LR reduction when safety high */
    float safety_gradient_clip_mult;    /**< Gradient clip multiplier */
    float safety_divergence_threshold;  /**< Loss divergence detection threshold */
    bool safety_enables_early_stopping; /**< Allow safety-triggered early stop */

    /* Competence drive configuration */
    float competence_difficulty_weight; /**< Weight for difficulty adjustment */
    float competence_mastery_threshold; /**< Threshold for mastery detection */
    bool competence_auto_curriculum;    /**< Enable automatic difficulty progression */

    /* Fatigue drive configuration */
    float fatigue_lr_decay;             /**< LR decay when fatigued */
    float fatigue_consolidation_threshold; /**< Fatigue level for consolidation */
    uint32_t fatigue_max_epochs_before_rest; /**< Max epochs before forced rest */

    /* Autonomy drive configuration */
    float autonomy_self_pacing_weight;  /**< Weight for self-paced learning */
    bool autonomy_override_teacher;     /**< Allow overriding teacher curriculum */
} hypo_training_drive_config_t;

/**
 * WHAT: Homeostatic loss regulation configuration
 * WHY: Configure the "core temperature" analogy for loss
 * HOW: Setpoint, tolerance, and response parameters
 */
typedef struct {
    float loss_setpoint;                /**< Target loss value ("ideal temperature") */
    float loss_tolerance;               /**< Acceptable deviation from setpoint */
    float deviation_response_gain;      /**< Gain for deviation response */
    bool adaptive_setpoint;             /**< Adjust setpoint based on progress */
    float setpoint_decay_rate;          /**< Rate of setpoint reduction */
    float min_setpoint;                 /**< Minimum setpoint value */
} hypo_training_homeostatic_config_t;

/**
 * WHAT: Full bridge configuration
 * WHY: Comprehensive configuration at creation time
 * HOW: Combine all sub-configurations
 */
typedef struct {
    /* Connection configuration */
    bool auto_connect_orchestrator;     /**< Auto-connect to hypothalamus orch */
    bool auto_connect_training_hub;     /**< Auto-connect to training hub */
    bool enable_bio_async;              /**< Enable bio-async integration */

    /* Drive modulation configuration */
    hypo_training_drive_config_t drive_config;

    /* Homeostatic configuration */
    hypo_training_homeostatic_config_t homeostatic_config;

    /* Operational configuration */
    bool enable_consolidation;          /**< Enable fatigue-based consolidation */
    bool enable_stress_response;        /**< Enable stress response for divergence */
    bool enable_reward_signals;         /**< Generate reward signals for progress */
    uint32_t update_interval_ms;        /**< Minimum time between updates (0=every event) */

    /* Logging and debugging */
    bool enable_logging;                /**< Log modulation decisions */
    bool enable_metrics;                /**< Collect performance metrics */
} hypo_training_bridge_config_t;

/* ============================================================================
 * STATE STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Current modulation output from drives to training
 * WHY: Package all modulations for training system
 * HOW: Per-type modulation values
 */
typedef struct {
    float lr_multiplier;                /**< Learning rate multiplier [0.1, 2.0] */
    float batch_size_multiplier;        /**< Batch size multiplier [0.5, 2.0] */
    float gradient_clip_multiplier;     /**< Gradient clip multiplier [0.5, 2.0] */
    float difficulty_adjustment;        /**< Difficulty adjustment [-1.0, 1.0] */
    float sample_priority_boost;        /**< Sample priority boost [0.0, 1.0] */
    float checkpoint_urgency;           /**< Checkpoint urgency [0.0, 1.0] */
    float multi_task_weight_shift;      /**< Multi-task weight shift [-1.0, 1.0] */
    float replay_priority_boost;        /**< Replay priority boost [0.0, 1.0] */

    /* Computed recommendations */
    hypo_consolidation_type_t recommended_consolidation;
    bool recommend_early_stopping;
    bool recommend_lr_reduction;
    bool recommend_checkpoint;
} hypo_training_modulation_t;

/**
 * WHAT: Homeostatic state tracking for loss regulation
 * WHY: Track loss deviation and trend
 * HOW: Running statistics and history
 */
typedef struct {
    float current_loss;                 /**< Most recent loss value */
    float loss_setpoint;                /**< Current setpoint */
    float deviation;                    /**< Current deviation from setpoint */
    float deviation_rate;               /**< Rate of change of deviation */
    float loss_trend;                   /**< Recent loss trend (negative = improving) */
    float loss_variance;                /**< Recent loss variance */
    hypo_training_state_t state;        /**< Current training state assessment */
    uint32_t epochs_since_improvement;  /**< Epochs without improvement */
    float best_loss_seen;               /**< Best loss achieved */
} hypo_training_homeostatic_state_t;

/**
 * WHAT: Drive state contribution to training
 * WHY: Track per-drive effect on training
 * HOW: Per-drive activation and contribution
 */
typedef struct {
    float curiosity_activation;         /**< Curiosity drive level [0, 1] */
    float safety_activation;            /**< Safety drive level [0, 1] */
    float competence_activation;        /**< Competence drive level [0, 1] */
    float fatigue_level;                /**< Fatigue accumulation [0, 1] */
    float autonomy_activation;          /**< Autonomy drive level [0, 1] */

    /* Derived states */
    float exploration_tendency;         /**< Curiosity - Safety balance */
    float learning_readiness;           /**< 1.0 - Fatigue */
    float difficulty_readiness;         /**< Competence-based difficulty */
} hypo_training_drive_state_t;

/**
 * WHAT: Bridge statistics
 * WHY: Monitor bridge performance
 * HOW: Counters and metrics
 */
typedef struct {
    /* Event statistics */
    uint64_t training_events_received;  /**< Total training events processed */
    uint64_t modulations_published;     /**< Modulations sent to training */
    uint64_t drive_updates;             /**< Drive updates to orchestrator */

    /* Modulation statistics */
    uint64_t lr_modulations;            /**< LR modulation events */
    uint64_t safety_interventions;      /**< Safety-triggered actions */
    uint64_t consolidation_phases;      /**< Consolidation phases triggered */
    uint64_t early_stopping_recommendations; /**< Early stop recommendations */

    /* Homeostatic statistics */
    float avg_loss_deviation;           /**< Average loss deviation */
    float max_loss_deviation;           /**< Maximum loss deviation seen */
    uint32_t divergence_detections;     /**< Times divergence detected */
    uint32_t plateau_detections;        /**< Times plateau detected */

    /* Performance */
    uint64_t avg_processing_time_us;    /**< Average event processing time */
    uint64_t uptime_us;                 /**< Bridge uptime */
} hypo_training_bridge_stats_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default bridge configuration
 * WHY: Provide sensible defaults
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - auto_connect_orchestrator: true
 * - auto_connect_training_hub: true
 * - loss_setpoint: 0.5
 * - loss_tolerance: 0.1
 * - curiosity_lr_multiplier: 1.5
 * - safety_lr_reduction: 0.5
 * - fatigue_consolidation_threshold: 0.8
 *
 * @param config Output: configuration structure
 * @return 0 on success, -1 on failure (NULL config)
 */
int hypo_training_bridge_default_config(hypo_training_bridge_config_t* config);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new hypothalamus-training bridge
 * WHY: Initialize bridge for drive-training integration
 * HOW: Allocate resources, initialize state
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param orchestrator Hypothalamus orchestrator (can be NULL, connect later)
 * @param training_hub Training integration hub (can be NULL, connect later)
 * @return Bridge handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 *
 * MEMORY: Caller must call hypo_training_bridge_destroy() when done
 */
hypo_training_bridge_t* hypo_training_bridge_create(
    const hypo_training_bridge_config_t* config,
    hypo_orchestrator_t orchestrator,
    training_integration_hub_t training_hub
);

/**
 * WHAT: Destroy hypothalamus-training bridge
 * WHY: Release all resources
 * HOW: Disconnect, cleanup, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void hypo_training_bridge_destroy(hypo_training_bridge_t* bridge);

/**
 * WHAT: Reset bridge to initial state
 * WHY: Clear state without destroying
 * HOW: Reset drives, homeostatic state, statistics
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_reset(hypo_training_bridge_t* bridge);

/* ============================================================================
 * CONNECTION MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Connect bridge to hypothalamus orchestrator
 * WHY: Enable drive communication
 * HOW: Register with orchestrator, subscribe to drive events
 *
 * @param bridge Training bridge
 * @param orchestrator Hypothalamus orchestrator
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_connect_orchestrator(
    hypo_training_bridge_t* bridge,
    hypo_orchestrator_t orchestrator
);

/**
 * WHAT: Connect bridge to training integration hub
 * WHY: Enable training event communication
 * HOW: Register with hub, subscribe to training events
 *
 * @param bridge Training bridge
 * @param training_hub Training integration hub
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_connect_training_hub(
    hypo_training_bridge_t* bridge,
    training_integration_hub_t training_hub
);

/**
 * WHAT: Disconnect bridge from all systems
 * WHY: Clean disconnection
 * HOW: Unregister from orchestrator and hub
 *
 * @param bridge Training bridge
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_disconnect(hypo_training_bridge_t* bridge);

/**
 * WHAT: Check if bridge is connected
 * WHY: Verify connection status
 * HOW: Check orchestrator and hub connections
 *
 * @param bridge Training bridge
 * @param orchestrator_connected Output: orchestrator connection status
 * @param hub_connected Output: training hub connection status
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_is_connected(
    const hypo_training_bridge_t* bridge,
    bool* orchestrator_connected,
    bool* hub_connected
);

/* ============================================================================
 * TRAINING EVENT PROCESSING
 * ============================================================================ */

/**
 * WHAT: Process a training loss event
 * WHY: Update homeostatic state based on loss
 * HOW: Compute deviation, update drives, generate modulations
 *
 * @param bridge Training bridge
 * @param epoch Current epoch
 * @param batch Current batch
 * @param loss Loss value
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_process_loss(
    hypo_training_bridge_t* bridge,
    uint32_t epoch,
    uint32_t batch,
    float loss
);

/**
 * WHAT: Process a gradient ready event
 * WHY: Detect gradient instability
 * HOW: Check norms, detect anomalies, update safety drive
 *
 * @param bridge Training bridge
 * @param gradient_norm L2 gradient norm
 * @param was_clipped Whether gradients were clipped
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_process_gradient(
    hypo_training_bridge_t* bridge,
    float gradient_norm,
    bool was_clipped
);

/**
 * WHAT: Process an epoch completion event
 * WHY: Update fatigue, competence tracking
 * HOW: Accumulate fatigue, assess progress, check consolidation
 *
 * @param bridge Training bridge
 * @param epoch Completed epoch
 * @param avg_loss Average loss for epoch
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_process_epoch(
    hypo_training_bridge_t* bridge,
    uint32_t epoch,
    float avg_loss
);

/**
 * WHAT: Process a learning rate change event
 * WHY: Track exploration/exploitation state
 * HOW: Update curiosity-safety balance
 *
 * @param bridge Training bridge
 * @param old_lr Previous learning rate
 * @param new_lr New learning rate
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_process_lr_change(
    hypo_training_bridge_t* bridge,
    float old_lr,
    float new_lr
);

/* ============================================================================
 * MODULATION OUTPUT
 * ============================================================================ */

/**
 * WHAT: Compute current training modulations
 * WHY: Generate modulation signals from drives
 * HOW: Combine drive activations with config weights
 *
 * @param bridge Training bridge
 * @param modulation Output: computed modulations
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_compute_modulation(
    hypo_training_bridge_t* bridge,
    hypo_training_modulation_t* modulation
);

/**
 * WHAT: Get recommended learning rate multiplier
 * WHY: Direct LR modulation query
 * HOW: Compute from curiosity-safety balance
 *
 * @param bridge Training bridge
 * @param lr_multiplier Output: LR multiplier [0.1, 2.0]
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_get_lr_multiplier(
    const hypo_training_bridge_t* bridge,
    float* lr_multiplier
);

/**
 * WHAT: Get recommended curriculum difficulty adjustment
 * WHY: Direct difficulty modulation query
 * HOW: Compute from competence state
 *
 * @param bridge Training bridge
 * @param difficulty_adj Output: difficulty adjustment [-1.0, 1.0]
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_get_difficulty_adjustment(
    const hypo_training_bridge_t* bridge,
    float* difficulty_adj
);

/**
 * WHAT: Check if consolidation is recommended
 * WHY: Query fatigue-based consolidation need
 * HOW: Check fatigue level against threshold
 *
 * @param bridge Training bridge
 * @param consolidation_type Output: recommended consolidation type
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_check_consolidation(
    const hypo_training_bridge_t* bridge,
    hypo_consolidation_type_t* consolidation_type
);

/* ============================================================================
 * HOMEOSTATIC STATE
 * ============================================================================ */

/**
 * WHAT: Get current homeostatic state
 * WHY: Query loss regulation state
 * HOW: Copy internal state
 *
 * @param bridge Training bridge
 * @param state Output: homeostatic state
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_get_homeostatic_state(
    const hypo_training_bridge_t* bridge,
    hypo_training_homeostatic_state_t* state
);

/**
 * WHAT: Get current training state assessment
 * WHY: Quick query of training health
 * HOW: Return cached state
 *
 * @param bridge Training bridge
 * @param state Output: training state
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_get_training_state(
    const hypo_training_bridge_t* bridge,
    hypo_training_state_t* state
);

/**
 * WHAT: Update homeostatic loss setpoint
 * WHY: Adjust target loss as training progresses
 * HOW: Set new setpoint, adjust tolerance
 *
 * @param bridge Training bridge
 * @param new_setpoint New loss setpoint
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_set_loss_setpoint(
    hypo_training_bridge_t* bridge,
    float new_setpoint
);

/* ============================================================================
 * DRIVE STATE
 * ============================================================================ */

/**
 * WHAT: Get current drive states
 * WHY: Query drive contributions to training
 * HOW: Copy internal drive state
 *
 * @param bridge Training bridge
 * @param drive_state Output: drive state
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_get_drive_state(
    const hypo_training_bridge_t* bridge,
    hypo_training_drive_state_t* drive_state
);

/**
 * WHAT: Manually set drive activation
 * WHY: External drive control for testing/overrides
 * HOW: Set specified drive level
 *
 * @param bridge Training bridge
 * @param drive_type Which drive to set (0=curiosity, 1=safety, 2=competence, 3=fatigue, 4=autonomy)
 * @param activation Activation level [0.0, 1.0]
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_set_drive(
    hypo_training_bridge_t* bridge,
    uint32_t drive_type,
    float activation
);

/**
 * WHAT: Reset fatigue accumulation
 * WHY: After consolidation phase
 * HOW: Reset fatigue to zero
 *
 * @param bridge Training bridge
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_reset_fatigue(hypo_training_bridge_t* bridge);

/* ============================================================================
 * STATISTICS AND MONITORING
 * ============================================================================ */

/**
 * WHAT: Get bridge statistics
 * WHY: Monitor bridge performance
 * HOW: Copy accumulated statistics
 *
 * @param bridge Training bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_get_stats(
    const hypo_training_bridge_t* bridge,
    hypo_training_bridge_stats_t* stats
);

/**
 * WHAT: Reset bridge statistics
 * WHY: Clear for new measurement period
 * HOW: Zero all counters
 *
 * @param bridge Training bridge
 * @return 0 on success, -1 on failure
 */
int hypo_training_bridge_reset_stats(hypo_training_bridge_t* bridge);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get string name for training state
 * WHY: Human-readable state identification
 * HOW: Lookup table
 *
 * @param state Training state
 * @return String name (never NULL)
 */
const char* hypo_training_state_name(hypo_training_state_t state);

/**
 * WHAT: Get string name for consolidation type
 * WHY: Human-readable consolidation identification
 * HOW: Lookup table
 *
 * @param type Consolidation type
 * @return String name (never NULL)
 */
const char* hypo_consolidation_type_name(hypo_consolidation_type_t type);

/**
 * WHAT: Get string name for modulation type
 * WHY: Human-readable modulation identification
 * HOW: Lookup table
 *
 * @param type Modulation type
 * @return String name (never NULL)
 */
const char* hypo_training_modulation_name(hypo_training_modulation_type_t type);

/**
 * WHAT: Print bridge summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print bridge state
 *
 * @param bridge Training bridge (NULL safe)
 */
void hypo_training_bridge_print_summary(const hypo_training_bridge_t* bridge);

/**
 * WHAT: Print bridge statistics to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print statistics
 *
 * @param stats Statistics to print (NULL safe)
 */
void hypo_training_bridge_print_stats(const hypo_training_bridge_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_TRAINING_BRIDGE_H */

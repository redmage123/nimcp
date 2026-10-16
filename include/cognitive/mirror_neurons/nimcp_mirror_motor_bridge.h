/**
 * @file nimcp_mirror_motor_bridge.h
 * @brief Mirror Neuron - Motor Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Bidirectional integration between mirror neurons and motor cortex
 * WHY:  Enable motor program extraction from observation and imitation execution
 * HOW:  F5 mirror neurons project to M1 motor cortex for imitation learning
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * F5-M1 MIRROR-MOTOR PATHWAY:
 * ---------------------------
 * 1. F5 Mirror Neurons (Premotor Cortex):
 *    - Fire during both action observation and execution
 *    - Encode goal-directed motor programs
 *    - Project to primary motor cortex (M1)
 *    - Reference: Rizzolatti & Craighero (2004) "The mirror-neuron system"
 *
 * 2. M1 Primary Motor Cortex:
 *    - Receives F5 input via cortico-cortical connections
 *    - Executes motor programs when suppression is released
 *    - Somatotopic organization preserved
 *    - Reference: Fadiga et al. (1995) "Motor facilitation during action observation"
 *
 * 3. Motor Resonance Pathway:
 *    - Observation -> F5 activation -> M1 priming
 *    - Basal ganglia gates execution
 *    - Learning context releases suppression
 *    - Reference: Brass et al. (2001) "Movement observation affects movement execution"
 *
 * MOTOR PROGRAM EXTRACTION:
 * -------------------------
 * 1. Visual Analysis:
 *    - Observed movement decomposed into motor primitives
 *    - Kinematic features extracted (velocity, trajectory)
 *    - Body part mapping (hand, arm, face)
 *
 * 2. Motor Encoding:
 *    - Primitives mapped to motor programs
 *    - Temporal sequence preserved
 *    - Force/precision parameters estimated
 *
 * 3. Execution Pathway:
 *    - F5 goal -> premotor planning -> M1 execution
 *    - Cerebellar timing coordination
 *    - Proprioceptive feedback loop
 *
 * ARCHITECTURE:
 * ```
 * +------------------+         +------------------+
 * |  MIRROR NEURONS  |         |   MOTOR CORTEX   |
 * |       (F5)       |         |      (M1)        |
 * +--------+---------+         +--------+---------+
 *          |                            |
 *          | motor_program              | motor_command
 *          |                            |
 *          v                            v
 * +--------------------------------------------------+
 * |           MIRROR-MOTOR BRIDGE                     |
 * |                                                   |
 * |  [Observation] -> [Program Extract] -> [Execute]  |
 * |  [Resonance]   -> [Suppression Gate] -> [M1 Out]  |
 * +--------------------------------------------------+
 * ```
 *
 * Bio-async Module ID: 0x027C
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_MIRROR_MOTOR_BRIDGE_H
#define NIMCP_MIRROR_MOTOR_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/mirror_neurons/nimcp_mirror_resonance.h"
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Note: BIO_MODULE_MIRROR_MOTOR_BRIDGE is defined in nimcp_bio_messages.h */

/** @brief Maximum motor programs that can be extracted */
#define MIRROR_MOTOR_MAX_PROGRAMS       64

/** @brief Maximum effectors for imitation */
#define MIRROR_MOTOR_MAX_EFFECTORS      32

/** @brief Default resonance-to-motor gain */
#define MIRROR_MOTOR_DEFAULT_GAIN       0.8f

/** @brief Default execution threshold */
#define MIRROR_MOTOR_EXEC_THRESHOLD     0.7f

/** @brief Default F5->M1 transmission delay (ms) */
#define MIRROR_MOTOR_F5_M1_DELAY_MS     15.0f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct mirror_motor_bridge mirror_motor_bridge_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Motor program extracted from observation
 *
 * WHAT: Represents a motor program derived from observed action
 * WHY:  Enable imitation by encoding observed movements
 */
typedef struct {
    uint32_t program_id;              /**< Unique program identifier */
    uint32_t action_id;               /**< Source action from mirror system */
    motor_region_t primary_region;    /**< Primary body region */
    movement_type_t type;             /**< Movement type classification */

    /* Kinematic parameters */
    motor_vec3_t start_position;      /**< Starting position */
    motor_vec3_t end_position;        /**< Target end position */
    motor_vec3_t peak_velocity;       /**< Peak velocity during movement */
    float duration_ms;                /**< Estimated movement duration */
    float force_estimate;             /**< Estimated force [0, 1] */
    float precision_required;         /**< Required precision [0, 1] */

    /* Extraction metadata */
    float extraction_confidence;      /**< Confidence in extraction [0, 1] */
    float observation_strength;       /**< Original observation strength */
    uint64_t extraction_time_ms;      /**< When extracted */
    uint32_t observation_count;       /**< Times observed */
} extracted_motor_program_t;

/**
 * @brief Imitation execution state
 *
 * WHAT: Tracks execution of imitated movement
 * WHY:  Monitor imitation progress and accuracy
 */
typedef struct {
    uint32_t program_id;              /**< Program being executed */
    uint32_t action_id;               /**< Original action ID */

    /* Execution state */
    bool is_executing;                /**< Currently executing */
    float execution_progress;         /**< Progress [0, 1] */
    float resonance_level;            /**< Current resonance from mirror */
    float suppression_level;          /**< Current suppression level */
    float motor_output;               /**< Actual motor output [0, 1] */

    /* Timing */
    uint64_t start_time_ms;           /**< Execution start time */
    uint64_t elapsed_ms;              /**< Time elapsed */
    float planned_duration_ms;        /**< Planned duration */

    /* Accuracy tracking */
    motor_vec3_t current_position;    /**< Current effector position */
    motor_vec3_t position_error;      /**< Error from planned trajectory */
    float accuracy;                   /**< Current accuracy [0, 1] */
} imitation_state_t;

/**
 * @brief Configuration for mirror-motor bridge
 */
typedef struct {
    /* Resonance-motor coupling */
    float resonance_gain;             /**< Resonance to motor gain (default: 0.8) */
    float execution_threshold;        /**< Threshold to trigger execution (default: 0.7) */
    float f5_m1_delay_ms;             /**< F5->M1 transmission delay (default: 15ms) */

    /* Program extraction */
    float min_observation_strength;   /**< Min strength for extraction (default: 0.3) */
    float extraction_threshold;       /**< Confidence threshold for extraction (default: 0.5) */
    uint32_t min_observations;        /**< Min observations before extraction (default: 1) */

    /* Execution control */
    bool enable_automatic_imitation;  /**< Allow automatic imitation (default: false) */
    bool enable_learning_mode;        /**< Learning mode releases suppression (default: true) */
    float learning_release_strength;  /**< Suppression release in learning mode (default: 0.8) */

    /* Motor adaptation */
    bool enable_motor_adaptation;     /**< Adapt motor programs from feedback (default: true) */
    float adaptation_rate;            /**< Motor program adaptation rate (default: 0.1) */

    /* Feature enables */
    bool enable_program_extraction;   /**< Extract programs from observations (default: true) */
    bool enable_resonance_gating;     /**< Gate execution by resonance (default: true) */
    bool enable_cerebellar_timing;    /**< Use cerebellar timing (default: false) */
} mirror_motor_config_t;

/**
 * @brief Effects of mirror neurons on motor cortex
 */
typedef struct {
    /* Resonance effects */
    float current_resonance;          /**< Current resonance level */
    float motor_priming;              /**< Motor system priming [0, 1] */
    float execution_readiness;        /**< Readiness for execution [0, 1] */

    /* Program extraction */
    uint32_t programs_extracted;      /**< Programs extracted this cycle */
    uint32_t active_programs;         /**< Currently active programs */

    /* Execution effects */
    uint32_t executions_triggered;    /**< Executions triggered this cycle */
    float avg_execution_accuracy;     /**< Average execution accuracy */
} mirror_motor_effects_t;

/**
 * @brief Current state of mirror-motor interaction
 */
typedef struct {
    /* Mirror neuron state */
    uint32_t active_mirror_channels;  /**< Active resonance channels */
    float max_resonance;              /**< Maximum resonance level */
    float mean_resonance;             /**< Mean resonance across channels */

    /* Motor cortex state */
    motor_status_t motor_status;      /**< Current motor status */
    uint32_t active_effectors;        /**< Active effector count */
    uint32_t queued_programs;         /**< Programs queued for execution */

    /* Bridge state */
    uint32_t extracted_programs;      /**< Total extracted programs */
    uint32_t executing_programs;      /**< Currently executing */
    bool in_learning_mode;            /**< Learning mode active */
    bool suppression_released;        /**< Suppression currently released */

    /* Performance */
    float extraction_rate;            /**< Extractions per second */
    float execution_rate;             /**< Executions per second */
} mirror_motor_state_t;

/**
 * @brief Statistics for mirror-motor bridge
 */
typedef struct {
    /* Extraction stats */
    uint64_t total_extractions;       /**< Total programs extracted */
    uint64_t successful_extractions;  /**< Successful extractions */
    uint64_t failed_extractions;      /**< Failed extractions */
    float avg_extraction_confidence;  /**< Average confidence */

    /* Execution stats */
    uint64_t total_executions;        /**< Total execution attempts */
    uint64_t successful_executions;   /**< Successful completions */
    uint64_t suppressed_executions;   /**< Executions suppressed */
    float avg_execution_accuracy;     /**< Average accuracy */

    /* Timing stats */
    float avg_extraction_latency_ms;  /**< Average extraction time */
    float avg_execution_latency_ms;   /**< Average execution time */
    float avg_f5_m1_delay_ms;         /**< Average F5->M1 delay */

    /* Learning stats */
    uint64_t learning_mode_activations; /**< Times learning mode activated */
    uint64_t motor_adaptations;       /**< Motor program adaptations */
} mirror_motor_stats_t;

/**
 * @brief Mirror-motor bridge state
 */
struct mirror_motor_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */
    struct nimcp_health_agent* health_agent; /**< Instance-level health agent */

    /* Configuration */
    mirror_motor_config_t config;

    /* Connected systems */
    motor_resonance_t resonance;      /**< Motor resonance system */
    motor_adapter_t* motor;           /**< Motor cortex adapter */

    /* Extracted programs */
    extracted_motor_program_t* programs;
    uint32_t num_programs;
    uint32_t max_programs;

    /* Execution tracking */
    imitation_state_t* executions;
    uint32_t num_executions;
    uint32_t max_executions;

    /* Current effects */
    mirror_motor_effects_t effects;
    mirror_motor_state_t state;

    /* Statistics */
    mirror_motor_stats_t stats;

    /* Internal state */
    bool learning_mode;
    float current_suppression_release;
    uint64_t last_update_ms;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default mirror-motor configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds based on F5-M1 pathway properties
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int mirror_motor_bridge_default_config(mirror_motor_config_t* config);

/**
 * @brief Create mirror-motor bridge
 *
 * WHAT: Initialize mirror neuron to motor cortex integration bridge
 * WHY:  Enable observation-based motor learning and imitation
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
mirror_motor_bridge_t* mirror_motor_bridge_create(
    const mirror_motor_config_t* config
);

/**
 * @brief Destroy mirror-motor bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void mirror_motor_bridge_destroy(mirror_motor_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect motor resonance system
 *
 * WHAT: Link bridge to motor resonance (F5 mirror neurons)
 * WHY:  Source of motor resonance signals
 * HOW:  Store resonance handle for observation monitoring
 *
 * @param bridge Mirror-motor bridge
 * @param resonance Motor resonance system
 * @return 0 on success
 */
int mirror_motor_bridge_connect_resonance(
    mirror_motor_bridge_t* bridge,
    motor_resonance_t resonance
);

/**
 * @brief Connect motor cortex adapter
 *
 * WHAT: Link bridge to motor cortex (M1)
 * WHY:  Target for motor program execution
 * HOW:  Store motor adapter handle for command output
 *
 * @param bridge Mirror-motor bridge
 * @param motor Motor cortex adapter
 * @return 0 on success
 */
int mirror_motor_bridge_connect_motor(
    mirror_motor_bridge_t* bridge,
    motor_adapter_t* motor
);

/* ============================================================================
 * Motor Program Extraction API
 * ============================================================================ */

/**
 * @brief Extract motor program from observation
 *
 * WHAT: Convert observed action to executable motor program
 * WHY:  Enable imitation by encoding observed movements
 * HOW:  Analyze resonance pattern, extract kinematic features
 *
 * @param bridge Mirror-motor bridge
 * @param action_id Action ID from mirror neurons
 * @param observation_strength Observation strength [0, 1]
 * @param region Target body region
 * @return Program ID on success, UINT32_MAX on failure
 */
uint32_t mirror_motor_extract_program(
    mirror_motor_bridge_t* bridge,
    uint32_t action_id,
    float observation_strength,
    motor_region_t region
);

/**
 * @brief Extract motor program with full parameters
 *
 * WHAT: Extract program with explicit kinematic parameters
 * WHY:  Allow precise specification of movement parameters
 * HOW:  Create program from provided parameters
 *
 * @param bridge Mirror-motor bridge
 * @param action_id Source action ID
 * @param start Start position
 * @param end End position
 * @param duration_ms Movement duration
 * @param region Body region
 * @param type Movement type
 * @return Program ID on success, UINT32_MAX on failure
 */
uint32_t mirror_motor_extract_program_full(
    mirror_motor_bridge_t* bridge,
    uint32_t action_id,
    const motor_vec3_t* start,
    const motor_vec3_t* end,
    float duration_ms,
    motor_region_t region,
    movement_type_t type
);

/**
 * @brief Get extracted motor program
 *
 * WHAT: Retrieve details of extracted program
 * WHY:  Inspect program before execution
 * HOW:  Look up program by ID
 *
 * @param bridge Mirror-motor bridge
 * @param program_id Program identifier
 * @param program Output program structure
 * @return 0 on success, -1 if not found
 */
int mirror_motor_get_program(
    const mirror_motor_bridge_t* bridge,
    uint32_t program_id,
    extracted_motor_program_t* program
);

/* ============================================================================
 * Imitation Execution API
 * ============================================================================ */

/**
 * @brief Execute extracted motor program
 *
 * WHAT: Initiate execution of extracted program on motor cortex
 * WHY:  Perform imitation of observed action
 * HOW:  Check suppression, send program to M1 if released
 *
 * @param bridge Mirror-motor bridge
 * @param program_id Program to execute
 * @return 0 on success, -1 on error or suppressed
 */
int mirror_motor_execute_program(
    mirror_motor_bridge_t* bridge,
    uint32_t program_id
);

/**
 * @brief Execute program from current resonance
 *
 * WHAT: Execute program for action with highest resonance
 * WHY:  Automatic imitation when suppression is released
 * HOW:  Find peak resonance, execute corresponding program
 *
 * @param bridge Mirror-motor bridge
 * @return Program ID being executed, UINT32_MAX if none
 */
uint32_t mirror_motor_execute_from_resonance(
    mirror_motor_bridge_t* bridge
);

/**
 * @brief Get imitation execution state
 *
 * WHAT: Get current state of executing imitation
 * WHY:  Monitor imitation progress
 * HOW:  Look up execution by program ID
 *
 * @param bridge Mirror-motor bridge
 * @param program_id Program identifier
 * @param state Output execution state
 * @return 0 on success, -1 if not executing
 */
int mirror_motor_get_execution_state(
    const mirror_motor_bridge_t* bridge,
    uint32_t program_id,
    imitation_state_t* state
);

/**
 * @brief Stop execution of program
 *
 * WHAT: Halt ongoing imitation execution
 * WHY:  Allow cancellation of imitation
 * HOW:  Send stop to motor cortex, clear execution state
 *
 * @param bridge Mirror-motor bridge
 * @param program_id Program to stop
 * @return 0 on success
 */
int mirror_motor_stop_execution(
    mirror_motor_bridge_t* bridge,
    uint32_t program_id
);

/* ============================================================================
 * Learning Mode API
 * ============================================================================ */

/**
 * @brief Enter learning mode
 *
 * WHAT: Release suppression to allow imitation
 * WHY:  Enable imitation learning when appropriate
 * HOW:  Reduce BG suppression, allow resonance to drive motor
 *
 * @param bridge Mirror-motor bridge
 * @param strength Learning mode strength [0, 1]
 * @return 0 on success
 */
int mirror_motor_enter_learning_mode(
    mirror_motor_bridge_t* bridge,
    float strength
);

/**
 * @brief Exit learning mode
 *
 * WHAT: Restore suppression to prevent automatic imitation
 * WHY:  Return to normal observation without imitation
 * HOW:  Restore BG tonic inhibition
 *
 * @param bridge Mirror-motor bridge
 * @return 0 on success
 */
int mirror_motor_exit_learning_mode(
    mirror_motor_bridge_t* bridge
);

/**
 * @brief Check if in learning mode
 *
 * @param bridge Mirror-motor bridge
 * @return true if learning mode active
 */
bool mirror_motor_is_learning_mode(
    const mirror_motor_bridge_t* bridge
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update mirror-motor bridge state
 *
 * WHAT: Main update loop for F5-M1 integration
 * WHY:  Keep mirror neurons and motor cortex synchronized
 * HOW:  Process resonance, update executions, adapt programs
 *
 * @param bridge Mirror-motor bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int mirror_motor_bridge_update(
    mirror_motor_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Mirror-motor bridge
 * @param state Output state
 * @return 0 on success
 */
int mirror_motor_bridge_get_state(
    const mirror_motor_bridge_t* bridge,
    mirror_motor_state_t* state
);

/**
 * @brief Get bridge effects
 *
 * @param bridge Mirror-motor bridge
 * @param effects Output effects
 * @return 0 on success
 */
int mirror_motor_bridge_get_effects(
    const mirror_motor_bridge_t* bridge,
    mirror_motor_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Mirror-motor bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int mirror_motor_bridge_get_stats(
    const mirror_motor_bridge_t* bridge,
    mirror_motor_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Mirror-motor bridge
 * @return 0 on success
 */
int mirror_motor_bridge_reset_stats(
    mirror_motor_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for mirror-motor coordination
 * WHY:  Distributed motor learning signaling
 * HOW:  Register module with ID 0x027C, set up handlers
 *
 * @param bridge Mirror-motor bridge
 * @return 0 on success
 */
int mirror_motor_bridge_connect_bio_async(
    mirror_motor_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Mirror-motor bridge
 * @return 0 on success
 */
int mirror_motor_bridge_disconnect_bio_async(
    mirror_motor_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Mirror-motor bridge
 * @return true if bio-async enabled
 */
bool mirror_motor_bridge_is_bio_async_connected(
    const mirror_motor_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_MOTOR_BRIDGE_H */

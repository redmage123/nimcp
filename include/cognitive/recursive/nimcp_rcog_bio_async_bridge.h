/**
 * @file nimcp_rcog_bio_async_bridge.h
 * @brief Bio-Async Integration Bridge for Recursive Cognition
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Bidirectional bridge connecting recursive cognition with bio-async system
 * WHY:  Bio-async provides coordination, futures, and neuromodulator-based signaling
 * HOW:  Full bridge pattern with message handlers and phase coupling
 *
 * BIOLOGICAL BASIS:
 * Recursive cognitive processes require coordination across brain regions:
 * - Dopamine signals subtask completion and reward
 * - Norepinephrine signals priority/urgency
 * - Acetylcholine modulates attention to context variables
 * - Serotonin reflects overall processing state
 * - Phase coupling synchronizes parallel subtask execution
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * | RECURSIVE COGNITION  |                    |     BIO-ASYNC        |
 * |                      |                    |                      |
 * | - Context Store      |<-- neuromodulator->| - Message Router     |
 * | - Orchestrator       |    channels        | - Future Manager     |
 * | - Delegation Pool    |                    | - Phase Sync         |
 * | - Answer Refiner     |<-- phase coupling->| - Glial Waves        |
 * |                      |                    | - Oscillators        |
 * +----------------------+                    +----------------------+
 *           |                                           |
 *           +---------------- BRIDGE -------------------+
 *                      (bidirectional flow)
 * ```
 *
 * MODULE ID ALLOCATION (from design doc):
 * - 0x1400: Recursive Cognition Engine
 * - 0x1401: Context Store
 * - 0x1402: Orchestrator
 * - 0x1403: Delegation Pool
 * - 0x1404: Answer Refiner
 * - 0x1405: Tool Router
 */

#ifndef NIMCP_RCOG_BIO_ASYNC_BRIDGE_H
#define NIMCP_RCOG_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_engine;
struct rcog_context_store;
struct rcog_orchestrator;
struct rcog_delegation_pool;
struct rcog_answer_refiner;
struct nimcp_bio_async;
struct nimcp_bio_future;
struct nimcp_phase_sync;
struct nimcp_glial_wave;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Module ID for recursive cognition engine */
#define RCOG_BIO_MODULE_ENGINE          0x1400

/** Module ID for context store */
#define RCOG_BIO_MODULE_CONTEXT         0x1401

/** Module ID for orchestrator */
#define RCOG_BIO_MODULE_ORCHESTRATOR    0x1402

/** Module ID for delegation pool */
#define RCOG_BIO_MODULE_DELEGATION      0x1403

/** Module ID for answer refiner */
#define RCOG_BIO_MODULE_ANSWER          0x1404

/** Module ID for tool router */
#define RCOG_BIO_MODULE_TOOLS           0x1405

/** Default dopamine release on subtask completion */
#define RCOG_BIO_DEFAULT_DOPAMINE_RELEASE   0.3f

/** Default norepinephrine threshold for priority escalation */
#define RCOG_BIO_DEFAULT_NE_PRIORITY_THRESHOLD  0.7f

/** Default coherence threshold for phase coupling */
#define RCOG_BIO_DEFAULT_COHERENCE_THRESHOLD    0.8f

/*=============================================================================
 * MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief Bio-async message types for recursive cognition
 */
typedef enum {
    /* Engine messages (0x1400xx) */
    RCOG_MSG_ENGINE_STARTED         = 0x140001,
    RCOG_MSG_ENGINE_STOPPED         = 0x140002,
    RCOG_MSG_GOAL_RECEIVED          = 0x140003,
    RCOG_MSG_PROCESSING_COMPLETE    = 0x140004,

    /* Context store messages (0x1401xx) */
    RCOG_MSG_CONTEXT_LOADED         = 0x140101,
    RCOG_MSG_CONTEXT_QUERIED        = 0x140102,
    RCOG_MSG_CONTEXT_CLEARED        = 0x140103,
    RCOG_MSG_CONTEXT_SHARED         = 0x140104,

    /* Orchestrator messages (0x1402xx) */
    RCOG_MSG_DECOMPOSITION_START    = 0x140201,
    RCOG_MSG_DECOMPOSITION_COMPLETE = 0x140202,
    RCOG_MSG_DEPTH_LIMIT_REACHED    = 0x140203,
    RCOG_MSG_STRATEGY_CHANGED       = 0x140204,

    /* Delegation messages (0x1403xx) */
    RCOG_MSG_BATCH_SUBMITTED        = 0x140301,
    RCOG_MSG_SUBTASK_STARTED        = 0x140302,
    RCOG_MSG_SUBTASK_COMPLETED      = 0x140303,
    RCOG_MSG_BATCH_COMPLETED        = 0x140304,
    RCOG_MSG_WORKER_SPAWNED         = 0x140305,
    RCOG_MSG_WORKER_TERMINATED      = 0x140306,

    /* Answer messages (0x1404xx) */
    RCOG_MSG_REFINEMENT_STEP        = 0x140401,
    RCOG_MSG_ANSWER_READY           = 0x140402,
    RCOG_MSG_CONVERGENCE_ACHIEVED   = 0x140403,
    RCOG_MSG_ANSWER_STALLED         = 0x140404,

    /* Tool messages (0x1405xx) */
    RCOG_MSG_TOOL_INVOKED           = 0x140501,
    RCOG_MSG_TOOL_COMPLETED         = 0x140502,
    RCOG_MSG_TOOL_ACCESS_DENIED     = 0x140503
} rcog_bio_message_type_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from recursive cognition to bio-async
 *
 * WHAT: Signals generated by recursive cognition for system-wide coordination
 * WHY:  Other brain modules need to know about cognitive processing state
 */
typedef struct {
    /* Dopamine channel - reward/completion */
    float dopamine_release;              /**< Subtask completion reward [0.0-1.0] */
    uint32_t completed_subtask_count;    /**< Number of subtasks completed */
    float answer_confidence;             /**< Current answer confidence */

    /* Norepinephrine channel - priority/urgency */
    float norepinephrine_level;          /**< Priority/urgency level [0.0-1.0] */
    bool priority_escalation;            /**< Whether priority was escalated */
    uint32_t timeout_warnings;           /**< Number of timeout warnings */

    /* Acetylcholine channel - attention/focus */
    float acetylcholine_level;           /**< Attention level [0.0-1.0] */
    const char* focused_variable;        /**< Currently focused context variable */
    float context_access_frequency;      /**< How often context is being accessed */

    /* Serotonin channel - state/mood */
    float serotonin_level;               /**< Processing state level [0.0-1.0] */
    rcog_answer_status_t processing_state; /**< Current processing state */
    bool long_horizon_active;            /**< Whether long-horizon processing is active */

    /* Phase coupling requests */
    bool request_phase_sync;             /**< Request phase synchronization */
    uint32_t phase_sync_subtask_count;   /**< Number of subtasks to synchronize */
    float desired_coherence;             /**< Desired coherence level */

    /* Glial wave triggers */
    bool trigger_glial_wave;             /**< Trigger a glial wave */
    rcog_state_transition_t transition;  /**< State transition type */
} rcog_to_bio_async_effects_t;

/**
 * @brief Effects flowing from bio-async to recursive cognition
 *
 * WHAT: Signals from bio-async that modulate recursive cognition
 * WHY:  Recursive cognition should adapt to system-wide state
 */
typedef struct {
    /* Global state effects */
    float global_arousal;                /**< Global arousal level [0.0-1.0] */
    float global_valence;                /**< Global valence [-1.0 to 1.0] */
    bool system_overload;                /**< System is overloaded */

    /* Phase synchronization status */
    bool phase_sync_achieved;            /**< Phase synchronization achieved */
    float current_coherence;             /**< Current phase coherence [0.0-1.0] */
    uint32_t synchronized_count;         /**< Number of synchronized processes */

    /* Oscillation band recommendations */
    uint8_t recommended_band;            /**< Recommended oscillation band */
    float band_power;                    /**< Power in recommended band */

    /* Glial wave effects */
    bool glial_wave_active;              /**< Glial wave is propagating */
    float wave_intensity;                /**< Intensity of current wave */
    uint32_t wave_origin_module;         /**< Module that initiated wave */

    /* Resource availability */
    float available_capacity;            /**< Available processing capacity [0.0-1.0] */
    uint32_t available_workers;          /**< Number of available worker slots */
    bool throttling_active;              /**< Whether throttling is active */

    /* Timing signals */
    uint64_t current_phase_ms;           /**< Current phase in milliseconds */
    float circadian_factor;              /**< Circadian modulation factor */
} bio_async_to_rcog_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bio-async bridge configuration
 */
typedef struct {
    /* Neuromodulator sensitivity */
    float dopamine_sensitivity;          /**< Sensitivity to dopamine signals */
    float norepinephrine_sensitivity;    /**< Sensitivity to NE signals */
    float acetylcholine_sensitivity;     /**< Sensitivity to ACh signals */
    float serotonin_sensitivity;         /**< Sensitivity to 5-HT signals */

    /* Phase coupling parameters */
    float coherence_threshold;           /**< Threshold for phase lock */
    float coupling_strength;             /**< Strength of phase coupling */
    uint8_t default_oscillation_band;    /**< Default oscillation band */

    /* Message routing */
    bool enable_message_logging;         /**< Log all bio-async messages */
    uint32_t message_queue_size;         /**< Size of message queue */

    /* Glial wave parameters */
    float glial_wave_threshold;          /**< Threshold to trigger glial wave */
    float glial_wave_decay;              /**< Decay rate of glial waves */
} rcog_bio_async_bridge_config_t;

/*=============================================================================
 * BRIDGE HANDLE
 *===========================================================================*/

/**
 * @brief Bio-async bridge opaque handle
 */
typedef struct rcog_bio_async_bridge rcog_bio_async_bridge_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create bio-async bridge with configuration
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
rcog_bio_async_bridge_t* rcog_bio_async_bridge_create(
    const rcog_bio_async_bridge_config_t* config
);

/**
 * @brief Create bridge with default configuration
 * @return Bridge handle or NULL on error
 */
rcog_bio_async_bridge_t* rcog_bio_async_bridge_create_default(void);

/**
 * @brief Destroy bio-async bridge
 * @param bridge Bridge handle (NULL safe)
 */
void rcog_bio_async_bridge_destroy(rcog_bio_async_bridge_t* bridge);

/**
 * @brief Get default configuration
 * @return Default configuration
 */
rcog_bio_async_bridge_config_t rcog_bio_async_bridge_default_config(void);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect bridge to bio-async system
 * @param bridge Bridge handle
 * @param bio_async Bio-async system handle
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_connect(
    rcog_bio_async_bridge_t* bridge,
    struct nimcp_bio_async* bio_async
);

/**
 * @brief Connect bridge to recursive cognition engine
 * @param bridge Bridge handle
 * @param engine Recursive cognition engine handle
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_connect_engine(
    rcog_bio_async_bridge_t* bridge,
    struct rcog_engine* engine
);

/**
 * @brief Disconnect from bio-async system
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_disconnect(rcog_bio_async_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge handle
 * @return true if connected
 */
bool rcog_bio_async_bridge_is_connected(const rcog_bio_async_bridge_t* bridge);

/*=============================================================================
 * UPDATE
 *===========================================================================*/

/**
 * @brief Update bridge state (call each frame/tick)
 * @param bridge Bridge handle
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_update(
    rcog_bio_async_bridge_t* bridge,
    float delta_time_ms
);

/*=============================================================================
 * MESSAGING
 *===========================================================================*/

/**
 * @brief Send message through bio-async
 * @param bridge Bridge handle
 * @param message_type Message type
 * @param payload Message payload
 * @param payload_size Payload size
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_send_message(
    rcog_bio_async_bridge_t* bridge,
    rcog_bio_message_type_t message_type,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Register message handler callback
 * @param bridge Bridge handle
 * @param message_type Message type to handle
 * @param handler Handler function
 * @param user_data User data for callback
 * @return 0 on success, error code on failure
 */
typedef void (*rcog_bio_message_handler_t)(
    rcog_bio_message_type_t type,
    const void* payload,
    size_t payload_size,
    void* user_data
);

int rcog_bio_async_bridge_register_handler(
    rcog_bio_async_bridge_t* bridge,
    rcog_bio_message_type_t message_type,
    rcog_bio_message_handler_t handler,
    void* user_data
);

/*=============================================================================
 * FUTURES
 *===========================================================================*/

/**
 * @brief Create a bio-future for subtask completion
 * @param bridge Bridge handle
 * @param subtask_id Subtask ID
 * @param future Output future handle
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_create_future(
    rcog_bio_async_bridge_t* bridge,
    uint64_t subtask_id,
    struct nimcp_bio_future** future
);

/**
 * @brief Wait for future completion
 * @param bridge Bridge handle
 * @param future Future handle
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, error code on timeout/failure
 */
int rcog_bio_async_bridge_await_future(
    rcog_bio_async_bridge_t* bridge,
    struct nimcp_bio_future* future,
    uint32_t timeout_ms
);

/*=============================================================================
 * PHASE COUPLING
 *===========================================================================*/

/**
 * @brief Create phase synchronization for subtask batch
 * @param bridge Bridge handle
 * @param subtask_ids Array of subtask IDs
 * @param count Number of subtasks
 * @param band Oscillation band
 * @param sync Output phase sync handle
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_create_phase_sync(
    rcog_bio_async_bridge_t* bridge,
    const uint64_t* subtask_ids,
    size_t count,
    uint8_t band,
    struct nimcp_phase_sync** sync
);

/**
 * @brief Wait for phase coherence
 * @param bridge Bridge handle
 * @param sync Phase sync handle
 * @param coherence_threshold Required coherence level
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, error code on timeout/failure
 */
int rcog_bio_async_bridge_wait_coherent(
    rcog_bio_async_bridge_t* bridge,
    struct nimcp_phase_sync* sync,
    float coherence_threshold,
    uint32_t timeout_ms
);

/*=============================================================================
 * GLIAL WAVES
 *===========================================================================*/

/**
 * @brief Initiate a glial wave for state transition
 * @param bridge Bridge handle
 * @param transition State transition type
 * @param wave Output wave handle
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_initiate_glial_wave(
    rcog_bio_async_bridge_t* bridge,
    rcog_state_transition_t transition,
    struct nimcp_glial_wave** wave
);

/*=============================================================================
 * NEUROMODULATOR CONTROL
 *===========================================================================*/

/**
 * @brief Release dopamine (subtask completion signal)
 * @param bridge Bridge handle
 * @param amount Amount to release [0.0-1.0]
 * @param subtask_id Associated subtask ID
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_release_dopamine(
    rcog_bio_async_bridge_t* bridge,
    float amount,
    uint64_t subtask_id
);

/**
 * @brief Signal priority escalation via norepinephrine
 * @param bridge Bridge handle
 * @param priority Priority level [0.0-1.0]
 * @param subtask_id Associated subtask ID
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_signal_priority(
    rcog_bio_async_bridge_t* bridge,
    float priority,
    uint64_t subtask_id
);

/**
 * @brief Modulate attention via acetylcholine
 * @param bridge Bridge handle
 * @param attention Attention level [0.0-1.0]
 * @param target Target variable or subtask name
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_modulate_attention(
    rcog_bio_async_bridge_t* bridge,
    float attention,
    const char* target
);

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

/**
 * @brief Get current effects from rcog to bio-async
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_get_outgoing_effects(
    const rcog_bio_async_bridge_t* bridge,
    rcog_to_bio_async_effects_t* effects
);

/**
 * @brief Get current effects from bio-async to rcog
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_get_incoming_effects(
    const rcog_bio_async_bridge_t* bridge,
    bio_async_to_rcog_effects_t* effects
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t futures_created;
    uint64_t futures_completed;
    uint64_t phase_syncs_created;
    uint64_t phase_syncs_achieved;
    uint64_t glial_waves_initiated;
    float avg_coherence;
    float avg_dopamine_release;
    uint64_t total_update_time_us;
} rcog_bio_async_bridge_stats_t;

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_bio_async_bridge_get_stats(
    const rcog_bio_async_bridge_t* bridge,
    rcog_bio_async_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 */
void rcog_bio_async_bridge_reset_stats(rcog_bio_async_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_BIO_ASYNC_BRIDGE_H */

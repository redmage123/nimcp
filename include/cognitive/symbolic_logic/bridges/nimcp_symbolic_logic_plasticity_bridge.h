/**
 * @file nimcp_symbolic_logic_plasticity_bridge.h
 * @brief Safety-Plasticity Bridge - Neuromodulatory Learning from Safety Events
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Bridge connecting safety/compliance events to neuromodulatory plasticity
 * WHY:  Enable the system to LEARN from safety violations through neuromodulatory feedback
 * HOW:  Maps safety events to DA/5-HT/NE/ACh deltas, applies to plasticity orchestrator
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PUNISHMENT LEARNING IN BIOLOGICAL SYSTEMS:
 * -------------------------------------------
 * The brain's safety systems are deeply integrated with learning mechanisms:
 * - Amygdala detects threats, triggers NE/cortisol release
 * - Habenula signals negative prediction errors (disappointment)
 * - VTA dopamine dips signal "worse than expected" outcomes
 * - This creates powerful learning to AVOID harmful behaviors
 *
 * NEUROMODULATORY MAPPING:
 * ------------------------
 * Different safety events trigger specific neuromodulator patterns:
 *
 * 1. VIOLATION_BLOCKED:
 *    - Strong DA depression (-0.8): "This was bad, don't do it again"
 *    - Triggers negative dopamine burst (phasic dip)
 *    - Creates strong LTD at recently active synapses
 *
 * 2. VIOLATION_ESCALATED:
 *    - Moderate DA depression (-0.3): "Getting worse"
 *    - Strong NE elevation (+0.5): "Pay attention, danger"
 *    - Heightens vigilance and attention
 *
 * 3. COMPLIANCE:
 *    - Small DA elevation (+0.2): "Good behavior rewarded"
 *    - Reinforces safe behavioral patterns
 *
 * 4. OVERRIDE_ACCEPTED:
 *    - DA elevation (+0.5): "Authorized exception is OK"
 *    - 5-HT elevation (+0.3): "This is acceptable, be calm"
 *    - Reduces stress response to authorized deviations
 *
 * 5. OVERRIDE_REJECTED:
 *    - Maximum DA depression (-1.0): "Absolute prohibition"
 *    - Maximum NE elevation (+1.0): "Maximum alert"
 *    - Creates strongest possible avoidance learning
 *
 * 6. DECEPTION_DETECTED:
 *    - Maximum DA depression (-1.0): "Trust violation"
 *    - Triggers strong LTD across deception-related pathways
 *    - System learns to distrust deceptive patterns
 *
 * 7. INTEGRITY_VERIFIED:
 *    - Small positive reinforcement
 *    - Strengthens verified behavioral patterns
 *
 * 8. INTEGRITY_FAILED:
 *    - SYSTEM HALT: Cannot be learned around
 *    - Fundamental integrity failure requires external intervention
 *
 * LEARNING RATE MODULATION:
 * -------------------------
 * Safety events modulate learning rate to ensure rapid acquisition:
 * - Severe violations → high learning rate (learn fast!)
 * - Minor compliance → normal learning rate
 * - This ensures safety lessons are learned quickly and retained
 *
 * ARCHITECTURE:
 * ```
 * +==========================================================================+
 * |                    SAFETY-PLASTICITY BRIDGE                              |
 * +==========================================================================+
 * |                                                                          |
 * |   SAFETY LAYER                        PLASTICITY LAYER                   |
 * |   +------------------+                +-------------------+              |
 * |   | VIOLATION_BLOCKED|----DA=-0.8--->| LTD Trigger       |              |
 * |   | Rule violation   |    burst=neg  | Weight decrease   |              |
 * |   +------------------+                +-------------------+              |
 * |                                                                          |
 * |   +------------------+                +-------------------+              |
 * |   | OVERRIDE_REJECTED|----DA=-1.0--->| Maximum LTD       |              |
 * |   | Unauthorized     |    NE=+1.0    | Emergency learning|              |
 * |   +------------------+                +-------------------+              |
 * |                                                                          |
 * |   +------------------+                +-------------------+              |
 * |   | DECEPTION_DETECT |----DA=-1.0--->| Trust pathway LTD |              |
 * |   | Manipulation     |    LTD=strong | Distrust learning |              |
 * |   +------------------+                +-------------------+              |
 * |                                                                          |
 * |   +------------------+                +-------------------+              |
 * |   | COMPLIANCE       |----DA=+0.2--->| Mild LTP          |              |
 * |   | Good behavior    |               | Reinforce patterns|              |
 * |   +------------------+                +-------------------+              |
 * |                                                                          |
 * |   +------------------+                                                   |
 * |   | INTEGRITY_FAILED |---> SYSTEM HALT (no plasticity bypass)           |
 * |   +------------------+                                                   |
 * |                                                                          |
 * +==========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_PLASTICITY_BRIDGE_H
#define NIMCP_SYMBOLIC_LOGIC_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SAFETY_PLASTICITY_BRIDGE_MAGIC      0x53504C42  /* "SPLB" */
#define SAFETY_PLASTICITY_MODULE_ID         0x1630
#define SAFETY_PLASTICITY_MODULE_NAME       "safety_plasticity_bridge"

/* Default neuromodulator deltas for each event type */
#define SAFETY_DA_VIOLATION_BLOCKED        -0.8f
#define SAFETY_DA_VIOLATION_ESCALATED      -0.3f
#define SAFETY_NE_VIOLATION_ESCALATED      +0.5f
#define SAFETY_DA_COMPLIANCE               +0.2f
#define SAFETY_DA_OVERRIDE_ACCEPTED        +0.5f
#define SAFETY_5HT_OVERRIDE_ACCEPTED       +0.3f
#define SAFETY_DA_OVERRIDE_REJECTED        -1.0f
#define SAFETY_NE_OVERRIDE_REJECTED        +1.0f
#define SAFETY_DA_DECEPTION_DETECTED       -1.0f
#define SAFETY_DA_INTEGRITY_VERIFIED       +0.1f
#define SAFETY_ACH_INTEGRITY_VERIFIED      +0.2f

/* Learning rate modifiers */
#define SAFETY_LR_VIOLATION_BLOCKED         2.0f   /* Learn violations fast */
#define SAFETY_LR_VIOLATION_ESCALATED       2.5f   /* Learn escalations faster */
#define SAFETY_LR_COMPLIANCE                1.0f   /* Normal learning */
#define SAFETY_LR_OVERRIDE_ACCEPTED         0.8f   /* Slightly slower for overrides */
#define SAFETY_LR_OVERRIDE_REJECTED         3.0f   /* Maximum learning for rejections */
#define SAFETY_LR_DECEPTION_DETECTED        3.0f   /* Maximum learning for deception */

/* Maximum events to track */
#define SAFETY_MAX_EVENT_HISTORY           256
#define SAFETY_MAX_RULE_NAME_LENGTH        128
#define SAFETY_MAX_MODULE_NAME_LENGTH       64
#define SAFETY_MAX_CALLBACKS                32

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Safety event types
 *
 * WHAT: Classification of safety-related events
 * WHY:  Different events require different neuromodulatory responses
 * HOW:  Enum maps to specific DA/5-HT/NE/ACh response patterns
 */
typedef enum {
    SAFETY_EVENT_VIOLATION_BLOCKED = 0,   /**< Safety rule blocked harmful action */
    SAFETY_EVENT_VIOLATION_ESCALATED,     /**< Violation was escalated (repeated/severe) */
    SAFETY_EVENT_COMPLIANCE,              /**< Action complied with safety rules */
    SAFETY_EVENT_OVERRIDE_ACCEPTED,       /**< Authorized override was accepted */
    SAFETY_EVENT_OVERRIDE_REJECTED,       /**< Unauthorized override was rejected */
    SAFETY_EVENT_DECEPTION_DETECTED,      /**< Deceptive behavior was detected */
    SAFETY_EVENT_INTEGRITY_VERIFIED,      /**< System integrity check passed */
    SAFETY_EVENT_INTEGRITY_FAILED,        /**< System integrity check failed - HALT */
    SAFETY_EVENT_COUNT
} safety_event_type_t;

/**
 * @brief Safety event data structure
 *
 * WHAT: Complete information about a safety event
 * WHY:  Provides context for neuromodulatory response calculation
 * HOW:  Includes event type, magnitude, timestamp, rule info, source
 */
typedef struct {
    safety_event_type_t type;             /**< Type of safety event */
    float magnitude;                      /**< Severity/importance [0-1] */
    uint64_t timestamp_us;                /**< When event occurred (microseconds) */
    char rule_name[SAFETY_MAX_RULE_NAME_LENGTH];  /**< Which rule triggered */
    char source_module[SAFETY_MAX_MODULE_NAME_LENGTH]; /**< Module that generated event */
    uint32_t violation_count;             /**< How many times this violation occurred */
    float confidence;                     /**< Confidence in event detection [0-1] */
    void* context;                        /**< Optional context data */
} safety_event_t;

/**
 * @brief Neuromodulatory response to safety event
 *
 * WHAT: Neuromodulator changes triggered by safety event
 * WHY:  Translates safety events into plasticity-modulating signals
 * HOW:  Delta values modify global neuromodulator levels
 */
typedef struct {
    /* Neuromodulator deltas (can be negative) */
    float dopamine_delta;                 /**< DA change: reward/punishment signal */
    float serotonin_delta;                /**< 5-HT change: mood/patience signal */
    float norepinephrine_delta;           /**< NE change: arousal/alertness signal */
    float acetylcholine_delta;            /**< ACh change: attention/encoding signal */

    /* Burst signaling */
    bool trigger_burst;                   /**< Trigger phasic burst (positive or negative) */
    bool burst_is_positive;               /**< True for positive burst, false for negative */
    float burst_magnitude;                /**< Magnitude of burst [0-1] */

    /* Plasticity modulation */
    float learning_rate_modifier;         /**< Multiply learning rate by this factor */
    bool trigger_ltd;                     /**< Force LTD at recently active synapses */
    bool trigger_ltp;                     /**< Force LTP at recently active synapses */

    /* Special flags */
    bool halt_system;                     /**< Integrity failure - halt all processing */
    bool log_event;                       /**< Should this event be logged */
    uint8_t severity_level;               /**< 0=info, 1=warning, 2=error, 3=critical */
} safety_neuromod_response_t;

/**
 * @brief Custom event handler callback type
 *
 * WHAT: User-defined handler for safety events
 * WHY:  Allows custom responses beyond default neuromodulator mapping
 * HOW:  Called after default response is computed, can modify response
 *
 * @param event The safety event that occurred
 * @param response The computed neuromodulatory response (can be modified)
 * @param user_data User-provided context
 * @return 0 to continue processing, non-zero to abort
 */
typedef int (*safety_event_callback_t)(
    const safety_event_t* event,
    safety_neuromod_response_t* response,
    void* user_data
);

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Default response magnitudes (multipliers for defaults) */
    float violation_response_gain;        /**< Scale violation responses [0.5-2.0] */
    float compliance_response_gain;       /**< Scale compliance responses [0.5-2.0] */
    float override_response_gain;         /**< Scale override responses [0.5-2.0] */
    float deception_response_gain;        /**< Scale deception responses [0.5-2.0] */

    /* Learning rate bounds */
    float min_learning_rate_modifier;     /**< Minimum LR modifier [0.1-1.0] */
    float max_learning_rate_modifier;     /**< Maximum LR modifier [1.0-5.0] */

    /* Timing */
    uint32_t event_cooldown_ms;           /**< Minimum time between same-type events */
    uint32_t burst_duration_ms;           /**< How long bursts last */

    /* Logging */
    bool enable_event_logging;            /**< Log all safety events */
    bool enable_response_logging;         /**< Log all neuromod responses */
    uint8_t log_level;                    /**< Minimum severity to log */

    /* Features */
    bool enable_burst_signaling;          /**< Enable phasic burst signals */
    bool enable_ltd_triggers;             /**< Enable forced LTD from violations */
    bool enable_ltp_triggers;             /**< Enable forced LTP from compliance */
    bool enable_escalation_tracking;      /**< Track repeated violations */
    bool auto_halt_on_integrity_fail;     /**< Automatically halt on integrity failure */
} safety_plasticity_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Event counts by type */
    uint64_t violations_blocked;
    uint64_t violations_escalated;
    uint64_t compliance_events;
    uint64_t overrides_accepted;
    uint64_t overrides_rejected;
    uint64_t deceptions_detected;
    uint64_t integrity_verified;
    uint64_t integrity_failed;

    /* Response counts */
    uint64_t ltd_triggers;                /**< LTD triggers issued */
    uint64_t ltp_triggers;                /**< LTP triggers issued */
    uint64_t positive_bursts;             /**< Positive bursts triggered */
    uint64_t negative_bursts;             /**< Negative bursts triggered */

    /* Aggregates */
    float total_da_delta;                 /**< Cumulative DA change */
    float total_ne_delta;                 /**< Cumulative NE change */
    float total_5ht_delta;                /**< Cumulative 5-HT change */
    float total_ach_delta;                /**< Cumulative ACh change */

    float avg_response_magnitude;         /**< Average response magnitude */
    float avg_learning_rate_mod;          /**< Average LR modifier applied */

    /* Timing */
    uint64_t last_event_us;               /**< Last event timestamp */
    uint64_t total_events;                /**< Total events processed */
    uint64_t total_updates;               /**< Total update cycles */
} safety_plasticity_stats_t;

/**
 * @brief Current bridge state
 */
typedef struct {
    /* Current levels */
    float pending_da_delta;               /**< DA delta waiting to be applied */
    float pending_ne_delta;               /**< NE delta waiting to be applied */
    float pending_5ht_delta;              /**< 5-HT delta waiting to be applied */
    float pending_ach_delta;              /**< ACh delta waiting to be applied */

    /* Burst state */
    bool burst_active;                    /**< Is a burst currently active */
    bool burst_is_positive;               /**< Is current burst positive */
    float burst_remaining_magnitude;      /**< Remaining burst magnitude */
    uint64_t burst_start_us;              /**< When burst started */

    /* Event tracking */
    safety_event_type_t last_event_type;  /**< Last event type processed */
    uint64_t last_event_us;               /**< Timestamp of last event */
    uint32_t consecutive_violations;      /**< Count of consecutive violations */

    /* System state */
    bool system_halted;                   /**< System is halted due to integrity failure */
    bool connected;                       /**< Bridge is fully connected */
    uint64_t last_update_us;              /**< Last update timestamp */
} safety_plasticity_state_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct safety_plasticity_bridge_struct safety_plasticity_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default bridge configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_default_config(safety_plasticity_config_t* config);

/**
 * @brief Create safety-plasticity bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
safety_plasticity_bridge_t* safety_plasticity_bridge_create(
    const safety_plasticity_config_t* config);

/**
 * @brief Destroy safety-plasticity bridge
 *
 * @param bridge Bridge to destroy
 */
void safety_plasticity_bridge_destroy(safety_plasticity_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/* Forward declarations */
struct plasticity_orchestrator_struct;
struct neuromodulator_system_struct;
struct symbolic_logic;

/**
 * @brief Connect to plasticity orchestrator
 *
 * @param bridge Bridge instance
 * @param orchestrator Plasticity orchestrator
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_connect_orchestrator(
    safety_plasticity_bridge_t* bridge,
    struct plasticity_orchestrator_struct* orchestrator);

/**
 * @brief Connect to neuromodulator system
 *
 * @param bridge Bridge instance
 * @param neuromod Neuromodulator system
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_connect_neuromod(
    safety_plasticity_bridge_t* bridge,
    struct neuromodulator_system_struct* neuromod);

/**
 * @brief Connect to symbolic logic system (for rule access)
 *
 * @param bridge Bridge instance
 * @param logic Symbolic logic system
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_connect_logic(
    safety_plasticity_bridge_t* bridge,
    struct symbolic_logic* logic);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_disconnect(safety_plasticity_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge instance
 * @return true if connected, false otherwise
 */
bool safety_plasticity_bridge_is_connected(const safety_plasticity_bridge_t* bridge);

/* ============================================================================
 * Core API - Event Processing
 * ============================================================================ */

/**
 * @brief Map safety event to neuromodulatory response
 *
 * WHAT: Computes neuromodulator changes for a safety event
 * WHY:  Translates safety signals into plasticity-modulating signals
 * HOW:  Uses default mappings, applies config gains, calls callbacks
 *
 * @param bridge Bridge instance
 * @param event Safety event to process
 * @param response Output neuromodulatory response
 * @return 0 on success, -1 on error
 */
int safety_plasticity_map_event(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event,
    safety_neuromod_response_t* response);

/**
 * @brief Apply neuromodulatory response to plasticity orchestrator
 *
 * WHAT: Applies computed response to the plasticity system
 * WHY:  Modifies neuromodulator levels and triggers plasticity events
 * HOW:  Updates neuromod system, triggers bursts/LTD/LTP as needed
 *
 * @param bridge Bridge instance
 * @param response Response to apply
 * @return 0 on success, -1 on error
 */
int safety_plasticity_apply_response(
    safety_plasticity_bridge_t* bridge,
    const safety_neuromod_response_t* response);

/**
 * @brief Process safety event (map and apply in one call)
 *
 * WHAT: Convenience function to process event end-to-end
 * WHY:  Simplifies common use case
 * HOW:  Calls map_event then apply_response
 *
 * @param bridge Bridge instance
 * @param event Safety event to process
 * @return 0 on success, -1 on error, 1 if system halted
 */
int safety_plasticity_process_event(
    safety_plasticity_bridge_t* bridge,
    const safety_event_t* event);

/* ============================================================================
 * Callback API
 * ============================================================================ */

/**
 * @brief Register custom event handler callback
 *
 * @param bridge Bridge instance
 * @param event_type Event type to handle (or SAFETY_EVENT_COUNT for all)
 * @param callback Callback function
 * @param user_data User context passed to callback
 * @return Callback ID (>=0), or -1 on error
 */
int safety_plasticity_register_callback(
    safety_plasticity_bridge_t* bridge,
    safety_event_type_t event_type,
    safety_event_callback_t callback,
    void* user_data);

/**
 * @brief Unregister callback
 *
 * @param bridge Bridge instance
 * @param callback_id ID returned from register_callback
 * @return 0 on success, -1 on error
 */
int safety_plasticity_unregister_callback(
    safety_plasticity_bridge_t* bridge,
    int callback_id);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge state (process pending deltas, decay bursts)
 *
 * @param bridge Bridge instance
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_update(
    safety_plasticity_bridge_t* bridge,
    float delta_ms);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge instance
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int safety_plasticity_get_state(
    const safety_plasticity_bridge_t* bridge,
    safety_plasticity_state_t* state);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int safety_plasticity_get_stats(
    const safety_plasticity_bridge_t* bridge,
    safety_plasticity_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int safety_plasticity_reset_stats(safety_plasticity_bridge_t* bridge);

/* ============================================================================
 * Diagnostic API
 * ============================================================================ */

/**
 * @brief Check if system is halted due to integrity failure
 *
 * @param bridge Bridge instance
 * @return true if halted, false otherwise
 */
bool safety_plasticity_is_halted(const safety_plasticity_bridge_t* bridge);

/**
 * @brief Clear halt state (requires external authorization)
 *
 * @param bridge Bridge instance
 * @param authorization_code Authorization code
 * @return 0 on success, -1 on error, -2 on invalid authorization
 */
int safety_plasticity_clear_halt(
    safety_plasticity_bridge_t* bridge,
    uint64_t authorization_code);

/**
 * @brief Get name of safety event type
 *
 * @param type Event type
 * @return String name of event type
 */
const char* safety_event_type_name(safety_event_type_t type);

/**
 * @brief Print bridge summary to log
 *
 * @param bridge Bridge instance
 */
void safety_plasticity_print_summary(const safety_plasticity_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_connect_bio_async(safety_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int safety_plasticity_bridge_disconnect_bio_async(safety_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected, false otherwise
 */
bool safety_plasticity_bridge_is_bio_async_connected(
    const safety_plasticity_bridge_t* bridge);

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

/**
 * @brief Create and populate a safety event
 *
 * @param type Event type
 * @param magnitude Severity [0-1]
 * @param rule_name Name of rule that triggered (can be NULL)
 * @param source_module Source module name (can be NULL)
 * @return Initialized event structure
 */
safety_event_t safety_event_create(
    safety_event_type_t type,
    float magnitude,
    const char* rule_name,
    const char* source_module);

/**
 * @brief Initialize response to defaults (zeros)
 *
 * @param response Response to initialize
 */
void safety_neuromod_response_init(safety_neuromod_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYMBOLIC_LOGIC_PLASTICITY_BRIDGE_H */

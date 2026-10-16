/**
 * @file nimcp_medulla.h
 * @brief Medulla Oblongata Main Orchestrator
 *
 * WHAT: Main coordination module for the medulla oblongata subsystems
 * WHY:  The medulla orchestrates vital functions (arousal, protection, circadian rhythm)
 *       and integrates with higher-level systems for homeostatic regulation
 * HOW:  Coordinates arousal state, protective cutoff, brainstem coupling, and circadian
 *       rhythm while integrating with health monitoring, recovery, sleep-wake, and
 *       neuromodulatory systems
 *
 * BIOLOGICAL BASIS:
 * The medulla oblongata is the most caudal part of the brainstem, controlling:
 * - Arousal and consciousness level (reticular activating system)
 * - Protective reflexes (cardiovascular, respiratory regulation analogs)
 * - Circadian coordination (synchronizing with SCN signals)
 * - Brainstem-cortex coupling (ascending/descending pathways)
 *
 * COORDINATION LOGIC:
 * 1. Health alerts → protective cutoff activation
 * 2. Protection level → arousal modulation (suppress during critical states)
 * 3. Circadian phase → arousal targets, neuromodulator baselines
 * 4. Sleep-wake state ↔ arousal state bidirectional synchronization
 * 5. Recovery system → protection level adjustments
 *
 * DESIGN PATTERNS:
 * - Orchestrator: Coordinates multiple subsystems
 * - Mediator: Central coordination point between subsystems
 * - Observer: Responds to health monitor events
 * - Strategy: Different coordination strategies per state
 *
 * PHASE: 1.1 - Main Orchestrator
 *
 * @author NIMCP Development Team
 * @date 2025-12-17
 * @version 1.0.0
 */

#ifndef NIMCP_MEDULLA_H
#define NIMCP_MEDULLA_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declaration - Health Agent
//=============================================================================
struct nimcp_health_agent;

//=============================================================================
// Forward Declarations (Subsystems - Phase 1.2-1.5)
//=============================================================================

/**
 * @brief Arousal state management (Phase 1.2)
 *
 * BIOLOGICAL: Reticular activating system - modulates consciousness level
 */
// arousal_state_t defined in nimcp_arousal_state.h
struct arousal_state_struct;

/**
 * @brief Protective cutoff system (Phase 1.3)
 *
 * BIOLOGICAL: Emergency shutdown mechanisms for system protection
 */
#ifndef NIMCP_PROTECTIVE_CUTOFF_T_DEFINED
#define NIMCP_PROTECTIVE_CUTOFF_T_DEFINED
typedef struct protective_cutoff_struct* protective_cutoff_t;
#endif

/**
 * @brief Brainstem-cortex coupling (Phase 1.4)
 *
 * BIOLOGICAL: Ascending/descending pathways between brainstem and cortex
 */
typedef struct brainstem_coupling_struct* brainstem_coupling_t;

/**
 * @brief Circadian rhythm coordination (Phase 1.5)
 *
 * BIOLOGICAL: Synchronization with suprachiasmatic nucleus (SCN) signals
 */
#ifndef NIMCP_CIRCADIAN_RHYTHM_T_DEFINED
#define NIMCP_CIRCADIAN_RHYTHM_T_DEFINED
typedef struct circadian_rhythm_struct* circadian_rhythm_t;
#endif

//=============================================================================
// External System Forward Declarations
//=============================================================================

// Health monitoring (existing)
typedef struct health_monitor_internal* health_monitor_t;

// Recovery system (existing)
typedef struct nimcp_recovery_struct* nimcp_recovery_t;

// Sleep-wake cycle (existing)
typedef struct sleep_system_struct* sleep_system_t;
#define NIMCP_SLEEP_SYSTEM_T_DEFINED

// Neuromodulator system (existing)
typedef struct neuromodulator_system_struct* neuromodulator_system_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Arousal level categories
 *
 * BIOLOGICAL: Maps to different states of consciousness
 */
typedef enum {
    AROUSAL_LEVEL_COMA = 0,         /**< Non-responsive, minimal activity */
    AROUSAL_LEVEL_DEEP_SLEEP = 1,   /**< Deep sleep, low arousal */
    AROUSAL_LEVEL_LIGHT_SLEEP = 2,  /**< Light sleep, moderate arousal */
    AROUSAL_LEVEL_DROWSY = 3,       /**< Drowsy, reduced alertness */
    AROUSAL_LEVEL_AWAKE = 4,        /**< Normal wakefulness */
    AROUSAL_LEVEL_ALERT = 5,        /**< High alertness */
    AROUSAL_LEVEL_HYPERAROUSAL = 6  /**< Extreme arousal (stress/panic) */
} arousal_level_t;

/**
 * @brief Protection level categories
 *
 * BIOLOGICAL: Emergency response escalation levels
 */
#ifndef NIMCP_PROTECTION_LEVEL_DEFINED
#define NIMCP_PROTECTION_LEVEL_DEFINED
typedef enum {
    PROTECTION_LEVEL_NORMAL = 0,    /**< Normal operation */
    PROTECTION_LEVEL_CAUTIOUS = 1,  /**< Minor issues detected */
    PROTECTION_LEVEL_GUARDED = 2,   /**< Moderate threat */
    PROTECTION_LEVEL_DEFENSIVE = 3, /**< Significant threat */
    PROTECTION_LEVEL_CRITICAL = 4,  /**< Critical state */
    PROTECTION_LEVEL_SHUTDOWN = 5   /**< Emergency shutdown */
} protection_level_t;
#endif

/**
 * @brief Circadian phase categories
 *
 * BIOLOGICAL: Maps to circadian cycle (24-hour rhythm)
 */
#ifndef NIMCP_CIRCADIAN_PHASE_DEFINED
#define NIMCP_CIRCADIAN_PHASE_DEFINED
typedef enum {
    CIRCADIAN_PHASE_EARLY_MORNING = 0,  /**< 06:00-09:00 - Rising arousal */
    CIRCADIAN_PHASE_MORNING = 1,        /**< 09:00-12:00 - Peak alertness */
    CIRCADIAN_PHASE_AFTERNOON = 2,      /**< 12:00-15:00 - Post-lunch dip */
    CIRCADIAN_PHASE_EVENING = 3,        /**< 15:00-18:00 - Second peak */
    CIRCADIAN_PHASE_LATE_EVENING = 4,   /**< 18:00-21:00 - Declining arousal */
    CIRCADIAN_PHASE_NIGHT = 5,          /**< 21:00-24:00 - Sleep preparation */
    CIRCADIAN_PHASE_DEEP_NIGHT = 6,     /**< 00:00-03:00 - Minimal arousal */
    CIRCADIAN_PHASE_PRE_DAWN = 7        /**< 03:00-06:00 - Sleep end */
} circadian_phase_t;
#endif

/**
 * @brief Medulla operational state
 */
typedef enum {
    MEDULLA_STATE_STOPPED = 0,      /**< Not running */
    MEDULLA_STATE_STARTING = 1,     /**< Initialization phase */
    MEDULLA_STATE_RUNNING = 2,      /**< Normal operation */
    MEDULLA_STATE_DEGRADED = 3,     /**< Degraded mode (subsystem failure) */
    MEDULLA_STATE_EMERGENCY = 4,    /**< Emergency state */
    MEDULLA_STATE_STOPPING = 5      /**< Shutdown in progress */
} medulla_state_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Arousal state configuration (Phase 1.2)
 */
typedef struct {
    float baseline_arousal;         /**< Baseline arousal level [0-1] */
    float arousal_decay_rate;       /**< Decay rate per second */
    float min_arousal;              /**< Minimum arousal threshold */
    float max_arousal;              /**< Maximum arousal threshold */
    bool enable_auto_regulation;    /**< Auto-regulate arousal */
} arousal_config_t;

/**
 * @brief Protective cutoff configuration (Phase 1.3)
 */
typedef struct {
    float health_threshold_critical;    /**< Health score for critical state */
    float health_threshold_defensive;   /**< Health score for defensive state */
    float recovery_time_ms;             /**< Recovery time before reset */
    bool enable_auto_shutdown;          /**< Auto-shutdown on critical */
} protection_config_t;

/**
 * @brief Brainstem coupling configuration (Phase 1.4)
 */
typedef struct {
    float coupling_strength;        /**< Coupling strength [0-1] */
    float latency_ms;              /**< Signal propagation delay */
    bool enable_bidirectional;     /**< Enable bidirectional coupling */
} coupling_config_t;

/**
 * @brief Circadian rhythm configuration (Phase 1.5)
 */
#ifndef NIMCP_CIRCADIAN_CONFIG_T_DEFINED
#define NIMCP_CIRCADIAN_CONFIG_T_DEFINED
typedef struct {
    float period_hours;            /**< Circadian period (default: 24.0) */
    float phase_offset_hours;      /**< Phase offset from midnight */
    float amplitude;               /**< Amplitude of rhythm [0-1] */
    bool enable_synchronization;   /**< Sync with external zeitgebers */
} circadian_config_t;
#endif

/**
 * @brief Medulla orchestrator configuration
 */
typedef struct {
    // Subsystem configurations
    arousal_config_t arousal;
    protection_config_t protection;
    coupling_config_t coupling;
    circadian_config_t circadian;

    // Integration settings
    bool enable_health_integration;     /**< Integrate with health monitor */
    bool enable_recovery_integration;   /**< Integrate with recovery system */
    bool enable_sleep_integration;      /**< Integrate with sleep-wake */
    bool enable_neuromod_integration;   /**< Integrate with neuromodulators */

    // Update timing
    uint32_t update_interval_ms;    /**< Medulla update interval (default: 100ms) */

    // Bio-async settings
    bool enable_bio_async;          /**< Enable bio-async messaging */
    uint32_t inbox_capacity;        /**< Bio-async inbox size */
} medulla_config_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Medulla statistics and monitoring
 */
typedef struct {
    // Current state
    medulla_state_t state;
    arousal_level_t arousal_level;
    protection_level_t protection_level;
    circadian_phase_t circadian_phase;

    // Arousal metrics
    float current_arousal;          /**< Current arousal level [0-1] */
    float avg_arousal;              /**< Average arousal (running) */
    uint64_t arousal_updates;       /**< Number of arousal updates */

    // Protection metrics
    uint32_t protection_activations; /**< Times protection activated */
    uint32_t emergency_shutdowns;    /**< Emergency shutdown count */
    uint64_t time_in_protection_ms;  /**< Time spent in protective state */

    // Circadian metrics
    float circadian_time_hours;     /**< Current circadian time (0-24) */
    uint32_t circadian_cycles;      /**< Completed circadian cycles */

    // Integration metrics
    uint32_t health_alerts_received;    /**< Health monitor alerts */
    uint32_t recovery_triggers;         /**< Recovery activations */
    uint32_t sleep_transitions;         /**< Sleep state changes */

    // Performance
    uint64_t total_updates;         /**< Total update cycles */
    uint64_t uptime_ms;             /**< Total runtime */
    float avg_update_time_us;       /**< Average update time */
} medulla_stats_t;

//=============================================================================
// Main Medulla Structure
//=============================================================================

/**
 * @brief Opaque medulla orchestrator handle
 */
typedef struct medulla_struct* medulla_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create medulla orchestrator
 * WHY:  Initialize medulla subsystems and coordination
 * HOW:  Allocate structure, create subsystems, initialize state
 *
 * @param config Medulla configuration (NULL for defaults)
 * @return Medulla handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
medulla_t medulla_create(const medulla_config_t* config);

/**
 * WHAT: Destroy medulla orchestrator
 * WHY:  Clean shutdown and resource cleanup
 * HOW:  Stop subsystems, free resources, destroy structures
 *
 * @param medulla Medulla handle to destroy
 *
 * SAFETY: Safe to call with NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void medulla_destroy(medulla_t medulla);

/**
 * WHAT: Get default medulla configuration
 * WHY:  Sensible defaults for typical operation
 * HOW:  Return pre-configured struct with biological defaults
 *
 * DEFAULTS:
 * - Arousal: baseline 0.5, auto-regulation enabled
 * - Protection: health threshold 30.0, auto-shutdown enabled
 * - Circadian: 24-hour period, 0.5 amplitude
 * - Update: 100ms interval
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
medulla_config_t medulla_default_config(void);

//=============================================================================
// Control Functions
//=============================================================================

/**
 * WHAT: Start medulla orchestrator
 * WHY:  Begin coordination of vital functions
 * HOW:  Initialize subsystems, start update loop
 *
 * @param medulla Medulla handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_start(medulla_t medulla);

/**
 * WHAT: Stop medulla orchestrator
 * WHY:  Graceful shutdown of medulla systems
 * HOW:  Stop subsystems, save state, transition to stopped
 *
 * @param medulla Medulla handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_stop(medulla_t medulla);

/**
 * WHAT: Main medulla update tick
 * WHY:  Coordinate all subsystems in synchronized manner
 * HOW:  Update arousal, protection, circadian, coupling in sequence
 *
 * COORDINATION PIPELINE:
 * 1. Update circadian rhythm (time progression)
 * 2. Check health monitor for alerts
 * 3. Update protection level based on health
 * 4. Update arousal state (modulated by protection/circadian)
 * 5. Update brainstem coupling (propagate states)
 * 6. Apply neuromodulator adjustments
 * 7. Synchronize with sleep-wake system
 *
 * @param medulla Medulla handle
 * @param dt Time step in seconds
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_update(medulla_t medulla, float dt);

/**
 * WHAT: Emergency shutdown
 * WHY:  Immediate protective shutdown in critical situations
 * HOW:  Activate maximum protection, reduce arousal to minimum
 *
 * BIOLOGICAL: Like protective reflexes (fainting, seizure suppression)
 *
 * @param medulla Medulla handle
 * @param reason Reason for emergency shutdown
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_emergency_shutdown(medulla_t medulla, const char* reason);

/**
 * WHAT: Request state change from cortex
 * WHY:  Allow higher-level control of medulla state
 * HOW:  Validate and apply state transition
 *
 * BIOLOGICAL: Cortical override of brainstem functions
 *
 * @param medulla Medulla handle
 * @param new_state Requested new state
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_request_state_change(medulla_t medulla, medulla_state_t new_state);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * WHAT: Connect health monitor
 * WHY:  Enable health-driven protection responses
 * HOW:  Store health monitor reference, register callbacks
 *
 * @param medulla Medulla handle
 * @param health_monitor Health monitor handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_connect_health_monitor(medulla_t medulla, health_monitor_t health_monitor);

/**
 * WHAT: Connect recovery system
 * WHY:  Enable coordinated recovery responses
 * HOW:  Store recovery system reference
 *
 * @param medulla Medulla handle
 * @param recovery Recovery system handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_connect_recovery_system(medulla_t medulla, nimcp_recovery_t recovery);

/**
 * WHAT: Connect sleep-wake system
 * WHY:  Bidirectional synchronization of arousal and sleep state
 * HOW:  Store sleep-wake reference, enable state synchronization
 *
 * @param medulla Medulla handle
 * @param sleep_wake Sleep-wake system handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_connect_sleep_wake(medulla_t medulla, sleep_system_t sleep_wake);

/**
 * WHAT: Connect neuromodulator system
 * WHY:  Enable neuromodulator-driven arousal/circadian modulation
 * HOW:  Store neuromodulator reference
 *
 * @param medulla Medulla handle
 * @param neuromodulators Neuromodulator system handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_connect_neuromodulators(medulla_t medulla, neuromodulator_system_t neuromodulators);

//=============================================================================
// Health Agent Integration
//=============================================================================

/**
 * WHAT: Set health agent for medulla heartbeats
 * WHY:  Enable health monitoring integration
 * HOW:  Store health agent reference for heartbeat calls
 *
 * @param agent Health agent pointer (can be NULL to disable)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void medulla_set_health_agent(struct nimcp_health_agent* agent);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get current arousal level
 * WHY:  Query arousal state for decision-making
 * HOW:  Read current arousal level from arousal subsystem
 *
 * @param medulla Medulla handle
 * @return Current arousal level (0-1), or -1.0f on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float medulla_get_arousal_level(const medulla_t medulla);

/**
 * WHAT: Boost arousal level
 * WHY:  Increase arousal in response to stimulating events (pursuit, threat)
 * HOW:  Add delta to current arousal, clamped to [min, max] range
 *
 * @param medulla Medulla handle
 * @param delta Arousal increase amount (typically 0.05-0.2)
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_boost_arousal(medulla_t medulla, float delta);

/**
 * WHAT: Reduce arousal level
 * WHY:  Decrease arousal in response to calming events (rest, failure recovery)
 * HOW:  Subtract delta from current arousal, clamped to [min, max] range
 *
 * @param medulla Medulla handle
 * @param delta Arousal decrease amount (typically 0.05-0.2)
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_reduce_arousal(medulla_t medulla, float delta);

/**
 * WHAT: Get current protection level
 * WHY:  Query protection state
 * HOW:  Read current protection level from protection subsystem
 *
 * @param medulla Medulla handle
 * @return Current protection level enum
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
protection_level_t medulla_get_protection_level(const medulla_t medulla);

/**
 * WHAT: Get current circadian phase
 * WHY:  Query circadian rhythm state
 * HOW:  Read current phase from circadian subsystem
 *
 * @param medulla Medulla handle
 * @return Current circadian phase enum
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
circadian_phase_t medulla_get_circadian_phase(const medulla_t medulla);

/**
 * WHAT: Get medulla statistics
 * WHY:  Monitor medulla performance and state
 * HOW:  Copy statistics structure
 *
 * @param medulla Medulla handle
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_get_stats(const medulla_t medulla, medulla_stats_t* stats);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async router
 * WHY:  Enable inter-module messaging
 * HOW:  Register with bio-async router, initialize inbox
 *
 * @param medulla Medulla handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_connect_bio_async(medulla_t medulla);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from router, flush messages
 *
 * @param medulla Medulla handle
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int medulla_disconnect_bio_async(medulla_t medulla);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection state
 * HOW:  Check bio_async_enabled flag
 *
 * @param medulla Medulla handle
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool medulla_is_bio_async_connected(const medulla_t medulla);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Convert arousal level to string
 * WHY:  Human-readable arousal state
 * HOW:  Enum to string mapping
 *
 * @param level Arousal level
 * @return String representation
 */
const char* medulla_arousal_level_to_string(arousal_level_t level);

/**
 * WHAT: Convert protection level to string
 * WHY:  Human-readable protection state
 * HOW:  Enum to string mapping
 *
 * @param level Protection level
 * @return String representation
 */
const char* medulla_protection_level_to_string(protection_level_t level);

/**
 * WHAT: Convert circadian phase to string
 * WHY:  Human-readable circadian state
 * HOW:  Enum to string mapping
 *
 * @param phase Circadian phase
 * @return String representation
 */
const char* medulla_circadian_phase_to_string(circadian_phase_t phase);

/**
 * WHAT: Convert medulla state to string
 * WHY:  Human-readable medulla state
 * HOW:  Enum to string mapping
 *
 * @param state Medulla state
 * @return String representation
 */
const char* medulla_state_to_string(medulla_state_t state);

//=============================================================================
// BBB Integration (Blood-Brain Barrier)
//=============================================================================

/* Forward declaration for BBB system type */
#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

/**
 * WHAT: Set BBB system for medulla input validation
 * WHY:  Enable security validation of all external inputs
 * HOW:  Store BBB reference, use in validation functions
 *
 * @param bbb BBB system handle (NULL to disable validation)
 *
 * NOTE: When bbb is NULL, all validation passes (permissive mode)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void medulla_set_bbb(bbb_system_t bbb);

//=============================================================================
// Test Helper Functions (for integration testing only)
//=============================================================================

/**
 * WHAT: Force arousal level for testing
 * WHY:  Allow integration tests to verify arousal-dependent behavior
 * HOW:  Directly set internal arousal state
 * WARNING: This bypasses normal arousal state transitions
 *
 * @param medulla Medulla handle
 * @param level Target arousal level [0.0-1.0]
 * @return 0 on success, negative error code on failure
 */
int medulla_test_set_arousal(medulla_t medulla, float level);

/**
 * WHAT: Force protection level for testing
 * WHY:  Allow integration tests to verify protection-dependent behavior
 * HOW:  Directly set internal protection state
 * WARNING: This bypasses normal protection escalation
 *
 * @param medulla Medulla handle
 * @param level Target protection level
 * @return 0 on success, negative error code on failure
 */
int medulla_test_set_protection(medulla_t medulla, protection_level_t level);

/**
 * WHAT: Force circadian phase for testing
 * WHY:  Allow integration tests to verify circadian-dependent behavior
 * HOW:  Directly set internal circadian phase
 * WARNING: This bypasses normal circadian rhythm progression
 *
 * @param medulla Medulla handle
 * @param phase Target circadian phase
 * @return 0 on success, negative error code on failure
 */
int medulla_test_set_circadian(medulla_t medulla, circadian_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MEDULLA_H

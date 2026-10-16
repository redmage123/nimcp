/**
 * @file nimcp_protective_cutoff.h
 * @brief Multi-Tier Emergency Protective Shutdown System
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Emergency protective cutoff system for graceful degradation under threat
 * WHY:  Models brainstem protective reflexes (vasovagal response, pain withdrawal);
 *       prevents catastrophic failure by progressively restricting operations as
 *       threat severity increases. Essential for system stability and safety.
 * HOW:  Multi-tier protection levels (NORMAL → WARN → THROTTLE → SHED_LOAD →
 *       SAFE_MODE → EMERGENCY_SHUTDOWN) triggered by threat assessments across
 *       multiple dimensions (temperature, memory, CPU, error rate). Each level
 *       progressively restricts capabilities with hysteresis for recovery.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * BRAINSTEM PROTECTIVE REFLEXES:
 * -----------------------------
 * The medulla oblongata orchestrates multi-tier protective responses to threats:
 *
 * 1. Vasovagal Response:
 *    - Sudden drop in blood pressure during extreme stress
 *    - Protective shutdown to prevent further harm
 *    - Gradual recovery with hysteresis (slower to normalize than to trigger)
 *    - Reference: Mosqueda-Garcia et al. (2000) "The elusive pathophysiology of
 *      neurally mediated syncope"
 *
 * 2. Pain Withdrawal Reflex:
 *    - Rapid protective response to noxious stimuli
 *    - Multi-tier escalation: detect → withdraw → immobilize → shutdown
 *    - Override of voluntary control during extreme danger
 *    - Reference: Woolf & Ma (2007) "Nociceptors—noxious stimulus detectors"
 *
 * 3. Respiratory Protective Reflexes:
 *    - Cough, sneeze, laryngospasm to protect airways
 *    - Graded response based on threat severity
 *    - Automatic override of conscious breathing control
 *    - Reference: Widdicombe (1998) "Afferent receptors in the airways and cough"
 *
 * 4. Cardiovascular Protective Responses:
 *    - Baroreceptor reflexes prevent extreme blood pressure
 *    - Chemoreceptor reflexes protect against hypoxia/hypercapnia
 *    - Progressive restriction of cardiac output under extreme stress
 *    - Reference: Chapleau & Abboud (2001) "Mechanisms of adaptation and resetting
 *      of the baroreceptor reflex"
 *
 * 5. Thermoregulatory Shutdown:
 *    - Heat stress → reduced metabolic activity
 *    - Protective hypothermia during extreme danger
 *    - Graded response: sweating → reduced activity → shutdown
 *    - Reference: Bouchama & Knochel (2002) "Heat stroke"
 *
 * PROTECTION TIER ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              MULTI-TIER PROTECTIVE CUTOFF SYSTEM                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   THREAT ASSESSMENTS:                  PROTECTION LEVELS:                  ║
 * ║   ┌──────────────────┐                ┌────────────────────────────┐      ║
 * ║   │ Temperature      │                │ NORMAL (0)                 │      ║
 * ║   │ Memory Pressure  │                │ All operations allowed     │      ║
 * ║   │ CPU Overload     │                └────────────────────────────┘      ║
 * ║   │ Error Rate       │ ──────────┐                                        ║
 * ║   │ Network Latency  │           │    ┌────────────────────────────┐      ║
 * ║   │ Immune Storm     │           ├───>│ WARN (1)                   │      ║
 * ║   │ Resource Leak    │           │    │ Monitoring increased       │      ║
 * ║   └──────────────────┘           │    └────────────────────────────┘      ║
 * ║                                  │                                        ║
 * ║   THRESHOLDS:                    │    ┌────────────────────────────┐      ║
 * ║   ┌──────────────────┐           ├───>│ THROTTLE (2)               │      ║
 * ║   │ Warn    : 0.30   │           │    │ Non-essential ops reduced  │      ║
 * ║   │ Throttle: 0.50   │           │    └────────────────────────────┘      ║
 * ║   │ Shed    : 0.70   │           │                                        ║
 * ║   │ Safe    : 0.85   │           │    ┌────────────────────────────┐      ║
 * ║   │ Shutdown: 0.95   │           ├───>│ SHED_LOAD (3)              │      ║
 * ║   └──────────────────┘           │    │ Only critical ops allowed  │      ║
 * ║                                  │    └────────────────────────────┘      ║
 * ║   HYSTERESIS (Recovery):         │                                        ║
 * ║   - Harder to recover than       │    ┌────────────────────────────┐      ║
 * ║     to degrade                   ├───>│ SAFE_MODE (4)              │      ║
 * ║   - Recovery threshold:          │    │ Minimal vital ops only     │      ║
 * ║     threshold - 0.15             │    └────────────────────────────┘      ║
 * ║                                  │                                        ║
 * ║                                  │    ┌────────────────────────────┐      ║
 * ║                                  └───>│ EMERGENCY_SHUTDOWN (5)     │      ║
 * ║                                       │ Complete system halt       │      ║
 * ║                                       └────────────────────────────┘      ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * CAPABILITY RESTRICTIONS BY LEVEL:
 * ================================
 * NORMAL (0):
 *   - All operations permitted
 *   - Full learning, inference, adaptation
 *   - All network communication
 *   - All resource allocation
 *
 * WARN (1):
 *   - All operations still permitted
 *   - Enhanced monitoring enabled
 *   - Logging increased
 *   - Alerts generated
 *
 * THROTTLE (2):
 *   - Non-essential learning throttled (50% reduction)
 *   - Batch sizes reduced
 *   - Cache flushing increased
 *   - Background tasks postponed
 *
 * SHED_LOAD (3):
 *   - Learning disabled except critical pathways
 *   - Inference only for high-priority requests
 *   - Non-critical connections dropped
 *   - Memory aggressively freed
 *
 * SAFE_MODE (4):
 *   - No learning permitted
 *   - Inference only for emergency requests
 *   - Minimal network activity
 *   - Preserve only critical state
 *
 * EMERGENCY_SHUTDOWN (5):
 *   - All operations halted
 *   - State checkpointed
 *   - Connections closed
 *   - Resources released
 *   - System awaiting manual intervention
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PROTECTIVE_CUTOFF_H
#define NIMCP_PROTECTIVE_CUTOFF_H

//=============================================================================
// Required Headers
//=============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

#ifndef NIMCP_PROTECTIVE_CUTOFF_T_DEFINED
#define NIMCP_PROTECTIVE_CUTOFF_T_DEFINED
typedef struct protective_cutoff_s protective_cutoff_t;
#endif

//=============================================================================
// Enumerations
//=============================================================================

/**
 * WHAT: Protection levels from normal operation to emergency shutdown
 * WHY:  Progressive degradation with increasing threat severity
 * HOW:  Each level restricts more capabilities, modeling brainstem reflexes
 *
 * BIOLOGICAL BASIS: Models graded protective responses in medulla oblongata
 */
#ifndef NIMCP_PROTECTION_LEVEL_DEFINED
#define NIMCP_PROTECTION_LEVEL_DEFINED
typedef enum {
    PROTECTION_NORMAL = 0,           /**< Normal operation, all capabilities */
    PROTECTION_WARN = 1,             /**< Warning state, enhanced monitoring */
    PROTECTION_THROTTLE = 2,         /**< Throttled, non-essential ops reduced */
    PROTECTION_SHED_LOAD = 3,        /**< Load shedding, critical ops only */
    PROTECTION_SAFE_MODE = 4,        /**< Safe mode, minimal vital ops */
    PROTECTION_EMERGENCY_SHUTDOWN = 5 /**< Emergency shutdown, all ops halted */
} protection_level_t;
#endif

/**
 * WHAT: Types of threats that can trigger protection escalation
 * WHY:  Multi-dimensional threat assessment for comprehensive protection
 * HOW:  Each threat type contributes to overall protection level decision
 */
#ifndef NIMCP_THREAT_TYPE_DEFINED
#define NIMCP_THREAT_TYPE_DEFINED
typedef enum {
    PROTECTIVE_THREAT_TEMPERATURE = 0,  /**< Thermal overload (CPU/GPU temperature) */
    PROTECTIVE_THREAT_MEMORY_PRESSURE,  /**< Memory exhaustion or pressure */
    PROTECTIVE_THREAT_CPU_OVERLOAD,     /**< CPU utilization approaching limits */
    PROTECTIVE_THREAT_ERROR_RATE,       /**< Error rate spike (failures, exceptions) */
    PROTECTIVE_THREAT_NETWORK_LATENCY,  /**< Network latency or timeout spike */
    PROTECTIVE_THREAT_IMMUNE_STORM,     /**< Cytokine storm from immune system */
    PROTECTIVE_THREAT_RESOURCE_LEAK,    /**< Resource leak detected */
    PROTECTIVE_THREAT_EXTERNAL_SIGNAL,  /**< External shutdown signal (SIGTERM, etc) */
    PROTECTIVE_THREAT_COUNT             /**< Number of threat types */
} protective_threat_type_t;
#endif

/**
 * WHAT: Operation categories that can be restricted
 * WHY:  Fine-grained control over what operations are permitted at each level
 * HOW:  Each protection level permits/denies specific operation types
 */
typedef enum {
    OP_LEARNING = 0,                 /**< Learning/training operations */
    OP_INFERENCE,                    /**< Inference/forward pass operations */
    OP_ADAPTATION,                   /**< Plasticity/adaptation operations */
    OP_NETWORK_TX,                   /**< Network transmission */
    OP_NETWORK_RX,                   /**< Network reception */
    OP_MEMORY_ALLOC,                 /**< Memory allocation */
    OP_FILE_IO,                      /**< File I/O operations */
    OP_CHECKPOINT,                   /**< Checkpointing operations */
    OP_COUNT                         /**< Number of operation types */
} operation_type_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * WHAT: Thresholds for each protection level transition
 * WHY:  Configurable trigger points for different deployment scenarios
 * HOW:  Threat severity (0.0-1.0) compared against these thresholds
 *
 * BIOLOGICAL BASIS: Models variable threshold for protective reflexes
 * (e.g., pain tolerance, baroreceptor sensitivity vary by individual)
 */
typedef struct {
    float warn_threshold;            /**< Transition to WARN level (default 0.30) */
    float throttle_threshold;        /**< Transition to THROTTLE (default 0.50) */
    float shed_load_threshold;       /**< Transition to SHED_LOAD (default 0.70) */
    float safe_mode_threshold;       /**< Transition to SAFE_MODE (default 0.85) */
    float emergency_threshold;       /**< Transition to EMERGENCY (default 0.95) */
    float recovery_hysteresis;       /**< Recovery offset (default 0.15) */
} protection_thresholds_t;

/**
 * WHAT: Weights for different threat types in overall assessment
 * WHY:  Some threats are more critical than others
 * HOW:  Weighted combination of all threat severities
 */
typedef struct {
    float temperature_weight;        /**< Weight for temperature threats */
    float memory_weight;             /**< Weight for memory pressure */
    float cpu_weight;                /**< Weight for CPU overload */
    float error_rate_weight;         /**< Weight for error rate */
    float network_weight;            /**< Weight for network issues */
    float immune_weight;             /**< Weight for immune storm */
    float leak_weight;               /**< Weight for resource leaks */
    float external_weight;           /**< Weight for external signals */
} threat_weights_t;

/**
 * WHAT: Configuration for protective cutoff system
 * WHY:  Customizable behavior for different deployment scenarios
 * HOW:  Passed to protective_cutoff_create()
 */
typedef struct {
    protection_thresholds_t thresholds; /**< Level transition thresholds */
    threat_weights_t weights;           /**< Threat type weights */
    uint32_t assessment_interval_ms;    /**< How often to reassess (default 1000ms) */
    bool enable_auto_recovery;          /**< Auto-recover when threats subside */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} protective_cutoff_config_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * WHAT: Callback invoked when protection level changes
 * WHY:  Allow modules to react to protection state transitions
 * HOW:  Registered via protective_cutoff_register_callback()
 *
 * @param old_level Previous protection level
 * @param new_level New protection level
 * @param user_data User-provided context
 */
typedef void (*protection_level_callback_t)(
    protection_level_t old_level,
    protection_level_t new_level,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Populate configuration with safe defaults
 * WHY:  Simplify initialization, prevent uninitialized values
 * HOW:  Sets reasonable thresholds and weights
 *
 * @param config Configuration to populate (must not be NULL)
 */
void protective_cutoff_default_config(protective_cutoff_config_t* config);

/**
 * WHAT: Create protective cutoff system
 * WHY:  Initialize multi-tier protection infrastructure
 * HOW:  Allocates context, initializes threat tracking, sets initial state
 *
 * @param config Configuration (NULL uses defaults)
 * @return Protective cutoff context or NULL on failure
 */
protective_cutoff_t* protective_cutoff_create(const protective_cutoff_config_t* config);

/**
 * WHAT: Destroy protective cutoff system
 * WHY:  Clean up resources, release memory
 * HOW:  Frees all internal structures, mutex, bio-async connection
 *
 * @param cutoff Context to destroy (NULL-safe)
 */
void protective_cutoff_destroy(protective_cutoff_t* cutoff);

//=============================================================================
// Threat Reporting
//=============================================================================

/**
 * WHAT: Report threat assessment for a specific type
 * WHY:  Update system's view of current threat landscape
 * HOW:  Updates threat severity, triggers level reassessment
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @param type Threat type to report
 * @param severity Threat severity 0.0 (none) to 1.0 (critical)
 * @return 0 on success, negative error code on failure
 */
int protective_cutoff_report_threat(
    protective_cutoff_t* cutoff,
    protective_threat_type_t type,
    float severity
);

//=============================================================================
// State Queries
//=============================================================================

/**
 * WHAT: Get current protection level
 * WHY:  Allow modules to query current operational constraints
 * HOW:  Returns cached protection level (thread-safe)
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @return Current protection level
 */
protection_level_t protective_cutoff_get_level(const protective_cutoff_t* cutoff);

/**
 * WHAT: Check if an operation is allowed at current protection level
 * WHY:  Gate operations based on current threat state
 * HOW:  Consults capability matrix for current level
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @param operation Operation type to check
 * @return true if operation is permitted, false otherwise
 */
bool protective_cutoff_can_execute(
    const protective_cutoff_t* cutoff,
    operation_type_t operation
);

/**
 * WHAT: Get current threat severity for a specific type
 * WHY:  Allow modules to inspect individual threat dimensions
 * HOW:  Returns cached threat severity (thread-safe)
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @param type Threat type to query
 * @return Threat severity 0.0-1.0, or -1.0 on error
 */
float protective_cutoff_get_threat(
    const protective_cutoff_t* cutoff,
    protective_threat_type_t type
);

//=============================================================================
// Manual Control
//=============================================================================

/**
 * WHAT: Force protection level to a specific value (emergency override)
 * WHY:  Allow manual intervention during critical situations
 * HOW:  Bypasses normal threat assessment, sets level directly
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @param level Protection level to force
 * @return 0 on success, negative error code on failure
 */
int protective_cutoff_force_level(
    protective_cutoff_t* cutoff,
    protection_level_t level
);

/**
 * WHAT: Attempt automatic recovery to lower protection level
 * WHY:  Allow system to recover after threat subsides
 * HOW:  Re-evaluates threats with hysteresis, downgrades if safe
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @return 0 on success, negative error code on failure
 */
int protective_cutoff_attempt_recovery(protective_cutoff_t* cutoff);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * WHAT: Register callback for protection level transitions
 * WHY:  Allow modules to react to protection state changes
 * HOW:  Stores callback and user data, invokes on level changes
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @param callback Callback function (must not be NULL)
 * @param user_data User data passed to callback (optional)
 * @return 0 on success, negative error code on failure
 */
int protective_cutoff_register_callback(
    protective_cutoff_t* cutoff,
    protection_level_callback_t callback,
    void* user_data
);

//=============================================================================
// Update and Assessment
//=============================================================================

/**
 * WHAT: Update protective cutoff system (evaluate threats, transition levels)
 * WHY:  Periodic assessment of threat landscape and protection state
 * HOW:  Combines threat severities, compares to thresholds, transitions levels
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @return 0 on success, negative error code on failure
 */
int protective_cutoff_update(protective_cutoff_t* cutoff);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging system
 * WHY:  Enable inter-module communication of protection state
 * HOW:  Registers with bio-async router, publishes level changes
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @return 0 on success, negative error code on failure
 */
int protective_cutoff_connect_bio_async(protective_cutoff_t* cutoff);

/**
 * WHAT: Disconnect from bio-async messaging system
 * WHY:  Clean up bio-async resources on shutdown
 * HOW:  Unregisters from bio-async router
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @return 0 on success, negative error code on failure
 */
int protective_cutoff_disconnect_bio_async(protective_cutoff_t* cutoff);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query bio-async integration status
 * HOW:  Returns cached connection state
 *
 * @param cutoff Protective cutoff context (must not be NULL)
 * @return true if connected, false otherwise
 */
bool protective_cutoff_is_bio_async_connected(const protective_cutoff_t* cutoff);

//=============================================================================
// String Conversion Utilities
//=============================================================================

/**
 * WHAT: Convert protection level to string
 * WHY:  Human-readable logging and debugging
 * HOW:  Maps enum to string constant
 *
 * @param level Protection level to convert
 * @return String representation
 */
const char* protective_cutoff_level_to_string(protection_level_t level);

/**
 * WHAT: Convert threat type to string
 * WHY:  Human-readable logging and debugging
 * HOW:  Maps enum to string constant
 *
 * @param type Threat type to convert
 * @return String representation
 */
const char* protective_cutoff_threat_to_string(protective_threat_type_t type);

/**
 * WHAT: Convert operation type to string
 * WHY:  Human-readable logging and debugging
 * HOW:  Maps enum to string constant
 *
 * @param op Operation type to convert
 * @return String representation
 */
const char* protective_cutoff_operation_to_string(operation_type_t op);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PROTECTIVE_CUTOFF_H

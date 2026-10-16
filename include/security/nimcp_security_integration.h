/**
 * @file nimcp_security_integration.h
 * @brief Universal Security Integration Framework for NIMCP
 *
 * Phase SC-4: Security Integration Layer
 *
 * Provides a unified security integration API that allows ALL NIMCP modules
 * to leverage the mathematical security framework:
 *
 * 1. ENTROPY MONITORING - Detect anomalies in module data/memory
 * 2. TRUST MANAGEMENT - Track trustworthiness of each module
 * 3. PRIVACY PROTECTION - Differential privacy for sensitive queries
 * 4. SECURITY EVENTS - Unified security event propagation
 * 5. SELF-MONITORING - Security system monitors itself
 *
 * ARCHITECTURE:
 * +------------------------------------------------------------------+
 * |                    NIMCP Security Integration                     |
 * +------------------------------------------------------------------+
 * |  Module Registry  |  Event Bus  |  Trust Network  |  DP Context  |
 * +------------------------------------------------------------------+
 * |                      Mathematical Security Core                   |
 * |  (Entropy Analysis)  (Bayesian Trust)  (Differential Privacy)    |
 * +------------------------------------------------------------------+
 *
 * @version 1.0.0
 * @author NIMCP Security Team
 */

#ifndef NIMCP_SECURITY_INTEGRATION_H
#define NIMCP_SECURITY_INTEGRATION_H

#include "security/nimcp_security_math.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory_pool.h"  // Phase SC-4.1: Memory Pool Integration
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

#define NIMCP_SEC_MAX_MODULES           256
#define NIMCP_SEC_MAX_REGIONS           1024
#define NIMCP_SEC_MAX_EVENTS_QUEUE      4096
#define NIMCP_SEC_MODULE_NAME_LEN       64
#define NIMCP_SEC_REGION_NAME_LEN       128

/**
 * @brief Module categories for security classification
 */
typedef enum {
    NIMCP_SEC_CAT_CORE = 0,         /**< Core modules (brain, neurons, events) */
    NIMCP_SEC_CAT_COGNITIVE,         /**< Cognitive modules (memory, executive) */
    NIMCP_SEC_CAT_MIDDLEWARE,        /**< Middleware modules (pipeline, encoding) */
    NIMCP_SEC_CAT_GLIAL,             /**< Glial support modules */
    NIMCP_SEC_CAT_PLASTICITY,        /**< Plasticity/learning modules */
    NIMCP_SEC_CAT_NETWORKING,        /**< Networking modules */
    NIMCP_SEC_CAT_IO,                /**< I/O and serialization modules */
    NIMCP_SEC_CAT_UTILITY,           /**< Utility modules */
    NIMCP_SEC_CAT_GPU,               /**< GPU acceleration modules */
    NIMCP_SEC_CAT_API,               /**< API and binding modules */
    NIMCP_SEC_CAT_SECURITY,          /**< Security modules (self-monitoring) */
    NIMCP_SEC_CAT_COUNT
} nimcp_sec_module_category_t;

/**
 * @brief Security event types
 */
typedef enum {
    NIMCP_SEC_EVENT_NONE = 0,
    NIMCP_SEC_EVENT_ENTROPY_ANOMALY,     /**< Entropy deviation detected */
    NIMCP_SEC_EVENT_TRUST_CHANGE,        /**< Trust score changed significantly */
    NIMCP_SEC_EVENT_TRUST_VIOLATION,     /**< Module fell below trust threshold */
    NIMCP_SEC_EVENT_PRIVACY_BUDGET,      /**< Privacy budget warning/exhaustion */
    NIMCP_SEC_EVENT_INTEGRITY_CHECK,     /**< Periodic integrity check result */
    NIMCP_SEC_EVENT_MODULE_REGISTERED,   /**< New module registered */
    NIMCP_SEC_EVENT_MODULE_UNREGISTERED, /**< Module unregistered */
    NIMCP_SEC_EVENT_REGION_TAMPERED,     /**< Memory region tampering detected */
    NIMCP_SEC_EVENT_SELF_CHECK,          /**< Security self-check event */
    NIMCP_SEC_EVENT_COUNT
} nimcp_sec_event_type_t;

/**
 * @brief Security severity levels
 */
typedef enum {
    NIMCP_SEC_SEVERITY_INFO = 0,
    NIMCP_SEC_SEVERITY_LOW,
    NIMCP_SEC_SEVERITY_MEDIUM,
    NIMCP_SEC_SEVERITY_HIGH,
    NIMCP_SEC_SEVERITY_CRITICAL
} nimcp_sec_severity_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Registered module information
 */
typedef struct {
    uint32_t module_id;                          /**< Unique module ID */
    char name[NIMCP_SEC_MODULE_NAME_LEN];        /**< Module name */
    nimcp_sec_module_category_t category;        /**< Module category */
    nimcp_trust_score_t trust_score;             /**< Current trust score */
    double entropy_baseline;                     /**< Established entropy baseline */
    uint64_t last_check_time;                    /**< Last integrity check time */
    uint64_t interaction_count;                  /**< Total interactions */
    uint64_t anomaly_count;                      /**< Total anomalies detected */
    bool active;                                 /**< Module is active */
    bool self_monitoring;                        /**< This is a security module */
} nimcp_sec_module_info_t;

/**
 * @brief Memory region monitored by security
 */
typedef struct {
    uint32_t region_id;                          /**< Unique region ID */
    uint32_t owner_module_id;                    /**< Owning module ID */
    char name[NIMCP_SEC_REGION_NAME_LEN];        /**< Region name/description */
    const void* base_address;                    /**< Base address (for reference) */
    size_t size;                                 /**< Region size */
    double baseline_entropy;                     /**< Baseline entropy value */
    double last_entropy;                         /**< Last measured entropy */
    uint64_t last_check_time;                    /**< Last check timestamp */
    uint32_t check_count;                        /**< Number of checks performed */
    uint32_t anomaly_count;                      /**< Anomalies detected */
    bool active;                                 /**< Region is active */
} nimcp_sec_region_info_t;

/**
 * @brief Security event structure
 */
typedef struct {
    nimcp_sec_event_type_t type;                 /**< Event type */
    nimcp_sec_severity_t severity;               /**< Severity level */
    uint32_t module_id;                          /**< Source module ID */
    uint32_t region_id;                          /**< Related region (if applicable) */
    uint64_t timestamp;                          /**< Event timestamp */
    double value;                                /**< Associated numeric value */
    char description[256];                       /**< Human-readable description */
} nimcp_sec_event_t;

/**
 * @brief Security event callback function type
 */
typedef void (*nimcp_sec_event_callback_t)(const nimcp_sec_event_t* event, void* user_data);

/**
 * @brief Security integration configuration
 */
typedef struct {
    double trust_threshold;                      /**< Minimum trust threshold (default: 0.5) */
    double entropy_deviation_threshold;          /**< Sigma threshold for anomaly (default: 3.0) */
    double privacy_budget;                       /**< Total DP budget (default: 10.0) */
    uint32_t integrity_check_interval_ms;        /**< Check interval (default: 1000) */
    uint32_t self_check_interval_ms;             /**< Self-check interval (default: 5000) */
    bool enable_continuous_monitoring;           /**< Enable continuous checks */
    bool enable_self_monitoring;                 /**< Security monitors itself */
    bool enable_event_logging;                   /**< Log all security events */
    nimcp_sec_event_callback_t event_callback;   /**< Event notification callback */
    void* callback_user_data;                    /**< User data for callback */

    // === PHASE SC-4.1: MEMORY POOL CONFIGURATION ===
    bool enable_memory_pools;                    /**< Enable memory pools for allocations (default: true) */
    uint32_t event_pool_size;                    /**< Number of events in pool (default: 4096) */
    uint32_t module_pool_size;                   /**< Number of modules in pool (default: 256) */
    uint32_t region_pool_size;                   /**< Number of regions in pool (default: 1024) */

    // === PHASE SC-4.2: COPY-ON-WRITE CONFIGURATION ===
    bool enable_cow;                             /**< Enable COW for security context sharing (default: true) */
    bool enable_cow_tracking;                    /**< Track COW statistics (default: true) */
} nimcp_sec_integration_config_t;

/**
 * @brief Security integration statistics
 */
typedef struct {
    uint32_t registered_modules;                 /**< Total registered modules */
    uint32_t active_modules;                     /**< Currently active modules */
    uint32_t monitored_regions;                  /**< Total monitored regions */
    uint64_t total_integrity_checks;             /**< Total checks performed */
    uint64_t total_anomalies_detected;           /**< Total anomalies found */
    uint64_t total_trust_violations;             /**< Trust threshold violations */
    uint64_t total_events_generated;             /**< Total security events */
    uint64_t self_checks_performed;              /**< Self-monitoring checks */
    double average_trust_score;                  /**< Average trust across modules */
    double privacy_budget_remaining;             /**< Remaining DP budget */
    uint64_t uptime_ms;                          /**< Security system uptime */

    // === PHASE SC-4.1: MEMORY POOL STATISTICS ===
    uint64_t pool_allocations;                   /**< Total pool allocations */
    uint64_t pool_deallocations;                 /**< Total pool deallocations */
    uint64_t pool_events_available;              /**< Available event slots */
    uint64_t pool_modules_available;             /**< Available module slots */
    uint64_t pool_regions_available;             /**< Available region slots */
    uint64_t pool_memory_saved_bytes;            /**< Estimated memory saved vs malloc */

    // === PHASE SC-4.2: COW STATISTICS ===
    uint32_t cow_contexts_shared;                /**< Number of COW-shared contexts */
    uint32_t cow_copies_triggered;               /**< Copy-on-write triggers */
    uint64_t cow_memory_saved_bytes;             /**< Memory saved via COW sharing */
} nimcp_sec_integration_stats_t;

/**
 * @brief COW state for security context
 */
typedef enum {
    NIMCP_SEC_COW_PRIVATE = 0,    /**< Private copy - full ownership */
    NIMCP_SEC_COW_SHARED,         /**< Shared via COW - read-only */
    NIMCP_SEC_COW_DIRTY           /**< Modified after COW - needs copy */
} nimcp_sec_cow_state_t;

/**
 * @brief Security integration context (opaque)
 */
typedef struct nimcp_sec_integration nimcp_sec_integration_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create security integration context
 * @return Context or NULL on failure
 */
nimcp_sec_integration_t* nimcp_sec_integration_create(void);

/**
 * @brief Initialize security integration
 * @param ctx Integration context
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_integration_init(
    nimcp_sec_integration_t* ctx,
    const nimcp_sec_integration_config_t* config
);

/**
 * @brief Destroy security integration context
 * @param ctx Integration context
 */
void nimcp_sec_integration_destroy(nimcp_sec_integration_t* ctx);

/**
 * @brief Get default configuration
 * @return Default configuration
 */
nimcp_sec_integration_config_t nimcp_sec_integration_default_config(void);

//=============================================================================
// Module Registration
//=============================================================================

/**
 * @brief Register a module with the security system
 *
 * Every NIMCP module should call this during initialization to:
 * - Establish trust identity
 * - Enable security monitoring
 * - Participate in the security network
 *
 * @param ctx Integration context
 * @param name Module name (e.g., "brain", "middleware_pipeline")
 * @param category Module category
 * @param module_id Output: assigned module ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_register_module(
    nimcp_sec_integration_t* ctx,
    const char* name,
    nimcp_sec_module_category_t category,
    uint32_t* module_id
);

/**
 * @brief Unregister a module
 * @param ctx Integration context
 * @param module_id Module ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_unregister_module(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id
);

/**
 * @brief Get module information
 * @param ctx Integration context
 * @param module_id Module ID
 * @param info Output: module information
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_get_module_info(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    nimcp_sec_module_info_t* info
);

//=============================================================================
// Memory Region Monitoring
//=============================================================================

/**
 * @brief Register a memory region for entropy monitoring
 *
 * Modules should register critical data regions (configuration, state,
 * learned parameters) to enable tampering detection.
 *
 * @param ctx Integration context
 * @param module_id Owning module ID
 * @param name Region name/description
 * @param data Region data pointer
 * @param size Region size
 * @param region_id Output: assigned region ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_register_region(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    const char* name,
    const void* data,
    size_t size,
    uint32_t* region_id
);

/**
 * @brief Update region baseline (after legitimate changes)
 * @param ctx Integration context
 * @param region_id Region ID
 * @param data New data pointer
 * @param size New size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_update_region_baseline(
    nimcp_sec_integration_t* ctx,
    uint32_t region_id,
    const void* data,
    size_t size
);

/**
 * @brief Check region integrity
 * @param ctx Integration context
 * @param region_id Region ID
 * @param data Current data pointer
 * @param size Current size
 * @param is_anomaly Output: true if anomaly detected
 * @param deviation Output: deviation from baseline (sigma)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_check_region(
    nimcp_sec_integration_t* ctx,
    uint32_t region_id,
    const void* data,
    size_t size,
    bool* is_anomaly,
    double* deviation
);

/**
 * @brief Unregister a memory region
 * @param ctx Integration context
 * @param region_id Region ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_unregister_region(
    nimcp_sec_integration_t* ctx,
    uint32_t region_id
);

//=============================================================================
// Trust Management
//=============================================================================

/**
 * @brief Record a module interaction (success or failure)
 *
 * Modules should call this after significant operations to update
 * their trust scores.
 *
 * @param ctx Integration context
 * @param module_id Module ID
 * @param success Was the interaction successful?
 * @param weight Interaction weight (1.0 = normal)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_record_interaction(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    bool success,
    double weight
);

/**
 * @brief Get module trust score
 * @param ctx Integration context
 * @param module_id Module ID
 * @param score Output: trust score
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_get_trust_score(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    nimcp_trust_score_t* score
);

/**
 * @brief Check if module is trusted
 * @param ctx Integration context
 * @param module_id Module ID
 * @return true if trusted, false otherwise
 */
bool nimcp_sec_is_module_trusted(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id
);

/**
 * @brief Add trust voucher (module A vouches for module B)
 * @param ctx Integration context
 * @param voucher_module_id Module doing the vouching
 * @param target_module_id Module being vouched for
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_add_trust_voucher(
    nimcp_sec_integration_t* ctx,
    uint32_t voucher_module_id,
    uint32_t target_module_id
);

/**
 * @brief Propagate trust through the module network
 * @param ctx Integration context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_propagate_trust(nimcp_sec_integration_t* ctx);

//=============================================================================
// Differential Privacy Queries
//=============================================================================

/**
 * @brief Get privatized count from module data
 *
 * Use this for queries that count sensitive items.
 *
 * @param ctx Integration context
 * @param module_id Requesting module ID
 * @param true_count True count value
 * @param noisy_count Output: privatized count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_private_count(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    uint64_t true_count,
    double* noisy_count
);

/**
 * @brief Get privatized sum from module data
 * @param ctx Integration context
 * @param module_id Requesting module ID
 * @param true_sum True sum value
 * @param max_contribution Maximum individual contribution
 * @param noisy_sum Output: privatized sum
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_private_sum(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    double true_sum,
    double max_contribution,
    double* noisy_sum
);

/**
 * @brief Get privatized mean from module data
 * @param ctx Integration context
 * @param module_id Requesting module ID
 * @param true_mean True mean value
 * @param count Number of items
 * @param max_value Maximum individual value
 * @param noisy_mean Output: privatized mean
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_private_mean(
    nimcp_sec_integration_t* ctx,
    uint32_t module_id,
    double true_mean,
    uint64_t count,
    double max_value,
    double* noisy_mean
);

/**
 * @brief Get remaining privacy budget
 * @param ctx Integration context
 * @return Remaining epsilon budget
 */
double nimcp_sec_get_privacy_budget(nimcp_sec_integration_t* ctx);

/**
 * @brief Reset privacy budget (use sparingly!)
 * @param ctx Integration context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_reset_privacy_budget(nimcp_sec_integration_t* ctx);

//=============================================================================
// Continuous Monitoring
//=============================================================================

/**
 * @brief Start continuous security monitoring
 * @param ctx Integration context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_start_monitoring(nimcp_sec_integration_t* ctx);

/**
 * @brief Stop continuous security monitoring
 * @param ctx Integration context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_stop_monitoring(nimcp_sec_integration_t* ctx);

/**
 * @brief Perform a single integrity check cycle
 * @param ctx Integration context
 * @param anomalies_found Output: number of anomalies found
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_check_integrity(
    nimcp_sec_integration_t* ctx,
    uint32_t* anomalies_found
);

/**
 * @brief Perform security self-check
 *
 * The security system monitors its own integrity.
 *
 * @param ctx Integration context
 * @param passed Output: true if self-check passed
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_self_check(
    nimcp_sec_integration_t* ctx,
    bool* passed
);

//=============================================================================
// Event Handling
//=============================================================================

/**
 * @brief Get next security event (non-blocking)
 * @param ctx Integration context
 * @param event Output: event data
 * @return NIMCP_SUCCESS if event available, NIMCP_NOT_FOUND if queue empty
 */
nimcp_result_t nimcp_sec_get_event(
    nimcp_sec_integration_t* ctx,
    nimcp_sec_event_t* event
);

/**
 * @brief Get pending event count
 * @param ctx Integration context
 * @return Number of pending events
 */
uint32_t nimcp_sec_get_event_count(nimcp_sec_integration_t* ctx);

/**
 * @brief Clear all pending events
 * @param ctx Integration context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_clear_events(nimcp_sec_integration_t* ctx);

//=============================================================================
// Statistics and Reporting
//=============================================================================

/**
 * @brief Get integration statistics
 * @param ctx Integration context
 * @param stats Output: statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_get_stats(
    nimcp_sec_integration_t* ctx,
    nimcp_sec_integration_stats_t* stats
);

/**
 * @brief Get trust report for all modules
 * @param ctx Integration context
 * @param buffer Output buffer for report
 * @param buffer_size Buffer size
 * @return Number of bytes written or required
 */
size_t nimcp_sec_generate_trust_report(
    nimcp_sec_integration_t* ctx,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Get privatized statistics (for external reporting)
 *
 * Returns differentially private statistics safe for public disclosure.
 *
 * @param ctx Integration context
 * @param stats Output: privatized statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_get_private_stats(
    nimcp_sec_integration_t* ctx,
    nimcp_sec_integration_stats_t* stats
);

//=============================================================================
// Phase SC-4.1: Memory Pool API
//=============================================================================

/**
 * @brief Get memory pool statistics
 *
 * @param ctx Integration context
 * @param event_pool_stats Output: event pool stats (NULL to skip)
 * @param module_pool_stats Output: module pool stats (NULL to skip)
 * @param region_pool_stats Output: region pool stats (NULL to skip)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_get_pool_stats(
    nimcp_sec_integration_t* ctx,
    memory_pool_stats_t* event_pool_stats,
    memory_pool_stats_t* module_pool_stats,
    memory_pool_stats_t* region_pool_stats
);

/**
 * @brief Reset memory pools (reclaim all allocations)
 *
 * WARNING: Invalidates all module and region registrations!
 * Only use during system reset.
 *
 * @param ctx Integration context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_reset_pools(nimcp_sec_integration_t* ctx);

//=============================================================================
// Phase SC-4.2: Copy-on-Write API
//=============================================================================

/**
 * @brief Create a COW-shared clone of security context
 *
 * Creates a new security context that shares data with the source
 * until modifications are made (copy-on-write semantics).
 *
 * WHAT: Creates a lightweight clone sharing security state
 * WHY:  Enable efficient brain cloning without duplicating security data
 * HOW:  Reference counting + lazy copy on first write
 *
 * @param source Source security context
 * @return New COW-shared context or NULL on failure
 */
nimcp_sec_integration_t* nimcp_sec_integration_cow_clone(
    nimcp_sec_integration_t* source
);

/**
 * @brief Check if context is COW-shared
 *
 * @param ctx Integration context
 * @return true if context is sharing data via COW
 */
bool nimcp_sec_integration_is_cow_shared(nimcp_sec_integration_t* ctx);

/**
 * @brief Get COW state for context
 *
 * @param ctx Integration context
 * @return Current COW state
 */
nimcp_sec_cow_state_t nimcp_sec_integration_get_cow_state(nimcp_sec_integration_t* ctx);

/**
 * @brief Force COW copy (make context fully private)
 *
 * Triggers immediate copy of shared data to make context independent.
 * Call this before making modifications to avoid lazy copy overhead
 * during time-critical operations.
 *
 * @param ctx Integration context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_sec_integration_cow_detach(nimcp_sec_integration_t* ctx);

/**
 * @brief Get number of contexts sharing this COW data
 *
 * @param ctx Integration context
 * @return Reference count (1 = no sharing, >1 = shared)
 */
uint32_t nimcp_sec_integration_cow_refcount(nimcp_sec_integration_t* ctx);

//=============================================================================
// Utility Macros for Module Integration
//=============================================================================

/**
 * @brief Convenience macro to register a module
 *
 * Usage:
 *   static uint32_t my_module_id;
 *   NIMCP_SEC_REGISTER_MODULE(ctx, "my_module", NIMCP_SEC_CAT_COGNITIVE, my_module_id);
 */
#define NIMCP_SEC_REGISTER_MODULE(ctx, name, category, id_var) \
    do { \
        nimcp_result_t _res = nimcp_sec_register_module(ctx, name, category, &(id_var)); \
        if (_res != NIMCP_SUCCESS) { \
            /* Log warning but don't fail - security integration is optional */ \
        } \
    } while(0)

/**
 * @brief Convenience macro to record successful interaction
 */
#define NIMCP_SEC_SUCCESS(ctx, module_id) \
    nimcp_sec_record_interaction(ctx, module_id, true, 1.0)

/**
 * @brief Convenience macro to record failed interaction
 */
#define NIMCP_SEC_FAILURE(ctx, module_id) \
    nimcp_sec_record_interaction(ctx, module_id, false, 1.0)

/**
 * @brief Convenience macro to record critical failure (higher weight)
 */
#define NIMCP_SEC_CRITICAL_FAILURE(ctx, module_id) \
    nimcp_sec_record_interaction(ctx, module_id, false, 3.0)

//=============================================================================
// Category Names
//=============================================================================

/**
 * @brief Get category name string
 * @param category Category enum
 * @return Category name
 */
const char* nimcp_sec_category_name(nimcp_sec_module_category_t category);

/**
 * @brief Get event type name string
 * @param type Event type enum
 * @return Event type name
 */
const char* nimcp_sec_event_type_name(nimcp_sec_event_type_t type);

/**
 * @brief Get severity name string
 * @param severity Severity enum
 * @return Severity name
 */
const char* nimcp_sec_severity_name(nimcp_sec_severity_t severity);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_INTEGRATION_H */

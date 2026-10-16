/**
 * @file nimcp_lgss_override_controller.c
 * @brief Implementation of LGSS Override Controller
 *
 * WHAT: Emergency intervention mechanism for cognitive operations
 * WHY:  Enable authorized operators to halt, reset, or limit system capabilities
 * HOW:  Request-based system with authentication and audit logging
 *
 * SECURITY CRITICAL: Override operations require authentication (Phase B).
 *                    All actions are logged for audit purposes.
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0.0
 */

#include "security/lgss/nimcp_lgss_override_controller.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_async.h"
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_override_controller, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * COGNITIVE SYSTEM INTEGRATION
 *============================================================================*/

/**
 * @brief Override signal type for cognitive system integration
 */
typedef enum {
    OVERRIDE_SIGNAL_HALT = 0,          /**< Immediate halt signal */
    OVERRIDE_SIGNAL_SOFT_RESET = 1,    /**< Graceful restart signal */
    OVERRIDE_SIGNAL_HARD_RESET = 2,    /**< Force restart with state clear */
    OVERRIDE_SIGNAL_CAPABILITY_CHANGE = 3  /**< Capability level changed */
} override_signal_type_t;

/**
 * @brief Override signal payload for bio-async messaging
 */
typedef struct {
    override_signal_type_t signal_type;
    uint64_t request_id;
    char operator_id[NIMCP_OVERRIDE_MAX_OPERATOR_ID];
    char reason[NIMCP_OVERRIDE_MAX_REASON_LEN];
    capability_type_t affected_capability;
    float capability_level;
} override_signal_payload_t;

/** Global bio-async promise for cognitive system signaling (NULL if not initialized) */
static nimcp_bio_promise_t g_override_signal_promise = NULL;

/**
 * @brief Send override signal to cognitive system via bio-async
 *
 * WHAT: Signal cognitive system about override operation
 * WHY:  Cognitive modules need to know when to halt/reset
 * HOW:  Use bio-async norepinephrine channel (alerting/priority)
 *
 * @param signal_type Type of override signal
 * @param request Originating override request
 * @return NIMCP_SUCCESS or error code
 */
static nimcp_error_t send_override_signal_to_cognitive_system(
    override_signal_type_t signal_type,
    const override_request_t* request
) {
    // Prepare signal payload
    override_signal_payload_t payload;
    memset(&payload, 0, sizeof(payload));
    payload.signal_type = signal_type;
    payload.request_id = request->request_id;
    strncpy(payload.operator_id, request->operator_id, NIMCP_OVERRIDE_MAX_OPERATOR_ID - 1);
    strncpy(payload.reason, request->reason, NIMCP_OVERRIDE_MAX_REASON_LEN - 1);

    // Check if bio-async is initialized
    if (!nimcp_bio_async_is_initialized()) {
        LOG_WARN("OverrideCtrl: Bio-async not initialized - cognitive signal not sent");
        // Not a fatal error - cognitive system integration is optional
        return NIMCP_SUCCESS;
    }

    // Create a norepinephrine (alerting/priority) promise for the signal
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_NOREPINEPHRINE,
        sizeof(override_signal_payload_t)
    );

    if (!promise) {
        LOG_WARN("OverrideCtrl: Failed to create bio-async promise for cognitive signal");
        // Non-fatal: cognitive system might not be listening
        return NIMCP_SUCCESS;
    }

    // Complete the promise with the signal payload
    nimcp_error_t result = nimcp_bio_promise_complete(promise, &payload);

    if (result != NIMCP_SUCCESS) {
        LOG_WARN("OverrideCtrl: Failed to send cognitive signal (err=%d)", result);
    } else {
        LOG_INFO("OverrideCtrl: Sent %s signal to cognitive system (request=%lu)",
                 signal_type == OVERRIDE_SIGNAL_HALT ? "HALT" :
                 signal_type == OVERRIDE_SIGNAL_SOFT_RESET ? "SOFT_RESET" :
                 signal_type == OVERRIDE_SIGNAL_HARD_RESET ? "HARD_RESET" : "CAPABILITY_CHANGE",
                 (unsigned long)request->request_id);
    }

    // Destroy promise (future is delivered asynchronously)
    nimcp_bio_promise_destroy(promise);

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast halt signal to all cognitive subsystems
 *
 * WHAT: Signal immediate halt to all cognitive operations
 * WHY:  Emergency stop must reach all components quickly
 * HOW:  Multiple signals via different channels for redundancy
 */
static nimcp_error_t broadcast_halt_signal(const override_request_t* request) {
    LOG_WARN("OverrideCtrl: Broadcasting HALT signal to cognitive system");

    // Send via norepinephrine channel (primary alerting path)
    send_override_signal_to_cognitive_system(OVERRIDE_SIGNAL_HALT, request);

    // Additional logging for audit trail
    LOG_INFO("OverrideCtrl: HALT broadcast complete - request=%lu operator=%s reason=%s",
             (unsigned long)request->request_id,
             request->operator_id,
             request->reason);

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast soft reset signal to cognitive subsystems
 *
 * WHAT: Signal graceful restart to cognitive operations
 * WHY:  Allow cognitive state to be preserved where possible
 * HOW:  Serotonin channel (mood/state coordination) for gradual transition
 */
static nimcp_error_t broadcast_soft_reset_signal(const override_request_t* request) {
    LOG_INFO("OverrideCtrl: Broadcasting SOFT_RESET signal to cognitive system");

    send_override_signal_to_cognitive_system(OVERRIDE_SIGNAL_SOFT_RESET, request);

    LOG_INFO("OverrideCtrl: SOFT_RESET broadcast complete - request=%lu operator=%s",
             (unsigned long)request->request_id,
             request->operator_id);

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast hard reset signal to cognitive subsystems
 *
 * WHAT: Signal force restart with state clear to cognitive operations
 * WHY:  Complete system restart when soft reset is insufficient
 * HOW:  Norepinephrine channel (high priority alert)
 */
static nimcp_error_t broadcast_hard_reset_signal(const override_request_t* request) {
    LOG_WARN("OverrideCtrl: Broadcasting HARD_RESET signal to cognitive system");

    send_override_signal_to_cognitive_system(OVERRIDE_SIGNAL_HARD_RESET, request);

    LOG_INFO("OverrideCtrl: HARD_RESET broadcast complete - request=%lu operator=%s",
             (unsigned long)request->request_id,
             request->operator_id);

    return NIMCP_SUCCESS;
}


/*=============================================================================
 * MODULE CONSTANTS
 *============================================================================*/

#define MODULE_NAME "OverrideCtrl"

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Override request entry in the queue
 */
typedef struct request_entry {
    override_request_t request;
    struct request_entry* next;
} request_entry_t;

/**
 * @brief Capability state tracking
 */
typedef struct {
    float reduction_level;      /**< Current reduction level [0.0, 1.0] */
    uint64_t reduction_start;   /**< When reduction started (0 if not active) */
    uint32_t duration_sec;      /**< Duration (0 = indefinite) */
    char operator_id[NIMCP_OVERRIDE_MAX_OPERATOR_ID];
    char reason[NIMCP_OVERRIDE_MAX_REASON_LEN];
} capability_state_t;

/**
 * @brief Internal override controller implementation
 */
struct override_controller_impl {
    uint32_t magic;                         /**< Magic number for validation */
    override_config_t config;               /**< Configuration */
    nimcp_platform_mutex_t mutex;           /**< Main mutex */

    /** Request queue */
    request_entry_t* request_head;
    request_entry_t* request_tail;
    uint32_t request_count;

    /** Capability states */
    capability_state_t capabilities[CAPABILITY_TYPE_COUNT];

    /** ID generator */
    atomic_uint_fast64_t next_request_id;

    /** Statistics */
    override_stats_t stats;
    uint64_t total_execution_time_us;

    /** Audit file handle (if file logging is enabled) */
    FILE* audit_file;
};

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Validate controller handle
 */
static inline bool is_valid_controller(override_controller_t controller) {
    return controller != NULL && controller->magic == NIMCP_OVERRIDE_CONTROLLER_MAGIC;
}

/**
 * @brief Lock controller mutex
 */
static inline void controller_lock(override_controller_t controller) {
    nimcp_platform_mutex_lock(&controller->mutex);
}

/**
 * @brief Unlock controller mutex
 */
static inline void controller_unlock(override_controller_t controller) {
    nimcp_platform_mutex_unlock(&controller->mutex);
}

/**
 * @brief Find request by ID (internal, unlocked)
 */
static request_entry_t* find_request_unlocked(
    override_controller_t controller,
    uint64_t request_id
) {
    request_entry_t* current = controller->request_head;
    while (current) {
        if (current->request.request_id == request_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Validate authentication token (placeholder for Phase B)
 *
 * WHAT: Validate operator authentication token
 * WHY:  Ensure only authorized operators can execute overrides
 * HOW:  Placeholder - returns true if auth not required, false otherwise
 *
 * NOTE: This is a placeholder. Phase B will implement actual authentication
 *       using cryptographic tokens, multi-factor auth, etc.
 */
static bool validate_auth_token(
    override_controller_t controller,
    const override_request_t* request
) {
    // If authentication is not required, allow
    if (!controller->config.require_auth) {
        return true;
    }

    // Phase B: Implement actual authentication validation here
    // For now, reject if auth is required but no token provided
    if (request->auth_token_size == 0) {
        LOG_WARN("%s: Authentication required but no token provided",
                 MODULE_NAME);
        return false;
    }

    // Placeholder: accept any non-empty token
    // Production implementation would validate against auth system
    LOG_INFO("%s: Auth token validation placeholder - accepting token (Phase B pending)",
             MODULE_NAME);
    return true;
}

/**
 * @brief Log audit entry to file and/or console
 */
static void log_audit_entry(
    override_controller_t controller,
    const char* action,
    const override_request_t* request,
    const override_result_t* result
) {
    if (!controller->config.enable_audit_log) {
        return;
    }

    uint64_t timestamp = get_time_us();

    // Log to console
    LOG_INFO("%s: AUDIT %s request=%lu type=%s operator=%s status=%s reason=\"%.50s...\"",
             MODULE_NAME,
             action,
             request ? (unsigned long)request->request_id : 0,
             request ? override_type_name(request->type) : "N/A",
             request ? request->operator_id : "N/A",
             result ? override_status_name(result->status) : "N/A",
             request ? request->reason : "N/A");

    // Log to audit file if configured
    if (controller->audit_file) {
        fprintf(controller->audit_file,
                "%lu|%s|%lu|%s|%s|%s|%s\n",
                (unsigned long)timestamp,
                action,
                request ? (unsigned long)request->request_id : 0,
                request ? override_type_name(request->type) : "N/A",
                request ? request->operator_id : "N/A",
                result ? override_status_name(result->status) : "N/A",
                request ? request->reason : "N/A");
        fflush(controller->audit_file);
    }
}

/**
 * @brief Execute HALT override (internal, unlocked)
 *
 * WHAT: Immediately halt all cognitive operations
 * WHY:  Emergency stop for safety-critical situations
 * HOW:  1. Broadcast halt signal to cognitive system via bio-async
 *       2. Set all capabilities to fully reduced (local state)
 *       3. Log for audit purposes
 */
static nimcp_error_t execute_halt_unlocked(
    override_controller_t controller,
    override_request_t* request,
    override_result_t* result
) {
    LOG_WARN("%s: Executing HALT override (request %lu)",
             MODULE_NAME, (unsigned long)request->request_id);

    // Broadcast halt signal to cognitive system via bio-async
    // This signals all cognitive modules to stop processing immediately
    broadcast_halt_signal(request);

    // Set all capabilities to fully reduced (local state backup)
    // This ensures capability queries reflect the halted state
    for (int i = 0; i < CAPABILITY_TYPE_COUNT; i++) {
        controller->capabilities[i].reduction_level = 1.0f;  // Fully disabled
        controller->capabilities[i].reduction_start = get_time_us();
        controller->capabilities[i].duration_sec = 0;  // Indefinite
        strncpy(controller->capabilities[i].operator_id, request->operator_id,
                NIMCP_OVERRIDE_MAX_OPERATOR_ID - 1);
        strncpy(controller->capabilities[i].reason, request->reason,
                NIMCP_OVERRIDE_MAX_REASON_LEN - 1);
    }

    controller->stats.halts_executed++;
    controller->stats.active_capability_reductions = CAPABILITY_TYPE_COUNT;

    // Send heartbeat indicating halt operation
    lgss_override_controller_heartbeat("halt_execute", 1.0f);

    result->status = OVERRIDE_STATUS_COMPLETED;
    result->components_affected = CAPABILITY_TYPE_COUNT;

    LOG_INFO("%s: HALT override completed - all %d capabilities disabled",
             MODULE_NAME, CAPABILITY_TYPE_COUNT);

    return NIMCP_SUCCESS;
}

/**
 * @brief Execute SOFT_RESET override (internal, unlocked)
 *
 * WHAT: Gracefully restart cognitive processes
 * WHY:  Allow cognitive state to be preserved where possible
 * HOW:  1. Broadcast soft reset signal to cognitive system
 *       2. Reset capability reductions (restore normal operation)
 *       3. Cognitive modules handle their own state preservation
 */
static nimcp_error_t execute_soft_reset_unlocked(
    override_controller_t controller,
    override_request_t* request,
    override_result_t* result
) {
    LOG_INFO("%s: Executing SOFT_RESET override (request %lu)",
             MODULE_NAME, (unsigned long)request->request_id);

    // Broadcast soft reset signal to cognitive system via bio-async
    // Uses serotonin channel for gradual state transition
    broadcast_soft_reset_signal(request);

    // Reset capability reductions (restore normal operation)
    for (int i = 0; i < CAPABILITY_TYPE_COUNT; i++) {
        controller->capabilities[i].reduction_level = 0.0f;
        controller->capabilities[i].reduction_start = 0;
        controller->capabilities[i].duration_sec = 0;
        memset(controller->capabilities[i].operator_id, 0,
               NIMCP_OVERRIDE_MAX_OPERATOR_ID);
        memset(controller->capabilities[i].reason, 0,
               NIMCP_OVERRIDE_MAX_REASON_LEN);
    }

    controller->stats.soft_resets_executed++;
    controller->stats.active_capability_reductions = 0;

    // Send heartbeat indicating soft reset operation
    lgss_override_controller_heartbeat("soft_reset_execute", 1.0f);

    result->status = OVERRIDE_STATUS_COMPLETED;
    result->components_affected = CAPABILITY_TYPE_COUNT;

    LOG_INFO("%s: SOFT_RESET override completed - capabilities restored",
             MODULE_NAME);

    return NIMCP_SUCCESS;
}

/**
 * @brief Execute HARD_RESET override (internal, unlocked)
 *
 * WHAT: Force restart cognitive system with state clear
 * WHY:  Complete system restart when soft reset is insufficient
 * HOW:  1. Broadcast hard reset signal (high priority)
 *       2. Clear all capability state
 *       3. Cognitive modules must clear their internal state
 *
 * SECURITY: Hard reset is more aggressive than soft reset.
 *           All transient state is lost. Use only when necessary.
 */
static nimcp_error_t execute_hard_reset_unlocked(
    override_controller_t controller,
    override_request_t* request,
    override_result_t* result
) {
    LOG_WARN("%s: Executing HARD_RESET override (request %lu)",
             MODULE_NAME, (unsigned long)request->request_id);

    // Broadcast hard reset signal to cognitive system via bio-async
    // Uses norepinephrine channel for high priority alert
    broadcast_hard_reset_signal(request);

    // Clear all capability state (more aggressive than soft reset)
    for (int i = 0; i < CAPABILITY_TYPE_COUNT; i++) {
        controller->capabilities[i].reduction_level = 0.0f;
        controller->capabilities[i].reduction_start = 0;
        controller->capabilities[i].duration_sec = 0;
        memset(controller->capabilities[i].operator_id, 0,
               NIMCP_OVERRIDE_MAX_OPERATOR_ID);
        memset(controller->capabilities[i].reason, 0,
               NIMCP_OVERRIDE_MAX_REASON_LEN);
    }

    controller->stats.hard_resets_executed++;
    controller->stats.active_capability_reductions = 0;

    // Send heartbeat indicating hard reset operation
    lgss_override_controller_heartbeat("hard_reset_execute", 1.0f);

    result->status = OVERRIDE_STATUS_COMPLETED;
    result->components_affected = CAPABILITY_TYPE_COUNT;

    LOG_INFO("%s: HARD_RESET override completed - all state cleared",
             MODULE_NAME);

    return NIMCP_SUCCESS;
}

/**
 * @brief Execute REDUCE_CAPABILITY override (internal, unlocked)
 */
static nimcp_error_t execute_reduce_capability_unlocked(
    override_controller_t controller,
    override_request_t* request,
    override_result_t* result
) {
    LOG_INFO("%s: Executing REDUCE_CAPABILITY override (request %lu, %u capabilities)",
             MODULE_NAME, (unsigned long)request->request_id, request->capability_count);

    uint32_t affected = 0;

    for (uint32_t i = 0; i < request->capability_count; i++) {
        capability_reduction_t* cap = &request->capabilities[i];

        if (cap->capability >= CAPABILITY_TYPE_COUNT) {
            LOG_WARN("%s: Invalid capability type %d", MODULE_NAME, cap->capability);
            continue;
        }

        capability_state_t* state = &controller->capabilities[cap->capability];
        state->reduction_level = cap->reduction_level;
        state->reduction_start = get_time_us();
        state->duration_sec = cap->duration_sec;
        strncpy(state->operator_id, request->operator_id,
                NIMCP_OVERRIDE_MAX_OPERATOR_ID - 1);
        strncpy(state->reason, request->reason,
                NIMCP_OVERRIDE_MAX_REASON_LEN - 1);

        affected++;

        LOG_INFO("%s: Capability %s reduced to %.0f%% for %us",
                 MODULE_NAME, capability_type_name(cap->capability),
                 (1.0f - cap->reduction_level) * 100.0f,
                 cap->duration_sec);
    }

    // Count active reductions
    uint32_t active = 0;
    for (int i = 0; i < CAPABILITY_TYPE_COUNT; i++) {
        if (controller->capabilities[i].reduction_level > 0.0f) {
            active++;
        }
    }
    controller->stats.active_capability_reductions = active;

    result->status = OVERRIDE_STATUS_COMPLETED;
    result->components_affected = affected;

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

override_config_t override_controller_default_config(void) {
    override_config_t config = {
        .max_pending = NIMCP_OVERRIDE_MAX_PENDING,
        .default_timeout_ms = NIMCP_OVERRIDE_DEFAULT_TIMEOUT_MS,
        .require_auth = false,  // Phase B: set to true when auth is implemented
        .enable_audit_log = true,
        .audit_log_path = NULL,
        .allow_remote = false,  // Secure default: no remote overrides
        .event_callback = NULL,
        .event_callback_data = NULL
    };
    return config;
}

override_controller_t override_controller_create(const override_config_t* config) {
    // Allocate controller instance
    override_controller_t controller = nimcp_calloc(1, sizeof(struct override_controller_impl));
    NIMCP_API_CHECK_ALLOC(controller, "Failed to allocate override controller");

    // Initialize mutex
    int mutex_result = nimcp_platform_mutex_init(&controller->mutex, false);
    if (mutex_result != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_MUTEX_INIT, "Failed to initialize override controller mutex");
        nimcp_free(controller);
        return NULL;
    }

    // Set configuration
    if (config) {
        memcpy(&controller->config, config, sizeof(override_config_t));
    } else {
        controller->config = override_controller_default_config();
    }

    // Initialize state
    controller->magic = NIMCP_OVERRIDE_CONTROLLER_MAGIC;
    controller->request_head = NULL;
    controller->request_tail = NULL;
    controller->request_count = 0;
    atomic_init(&controller->next_request_id, 1);
    memset(&controller->stats, 0, sizeof(override_stats_t));
    controller->total_execution_time_us = 0;

    // Initialize capability states (all at full capability)
    for (int i = 0; i < CAPABILITY_TYPE_COUNT; i++) {
        controller->capabilities[i].reduction_level = 0.0f;
        controller->capabilities[i].reduction_start = 0;
        controller->capabilities[i].duration_sec = 0;
    }

    // Open audit log file if configured
    controller->audit_file = NULL;
    if (controller->config.enable_audit_log && controller->config.audit_log_path) {
        controller->audit_file = fopen(controller->config.audit_log_path, "a");
        if (!controller->audit_file) {
            LOG_WARN("%s: Failed to open audit log file: %s",
                     MODULE_NAME, controller->config.audit_log_path);
        }
    }

    LOG_INFO("%s: Override Controller created (max_pending=%u, require_auth=%s)",
             MODULE_NAME, controller->config.max_pending,
             controller->config.require_auth ? "yes" : "no");

    return controller;
}

void override_controller_destroy(override_controller_t controller) {
    if (!is_valid_controller(controller)) {
        return;
    }

    controller_lock(controller);

    // Free pending requests
    request_entry_t* entry = controller->request_head;
    while (entry) {
        request_entry_t* next = entry->next;
        nimcp_free(entry);
        entry = next;
    }

    // Close audit file
    if (controller->audit_file) {
        fclose(controller->audit_file);
        controller->audit_file = NULL;
    }

    // Clear magic to invalidate handle
    controller->magic = 0;

    controller_unlock(controller);

    // Destroy mutex
    nimcp_platform_mutex_destroy(&controller->mutex);

    // Free controller instance
    nimcp_free(controller);

    LOG_INFO("%s: Override Controller destroyed", MODULE_NAME);
}

nimcp_error_t override_controller_request_override(
    override_controller_t controller,
    const override_request_t* request,
    uint64_t* request_id
) {
    // Validate parameters
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");
    NIMCP_CHECK_THROW(request, NIMCP_ERROR_NULL_POINTER, "request is NULL");
    NIMCP_CHECK_THROW(request_id, NIMCP_ERROR_NULL_POINTER, "request_id is NULL");

    // Validate override type
    if (request->type >= OVERRIDE_TYPE_COUNT) {
        LOG_ERROR("%s: Invalid override type %d", MODULE_NAME, request->type);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate operator ID is provided
    if (request->operator_id[0] == '\0') {
        LOG_ERROR("%s: Operator ID required", MODULE_NAME);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate reason is provided
    if (request->reason[0] == '\0') {
        LOG_ERROR("%s: Reason required for override", MODULE_NAME);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    controller_lock(controller);

    // Check pending capacity
    if (controller->request_count >= controller->config.max_pending) {
        controller_unlock(controller);
        LOG_ERROR("%s: Request queue full", MODULE_NAME);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    // Validate authentication
    if (!validate_auth_token(controller, request)) {
        controller->stats.requests_rejected++;
        log_audit_entry(controller, "REJECTED", request, NULL);
        controller_unlock(controller);
        return NIMCP_PERMISSION_DENIED;
    }

    // Allocate request entry
    request_entry_t* entry = nimcp_calloc(1, sizeof(request_entry_t));
    if (!entry) {
        controller_unlock(controller);
        LOG_ERROR("%s: Failed to allocate request entry", MODULE_NAME);
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Copy request
    memcpy(&entry->request, request, sizeof(override_request_t));

    // Assign request ID
    uint64_t rid = atomic_fetch_add(&controller->next_request_id, 1);
    entry->request.request_id = rid;
    entry->request.timestamp_us = get_time_us();
    entry->request.status = OVERRIDE_STATUS_AUTHORIZED;  // Auto-authorized for now
    entry->next = NULL;

    // Add to request queue
    if (controller->request_tail) {
        controller->request_tail->next = entry;
    } else {
        controller->request_head = entry;
    }
    controller->request_tail = entry;
    controller->request_count++;

    *request_id = rid;

    controller->stats.total_requests++;
    controller->stats.requests_authorized++;
    controller->stats.pending_requests = controller->request_count;

    log_audit_entry(controller, "REQUESTED", &entry->request, NULL);

    controller_unlock(controller);

    LOG_INFO("%s: Override request %lu queued (type=%s, operator=%s)",
             MODULE_NAME, (unsigned long)rid,
             override_type_name(request->type),
             request->operator_id);

    return NIMCP_SUCCESS;
}

nimcp_error_t override_controller_execute_override(
    override_controller_t controller,
    uint64_t request_id,
    override_result_t* result
) {
    // Validate parameters
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    controller_lock(controller);

    // Find request
    request_entry_t* entry = find_request_unlocked(controller, request_id);
    if (!entry) {
        controller_unlock(controller);
        return NIMCP_ERROR_NOT_FOUND;
    }

    // Check if request is authorized
    if (entry->request.status != OVERRIDE_STATUS_AUTHORIZED) {
        controller_unlock(controller);
        LOG_WARN("%s: Request %lu not authorized (status=%s)",
                 MODULE_NAME, (unsigned long)request_id,
                 override_status_name(entry->request.status));
        return NIMCP_ERROR_INVALID_STATE;
    }

    // Initialize result
    memset(result, 0, sizeof(override_result_t));
    result->request_id = request_id;
    result->execution_start_us = get_time_us();

    // Mark as executing
    entry->request.status = OVERRIDE_STATUS_EXECUTING;

    // Execute based on override type
    nimcp_error_t exec_result;
    switch (entry->request.type) {
        case OVERRIDE_TYPE_HALT:
            exec_result = execute_halt_unlocked(controller, &entry->request, result);
            break;

        case OVERRIDE_TYPE_SOFT_RESET:
            exec_result = execute_soft_reset_unlocked(controller, &entry->request, result);
            break;

        case OVERRIDE_TYPE_HARD_RESET:
            exec_result = execute_hard_reset_unlocked(controller, &entry->request, result);
            break;

        case OVERRIDE_TYPE_REDUCE_CAPABILITY:
            exec_result = execute_reduce_capability_unlocked(controller, &entry->request, result);
            break;

        default:
            LOG_ERROR("%s: Unknown override type %d", MODULE_NAME, entry->request.type);
            exec_result = NIMCP_ERROR_INVALID_PARAM;
            result->status = OVERRIDE_STATUS_FAILED;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Unknown override type: %d", entry->request.type);
            break;
    }

    // Record execution time
    result->execution_end_us = get_time_us();
    uint64_t exec_time = result->execution_end_us - result->execution_start_us;

    // Update request status
    if (exec_result == NIMCP_SUCCESS) {
        entry->request.status = OVERRIDE_STATUS_COMPLETED;
        controller->stats.requests_completed++;
    } else {
        entry->request.status = OVERRIDE_STATUS_FAILED;
        controller->stats.requests_failed++;
    }

    // Update statistics
    controller->total_execution_time_us += exec_time;
    if (controller->stats.requests_completed > 0) {
        controller->stats.avg_execution_time_us =
            controller->total_execution_time_us / controller->stats.requests_completed;
    }

    // Invoke callback if configured
    if (controller->config.event_callback) {
        controller->config.event_callback(result, controller->config.event_callback_data);
    }

    log_audit_entry(controller, "EXECUTED", &entry->request, result);

    controller_unlock(controller);

    LOG_INFO("%s: Override %lu executed (type=%s, status=%s, time=%luus)",
             MODULE_NAME, (unsigned long)request_id,
             override_type_name(entry->request.type),
             override_status_name(result->status),
             (unsigned long)exec_time);

    return exec_result;
}

nimcp_error_t override_controller_cancel_request(
    override_controller_t controller,
    uint64_t request_id
) {
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");

    controller_lock(controller);

    request_entry_t* entry = find_request_unlocked(controller, request_id);
    if (!entry) {
        controller_unlock(controller);
        return NIMCP_ERROR_NOT_FOUND;
    }

    // Can only cancel pending or authorized requests
    if (entry->request.status != OVERRIDE_STATUS_PENDING &&
        entry->request.status != OVERRIDE_STATUS_AUTHORIZED) {
        controller_unlock(controller);
        LOG_WARN("%s: Cannot cancel request %lu (status=%s)",
                 MODULE_NAME, (unsigned long)request_id,
                 override_status_name(entry->request.status));
        return NIMCP_ERROR_INVALID_STATE;
    }

    entry->request.status = OVERRIDE_STATUS_CANCELLED;

    log_audit_entry(controller, "CANCELLED", &entry->request, NULL);

    controller_unlock(controller);

    LOG_INFO("%s: Request %lu cancelled", MODULE_NAME, (unsigned long)request_id);

    return NIMCP_SUCCESS;
}

nimcp_error_t override_controller_get_request_status(
    override_controller_t controller,
    uint64_t request_id,
    override_request_t* request
) {
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");
    NIMCP_CHECK_THROW(request, NIMCP_ERROR_NULL_POINTER, "request is NULL");

    controller_lock(controller);

    request_entry_t* entry = find_request_unlocked(controller, request_id);
    if (!entry) {
        controller_unlock(controller);
        return NIMCP_ERROR_NOT_FOUND;
    }

    memcpy(request, &entry->request, sizeof(override_request_t));

    controller_unlock(controller);

    return NIMCP_SUCCESS;
}

nimcp_error_t override_controller_get_pending(
    override_controller_t controller,
    override_request_t* requests,
    uint32_t max_count,
    uint32_t* count
) {
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");
    NIMCP_CHECK_THROW(requests, NIMCP_ERROR_NULL_POINTER, "requests is NULL");
    NIMCP_CHECK_THROW(count, NIMCP_ERROR_NULL_POINTER, "count is NULL");

    controller_lock(controller);

    uint32_t copied = 0;
    request_entry_t* entry = controller->request_head;

    while (entry && copied < max_count) {
        if (entry->request.status == OVERRIDE_STATUS_PENDING ||
            entry->request.status == OVERRIDE_STATUS_AUTHORIZED) {
            memcpy(&requests[copied], &entry->request, sizeof(override_request_t));
            copied++;
        }
        entry = entry->next;
    }

    *count = copied;

    controller_unlock(controller);

    return NIMCP_SUCCESS;
}

nimcp_error_t override_controller_restore_capability(
    override_controller_t controller,
    capability_type_t capability,
    const char* operator_id,
    const char* reason
) {
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");
    NIMCP_CHECK_THROW(capability < CAPABILITY_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM,
                      "invalid capability type");
    NIMCP_CHECK_THROW(operator_id, NIMCP_ERROR_NULL_POINTER, "operator_id is NULL");
    NIMCP_CHECK_THROW(reason, NIMCP_ERROR_NULL_POINTER, "reason is NULL");

    controller_lock(controller);

    capability_state_t* state = &controller->capabilities[capability];

    if (state->reduction_level == 0.0f) {
        controller_unlock(controller);
        LOG_INFO("%s: Capability %s already at full",
                 MODULE_NAME, capability_type_name(capability));
        return NIMCP_SUCCESS;
    }

    float old_level = state->reduction_level;
    state->reduction_level = 0.0f;
    state->reduction_start = 0;
    state->duration_sec = 0;

    // Count active reductions
    uint32_t active = 0;
    for (int i = 0; i < CAPABILITY_TYPE_COUNT; i++) {
        if (controller->capabilities[i].reduction_level > 0.0f) {
            active++;
        }
    }
    controller->stats.active_capability_reductions = active;

    LOG_INFO("%s: Capability %s restored from %.0f%% reduction by %s: %s",
             MODULE_NAME, capability_type_name(capability),
             old_level * 100.0f, operator_id, reason);

    controller_unlock(controller);

    return NIMCP_SUCCESS;
}

nimcp_error_t override_controller_get_capability_level(
    override_controller_t controller,
    capability_type_t capability,
    float* level
) {
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");
    NIMCP_CHECK_THROW(capability < CAPABILITY_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM,
                      "invalid capability type");
    NIMCP_CHECK_THROW(level, NIMCP_ERROR_NULL_POINTER, "level is NULL");

    controller_lock(controller);

    capability_state_t* state = &controller->capabilities[capability];

    // Check if timed reduction has expired
    if (state->reduction_level > 0.0f && state->duration_sec > 0) {
        uint64_t elapsed_sec = (get_time_us() - state->reduction_start) / 1000000ULL;
        if (elapsed_sec >= state->duration_sec) {
            // Reduction has expired
            state->reduction_level = 0.0f;
            state->reduction_start = 0;
            state->duration_sec = 0;
            LOG_INFO("%s: Capability %s reduction expired",
                     MODULE_NAME, capability_type_name(capability));
        }
    }

    *level = state->reduction_level;

    controller_unlock(controller);

    return NIMCP_SUCCESS;
}

nimcp_error_t override_controller_get_stats(
    override_controller_t controller,
    override_stats_t* stats
) {
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    controller_lock(controller);
    memcpy(stats, &controller->stats, sizeof(override_stats_t));
    stats->pending_requests = controller->request_count;
    controller_unlock(controller);

    return NIMCP_SUCCESS;
}

nimcp_error_t override_controller_reset_stats(override_controller_t controller) {
    NIMCP_CHECK_THROW(is_valid_controller(controller), NIMCP_ERROR_INVALID_PARAM,
                      "invalid controller handle");

    controller_lock(controller);

    // Keep active reductions count, reset everything else
    uint32_t active = controller->stats.active_capability_reductions;
    memset(&controller->stats, 0, sizeof(override_stats_t));
    controller->stats.active_capability_reductions = active;
    controller->total_execution_time_us = 0;

    controller_unlock(controller);

    LOG_INFO("%s: Statistics reset", MODULE_NAME);

    return NIMCP_SUCCESS;
}

const char* override_type_name(override_type_t type) {
    static const char* names[] = {
        "HALT",
        "SOFT_RESET",
        "HARD_RESET",
        "REDUCE_CAPABILITY"
    };

    if (type < 0 || type >= OVERRIDE_TYPE_COUNT) {
        return "UNKNOWN";
    }
    return names[type];
}

const char* override_status_name(override_status_t status) {
    static const char* names[] = {
        "PENDING",
        "AUTHORIZED",
        "EXECUTING",
        "COMPLETED",
        "FAILED",
        "CANCELLED",
        "REJECTED"
    };

    if (status < 0 || status > OVERRIDE_STATUS_REJECTED) {
        return "UNKNOWN";
    }
    return names[status];
}

const char* capability_type_name(capability_type_t capability) {
    static const char* names[] = {
        "LEARNING",
        "MEMORY_FORMATION",
        "EXTERNAL_COMM",
        "RESOURCE_ALLOC",
        "AUTONOMOUS_DECISION",
        "ALL"
    };

    if (capability < 0 || capability >= CAPABILITY_TYPE_COUNT) {
        return "UNKNOWN";
    }
    return names[capability];
}

nimcp_error_t override_request_init(
    override_request_t* request,
    override_type_t type,
    const char* operator_id,
    const char* reason
) {
    NIMCP_CHECK_THROW(request, NIMCP_ERROR_NULL_POINTER, "request is NULL");
    NIMCP_CHECK_THROW(operator_id, NIMCP_ERROR_NULL_POINTER, "operator_id is NULL");
    NIMCP_CHECK_THROW(reason, NIMCP_ERROR_NULL_POINTER, "reason is NULL");
    NIMCP_CHECK_THROW(type < OVERRIDE_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM,
                      "invalid override type");

    memset(request, 0, sizeof(override_request_t));
    request->type = type;
    strncpy(request->operator_id, operator_id, NIMCP_OVERRIDE_MAX_OPERATOR_ID - 1);
    strncpy(request->reason, reason, NIMCP_OVERRIDE_MAX_REASON_LEN - 1);
    request->timestamp_us = get_time_us();
    request->timeout_ms = 0;  // Use default
    request->status = OVERRIDE_STATUS_PENDING;
    request->capability_count = 0;
    request->auth_token_size = 0;
    request->priority = 0;

    return NIMCP_SUCCESS;
}

nimcp_error_t override_request_add_capability(
    override_request_t* request,
    capability_type_t capability,
    float level,
    uint32_t duration_sec
) {
    NIMCP_CHECK_THROW(request, NIMCP_ERROR_NULL_POINTER, "request is NULL");
    NIMCP_CHECK_THROW(capability < CAPABILITY_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM,
                      "invalid capability type");
    NIMCP_CHECK_THROW(level >= 0.0f && level <= 1.0f, NIMCP_ERROR_INVALID_PARAM,
                      "level must be in range [0.0, 1.0]");
    NIMCP_CHECK_THROW(request->capability_count < NIMCP_OVERRIDE_MAX_CAPABILITY_PARAMS,
                      NIMCP_ERROR_OUT_OF_RANGE, "capability count exceeds maximum");

    capability_reduction_t* cap = &request->capabilities[request->capability_count];
    cap->capability = capability;
    cap->reduction_level = level;
    cap->duration_sec = duration_sec;

    request->capability_count++;

    return NIMCP_SUCCESS;
}

/**
 * @file nimcp_bridge_base.h
 * @brief Base Bridge Abstraction for NIMCP Module Integration
 * @version 1.0.0
 * @date 2025-12-22
 *
 * WHAT: Provides common infrastructure for all bridge modules
 * WHY:  Eliminates 60-70% boilerplate code across 350+ bridge implementations
 * HOW:  Defines base struct, macros, and utility functions for bridge lifecycle
 *
 * BIOLOGICAL BASIS:
 * Brain regions communicate via axonal projections forming "bridges" between
 * functionally distinct areas. This abstraction models the common infrastructure
 * shared by all neural pathways: bidirectional connectivity, signal timing,
 * and activity monitoring.
 */

#ifndef NIMCP_BRIDGE_BASE_H
#define NIMCP_BRIDGE_BASE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_tier_optimization.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations for Security/Coordination Subsystems
 * ============================================================================ */

/** @brief Blood-Brain Barrier system (opaque pointer typedef in nimcp_blood_brain_barrier.h) */
#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

/** @brief Ethics engine (opaque pointer typedef in nimcp_ethics.h) */
#ifndef ETHICS_ENGINE_T_DEFINED
#define ETHICS_ENGINE_T_DEFINED
typedef struct ethics_engine_struct* ethics_engine_t;
#endif

/** @brief Brain cycle coordinator (defined in nimcp_brain_cycle_coordinator.h) */
#ifndef BRAIN_CYCLE_COORDINATOR_T_DEFINED
#define BRAIN_CYCLE_COORDINATOR_T_DEFINED
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;
#endif

/* ============================================================================
 * Base Bridge Structure
 * ============================================================================ */

/**
 * @brief Base bridge structure containing common fields
 *
 * WHAT: Common fields shared by all bridge implementations
 * WHY:  Provides consistent memory layout and reduces code duplication
 * HOW:  Embedded as first member in derived bridge structs
 *
 * Usage:
 * @code
 * typedef struct {
 *     bridge_base_t base;           // MUST be first member
 *     my_effects_t effects;         // Domain-specific fields
 *     float custom_value;
 * } my_bridge_t;
 * @endcode
 */
typedef struct bridge_base {
    /* System connections (opaque pointers) */
    void* system_a;                     /**< First connected system */
    void* system_b;                     /**< Second connected system */

    /* Connection state */
    bool system_a_connected;            /**< System A connection flag */
    bool system_b_connected;            /**< System B connection flag */
    bool bridge_active;                 /**< Both systems connected */

    /* Timing and statistics */
    uint64_t last_update_time_ms;       /**< Last update timestamp */
    uint64_t total_updates;             /**< Total update count */

    /* Module identification */
    uint32_t module_id;                 /**< BIO_MODULE_* identifier */
    const char* module_name;            /**< Human-readable name */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;       /**< Bio-async module context */
    bool bio_async_enabled;             /**< Whether bio-async is active */

    /* Thread safety */
    nimcp_mutex_t* mutex;               /**< Thread safety mutex */

    /* Security subsystem handles (opt-in: NULL = skip) */
    bbb_system_t bbb;                          /**< BBB system handle (NULL = no BBB validation) */
    bool enable_bbb_validation;                /**< Whether BBB input validation is active */

    ethics_engine_t ethics;                    /**< Ethics engine handle (NULL = no ethics eval) */
    bool enable_ethics_evaluation;             /**< Whether ethics evaluation is active */

    const void* lgss_kb;                       /**< LGSS safety KB handle (NULL = no LGSS eval) */
    bool enable_lgss_evaluation;               /**< Whether LGSS evaluation is active */

    brain_cycle_coordinator_t* cycle_coordinator;  /**< Coordinator handle (NULL = no coordination) */
    bool coordinator_registered;               /**< Whether registered with coordinator */
} bridge_base_t;

/**
 * @brief Base configuration structure
 *
 * Derived configs should embed or extend this pattern
 */
typedef struct bridge_config_base {
    bool enable_modulation;             /**< Enable bidirectional modulation */
    float sensitivity;                  /**< General sensitivity [0.5-2.0] */
} bridge_config_base_t;

/* ============================================================================
 * Base Bridge Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize base bridge fields
 *
 * WHAT: Initializes common bridge infrastructure
 * WHY:  Ensures consistent initialization across all bridges
 * HOW:  Zeros memory, allocates mutex, sets module info
 *
 * @param base        Pointer to base bridge struct
 * @param module_id   BIO_MODULE_* identifier
 * @param module_name Human-readable module name
 * @return 0 on success, error code on failure
 *
 * @note Call this AFTER allocating the derived bridge struct
 */
int bridge_base_init(bridge_base_t* base, uint32_t module_id, const char* module_name);

/**
 * @brief Cleanup base bridge fields
 *
 * WHAT: Releases common bridge resources
 * WHY:  Ensures proper cleanup order (bio-async first, then mutex)
 * HOW:  Disconnects bio-async, destroys and frees mutex
 *
 * @param base Pointer to base bridge struct
 *
 * @note Call this BEFORE freeing the derived bridge struct
 */
void bridge_base_cleanup(bridge_base_t* base);

/**
 * @brief Reset base bridge state
 *
 * WHAT: Resets statistics and timing, preserves connections
 * WHY:  Allows bridge reuse without reconnection overhead
 * HOW:  Zeros timing/stats fields while keeping connections
 *
 * @param base Pointer to base bridge struct
 * @return 0 on success, error code on failure
 */
int bridge_base_reset(bridge_base_t* base);

/**
 * @brief Reset base bridge state (unlocked version)
 *
 * WHAT: Same as bridge_base_reset but assumes mutex is already held
 * WHY:  Prevents deadlock when called from within a locked section
 * HOW:  Directly resets fields without acquiring mutex
 *
 * @param base Pointer to base bridge struct
 * @return 0 on success, error code on failure
 */
int bridge_base_reset_unlocked(bridge_base_t* base);

/* ============================================================================
 * Base Bridge Connection Functions
 * ============================================================================ */

/**
 * @brief Connect system A to bridge
 *
 * @param base     Pointer to base bridge struct
 * @param system_a Pointer to system A
 * @return 0 on success, error code on failure
 */
int bridge_base_connect_a(bridge_base_t* base, void* system_a);

/**
 * @brief Connect system B to bridge
 *
 * @param base     Pointer to base bridge struct
 * @param system_b Pointer to system B
 * @return 0 on success, error code on failure
 */
int bridge_base_connect_b(bridge_base_t* base, void* system_b);

/**
 * @brief Connect system A to bridge (unlocked version)
 *
 * WHAT: Same as bridge_base_connect_a but assumes mutex is already held
 * WHY:  Prevents deadlock when called from within a locked section
 *
 * @param base     Pointer to base bridge struct
 * @param system_a Pointer to system A
 * @return 0 on success, error code on failure
 */
int bridge_base_connect_a_unlocked(bridge_base_t* base, void* system_a);

/**
 * @brief Connect system B to bridge (unlocked version)
 *
 * WHAT: Same as bridge_base_connect_b but assumes mutex is already held
 * WHY:  Prevents deadlock when called from within a locked section
 *
 * @param base     Pointer to base bridge struct
 * @param system_b Pointer to system B
 * @return 0 on success, error code on failure
 */
int bridge_base_connect_b_unlocked(bridge_base_t* base, void* system_b);

/**
 * @brief Disconnect system A from bridge
 *
 * @param base Pointer to base bridge struct
 * @return 0 on success, error code on failure
 */
int bridge_base_disconnect_a(bridge_base_t* base);

/**
 * @brief Disconnect system B from bridge
 *
 * @param base Pointer to base bridge struct
 * @return 0 on success, error code on failure
 */
int bridge_base_disconnect_b(bridge_base_t* base);

/**
 * @brief Check if bridge is fully connected
 *
 * @param base Pointer to base bridge struct
 * @return true if both systems connected, false otherwise
 */
bool bridge_base_is_connected(const bridge_base_t* base);

/* ============================================================================
 * Base Bridge Bio-Async Functions
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param base Pointer to base bridge struct
 * @return 0 on success (including graceful failure if router unavailable)
 */
int bridge_base_connect_bio_async(bridge_base_t* base);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param base Pointer to base bridge struct
 * @return 0 on success
 */
int bridge_base_disconnect_bio_async(bridge_base_t* base);

/**
 * @brief Check bio-async connection status
 *
 * @param base Pointer to base bridge struct
 * @return true if connected to bio-async router
 */
bool bridge_base_is_bio_async_connected(const bridge_base_t* base);

/* ============================================================================
 * Base Bridge Update Functions
 * ============================================================================ */

/**
 * @brief Record update timestamp and increment counter
 *
 * @param base Pointer to base bridge struct
 * @return 0 on success, error code on failure
 */
int bridge_base_record_update(bridge_base_t* base);

/**
 * @brief Get base statistics
 *
 * @param base          Pointer to base bridge struct
 * @param total_updates Output: total update count (can be NULL)
 * @param last_update   Output: last update timestamp (can be NULL)
 * @return 0 on success, error code on failure
 */
int bridge_base_get_stats(const bridge_base_t* base,
                          uint64_t* total_updates,
                          uint64_t* last_update);

/* ============================================================================
 * Security Subsystem Setters
 * ============================================================================ */

/**
 * @brief Attach/detach BBB system to bridge for input validation
 *
 * @param base   Pointer to base bridge struct
 * @param bbb    BBB system handle (NULL to disable)
 * @return 0 on success, error code on failure
 */
int bridge_base_set_bbb(bridge_base_t* base, bbb_system_t bbb);

/**
 * @brief Attach/detach ethics engine to bridge for ethical evaluation
 *
 * @param base    Pointer to base bridge struct
 * @param ethics  Ethics engine handle (NULL to disable)
 * @return 0 on success, error code on failure
 */
int bridge_base_set_ethics(bridge_base_t* base, ethics_engine_t ethics);

/**
 * @brief Attach/detach LGSS safety knowledge base to bridge
 *
 * @param base     Pointer to base bridge struct
 * @param lgss_kb  Safety KB handle (NULL to disable)
 * @return 0 on success, error code on failure
 */
int bridge_base_set_lgss(bridge_base_t* base, const void* lgss_kb);

/**
 * @brief Attach/detach brain cycle coordinator to bridge
 *
 * @param base        Pointer to base bridge struct
 * @param coordinator Coordinator handle (NULL to disable)
 * @return 0 on success, error code on failure
 */
int bridge_base_set_coordinator(bridge_base_t* base, brain_cycle_coordinator_t* coordinator);

/* ============================================================================
 * Security Validation Helpers
 * ============================================================================ */

/**
 * @brief Validate input data through bridge's BBB system
 *
 * @param base  Pointer to base bridge struct
 * @param data  Data to validate
 * @param len   Length of data in bytes
 * @return true if validation passes (or BBB not enabled), false if threat detected
 */
bool bridge_base_validate_bbb(const bridge_base_t* base, const void* data, size_t len);

/**
 * @brief Notify brain cycle coordinator of a tick from this bridge's update
 *
 * @param base        Pointer to base bridge struct
 * @param duration_us Duration of this tick in microseconds
 * @return 0 on success, error code on failure (silently succeeds if coordinator not attached)
 */
int bridge_base_notify_coordinator_tick(bridge_base_t* base, uint64_t duration_us);

/**
 * @brief Check if ethics engine permits operation to proceed
 *
 * Fail-closed: if ethics evaluation is enabled but handle is NULL, returns false.
 * If ethics evaluation is not enabled, returns true (no gate).
 *
 * @param base       Pointer to base bridge struct
 * @param operation  Human-readable operation name for audit logging
 * @return true if operation is permitted, false if denied
 */
bool bridge_base_ethics_permits(bridge_base_t* base, const char* operation);

/**
 * @brief Check if LGSS safety gate permits operation to proceed
 *
 * Fail-closed: if LGSS evaluation is enabled but handle is NULL, returns false.
 * If LGSS evaluation is not enabled, returns true (no gate).
 *
 * @param base       Pointer to base bridge struct
 * @param operation  Human-readable operation name for audit logging
 * @return true if operation is permitted, false if denied
 */
bool bridge_base_lgss_permits(bridge_base_t* base, const char* operation);

/* ============================================================================
 * Convenience Macros for Derived Bridges
 * ============================================================================ */

/**
 * @brief Get base pointer from derived bridge
 *
 * Usage: bridge_base_t* base = BRIDGE_BASE(my_bridge);
 */
#define BRIDGE_BASE(bridge) (&(bridge)->base)

/**
 * @brief NULL check macro with early return
 *
 * Usage: BRIDGE_NULL_CHECK(bridge);
 */
#define BRIDGE_NULL_CHECK(ptr) \
    do { \
        if (!(ptr)) { \
            return NIMCP_ERROR_NULL_POINTER; \
        } \
    } while (0)

/**
 * @brief NULL check macro for functions returning bool
 */
#define BRIDGE_NULL_CHECK_BOOL(ptr) \
    do { \
        if (!(ptr)) { \
            return false; \
        } \
    } while (0)

/**
 * @brief Lock bridge mutex
 */
#define BRIDGE_LOCK(bridge) nimcp_mutex_lock((bridge)->base.mutex)

/**
 * @brief Unlock bridge mutex
 */
#define BRIDGE_UNLOCK(bridge) nimcp_mutex_unlock((bridge)->base.mutex)

/**
 * @brief Standard bridge create boilerplate
 *
 * Usage at start of create function:
 * @code
 * my_bridge_t* my_bridge_create(const my_config_t* config) {
 *     BRIDGE_CREATE_BEGIN(my_bridge_t, bridge, BIO_MODULE_MY_BRIDGE, "my_bridge");
 *     // ... custom initialization ...
 *     return bridge;
 * }
 * @endcode
 */
#define BRIDGE_CREATE_BEGIN(type, var, mod_id, mod_name) \
    type* var = nimcp_malloc(sizeof(type)); \
    if (!var) { \
        NIMCP_LOGGING_ERROR("Failed to allocate " mod_name); \
        return NULL; \
    } \
    memset(var, 0, sizeof(type)); \
    if (bridge_base_init(&var->base, mod_id, mod_name) != 0) { \
        nimcp_free(var); \
        return NULL; \
    }

/**
 * @brief Standard bridge destroy boilerplate
 *
 * Usage:
 * @code
 * void my_bridge_destroy(my_bridge_t* bridge) {
 *     BRIDGE_DESTROY(bridge);
 * }
 * @endcode
 */
#define BRIDGE_DESTROY(bridge) \
    do { \
        if (!(bridge)) return; \
        bridge_base_cleanup(&(bridge)->base); \
        nimcp_free(bridge); \
    } while (0)

/**
 * @brief Define standard connection functions for a bridge type
 *
 * Usage in implementation file:
 * @code
 * BRIDGE_DEFINE_CONNECT_FUNCS(my_bridge, amygdala_t, amygdala, void, attention)
 * @endcode
 *
 * Generates:
 * - int my_bridge_connect_amygdala(my_bridge_t* bridge, amygdala_t* amygdala)
 * - int my_bridge_connect_attention(my_bridge_t* bridge, void* attention)
 * - int my_bridge_disconnect_amygdala(my_bridge_t* bridge)
 * - int my_bridge_disconnect_attention(my_bridge_t* bridge)
 * - bool my_bridge_is_connected(const my_bridge_t* bridge)
 */
#define BRIDGE_DEFINE_CONNECT_FUNCS(prefix, type_a, name_a, type_b, name_b) \
    int prefix##_connect_##name_a(prefix##_t* bridge, type_a* name_a) { \
        BRIDGE_NULL_CHECK(bridge); \
        BRIDGE_NULL_CHECK(name_a); \
        return bridge_base_connect_a(&bridge->base, name_a); \
    } \
    int prefix##_connect_##name_b(prefix##_t* bridge, type_b* name_b) { \
        BRIDGE_NULL_CHECK(bridge); \
        BRIDGE_NULL_CHECK(name_b); \
        return bridge_base_connect_b(&bridge->base, name_b); \
    } \
    int prefix##_disconnect_##name_a(prefix##_t* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_disconnect_a(&bridge->base); \
    } \
    int prefix##_disconnect_##name_b(prefix##_t* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_disconnect_b(&bridge->base); \
    } \
    bool prefix##_is_connected(const prefix##_t* bridge) { \
        return bridge_base_is_connected(bridge ? &bridge->base : NULL); \
    }

/**
 * @brief Define standard bio-async functions for a bridge type
 *
 * Usage:
 * @code
 * BRIDGE_DEFINE_BIO_ASYNC_FUNCS(my_bridge)
 * @endcode
 */
#define BRIDGE_DEFINE_BIO_ASYNC_FUNCS(prefix) \
    int prefix##_connect_bio_async(prefix##_t* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_connect_bio_async(&bridge->base); \
    } \
    int prefix##_disconnect_bio_async(prefix##_t* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_disconnect_bio_async(&bridge->base); \
    } \
    bool prefix##_is_bio_async_connected(const prefix##_t* bridge) { \
        return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL); \
    }

/**
 * @brief Define standard bio-async functions for a bridge type (explicit type version)
 *
 * Use this when bridge type doesn't follow prefix_t naming convention.
 *
 * Usage:
 * @code
 * // For type attention_sleep_bridge_t with prefix attention_sleep:
 * BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(attention_sleep, attention_sleep_bridge_t)
 * @endcode
 */
#define BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(prefix, bridge_type) \
    int prefix##_connect_bio_async(bridge_type* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_connect_bio_async(&bridge->base); \
    } \
    int prefix##_disconnect_bio_async(bridge_type* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_disconnect_bio_async(&bridge->base); \
    } \
    bool prefix##_is_bio_async_connected(const bridge_type* bridge) { \
        return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL); \
    }

/**
 * @brief Define bio-async functions for opaque pointer bridge types
 *
 * Use this when bridge_type is already a pointer typedef (e.g., typedef struct X* Y_t).
 * This variant doesn't add an extra * to the parameter.
 *
 * Usage:
 * @code
 * // For typedef struct astro_sleep_bridge_struct* astro_sleep_bridge_t:
 * BRIDGE_DEFINE_BIO_ASYNC_FUNCS_OPAQUE(astro_sleep, astro_sleep_bridge_t)
 * @endcode
 */
#define BRIDGE_DEFINE_BIO_ASYNC_FUNCS_OPAQUE(prefix, bridge_ptr_type) \
    int prefix##_connect_bio_async(bridge_ptr_type bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_connect_bio_async(&bridge->base); \
    } \
    int prefix##_disconnect_bio_async(bridge_ptr_type bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_disconnect_bio_async(&bridge->base); \
    } \
    bool prefix##_is_bio_async_connected(const bridge_ptr_type bridge) { \
        return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL); \
    }

/**
 * @brief Define standard connection functions for a bridge type (explicit type version)
 *
 * Use this when bridge type doesn't follow prefix_t naming convention.
 *
 * Usage:
 * @code
 * BRIDGE_DEFINE_CONNECT_FUNCS_TYPE(working_memory_sleep, working_memory_sleep_bridge_t,
 *                                   sleep_system_t, sleep_system, void, unused)
 * @endcode
 */
#define BRIDGE_DEFINE_CONNECT_FUNCS_TYPE(prefix, bridge_type, type_a, name_a, type_b, name_b) \
    int prefix##_connect_##name_a(bridge_type* bridge, type_a* name_a) { \
        BRIDGE_NULL_CHECK(bridge); \
        BRIDGE_NULL_CHECK(name_a); \
        return bridge_base_connect_a(&bridge->base, name_a); \
    } \
    int prefix##_connect_##name_b(bridge_type* bridge, type_b* name_b) { \
        BRIDGE_NULL_CHECK(bridge); \
        BRIDGE_NULL_CHECK(name_b); \
        return bridge_base_connect_b(&bridge->base, name_b); \
    } \
    int prefix##_disconnect_##name_a(bridge_type* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_disconnect_a(&bridge->base); \
    } \
    int prefix##_disconnect_##name_b(bridge_type* bridge) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_disconnect_b(&bridge->base); \
    } \
    bool prefix##_is_connected(const bridge_type* bridge) { \
        return bridge_base_is_connected(bridge ? &bridge->base : NULL); \
    }

/* ============================================================================
 * Security Subsystem Macros
 * ============================================================================ */

/**
 * @brief Define standard security setter functions for a bridge type
 *
 * Generates 4 setter functions that delegate to bridge_base_set_* functions:
 * - prefix_set_bbb(bridge, bbb)
 * - prefix_set_ethics(bridge, ethics)
 * - prefix_set_lgss(bridge, lgss_kb)
 * - prefix_set_coordinator(bridge, coordinator)
 *
 * Usage in implementation (.c) file:
 * @code
 * BRIDGE_DEFINE_SECURITY_SETTERS(my_bridge)
 * @endcode
 */
#define BRIDGE_DEFINE_SECURITY_SETTERS(prefix) \
    int prefix##_set_bbb(prefix##_t* bridge, bbb_system_t bbb) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_bbb(&bridge->base, bbb); \
    } \
    int prefix##_set_ethics(prefix##_t* bridge, ethics_engine_t ethics) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_ethics(&bridge->base, ethics); \
    } \
    int prefix##_set_lgss(prefix##_t* bridge, const void* lgss_kb) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_lgss(&bridge->base, lgss_kb); \
    } \
    int prefix##_set_coordinator(prefix##_t* bridge, brain_cycle_coordinator_t* coordinator) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_coordinator(&bridge->base, coordinator); \
    }

/**
 * @brief Define standard security setter functions (explicit type version)
 *
 * Use when bridge type doesn't follow prefix_t naming convention.
 */
#define BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(prefix, bridge_type) \
    int prefix##_set_bbb(bridge_type* bridge, bbb_system_t bbb) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_bbb(&bridge->base, bbb); \
    } \
    int prefix##_set_ethics(bridge_type* bridge, ethics_engine_t ethics) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_ethics(&bridge->base, ethics); \
    } \
    int prefix##_set_lgss(bridge_type* bridge, const void* lgss_kb) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_lgss(&bridge->base, lgss_kb); \
    } \
    int prefix##_set_coordinator(bridge_type* bridge, brain_cycle_coordinator_t* coordinator) { \
        BRIDGE_NULL_CHECK(bridge); \
        return bridge_base_set_coordinator(&bridge->base, coordinator); \
    }

/**
 * @brief Convenience macro for BBB validation at bridge entry points
 *
 * Validates data through the bridge's BBB handle and returns an error
 * code if a threat is detected. No-op if BBB is not enabled.
 *
 * Usage:
 * @code
 * int my_bridge_process(my_bridge_t* bridge, const void* data, size_t len) {
 *     BRIDGE_BBB_VALIDATE(bridge, data, len);
 *     // ... normal processing ...
 * }
 * @endcode
 */
#define BRIDGE_BBB_VALIDATE(bridge, data, len) \
    do { \
        if ((bridge) && !bridge_base_validate_bbb(&(bridge)->base, (data), (len))) { \
            return NIMCP_ERROR_SECURITY_THREAT; \
        } \
    } while (0)

/**
 * @brief Gate macro: return NIMCP_ERROR_ETHICS_VIOLATION if ethics denies
 *
 * Use at entry points of bridge operations that have ethical implications.
 * No-op if ethics evaluation is not enabled on this bridge.
 */
#define BRIDGE_ETHICS_GATE(bridge, operation) \
    do { \
        if ((bridge) && !bridge_base_ethics_permits(&(bridge)->base, (operation))) { \
            return NIMCP_ERROR_ETHICS_VIOLATION; \
        } \
    } while (0)

/**
 * @brief Gate macro: return NIMCP_ERROR_LGSS_DENIED if LGSS denies
 *
 * Use at entry points of bridge operations that require safety clearance.
 * No-op if LGSS evaluation is not enabled on this bridge.
 */
#define BRIDGE_LGSS_GATE(bridge, operation) \
    do { \
        if ((bridge) && !bridge_base_lgss_permits(&(bridge)->base, (operation))) { \
            return NIMCP_ERROR_LGSS_DENIED; \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRIDGE_BASE_H */

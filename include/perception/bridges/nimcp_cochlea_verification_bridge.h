/**
 * @file nimcp_cochlea_verification_bridge.h
 * @brief Cochlea bidirectional verification system
 *
 * WHAT: Comprehensive verification of all cochlear bridge connections
 * WHY:  Ensure reliable bidirectional data flow across all integrations
 * HOW:  Centralized verification coordinator with per-bridge checks
 *
 * VERIFICATION TYPES:
 * - Ping-pong: Round-trip message verification
 * - Latency: Timing measurement for each path
 * - Data integrity: Payload verification
 * - Timestamp freshness: Activity monitoring
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_VERIFICATION_BRIDGE_H
#define NIMCP_COCHLEA_VERIFICATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_VERIFY_MAX_BRIDGES      32
#define COCHLEA_VERIFY_TIMEOUT_MS       5000
#define COCHLEA_VERIFY_FRESHNESS_MS     5000

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Bridge type identifier
 */
typedef enum {
    COCHLEA_BRIDGE_MEDULLA,
    COCHLEA_BRIDGE_THALAMIC,
    COCHLEA_BRIDGE_AUDIO_CORTEX,
    COCHLEA_BRIDGE_FEP,
    COCHLEA_BRIDGE_SLEEP,
    COCHLEA_BRIDGE_IMMUNE,
    COCHLEA_BRIDGE_KG,
    COCHLEA_BRIDGE_SUBSTRATE,
    COCHLEA_BRIDGE_RCOG,
    COCHLEA_BRIDGE_COLLECTIVE,
    COCHLEA_BRIDGE_CORTICAL_DEEP,
    COCHLEA_BRIDGE_OCCIPITAL,
    COCHLEA_BRIDGE_BROCA,
    COCHLEA_BRIDGE_BIO_ASYNC,
    COCHLEA_BRIDGE_COUNT
} cochlea_bridge_type_t;

/**
 * @brief Verification status
 */
typedef enum {
    VERIFY_STATUS_UNKNOWN,
    VERIFY_STATUS_PENDING,
    VERIFY_STATUS_PASSED,
    VERIFY_STATUS_FAILED,
    VERIFY_STATUS_TIMEOUT,
    VERIFY_STATUS_STALE
} verify_status_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Per-bridge verification state
 */
typedef struct {
    cochlea_bridge_type_t type;       /**< Bridge type */
    const char* name;                 /**< Bridge name */
    void* bridge_ptr;                 /**< Bridge pointer */

    /* Verification function pointers */
    bool (*verify_fn)(const void*);   /**< Bidirectional verify function */
    uint64_t (*get_outbound_fn)(const void*); /**< Get last outbound */
    uint64_t (*get_inbound_fn)(const void*);  /**< Get last inbound */

    /* Status */
    verify_status_t status;           /**< Current verification status */
    uint64_t last_verified_ms;        /**< Last verification time */
    uint64_t last_outbound_ms;        /**< Last outbound timestamp */
    uint64_t last_inbound_ms;         /**< Last inbound timestamp */
    float latency_ms;                 /**< Measured latency */

    /* Statistics */
    uint32_t verify_passes;           /**< Total passes */
    uint32_t verify_fails;            /**< Total failures */
    uint32_t consecutive_fails;       /**< Consecutive failures */

    bool registered;                  /**< Bridge is registered */
} cochlea_bridge_verify_state_t;

/**
 * @brief Overall verification result
 */
typedef struct {
    uint32_t total_bridges;           /**< Total registered bridges */
    uint32_t passed_bridges;          /**< Bridges that passed */
    uint32_t failed_bridges;          /**< Bridges that failed */
    uint32_t stale_bridges;           /**< Bridges with stale data */

    float overall_health;             /**< Overall health 0-1 */
    float avg_latency_ms;             /**< Average latency */
    float max_latency_ms;             /**< Maximum latency */

    bool all_bidirectional;           /**< All bridges bidirectional */
    uint64_t verification_time_ms;    /**< Time of verification */
} cochlea_verify_result_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Verification settings */
    float verification_interval_ms;   /**< Auto-verify interval */
    float freshness_threshold_ms;     /**< Staleness threshold */
    float latency_warning_ms;         /**< Latency warning threshold */

    /* Failure handling */
    uint32_t max_consecutive_fails;   /**< Max fails before alert */
    bool auto_reset_on_fail;          /**< Auto-reset failed bridges */

    /* Logging */
    bool log_verifications;           /**< Log verification results */
    bool log_failures_only;           /**< Only log failures */
} cochlea_verification_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_verification_bridge cochlea_verification_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_verification_config_t cochlea_verification_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_verification_bridge_t* cochlea_verification_bridge_create(
    cochlea_t* cochlea,
    const cochlea_verification_config_t* config
);

void cochlea_verification_bridge_destroy(cochlea_verification_bridge_t* bridge);

nimcp_error_t cochlea_verification_bridge_update(
    cochlea_verification_bridge_t* bridge,
    float dt_ms
);

nimcp_error_t cochlea_verification_bridge_reset(cochlea_verification_bridge_t* bridge);

//=============================================================================
// Bridge Registration
//=============================================================================

/**
 * @brief Register a bridge for verification
 */
nimcp_error_t cochlea_verification_register(
    cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type,
    void* bridge_ptr,
    bool (*verify_fn)(const void*),
    uint64_t (*get_outbound_fn)(const void*),
    uint64_t (*get_inbound_fn)(const void*)
);

/**
 * @brief Unregister a bridge
 */
nimcp_error_t cochlea_verification_unregister(
    cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
);

/**
 * @brief Check if bridge is registered
 */
bool cochlea_verification_is_registered(
    const cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
);

//=============================================================================
// Verification
//=============================================================================

/**
 * @brief Verify single bridge
 */
nimcp_error_t cochlea_verification_verify_bridge(
    cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
);

/**
 * @brief Verify all registered bridges
 */
nimcp_error_t cochlea_verification_verify_all(
    cochlea_verification_bridge_t* verifier
);

/**
 * @brief Get verification result for single bridge
 */
nimcp_error_t cochlea_verification_get_bridge_status(
    const cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type,
    cochlea_bridge_verify_state_t* state
);

/**
 * @brief Get overall verification result
 */
nimcp_error_t cochlea_verification_get_result(
    const cochlea_verification_bridge_t* verifier,
    cochlea_verify_result_t* result
);

//=============================================================================
// Status Queries
//=============================================================================

/**
 * @brief Check if all bridges are bidirectional
 */
bool cochlea_verification_all_bidirectional(
    const cochlea_verification_bridge_t* verifier
);

/**
 * @brief Get number of passing bridges
 */
uint32_t cochlea_verification_get_passing_count(
    const cochlea_verification_bridge_t* verifier
);

/**
 * @brief Get number of failing bridges
 */
uint32_t cochlea_verification_get_failing_count(
    const cochlea_verification_bridge_t* verifier
);

/**
 * @brief Get overall health score
 */
float cochlea_verification_get_health(
    const cochlea_verification_bridge_t* verifier
);

//=============================================================================
// Latency
//=============================================================================

/**
 * @brief Get latency for specific bridge
 */
float cochlea_verification_get_latency(
    const cochlea_verification_bridge_t* verifier,
    cochlea_bridge_type_t type
);

/**
 * @brief Get average latency across all bridges
 */
float cochlea_verification_get_avg_latency(
    const cochlea_verification_bridge_t* verifier
);

//=============================================================================
// Bridge name utility
//=============================================================================

/**
 * @brief Get bridge name from type
 */
const char* cochlea_verification_get_bridge_name(cochlea_bridge_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_VERIFICATION_BRIDGE_H */

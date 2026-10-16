/**
 * @file nimcp_rate_limiter_fep_bridge.h
 * @brief Free Energy Principle bridge for Rate Limiter
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: FEP integration for rate limiting - homeostatic regulation
 * WHY:  Rate limiting maintains system homeostasis under FEP
 * HOW:  Map rate violations to prediction errors, adapt limits via precision
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * RATE LIMITING AS HOMEOSTATIC REGULATION:
 * - Expected request rate = generative model predictions
 * - Rate violations = high prediction error (surprising behavior)
 * - Token bucket = precision-weighted capacity
 * - Adaptive limits = precision updates based on violations
 *
 * FEP INTEGRATION:
 * ```
 * Request Rate (o) → Observation
 *         ↓
 * Expected Rate μ (learned baseline)
 *         ↓
 * Prediction Error: ε = o - μ
 *         ↓
 * Precision-Weighted: Π * ε
 *         ↓
 * Action Selection:
 *   - Low ε → ALLOW (refill tokens)
 *   - High ε → DENY (rate exceeded)
 *   - Very high ε → BLOCK (penalty)
 * ```
 *
 * HOMEOSTATIC MAPPING:
 * - Normal request patterns = low free energy
 * - Burst attacks = high surprise (prediction error)
 * - Adaptive rate limits = precision modulation
 * - Penalty escalation = active inference (minimize future violations)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           RATE LIMITER - FEP BRIDGE (Homeostatic Regulation)             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌──────────────────┐         ┌──────────────────┐                      ║
 * ║   │  FEP System      │────────▶│  Rate Limiter    │                      ║
 * ║   │                  │         │                  │                      ║
 * ║   │ • Free Energy    │         │ • Token Bucket   │                      ║
 * ║   │ • Prediction Err │         │ • Penalties      │                      ║
 * ║   │ • Precision      │         │ • Violations     │                      ║
 * ║   └──────────────────┘         └──────────────────┘                      ║
 * ║           ↓                              ↓                                ║
 * ║   ┌──────────────────────────────────────────────────────────────┐       ║
 * ║   │              BIDIRECTIONAL EFFECTS                           │       ║
 * ║   │                                                              │       ║
 * ║   │  FEP → Rate Limiter:                                         │       ║
 * ║   │    • Free energy → Violation severity                        │       ║
 * ║   │    • Prediction error → Burst score                          │       ║
 * ║   │    • Precision → Rate limit strictness                       │       ║
 * ║   │                                                              │       ║
 * ║   │  Rate Limiter → FEP:                                         │       ║
 * ║   │    • Violations → High-surprise observations                 │       ║
 * ║   │    • Normal requests → Update expected rate                  │       ║
 * ║   │    • Penalties → Increase precision                          │       ║
 * ║   └──────────────────────────────────────────────────────────────┘       ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RATE_LIMITER_FEP_BRIDGE_H
#define NIMCP_RATE_LIMITER_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "security/nimcp_rate_limiter.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Free energy thresholds for rate violations */
#define RATE_FEP_NORMAL_THRESHOLD      2.0f
#define RATE_FEP_VIOLATION_THRESHOLD   5.0f
#define RATE_FEP_BURST_THRESHOLD       10.0f
#define RATE_FEP_ATTACK_THRESHOLD      20.0f

/** Precision modulation bounds */
#define RATE_FEP_MIN_PRECISION         0.1f
#define RATE_FEP_MAX_PRECISION         10.0f
#define RATE_FEP_DEFAULT_PRECISION     1.0f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Rate limiter FEP configuration
 */
typedef struct {
    /** FEP parameters */
    float violation_fe_threshold;        /**< FE threshold for violations */
    float burst_fe_threshold;            /**< FE threshold for bursts */
    float precision_learning_rate;       /**< Precision adaptation rate */

    /** Rate adaptation */
    bool enable_adaptive_limits;         /**< Adapt rate limits via FEP */
    bool enable_precision_modulation;    /**< Modulate strictness */
    float min_rate_multiplier;           /**< Minimum rate multiplier (0.5 = 50%) */
    float max_rate_multiplier;           /**< Maximum rate multiplier (2.0 = 200%) */

    /** Learning */
    bool enable_online_learning;         /**< Update FEP from requests */
    float learning_rate;                 /**< Belief update rate */
    bool learn_from_violations;          /**< Update on violations */
} rate_fep_config_t;

/**
 * @brief FEP effects on rate limiter (FEP → Rate)
 */
typedef struct {
    float violation_score;               /**< Violation score from FE */
    float burst_score;                   /**< Burst score from PE */
    float rate_strictness;               /**< Precision-based strictness */
    float adaptive_rate_multiplier;      /**< Dynamic rate adjustment */
} rate_fep_effects_t;

/**
 * @brief Rate limiter effects on FEP (Rate → FEP)
 */
typedef struct {
    uint64_t total_requests;             /**< Total requests processed */
    uint64_t violations;                 /**< Rate violations */
    uint64_t penalties_applied;          /**< Penalties applied */
    float avg_request_rate;              /**< Average request rate */
} fep_rate_effects_t;

/**
 * @brief Rate limiter FEP state
 */
typedef struct {
    bool active;                         /**< Whether bridge is active */
    uint64_t update_count;               /**< Number of updates */
    uint64_t request_count;              /**< Requests processed */
    float current_precision;             /**< Current precision level */
    float current_rate_multiplier;       /**< Current rate multiplier */
} rate_fep_state_t;

/**
 * @brief Rate limiter FEP statistics
 */
typedef struct {
    uint64_t total_requests_processed;   /**< Total requests */
    uint64_t fep_based_decisions;        /**< Decisions using FEP */
    uint64_t violations_detected;        /**< Violations found */
    uint64_t precision_adaptations;      /**< Precision updates */
    float avg_free_energy;               /**< Average free energy */
    float avg_prediction_error;          /**< Average PE */
    float current_precision;             /**< Current precision */
} rate_fep_stats_t;

/**
 * @brief Rate limiter FEP bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    rate_fep_config_t config;            /**< Configuration */
    fep_system_t* fep_system;            /**< FEP system */
    nimcp_rate_limiter_t limiter;        /**< Rate limiter */

    rate_fep_effects_t fep_effects;      /**< FEP → Rate effects */
    fep_rate_effects_t rate_effects;     /**< Rate → FEP effects */

    rate_fep_state_t state;              /**< Current state */
    rate_fep_stats_t stats;              /**< Statistics */} rate_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default rate limiter FEP configuration
 *
 * WHAT: Provide sensible defaults for rate-FEP integration
 * WHY:  Simplify initialization
 * HOW:  Return biologically-plausible defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int rate_fep_default_config(rate_fep_config_t* config);

/**
 * @brief Create rate limiter FEP bridge
 *
 * WHAT: Initialize FEP integration for rate limiting
 * WHY:  Enable homeostatic rate regulation
 * HOW:  Connect FEP system to limiter, allocate structures
 *
 * @param config Configuration
 * @param limiter Rate limiter handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
rate_fep_bridge_t* rate_fep_create(
    const rate_fep_config_t* config,
    nimcp_rate_limiter_t limiter,
    fep_system_t* fep_system
);

/**
 * @brief Destroy rate limiter FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void rate_fep_destroy(rate_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update FEP effects on rate limiter
 *
 * WHAT: Compute FEP-derived violation scores
 * WHY:  Use free energy for adaptive rate limiting
 * HOW:  Process current FEP state, update effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int rate_fep_update(rate_fep_bridge_t* bridge);

/**
 * @brief Check request allowance with FEP enhancement
 *
 * WHAT: Determine if request should be allowed using FEP
 * WHY:  Combine token bucket with prediction-based limiting
 * HOW:  Check both limiter and FEP violation scores
 *
 * @param bridge Bridge handle
 * @param client_id Client identifier
 * @param allowed Output: whether request is allowed
 * @return 0 on success, error code on failure
 */
int rate_fep_check_request(
    rate_fep_bridge_t* bridge,
    const char* client_id,
    bool* allowed
);

/**
 * @brief Apply FEP modulation to rate limiter
 *
 * WHAT: Adjust rate limits based on FEP state
 * WHY:  Adapt to changing request patterns
 * HOW:  Modulate precision, update rate multipliers
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int rate_fep_apply_modulation(rate_fep_bridge_t* bridge);

/**
 * @brief Report violation to FEP
 *
 * WHAT: Feed rate violation back to FEP
 * WHY:  High-surprise observation updates beliefs
 * HOW:  Convert violation to FEP observation, process
 *
 * @param bridge Bridge handle
 * @param client_id Client identifier
 * @param violation_count Number of violations
 * @return 0 on success, error code on failure
 */
int rate_fep_report_violation(
    rate_fep_bridge_t* bridge,
    const char* client_id,
    uint32_t violation_count
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on rate limiter
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int rate_fep_get_effects(
    const rate_fep_bridge_t* bridge,
    rate_fep_effects_t* effects
);

/**
 * @brief Get rate limiter effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, error code on failure
 */
int rate_fep_get_rate_effects(
    const rate_fep_bridge_t* bridge,
    fep_rate_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rate_fep_get_stats(
    const rate_fep_bridge_t* bridge,
    rate_fep_stats_t* stats
);

/**
 * @brief Get current violation score
 *
 * @param bridge Bridge handle
 * @return Current violation score [0, 1] or -1 on error
 */
float rate_fep_get_violation_score(const rate_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module rate violation notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int rate_fep_connect_bio_async(rate_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int rate_fep_disconnect_bio_async(rate_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool rate_fep_is_bio_async_connected(const rate_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RATE_LIMITER_FEP_BRIDGE_H */

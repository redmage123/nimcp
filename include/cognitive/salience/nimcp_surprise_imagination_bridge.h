/**
 * @file nimcp_surprise_imagination_bridge.h
 * @brief Bridge between Surprise Amplifier and imagination/counterfactual system
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: High surprise triggers counterfactual imagination scenarios
 * WHY:  Surprising events should trigger "what if" reasoning to update expectations;
 *       imagined outcomes adjust surprise predictions
 * HOW:  Surprise → imagination trigger; imagination results → expectation update
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SURPRISE → IMAGINATION:
 * - Events exceeding threshold trigger counterfactual scenario generation
 * - Cooldown prevents imagination overload from sustained surprise
 * - Multiple concurrent scenarios explore alternative outcomes
 * - Reference: Schacter et al. (2012) "Future thinking and the brain"
 *
 * IMAGINATION → SURPRISE:
 * - Imagined outcomes adjust surprise expectations (predictive coding)
 * - Divergence between imagined and actual outcomes updates predictions
 * - Successful predictions reduce future surprise for similar events
 *
 * ERROR CODE RANGE: 28900-28999 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_IMAGINATION_BRIDGE_H
#define NIMCP_SURPRISE_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct surprise_amplifier;
struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 28900-28999)
 * ============================================================================ */

#define NIMCP_SURPRISE_IMAGINATION_ERROR_BASE           28900
#define NIMCP_SURPRISE_IMAGINATION_ERROR_NULL_POINTER   (NIMCP_SURPRISE_IMAGINATION_ERROR_BASE + 1)
#define NIMCP_SURPRISE_IMAGINATION_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_IMAGINATION_ERROR_BASE + 2)
#define NIMCP_SURPRISE_IMAGINATION_ERROR_NO_MEMORY      (NIMCP_SURPRISE_IMAGINATION_ERROR_BASE + 3)
#define NIMCP_SURPRISE_IMAGINATION_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_IMAGINATION_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_IMAGINATION_DEFAULT_TRIGGER_THRESHOLD   0.7f
#define SURPRISE_IMAGINATION_DEFAULT_COOLDOWN_SECONDS    5.0f
#define SURPRISE_IMAGINATION_DEFAULT_MAX_SCENARIOS       4
#define SURPRISE_IMAGINATION_DEFAULT_EXPECT_UPDATE_RATE  0.1f
#define SURPRISE_IMAGINATION_DEFAULT_CF_DEPTH            3

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** @brief Scenario status */
typedef enum {
    SURPRISE_IMAGINATION_STATUS_PENDING = 0,
    SURPRISE_IMAGINATION_STATUS_ACTIVE,
    SURPRISE_IMAGINATION_STATUS_COMPLETED,
    SURPRISE_IMAGINATION_STATUS_CANCELLED
} surprise_imagination_status_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief An imagination scenario
 */
typedef struct {
    uint32_t scenario_id;               /**< Unique scenario identifier */
    float trigger_magnitude;            /**< Surprise level that triggered this */
    uint32_t trigger_source;            /**< Source module that triggered */
    surprise_imagination_status_t status; /**< Current scenario status */
    float expected_outcome;             /**< Expected outcome prediction */
    float actual_outcome;               /**< Actual outcome (when available) */
    float divergence;                   /**< Expected vs actual divergence */
    uint64_t timestamp_ms;              /**< Scenario creation timestamp */
} surprise_imagination_scenario_t;

/**
 * @brief Configuration for surprise-imagination bridge
 */
typedef struct {
    float trigger_threshold;            /**< Surprise level to trigger imagination [0.7] */
    float cooldown_seconds;             /**< Minimum between triggers [5.0] */
    uint32_t max_scenarios;             /**< Max concurrent scenarios [4] */
    float expectation_update_rate;      /**< How fast expectations update [0.1] */
    uint32_t counterfactual_depth;      /**< Steps of counterfactual chain [3] */
    bool enable_bio_async;              /**< Bio-async messaging [true] */
    bool enable_logging;                /**< Diagnostic logging [true] */
} surprise_imagination_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    uint32_t scenarios_active;          /**< Number of active scenarios */
    float expectation_adjustment;       /**< Current expectation adjustment */
    float last_trigger_magnitude;       /**< Last trigger surprise level */
    float cooldown_remaining;           /**< Remaining cooldown time */
    bool imagination_connected;         /**< Whether imagination engine is connected */
} surprise_imagination_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t triggers;                  /**< Imagination triggers */
    uint64_t scenarios_completed;       /**< Scenarios completed */
    uint64_t expectations_updated;      /**< Expectation updates */
    uint64_t cooldown_blocked;          /**< Triggers blocked by cooldown */
    uint64_t total_updates;             /**< Total update cycles */
} surprise_imagination_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_imagination_bridge surprise_imagination_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_imagination_config_t surprise_imagination_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_imagination_bridge_t* surprise_imagination_bridge_create(
    const surprise_imagination_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_imagination_bridge_destroy(surprise_imagination_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_imagination_bridge_reset(surprise_imagination_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_imagination_bridge_connect_amplifier(
    surprise_imagination_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to imagination engine */
int surprise_imagination_bridge_connect_imagination_engine(
    surprise_imagination_bridge_t* bridge,
    void* engine);

/** @brief Connect to bio-async router */
int surprise_imagination_bridge_connect_bio_async(
    surprise_imagination_bridge_t* bridge,
    void* router);

/** @brief Disconnect from bio-async router */
int surprise_imagination_bridge_disconnect_bio_async(
    surprise_imagination_bridge_t* bridge);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Check if surprise level should trigger imagination
 * @param bridge Bridge handle
 * @param surprise_level Current surprise level [0-1]
 * @param source_module Source of surprise
 * @return 0 on success (trigger or not), error code otherwise
 */
int surprise_imagination_check_trigger(
    surprise_imagination_bridge_t* bridge,
    float surprise_level,
    uint32_t source_module);

/**
 * @brief Process imagination result
 * @param bridge Bridge handle
 * @param scenario_id Completed scenario ID
 * @param actual_outcome Actual outcome to compare
 * @return 0 on success, error code otherwise
 */
int surprise_imagination_on_result(
    surprise_imagination_bridge_t* bridge,
    uint32_t scenario_id,
    float actual_outcome);

/** @brief Periodic update */
int surprise_imagination_bridge_update(
    surprise_imagination_bridge_t* bridge,
    float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_imagination_bridge_get_effects(
    const surprise_imagination_bridge_t* bridge,
    surprise_imagination_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_imagination_bridge_get_stats(
    const surprise_imagination_bridge_t* bridge,
    surprise_imagination_stats_t* stats_out);

/** @brief Get a specific scenario by ID */
int surprise_imagination_get_scenario(
    const surprise_imagination_bridge_t* bridge,
    uint32_t scenario_id,
    surprise_imagination_scenario_t* scenario_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_imagination_bridge_set_health_agent(
    surprise_imagination_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_IMAGINATION_BRIDGE_H */

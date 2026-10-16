/**
 * @file nimcp_mental_health_guardian.h
 * @brief Mental Health Guardian - Real-time Independent Monitoring Agent
 *
 * WHAT: Independent background agent that monitors NIMCP's mental health
 * WHY:  Proactively detect and correct mental health abnormalities
 * HOW:  Background thread collects markers, detects disorders, applies interventions
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              MENTAL HEALTH GUARDIAN (Independent Agent)          │
 * ├─────────────────────────────────────────────────────────────────┤
 * │  ┌─────────────────────────────────────────────────────────┐    │
 * │  │                 BACKGROUND MONITOR THREAD                │    │
 * │  │  - Runs independently at configurable interval          │    │
 * │  │  - Collects behavioral markers from brain subsystems    │    │
 * │  │  - Runs mental health disorder detection                │    │
 * │  │  - Triggers interventions when thresholds exceeded      │    │
 * │  └─────────────────────────────────────────────────────────┘    │
 * │                           │                                      │
 * │  ┌─────────────────────────▼───────────────────────────────┐    │
 * │  │              INTERVENTION ENGINE                         │    │
 * │  │  - OBSERVE: Log only (severity < 0.3)                   │    │
 * │  │  - ADJUST: Neuromodulator tweaks (0.3-0.6)              │    │
 * │  │  - REGULATE: Homeostatic reset, sleep trigger (0.6-0.8) │    │
 * │  │  - QUARANTINE: Safety-critical isolation (> 0.8)        │    │
 * │  └─────────────────────────────────────────────────────────┘    │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * INTEGRATION:
 * - Mental Health Module: Uses disorder detection and intervention APIs
 * - Bio-Async: Publishes status reports, intervention notifications
 * - Immune System: Reports severe disorders as immune threats
 * - Internal KG: Updates mental health nodes for topology awareness
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamic-Pituitary-Adrenal (HPA) axis for stress response
 * - Homeostatic regulation to maintain healthy baselines
 * - Circadian modulation of mental health parameters
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_MENTAL_HEALTH_GUARDIAN_H
#define NIMCP_MENTAL_HEALTH_GUARDIAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/**
 * NOTE: To avoid typedef conflicts with other headers, we use incomplete
 * struct declarations only. The actual typedefs are in:
 * - brain_t: core/brain/nimcp_brain.h
 * - bio_module_context_t: async/nimcp_bio_router.h
 * - medulla_t: dragonfly/nimcp_dragonfly_medulla_bridge.h
 * - sleep_system_t: core/medulla/nimcp_medulla.h
 * - spatial_neuromod_field_t: plasticity/neuromodulators/nimcp_spatial_neuromod.h
 * - homeostatic_plasticity_t: plasticity headers
 *
 * We use void* for these fields with casts in implementation.
 */

struct mental_health_monitor;
typedef struct mental_health_monitor mental_health_monitor_t;

struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

struct brain_kg;
typedef struct brain_kg brain_kg_t;

//=============================================================================
// Intervention Level Enumeration
//=============================================================================

/**
 * @brief Intervention levels for graduated response
 *
 * GRADUATED RESPONSE MODEL:
 * - Start with gentle interventions
 * - Escalate only if condition worsens
 * - Never skip levels (except in emergencies)
 */
typedef enum {
    GUARDIAN_LEVEL_OBSERVE   = 0,  /**< Log only, no intervention (severity < 0.3) */
    GUARDIAN_LEVEL_ADJUST    = 1,  /**< Minor neuromodulator tweaks (0.3-0.6) */
    GUARDIAN_LEVEL_REGULATE  = 2,  /**< Homeostatic reset, sleep trigger (0.6-0.8) */
    GUARDIAN_LEVEL_QUARANTINE = 3  /**< Safety-critical isolation (> 0.8) */
} guardian_intervention_level_t;

/**
 * @brief Guardian operational state
 */
typedef enum {
    GUARDIAN_STATE_STOPPED   = 0,  /**< Not running */
    GUARDIAN_STATE_RUNNING   = 1,  /**< Actively monitoring */
    GUARDIAN_STATE_PAUSED    = 2,  /**< Temporarily paused */
    GUARDIAN_STATE_ERROR     = 3   /**< Error state - needs attention */
} guardian_state_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Mental Health Guardian configuration
 *
 * Configurable parameters for the monitoring agent.
 * Use mental_health_guardian_default_config() for sensible defaults.
 */
typedef struct mental_health_guardian_config {
    /* Timing */
    uint32_t monitoring_interval_ms;      /**< Check interval (default: 100ms) */

    /* Thresholds */
    float observe_threshold;              /**< Severity for OBSERVE (default: 0.0) */
    float adjust_threshold;               /**< Severity for ADJUST (default: 0.3) */
    float regulate_threshold;             /**< Severity for REGULATE (default: 0.6) */
    float quarantine_threshold;           /**< Severity for QUARANTINE (default: 0.8) */

    /* Behavior */
    bool auto_intervene;                  /**< Automatically apply interventions (default: true) */
    bool immune_integration;              /**< Report to immune system (default: true) */
    bool kg_integration;                  /**< Update internal KG (default: true) */

    /* Intervention parameters */
    float neuromod_adjust_strength;       /**< Neuromodulator adjustment strength (default: 0.1) */
    bool enable_sleep_trigger;            /**< Allow triggering sleep cycle (default: true) */

    /* Logging */
    bool verbose_logging;                 /**< Enable verbose logging (default: false) */

} mental_health_guardian_config_t;

//=============================================================================
// Status Structure
//=============================================================================

/**
 * @brief Mental Health Guardian status report
 *
 * Snapshot of current guardian state and metrics.
 */
typedef struct mental_health_guardian_status {
    /* State */
    guardian_state_t state;               /**< Current operational state */
    guardian_intervention_level_t level;  /**< Current intervention level */

    /* Metrics */
    float overall_severity;               /**< Current overall severity [0.0-1.0] */
    uint64_t checks_performed;            /**< Total health checks since start */
    uint64_t interventions_applied;       /**< Total interventions since start */

    /* Breakdown by level */
    uint64_t observe_count;               /**< Times in OBSERVE level */
    uint64_t adjust_count;                /**< Times in ADJUST level */
    uint64_t regulate_count;              /**< Times in REGULATE level */
    uint64_t quarantine_count;            /**< Times in QUARANTINE level */

    /* Timing */
    uint64_t last_check_time_ms;          /**< Timestamp of last check */
    uint64_t uptime_ms;                   /**< Total running time */

    /* Disorder breakdown */
    uint32_t active_disorders;            /**< Number of active disorders */
    int primary_disorder;                 /**< Most severe disorder type (-1 if none) */
    int secondary_disorder;               /**< Second most severe disorder (-1 if none) */
    float secondary_disorder_score;       /**< Score of secondary disorder [0.0-1.0] */

} mental_health_guardian_status_t;

//=============================================================================
// Main Structure (Opaque)
//=============================================================================

/**
 * @brief Mental Health Guardian opaque handle
 */
typedef struct mental_health_guardian mental_health_guardian_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Get default guardian configuration
 *
 * Returns configuration with sensible defaults:
 * - 100ms monitoring interval (10Hz)
 * - Graduated thresholds: 0.0/0.3/0.6/0.8
 * - Auto-intervention enabled
 * - Immune and KG integration enabled
 *
 * @return Default configuration
 */
mental_health_guardian_config_t mental_health_guardian_default_config(void);

/**
 * @brief Create mental health guardian
 *
 * Creates an independent monitoring agent that will run in a background
 * thread. Does NOT start the thread - call mental_health_guardian_start().
 *
 * @param brain Parent brain instance (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @return Guardian handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~4KB for guardian structure + thread stack
 *
 * @note Caller must call mental_health_guardian_destroy() to free resources
 * @note Guardian requires brain's mental_health_monitor to be initialized
 */
mental_health_guardian_t* mental_health_guardian_create(
    void* brain,  /* brain_t */
    const mental_health_guardian_config_t* config
);

/**
 * @brief Destroy mental health guardian
 *
 * Stops the background thread (if running) and frees all resources.
 *
 * @param guardian Guardian to destroy (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Will block until background thread exits
 *
 * @note Safe to call with NULL pointer (no-op)
 */
void mental_health_guardian_destroy(mental_health_guardian_t* guardian);

//=============================================================================
// Thread Control API
//=============================================================================

/**
 * @brief Start guardian monitoring thread
 *
 * Starts the background thread that monitors mental health at the
 * configured interval.
 *
 * @param guardian Guardian handle (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 *
 * @note No-op if already running
 * @note Thread runs until stop() is called or guardian is destroyed
 */
bool mental_health_guardian_start(mental_health_guardian_t* guardian);

/**
 * @brief Stop guardian monitoring thread
 *
 * Signals the background thread to stop and waits for it to exit.
 *
 * @param guardian Guardian handle (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1) + wait for thread exit
 *
 * @note No-op if not running
 * @note Blocks until thread exits (max ~2x monitoring interval)
 */
bool mental_health_guardian_stop(mental_health_guardian_t* guardian);

/**
 * @brief Pause guardian monitoring (without stopping thread)
 *
 * Temporarily pauses the monitoring loop. Thread remains alive
 * but skips checks until resume() is called.
 *
 * @param guardian Guardian handle (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 *
 * @note Useful for temporary disable without thread restart overhead
 */
bool mental_health_guardian_pause(mental_health_guardian_t* guardian);

/**
 * @brief Resume guardian monitoring after pause
 *
 * @param guardian Guardian handle (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_resume(mental_health_guardian_t* guardian);

//=============================================================================
// Status & Metrics API
//=============================================================================

/**
 * @brief Get current guardian status
 *
 * Returns a snapshot of the guardian's current state and metrics.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param status Output status structure (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes (uses internal lock)
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_get_status(
    mental_health_guardian_t* guardian,
    mental_health_guardian_status_t* status
);

/**
 * @brief Reset guardian statistics
 *
 * Clears all counters (checks_performed, interventions_applied, etc.)
 * without stopping the guardian.
 *
 * @param guardian Guardian handle (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_reset_stats(mental_health_guardian_t* guardian);

//=============================================================================
// Manual Control API
//=============================================================================

/**
 * @brief Force immediate health check
 *
 * Triggers an immediate mental health check outside the normal
 * monitoring interval. Useful for on-demand assessment.
 *
 * @param guardian Guardian handle (non-NULL)
 * @return Current intervention level after check
 *
 * THREAD-SAFE: Yes (uses internal lock)
 * COMPLEXITY: O(disorders) - runs full disorder detection
 *
 * @note Does NOT wait for next scheduled check
 * @note Respects auto_intervene setting
 */
guardian_intervention_level_t mental_health_guardian_force_check(
    mental_health_guardian_t* guardian
);

/**
 * @brief Manually set intervention level
 *
 * Override the automatically determined intervention level.
 * Useful for external control or testing.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param level Desired intervention level
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 *
 * @note Will be overwritten on next automatic check
 * @note Use pause() to prevent automatic override
 */
bool mental_health_guardian_set_level(
    mental_health_guardian_t* guardian,
    guardian_intervention_level_t level
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Update guardian configuration
 *
 * Updates configuration while guardian is running. Changes take
 * effect on next monitoring cycle.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param config New configuration (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes (uses internal lock)
 * COMPLEXITY: O(1)
 *
 * @note Some changes (like monitoring_interval_ms) may take up to
 *       one full cycle to take effect
 */
bool mental_health_guardian_update_config(
    mental_health_guardian_t* guardian,
    const mental_health_guardian_config_t* config
);

/**
 * @brief Get current guardian configuration
 *
 * @param guardian Guardian handle (non-NULL)
 * @param config Output configuration (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_get_config(
    mental_health_guardian_t* guardian,
    mental_health_guardian_config_t* config
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect guardian to immune system
 *
 * Enables reporting of severe mental health states to the brain's
 * immune system as potential threats.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param immune Immune system to connect (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 *
 * @note Usually called automatically during brain initialization
 */
bool mental_health_guardian_connect_immune(
    mental_health_guardian_t* guardian,
    brain_immune_system_t* immune
);

/**
 * @brief Connect guardian to internal knowledge graph
 *
 * Enables updating mental health nodes in the internal KG for
 * topology-aware analysis.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param kg Internal KG to connect (non-NULL)
 * @param admin_token Admin token for KG write access
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_kg(
    mental_health_guardian_t* guardian,
    brain_kg_t* kg,
    uint64_t admin_token
);

/**
 * @brief Connect guardian to bio-async messaging system
 *
 * Enables publishing status reports, intervention notifications,
 * and receiving neuromodulator adjustment requests.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param bio_context Bio-async module context (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_bio_async(
    mental_health_guardian_t* guardian,
    void* bio_context  /* bio_module_context_t */
);

/**
 * @brief Connect guardian to neuromodulator system
 *
 * Enables direct neuromodulator adjustments for interventions.
 * Guardian will adjust dopamine, serotonin, norepinephrine based
 * on detected disorders.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param neuromod Spatial neuromodulator field (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_neuromod(
    mental_health_guardian_t* guardian,
    void* neuromod  /* spatial_neuromod_field_t* */
);

/**
 * @brief Connect guardian to brainstem/medulla for arousal control
 *
 * Enables arousal modulation and sleep triggering for interventions.
 * - ADJUST: Increase arousal for depression
 * - REGULATE: Trigger sleep cycle for recovery
 * - QUARANTINE: Reduce activity for safety
 *
 * @param guardian Guardian handle (non-NULL)
 * @param medulla Medulla system (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_brainstem(
    mental_health_guardian_t* guardian,
    void* medulla  /* medulla_t */
);

/**
 * @brief Connect guardian to sleep system
 *
 * Enables triggering sleep cycles at REGULATE level for
 * homeostatic recovery.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param sleep Sleep system (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_sleep(
    mental_health_guardian_t* guardian,
    void* sleep  /* sleep_system_t */
);

/**
 * @brief Connect guardian to homeostatic plasticity system
 *
 * Enables synaptic reset interventions at REGULATE level.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param plasticity Homeostatic plasticity system (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_plasticity(
    mental_health_guardian_t* guardian,
    void* plasticity  /* homeostatic_plasticity_t */
);

/**
 * @brief Register guardian with FEP orchestrator
 *
 * Registers the guardian as a cognitive FEP bridge, enabling coordinated
 * free energy minimization. The FEP orchestrator will call the guardian's
 * update function during update cycles.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param orchestrator FEP orchestrator (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_register_fep(
    mental_health_guardian_t* guardian,
    void* orchestrator  /* fep_orchestrator_t* */
);

/**
 * @brief Unregister guardian from FEP orchestrator
 *
 * @param guardian Guardian handle (non-NULL)
 * @return true on success, false on error
 */
bool mental_health_guardian_unregister_fep(
    mental_health_guardian_t* guardian
);

/**
 * @brief FEP update callback for guardian
 *
 * Called by FEP orchestrator during update cycles. Performs a health check
 * and returns free energy contribution.
 *
 * @param handle Guardian handle (cast from fep_bridge_handle_t)
 * @return 0 on success, -1 on error
 */
int mental_health_guardian_fep_update(void* handle);

//=============================================================================
// Cognitive Module Integration API
//=============================================================================

/**
 * @brief Connect guardian to working memory
 *
 * Enables guardian to interact with working memory for attention resets
 * and cognitive load management at REGULATE intervention level.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param working_memory Working memory system (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_working_memory(
    mental_health_guardian_t* guardian,
    void* working_memory  /* working_memory_t* */
);

/**
 * @brief Connect guardian to executive controller
 *
 * Enables guardian to interact with executive functions for cognitive
 * control adjustments and task prioritization during interventions.
 *
 * @param guardian Guardian handle (non-NULL)
 * @param executive Executive controller (non-NULL)
 * @return true on success, false on error
 *
 * THREAD-SAFE: Yes
 * COMPLEXITY: O(1)
 */
bool mental_health_guardian_connect_executive(
    mental_health_guardian_t* guardian,
    void* executive  /* executive_controller_t* */
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert intervention level to string
 *
 * @param level Intervention level
 * @return Human-readable string (e.g., "ADJUST")
 */
const char* guardian_level_to_string(guardian_intervention_level_t level);

/**
 * @brief Convert guardian state to string
 *
 * @param state Guardian state
 * @return Human-readable string (e.g., "RUNNING")
 */
const char* guardian_state_to_string(guardian_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_GUARDIAN_H */

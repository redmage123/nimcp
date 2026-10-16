/**
 * @file nimcp_gpu_execution_immune_bridge.h
 * @brief GPU Execution Mode-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and GPU execution modes
 * WHY:  Execution mode selection (CPU/GPU/Hybrid) should adapt to immune state;
 *       execution failures should trigger immune responses
 * HOW:  Inflammation modulates execution mode selection and fallback strategies;
 *       execution errors trigger antigen presentation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → EXECUTION PATHWAYS:
 * ---------------------------
 * 1. Energy Conservation During Illness:
 *    - High inflammation → prefer CPU execution (lower energy)
 *    - Cytokine storm → force CPU-only mode (emergency conservation)
 *    - Systemic inflammation → hybrid mode (balanced approach)
 *    - Reference: Biological systems reduce energy expenditure during infection
 *
 * 2. Adaptive Mode Selection:
 *    - IL-1β/IL-6/TNF-α → bias toward CPU execution
 *    - Inflammation duration → gradually shift to lower-energy modes
 *    - IL-10 (anti-inflammatory) → allow GPU mode recovery
 *
 * 3. Fallback Strategy Modulation:
 *    - High inflammation → more aggressive fallback
 *    - Enable auto-fallback during immune activation
 *    - Reduce fallback threshold during stress
 *
 * EXECUTION → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Mode Switch Failures:
 *    - Failed GPU initialization → antigen presentation (severity 7)
 *    - Context creation failure → immune activation (severity 8)
 *    - Device enumeration failure → moderate threat (severity 5)
 *
 * 2. Performance Monitoring:
 *    - Execution time degradation → stress response
 *    - Repeated fallback events → inflammatory signaling
 *    - Resource allocation failures → cytokine release
 *
 * 3. Profiling Data:
 *    - Validation failures → immune system alert
 *    - Performance below baseline → chronic stress indicator
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   GPU EXECUTION-IMMUNE BRIDGE                              ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → EXECUTION PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │  INFLAMMATION    │                                             │  ║
 * ║   │   │ ──────────────── │                                             │  ║
 * ║   │   │ NONE     → GPU   │  ───────┐                                   │  ║
 * ║   │   │ LOCAL    → GPU   │         │                                   │  ║
 * ║   │   │ REGIONAL → Hybrid│         ├──→ Mode Selection                 │  ║
 * ║   │   │ SYSTEMIC → CPU   │         │                                   │  ║
 * ║   │   │ STORM    → CPU   │         │                                   │  ║
 * ║   │   └──────────────────┘         │                                   │  ║
 * ║   │                                ▼                                   │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     EXECUTION MODE SYSTEM       │                             │  ║
 * ║   │   │  - Mode preference updated      │                             │  ║
 * ║   │   │  - Fallback strategy adjusted   │                             │  ║
 * ║   │   │  - Energy conservation enabled  │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  EXECUTION → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ INIT FAILURES    │ ──→ Antigen Presentation (severity 8)       │  ║
 * ║   │   │ MODE SWITCH FAIL │ ──→ Immune Activation (severity 6)          │  ║
 * ║   │   │ FALLBACK EVENTS  │ ──→ Stress Response (severity 4)            │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ PERF DEGRADATION │ ──→ Chronic Stress Indicator                │  ║
 * ║   │   │ VALIDATION FAIL  │ ──→ Inflammatory Signaling                  │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_GPU_EXECUTION_IMMUNE_BRIDGE_H
#define NIMCP_GPU_EXECUTION_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "gpu/nimcp_execution_mode.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Inflammation execution mode preferences */
#define INFLAMMATION_MODE_NONE_PREFERENCE     EXEC_MODE_GPU_CUDA     /**< Full GPU */
#define INFLAMMATION_MODE_LOCAL_PREFERENCE    EXEC_MODE_GPU_CUDA     /**< Still GPU */
#define INFLAMMATION_MODE_REGIONAL_PREFERENCE EXEC_MODE_HYBRID       /**< CPU+GPU */
#define INFLAMMATION_MODE_SYSTEMIC_PREFERENCE EXEC_MODE_CPU_PARALLEL /**< CPU only */
#define INFLAMMATION_MODE_STORM_PREFERENCE    EXEC_MODE_CPU_SEQUENTIAL /**< Minimal energy */

/* Execution error severity mapping */
#define EXEC_ERROR_SEVERITY_INIT_FAILURE       8   /**< Context init failed */
#define EXEC_ERROR_SEVERITY_MODE_SWITCH_FAIL   6   /**< Mode switch failed */
#define EXEC_ERROR_SEVERITY_FALLBACK_EVENT     4   /**< Fallback triggered */
#define EXEC_ERROR_SEVERITY_ALLOC_FAILURE      7   /**< Memory allocation failed */
#define EXEC_ERROR_SEVERITY_SYNC_FAILURE       5   /**< Synchronization failed */
#define EXEC_ERROR_SEVERITY_VALIDATION_FAIL    3   /**< Validation error */

/* Energy conservation factors */
#define EXEC_ENERGY_FACTOR_CPU_SEQUENTIAL  0.3f  /**< 30% of GPU energy */
#define EXEC_ENERGY_FACTOR_CPU_PARALLEL    0.5f  /**< 50% of GPU energy */
#define EXEC_ENERGY_FACTOR_HYBRID          0.7f  /**< 70% of GPU energy */
#define EXEC_ENERGY_FACTOR_GPU             1.0f  /**< Full GPU energy */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine execution mode effects
 *
 * Represents how cytokine levels modulate execution mode selection
 */
typedef struct {
    /* Pro-inflammatory biases */
    float il1_cpu_bias;               /**< IL-1β bias toward CPU */
    float il6_cpu_bias;               /**< IL-6 bias toward CPU */
    float tnf_cpu_bias;               /**< TNF-α bias toward CPU */
    float ifn_gamma_cpu_bias;         /**< IFN-γ bias toward CPU */

    /* Anti-inflammatory effects */
    float il10_gpu_recovery;          /**< IL-10 allows GPU recovery */

    /* Mode preference */
    execution_mode_t preferred_mode;  /**< Immune-recommended mode */
    float energy_conservation_factor; /**< Energy usage factor [0-1] */
    bool force_cpu_only;              /**< Force CPU-only mode */
    bool enable_aggressive_fallback;  /**< Enable quick fallback */
} execution_cytokine_effects_t;

/**
 * @brief Execution error state for immune monitoring
 *
 * Tracks execution failures and performance for immune system
 */
typedef struct {
    /* Error counters */
    uint32_t init_failures;           /**< Context init failures */
    uint32_t mode_switch_failures;    /**< Mode switch failures */
    uint32_t fallback_events;         /**< Auto-fallback events */
    uint32_t allocation_failures;     /**< Memory allocation failures */
    uint32_t sync_failures;           /**< Sync failures */
    uint32_t validation_failures;     /**< Validation failures */

    /* Performance metrics */
    double current_ops_per_sec;       /**< Current throughput */
    double baseline_ops_per_sec;      /**< Baseline performance */
    float performance_ratio;          /**< current/baseline */

    /* Current state */
    execution_mode_t active_mode;     /**< Currently active mode */
    execution_mode_t fallback_mode;   /**< Current fallback mode */
    bool in_fallback_state;           /**< Currently fallen back */

    /* Stress indicators */
    bool performance_degraded;        /**< Below acceptable threshold */
    bool repeated_failures;           /**< Multiple failures in window */

    /* Last error info */
    int last_error_code;              /**< Last error code */
    uint64_t last_error_timestamp;    /**< When last error occurred */
} execution_error_state_t;

/**
 * @brief Execution-driven immune modulation
 *
 * How execution state affects immune function
 */
typedef struct {
    /* Execution stress state */
    float failure_stress_level;       /**< Stress from failures [0-1] */
    float performance_stress_level;   /**< Stress from degradation [0-1] */
    float fallback_stress_level;      /**< Stress from fallbacks [0-1] */

    /* Immune triggers */
    bool should_trigger_immune;       /**< Should activate immune response */
    uint8_t antigen_severity;         /**< Severity for antigen presentation */
    bool cytokine_release_triggered;  /**< Cytokine release from exec error */

    /* Error-specific responses */
    uint32_t errors_since_last_update; /**< Errors since last immune update */
    bool critical_error_detected;     /**< Critical error requiring response */
} execution_immune_modulation_t;

/**
 * @brief Complete GPU execution-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    execution_context_t exec_context;

    /* Current state */
    execution_cytokine_effects_t cytokine_effects;
    execution_error_state_t error_state;
    execution_immune_modulation_t immune_modulation;

    /* Integration flags */
    bool enable_cytokine_mode_modulation;
    bool enable_exec_error_immune_response;
    bool enable_energy_conservation;
    bool enable_fallback_modulation;
    bool enable_performance_monitoring;

    /* Configuration */
    execution_mode_t baseline_mode;   /**< Baseline execution mode */
    bool allow_mode_switching;        /**< Allow runtime mode changes */

    /* Timing */
    uint64_t last_update_time;
    uint64_t error_window_start;      /**< Start of error counting window */

    /* Statistics */
    uint64_t total_updates;
    uint32_t mode_changes;
    uint32_t immune_triggers;
    uint32_t fallback_triggers;
    uint32_t energy_conservation_events;

    nimcp_platform_mutex_t* mutex;
} execution_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_mode_modulation;
    bool enable_exec_error_immune_response;
    bool enable_energy_conservation;
    bool enable_fallback_modulation;
    bool enable_performance_monitoring;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float error_sensitivity;            /**< Error response sensitivity [0.5-2.0] */

    /* Mode selection */
    execution_mode_t baseline_mode;     /**< Baseline execution mode */
    bool allow_mode_switching;          /**< Allow runtime mode changes */

    /* Thresholds */
    float performance_degradation_threshold; /**< Perf ratio for degradation [0.5-0.9] */
    uint32_t error_window_ms;           /**< Error counting window (ms) */
    uint32_t max_errors_per_window;     /**< Max errors before immune trigger */
} execution_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int execution_immune_default_config(execution_immune_config_t* config);

/**
 * @brief Create GPU execution-immune bridge
 *
 * WHAT: Initialize bidirectional execution-immune integration
 * WHY:  Enable realistic execution-immune coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param exec_context Execution context
 * @return New bridge or NULL on failure
 */
execution_immune_bridge_t* execution_immune_create(
    const execution_immune_config_t* config,
    brain_immune_system_t* immune_system,
    execution_context_t exec_context
);

/**
 * @brief Destroy GPU execution-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void execution_immune_destroy(execution_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Execution API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to execution mode
 *
 * WHAT: Modulate execution mode preference based on cytokine levels
 * WHY:  Inflammation should conserve energy via CPU execution
 * HOW:  Query immune system cytokines, adjust mode preference
 *
 * @param bridge Execution-immune bridge
 * @return 0 on success
 */
int execution_immune_apply_cytokine_effects(execution_immune_bridge_t* bridge);

/**
 * @brief Get recommended execution mode from immune state
 *
 * WHAT: Calculate optimal execution mode given inflammation level
 * WHY:  Balance performance vs energy conservation
 * HOW:  Map inflammation level to execution mode
 *
 * @param bridge Execution-immune bridge
 * @return Recommended execution mode
 */
execution_mode_t execution_immune_get_recommended_mode(
    const execution_immune_bridge_t* bridge
);

/**
 * @brief Get energy conservation factor
 *
 * WHAT: Get current energy usage factor from immune modulation
 * WHY:  Need energy factor for resource allocation
 * HOW:  Return factor based on inflammation-preferred mode
 *
 * @param bridge Execution-immune bridge
 * @return Energy factor [0-1] (1.0 = full GPU, 0.3 = CPU sequential)
 */
float execution_immune_get_energy_factor(const execution_immune_bridge_t* bridge);

/* ============================================================================
 * Execution → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from execution error
 *
 * WHAT: Activate immune system when execution errors occur
 * WHY:  Execution failures threaten neural computation integrity
 * HOW:  Present antigen based on error severity
 *
 * @param bridge Execution-immune bridge
 * @param error_code Error code
 * @param error_message Error description
 * @return 0 on success
 */
int execution_immune_trigger_error_response(
    execution_immune_bridge_t* bridge,
    int error_code,
    const char* error_message
);

/**
 * @brief Monitor execution performance and trigger immune if degraded
 *
 * WHAT: Check execution performance; trigger immune if below baseline
 * WHY:  Performance degradation indicates system stress
 * HOW:  Compare current vs baseline performance, trigger if ratio low
 *
 * @param bridge Execution-immune bridge
 * @return 0 on success
 */
int execution_immune_monitor_performance(execution_immune_bridge_t* bridge);

/**
 * @brief Update execution error state
 *
 * WHAT: Refresh execution error counters and performance metrics
 * WHY:  Need current execution state for immune decision making
 * HOW:  Query execution context for stats and errors
 *
 * @param bridge Execution-immune bridge
 * @return 0 on success
 */
int execution_immune_update_error_state(execution_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update execution-immune bridge (both directions)
 *
 * WHAT: Process all execution-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, check execution errors, adjust modes
 *
 * @param bridge Execution-immune bridge
 * @return 0 on success
 */
int execution_immune_update(execution_immune_bridge_t* bridge);

/**
 * @brief Apply modulation to execution context
 *
 * WHAT: Update execution context with immune-modulated parameters
 * WHY:  Actually apply mode changes and fallback strategies
 * HOW:  Call execution context APIs to update mode and config
 *
 * @param bridge Execution-immune bridge
 * @return 0 on success
 */
int execution_immune_apply_modulation(execution_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine execution effects
 *
 * @param bridge Execution-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int execution_immune_get_cytokine_effects(
    const execution_immune_bridge_t* bridge,
    execution_cytokine_effects_t* effects
);

/**
 * @brief Get current execution error state
 *
 * @param bridge Execution-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int execution_immune_get_error_state(
    const execution_immune_bridge_t* bridge,
    execution_error_state_t* state
);

/**
 * @brief Check if execution is under immune-induced mode change
 *
 * @param bridge Execution-immune bridge
 * @return true if mode changed due to immune
 */
bool execution_immune_is_mode_changed(const execution_immune_bridge_t* bridge);

/**
 * @brief Get current energy conservation factor
 *
 * @param bridge Execution-immune bridge
 * @return Energy factor [0-1]
 */
float execution_immune_get_energy_conservation_factor(
    const execution_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_GPU_EXECUTION
 *
 * @param bridge Execution-immune bridge
 * @return 0 on success, -1 on error
 */
int execution_immune_connect_bio_async(execution_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Execution-immune bridge
 * @return 0 on success
 */
int execution_immune_disconnect_bio_async(execution_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Execution-immune bridge
 * @return true if connected
 */
bool execution_immune_is_bio_async_connected(const execution_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_EXECUTION_IMMUNE_BRIDGE_H */

/**
 * @file nimcp_quantum_annealing_immune_bridge.h
 * @brief Quantum Annealing-Immune System Integration for NIMCP
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and quantum annealing optimizer
 * WHY:  Model immune-modulated exploration/exploitation tradeoff and adaptive optimization
 *       under inflammatory conditions
 * HOW:  Immune inflammation modulates annealing temperature; optimization divergence
 *       triggers immune response
 *
 * BIOLOGICAL BASIS:
 * ==================
 * The immune system influences neural optimization through multiple mechanisms:
 *
 * 1. FEVER & EXPLORATION:
 *    - High inflammation → increased neural noise and exploration
 *    - Fever disrupts stable patterns → forces search for new solutions
 *    - Cytokines modulate neurotransmitter balance → altered optimization landscape
 *
 * 2. ENERGY CONSERVATION:
 *    - Immune activation is metabolically expensive
 *    - During illness, reduce costly optimization iterations
 *    - Prioritize survival over fine-tuning
 *
 * 3. ADAPTIVE LEARNING:
 *    - Mild inflammation → enhanced pattern recognition (immune priming)
 *    - Severe inflammation → reduced precision, increased exploration
 *    - Anti-inflammatory state → enable fine-grained optimization
 *
 * This manifests as:
 * - Temperature modulation during fever (increased annealing temperature)
 * - Iteration reduction during severe inflammation (energy conservation)
 * - Cooling schedule modification based on cytokine levels
 * - Early termination during cytokine storm (survival mode)
 *
 * NIMCP MAPPING:
 * ==============
 * ```
 * BIOLOGICAL MECHANISM              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Fever-induced neural noise     → Annealing temperature increase
 * Cytokine IL-1β, IL-6, TNF-α    → Temperature multiplier (1.0-3.0x)
 * Energy conservation during sick→ Iteration reduction (50%-90%)
 * Immune activation cost         → Premature cooling (faster convergence)
 * Anti-inflammatory IL-10        → Fine-grained search (lower temp)
 * Optimization divergence        → Antigen presentation (threat)
 * Energy landscape corruption    → Inflammation escalation
 * Stuck in local minimum         → Immune-triggered exploration boost
 * ```
 *
 * ARCHITECTURE:
 * =============
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              QUANTUM ANNEALING IMMUNE BRIDGE                     │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  IMMUNE → OPTIMIZATION (Inflammation modulates search)          │
 * │  ┌────────────────┐           ┌─────────────────────────┐      │
 * │  │ Brain Immune   │           │ Quantum Annealing       │      │
 * │  │ Inflammation   │──temp──→ │ Temperature             │      │
 * │  │   Level (0-4)  │ modifier │ Modulation              │      │
 * │  └────────────────┘           └─────────────────────────┘      │
 * │         │                              │                        │
 * │         │ NONE      → Temp × 1.0       │ Normal annealing      │
 * │         │ LOCAL     → Temp × 1.2       │ Mild exploration      │
 * │         │ REGIONAL  → Temp × 1.5       │ Moderate exploration  │
 * │         │ SYSTEMIC  → Temp × 2.0       │ High exploration      │
 * │         │ STORM     → Temp × 3.0       │ Maximum exploration   │
 * │         │                              │                        │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │         Iteration Control                                   │ │
 * │  │  - NONE: 100% iterations                                   │ │
 * │  │  - LOCAL: 90% iterations                                   │ │
 * │  │  - REGIONAL: 70% iterations                                │ │
 * │  │  - SYSTEMIC: 50% iterations                                │ │
 * │  │  - STORM: 10% iterations (emergency)                       │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                                                                  │
 * │  OPTIMIZATION → IMMUNE (Divergence triggers response)           │
 * │  ┌─────────────────────────┐       ┌──────────────────┐        │
 * │  │   Annealing Metrics     │       │  Brain Immune    │        │
 * │  │   Energy, Stuck count   │─detect→│  Antigen         │        │
 * │  │   Divergence Detection  │ threat │  Presentation    │        │
 * │  └─────────────────────────┘       └──────────────────┘        │
 * │         │                                   │                   │
 * │         │ Energy explosion  → SEVERITY 9    │                   │
 * │         │ Stuck in minimum  → SEVERITY 6    │                   │
 * │         │ No improvement    → SEVERITY 4    │                   │
 * │         │ Oscillation       → SEVERITY 5    │                   │
 * │         │                                   │                   │
 * │         └────────────→ Immune Response ─────┘                   │
 * │                       (Temp boost, exploration)                 │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * CYTOKINE EFFECTS ON OPTIMIZATION:
 * ==================================
 * - IL-1β: Increase temperature, reduce iterations (pro-inflammatory)
 * - IL-6: Accelerate cooling, premature convergence (acute phase)
 * - TNF-α: Reduce quantum tunneling strength (severe inflammation)
 * - IFN-γ: Increase exploration, reject current solution (antiviral-like)
 * - IL-10: Enable fine-tuning, reduce temperature (anti-inflammatory)
 *
 * INTEGRATION POINTS:
 * ===================
 * 1. Temperature: Multiply base temperature by inflammation factor
 * 2. Iterations: Reduce max iterations during immune response
 * 3. Cooling schedule: Modify cooling rate based on cytokine levels
 * 4. Quantum tunneling: Adjust strength based on inflammation
 * 5. Acceptance probability: Modulate based on immune state
 *
 * USAGE EXAMPLE:
 * ==============
 * ```c
 * // Create quantum annealing immune bridge
 * qa_immune_config_t config = qa_immune_default_config();
 * qa_immune_bridge_t* qa_bridge = qa_immune_create(&config, annealer, brain_immune);
 *
 * // Connect bio-async
 * qa_immune_connect_bio_async(qa_bridge);
 *
 * // Before optimization
 * qa_immune_update(qa_bridge);
 * qa_immune_apply_modulation(qa_bridge);
 *
 * // Run annealing (automatically uses modulated parameters)
 * float energy = quantum_anneal(annealer, energy_func, initial, result, dim, NULL);
 *
 * // Check for optimization problems
 * qa_immune_check_convergence(qa_bridge, energy);
 * ```
 *
 * GOTCHAS:
 * ========
 * - Temperature modulation is multiplicative per iteration
 * - Iteration reduction happens before optimization starts
 * - High inflammation can cause over-exploration (slow convergence)
 * - Cytokine storm may prevent convergence entirely
 * - Monitor effective temperature to avoid thermal runaway
 *
 * NIMCP STANDARDS:
 * ================
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_QUANTUM_ANNEALING_IMMUNE_BRIDGE_H
#define NIMCP_QUANTUM_ANNEALING_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Core dependencies */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define QA_IMMUNE_MODULE_NAME           "qa_immune_bridge"
#define QA_IMMUNE_MAX_HISTORY           100  /**< Max optimization metric history */

/* Temperature modulation factors per inflammation level */
#define QA_IMMUNE_TEMP_FACTOR_NONE      1.0f  /**< No inflammation - normal temp */
#define QA_IMMUNE_TEMP_FACTOR_LOCAL     1.2f  /**< Local inflammation - mild exploration boost */
#define QA_IMMUNE_TEMP_FACTOR_REGIONAL  1.5f  /**< Regional inflammation - moderate exploration */
#define QA_IMMUNE_TEMP_FACTOR_SYSTEMIC  2.0f  /**< Systemic inflammation - high exploration */
#define QA_IMMUNE_TEMP_FACTOR_STORM     3.0f  /**< Cytokine storm - maximum exploration */

/* Iteration reduction factors per inflammation level */
#define QA_IMMUNE_ITER_FACTOR_NONE      1.00f  /**< No inflammation - full iterations */
#define QA_IMMUNE_ITER_FACTOR_LOCAL     0.90f  /**< Local inflammation - 90% iterations */
#define QA_IMMUNE_ITER_FACTOR_REGIONAL  0.70f  /**< Regional inflammation - 70% iterations */
#define QA_IMMUNE_ITER_FACTOR_SYSTEMIC  0.50f  /**< Systemic inflammation - 50% iterations */
#define QA_IMMUNE_ITER_FACTOR_STORM     0.10f  /**< Cytokine storm - 10% iterations (emergency) */

/* Quantum tunneling modulation per inflammation level */
#define QA_IMMUNE_TUNNEL_FACTOR_NONE      1.0f  /**< No inflammation - normal tunneling */
#define QA_IMMUNE_TUNNEL_FACTOR_LOCAL     1.1f  /**< Local inflammation - mild boost */
#define QA_IMMUNE_TUNNEL_FACTOR_REGIONAL  1.2f  /**< Regional inflammation - moderate boost */
#define QA_IMMUNE_TUNNEL_FACTOR_SYSTEMIC  0.8f  /**< Systemic inflammation - reduce precision */
#define QA_IMMUNE_TUNNEL_FACTOR_STORM     0.5f  /**< Cytokine storm - severely reduced */

/* Convergence detection thresholds */
#define QA_IMMUNE_ENERGY_EXPLOSION_RATIO       100.0f  /**< Energy increase ratio for explosion */
#define QA_IMMUNE_STUCK_ITERATIONS             50      /**< Iterations without improvement = stuck */
#define QA_IMMUNE_OSCILLATION_THRESHOLD        0.1f    /**< Energy variance threshold */

/* Immune response parameters */
#define QA_IMMUNE_MIN_RESPONSE_DURATION_MS     3000    /**< Min immune response time */
#define QA_IMMUNE_TEMP_EMA_ALPHA               0.15f   /**< EMA smoothing for temperature */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct qa_immune_bridge qa_immune_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Quantum annealing problem types that trigger immune response
 *
 * BIOLOGICAL BASIS:
 * Different optimization failures map to different threat types
 */
typedef enum {
    QA_PROBLEM_NONE = 0,              /**< No problem detected */
    QA_PROBLEM_ENERGY_EXPLOSION,      /**< Energy exploded to infinity */
    QA_PROBLEM_STUCK_LOCAL_MINIMUM,   /**< Stuck in local minimum */
    QA_PROBLEM_NO_IMPROVEMENT,        /**< No progress for many iterations */
    QA_PROBLEM_OSCILLATION,           /**< Energy oscillating wildly */
    QA_PROBLEM_DIVERGENCE,            /**< Solution diverging */
    QA_PROBLEM_NUMERICAL_INSTABILITY, /**< NaN/Inf in state */
    QA_PROBLEM_COUNT
} qa_problem_type_t;

/**
 * @brief Quantum annealing immune bridge phase
 */
typedef enum {
    QA_IMMUNE_PHASE_IDLE = 0,         /**< Not optimizing */
    QA_IMMUNE_PHASE_OPTIMIZING,       /**< Active optimization */
    QA_IMMUNE_PHASE_RESPONDING,       /**< Immune response active */
    QA_IMMUNE_PHASE_RECOVERING,       /**< Post-immune recovery */
    QA_IMMUNE_PHASE_RESOLVED          /**< Problem resolved */
} qa_immune_phase_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Optimization metrics for immune system monitoring
 *
 * WHAT: Current optimization state for divergence detection
 * WHY:  Immune system needs optimization context to assess health
 */
typedef struct {
    uint64_t iteration;                /**< Current iteration */
    float energy;                      /**< Current energy value */
    float energy_prev;                 /**< Previous energy */
    float energy_min;                  /**< Minimum energy found */
    float energy_ema;                  /**< Exponential moving average */
    float energy_variance;             /**< Energy variance */
    float temperature;                 /**< Current temperature */
    float effective_temperature;       /**< After immune modulation */
    uint32_t iterations_without_improvement; /**< Stuck counter */
    bool has_nan;                      /**< NaN detected */
    bool has_inf;                      /**< Inf detected */
    uint64_t timestamp_ms;             /**< Metric timestamp */
} qa_immune_metrics_t;

/**
 * @brief Cytokine effects on quantum annealing parameters
 *
 * WHAT: Computed effects of cytokines on optimization
 * WHY:  Translate immune state to annealing parameter adjustments
 */
typedef struct {
    /* Cytokine concentrations (from brain immune) */
    float il1_concentration;           /**< IL-1β (pro-inflammatory) */
    float il6_concentration;           /**< IL-6 (acute phase) */
    float il10_concentration;          /**< IL-10 (anti-inflammatory) */
    float tnf_concentration;           /**< TNF-α (severe inflammation) */
    float ifn_gamma_concentration;     /**< IFN-γ (exploration boost) */

    /* Computed modulation factors */
    float temperature_factor;          /**< Temperature multiplier */
    float iteration_factor;            /**< Iteration reduction factor */
    float tunneling_factor;            /**< Quantum tunneling multiplier */
    float cooling_rate_factor;         /**< Cooling schedule modifier */
    float acceptance_threshold_factor; /**< Acceptance probability modifier */

    /* Aggregate effects */
    float total_exploration_boost;     /**< Combined exploration increase */
    float energy_conservation_factor;  /**< Energy cost reduction */
} qa_immune_cytokine_effects_t;

/**
 * @brief Quantum annealing problem event
 *
 * WHAT: Detected optimization problem that triggers immune response
 * WHY:  Map optimization failures to immune antigens
 */
typedef struct {
    uint32_t event_id;                 /**< Unique event ID */
    qa_problem_type_t type;            /**< Type of problem */
    uint32_t severity;                 /**< Severity (1-10) */
    float confidence;                  /**< Detection confidence (0-1) */
    qa_immune_metrics_t metrics;       /**< Metrics at detection time */
    uint32_t antigen_id;               /**< Brain immune antigen ID */
    bool immune_triggered;             /**< Immune response activated */
    uint64_t detection_time_ms;        /**< When detected */
} qa_immune_problem_event_t;

/**
 * @brief Configuration for quantum annealing immune bridge
 */
typedef struct {
    /* Modulation enables */
    bool enable_temp_modulation;       /**< Enable temperature modulation */
    bool enable_iter_reduction;        /**< Enable iteration reduction */
    bool enable_tunneling_modulation;  /**< Enable quantum tunneling modulation */
    bool enable_cooling_modulation;    /**< Enable cooling rate modulation */

    /* Custom factors (override defaults) */
    float temp_factor_local;           /**< Custom temp factor for local inflammation */
    float temp_factor_regional;        /**< Custom temp factor for regional inflammation */
    float temp_factor_systemic;        /**< Custom temp factor for systemic inflammation */
    float temp_factor_storm;           /**< Custom temp factor for cytokine storm */

    /* Problem detection thresholds */
    float energy_explosion_ratio;      /**< Energy increase ratio for explosion */
    uint32_t stuck_iterations;         /**< Iterations without improvement = stuck */
    float oscillation_threshold;       /**< Energy variance threshold */

    /* Immune response settings */
    bool enable_auto_immune_response;  /**< Auto-trigger immune on problems */
    uint64_t min_response_duration_ms; /**< Min immune response time */
    float temp_ema_alpha;              /**< EMA smoothing for temperature */

    /* Monitoring */
    uint32_t history_size;             /**< Optimization history buffer size */
    bool enable_logging;               /**< Enable detailed logging */
} qa_immune_config_t;

/**
 * @brief Quantum annealing immune bridge statistics
 */
typedef struct {
    /* Problem detections */
    uint64_t total_problems;
    uint64_t problems_by_type[QA_PROBLEM_COUNT];

    /* Immune responses */
    uint64_t immune_responses_triggered;
    uint64_t total_immune_duration_ms;
    float avg_immune_duration_ms;

    /* Temperature modulation */
    uint64_t temp_modulations;
    float avg_temp_factor;
    float max_effective_temp;

    /* Iteration reduction */
    uint64_t optimizations_run;
    float avg_iter_reduction_pct;

    /* Health metrics */
    uint64_t successful_optimizations;
    uint64_t failed_optimizations;
    float success_rate;

    /* Current state */
    qa_immune_phase_t current_phase;
    brain_inflammation_level_t current_inflammation;
    float current_temp_factor;
} qa_immune_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Quantum annealing immune bridge state
 */
struct qa_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    qa_immune_config_t config;         /**< Configuration */
    qa_immune_phase_t phase;           /**< Current phase */

    /* Integration handles */
    brain_immune_system_t* immune_system;      /**< Brain immune system */
    quantum_annealer_t annealer;               /**< Quantum annealer */

    /* Current state */
    brain_inflammation_level_t inflammation;   /**< Current inflammation */
    qa_immune_cytokine_effects_t cytokine_effects; /**< Computed cytokine effects */
    qa_immune_metrics_t current_metrics;       /**< Current optimization metrics */

    /* History */
    qa_immune_metrics_t* history;      /**< Metric history */
    size_t history_count;
    size_t history_capacity;
    size_t history_index;

    /* Problem tracking */
    qa_immune_problem_event_t* events; /**< Problem events */
    size_t event_count;
    size_t event_capacity;
    uint32_t next_event_id;

    /* Modulation state */
    float current_temp_factor;         /**< Current temperature multiplier */
    float current_iter_factor;         /**< Current iteration multiplier */
    float temp_ema;                    /**< Smoothed temperature */
    bool in_immune_response;           /**< Active immune response */
    uint64_t immune_response_start_ms; /**< When response began */

    /* Convergence tracking */
    float best_energy;                 /**< Best energy found */
    uint32_t iterations_without_improvement; /**< Stuck counter */

    /* Statistics */
    qa_immune_stats_t stats;

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;     /**< Platform mutex */

    /* State */
    bool running;                      /**< Bridge active */
    uint64_t start_time_ms;            /**< Bridge start time */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int qa_immune_default_config(qa_immune_config_t* config);

/**
 * @brief Create quantum annealing immune bridge
 *
 * WHAT: Initialize quantum annealing-immune integration
 * WHY:  Set up bidirectional coordination
 * HOW:  Allocate state, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param annealer Quantum annealer instance
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
qa_immune_bridge_t* qa_immune_create(
    const qa_immune_config_t* config,
    quantum_annealer_t annealer,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy quantum annealing immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free buffers, unregister callbacks
 *
 * @param bridge Bridge to destroy
 */
void qa_immune_destroy(qa_immune_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable cytokine messaging via bio-async
 * WHY:  Distributed immune signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Quantum annealing immune bridge
 * @return 0 on success
 */
int qa_immune_connect_bio_async(qa_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Disable bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 *
 * @param bridge Quantum annealing immune bridge
 * @return 0 on success
 */
int qa_immune_disconnect_bio_async(qa_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Verify messaging availability
 * HOW:  Check bio_async_enabled flag
 *
 * @param bridge Quantum annealing immune bridge
 * @return true if connected
 */
bool qa_immune_is_bio_async_connected(const qa_immune_bridge_t* bridge);

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

/**
 * @brief Update immune bridge state
 *
 * WHAT: Sync inflammation and cytokine effects from brain immune
 * WHY:  Keep optimization parameters aligned with immune state
 * HOW:  Query brain immune, compute modulation factors
 *
 * @param bridge Quantum annealing immune bridge
 * @return 0 on success
 */
int qa_immune_update(qa_immune_bridge_t* bridge);

/**
 * @brief Apply immune modulation to quantum annealing parameters
 *
 * WHAT: Modify annealer temperature, iterations based on inflammation
 * WHY:  Reflect immune state in optimization behavior
 * HOW:  Apply computed modulation factors to annealer config
 *
 * @param bridge Quantum annealing immune bridge
 * @return 0 on success
 */
int qa_immune_apply_modulation(qa_immune_bridge_t* bridge);

/**
 * @brief Update optimization metrics
 *
 * WHAT: Record current optimization state
 * WHY:  Track metrics for problem detection
 * HOW:  Store metrics in history buffer
 *
 * @param bridge Quantum annealing immune bridge
 * @param iteration Current iteration
 * @param energy Current energy value
 * @param temperature Current temperature
 * @return 0 on success
 */
int qa_immune_update_metrics(
    qa_immune_bridge_t* bridge,
    uint64_t iteration,
    float energy,
    float temperature
);

/* ============================================================================
 * Problem Detection and Response API
 * ============================================================================ */

/**
 * @brief Check optimization convergence
 *
 * WHAT: Detect optimization problems
 * WHY:  Auto-trigger immune response to failures
 * HOW:  Analyze metrics for divergence, stuck states, etc.
 *
 * @param bridge Quantum annealing immune bridge
 * @param final_energy Final energy after optimization
 * @return Detected problem type (NONE if healthy)
 */
qa_problem_type_t qa_immune_check_convergence(
    qa_immune_bridge_t* bridge,
    float final_energy
);

/**
 * @brief Report optimization problem
 *
 * WHAT: Manually report optimization problem
 * WHY:  Allow external detection to trigger immune response
 * HOW:  Create event, present as antigen to brain immune
 *
 * @param bridge Quantum annealing immune bridge
 * @param type Problem type
 * @param severity Severity (1-10)
 * @param event_id Output: problem event ID
 * @return 0 on success
 */
int qa_immune_report_problem(
    qa_immune_bridge_t* bridge,
    qa_problem_type_t type,
    uint32_t severity,
    uint32_t* event_id
);

/**
 * @brief Trigger immune response to optimization problem
 *
 * WHAT: Present optimization problem as immune antigen
 * WHY:  Activate brain immune system for optimization failure
 * HOW:  Convert problem to epitope, present to brain immune
 *
 * @param bridge Quantum annealing immune bridge
 * @param event_id Problem event ID
 * @return 0 on success
 */
int qa_immune_trigger_immune_response(
    qa_immune_bridge_t* bridge,
    uint32_t event_id
);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get current phase
 *
 * @param bridge Quantum annealing immune bridge
 * @return Current phase
 */
qa_immune_phase_t qa_immune_get_phase(const qa_immune_bridge_t* bridge);

/**
 * @brief Get current inflammation level
 *
 * @param bridge Quantum annealing immune bridge
 * @return Current inflammation level
 */
brain_inflammation_level_t qa_immune_get_inflammation(const qa_immune_bridge_t* bridge);

/**
 * @brief Get current temperature factor
 *
 * @param bridge Quantum annealing immune bridge
 * @return Current temperature multiplier
 */
float qa_immune_get_temp_factor(const qa_immune_bridge_t* bridge);

/**
 * @brief Get current iteration factor
 *
 * @param bridge Quantum annealing immune bridge
 * @return Current iteration multiplier
 */
float qa_immune_get_iter_factor(const qa_immune_bridge_t* bridge);

/**
 * @brief Get cytokine effects
 *
 * @param bridge Quantum annealing immune bridge
 * @param effects Output: cytokine effects
 * @return 0 on success
 */
int qa_immune_get_cytokine_effects(
    const qa_immune_bridge_t* bridge,
    qa_immune_cytokine_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Quantum annealing immune bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int qa_immune_get_stats(
    const qa_immune_bridge_t* bridge,
    qa_immune_stats_t* stats
);

/**
 * @brief Get current metrics
 *
 * @param bridge Quantum annealing immune bridge
 * @param metrics Output current metrics
 * @return 0 on success
 */
int qa_immune_get_current_metrics(
    const qa_immune_bridge_t* bridge,
    qa_immune_metrics_t* metrics
);

/**
 * @brief Get problem event by ID
 *
 * @param bridge Quantum annealing immune bridge
 * @param event_id Event ID
 * @return Event or NULL if not found
 */
const qa_immune_problem_event_t* qa_immune_get_event(
    const qa_immune_bridge_t* bridge,
    uint32_t event_id
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* qa_immune_phase_to_string(qa_immune_phase_t phase);
const char* qa_problem_type_to_string(qa_problem_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_ANNEALING_IMMUNE_BRIDGE_H */

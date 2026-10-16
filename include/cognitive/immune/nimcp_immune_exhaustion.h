/**
 * @file nimcp_immune_exhaustion.h
 * @brief Immune Exhaustion Modeling - T Cell Functional Decline
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Models progressive T cell exhaustion during chronic immune activation
 * WHY:  Prevents indefinite high-intensity immune response, models biological
 *       fatigue, enables recovery mechanisms
 * HOW:  Tracks activation duration, models progressive loss of effector function,
 *       implements checkpoint blockade for exhaustion reversal
 *
 * BIOLOGICAL BASIS:
 * ```
 * CHRONIC ANTIGEN EXPOSURE → T CELL EXHAUSTION
 * ─────────────────────────────────────────────────────────────────────
 * Normal:   Antigen → T cell activation → effector function → memory
 * Chronic:  Persistent antigen → prolonged activation → exhaustion
 *
 * PROGRESSIVE DYSFUNCTION:
 * Stage 1: Early exhaustion  → Loss of IL-2 production (proliferation)
 * Stage 2: Intermediate      → Loss of TNF-α (co-stimulation)
 * Stage 3: Advanced          → Loss of IFN-γ (killing capacity)
 * Stage 4: Terminal          → Complete dysfunction, cell death
 *
 * INHIBITORY RECEPTORS (exhaustion markers):
 * - PD-1 (Programmed Death-1):     Primary exhaustion marker
 * - LAG-3 (Lymphocyte Activating): Negative regulator
 * - TIM-3 (T cell Immunoglobulin): Late-stage exhaustion
 * - CTLA-4, TIGIT, 2B4:            Additional checkpoint receptors
 *
 * CHECKPOINT BLOCKADE:
 * Anti-PD-1 antibodies (e.g., pembrolizumab) can partially reverse
 * exhaustion by blocking inhibitory signals, restoring some effector
 * function. This is the basis of cancer immunotherapy.
 * ```
 *
 * NIMCP MAPPING:
 * ```
 * BIOLOGICAL CONCEPT          NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * T cell activation          → brain_t_cell_t activation tracking
 * Chronic exposure           → Prolonged antigen_id binding
 * IL-2/TNF/IFN-γ loss        → Effector capacity reduction (0.0-1.0)
 * PD-1/LAG-3/TIM-3           → Exhaustion marker levels (0.0-1.0)
 * Effector function          → Killing capacity, cytokine production
 * Checkpoint blockade        → exhaustion_checkpoint_blockade()
 * Recovery/rest              → exhaustion_initiate_recovery()
 * System fatigue             → Aggregate exhaustion across all cells
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │           IMMUNE EXHAUSTION TRACKING SYSTEM                     │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │              T CELL EXHAUSTION STATES                       │ │
 * │  │                                                             │ │
 * │  │  NAIVE ────> EFFECTOR ────> EXHAUSTED ────> TERMINAL       │ │
 * │  │    │            │               │              │            │ │
 * │  │    │            └──> MEMORY     │              │            │ │
 * │  │    │                            │              │            │ │
 * │  │    │                            ↓              ↓            │ │
 * │  │    │                        RECOVERY      APOPTOSIS         │ │
 * │  │    │                            │                           │ │
 * │  │    └────────────────────────────┘                           │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                                                                  │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │       PROGRESSIVE FUNCTIONAL DECLINE                        │ │
 * │  │                                                             │ │
 * │  │  Effector Capacity: 1.0 ───────────────────────> 0.0       │ │
 * │  │                                                             │ │
 * │  │  Time:   0h ─── 6h ─── 12h ─── 24h ─── 48h+               │ │
 * │  │  Markers:                                                   │ │
 * │  │    PD-1:   0.0 ─ 0.3 ── 0.6 ─── 0.9 ─── 1.0               │ │
 * │  │    LAG-3:  0.0 ─ 0.2 ── 0.5 ─── 0.8 ─── 1.0               │ │
 * │  │    TIM-3:  0.0 ─ 0.1 ── 0.3 ─── 0.6 ─── 1.0               │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                                                                  │
 * │  ┌────────────────────────────────────────────────────────────┐ │
 * │  │         CHECKPOINT BLOCKADE THERAPY                         │ │
 * │  │                                                             │ │
 * │  │  Exhausted Cell + Anti-PD-1 → Partial Recovery             │ │
 * │  │                                                             │ │
 * │  │  Recovery Rate: ~50-70% of original function               │ │
 * │  │  Duration: Gradual over 6-12 hours                         │ │
 * │  └────────────────────────────────────────────────────────────┘ │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * USAGE EXAMPLE:
 * ```c
 * // Create exhaustion tracking system
 * exhaustion_config_t config;
 * exhaustion_default_config(&config);
 * exhaustion_system_t* exh = exhaustion_create(&config, brain_immune);
 *
 * // During immune response updates
 * exhaustion_update(exh, delta_ms);
 *
 * // Check T cell state
 * exhaustion_state_t state = exhaustion_get_cell_state(exh, t_cell_id);
 * float capacity = exhaustion_get_effector_capacity(exh, t_cell_id);
 *
 * // If exhausted, consider recovery or checkpoint blockade
 * if (state == EXHAUSTION_STATE_EXHAUSTED) {
 *     // Natural recovery (requires rest)
 *     exhaustion_initiate_recovery(exh, t_cell_id);
 *
 *     // Or therapeutic intervention
 *     exhaustion_checkpoint_blockade(exh, t_cell_id);
 * }
 *
 * // Monitor system-wide fatigue
 * float system_fatigue = exhaustion_get_system_fatigue(exh);
 * if (system_fatigue > 0.7f) {
 *     // Reduce immune system intensity
 * }
 * ```
 *
 * DESIGN PATTERNS:
 * - Observer: Monitors brain immune system T cell activations
 * - State Machine: Models T cell state transitions
 * - Strategy: Pluggable recovery strategies
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

#ifndef NIMCP_IMMUNE_EXHAUSTION_H
#define NIMCP_IMMUNE_EXHAUSTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Integration with brain immune system */
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define EXHAUSTION_MAX_CELLS            512   /**< Max tracked T cells */
#define EXHAUSTION_EARLY_THRESHOLD_MS   21600000  /**< 6 hours */
#define EXHAUSTION_ADVANCED_THRESHOLD_MS 86400000 /**< 24 hours */
#define EXHAUSTION_TERMINAL_THRESHOLD_MS 172800000 /**< 48 hours */
#define EXHAUSTION_RECOVERY_DURATION_MS  43200000  /**< 12 hours */
#define EXHAUSTION_CHECKPOINT_RECOVERY   0.65f     /**< 65% function restoration */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct exhaustion_system exhaustion_system_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief T cell exhaustion states
 *
 * BIOLOGICAL BASIS:
 * T cells progress from functional effector state to exhausted state
 * during chronic antigen exposure. Terminal exhaustion leads to apoptosis.
 */
typedef enum {
    EXHAUSTION_STATE_NAIVE = 0,      /**< Naive T cell, no activation */
    EXHAUSTION_STATE_EFFECTOR,       /**< Active effector, full function */
    EXHAUSTION_STATE_MEMORY,         /**< Memory T cell, resting state */
    EXHAUSTION_STATE_EXHAUSTED,      /**< Exhausted, reduced function */
    EXHAUSTION_STATE_TERMINAL,       /**< Terminal exhaustion, minimal function */
    EXHAUSTION_STATE_RECOVERING      /**< Recovery in progress */
} exhaustion_state_t;

/**
 * @brief Recovery strategy types
 */
typedef enum {
    RECOVERY_STRATEGY_NATURAL = 0,   /**< Natural rest-based recovery */
    RECOVERY_STRATEGY_CHECKPOINT,    /**< Checkpoint blockade (anti-PD-1) */
    RECOVERY_STRATEGY_COMBINED       /**< Combined therapy */
} exhaustion_recovery_strategy_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Exhaustion markers for a single T cell
 *
 * BIOLOGICAL BASIS:
 * Exhausted T cells express multiple inhibitory receptors.
 * Higher levels indicate deeper exhaustion.
 */
typedef struct {
    float pd1_level;      /**< PD-1 expression (0.0-1.0) */
    float lag3_level;     /**< LAG-3 expression (0.0-1.0) */
    float tim3_level;     /**< TIM-3 expression (0.0-1.0) */
    float composite;      /**< Weighted composite score (0.0-1.0) */
} exhaustion_markers_t;

/**
 * @brief Functional capacity metrics
 *
 * BIOLOGICAL BASIS:
 * Exhaustion causes progressive loss of cytokine production and
 * killing capacity in hierarchical order.
 */
typedef struct {
    float il2_production;     /**< IL-2 production (0.0-1.0) - lost first */
    float tnf_production;     /**< TNF production (0.0-1.0) - lost second */
    float ifng_production;    /**< IFN-γ production (0.0-1.0) - lost last */
    float killing_capacity;   /**< Cytotoxic capacity (0.0-1.0) */
    float overall_capacity;   /**< Weighted overall capacity (0.0-1.0) */
} exhaustion_functional_capacity_t;

/**
 * @brief Per-cell exhaustion tracking
 *
 * Tracks exhaustion state for individual T cells in the brain immune system.
 */
typedef struct {
    uint32_t t_cell_id;                /**< Linked T cell ID */
    bool active;                       /**< Tracking active */

    /* State */
    exhaustion_state_t state;          /**< Current exhaustion state */
    exhaustion_state_t previous_state; /**< Previous state (for transitions) */

    /* Timing */
    uint64_t activation_start_ms;      /**< When T cell first activated */
    uint64_t total_activation_ms;      /**< Cumulative activation time */
    uint64_t last_update_ms;           /**< Last update timestamp */
    uint64_t recovery_start_ms;        /**< When recovery initiated (0=none) */

    /* Exhaustion markers */
    exhaustion_markers_t markers;      /**< Inhibitory receptor levels */

    /* Functional capacity */
    exhaustion_functional_capacity_t capacity;  /**< Effector functions */

    /* Recovery */
    bool recovering;                   /**< Recovery in progress */
    exhaustion_recovery_strategy_t recovery_strategy;  /**< Recovery method */
    float recovery_progress;           /**< Recovery completion (0.0-1.0) */

    /* Statistics */
    uint32_t exhaustion_cycles;        /**< Times exhausted */
    uint32_t recovery_cycles;          /**< Times recovered */
} exhaustion_cell_record_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Exhaustion system configuration
 */
typedef struct {
    /* Thresholds (milliseconds) */
    uint64_t early_exhaustion_threshold_ms;    /**< Early exhaustion onset */
    uint64_t advanced_exhaustion_threshold_ms; /**< Advanced exhaustion */
    uint64_t terminal_exhaustion_threshold_ms; /**< Terminal exhaustion */

    /* Recovery parameters */
    uint64_t natural_recovery_duration_ms;     /**< Natural recovery time */
    float checkpoint_blockade_efficacy;        /**< Checkpoint blockade % */
    bool allow_auto_recovery;                  /**< Auto-initiate recovery */

    /* Functional decline rates */
    float il2_decline_rate;            /**< IL-2 loss rate (per hour) */
    float tnf_decline_rate;            /**< TNF loss rate (per hour) */
    float ifng_decline_rate;           /**< IFN-γ loss rate (per hour) */

    /* Marker accumulation rates */
    float pd1_accumulation_rate;       /**< PD-1 increase rate (per hour) */
    float lag3_accumulation_rate;      /**< LAG-3 increase rate (per hour) */
    float tim3_accumulation_rate;      /**< TIM-3 increase rate (per hour) */

    /* System parameters */
    size_t max_tracked_cells;          /**< Max T cells to track */
    bool enable_logging;               /**< Enable debug logging */
} exhaustion_config_t;

/**
 * @brief System-wide exhaustion statistics
 */
typedef struct {
    /* Cell counts by state */
    uint32_t naive_cells;
    uint32_t effector_cells;
    uint32_t memory_cells;
    uint32_t exhausted_cells;
    uint32_t terminal_cells;
    uint32_t recovering_cells;

    /* Activity */
    uint64_t total_exhaustion_events;
    uint64_t total_recovery_events;
    uint64_t checkpoint_blockade_uses;

    /* Metrics */
    float avg_effector_capacity;      /**< Average capacity across all cells */
    float system_fatigue;              /**< Overall system exhaustion (0.0-1.0) */
    float avg_activation_duration_ms; /**< Average activation time */
} exhaustion_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for T cell exhaustion event
 */
typedef void (*exhaustion_event_cb_t)(
    exhaustion_system_t* system,
    uint32_t t_cell_id,
    exhaustion_state_t old_state,
    exhaustion_state_t new_state,
    void* user_data
);

/**
 * @brief Callback for recovery completion
 */
typedef void (*exhaustion_recovery_cb_t)(
    exhaustion_system_t* system,
    uint32_t t_cell_id,
    float recovered_capacity,
    void* user_data
);

/* ============================================================================
 * Main System Structure
 * ============================================================================ */

/**
 * @brief Exhaustion tracking system state
 */
struct exhaustion_system {
    exhaustion_config_t config;        /**< Configuration */

    /* Integration */
    brain_immune_system_t* immune_system;  /**< Linked brain immune system */

    /* Cell tracking */
    exhaustion_cell_record_t* cells;   /**< Cell records */
    size_t cell_count;                 /**< Active records */
    size_t cell_capacity;              /**< Array capacity */

    /* Callbacks */
    exhaustion_event_cb_t on_exhaustion;
    exhaustion_recovery_cb_t on_recovery;
    void* callback_user_data;

    /* Statistics */
    exhaustion_stats_t stats;

    /* Thread safety */
    void* mutex;                       /**< Platform mutex */

    /* State */
    bool running;                      /**< System active */
    uint64_t start_time_ms;            /**< System start time */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default exhaustion parameters
 * WHY:  Easy initialization with biologically-grounded defaults
 * HOW:  Return struct with tuned thresholds and rates
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int exhaustion_default_config(exhaustion_config_t* config);

/**
 * @brief Create exhaustion tracking system
 *
 * WHAT: Initialize exhaustion monitoring for T cells
 * WHY:  Track and manage T cell functional decline
 * HOW:  Allocate tracking records, link to immune system
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system to monitor
 * @return New exhaustion system or NULL on failure
 */
exhaustion_system_t* exhaustion_create(
    const exhaustion_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy exhaustion system
 *
 * WHAT: Clean up exhaustion tracking resources
 * WHY:  Proper resource deallocation
 * HOW:  Free records, detach from immune system
 *
 * @param system System to destroy
 */
void exhaustion_destroy(exhaustion_system_t* system);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update exhaustion state for all tracked cells
 *
 * WHAT: Process exhaustion progression and recovery
 * WHY:  Advance exhaustion state machine based on activation duration
 * HOW:  Update markers, capacity, check state transitions
 *
 * @param system Exhaustion system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int exhaustion_update(
    exhaustion_system_t* system,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get exhaustion state for a T cell
 *
 * WHAT: Retrieve current exhaustion state
 * WHY:  Check if T cell is functional or exhausted
 * HOW:  Lookup cell record by ID
 *
 * @param system Exhaustion system
 * @param t_cell_id T cell ID from brain immune system
 * @return Exhaustion state (NAIVE if not tracked)
 */
exhaustion_state_t exhaustion_get_cell_state(
    exhaustion_system_t* system,
    uint32_t t_cell_id
);

/**
 * @brief Get effector capacity for a T cell
 *
 * WHAT: Retrieve functional capacity (0.0 = none, 1.0 = full)
 * WHY:  Determine how effective T cell is at killing/signaling
 * HOW:  Return weighted capacity from cell record
 *
 * @param system Exhaustion system
 * @param t_cell_id T cell ID from brain immune system
 * @return Capacity (0.0-1.0), or 1.0 if not tracked
 */
float exhaustion_get_effector_capacity(
    exhaustion_system_t* system,
    uint32_t t_cell_id
);

/**
 * @brief Get exhaustion markers for a T cell
 *
 * WHAT: Retrieve PD-1/LAG-3/TIM-3 levels
 * WHY:  Check exhaustion marker expression
 * HOW:  Copy markers from cell record
 *
 * @param system Exhaustion system
 * @param t_cell_id T cell ID
 * @param markers Output markers
 * @return 0 on success, -1 if not tracked
 */
int exhaustion_get_markers(
    exhaustion_system_t* system,
    uint32_t t_cell_id,
    exhaustion_markers_t* markers
);

/**
 * @brief Get system-wide fatigue level
 *
 * WHAT: Compute aggregate exhaustion across all T cells
 * WHY:  Monitor overall immune system health/fatigue
 * HOW:  Weighted average of exhaustion levels
 *
 * @param system Exhaustion system
 * @return Fatigue level (0.0 = fresh, 1.0 = totally exhausted)
 */
float exhaustion_get_system_fatigue(exhaustion_system_t* system);

/**
 * @brief Get exhaustion statistics
 *
 * @param system Exhaustion system
 * @param stats Output statistics
 * @return 0 on success
 */
int exhaustion_get_stats(
    exhaustion_system_t* system,
    exhaustion_stats_t* stats
);

/* ============================================================================
 * Recovery API
 * ============================================================================ */

/**
 * @brief Initiate natural recovery for a T cell
 *
 * WHAT: Begin rest-based recovery from exhaustion
 * WHY:  Allow T cell to restore function through rest
 * HOW:  Transition to RECOVERING state, gradual capacity restoration
 *
 * BIOLOGICAL BASIS:
 * Removing antigen stimulation allows exhausted T cells to partially
 * recover function over time (similar to rest after intense exercise).
 *
 * @param system Exhaustion system
 * @param t_cell_id T cell to recover
 * @return 0 on success, -1 if not exhausted or not tracked
 */
int exhaustion_initiate_recovery(
    exhaustion_system_t* system,
    uint32_t t_cell_id
);

/**
 * @brief Apply checkpoint blockade therapy
 *
 * WHAT: Reverse exhaustion via PD-1 blockade (immunotherapy)
 * WHY:  Rapidly restore T cell function in severe exhaustion
 * HOW:  Reduce PD-1 markers, boost capacity to ~65% of original
 *
 * BIOLOGICAL BASIS:
 * Anti-PD-1 antibodies (pembrolizumab, nivolumab) block PD-1 receptor,
 * preventing inhibitory signals and partially restoring T cell function.
 * This is the mechanism behind checkpoint inhibitor cancer therapy.
 *
 * @param system Exhaustion system
 * @param t_cell_id T cell to treat
 * @return 0 on success, -1 if not exhausted or not tracked
 */
int exhaustion_checkpoint_blockade(
    exhaustion_system_t* system,
    uint32_t t_cell_id
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set exhaustion event callback
 */
int exhaustion_set_exhaustion_callback(
    exhaustion_system_t* system,
    exhaustion_event_cb_t callback,
    void* user_data
);

/**
 * @brief Set recovery completion callback
 */
int exhaustion_set_recovery_callback(
    exhaustion_system_t* system,
    exhaustion_recovery_cb_t callback,
    void* user_data
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* exhaustion_state_to_string(exhaustion_state_t state);
const char* exhaustion_recovery_strategy_to_string(exhaustion_recovery_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMMUNE_EXHAUSTION_H */

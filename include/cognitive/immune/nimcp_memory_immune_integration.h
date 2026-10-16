/**
 * @file nimcp_memory_immune_integration.h
 * @brief Brain Immune System Integration with Memory Modules
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integrates brain immune system with working memory and consolidation,
 *       modeling how inflammation and cytokines affect cognitive memory processes.
 * WHY:  Biological immune responses significantly impact memory formation and
 *       retrieval. Inflammation impairs working memory capacity, cytokines
 *       modulate encoding strength, and sleep/consolidation are critical for
 *       both immune and cognitive memory formation.
 * HOW:  Provides coordination layer that:
 *       - Monitors immune state (inflammation, cytokines)
 *       - Modulates working memory capacity based on inflammation
 *       - Adjusts memory encoding strength based on cytokine levels
 *       - Coordinates consolidation with immune memory formation
 *       - Enables cross-talk between immune and cognitive memory cells
 *
 * BIOLOGICAL FOUNDATIONS:
 * ```
 * BIOLOGICAL PHENOMENON           NIMCP IMPLEMENTATION
 * ────────────────────────────────────────────────────────────────────
 * Inflammation → WM impairment   → Reduce capacity from 7±2 to 3±1
 * Cytokines → encoding strength  → IL-1β boosts, TNF-α impairs encoding
 * Sleep → immune memory          → Consolidation strengthens immune memories
 * Immune-cognitive cross-talk    → Shared memory cell representations
 * Stress cytokines → forgetting  → Increase decay rate in working memory
 * Resolution → capacity restore  → Return to baseline 7±2 capacity
 * ```
 *
 * THEORETICAL BASIS:
 * - Yirmiya & Goshen (2011): Immune modulation of learning and memory
 * - McAfoose & Baune (2009): Evidence for a cytokine model of cognitive function
 * - Gibbs et al. (2008): Interleukin-1 in hippocampal memory processes
 * - Barrientos et al. (2009): IL-1β impairs hippocampal long-term potentiation
 * - Besedovsky et al. (2019): Sleep and immune function
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║              MEMORY-IMMUNE INTEGRATION LAYER                          ║
 * ╠═══════════════════════════════════════════════════════════════════════╣
 * ║                                                                        ║
 * ║   ┌────────────────────────────────────────────────────────────────┐  ║
 * ║   │               IMMUNE STATE MONITORING                           │  ║
 * ║   │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐ │  ║
 * ║   │  │ Inflammation │  │  Cytokine    │  │  Immune Phase       │ │  ║
 * ║   │  │    Level     │  │Concentrations│  │   Detection         │ │  ║
 * ║   │  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘ │  ║
 * ║   │         │                 │                      │             │  ║
 * ║   │         └─────────────────┼──────────────────────┘             │  ║
 * ║   │                           ▼                                    │  ║
 * ║   │                   [State Analysis]                             │  ║
 * ║   └───────────────────────────┬────────────────────────────────────┘  ║
 * ║                               │                                       ║
 * ║        ┌──────────────────────┼──────────────────────┐                ║
 * ║        │                      │                      │                ║
 * ║        ▼                      ▼                      ▼                ║
 * ║   ┌──────────┐         ┌──────────┐          ┌──────────────┐        ║
 * ║   │ WORKING  │         │ ENCODING │          │CONSOLIDATION │        ║
 * ║   │  MEMORY  │         │ STRENGTH │          │   CONTROL    │        ║
 * ║   │ CAPACITY │         │ MODULATE │          │              │        ║
 * ║   │          │         │          │          │              │        ║
 * ║   │ 7±2 → 3±1│         │ ±IL-1β   │          │ Sleep + Immune│       ║
 * ║   │ (inflame)│         │ ±TNF-α   │          │ Memory Form  │        ║
 * ║   └──────────┘         └──────────┘          └──────────────┘        ║
 * ║                                                                        ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * INTEGRATION FEATURES:
 * 1. Working Memory Modulation:
 *    - Inflammation reduces capacity (7±2 → 5±2 → 3±1)
 *    - Pro-inflammatory cytokines increase decay rate
 *    - Resolution restores normal capacity
 *
 * 2. Encoding Strength Modulation:
 *    - IL-1β (low dose): Enhances encoding
 *    - IL-1β (high dose): Impairs encoding
 *    - TNF-α: Generally impairs encoding
 *    - IL-6: Context-dependent effects
 *    - IL-10: Protective, anti-inflammatory
 *
 * 3. Consolidation Integration:
 *    - Sleep-like consolidation forms immune memories
 *    - Immune memory cells stored alongside cognitive patterns
 *    - Memory B cells → pattern memory
 *    - Memory T cells → threat context memory
 *
 * 4. Cross-Talk:
 *    - Shared memory representations
 *    - Cognitive stress activates immune response
 *    - Immune activation affects cognitive performance
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

#ifndef NIMCP_MEMORY_IMMUNE_INTEGRATION_H
#define NIMCP_MEMORY_IMMUNE_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Memory module integrations */
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/memory/nimcp_wm_transfer.h"

/* Immune system integration */
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MEMORY_IMMUNE_MODULE_NAME            "memory_immune_integration"

/* Working memory capacity modulation */
#define WM_CAPACITY_BASELINE                 7      /**< Normal capacity (7±2) */
#define WM_CAPACITY_MILD_INFLAMMATION        5      /**< Mild inflammation */
#define WM_CAPACITY_MODERATE_INFLAMMATION    4      /**< Moderate inflammation */
#define WM_CAPACITY_SEVERE_INFLAMMATION      3      /**< Severe inflammation */

/* Encoding strength modulation ranges */
#define ENCODING_BOOST_IL1_LOW               1.3f   /**< IL-1β low dose boost */
#define ENCODING_IMPAIR_IL1_HIGH             0.6f   /**< IL-1β high dose impair */
#define ENCODING_IMPAIR_TNF                  0.7f   /**< TNF-α impairment */
#define ENCODING_BOOST_IL10                  1.2f   /**< IL-10 protective boost */

/* Decay rate modulation */
#define DECAY_MULTIPLIER_INFLAMED            2.0f   /**< 2x faster decay */
#define DECAY_MULTIPLIER_RESOLVED            0.8f   /**< 0.8x slower decay */

/* Consolidation boost during immune memory formation */
#define CONSOLIDATION_IMMUNE_MEMORY_BOOST    1.5f   /**< Boost during immune memory */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct memory_immune_integration memory_immune_integration_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Memory-immune interaction state
 */
typedef enum {
    MEM_IMMUNE_NORMAL = 0,          /**< Normal cognitive function */
    MEM_IMMUNE_ENHANCED,            /**< Enhanced encoding (mild cytokines) */
    MEM_IMMUNE_IMPAIRED,            /**< Impaired function (inflammation) */
    MEM_IMMUNE_RECOVERING,          /**< Recovery phase */
    MEM_IMMUNE_STORM                /**< Cytokine storm (severe impairment) */
} memory_immune_state_t;

/**
 * @brief Cytokine effect on memory
 */
typedef enum {
    CYTOKINE_EFFECT_ENHANCE = 0,    /**< Enhances memory encoding */
    CYTOKINE_EFFECT_IMPAIR,         /**< Impairs memory encoding */
    CYTOKINE_EFFECT_NEUTRAL,        /**< No significant effect */
    CYTOKINE_EFFECT_BIPHASIC        /**< Dose-dependent (low=enhance, high=impair) */
} cytokine_memory_effect_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Configuration for memory-immune integration
 */
typedef struct {
    /* Working memory modulation */
    bool enable_wm_capacity_modulation;     /**< Modulate WM capacity */
    bool enable_wm_decay_modulation;        /**< Modulate decay rate */
    float inflammation_threshold_mild;      /**< Threshold for mild effects */
    float inflammation_threshold_moderate;  /**< Threshold for moderate effects */
    float inflammation_threshold_severe;    /**< Threshold for severe effects */

    /* Encoding modulation */
    bool enable_encoding_modulation;        /**< Modulate encoding strength */
    float il1_low_dose_threshold;           /**< IL-1β low dose cutoff */
    float il1_high_dose_threshold;          /**< IL-1β high dose cutoff */
    float tnf_impairment_threshold;         /**< TNF-α impairment cutoff */

    /* Consolidation integration */
    bool enable_consolidation_integration;  /**< Integrate with consolidation */
    bool form_immune_memories_during_consolidation; /**< Form immune memories */
    float immune_memory_priority;           /**< Priority for immune patterns */

    /* Cross-talk */
    bool enable_immune_cognitive_crosstalk; /**< Enable bidirectional effects */
    bool cognitive_stress_activates_immune; /**< Stress activates immune */

    /* Thresholds */
    float cytokine_storm_threshold;         /**< Storm detection threshold */
    float recovery_cytokine_threshold;      /**< Recovery detection threshold */

    /* Timing */
    uint64_t state_update_interval_ms;      /**< How often to update state */
    uint64_t consolidation_check_interval_ms; /**< Consolidation check interval */

    /* Logging */
    bool enable_logging;                    /**< Enable integration logging */
} memory_immune_config_t;

/**
 * @brief Current memory-immune interaction metrics
 */
typedef struct {
    memory_immune_state_t state;            /**< Current state */

    /* Working memory effects */
    uint32_t current_wm_capacity;           /**< Current WM capacity */
    uint32_t baseline_wm_capacity;          /**< Normal WM capacity */
    float wm_capacity_ratio;                /**< Ratio (current/baseline) */
    float wm_decay_multiplier;              /**< Decay rate multiplier */

    /* Encoding effects */
    float encoding_strength_multiplier;     /**< Encoding strength multiplier */
    float il1_concentration;                /**< Current IL-1β level */
    float tnf_concentration;                /**< Current TNF-α level */
    float il6_concentration;                /**< Current IL-6 level */
    float il10_concentration;               /**< Current IL-10 level */

    /* Consolidation effects */
    float consolidation_boost;              /**< Consolidation boost factor */
    bool immune_memory_formation_active;    /**< Forming immune memories */
    uint32_t immune_memories_formed;        /**< Count of immune memories */

    /* Inflammation tracking */
    brain_inflammation_level_t inflammation_level; /**< Current inflammation */
    uint32_t active_inflammation_sites;     /**< Number of inflamed regions */

    /* Phase tracking */
    brain_immune_phase_t immune_phase;      /**< Current immune phase */
} memory_immune_metrics_t;

/**
 * @brief Statistics for memory-immune integration
 */
typedef struct {
    /* State transitions */
    uint64_t state_changes;                 /**< Total state transitions */
    uint64_t impairment_episodes;           /**< Times entered impaired state */
    uint64_t enhancement_episodes;          /**< Times entered enhanced state */
    uint64_t storm_episodes;                /**< Cytokine storm episodes */

    /* Working memory */
    uint64_t wm_capacity_reductions;        /**< Times capacity reduced */
    uint64_t wm_capacity_restorations;      /**< Times capacity restored */
    float avg_wm_capacity_ratio;            /**< Average capacity ratio */
    float min_wm_capacity_observed;         /**< Minimum capacity observed */

    /* Encoding */
    uint64_t encoding_enhancements;         /**< Times encoding enhanced */
    uint64_t encoding_impairments;          /**< Times encoding impaired */
    float avg_encoding_multiplier;          /**< Average encoding multiplier */

    /* Consolidation */
    uint64_t consolidations_performed;      /**< Total consolidations */
    uint64_t immune_memories_consolidated;  /**< Immune patterns consolidated */
    float avg_consolidation_boost;          /**< Average consolidation boost */

    /* Timing */
    uint64_t total_updates;                 /**< Total state updates */
    float avg_update_time_ms;               /**< Average update time */
} memory_immune_stats_t;

/**
 * @brief Immune memory cell - cognitive memory association
 *
 * Links immune memory cells (B/T cells) with cognitive memory patterns
 * for cross-system memory integration.
 */
typedef struct {
    uint32_t immune_cell_id;                /**< B or T cell ID */
    bool is_b_cell;                         /**< true=B cell, false=T cell */

    /* Cognitive memory reference */
    char pattern_name[128];                 /**< Associated cognitive pattern */
    float pattern_importance;               /**< Pattern importance score */

    /* Memory metadata */
    uint64_t formation_timestamp;           /**< When memory formed */
    uint32_t reactivation_count;            /**< Times reactivated */
    float memory_strength;                  /**< Memory strength (0-1) */

    /* Context */
    brain_antigen_t* triggering_antigen;    /**< Antigen that triggered formation */
} immune_cognitive_memory_link_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for working memory capacity changes
 */
typedef void (*memory_immune_wm_capacity_cb_t)(
    memory_immune_integration_t* integration,
    uint32_t old_capacity,
    uint32_t new_capacity,
    brain_inflammation_level_t inflammation,
    void* user_data
);

/**
 * @brief Callback for encoding strength changes
 */
typedef void (*memory_immune_encoding_cb_t)(
    memory_immune_integration_t* integration,
    float old_multiplier,
    float new_multiplier,
    brain_cytokine_type_t dominant_cytokine,
    void* user_data
);

/**
 * @brief Callback for immune memory formation
 */
typedef void (*memory_immune_memory_formed_cb_t)(
    memory_immune_integration_t* integration,
    const immune_cognitive_memory_link_t* memory_link,
    void* user_data
);

/**
 * @brief Callback for state changes
 */
typedef void (*memory_immune_state_change_cb_t)(
    memory_immune_integration_t* integration,
    memory_immune_state_t old_state,
    memory_immune_state_t new_state,
    void* user_data
);

/* ============================================================================
 * Main Integration Structure
 * ============================================================================ */

/**
 * @brief Memory-immune integration state
 */
struct memory_immune_integration {
    memory_immune_config_t config;          /**< Configuration */
    memory_immune_state_t state;            /**< Current state */

    /* Module handles */
    brain_immune_system_t* immune_system;   /**< Brain immune system */
    working_memory_t* working_memory;       /**< Working memory */
    consolidation_handle_t consolidation;   /**< Consolidation handle */
    engram_system_t* engram_system;         /**< Engram system (memory traces) */
    semantic_memory_system_t* semantic_memory; /**< Semantic memory network */
    systems_consolidation_system_t* systems_consolidation; /**< Hippocampus→cortex */
    wm_transfer_system_t* wm_transfer;      /**< WM→LTM transfer */

    /* Current metrics */
    memory_immune_metrics_t metrics;        /**< Current metrics */

    /* Memory links */
    immune_cognitive_memory_link_t* memory_links; /**< Memory associations */
    size_t memory_link_count;               /**< Number of links */
    size_t memory_link_capacity;            /**< Link array capacity */

    /* Callbacks */
    memory_immune_wm_capacity_cb_t on_wm_capacity_change;
    memory_immune_encoding_cb_t on_encoding_change;
    memory_immune_memory_formed_cb_t on_memory_formed;
    memory_immune_state_change_cb_t on_state_change;
    void* callback_user_data;

    /* Statistics */
    memory_immune_stats_t stats;

    /* State tracking */
    uint64_t last_update_time;              /**< Last state update */
    uint64_t last_consolidation_check;      /**< Last consolidation check */

    /* Thread safety */
    void* mutex;                            /**< Platform mutex */

    /* Status */
    bool running;                           /**< Integration active */
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
int memory_immune_default_config(memory_immune_config_t* config);

/**
 * @brief Create memory-immune integration
 *
 * WHAT: Initialize integration layer between immune and memory systems
 * WHY:  Enable biological immune-memory interactions
 * HOW:  Allocate structures, register callbacks, start monitoring
 *
 * @param immune_system Brain immune system
 * @param working_memory Working memory system (can be NULL)
 * @param consolidation Consolidation handle (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Integration instance or NULL on failure
 */
memory_immune_integration_t* memory_immune_integration_create(
    brain_immune_system_t* immune_system,
    working_memory_t* working_memory,
    consolidation_handle_t consolidation,
    const memory_immune_config_t* config
);

/**
 * @brief Destroy memory-immune integration
 *
 * WHAT: Clean up integration resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister callbacks, free memory, destroy mutex
 *
 * @param integration Integration to destroy
 */
void memory_immune_integration_destroy(memory_immune_integration_t* integration);

/**
 * @brief Start integration monitoring
 *
 * WHAT: Activate immune-memory monitoring
 * WHY:  Begin modulation of memory based on immune state
 * HOW:  Register immune callbacks, start periodic updates
 *
 * @param integration Integration instance
 * @return 0 on success
 */
int memory_immune_integration_start(memory_immune_integration_t* integration);

/**
 * @brief Stop integration
 *
 * WHAT: Deactivate integration
 * WHY:  Graceful shutdown
 * HOW:  Unregister callbacks, complete pending operations
 *
 * @param integration Integration instance
 * @return 0 on success
 */
int memory_immune_integration_stop(memory_immune_integration_t* integration);

/* ============================================================================
 * Working Memory Modulation API
 * ============================================================================ */

/**
 * @brief Update working memory capacity based on inflammation
 *
 * WHAT: Adjust WM capacity based on current inflammation level
 * WHY:  Inflammation impairs working memory (biological)
 * HOW:  Query inflammation → Map to capacity → Update WM config
 *
 * BIOLOGICAL BASIS:
 * - Inflammation → cytokine release → prefrontal dysfunction
 * - Impaired sustained attention → fewer items maintained
 * - Resolution → capacity restoration
 *
 * @param integration Integration instance
 * @return New capacity or 0 on error
 */
uint32_t memory_immune_update_wm_capacity(
    memory_immune_integration_t* integration
);

/**
 * @brief Update working memory decay rate based on cytokines
 *
 * WHAT: Modulate WM decay based on pro/anti-inflammatory balance
 * WHY:  Stress cytokines accelerate forgetting
 * HOW:  Compute cytokine balance → Adjust decay multiplier
 *
 * @param integration Integration instance
 * @return Decay multiplier or 1.0 on error
 */
float memory_immune_update_wm_decay_rate(
    memory_immune_integration_t* integration
);

/* ============================================================================
 * Encoding Strength Modulation API
 * ============================================================================ */

/**
 * @brief Compute encoding strength multiplier
 *
 * WHAT: Calculate memory encoding strength based on cytokine profile
 * WHY:  Cytokines modulate synaptic plasticity and LTP
 * HOW:  Analyze IL-1β, TNF-α, IL-6, IL-10 levels → Compute multiplier
 *
 * BIOLOGICAL BASIS:
 * - IL-1β (low): Enhances LTP and memory encoding
 * - IL-1β (high): Impairs LTP and memory encoding
 * - TNF-α: Generally impairs encoding
 * - IL-10: Protective, counters pro-inflammatory effects
 *
 * @param integration Integration instance
 * @return Encoding multiplier (0.5 - 1.5) or 1.0 on error
 */
float memory_immune_compute_encoding_strength(
    memory_immune_integration_t* integration
);

/**
 * @brief Apply cytokine modulation to salience
 *
 * WHAT: Modulate item salience for working memory based on cytokines
 * WHY:  Cytokines affect attention and importance encoding
 * HOW:  base_salience × encoding_strength_multiplier
 *
 * @param integration Integration instance
 * @param base_salience Base salience value
 * @return Modulated salience
 */
float memory_immune_modulate_salience(
    memory_immune_integration_t* integration,
    float base_salience
);

/* ============================================================================
 * Consolidation Integration API
 * ============================================================================ */

/**
 * @brief Trigger consolidation with immune memory formation
 *
 * WHAT: Run consolidation that also forms immune memory cells
 * WHY:  Sleep consolidates both cognitive and immune memories
 * HOW:  Run standard consolidation + create immune memory links
 *
 * BIOLOGICAL BASIS:
 * - Sleep enhances immune memory formation (Besedovsky, 2019)
 * - Memory B/T cells form during rest periods
 * - Shared consolidation mechanisms
 *
 * @param integration Integration instance
 * @return 0 on success
 */
int memory_immune_consolidate_with_immune_memory(
    memory_immune_integration_t* integration
);

/**
 * @brief Check if consolidation should be boosted
 *
 * WHAT: Determine if immune state warrants consolidation boost
 * WHY:  Immune memory formation benefits from enhanced consolidation
 * HOW:  Check immune phase and memory cell formation activity
 *
 * @param integration Integration instance
 * @return Consolidation boost factor (1.0 - 2.0)
 */
float memory_immune_get_consolidation_boost(
    const memory_immune_integration_t* integration
);

/* ============================================================================
 * Immune Memory Integration API
 * ============================================================================ */

/**
 * @brief Create immune-cognitive memory link
 *
 * WHAT: Associate immune memory cell with cognitive pattern
 * WHY:  Enable cross-system memory integration
 * HOW:  Create link structure, store in array
 *
 * @param integration Integration instance
 * @param immune_cell_id B or T cell ID
 * @param is_b_cell true for B cell, false for T cell
 * @param pattern_name Associated cognitive pattern
 * @param importance Pattern importance score
 * @return 0 on success
 */
int memory_immune_create_memory_link(
    memory_immune_integration_t* integration,
    uint32_t immune_cell_id,
    bool is_b_cell,
    const char* pattern_name,
    float importance
);

/**
 * @brief Reactivate cognitive pattern when immune memory reactivates
 *
 * WHAT: Trigger cognitive recall when immune memory cell activates
 * WHY:  Cross-talk between immune and cognitive memory
 * HOW:  Find link → Boost pattern importance → Trigger recall
 *
 * @param integration Integration instance
 * @param immune_cell_id Reactivated immune cell
 * @return 0 on success, -1 if no link found
 */
int memory_immune_reactivate_linked_pattern(
    memory_immune_integration_t* integration,
    uint32_t immune_cell_id
);

/**
 * @brief Get immune-cognitive memory links
 *
 * WHAT: Retrieve all memory associations
 * WHY:  Inspect cross-system memory integrations
 * HOW:  Return link array and count
 *
 * @param integration Integration instance
 * @param count Output: number of links
 * @return Array of links (read-only)
 */
const immune_cognitive_memory_link_t* memory_immune_get_memory_links(
    const memory_immune_integration_t* integration,
    size_t* count
);

/* ============================================================================
 * State Management API
 * ============================================================================ */

/**
 * @brief Update integration state
 *
 * WHAT: Process current immune state and update memory modulation
 * WHY:  Maintain synchronization with immune system
 * HOW:  Query immune state → Analyze → Update WM/encoding/consolidation
 *
 * @param integration Integration instance
 * @param current_time_ms Current timestamp
 * @return 0 on success
 */
int memory_immune_update_state(
    memory_immune_integration_t* integration,
    uint64_t current_time_ms
);

/**
 * @brief Get current memory-immune state
 *
 * WHAT: Return current interaction state
 * WHY:  Monitor overall system state
 * HOW:  Return state enum
 *
 * @param integration Integration instance
 * @return Current state
 */
memory_immune_state_t memory_immune_get_state(
    const memory_immune_integration_t* integration
);

/**
 * @brief Get current metrics
 *
 * WHAT: Retrieve detailed interaction metrics
 * WHY:  Monitor all modulation parameters
 * HOW:  Copy metrics structure
 *
 * @param integration Integration instance
 * @param metrics Output metrics
 * @return 0 on success
 */
int memory_immune_get_metrics(
    const memory_immune_integration_t* integration,
    memory_immune_metrics_t* metrics
);

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve integration statistics
 * WHY:  Monitor long-term behavior
 * HOW:  Copy stats structure
 *
 * @param integration Integration instance
 * @param stats Output statistics
 * @return 0 on success
 */
int memory_immune_get_stats(
    const memory_immune_integration_t* integration,
    memory_immune_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero stats structure
 *
 * @param integration Integration instance
 */
void memory_immune_reset_stats(memory_immune_integration_t* integration);

/* ============================================================================
 * Engram System Integration API
 * ============================================================================ */

/**
 * @brief Connect to engram system
 *
 * WHAT: Link integration to engram (memory trace) system
 * WHY:  Enable immune modulation of memory consolidation and retrieval
 * HOW:  Store engram system handle
 *
 * @param integration Integration instance
 * @param engram_system Engram system handle
 * @return 0 on success
 */
int memory_immune_connect_engram_system(
    memory_immune_integration_t* integration,
    engram_system_t* engram_system
);

/**
 * @brief Modulate engram consolidation based on IL-1β levels
 *
 * WHAT: Adjust engram consolidation strength based on cytokine levels
 * WHY:  IL-1β has biphasic effect on hippocampal LTP and consolidation
 * HOW:  Low IL-1β enhances, high IL-1β impairs consolidation rate
 *
 * BIOLOGICAL BASIS:
 * - IL-1β (low): Enhances hippocampal LTP, facilitates consolidation
 * - IL-1β (high): Impairs LTP, blocks protein synthesis, impairs consolidation
 * - Inflammation: Disrupts sleep-dependent consolidation
 *
 * @param integration Integration instance
 * @param dt Time step for consolidation
 * @param is_sleeping Currently in sleep state
 * @return Modulated consolidation strength multiplier
 */
float memory_immune_modulate_engram_consolidation(
    memory_immune_integration_t* integration,
    float dt,
    bool is_sleeping
);

/**
 * @brief Modulate engram retrieval based on inflammation
 *
 * WHAT: Adjust engram recall confidence based on inflammation level
 * WHY:  Inflammation impairs hippocampal-dependent memory retrieval
 * HOW:  Reduce recall confidence and increase latency during inflammation
 *
 * BIOLOGICAL BASIS:
 * - Acute inflammation impairs hippocampal memory retrieval
 * - Pro-inflammatory cytokines reduce recall accuracy
 * - Resolution restores normal retrieval
 *
 * @param integration Integration instance
 * @param base_confidence Base recall confidence
 * @return Modulated recall confidence (0.0-1.0)
 */
float memory_immune_modulate_engram_retrieval(
    memory_immune_integration_t* integration,
    float base_confidence
);

/**
 * @brief Check if immune memory matches engram pattern
 *
 * WHAT: Determine if current threat matches stored engram pattern
 * WHY:  Cognitive memory of threats triggers faster immune response
 * HOW:  Compare antigen epitope with engram neuron patterns
 *
 * BIOLOGICAL BASIS:
 * - Cognitive threat memory can prime immune responses
 * - Learned fear activates immune preparedness
 * - Cross-talk between cognitive and immune memory
 *
 * @param integration Integration instance
 * @param antigen_id Antigen to match
 * @param engram_id Output: matching engram ID (0 if no match)
 * @param affinity Output: match strength (0.0-1.0)
 * @return 0 on success, -1 if no match
 */
int memory_immune_check_threat_memory_in_engrams(
    memory_immune_integration_t* integration,
    uint32_t antigen_id,
    uint64_t* engram_id,
    float* affinity
);

/**
 * @brief Trigger immune response from engram reactivation
 *
 * WHAT: Activate immune response when threat-related engram is recalled
 * WHY:  Memory of past threats primes faster immune response
 * HOW:  Engram recall → Check for threat association → Boost immune activation
 *
 * BIOLOGICAL BASIS:
 * - Conditioned immune responses (Ader & Cohen, 1975)
 * - Learned immune enhancement
 * - Cognitive-immune bidirectional communication
 *
 * @param integration Integration instance
 * @param engram_id Reactivated engram
 * @return 0 on success, -1 if not threat-related
 */
int memory_immune_trigger_from_engram_recall(
    memory_immune_integration_t* integration,
    uint64_t engram_id
);

/* ============================================================================
 * Semantic Memory Integration API
 * ============================================================================ */

/**
 * @brief Connect to semantic memory system
 *
 * WHAT: Link integration to semantic memory network
 * WHY:  Enable immune concepts in semantic knowledge base
 * HOW:  Store semantic memory system handle
 *
 * @param integration Integration instance
 * @param semantic_memory Semantic memory system
 * @return 0 on success
 */
int memory_immune_connect_semantic_memory(
    memory_immune_integration_t* integration,
    semantic_memory_system_t* semantic_memory
);

/**
 * @brief Create semantic concept from immune memory
 *
 * WHAT: Convert immune memory cell (B/T cell) to semantic concept
 * WHY:  Abstract threat patterns into semantic knowledge
 * HOW:  Extract features from immune receptor → Create semantic concept
 *
 * BIOLOGICAL BASIS:
 * - Abstraction of threat patterns
 * - Generalization across similar threats
 * - Conceptual immune memory
 *
 * @param integration Integration instance
 * @param immune_cell_id B or T cell memory ID
 * @param is_b_cell true for B cell, false for T cell
 * @param concept_id Output: created concept ID
 * @return 0 on success
 */
int memory_immune_create_semantic_immune_concept(
    memory_immune_integration_t* integration,
    uint32_t immune_cell_id,
    bool is_b_cell,
    uint64_t* concept_id
);

/**
 * @brief Query semantic memory for similar threats
 *
 * WHAT: Find semantically similar threat concepts
 * WHY:  Enable generalization and cross-reactive immunity
 * HOW:  Query semantic network with threat features
 *
 * @param integration Integration instance
 * @param antigen_id Current antigen
 * @param max_results Maximum similar concepts to return
 * @param concept_ids Output: similar concept IDs
 * @param similarities Output: similarity scores
 * @return Number of similar concepts found
 */
uint32_t memory_immune_query_semantic_threats(
    memory_immune_integration_t* integration,
    uint32_t antigen_id,
    uint32_t max_results,
    uint64_t* concept_ids,
    float* similarities
);

/* ============================================================================
 * Systems Consolidation Integration API
 * ============================================================================ */

/**
 * @brief Connect to systems consolidation system
 *
 * WHAT: Link integration to hippocampus→cortex transfer system
 * WHY:  Enable immune modulation of sleep-dependent consolidation
 * HOW:  Store systems consolidation handle
 *
 * @param integration Integration instance
 * @param systems_consolidation Systems consolidation system
 * @return 0 on success
 */
int memory_immune_connect_systems_consolidation(
    memory_immune_integration_t* integration,
    systems_consolidation_system_t* systems_consolidation
);

/**
 * @brief Modulate sleep replay rate based on immune state
 *
 * WHAT: Adjust hippocampal replay frequency during sleep
 * WHY:  Inflammation disrupts sleep-dependent consolidation
 * HOW:  Reduce replay rate during high inflammation
 *
 * BIOLOGICAL BASIS:
 * - Inflammation disrupts slow-wave sleep
 * - Cytokines alter replay dynamics
 * - IL-1β affects hippocampal replay
 *
 * @param integration Integration instance
 * @param base_replay_rate Base replay frequency (Hz)
 * @return Modulated replay rate (Hz)
 */
float memory_immune_modulate_replay_rate(
    memory_immune_integration_t* integration,
    float base_replay_rate
);

/**
 * @brief Modulate hippocampus→cortex transfer strength
 *
 * WHAT: Adjust systems consolidation transfer rate based on IL-1β
 * WHY:  IL-1β affects both hippocampal and cortical plasticity
 * HOW:  Low IL-1β enhances, high IL-1β impairs transfer
 *
 * BIOLOGICAL BASIS:
 * - IL-1β modulates hippocampal-cortical communication
 * - Inflammation impairs systems consolidation
 * - Sleep deprivation (often with inflammation) impairs transfer
 *
 * @param integration Integration instance
 * @param base_transfer_rate Base transfer rate
 * @return Modulated transfer rate
 */
float memory_immune_modulate_systems_transfer(
    memory_immune_integration_t* integration,
    float base_transfer_rate
);

/**
 * @brief Prioritize immune-related memories for consolidation
 *
 * WHAT: Boost consolidation priority for threat-related memories
 * WHY:  Survival-relevant memories preferentially consolidated
 * HOW:  Increase replay priority for immune-tagged engrams
 *
 * @param integration Integration instance
 * @param engram_id Engram to check
 * @return Priority boost factor (1.0-2.0)
 */
float memory_immune_get_consolidation_priority_boost(
    memory_immune_integration_t* integration,
    uint64_t engram_id
);

/* ============================================================================
 * Working Memory Transfer Integration API
 * ============================================================================ */

/**
 * @brief Connect to WM transfer system
 *
 * WHAT: Link integration to working memory→long-term memory transfer
 * WHY:  Enable immune modulation of encoding and transfer criteria
 * HOW:  Store WM transfer system handle
 *
 * @param integration Integration instance
 * @param wm_transfer WM transfer system
 * @return 0 on success
 */
int memory_immune_connect_wm_transfer(
    memory_immune_integration_t* integration,
    wm_transfer_system_t* wm_transfer
);

/**
 * @brief Modulate WM transfer criteria based on immune state
 *
 * WHAT: Adjust thresholds for WM→engram transfer
 * WHY:  Inflammation affects encoding selectivity
 * HOW:  Increase rehearsal/attention thresholds during inflammation
 *
 * BIOLOGICAL BASIS:
 * - Inflammation increases noise-to-signal ratio
 * - Higher thresholds compensate for impaired encoding
 * - Selective transfer preserves important memories
 *
 * @param integration Integration instance
 * @param base_criteria Base transfer criteria
 * @param modulated_criteria Output: modulated criteria
 * @return 0 on success
 */
int memory_immune_modulate_transfer_criteria(
    memory_immune_integration_t* integration,
    const wm_transfer_criteria_t* base_criteria,
    wm_transfer_criteria_t* modulated_criteria
);

/**
 * @brief Boost transfer of threat-related WM items
 *
 * WHAT: Prioritize transfer of immune-salient working memory items
 * WHY:  Threat-related information preferentially encoded
 * HOW:  Reduce transfer thresholds for immune-tagged items
 *
 * @param integration Integration instance
 * @param wm_slot Working memory slot
 * @param is_threat_related Is item threat-related
 * @return Transfer priority boost (1.0-2.0)
 */
float memory_immune_get_transfer_priority(
    memory_immune_integration_t* integration,
    uint32_t wm_slot,
    bool is_threat_related
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set working memory capacity change callback
 */
int memory_immune_set_wm_capacity_callback(
    memory_immune_integration_t* integration,
    memory_immune_wm_capacity_cb_t callback,
    void* user_data
);

/**
 * @brief Set encoding strength change callback
 */
int memory_immune_set_encoding_callback(
    memory_immune_integration_t* integration,
    memory_immune_encoding_cb_t callback,
    void* user_data
);

/**
 * @brief Set memory formation callback
 */
int memory_immune_set_memory_formed_callback(
    memory_immune_integration_t* integration,
    memory_immune_memory_formed_cb_t callback,
    void* user_data
);

/**
 * @brief Set state change callback
 */
int memory_immune_set_state_change_callback(
    memory_immune_integration_t* integration,
    memory_immune_state_change_cb_t callback,
    void* user_data
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* memory_immune_state_to_string(memory_immune_state_t state);
const char* cytokine_memory_effect_to_string(cytokine_memory_effect_t effect);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_IMMUNE_INTEGRATION_H */

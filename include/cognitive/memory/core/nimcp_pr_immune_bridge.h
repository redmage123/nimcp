//=============================================================================
// nimcp_pr_immune_bridge.h - Prime Resonant Immune Bridge
//=============================================================================
/**
 * @file nimcp_pr_immune_bridge.h
 * @brief Integration between Prime Resonant memory and brain immune system
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bridge connecting Prime Resonant memory (quaternion states, entanglement,
 *       resonance) with brain immune system (cytokines, inflammation, immune cells)
 * WHY:  Biological evidence shows immune-memory coupling: inflammation affects
 *       memory consolidation, immune system tags corrupted memories for cleanup,
 *       and sleep coordinates immune-memory interactions
 * HOW:  Bidirectional integration where:
 *       - Cytokine levels modulate quaternion consolidation (w) and accessibility (z)
 *       - Memory decay triggers immune cleanup (tagging weak memories)
 *       - Inflammation reduces memory formation capacity
 *       - Sleep coordinates immune consolidation of memories
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Immune → Memory Pathways:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  Pro-inflammatory Cytokines (IL-1β, TNF-α, IL-6):                     |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  - Impair hippocampal LTP and memory consolidation              |   |
 *   |  |  - Reduce quaternion.w (consolidation strength)                 |   |
 *   |  |  - Increase memory decay rate in Z0/Z1 tiers                    |   |
 *   |  |  Reference: Yirmiya & Goshen (2011) "Immune modulation of       |   |
 *   |  |             learning and memory"                                 |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  Anti-inflammatory Cytokines (IL-10):                                 |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  - Facilitate memory consolidation during resolution            |   |
 *   |  |  - Increase quaternion.w as inflammation subsides               |   |
 *   |  |  - Promote memory protection and tier promotion                 |   |
 *   |  |  Reference: Donzis & Bhattacharya (2017) "IL-10 and Memory"    |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  Chronic Inflammation:                                                 |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  - Persistent impairment of memory formation                    |   |
 *   |  |  - Reduced quaternion.z (accessibility)                         |   |
 *   |  |  - Accelerated memory decay across all tiers                    |   |
 *   |  |  Reference: Sartori et al. (2012) "Inflammation and dementia"  |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Memory → Immune Pathways:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  Memory Decay and Cleanup:                                             |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  - Weak memories (low strength) tagged for immune cleanup       |   |
 *   |  |  - Microglia-like cleanup of decayed memory nodes               |   |
 *   |  |  - Corrupted signatures trigger antibody-like responses         |   |
 *   |  |  Biological analog: Synaptic pruning by microglia               |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  Memory Corruption Detection:                                          |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  - Invalid quaternion states detected as "antigens"             |   |
 *   |  |  - Signature anomalies trigger immune response                  |   |
 *   |  |  - Corrupted entanglements isolated and removed                 |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Sleep-Immune-Memory Coordination:
 *   +-----------------------------------------------------------------------+
 *   |                                                                        |
 *   |  Deep NREM Sleep:                                                      |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  - Enhanced memory consolidation (quaternion.w increase)        |   |
 *   |  |  - Immune-memory synchronization                                |   |
 *   |  |  - Tier promotion eligibility boost                             |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   |  REM Sleep:                                                            |
 *   |  +----------------------------------------------------------------+   |
 *   |  |  - Memory replay and immune tagging                             |   |
 *   |  |  - Weak memory identification for cleanup                       |   |
 *   |  |  - Entanglement graph pruning                                   |   |
 *   |  +----------------------------------------------------------------+   |
 *   |                                                                        |
 *   +-----------------------------------------------------------------------+
 *
 *   Cytokine → Quaternion Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  Cytokine  | Quaternion Effect         | Biological Basis             |
 *   |------------|---------------------------|------------------------------|
 *   |  IL-1β     | Reduce w, z               | Impairs LTP, consolidation   |
 *   |  TNF-α     | Reduce w, increase decay  | Disrupts synaptic plasticity |
 *   |  IL-6      | Reduce z (accessibility)  | Impairs retrieval            |
 *   |  IL-10     | Increase w (resolution)   | Facilitates consolidation    |
 *   |  IFN-γ     | Tag for review            | Immune surveillance          |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Modulation update: ~100ns per memory node
 * - Tag for cleanup: ~50ns
 * - Cytokine to quaternion: ~30ns
 * - Full bridge update: ~2ms for 10K nodes
 *
 * MEMORY:
 * - pr_immune_bridge_t: ~4KB base + tag buffer
 * - Per-node overhead: ~16 bytes (cleanup state)
 *
 * INTEGRATION:
 * - Core: PR memory nodes, quaternion state, entanglement graph
 * - Immune: Brain immune system, cytokines, inflammation
 * - Sleep: Sleep-wake cycle, sleep stages
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_IMMUNE_BRIDGE_H
#define NIMCP_PR_IMMUNE_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Prime Resonant core dependencies */
#include "nimcp_pr_memory_node.h"
#include "nimcp_quaternion.h"

/* Brain immune system */
#include "cognitive/immune/nimcp_brain_immune.h"

/* Sleep-wake system */
#include "cognitive/nimcp_sleep_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Export macro (for shared library builds) */
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of memory nodes to track for cleanup */
#define PR_IMMUNE_MAX_CLEANUP_QUEUE         4096

/** Maximum number of tagged memories per update cycle */
#define PR_IMMUNE_MAX_TAGS_PER_CYCLE        256

/** Default memory strength threshold for cleanup tagging */
#define PR_IMMUNE_CLEANUP_STRENGTH_THRESHOLD  0.1f

/** Default decay rate multiplier during inflammation */
#define PR_IMMUNE_INFLAMMATION_DECAY_MULT     2.0f

/** Default consolidation reduction from IL-1β */
#define PR_IMMUNE_IL1_CONSOLIDATION_REDUCTION  0.15f

/** Default consolidation reduction from TNF-α */
#define PR_IMMUNE_TNF_CONSOLIDATION_REDUCTION  0.20f

/** Default accessibility reduction from IL-6 */
#define PR_IMMUNE_IL6_ACCESSIBILITY_REDUCTION  0.10f

/** Default consolidation boost from IL-10 */
#define PR_IMMUNE_IL10_CONSOLIDATION_BOOST     0.15f

/** Deep sleep consolidation boost factor */
#define PR_IMMUNE_DEEP_SLEEP_CONSOLIDATION     0.25f

/** REM sleep cleanup efficiency factor */
#define PR_IMMUNE_REM_CLEANUP_EFFICIENCY       0.30f

/** Minimum quaternion component value */
#define PR_IMMUNE_QUAT_MIN                     0.0f

/** Maximum quaternion component value */
#define PR_IMMUNE_QUAT_MAX                     1.0f

/** Inflammation level thresholds */
#define PR_IMMUNE_INFLAMMATION_MILD_THRESHOLD  0.3f
#define PR_IMMUNE_INFLAMMATION_HIGH_THRESHOLD  0.6f
#define PR_IMMUNE_INFLAMMATION_SEVERE_THRESHOLD 0.8f

/** Sleep stage consolidation multipliers */
#define PR_IMMUNE_SLEEP_AWAKE_MULT             1.0f
#define PR_IMMUNE_SLEEP_LIGHT_MULT             1.2f
#define PR_IMMUNE_SLEEP_DEEP_MULT              1.8f
#define PR_IMMUNE_SLEEP_REM_MULT               1.1f

/** Numerical epsilon */
#define PR_IMMUNE_EPSILON                      1e-6f

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Cleanup tag reason
 *
 * WHAT: Reason a memory was tagged for immune cleanup
 * WHY:  Different reasons may require different cleanup actions
 */
typedef enum {
    PR_CLEANUP_REASON_NONE = 0,        /**< Not tagged */
    PR_CLEANUP_REASON_DECAY,           /**< Strength decayed below threshold */
    PR_CLEANUP_REASON_CORRUPTION,      /**< Quaternion state corrupted */
    PR_CLEANUP_REASON_SIGNATURE,       /**< Signature anomaly detected */
    PR_CLEANUP_REASON_ORPHAN,          /**< No entanglements remaining */
    PR_CLEANUP_REASON_INFLAMMATION,    /**< Chronic inflammation damage */
    PR_CLEANUP_REASON_MANUAL           /**< Manually tagged */
} pr_cleanup_reason_t;

/**
 * @brief Inflammation impact level
 *
 * WHAT: How severely inflammation affects memory operations
 * WHY:  Different inflammation levels have different effects
 */
typedef enum {
    PR_INFLAMMATION_NONE = 0,          /**< No inflammation impact */
    PR_INFLAMMATION_MILD,              /**< Mild impact (10-20% reduction) */
    PR_INFLAMMATION_MODERATE,          /**< Moderate impact (30-50% reduction) */
    PR_INFLAMMATION_SEVERE,            /**< Severe impact (60-80% reduction) */
    PR_INFLAMMATION_CRITICAL           /**< Critical (>80% reduction) */
} pr_inflammation_impact_t;

/**
 * @brief Sleep-immune-memory phase
 *
 * WHAT: Current phase of sleep-immune-memory coordination
 * WHY:  Different phases have different consolidation behaviors
 */
typedef enum {
    PR_SIM_PHASE_AWAKE = 0,            /**< Awake - normal operation */
    PR_SIM_PHASE_LIGHT_SLEEP,          /**< Light sleep - mild consolidation */
    PR_SIM_PHASE_DEEP_SLEEP,           /**< Deep NREM - strong consolidation */
    PR_SIM_PHASE_REM_SLEEP,            /**< REM - replay and cleanup */
    PR_SIM_PHASE_TRANSITION            /**< Transitioning between phases */
} pr_sim_phase_t;

/**
 * @brief Memory cleanup tag
 *
 * WHAT: Tag marking a memory for immune cleanup
 * WHY:  Track which memories need cleanup and why
 */
typedef struct {
    uint64_t node_id;                  /**< Tagged memory node ID */
    pr_cleanup_reason_t reason;        /**< Reason for tagging */
    float strength_at_tag;             /**< Memory strength when tagged */
    uint64_t tag_time_ms;              /**< When tagged */
    bool processed;                    /**< Has been processed for cleanup */
    uint32_t cleanup_attempts;         /**< Number of cleanup attempts */
} pr_cleanup_tag_t;

/**
 * @brief Cytokine effects on memory
 *
 * WHAT: Current cytokine effects on memory operations
 * WHY:  Track how cytokines are modulating memory
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_consolidation_reduction;  /**< IL-1β consolidation reduction [0-1] */
    float tnf_consolidation_reduction;  /**< TNF-α consolidation reduction [0-1] */
    float il6_accessibility_reduction;  /**< IL-6 accessibility reduction [0-1] */
    float ifn_gamma_surveillance_level; /**< IFN-γ surveillance intensity [0-1] */

    /* Anti-inflammatory effects */
    float il10_consolidation_boost;     /**< IL-10 consolidation boost [0-1] */

    /* Aggregate effects */
    float net_consolidation_modifier;   /**< Net effect on consolidation [-1, +1] */
    float net_accessibility_modifier;   /**< Net effect on accessibility [-1, +1] */
    float decay_rate_multiplier;        /**< Memory decay rate multiplier [1.0-3.0] */

    /* Inflammation state */
    pr_inflammation_impact_t impact_level; /**< Current inflammation impact */
    bool is_chronic;                    /**< Chronic inflammation flag */
} pr_cytokine_memory_effects_t;

/**
 * @brief Sleep-immune-memory coordination state
 *
 * WHAT: Current state of sleep-immune-memory coordination
 * WHY:  Track coordinated consolidation and cleanup during sleep
 */
typedef struct {
    /* Sleep state */
    pr_sim_phase_t current_phase;       /**< Current sleep-immune-memory phase */
    uint64_t phase_duration_ms;         /**< Duration in current phase */

    /* Consolidation tracking */
    float consolidation_multiplier;     /**< Current consolidation boost [0.5-2.0] */
    uint32_t nodes_consolidated;        /**< Nodes consolidated this sleep cycle */
    uint32_t promotions_triggered;      /**< Tier promotions this cycle */

    /* Cleanup tracking */
    float cleanup_efficiency;           /**< Cleanup efficiency [0-1] */
    uint32_t nodes_cleaned;             /**< Nodes cleaned this cycle */
    uint32_t entanglements_pruned;      /**< Entanglements pruned this cycle */

    /* Immune coordination */
    bool immune_consolidation_active;   /**< Immune-coordinated consolidation active */
    float immune_sync_level;            /**< Immune-memory sync level [0-1] */
} pr_sim_coordination_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Complete configuration for immune-memory bridge
 * WHY:  Centralize all configuration options
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;    /**< Enable cytokine → quaternion effects */
    bool enable_cleanup_tagging;        /**< Enable weak memory cleanup tagging */
    bool enable_inflammation_effects;   /**< Enable inflammation → memory effects */
    bool enable_sleep_coordination;     /**< Enable sleep-immune-memory sync */
    bool enable_corruption_detection;   /**< Enable memory corruption detection */

    /* Cytokine sensitivity */
    float cytokine_sensitivity;         /**< Overall cytokine effect scale [0.5-2.0] */
    float il1_sensitivity;              /**< IL-1β specific sensitivity */
    float tnf_sensitivity;              /**< TNF-α specific sensitivity */
    float il6_sensitivity;              /**< IL-6 specific sensitivity */
    float il10_sensitivity;             /**< IL-10 specific sensitivity */

    /* Cleanup thresholds */
    float cleanup_strength_threshold;   /**< Min strength to avoid cleanup [0-1] */
    float corruption_detection_threshold; /**< Threshold for corruption detection */
    uint32_t max_cleanup_per_cycle;     /**< Max cleanups per update cycle */

    /* Sleep coordination */
    float deep_sleep_consolidation_boost; /**< Consolidation boost in deep sleep */
    float rem_cleanup_efficiency;       /**< Cleanup efficiency in REM */

    /* Event logging */
    bool enable_event_logging;          /**< Track bridge events */
    uint32_t max_events;                /**< Maximum events to store */

    /* Bio-async integration */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} pr_immune_bridge_config_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Operational metrics for the immune-memory bridge
 * WHY:  Track bridge health and performance
 */
typedef struct {
    /* Modulation counts */
    uint64_t cytokine_modulations;      /**< Total cytokine modulations */
    uint64_t inflammation_effects;      /**< Inflammation effect applications */
    uint64_t consolidation_boosts;      /**< Consolidation boosts applied */
    uint64_t accessibility_reductions;  /**< Accessibility reductions applied */

    /* Cleanup counts */
    uint64_t nodes_tagged;              /**< Total nodes tagged for cleanup */
    uint64_t nodes_cleaned;             /**< Total nodes cleaned */
    uint64_t tags_expired;              /**< Tags that expired without cleanup */
    uint64_t corruption_detected;       /**< Corruptions detected */

    /* Sleep coordination */
    uint64_t sleep_consolidations;      /**< Consolidations during sleep */
    uint64_t rem_cleanups;              /**< Cleanups during REM */
    uint64_t deep_sleep_promotions;     /**< Tier promotions during deep sleep */

    /* Current state */
    float avg_consolidation_modifier;   /**< Average consolidation modifier */
    float avg_accessibility_modifier;   /**< Average accessibility modifier */
    pr_inflammation_impact_t current_impact; /**< Current inflammation impact */

    /* Performance */
    uint64_t total_updates;             /**< Total update cycles */
    float avg_update_time_us;           /**< Average update time */
    uint32_t cleanup_queue_size;        /**< Current cleanup queue size */
} pr_immune_bridge_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct pr_immune_bridge_struct* pr_immune_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides biologically-plausible starting point
 *
 * @return Default configuration with balanced parameters
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT pr_immune_bridge_config_t pr_immune_bridge_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Check configuration for validity
 * WHY:  Prevent invalid parameters causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT bool pr_immune_bridge_config_validate(
    const pr_immune_bridge_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create immune-memory bridge
 *
 * WHAT: Initialize bridge for Prime Resonant memory and brain immune integration
 * WHY:  Entry point for immune-memory coupling
 * HOW:  Allocate state, initialize tracking, connect to immune system
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param immune_system Brain immune system to connect (can be NULL)
 * @param sleep_system Sleep-wake system for coordination (can be NULL)
 * @return Bridge handle, or NULL on failure
 *
 * Performance: O(1)
 * Memory: ~4KB base
 *
 * Thread safety: The returned bridge is thread-safe
 *
 * Example:
 *   pr_immune_bridge_config_t config = pr_immune_bridge_config_default();
 *   config.enable_sleep_coordination = true;
 *   pr_immune_bridge_t bridge = pr_immune_bridge_create(&config, immune, sleep);
 */
NIMCP_EXPORT pr_immune_bridge_t pr_immune_bridge_create(
    const pr_immune_bridge_config_t* config,
    brain_immune_system_t* immune_system,
    sleep_system_t sleep_system);

/**
 * @brief Destroy immune-memory bridge
 *
 * WHAT: Free all bridge resources
 * WHY:  Proper resource cleanup
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(1)
 */
NIMCP_EXPORT void pr_immune_bridge_destroy(pr_immune_bridge_t bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset statistics and cleanup queue
 * WHY:  Start fresh measurement period
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 *
 * Performance: O(n) where n = cleanup queue size
 */
NIMCP_EXPORT int pr_immune_bridge_reset(pr_immune_bridge_t bridge);

/**
 * @brief Connect bridge to immune system
 *
 * WHAT: Link bridge to brain immune system
 * WHY:  Enable cytokine and inflammation effects
 *
 * @param bridge Bridge instance
 * @param immune_system Brain immune system
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_immune_bridge_connect_immune(
    pr_immune_bridge_t bridge,
    brain_immune_system_t* immune_system);

/**
 * @brief Connect bridge to sleep system
 *
 * WHAT: Link bridge to sleep-wake cycle
 * WHY:  Enable sleep-immune-memory coordination
 *
 * @param bridge Bridge instance
 * @param sleep_system Sleep-wake system
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_immune_bridge_connect_sleep(
    pr_immune_bridge_t bridge,
    sleep_system_t sleep_system);

//=============================================================================
// Consolidation Modulation API
//=============================================================================

/**
 * @brief Modulate memory consolidation based on cytokines
 *
 * WHAT: Adjust quaternion.w based on current cytokine levels
 * WHY:  Pro-inflammatory cytokines impair consolidation, IL-10 enhances it
 * HOW:  Query immune system, compute modulation, update quaternion
 *
 * @param bridge Immune-memory bridge
 * @param node Memory node to modulate
 * @return Modulated quaternion state, or unchanged on error
 *
 * Effect:
 *   - High IL-1β/TNF-α → reduce quaternion.w
 *   - High IL-10 → increase quaternion.w
 *   - Chronic inflammation → sustained reduction
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT nimcp_quaternion_t pr_immune_bridge_modulate_consolidation(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node);

/**
 * @brief Apply inflammation effects to memory node
 *
 * WHAT: Apply current inflammation state to memory node
 * WHY:  Inflammation reduces consolidation and accessibility
 *
 * @param bridge Immune-memory bridge
 * @param node Memory node to affect
 * @param quat_out Output modulated quaternion
 * @return 0 on success, -1 on error
 *
 * Performance: ~80ns
 */
NIMCP_EXPORT int pr_immune_bridge_apply_inflammation(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node,
    nimcp_quaternion_t* quat_out);

/**
 * @brief Get consolidation modifier for current immune state
 *
 * WHAT: Compute consolidation modifier from cytokines
 * WHY:  Preview effect without applying
 *
 * @param bridge Immune-memory bridge
 * @return Consolidation modifier [-1, +1] (negative = impaired)
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT float pr_immune_bridge_get_consolidation_modifier(
    pr_immune_bridge_t bridge);

/**
 * @brief Get accessibility modifier for current immune state
 *
 * WHAT: Compute accessibility modifier from cytokines
 * WHY:  Preview effect without applying
 *
 * @param bridge Immune-memory bridge
 * @return Accessibility modifier [-1, +1] (negative = impaired)
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT float pr_immune_bridge_get_accessibility_modifier(
    pr_immune_bridge_t bridge);

//=============================================================================
// Cleanup Tagging API
//=============================================================================

/**
 * @brief Tag memory for immune cleanup
 *
 * WHAT: Mark memory node for cleanup by immune-like process
 * WHY:  Weak, corrupted, or orphaned memories should be cleaned
 *
 * @param bridge Immune-memory bridge
 * @param node_id Memory node ID to tag
 * @param reason Reason for cleanup tag
 * @param strength Current memory strength
 * @return 0 on success, -1 on error
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT int pr_immune_bridge_tag_for_cleanup(
    pr_immune_bridge_t bridge,
    uint64_t node_id,
    pr_cleanup_reason_t reason,
    float strength);

/**
 * @brief Check if memory is tagged for cleanup
 *
 * WHAT: Query cleanup status for a memory node
 * WHY:  Avoid double-tagging, check before operations
 *
 * @param bridge Immune-memory bridge
 * @param node_id Memory node ID to check
 * @return true if tagged for cleanup
 *
 * Performance: O(n) where n = cleanup queue size
 */
NIMCP_EXPORT bool pr_immune_bridge_is_tagged(
    pr_immune_bridge_t bridge,
    uint64_t node_id);

/**
 * @brief Get cleanup tag for memory node
 *
 * WHAT: Retrieve cleanup tag details for a node
 * WHY:  Inspect tag reason and state
 *
 * @param bridge Immune-memory bridge
 * @param node_id Memory node ID
 * @param tag_out Output tag structure
 * @return 0 on success, -1 if not tagged
 *
 * Performance: O(n) where n = cleanup queue size
 */
NIMCP_EXPORT int pr_immune_bridge_get_tag(
    pr_immune_bridge_t bridge,
    uint64_t node_id,
    pr_cleanup_tag_t* tag_out);

/**
 * @brief Remove cleanup tag from memory
 *
 * WHAT: Cancel cleanup for a memory node
 * WHY:  Memory may have been reinforced or is still needed
 *
 * @param bridge Immune-memory bridge
 * @param node_id Memory node ID
 * @return 0 on success, -1 if not tagged
 *
 * Performance: O(n) where n = cleanup queue size
 */
NIMCP_EXPORT int pr_immune_bridge_untag(
    pr_immune_bridge_t bridge,
    uint64_t node_id);

/**
 * @brief Get next memory tagged for cleanup
 *
 * WHAT: Pop next memory from cleanup queue
 * WHY:  Process cleanups in FIFO order
 *
 * @param bridge Immune-memory bridge
 * @param tag_out Output tag structure
 * @return 0 on success, -1 if queue empty
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_immune_bridge_get_next_cleanup(
    pr_immune_bridge_t bridge,
    pr_cleanup_tag_t* tag_out);

/**
 * @brief Mark cleanup as complete
 *
 * WHAT: Confirm cleanup of a tagged memory
 * WHY:  Track cleanup success for statistics
 *
 * @param bridge Immune-memory bridge
 * @param node_id Cleaned memory node ID
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_immune_bridge_cleanup_complete(
    pr_immune_bridge_t bridge,
    uint64_t node_id);

//=============================================================================
// Inflammation Processing API
//=============================================================================

/**
 * @brief Process current inflammation state
 *
 * WHAT: Update bridge with current inflammation from immune system
 * WHY:  Inflammation state affects all memory operations
 *
 * @param bridge Immune-memory bridge
 * @return 0 on success, -1 on error
 *
 * Performance: ~50ns
 */
NIMCP_EXPORT int pr_immune_bridge_process_inflammation(
    pr_immune_bridge_t bridge);

/**
 * @brief Get current inflammation impact level
 *
 * WHAT: Query current inflammation impact on memory
 * WHY:  Allow other systems to adapt to inflammation state
 *
 * @param bridge Immune-memory bridge
 * @return Current inflammation impact level
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_inflammation_impact_t pr_immune_bridge_get_inflammation_impact(
    pr_immune_bridge_t bridge);

/**
 * @brief Check if chronic inflammation is present
 *
 * WHAT: Determine if inflammation is chronic
 * WHY:  Chronic inflammation has different effects than acute
 *
 * @param bridge Immune-memory bridge
 * @return true if chronic inflammation detected
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool pr_immune_bridge_is_chronic_inflammation(
    pr_immune_bridge_t bridge);

/**
 * @brief Get decay rate multiplier from inflammation
 *
 * WHAT: Compute memory decay rate increase from inflammation
 * WHY:  Inflammation accelerates memory decay
 *
 * @param bridge Immune-memory bridge
 * @return Decay rate multiplier (1.0 = normal, >1.0 = faster decay)
 *
 * Performance: O(1)
 */
NIMCP_EXPORT float pr_immune_bridge_get_decay_multiplier(
    pr_immune_bridge_t bridge);

//=============================================================================
// Cytokine to Quaternion Mapping API
//=============================================================================

/**
 * @brief Convert cytokine levels to quaternion modulation
 *
 * WHAT: Map immune cytokine state to quaternion effect
 * WHY:  Enable cytokine-driven memory state changes
 *
 * @param bridge Immune-memory bridge
 * @param base_quat Base quaternion to modulate
 * @param modulated_out Output modulated quaternion
 * @return 0 on success, -1 on error
 *
 * Mapping:
 *   - IL-1β, TNF-α → reduce w (consolidation)
 *   - IL-6 → reduce z (accessibility)
 *   - IL-10 → increase w (during resolution)
 *
 * Performance: ~40ns
 */
NIMCP_EXPORT int pr_immune_bridge_cytokine_to_quaternion(
    pr_immune_bridge_t bridge,
    nimcp_quaternion_t base_quat,
    nimcp_quaternion_t* modulated_out);

/**
 * @brief Get current cytokine memory effects
 *
 * WHAT: Query current cytokine effects on memory
 * WHY:  Inspect individual cytokine contributions
 *
 * @param bridge Immune-memory bridge
 * @param effects_out Output effects structure
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_immune_bridge_get_cytokine_effects(
    pr_immune_bridge_t bridge,
    pr_cytokine_memory_effects_t* effects_out);

/**
 * @brief Apply specific cytokine to quaternion
 *
 * WHAT: Apply single cytokine effect to quaternion
 * WHY:  Fine-grained control over cytokine effects
 *
 * @param bridge Immune-memory bridge
 * @param cytokine Cytokine type
 * @param concentration Cytokine concentration [0-1]
 * @param base_quat Base quaternion
 * @param result_out Output modulated quaternion
 * @return 0 on success, -1 on error
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT int pr_immune_bridge_apply_cytokine(
    pr_immune_bridge_t bridge,
    brain_cytokine_type_t cytokine,
    float concentration,
    nimcp_quaternion_t base_quat,
    nimcp_quaternion_t* result_out);

//=============================================================================
// Sleep-Immune-Memory Coordination API
//=============================================================================

/**
 * @brief Coordinate sleep consolidation with immune state
 *
 * WHAT: Perform sleep-stage-appropriate memory consolidation with immune sync
 * WHY:  Sleep coordinates immune and memory consolidation
 *
 * @param bridge Immune-memory bridge
 * @param node Memory node to consolidate
 * @param quat_out Output consolidated quaternion
 * @return 0 on success, -1 on error
 *
 * Effect:
 *   - Deep NREM: Strong consolidation (quaternion.w boost)
 *   - REM: Cleanup identification
 *   - Light sleep: Mild consolidation
 *   - Awake: Normal operation
 *
 * Performance: ~150ns
 */
NIMCP_EXPORT int pr_immune_bridge_sleep_consolidation(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node,
    nimcp_quaternion_t* quat_out);

/**
 * @brief Get current sleep-immune-memory phase
 *
 * WHAT: Query current phase of sleep-immune-memory coordination
 * WHY:  Allow operations to adapt to current phase
 *
 * @param bridge Immune-memory bridge
 * @return Current SIM phase
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_sim_phase_t pr_immune_bridge_get_sim_phase(
    pr_immune_bridge_t bridge);

/**
 * @brief Get sleep-immune-memory coordination state
 *
 * WHAT: Query full coordination state
 * WHY:  Inspect consolidation and cleanup progress
 *
 * @param bridge Immune-memory bridge
 * @param coordination_out Output coordination structure
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_immune_bridge_get_sim_coordination(
    pr_immune_bridge_t bridge,
    pr_sim_coordination_t* coordination_out);

/**
 * @brief Update sleep phase from sleep system
 *
 * WHAT: Sync bridge with current sleep state
 * WHY:  Keep bridge synchronized with sleep system
 *
 * @param bridge Immune-memory bridge
 * @return 0 on success, -1 on error
 *
 * Performance: ~30ns
 */
NIMCP_EXPORT int pr_immune_bridge_sync_sleep_phase(pr_immune_bridge_t bridge);

/**
 * @brief Check if deep sleep consolidation is active
 *
 * WHAT: Query if currently in deep sleep consolidation mode
 * WHY:  Apply enhanced consolidation during deep sleep
 *
 * @param bridge Immune-memory bridge
 * @return true if deep sleep consolidation active
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool pr_immune_bridge_is_deep_sleep_consolidation(
    pr_immune_bridge_t bridge);

/**
 * @brief Check if REM cleanup mode is active
 *
 * WHAT: Query if currently in REM cleanup mode
 * WHY:  Apply enhanced cleanup during REM sleep
 *
 * @param bridge Immune-memory bridge
 * @return true if REM cleanup active
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool pr_immune_bridge_is_rem_cleanup_active(
    pr_immune_bridge_t bridge);

//=============================================================================
// Corruption Detection API
//=============================================================================

/**
 * @brief Detect memory corruption
 *
 * WHAT: Check memory node for corruption indicators
 * WHY:  Corrupted memories should be tagged for cleanup
 *
 * @param bridge Immune-memory bridge
 * @param node Memory node to check
 * @return true if corruption detected
 *
 * Checks:
 *   - Quaternion component ranges
 *   - Signature validity
 *   - State consistency
 *
 * Performance: ~60ns
 */
NIMCP_EXPORT bool pr_immune_bridge_detect_corruption(
    pr_immune_bridge_t bridge,
    const pr_memory_node_t* node);

/**
 * @brief Validate quaternion state
 *
 * WHAT: Check if quaternion is valid for memory use
 * WHY:  Invalid quaternions indicate corruption
 *
 * @param bridge Immune-memory bridge
 * @param quat Quaternion to validate
 * @return true if valid
 *
 * Performance: ~20ns
 */
NIMCP_EXPORT bool pr_immune_bridge_validate_quaternion(
    pr_immune_bridge_t bridge,
    nimcp_quaternion_t quat);

//=============================================================================
// Main Update Function
//=============================================================================

/**
 * @brief Main bridge update
 *
 * WHAT: Perform all enabled bridge operations
 * WHY:  Single entry point for regular updates
 * HOW:  Update cytokine effects, process inflammation, sync sleep phase
 *
 * @param bridge Immune-memory bridge
 * @param dt_ms Time delta since last update (milliseconds)
 * @return 0 on success, -1 on error
 *
 * Update sequence:
 * 1. Sync with immune system (cytokine levels)
 * 2. Process inflammation state
 * 3. Sync sleep phase
 * 4. Process cleanup queue (limited per cycle)
 * 5. Update statistics
 *
 * Performance: ~2ms for 10K tracked nodes
 */
NIMCP_EXPORT int pr_immune_bridge_update(
    pr_immune_bridge_t bridge,
    float dt_ms);

//=============================================================================
// Statistics and Information
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational metrics
 * WHY:  Monitoring and debugging
 *
 * @param bridge Immune-memory bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * Performance: O(1)
 */
NIMCP_EXPORT int pr_immune_bridge_get_stats(
    pr_immune_bridge_t bridge,
    pr_immune_bridge_stats_t* stats);

/**
 * @brief Get cleanup queue size
 *
 * WHAT: Query number of memories pending cleanup
 * WHY:  Monitor cleanup backlog
 *
 * @param bridge Immune-memory bridge
 * @return Current cleanup queue size
 *
 * Performance: O(1)
 */
NIMCP_EXPORT uint32_t pr_immune_bridge_get_cleanup_queue_size(
    pr_immune_bridge_t bridge);

/**
 * @brief Check if bridge is connected to immune system
 *
 * @param bridge Immune-memory bridge
 * @return true if connected
 */
NIMCP_EXPORT bool pr_immune_bridge_is_immune_connected(
    pr_immune_bridge_t bridge);

/**
 * @brief Check if bridge is connected to sleep system
 *
 * @param bridge Immune-memory bridge
 * @return true if connected
 */
NIMCP_EXPORT bool pr_immune_bridge_is_sleep_connected(
    pr_immune_bridge_t bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async messaging
 *
 * WHAT: Enable bio-async integration
 * WHY:  Cross-system coordination
 *
 * @param bridge Immune-memory bridge
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pr_immune_bridge_connect_bio_async(pr_immune_bridge_t bridge);

/**
 * @brief Disconnect from bio-async
 *
 * @param bridge Immune-memory bridge
 * @return 0 on success
 */
NIMCP_EXPORT int pr_immune_bridge_disconnect_bio_async(pr_immune_bridge_t bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Immune-memory bridge
 * @return true if connected
 */
NIMCP_EXPORT bool pr_immune_bridge_is_bio_async_connected(
    pr_immune_bridge_t bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get cleanup reason name as string
 *
 * @param reason Cleanup reason
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_cleanup_reason_name(pr_cleanup_reason_t reason);

/**
 * @brief Get inflammation impact name as string
 *
 * @param impact Inflammation impact level
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_inflammation_impact_name(pr_inflammation_impact_t impact);

/**
 * @brief Get SIM phase name as string
 *
 * @param phase Sleep-immune-memory phase
 * @return Human-readable string
 */
NIMCP_EXPORT const char* pr_sim_phase_name(pr_sim_phase_t phase);

/**
 * @brief Print bridge statistics to stdout
 *
 * @param bridge Bridge to print stats for
 */
NIMCP_EXPORT void pr_immune_bridge_print_stats(pr_immune_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_IMMUNE_BRIDGE_H */

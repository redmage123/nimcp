/**
 * @file nimcp_pattern_db_immune_bridge.h
 * @brief Pattern Database-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and pattern database
 * WHY:  Biological immune memory (memory B/T cells) maps naturally to pattern databases;
 *       immune system learns threat patterns like pattern DB learns attack signatures;
 *       inflammation affects pattern matching sensitivity
 * HOW:  Immune memory cells → pattern database entries (learned threats);
 *       pattern matches → antigen presentation (detected threats);
 *       inflammation → pattern matching threshold modulation;
 *       antibody affinity computation → pattern matching algorithms
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → PATTERN DATABASE PATHWAYS:
 * ------------------------------------
 * 1. Immune Memory as Pattern Database:
 *    - Memory B cells store antigen patterns (epitopes)
 *    - Each memory cell = learned threat signature
 *    - Maps to: Pattern DB entries for known attacks
 *    - Reference: Adaptive immunity learning and memory formation
 *
 * 2. Affinity Maturation as Pattern Refinement:
 *    - B cells undergo somatic hypermutation → better pattern recognition
 *    - High-affinity variants selected → precise pattern matching
 *    - Maps to: Pattern DB update/refinement based on effectiveness
 *    - Reference: Germinal center reactions and antibody optimization
 *
 * 3. Inflammation-Modulated Pattern Matching:
 *    - High inflammation → lowered activation thresholds
 *    - More sensitive pattern matching (like paranoid anomaly detection)
 *    - IL-10 → relaxed thresholds (reduce false positives)
 *    - Maps to: Dynamic pattern weight/priority adjustment
 *
 * 4. Cross-Reactive Immunity as Pattern Generalization:
 *    - Memory cells recognize similar (but not identical) antigens
 *    - Maps to: Pattern variants, regex fuzzy matching
 *    - Reference: Immune response to antigen variants
 *
 * PATTERN DATABASE → IMMUNE PATHWAYS:
 * ------------------------------------
 * 1. Pattern Match as Antigen Recognition:
 *    - Pattern match = epitope recognition
 *    - Match score → antigen binding affinity
 *    - Pattern category → antigen classification
 *    - Reference: TCR/BCR recognition specificity
 *
 * 2. New Patterns as Immune Learning:
 *    - Adding new pattern = forming new memory cell
 *    - Pattern effectiveness → memory cell persistence
 *    - Unused patterns → memory cell apoptosis
 *    - Reference: Immune memory maintenance and decay
 *
 * 3. Pattern Match Triggers Immune Response:
 *    - High-confidence match → present as antigen
 *    - Pattern category → determines immune response type
 *    - Repeated matches → escalate inflammation
 *    - Reference: Secondary immune response on re-exposure
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                 PATTERN DATABASE-IMMUNE BRIDGE                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → PATTERN DATABASE PATHWAYS                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ MEMORY CELLS │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ B cell mem.  │  ───────┐                                       │  ║
 * ║   │   │ T cell mem.  │         │                                       │  ║
 * ║   │   │ Antibodies   │         ├──→ Pattern Database Entries           │  ║
 * ║   │   │              │         │    (Learned Threat Signatures)        │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     PATTERN DATABASE            │                             │  ║
 * ║   │   │  - Pattern addition/update      │                             │  ║
 * ║   │   │  - Weight/priority adjustment   │                             │  ║
 * ║   │   │  - Pattern pruning (unused)     │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION EFFECTS   │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ NONE    → weight×1.0     │                                     │  ║
 * ║   │   │ LOCAL   → weight×1.1     │                                     │  ║
 * ║   │   │ REGIONAL→ weight×1.3     │                                     │  ║
 * ║   │   │ SYSTEMIC→ weight×1.5     │                                     │  ║
 * ║   │   │ STORM   → weight×2.0     │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │           PATTERN DATABASE → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  PATTERN     │ ──→ Present as Antigen                          │  ║
 * ║   │   │  MATCHED     │ ──→ Activate Immune Response                    │  ║
 * ║   │   │  (score>0.5) │ ──→ Form Memory Cell                            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  NEW PATTERN │ ──→ New Memory Formation                        │  ║
 * ║   │   │  ADDED       │ ──→ Immune Learning                             │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PATTERN      │ ──→ Antibody Affinity                           │  ║
 * ║   │   │ MATCH SCORE  │ ──→ Epitope Recognition Strength                │  ║
 * ║   │   └──────────────┘                                                 │  ║
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

#ifndef NIMCP_PATTERN_DB_IMMUNE_BRIDGE_H
#define NIMCP_PATTERN_DB_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_pattern_db.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Inflammation pattern weight modulation */
#define INFLAMMATION_NONE_WEIGHT_FACTOR     1.0f   /**< No change */
#define INFLAMMATION_LOCAL_WEIGHT_FACTOR    1.1f   /**< +10% weight */
#define INFLAMMATION_REGIONAL_WEIGHT_FACTOR 1.3f   /**< +30% weight */
#define INFLAMMATION_SYSTEMIC_WEIGHT_FACTOR 1.5f   /**< +50% weight */
#define INFLAMMATION_STORM_WEIGHT_FACTOR    2.0f   /**< 2x weight (hypervigilant) */

/* Cytokine pattern matching effects */
#define CYTOKINE_IL1_WEIGHT_BOOST      0.10f  /**< IL-1β → boost pattern weights */
#define CYTOKINE_IL6_WEIGHT_BOOST      0.15f  /**< IL-6 → boost pattern weights */
#define CYTOKINE_TNF_WEIGHT_BOOST      0.25f  /**< TNF-α → aggressive boosting */
#define CYTOKINE_IFN_GAMMA_WEIGHT_BOOST 0.12f /**< IFN-γ → moderate boost */
#define CYTOKINE_IL10_WEIGHT_REDUCTION -0.20f /**< IL-10 → reduce false positives */

/* Pattern match to immune mapping */
#define PATTERN_MATCH_ANTIGEN_THRESHOLD 0.5f  /**< Min match score for antigen */
#define PATTERN_MATCH_SEVERITY_MULTIPLIER 10.0f /**< Score → severity mapping */
#define PATTERN_MATCH_INFLAMMATION_THRESHOLD 0.8f /**< Score to trigger inflammation */

/* Memory cell to pattern mapping */
#define MEMORY_CELL_PATTERN_PRIORITY_BASE 5   /**< Base priority for immune memory patterns */
#define MEMORY_CELL_PATTERN_WEIGHT      0.9f  /**< Pattern weight for memory cells */

/* Pattern effectiveness tracking */
#define PATTERN_UNUSED_THRESHOLD_SEC    86400.0f  /**< 24 hours without match → unused */
#define PATTERN_PRUNE_UNUSED            true      /**< Auto-prune unused patterns */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on pattern matching
 *
 * Represents how cytokines modulate pattern matching sensitivity
 */
typedef struct {
    /* Pro-inflammatory effects (boost sensitivity) */
    float il1_weight_boost;             /**< IL-1β induced boost */
    float il6_weight_boost;             /**< IL-6 induced boost */
    float tnf_weight_boost;             /**< TNF-α induced boost */
    float ifn_gamma_weight_boost;       /**< IFN-γ induced boost */

    /* Anti-inflammatory effects (reduce false positives) */
    float il10_weight_reduction;        /**< IL-10 relaxation */

    /* Aggregate effects */
    float total_weight_modulation;      /**< Combined weight change [-1, +1] */
    float effective_weight_multiplier;  /**< Actual weight multiplier */
    bool hypervigilant_matching;        /**< Maximum sensitivity mode */
} pattern_db_cytokine_effects_t;

/**
 * @brief Inflammation effects on pattern database
 *
 * How systemic inflammation affects pattern matching
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< Chronic inflammation flag */

    /* Pattern matching modulation */
    float weight_multiplier;            /**< Overall weight boost */
    float priority_boost;               /**< Priority increase for all patterns */
    bool aggressive_matching;           /**< Aggressive matching mode */

    /* Mode flags */
    bool emergency_mode;                /**< Storm-level matching */
} pattern_db_inflammation_state_t;

/**
 * @brief Pattern database immune modulation
 *
 * How pattern matches affect immune system
 */
typedef struct {
    /* Pattern match tracking */
    uint32_t recent_matches;            /**< Recent pattern matches */
    uint32_t total_matches;             /**< Total matches seen */
    float max_match_score;              /**< Highest recent score */
    uint64_t last_match_time;           /**< Last match timestamp */

    /* Immune effects */
    uint32_t antigens_presented;        /**< Matches presented as antigens */
    uint32_t inflammation_triggers;     /**< Times inflammation triggered */
    uint32_t memory_formations;         /**< New memory cells from patterns */

    /* Pattern learning */
    uint32_t patterns_from_memory;      /**< Patterns learned from immune memory */
    uint32_t patterns_refined;          /**< Patterns refined from antibody affinity */
    uint32_t patterns_pruned;           /**< Unused patterns removed */
} pattern_db_immune_modulation_t;

/**
 * @brief Pattern-immune mapping entry
 *
 * Tracks relationship between pattern and immune memory
 */
typedef struct {
    nimcp_pattern_id_t pattern_id;      /**< Pattern DB ID */
    uint32_t b_cell_id;                 /**< Associated B cell (if any) */
    uint32_t antibody_id;               /**< Associated antibody (if any) */
    float affinity_score;               /**< Pattern match → antibody affinity */
    bool is_memory_derived;             /**< Pattern from immune memory */
    uint64_t last_match_time;           /**< Last successful match */
} pattern_immune_mapping_t;

/**
 * @brief Pattern database immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_weight_modulation;
    bool enable_inflammation_priority_boost;
    bool enable_pattern_match_antigen_presentation;
    bool enable_memory_cell_pattern_sync;
    bool enable_affinity_based_refinement;
    bool enable_auto_pattern_pruning;

    /* Pattern modulation config */
    float max_weight_multiplier;        /**< Max weight boost (protection) */
    float min_weight_multiplier;        /**< Min weight (prevent over-relaxation) */
    uint32_t max_patterns_from_memory;  /**< Max patterns to learn from immune memory */

    /* Antigen presentation config */
    float min_match_score_for_antigen;  /**< Min score to present */
    float severity_multiplier;          /**< Score → severity mapping */

    /* Pattern pruning config */
    bool auto_prune_unused;             /**< Remove unused patterns */
    float unused_threshold_sec;         /**< Time without match → unused */
    uint32_t min_patterns_to_keep;      /**< Min patterns (never prune below) */

    /* Learning feedback */
    bool refine_from_neutralization;    /**< Improve patterns from successful responses */
    bool learn_from_false_positives;    /**< Adjust from false alarms */
} pattern_db_immune_config_t;

/**
 * @brief Complete pattern database-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    nimcp_pattern_db_t pattern_db;

    /* Configuration */
    pattern_db_immune_config_t config;

    /* Current state */
    pattern_db_cytokine_effects_t cytokine_effects;
    pattern_db_inflammation_state_t inflammation_state;
    pattern_db_immune_modulation_t immune_modulation;

    /* Pattern-immune mappings */
    pattern_immune_mapping_t* mappings;
    size_t mapping_count;
    size_t mapping_capacity;

    /* Timing */
    uint64_t last_update_time;
    uint64_t last_prune_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t weight_modulations;
    uint32_t antigens_presented;
    uint32_t patterns_synced;
    uint32_t patterns_pruned;

    nimcp_platform_mutex_t* mutex;
} pattern_db_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int pattern_db_immune_default_config(pattern_db_immune_config_t* config);

/**
 * @brief Create pattern database-immune bridge
 *
 * WHAT: Initialize bridge between pattern DB and immune system
 * WHY:  Enable bidirectional integration
 * HOW:  Allocate state, connect modules, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param pattern_db Pattern database handle
 * @param immune_system Immune system handle
 * @return New bridge or NULL on failure
 */
pattern_db_immune_bridge_t* pattern_db_immune_create(
    const pattern_db_immune_config_t* config,
    nimcp_pattern_db_t pattern_db,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy pattern database-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect modules, free memory
 *
 * @param bridge Bridge to destroy
 */
void pattern_db_immune_destroy(pattern_db_immune_bridge_t* bridge);

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Process immune state, update pattern weights/priorities
 * WHY:  Keep pattern matching in sync with immune state
 * HOW:  Read cytokines/inflammation, modulate patterns
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int pattern_db_immune_update(pattern_db_immune_bridge_t* bridge);

/**
 * @brief Apply pattern modulation
 *
 * WHAT: Adjust pattern weights/priorities based on immune state
 * WHY:  Implement inflammation-induced sensitivity
 * HOW:  Calculate modulated weights, update pattern DB
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int pattern_db_immune_apply_modulation(pattern_db_immune_bridge_t* bridge);

/**
 * @brief Sync immune memory cells to pattern database
 *
 * WHAT: Create pattern DB entries from immune memory cells
 * WHY:  Share learned threats between immune and pattern systems
 * HOW:  Convert B cell receptors to pattern entries
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int pattern_db_immune_sync_memory_to_patterns(pattern_db_immune_bridge_t* bridge);

/**
 * @brief Present pattern match as immune antigen
 *
 * WHAT: Convert pattern match to immune antigen
 * WHY:  Enable immune response to detected threats
 * HOW:  Map match score to severity, present to immune system
 *
 * @param bridge Bridge handle
 * @param match_result Pattern match result
 * @param antigen_id Output: assigned antigen ID
 * @return 0 on success
 */
int pattern_db_immune_present_match(
    pattern_db_immune_bridge_t* bridge,
    const nimcp_pattern_match_result_t* match_result,
    uint32_t* antigen_id
);

/**
 * @brief Refine pattern from antibody affinity
 *
 * WHAT: Update pattern based on immune response effectiveness
 * WHY:  Improve pattern accuracy from immune learning
 * HOW:  High affinity neutralization → boost pattern weight
 *
 * @param bridge Bridge handle
 * @param pattern_id Pattern to refine
 * @param antibody_id Antibody that neutralized
 * @param affinity_score Antibody affinity (0-1)
 * @return 0 on success
 */
int pattern_db_immune_refine_from_affinity(
    pattern_db_immune_bridge_t* bridge,
    nimcp_pattern_id_t pattern_id,
    uint32_t antibody_id,
    float affinity_score
);

/**
 * @brief Prune unused patterns
 *
 * WHAT: Remove patterns that haven't matched recently
 * WHY:  Cleanup obsolete patterns (like memory cell apoptosis)
 * HOW:  Check last match time, remove old patterns
 *
 * @param bridge Bridge handle
 * @return Number of patterns pruned
 */
uint32_t pattern_db_immune_prune_unused(pattern_db_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging
 * WHY:  Enable inter-module communication
 * HOW:  Register module, set up handlers
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int pattern_db_immune_connect_bio_async(pattern_db_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister module
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int pattern_db_immune_disconnect_bio_async(pattern_db_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool pattern_db_immune_is_bio_async_connected(const pattern_db_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current pattern weight multiplier
 *
 * WHAT: Return current weight boost from inflammation
 * WHY:  Monitor pattern matching sensitivity
 * HOW:  Return computed weight multiplier
 *
 * @param bridge Bridge handle
 * @return Weight multiplier (1.0 = no change)
 */
float pattern_db_immune_get_weight_multiplier(const pattern_db_immune_bridge_t* bridge);

/**
 * @brief Get pattern-immune mapping
 *
 * WHAT: Retrieve immune mapping for pattern
 * WHY:  Check immune relationship
 * HOW:  Search mapping table
 *
 * @param bridge Bridge handle
 * @param pattern_id Pattern to query
 * @param mapping Output: mapping (if found)
 * @return 0 if found, -1 if not found
 */
int pattern_db_immune_get_mapping(
    const pattern_db_immune_bridge_t* bridge,
    nimcp_pattern_id_t pattern_id,
    pattern_immune_mapping_t* mapping
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PATTERN_DB_IMMUNE_BRIDGE_H */

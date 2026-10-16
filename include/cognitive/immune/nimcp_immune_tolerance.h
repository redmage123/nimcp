/**
 * @file nimcp_immune_tolerance.h
 * @brief Immune Tolerance Learning - Self vs Non-Self Discrimination
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Self-tolerance mechanisms preventing autoimmune attacks on legitimate patterns
 * WHY:  Brain immune system must learn to distinguish "self" (normal operation) from
 *       "non-self" (threats), preventing false positives and autoimmune responses
 * HOW:  Implements central tolerance (clonal deletion), peripheral tolerance (anergy),
 *       and self-pattern database for discrimination
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL MECHANISM          NIMCP IMPLEMENTATION
 * ──────────────────────────────────────────────────────────────────
 * Thymic Selection (T cells)  → Central tolerance (positive/negative selection)
 * Bone Marrow Selection (B)   → Central tolerance for B cell repertoire
 * Clonal Deletion             → Delete self-reactive cells during development
 * Anergy Induction            → Inactivate escaped self-reactive cells
 * Receptor Editing            → Modify receptor to avoid self-reactivity
 * AIRE Gene Function          → Present self-antigens during selection
 * Peripheral Tolerance        → Suppress/regulate in periphery
 * Regulatory T Cells          → Active suppression of autoimmunity
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                      TOLERANCE LEARNING SYSTEM                             ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              SELF-PATTERN DATABASE (AIRE-like)                      │  ║
 * ║   │   ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐    │  ║
 * ║   │   │ Normal Ops   │  │ Expected     │  │ Legitimate           │    │  ║
 * ║   │   │  Patterns    │  │  Behaviors   │  │  Signatures          │    │  ║
 * ║   │   └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘    │  ║
 * ║   │          │                 │                      │                │  ║
 * ║   │          └─────────────────┴──────────────────────┘                │  ║
 * ║   └────────────────────────────┬───────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 CENTRAL TOLERANCE (Thymic Selection)                │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐   │  ║
 * ║   │   │  Positive   │    │  Negative   │    │    Clonal           │   │  ║
 * ║   │   │  Selection  │    │  Selection  │    │    Deletion         │   │  ║
 * ║   │   │ ──────────  │    │ ──────────  │    │ ─────────────────   │   │  ║
 * ║   │   │ Weak self-  │    │ Strong self-│    │ Remove self-        │   │  ║
 * ║   │   │ recognition │    │ recognition │    │ reactive cells      │   │  ║
 * ║   │   │  = Pass     │    │  = Deleted  │    │ during training     │   │  ║
 * ║   │   └─────────────┘    └─────────────┘    └─────────────────────┘   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                │                                          ║
 * ║                                ▼                                          ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              PERIPHERAL TOLERANCE (Anergy/Regulation)               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────────┐    ┌─────────────┐    ┌─────────────────────┐   │  ║
 * ║   │   │   Anergy    │    │ Regulatory  │    │   Suppression       │   │  ║
 * ║   │   │ Induction   │    │   T Cells   │    │                     │   │  ║
 * ║   │   │ ──────────  │    │ ──────────  │    │ ─────────────────   │   │  ║
 * ║   │   │ Self-antigen│    │ Active      │    │ Prevent auto-       │   │  ║
 * ║   │   │ w/o co-stim │    │ suppression │    │ immune response     │   │  ║
 * ║   │   │ = Inactivate│    │ of auto-rxn │    │ in periphery        │   │  ║
 * ║   │   └─────────────┘    └─────────────┘    └─────────────────────┘   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * TOLERANCE PHASES:
 * 1. Training Phase: Register normal operation patterns as "self"
 * 2. Central Selection: Delete cells strongly reactive to self during development
 * 3. Peripheral Control: Anergize escaped self-reactive cells in operation
 * 4. Continuous Learning: Update self-patterns as system evolves
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

#ifndef NIMCP_IMMUNE_TOLERANCE_H
#define NIMCP_IMMUNE_TOLERANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TOLERANCE_MAX_SELF_PATTERNS     2048   /**< Max self patterns */
#define TOLERANCE_PATTERN_SIZE          64     /**< Self pattern size */
#define TOLERANCE_DEFAULT_THRESHOLD     0.85f  /**< Default self-recognition threshold */
#define TOLERANCE_CENTRAL_THRESHOLD     0.90f  /**< Central deletion threshold (high affinity) */
#define TOLERANCE_ANERGY_THRESHOLD      0.70f  /**< Anergy induction threshold (medium affinity) */
#define TOLERANCE_MODULE_NAME           "immune_tolerance"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct tolerance_system tolerance_system_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Tolerance phase - developmental vs operational
 *
 * BIOLOGICAL BASIS:
 * Central tolerance occurs during T/B cell development (thymus/bone marrow).
 * Peripheral tolerance occurs after cells enter circulation.
 */
typedef enum {
    TOLERANCE_PHASE_TRAINING = 0,  /**< Training phase - learning self patterns */
    TOLERANCE_PHASE_CENTRAL,       /**< Central tolerance - negative selection */
    TOLERANCE_PHASE_PERIPHERAL,    /**< Peripheral tolerance - anergy/regulation */
    TOLERANCE_PHASE_OPERATIONAL    /**< Normal operation with tolerance checks */
} tolerance_phase_t;

/**
 * @brief Selection outcome for cells during central tolerance
 *
 * BIOLOGICAL BASIS:
 * Positive selection: weak self-recognition = survive
 * Negative selection: strong self-recognition = death
 */
typedef enum {
    SELECTION_PASS = 0,            /**< Cell survives selection */
    SELECTION_DELETE,              /**< Clonal deletion (strong self-reactivity) */
    SELECTION_EDIT,                /**< Receptor editing (modify to avoid self) */
    SELECTION_ANERGIZE             /**< Induce anergy (inactivate without deletion) */
} selection_outcome_t;

/**
 * @brief Cell tolerance state
 */
typedef enum {
    CELL_TOLERANT = 0,             /**< Cell is tolerant (not self-reactive) */
    CELL_ANERGIC,                  /**< Cell is anergic (inactivated) */
    CELL_DELETED,                  /**< Cell deleted (marked for removal) */
    CELL_EDITED                    /**< Cell receptor edited */
} cell_tolerance_state_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Self pattern - represents normal operation signature
 *
 * WHAT: Stored pattern representing legitimate system behavior
 * WHY:  Immune system must recognize these as "self" to avoid autoimmunity
 * HOW:  Pattern matching during antigen presentation
 */
typedef struct {
    uint32_t id;                           /**< Unique pattern ID */
    uint8_t pattern[TOLERANCE_PATTERN_SIZE]; /**< Self signature */
    size_t pattern_len;                    /**< Pattern length */

    float match_threshold;                 /**< Required match strength (0-1) */
    uint32_t presentation_count;           /**< Times presented during training */
    uint64_t registered_time;              /**< When registered */
    uint64_t last_matched;                 /**< Last time matched */

    char description[128];                 /**< Human-readable description */
    bool immutable;                        /**< Cannot be removed */
    float confidence;                      /**< Confidence this is self (0-1) */
} self_pattern_t;

/**
 * @brief Anergic cell record - inactivated self-reactive cell
 *
 * WHAT: Record of cell that recognized self and was anergized
 * WHY:  Track peripheral tolerance without deleting cells
 * HOW:  Cell remains but cannot respond to antigen
 */
typedef struct {
    uint32_t cell_id;                      /**< B or T cell ID */
    bool is_b_cell;                        /**< true=B cell, false=T cell */
    uint32_t self_pattern_id;              /**< Matched self pattern */
    float affinity;                        /**< Self-affinity that triggered anergy */
    uint64_t anergy_time;                  /**< When anergized */
    bool reversible;                       /**< Can anergy be reversed */
} anergic_cell_record_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Tolerance system configuration
 */
typedef struct {
    /* Pattern database */
    size_t max_self_patterns;              /**< Max self patterns to store */
    float self_match_threshold;            /**< Minimum match for "self" (0-1) */

    /* Central tolerance thresholds */
    float central_deletion_threshold;      /**< Affinity for clonal deletion */
    float receptor_editing_threshold;      /**< Affinity for receptor editing */

    /* Peripheral tolerance */
    float anergy_threshold;                /**< Affinity for anergy induction */
    bool enable_anergy;                    /**< Enable anergy mechanism */
    bool enable_receptor_editing;          /**< Enable receptor editing */

    /* Operational */
    tolerance_phase_t initial_phase;       /**< Starting phase */
    bool auto_transition;                  /**< Auto-advance through phases */
    uint32_t training_pattern_count;       /**< Patterns needed for training */

    /* Thread safety */
    bool thread_safe;                      /**< Enable mutex protection */
} tolerance_config_t;

/**
 * @brief Tolerance system statistics
 */
typedef struct {
    /* Self patterns */
    uint32_t self_pattern_count;
    uint64_t total_self_checks;
    uint64_t self_matches;
    uint64_t non_self_matches;

    /* Central tolerance */
    uint64_t cells_deleted;
    uint64_t cells_edited;
    uint64_t positive_selections;
    uint64_t negative_selections;

    /* Peripheral tolerance */
    uint32_t anergic_cells;
    uint64_t anergy_inductions;
    uint64_t anergy_reversals;

    /* Autoimmune prevention */
    uint64_t autoimmune_prevented;
    float false_positive_rate;
} tolerance_stats_t;

/* ============================================================================
 * Main System Structure
 * ============================================================================ */

/**
 * @brief Tolerance system state
 */
struct tolerance_system {
    tolerance_config_t config;             /**< Configuration */
    tolerance_phase_t phase;               /**< Current phase */

    /* Self pattern database (AIRE-like) */
    self_pattern_t* self_patterns;
    size_t self_pattern_count;
    size_t self_pattern_capacity;
    uint32_t next_pattern_id;

    /* Anergic cells tracking */
    anergic_cell_record_t* anergic_cells;
    size_t anergic_cell_count;
    size_t anergic_cell_capacity;

    /* Integration with brain immune */
    brain_immune_system_t* immune_system;  /**< Connected immune system */

    /* Statistics */
    tolerance_stats_t stats;

    /* Thread safety */
    void* mutex;                           /**< Platform mutex */

    /* State */
    bool initialized;
    uint64_t creation_time;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default tolerance configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological thresholds
 * HOW:  Return struct with biologically-motivated parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int tolerance_default_config(tolerance_config_t* config);

/**
 * @brief Create tolerance system
 *
 * WHAT: Initialize tolerance learning system
 * WHY:  Set up self/non-self discrimination infrastructure
 * HOW:  Allocate pattern database, integrate with immune system
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system to integrate with
 * @return New tolerance system or NULL on failure
 */
tolerance_system_t* tolerance_create(
    const tolerance_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy tolerance system
 *
 * WHAT: Clean up tolerance system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free patterns, anergic records, mutex
 *
 * @param system System to destroy
 */
void tolerance_destroy(tolerance_system_t* system);

/* ============================================================================
 * Self Pattern Registration API
 * ============================================================================ */

/**
 * @brief Register self pattern
 *
 * WHAT: Add normal operation pattern to self database
 * WHY:  Teach immune system what is legitimate
 * HOW:  Store pattern with metadata for matching
 *
 * @param system Tolerance system
 * @param pattern Pattern data
 * @param len Pattern length
 * @param description Human-readable description (optional)
 * @param pattern_id Output: assigned pattern ID (optional)
 * @return 0 on success, -1 on error
 */
int tolerance_register_self_pattern(
    tolerance_system_t* system,
    const uint8_t* pattern,
    size_t len,
    const char* description,
    uint32_t* pattern_id
);

/**
 * @brief Check if pattern is self
 *
 * WHAT: Determine if pattern matches any self pattern
 * WHY:  Primary mechanism for self/non-self discrimination
 * HOW:  Fuzzy match against all registered self patterns
 *
 * @param system Tolerance system
 * @param pattern Pattern to check
 * @param len Pattern length
 * @param matched_pattern_id Output: matched pattern ID (optional)
 * @param affinity Output: match strength 0-1 (optional)
 * @return true if self, false if non-self
 */
bool tolerance_check_self(
    tolerance_system_t* system,
    const uint8_t* pattern,
    size_t len,
    uint32_t* matched_pattern_id,
    float* affinity
);

/**
 * @brief Remove self pattern
 *
 * WHAT: Delete self pattern from database
 * WHY:  System behavior may change, pattern no longer valid
 * HOW:  Remove from array, compact
 *
 * @param system Tolerance system
 * @param pattern_id Pattern to remove
 * @return 0 on success, -1 if not found or immutable
 */
int tolerance_remove_self_pattern(
    tolerance_system_t* system,
    uint32_t pattern_id
);

/**
 * @brief Clear all self patterns
 *
 * WHAT: Remove all self patterns
 * WHY:  Reset tolerance for retraining
 * HOW:  Clear array (keep immutable patterns)
 *
 * @param system Tolerance system
 * @return 0 on success
 */
int tolerance_clear_self_patterns(tolerance_system_t* system);

/**
 * @brief Get self pattern count
 *
 * @param system Tolerance system
 * @return Number of registered self patterns
 */
size_t tolerance_get_self_patterns_count(const tolerance_system_t* system);

/* ============================================================================
 * Central Tolerance API (Thymic/Bone Marrow Selection)
 * ============================================================================ */

/**
 * @brief Perform central selection on cell
 *
 * WHAT: Positive and negative selection during development
 * WHY:  Delete strongly self-reactive cells before deployment
 * HOW:  Test receptor against self patterns, determine fate
 *
 * BIOLOGICAL BASIS:
 * - Positive selection: must recognize MHC (weak self-recognition)
 * - Negative selection: strong self-recognition = deletion
 *
 * @param system Tolerance system
 * @param cell_id Cell ID (B or T cell)
 * @param is_b_cell true=B cell, false=T cell
 * @param receptor Cell receptor pattern
 * @param receptor_len Receptor length
 * @param outcome Output: selection outcome
 * @return 0 on success, -1 on error
 */
int tolerance_central_selection(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell,
    const uint8_t* receptor,
    size_t receptor_len,
    selection_outcome_t* outcome
);

/**
 * @brief Delete self-reactive cell
 *
 * WHAT: Clonal deletion - remove self-reactive cell
 * WHY:  Central tolerance mechanism
 * HOW:  Mark cell for deletion in immune system
 *
 * @param system Tolerance system
 * @param cell_id Cell to delete
 * @param is_b_cell true=B cell, false=T cell
 * @return 0 on success, -1 on error
 */
int tolerance_delete_cell(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell
);

/* ============================================================================
 * Peripheral Tolerance API (Anergy/Regulation)
 * ============================================================================ */

/**
 * @brief Induce anergy in cell
 *
 * WHAT: Inactivate self-reactive cell without deletion
 * WHY:  Peripheral tolerance for escaped cells
 * HOW:  Record cell as anergic, prevent activation
 *
 * BIOLOGICAL BASIS:
 * Anergy occurs when antigen presented without co-stimulation.
 * Cell recognizes antigen but cannot respond.
 *
 * @param system Tolerance system
 * @param cell_id Cell to anergize
 * @param is_b_cell true=B cell, false=T cell
 * @param self_pattern_id Matched self pattern
 * @param affinity Self-affinity
 * @return 0 on success, -1 on error
 */
int tolerance_induce_anergy(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell,
    uint32_t self_pattern_id,
    float affinity
);

/**
 * @brief Check if cell is anergic
 *
 * WHAT: Determine if cell has been anergized
 * WHY:  Prevent anergic cells from responding
 * HOW:  Lookup in anergic cell records
 *
 * @param system Tolerance system
 * @param cell_id Cell ID
 * @param is_b_cell true=B cell, false=T cell
 * @return true if anergic, false otherwise
 */
bool tolerance_is_anergic(
    const tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell
);

/**
 * @brief Reverse anergy
 *
 * WHAT: Reactivate anergic cell
 * WHY:  Allow recovery if self pattern was incorrect
 * HOW:  Remove from anergic records
 *
 * @param system Tolerance system
 * @param cell_id Cell to reactivate
 * @param is_b_cell true=B cell, false=T cell
 * @return 0 on success, -1 if not found or irreversible
 */
int tolerance_reverse_anergy(
    tolerance_system_t* system,
    uint32_t cell_id,
    bool is_b_cell
);

/* ============================================================================
 * Phase Management API
 * ============================================================================ */

/**
 * @brief Set tolerance phase
 *
 * WHAT: Transition to different tolerance phase
 * WHY:  Control when central vs peripheral mechanisms apply
 * HOW:  Update phase, apply phase-specific rules
 *
 * @param system Tolerance system
 * @param phase New phase
 * @return 0 on success
 */
int tolerance_set_phase(
    tolerance_system_t* system,
    tolerance_phase_t phase
);

/**
 * @brief Get current phase
 *
 * @param system Tolerance system
 * @return Current phase
 */
tolerance_phase_t tolerance_get_phase(const tolerance_system_t* system);

/* ============================================================================
 * Threshold Tuning API
 * ============================================================================ */

/**
 * @brief Set self-match threshold
 *
 * WHAT: Adjust threshold for self recognition
 * WHY:  Tune sensitivity vs specificity tradeoff
 * HOW:  Update config threshold
 *
 * @param system Tolerance system
 * @param threshold New threshold (0-1)
 * @return 0 on success, -1 if out of range
 */
int tolerance_set_self_threshold(
    tolerance_system_t* system,
    float threshold
);

/**
 * @brief Set central deletion threshold
 *
 * @param system Tolerance system
 * @param threshold New threshold (0-1)
 * @return 0 on success, -1 if out of range
 */
int tolerance_set_central_threshold(
    tolerance_system_t* system,
    float threshold
);

/**
 * @brief Set anergy threshold
 *
 * @param system Tolerance system
 * @param threshold New threshold (0-1)
 * @return 0 on success, -1 if out of range
 */
int tolerance_set_anergy_threshold(
    tolerance_system_t* system,
    float threshold
);

/* ============================================================================
 * Statistics and Query API
 * ============================================================================ */

/**
 * @brief Get tolerance statistics
 *
 * @param system Tolerance system
 * @param stats Output statistics
 * @return 0 on success
 */
int tolerance_get_stats(
    const tolerance_system_t* system,
    tolerance_stats_t* stats
);

/**
 * @brief Get self pattern by ID
 *
 * @param system Tolerance system
 * @param pattern_id Pattern ID
 * @return Pattern or NULL if not found
 */
const self_pattern_t* tolerance_get_pattern(
    const tolerance_system_t* system,
    uint32_t pattern_id
);

/**
 * @brief Compute affinity between patterns
 *
 * WHAT: Calculate pattern similarity
 * WHY:  Determine self-recognition strength
 * HOW:  Fuzzy pattern matching (delegates to brain_immune_compute_affinity)
 *
 * @param pattern1 First pattern
 * @param len1 First length
 * @param pattern2 Second pattern
 * @param len2 Second length
 * @return Affinity score (0-1)
 */
float tolerance_compute_affinity(
    const uint8_t* pattern1,
    size_t len1,
    const uint8_t* pattern2,
    size_t len2
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* tolerance_phase_to_string(tolerance_phase_t phase);
const char* tolerance_selection_to_string(selection_outcome_t outcome);
const char* tolerance_cell_state_to_string(cell_tolerance_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMMUNE_TOLERANCE_H */

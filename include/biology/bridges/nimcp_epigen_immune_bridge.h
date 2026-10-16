/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_epigen_immune_bridge.h - Epigenetics to Immune System Bridge
//=============================================================================
/**
 * @file nimcp_epigen_immune_bridge.h
 * @brief Bridge between Epigenetics and Brain Immune System
 *
 * WHAT: Connects epigenetic modifications to the brain's immune system,
 *       enabling neuroimmune modulation through gene expression changes
 *       and immune-mediated epigenetic feedback.
 *
 * WHY:  Bridges the gap between:
 *       - Epigenetic state (methylation, histone modifications)
 *       - Microglial activation and pruning
 *       - Neuroinflammation regulation
 *       - Immune memory and trained immunity
 *
 * HOW:  Two-way integration:
 *       1. Epigenetics -> Immune: Gene expression affects immune cell function
 *       2. Immune -> Epigenetics: Inflammation triggers epigenetic marks
 *       3. Chromatin state -> Cytokine production capacity
 *       4. Immune memory -> Epigenetic priming
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPIGENETICS                           IMMUNE EFFECTS
 * ---------------------------------------------------------------------------
 * Cytokine gene methylation          -> Reduced inflammatory response
 * NF-kB promoter accessibility       -> Inflammatory gene activation
 * Microglial phenotype genes         -> M1/M2 polarization bias
 * Complement gene expression         -> Synaptic pruning intensity
 * Chronic inflammation              <- Triggers DNA methylation
 * Acute immune activation           <- Triggers histone acetylation
 * ```
 *
 * NEUROIMMUNE COUPLING:
 * - Methylated cytokine genes: Attenuated inflammation
 * - Open chromatin at NF-kB: Enhanced immune response
 * - Trained immunity: Epigenetic memory of prior infections
 * - Inflammation-induced marks: Long-term immune phenotype
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPIGEN_IMMUNE_BRIDGE_H
#define NIMCP_EPIGEN_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define EPIGEN_IMMUNE_MODULE_NAME       "epigen_immune_bridge"

/** Maximum immune cell types tracked */
#define EPIGEN_IMM_MAX_CELL_TYPES       16

/** Maximum cytokine types */
#define EPIGEN_IMM_MAX_CYTOKINES        32

/** Maximum inflammation events per update */
#define EPIGEN_IMM_MAX_INFLAMM_EVENTS   64

/** Maximum immune memory entries */
#define EPIGEN_IMM_MAX_MEMORY           128

/** Chronic inflammation threshold (ms) */
#define EPIGEN_IMM_CHRONIC_DURATION_MS  86400000.0f

/** Default cytokine methylation effect */
#define EPIGEN_IMM_CYTOKINE_METHYL      0.4f

/** Inflammation threshold for epigenetic trigger */
#define EPIGEN_IMM_INFLAMM_THRESHOLD    0.7f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Immune cell type
 */
typedef enum {
    EPIGEN_IMM_CELL_MICROGLIA = 0,   /**< Brain-resident macrophages */
    EPIGEN_IMM_CELL_ASTROCYTE,       /**< Astrocyte (immune-supporting) */
    EPIGEN_IMM_CELL_OLIGODENDROCYTE, /**< Oligodendrocyte */
    EPIGEN_IMM_CELL_T_CELL,          /**< Infiltrating T cells */
    EPIGEN_IMM_CELL_B_CELL           /**< Infiltrating B cells */
} epigen_imm_cell_t;

/**
 * @brief Microglial polarization state
 */
typedef enum {
    EPIGEN_IMM_MICROGLIA_M0 = 0,     /**< Resting/surveillance */
    EPIGEN_IMM_MICROGLIA_M1,         /**< Pro-inflammatory */
    EPIGEN_IMM_MICROGLIA_M2,         /**< Anti-inflammatory/repair */
    EPIGEN_IMM_MICROGLIA_DAM         /**< Disease-associated */
} epigen_imm_microglia_t;

/**
 * @brief Cytokine type
 */
typedef enum {
    EPIGEN_IMM_CYTOKINE_IL1B = 0,    /**< Interleukin-1 beta (pro-inflamm) */
    EPIGEN_IMM_CYTOKINE_IL6,         /**< Interleukin-6 (context-dependent) */
    EPIGEN_IMM_CYTOKINE_TNF,         /**< TNF-alpha (pro-inflammatory) */
    EPIGEN_IMM_CYTOKINE_IL10,        /**< Interleukin-10 (anti-inflamm) */
    EPIGEN_IMM_CYTOKINE_TGFB,        /**< TGF-beta (regulatory) */
    EPIGEN_IMM_CYTOKINE_BDNF         /**< Brain-derived neurotrophic factor */
} epigen_imm_cytokine_t;

/**
 * @brief Inflammation type
 */
typedef enum {
    EPIGEN_IMM_INFLAMM_ACUTE = 0,    /**< Acute inflammation */
    EPIGEN_IMM_INFLAMM_CHRONIC,      /**< Chronic inflammation */
    EPIGEN_IMM_INFLAMM_STERILE,      /**< Sterile (damage-induced) */
    EPIGEN_IMM_INFLAMM_RESOLVED      /**< Resolved inflammation */
} epigen_imm_inflamm_t;

/**
 * @brief Epigenetic trigger from immune activity
 */
typedef enum {
    EPIGEN_IMM_TRIGGER_ACUTE_RESPONSE = 0, /**< Acute immune activation */
    EPIGEN_IMM_TRIGGER_CHRONIC_INFLAMM,    /**< Sustained inflammation */
    EPIGEN_IMM_TRIGGER_TOLERANCE,          /**< Immune tolerance induction */
    EPIGEN_IMM_TRIGGER_TRAINED_IMMUNITY    /**< Trained immunity formation */
} epigen_imm_trigger_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for epigenetics-immune bridge
 */
typedef struct {
    /** Cytokine expression parameters */
    float cytokine_methylation_effect;   /**< Methylation effect on cytokines */
    float cytokine_acetylation_effect;   /**< Acetylation effect on cytokines */
    float expression_delay_ms;           /**< Gene expression delay */
    bool enable_cytokine_modulation;     /**< Enable cytokine gene control */

    /** Microglial parameters */
    float polarization_threshold;        /**< Threshold for M1/M2 switch */
    float pruning_gene_effect;           /**< Effect on pruning genes */
    float methylation_polarization_bias; /**< Methylation effect on M1/M2 */
    bool enable_microglial_control;      /**< Enable microglial epigenetics */

    /** Inflammation feedback parameters */
    float inflamm_epigen_threshold;      /**< Inflammation for epigen trigger */
    float chronic_inflamm_duration_ms;   /**< Duration for chronic */
    float acute_acetylation_boost;       /**< Acetylation from acute response */
    float chronic_methylation_strength;  /**< Methylation from chronic */
    bool enable_inflamm_feedback;        /**< Inflammation triggers epigenetics */

    /** Immune memory parameters */
    float trained_immunity_strength;     /**< Strength of trained response */
    float tolerance_strength;            /**< Strength of tolerance */
    float memory_duration_ms;            /**< How long immune memory lasts */
    bool enable_immune_memory;           /**< Enable trained immunity */

    /** Update parameters */
    float update_interval_ms;
    bool enable_logging;
    bool enable_metrics;
} epigen_imm_config_t;

/**
 * @brief Cytokine gene expression state
 */
typedef struct {
    epigen_imm_cytokine_t cytokine;      /**< Cytokine type */
    float baseline_expression;           /**< Baseline expression */
    float current_expression;            /**< Current expression */
    float methylation_level;             /**< Gene methylation (0-1) */
    float acetylation_level;             /**< Gene acetylation (0-1) */
    float production_capacity;           /**< Resulting production capacity */
    bool is_silenced;                    /**< Gene fully silenced */
} epigen_imm_cytokine_state_t;

/**
 * @brief Microglial epigenetic state
 */
typedef struct {
    uint32_t region_id;                  /**< Neural region */
    epigen_imm_microglia_t phenotype;    /**< Current phenotype */
    float m1_gene_methylation;           /**< M1 gene methylation */
    float m2_gene_methylation;           /**< M2 gene methylation */
    float polarization_bias;             /**< Bias toward M1 (+) or M2 (-) */
    float pruning_intensity;             /**< Synaptic pruning rate */
    float surveillance_activity;         /**< Baseline surveillance */
} epigen_imm_microglia_state_t;

/**
 * @brief Inflammation-epigenetics coupling state
 */
typedef struct {
    epigen_imm_inflamm_t inflamm_type;   /**< Current inflammation type */
    float inflammation_level;            /**< Inflammation magnitude (0-1) */
    float integrated_inflammation;       /**< Time-integrated inflammation */
    float duration_ms;                   /**< Duration of current episode */
    bool chronic_detected;               /**< Chronic inflammation flag */
    bool trigger_pending;                /**< Epigenetic trigger pending */
    epigen_imm_trigger_t pending_trigger;/**< What trigger to apply */
} epigen_imm_inflamm_state_t;

/**
 * @brief Immune memory entry (trained immunity)
 */
typedef struct {
    uint32_t memory_id;                  /**< Memory entry ID */
    uint32_t antigen_signature;          /**< Signature of triggering antigen */
    float response_enhancement;          /**< Enhanced response magnitude */
    float methylation_pattern;           /**< Associated methylation pattern */
    float histone_pattern;               /**< Associated histone pattern */
    float creation_time_ms;              /**< When memory formed */
    float decay_rate;                    /**< Memory decay rate */
    bool is_tolerance;                   /**< Tolerance vs trained immunity */
} epigen_imm_memory_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t cytokine_modulations;       /**< Cytokine expression changes */
    uint64_t microglial_polarizations;   /**< Microglial phenotype changes */
    uint64_t inflammation_triggers;      /**< Inflammation-triggered changes */
    uint64_t trained_immunity_events;    /**< Trained immunity formations */
    uint64_t tolerance_events;           /**< Tolerance induction events */
    float avg_inflammation_level;        /**< Average inflammation */
    float avg_cytokine_expression;       /**< Average cytokine expression */
    float last_update_ms;                /**< Last update timestamp */
} epigen_imm_stats_t;

/** Opaque bridge handle */
typedef struct epigen_imm_bridge_struct epigen_imm_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_default_config(epigen_imm_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create epigenetics-immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT epigen_imm_bridge_t* epigen_imm_bridge_create(
    const epigen_imm_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void epigen_imm_bridge_destroy(epigen_imm_bridge_t* bridge);

//=============================================================================
// Cytokine Expression API (Epigenetics -> Immune)
//=============================================================================

/**
 * @brief Set cytokine gene methylation state
 *
 * WHAT: Updates cytokine production based on methylation
 * WHY:  Methylation silences cytokine gene transcription
 * HOW:  Scales expression capacity by methylation level
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine type
 * @param methylation_level Methylation (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_set_cytokine_methylation(
    epigen_imm_bridge_t* bridge,
    epigen_imm_cytokine_t cytokine,
    float methylation_level
);

/**
 * @brief Get cytokine production capacity
 *
 * WHAT: Returns epigenetic cytokine production modifier
 * WHY:  Immune system needs to scale cytokine output
 * HOW:  Based on gene methylation/acetylation state
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine type
 * @param capacity Output production capacity (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_get_cytokine_capacity(
    epigen_imm_bridge_t* bridge,
    epigen_imm_cytokine_t cytokine,
    float* capacity
);

/**
 * @brief Apply histone modification to cytokine gene
 *
 * WHAT: Modifies cytokine gene accessibility
 * WHY:  Acetylation enhances, deacetylation suppresses
 * HOW:  Adjusts production capacity
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine type
 * @param acetylation_level Acetylation (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_set_cytokine_acetylation(
    epigen_imm_bridge_t* bridge,
    epigen_imm_cytokine_t cytokine,
    float acetylation_level
);

//=============================================================================
// Microglial Control API
//=============================================================================

/**
 * @brief Set microglial gene methylation
 *
 * WHAT: Updates microglial phenotype via gene methylation
 * WHY:  Methylation pattern determines M1/M2 bias
 * HOW:  Modifies polarization tendency
 *
 * @param bridge Bridge handle
 * @param region_id Neural region
 * @param m1_methylation M1-associated gene methylation
 * @param m2_methylation M2-associated gene methylation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_set_microglia_methylation(
    epigen_imm_bridge_t* bridge,
    uint32_t region_id,
    float m1_methylation,
    float m2_methylation
);

/**
 * @brief Get microglial polarization bias
 *
 * WHAT: Returns epigenetic M1/M2 polarization bias
 * WHY:  Immune system needs polarization tendency
 * HOW:  Based on M1/M2 gene methylation balance
 *
 * @param bridge Bridge handle
 * @param region_id Neural region
 * @param polarization_bias Output bias (-1 M2, +1 M1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_get_polarization_bias(
    epigen_imm_bridge_t* bridge,
    uint32_t region_id,
    float* polarization_bias
);

/**
 * @brief Get synaptic pruning modifier
 *
 * WHAT: Returns epigenetic pruning intensity modifier
 * WHY:  Pruning rate affected by complement genes
 * HOW:  Based on complement gene expression
 *
 * @param bridge Bridge handle
 * @param region_id Neural region
 * @param pruning_modifier Output modifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_get_pruning_modifier(
    epigen_imm_bridge_t* bridge,
    uint32_t region_id,
    float* pruning_modifier
);

//=============================================================================
// Inflammation Feedback API (Immune -> Epigenetics)
//=============================================================================

/**
 * @brief Report inflammation level
 *
 * WHAT: Reports current inflammation to bridge
 * WHY:  Chronic inflammation triggers epigenetic changes
 * HOW:  Integrates inflammation, detects chronic state
 *
 * @param bridge Bridge handle
 * @param inflammation_level Current inflammation (0-1)
 * @param inflamm_type Type of inflammation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_report_inflammation(
    epigen_imm_bridge_t* bridge,
    float inflammation_level,
    epigen_imm_inflamm_t inflamm_type
);

/**
 * @brief Get inflammation-triggered epigenetic events
 *
 * WHAT: Returns inflammation-induced triggers
 * WHY:  Epigenetics module needs to apply changes
 * HOW:  Returns accumulated trigger events
 *
 * @param bridge Bridge handle
 * @param triggers Output array for triggers
 * @param max_triggers Maximum triggers to return
 * @return Number of triggers, -1 on error
 */
NIMCP_EXPORT int epigen_imm_get_inflamm_triggers(
    epigen_imm_bridge_t* bridge,
    epigen_imm_trigger_t* triggers,
    uint32_t max_triggers
);

/**
 * @brief Get current inflammation state
 *
 * @param bridge Bridge handle
 * @param state Output inflammation state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_get_inflamm_state(
    epigen_imm_bridge_t* bridge,
    epigen_imm_inflamm_state_t* state
);

//=============================================================================
// Immune Memory API (Trained Immunity)
//=============================================================================

/**
 * @brief Create immune memory entry
 *
 * WHAT: Creates trained immunity or tolerance memory
 * WHY:  Immune system can remember prior challenges
 * HOW:  Stores epigenetic pattern for later recall
 *
 * @param bridge Bridge handle
 * @param antigen_signature Signature of antigen
 * @param is_tolerance true for tolerance, false for trained
 * @return Memory ID on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_create_memory(
    epigen_imm_bridge_t* bridge,
    uint32_t antigen_signature,
    bool is_tolerance
);

/**
 * @brief Recall immune memory
 *
 * WHAT: Recalls trained immunity response
 * WHY:  Provides enhanced/reduced response to known antigens
 * HOW:  Returns stored epigenetic pattern
 *
 * @param bridge Bridge handle
 * @param antigen_signature Signature to look up
 * @param memory Output memory entry
 * @return 0 on success, -1 if not found
 */
NIMCP_EXPORT int epigen_imm_recall_memory(
    epigen_imm_bridge_t* bridge,
    uint32_t antigen_signature,
    epigen_imm_memory_t* memory
);

/**
 * @brief Get immune memory modifier
 *
 * WHAT: Returns response modifier from immune memory
 * WHY:  Affects magnitude of immune response
 * HOW:  Based on trained immunity/tolerance state
 *
 * @param bridge Bridge handle
 * @param antigen_signature Signature to check
 * @param response_modifier Output modifier (>1 trained, <1 tolerance)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_get_memory_modifier(
    epigen_imm_bridge_t* bridge,
    uint32_t antigen_signature,
    float* response_modifier
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Integrate inflammation, decay memory, process triggers
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_update(
    epigen_imm_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_reset(epigen_imm_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_get_stats(
    const epigen_imm_bridge_t* bridge,
    epigen_imm_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int epigen_imm_reset_stats(epigen_imm_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPIGEN_IMMUNE_BRIDGE_H */
/**
 * @file nimcp_protein_immune_bridge.h
 * @brief Protein Synthesis-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and protein synthesis
 * WHY:  Pro-inflammatory cytokines suppress protein synthesis, preventing consolidation
 *       during immune challenge (energy conservation, metabolic prioritization)
 * HOW:  Inflammation level modulates PRP synthesis rate, cytokines affect consolidation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → PROTEIN SYNTHESIS PATHWAYS:
 * -------------------------------------
 * 1. Pro-inflammatory Cytokines Suppress Protein Synthesis:
 *    - IL-1β, IL-6, TNF-α reduce mRNA translation efficiency
 *    - Inflammation → eIF2α phosphorylation → translation block
 *    - 30-50% reduction in synaptic protein synthesis
 *    - Prevents memory consolidation during immune challenge
 *    - Reference: Costa-Mattioli et al. (2009) "Translation dysregulation in brain disease"
 *
 * 2. Inflammation Levels → Synthesis Suppression:
 *    - NONE: 100% synthesis rate (normal)
 *    - LOCAL: 90% synthesis rate (mild impairment)
 *    - REGIONAL: 70% synthesis rate (moderate impairment)
 *    - SYSTEMIC: 40% synthesis rate (severe impairment)
 *    - STORM: 10% synthesis rate (critical impairment)
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning and memory"
 *
 * 3. Anti-inflammatory Cytokines Restore Synthesis:
 *    - IL-10 removes eIF2α phosphorylation
 *    - Restores protein synthesis capacity
 *    - Allows delayed consolidation after inflammation resolves
 *    - Reference: Rizzo et al. (2018) "IL-10 restores synaptic plasticity"
 *
 * 4. Metabolic Prioritization During Immune Response:
 *    - Immune activation is energetically expensive
 *    - Protein synthesis for consolidation competes for ATP
 *    - Inflammatory signaling prioritizes immune response
 *    - Memory consolidation deferred until resolution
 *    - Reference: Heller & Ruskin (2015) "Inflammation and memory consolidation"
 *
 * 5. Fever Effects on Protein Synthesis:
 *    - Elevated temperature (>38.5°C) reduces ribosomal activity
 *    - Similar suppression to systemic inflammation
 *    - Tags may expire before synthesis resumes
 *    - Reference: Schultzberg et al. (1999) "Fever and learning deficits"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           PROTEIN SYNTHESIS-IMMUNE INTEGRATION BRIDGE                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   INFLAMMATION     PRP Synthesis    Consolidation                         ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   NONE             1.0x             Normal                                ║
 * ║   LOCAL            0.9x             Slightly impaired                     ║
 * ║   REGIONAL         0.7x             Moderately impaired                   ║
 * ║   SYSTEMIC         0.4x             Severely impaired                     ║
 * ║   STORM            0.1x             Critical failure                      ║
 * ║                                                                            ║
 * ║   CYTOKINE         Effect                                                 ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   IL-1β            -10% synthesis                                         ║
 * ║   IL-6             -15% synthesis                                         ║
 * ║   TNF-α            -20% synthesis                                         ║
 * ║   IFN-γ            -25% synthesis (antiviral priority)                    ║
 * ║   IL-10            +20% synthesis (restoration)                           ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PROTEIN_IMMUNE_BRIDGE_H
#define NIMCP_PROTEIN_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/protein/nimcp_protein_synthesis.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine suppression factors (cumulative) */
#define CYTOKINE_IL1_SYNTHESIS_SUPPRESSION      0.90f   /**< IL-1β reduces to 90% */
#define CYTOKINE_IL6_SYNTHESIS_SUPPRESSION      0.85f   /**< IL-6 reduces to 85% */
#define CYTOKINE_TNF_SYNTHESIS_SUPPRESSION      0.80f   /**< TNF-α reduces to 80% */
#define CYTOKINE_IFN_GAMMA_SYNTHESIS_SUPPRESSION 0.75f  /**< IFN-γ reduces to 75% */
#define CYTOKINE_IL10_SYNTHESIS_RESTORATION     1.20f   /**< IL-10 restores by 20% */

/* Inflammation-based synthesis suppression */
#define INFLAM_SYNTHESIS_NONE                   1.0f    /**< 100% synthesis */
#define INFLAM_SYNTHESIS_LOCAL                  0.9f    /**< 90% synthesis */
#define INFLAM_SYNTHESIS_REGIONAL               0.7f    /**< 70% synthesis */
#define INFLAM_SYNTHESIS_SYSTEMIC               0.4f    /**< 40% synthesis */
#define INFLAM_SYNTHESIS_STORM                  0.1f    /**< 10% synthesis */

/* Cytokine concentration thresholds for effects */
#define CYTOKINE_THRESHOLD_LOW                  0.1f    /**< Minimal effect */
#define CYTOKINE_THRESHOLD_MODERATE             0.3f    /**< Moderate effect */
#define CYTOKINE_THRESHOLD_HIGH                 0.6f    /**< Strong effect */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Cytokine effects on protein synthesis
 *
 * How individual cytokines modulate synthesis rate
 */
typedef struct {
    /* Pro-inflammatory suppression */
    float il1_suppression;          /**< IL-1β effect [0-1] */
    float il6_suppression;          /**< IL-6 effect [0-1] */
    float tnf_suppression;          /**< TNF-α effect [0-1] */
    float ifn_gamma_suppression;    /**< IFN-γ effect [0-1] */

    /* Anti-inflammatory restoration */
    float il10_restoration;         /**< IL-10 effect [1-2] */

    /* Aggregate effect */
    float total_modulation;         /**< Combined factor [0-2] */
} cytokine_protein_effects_t;

/**
 * @brief Inflammation effects on protein synthesis
 *
 * How chronic inflammation affects consolidation capacity
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Synthesis impacts */
    float synthesis_suppression;       /**< Rate reduction [0-1] */
    float consolidation_failure_rate;  /**< Tags expire without capture */
    float tag_decay_acceleration;      /**< Faster tag expiration */

    /* Chronic effects */
    float prp_pool_depletion;          /**< Pool capacity reduction */
    float recovery_impairment;         /**< Slower restoration after resolution */
} inflammation_protein_state_t;

/**
 * @brief Protein synthesis-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_suppression;     /**< Cytokines affect synthesis */
    bool enable_inflammation_impairment;  /**< Inflammation affects consolidation */
    bool enable_tag_decay_modulation;     /**< Inflammation accelerates tag decay */

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */

    /* Thresholds */
    float chronic_inflammation_threshold_sec; /**< Duration for chronic effects */
} protein_immune_config_t;

/**
 * @brief Complete protein-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    protein_synthesis_system_t protein_system;

    /* Configuration */
    protein_immune_config_t config;

    /* Current state */
    cytokine_protein_effects_t cytokine_effects;
    inflammation_protein_state_t inflammation_state;

    /* Statistics */
    uint64_t total_updates;
    uint32_t synthesis_suppressions;
    uint32_t consolidation_failures;
    uint32_t restoration_events;

    } protein_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default configuration
 * WHY:  Provide sensible default configuration
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int protein_immune_default_config(protein_immune_config_t* config);

/**
 * WHAT: Create protein-immune bridge
 * WHY:  Initialize bidirectional protein-immune integration
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param protein_system Protein synthesis system
 * @return New bridge or NULL on failure
 */
protein_immune_bridge_t* protein_immune_bridge_create(
    const protein_immune_config_t* config,
    brain_immune_system_t* immune_system,
    protein_synthesis_system_t protein_system
);

/**
 * WHAT: Destroy protein-immune bridge
 * WHY:  Clean up bridge resources
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void protein_immune_bridge_destroy(protein_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Protein Synthesis API
 * ============================================================================ */

/**
 * WHAT: Apply cytokine effects to protein synthesis
 * WHY:  Pro-inflammatory cytokines suppress synthesis
 * HOW:  Query immune system cytokine levels, adjust synthesis rate
 *
 * @param bridge Protein-immune bridge
 * @return 0 on success
 */
int protein_immune_apply_cytokine_effects(protein_immune_bridge_t* bridge);

/**
 * WHAT: Apply inflammation effects to protein synthesis
 * WHY:  Chronic inflammation impairs consolidation capacity
 * HOW:  Check inflammation duration/level, suppress synthesis
 *
 * @param bridge Protein-immune bridge
 * @return 0 on success
 */
int protein_immune_apply_inflammation_effects(protein_immune_bridge_t* bridge);

/**
 * WHAT: Get inflammation-suppressed synthesis rate
 * WHY:  Compute effective synthesis rate with immune suppression
 * HOW:  Map inflammation level to suppression factor
 *
 * @param bridge Protein-immune bridge
 * @param base_rate Original synthesis rate
 * @return Effective synthesis rate [0-base_rate]
 */
float protein_immune_get_effective_synthesis_rate(
    const protein_immune_bridge_t* bridge,
    float base_rate
);

/**
 * WHAT: Restore synthesis after inflammation resolution
 * WHY:  IL-10 and resolution restore synthesis capacity
 * HOW:  Interpolate back to normal synthesis
 *
 * @param bridge Protein-immune bridge
 * @param recovery_factor Recovery progress [0-1]
 * @return 0 on success
 */
int protein_immune_restore_synthesis(
    protein_immune_bridge_t* bridge,
    float recovery_factor
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * WHAT: Update protein-immune bridge (both directions)
 * WHY:  Process all protein-immune interactions
 * HOW:  Apply cytokine/inflammation effects
 *
 * @param bridge Protein-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int protein_immune_bridge_update(
    protein_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * WHAT: Get current cytokine effects on synthesis
 * WHY:  Monitor immune modulation of consolidation
 * HOW:  Return computed effects structure
 *
 * @param bridge Protein-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int protein_immune_get_cytokine_effects(
    const protein_immune_bridge_t* bridge,
    cytokine_protein_effects_t* effects
);

/**
 * WHAT: Get current inflammation state
 * WHY:  Monitor inflammation impact on consolidation
 * HOW:  Return inflammation state structure
 *
 * @param bridge Protein-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int protein_immune_get_inflammation_state(
    const protein_immune_bridge_t* bridge,
    inflammation_protein_state_t* state
);

/**
 * WHAT: Check if synthesis is impaired by inflammation
 * WHY:  Determine if consolidation capacity is reduced
 * HOW:  Check if synthesis suppression factor < 1.0
 *
 * @param bridge Protein-immune bridge
 * @return true if impaired
 */
bool protein_immune_is_synthesis_impaired(const protein_immune_bridge_t* bridge);

/**
 * WHAT: Get synthesis capacity reduction percentage
 * WHY:  Quantify immune suppression impact
 * HOW:  Return (1.0 - suppression_factor) * 100
 *
 * @param bridge Protein-immune bridge
 * @return Reduction percentage [0-100]
 */
float protein_immune_get_synthesis_reduction(const protein_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * WHAT: Connect bridge to bio-async router
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_PROTEIN
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int protein_immune_connect_bio_async(protein_immune_bridge_t* bridge);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int protein_immune_disconnect_bio_async(protein_immune_bridge_t* bridge);

/**
 * WHAT: Check if bio-async is connected
 * WHY:  Query connection status
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool protein_immune_is_bio_async_connected(const protein_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROTEIN_IMMUNE_BRIDGE_H */

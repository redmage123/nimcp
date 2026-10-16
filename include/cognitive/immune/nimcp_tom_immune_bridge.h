/**
 * @file nimcp_tom_immune_bridge.h
 * @brief Theory of Mind-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and theory of mind
 * WHY:  Biological evidence shows immune-social cognition coupling (cytokines impair
 *       mentalizing, social stress triggers inflammation). Essential for realistic
 *       social brain modeling.
 * HOW:  Cytokines impair perspective-taking and mentalizing accuracy, social
 *       rejection/isolation triggers inflammatory response, inflammation reduces
 *       empathy capacity.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → ToM PATHWAYS:
 * ----------------------
 * 1. Pro-inflammatory Cytokines (IL-6, TNF-α) Impair Mentalizing:
 *    - IL-6 reduces theory of mind task performance
 *    - TNF-α impairs perspective-taking accuracy
 *    - Affects temporoparietal junction (TPJ) and medial prefrontal cortex (mPFC)
 *    - Reduces empathy and emotional recognition accuracy
 *    - Reference: Moieni et al. (2015) "Endotoxin-induced inflammation reduces
 *                 social connection"
 *
 * 2. Inflammation During Development:
 *    - IL-6 exposure during development → autism spectrum traits
 *    - Impaired false belief understanding
 *    - Reduced social motivation
 *    - Reference: Smith et al. (2007) "Maternal immune activation"
 *
 * 3. Sickness Behavior:
 *    - Cytokines induce social withdrawal
 *    - Reduced interest in social interaction
 *    - Impaired ability to infer others' mental states
 *    - Conservation of energy for immune response
 *    - Reference: Dantzer (2001) "Cytokine-induced sickness behavior"
 *
 * 4. Chronic Inflammation:
 *    - Sustained inflammation → persistent ToM deficits
 *    - Reduced empathic accuracy
 *    - Impaired emotional perspective-taking
 *    - Reference: Eisenberger et al. (2010) "Inflammation and social experience"
 *
 * ToM → IMMUNE PATHWAYS:
 * ----------------------
 * 1. Social Rejection and Isolation:
 *    - Social rejection activates neural pain pathways
 *    - Triggers inflammatory response (IL-6, CRP elevation)
 *    - Failed social predictions → stress-induced cytokine release
 *    - Reference: Slavich et al. (2010) "Neural sensitivity to social rejection"
 *
 * 2. Social Prediction Errors:
 *    - Unexpected social outcomes trigger stress response
 *    - HPA axis activation → cortisol → inflammatory rebound
 *    - Failed mentalizing attempts → frustration → IL-1β release
 *    - Reference: Eisenberger & Cole (2012) "Social neuroscience and health"
 *
 * 3. Loneliness and Social Isolation:
 *    - Chronic loneliness increases inflammation markers
 *    - Upregulates pro-inflammatory gene expression
 *    - Downregulates anti-inflammatory genes
 *    - Reference: Cole et al. (2007) "Social regulation of gene expression"
 *
 * 4. Social Connection Benefits:
 *    - Strong social bonds reduce inflammation
 *    - Positive social interactions → IL-10 release
 *    - Accurate mentalizing → reduced social stress
 *    - Reference: Uchino (2006) "Social support and health"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    ToM-IMMUNE BRIDGE                                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → ToM PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-6  → -0.25│  ───────┐                                       │  ║
 * ║   │   │ TNF-α → -0.35│         │                                       │  ║
 * ║   │   │ IL-1β → -0.20│         ├──→ Impaired Mentalizing               │  ║
 * ║   │   │              │         │    (Reduced Perspective Score)        │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     THEORY OF MIND SYSTEM       │                             │  ║
 * ║   │   │  - Perspective-taking impaired  │                             │  ║
 * ║   │   │  - Empathy accuracy reduced     │                             │  ║
 * ║   │   │  - Goal inference confidence ↓  │                             │  ║
 * ║   │   │  - Social motivation reduced    │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   +0.15      │     Recovery, Social Engagement                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ToM → IMMUNE PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ SOCIAL       │ ──→ IL-6 Release                                │  ║
 * ║   │   │ REJECTION    │ ──→ Inflammation Trigger                        │  ║
 * ║   │   │              │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PREDICTION   │ ──→ IL-1β Release                               │  ║
 * ║   │   │ ERRORS       │ ──→ Stress Cytokines                            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ LONELINESS/  │ ──→ Chronic Inflammation                        │  ║
 * ║   │   │ ISOLATION    │ ──→ Pro-inflammatory Gene Expression            │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ SOCIAL       │ ──→ IL-10 Release                               │  ║
 * ║   │   │ CONNECTION   │ ──→ Reduced Inflammation                        │  ║
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

#ifndef NIMCP_TOM_IMMUNE_BRIDGE_H
#define NIMCP_TOM_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_theory_of_mind.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine ToM impact factors (perspective-taking impairment) */
#define CYTOKINE_IL6_TOM_IMPAIRMENT      0.25f   /**< IL-6 → mentalizing impairment */
#define CYTOKINE_TNF_TOM_IMPAIRMENT      0.35f   /**< TNF-α → strong impairment */
#define CYTOKINE_IL1_TOM_IMPAIRMENT      0.20f   /**< IL-1β → mild impairment */
#define CYTOKINE_IFN_GAMMA_TOM_IMPAIRMENT 0.15f  /**< IFN-γ → mild impairment */
#define CYTOKINE_IL10_TOM_RECOVERY        0.15f  /**< IL-10 → recovery boost */

/* Inflammation-mentalizing mapping */
#define INFLAMMATION_TOM_THRESHOLD        0.5f   /**< Inflammation level for ToM impairment */
#define INFLAMMATION_TOM_MAX_IMPAIRMENT   0.8f   /**< Maximum ToM impairment from inflammation */

/* Social stress immune trigger thresholds */
#define SOCIAL_STRESS_IMMUNE_THRESHOLD    0.6f   /**< Social stress to trigger immune */
#define REJECTION_INFLAMMATION_MULTIPLIER 1.8f   /**< Rejection amplifies inflammation */
#define ISOLATION_CHRONIC_THRESHOLD       (86400.0f * 3)  /**< 3 days = chronic isolation */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on Theory of Mind
 *
 * Represents how cytokine levels impair social cognition
 */
typedef struct {
    /* Pro-inflammatory effects on ToM */
    float il6_tom_impairment;         /**< IL-6 induced mentalizing deficit */
    float tnf_tom_impairment;         /**< TNF-α induced ToM deficit */
    float il1_tom_impairment;         /**< IL-1β induced deficit */
    float ifn_gamma_tom_impairment;   /**< IFN-γ induced deficit */

    /* Anti-inflammatory effects */
    float il10_tom_recovery;          /**< IL-10 recovery effect */

    /* Aggregate effects */
    float total_perspective_impairment; /**< Combined perspective-taking impairment [0-1] */
    float empathy_reduction;          /**< Empathy accuracy reduction [0-1] */
    float social_motivation_loss;     /**< Reduced interest in social cognition [0-1] */
    float mentalizing_accuracy_loss;  /**< Goal/belief inference impairment [0-1] */
} cytokine_tom_effects_t;

/**
 * @brief Inflammation effects on social cognition
 *
 * How chronic inflammation affects ToM capacity
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* ToM impacts */
    float perspective_score_reduction; /**< Reduced perspective-taking [0-1] */
    float false_belief_impairment;     /**< Impaired false belief understanding [0-1] */
    float empathy_capacity_loss;       /**< Reduced empathic response [0-1] */
    float social_withdrawal;           /**< Sickness behavior withdrawal [0-1] */

    /* Specific deficits */
    float emotion_inference_impairment; /**< Reduced emotion inference accuracy */
    float goal_inference_impairment;   /**< Reduced goal inference accuracy */
    float intention_inference_impairment; /**< Reduced intention inference accuracy */
} inflammation_tom_state_t;

/**
 * @brief Social stress immune trigger
 *
 * How ToM failures trigger immune response
 */
typedef struct {
    /* Social stress indicators */
    float prediction_error;            /**< Social prediction error [0-1] */
    float rejection_severity;          /**< Social rejection intensity [0-1] */
    float isolation_duration_sec;      /**< How long isolated */
    bool is_chronic_isolation;         /**< >= 3 days */

    /* Immune triggers */
    bool cortisol_triggered;           /**< HPA axis activated */
    bool inflammatory_response;        /**< Cytokine release triggered */
    float immune_activation;           /**< Immune system activation [0-1] */

    /* Chronic effects */
    float chronic_inflammation_risk;   /**< Risk of chronic inflammation [0-1] */
    float gene_expression_changes;     /**< Pro-inflammatory gene upregulation [0-1] */
} social_stress_immune_trigger_t;

/**
 * @brief Social connection immune benefit
 *
 * How positive social cognition boosts immunity
 */
typedef struct {
    /* Social connection indicators */
    float mentalizing_success_rate;    /**< Successful ToM predictions [0-1] */
    float empathy_engagement;          /**< Empathic connection level [0-1] */
    float social_bond_strength;        /**< Social connection quality [0-1] */

    /* Immune benefits */
    float immune_enhancement;          /**< Improved function [0-1] */
    float il10_release_boost;          /**< Anti-inflammatory boost */
    float inflammation_reduction;      /**< Reduced inflammation [0-1] */
    float stress_resistance;           /**< Buffering against stress [0-1] */
} social_connection_immune_boost_t;

/**
 * @brief Complete ToM-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    theory_of_mind_t tom_system;
    brain_immune_system_t* immune_system;

    /* Current state */
    cytokine_tom_effects_t cytokine_effects;
    inflammation_tom_state_t inflammation_state;
    social_stress_immune_trigger_t social_stress_trigger;
    social_connection_immune_boost_t social_connection_boost;

    /* Integration flags */
    bool enable_cytokine_tom_modulation;
    bool enable_inflammation_impairment;
    bool enable_social_stress_immune_trigger;
    bool enable_social_connection_boost;
    bool enable_rejection_inflammation;
    bool enable_isolation_chronic_inflammation;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t social_stress_triggers;
    uint32_t social_connection_boosts;
    uint32_t rejection_inflammations;
    uint32_t isolation_inflammations;
    } tom_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_tom_modulation;
    bool enable_inflammation_impairment;
    bool enable_social_stress_immune_trigger;
    bool enable_social_connection_boost;
    bool enable_rejection_inflammation;
    bool enable_isolation_chronic_inflammation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float social_stress_sensitivity;   /**< Social stress trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float social_stress_threshold;     /**< Social stress to trigger immune [0.4-0.8] */
    float inflammation_tom_threshold;  /**< Inflammation for ToM impairment [0.3-0.7] */
} tom_immune_config_t;

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
int tom_immune_default_config(tom_immune_config_t* config);

/**
 * @brief Create ToM-immune bridge
 *
 * WHAT: Initialize bidirectional ToM-immune integration
 * WHY:  Enable realistic immune-social cognition coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param tom_system Theory of Mind system
 * @param immune_system Brain immune system
 * @return New bridge or NULL on failure
 */
tom_immune_bridge_t* tom_immune_bridge_create(
    const tom_immune_config_t* config,
    theory_of_mind_t tom_system,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy ToM-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void tom_immune_bridge_destroy(tom_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → ToM API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to ToM capacity
 *
 * WHAT: Impair mentalizing based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce social cognition
 * HOW:  Query immune system cytokines, reduce perspective score
 *
 * @param bridge ToM-immune bridge
 * @return 0 on success
 */
int tom_immune_apply_cytokine_effects(tom_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to ToM
 *
 * WHAT: Induce mentalizing deficits from prolonged inflammation
 * WHY:  Chronic inflammation causes persistent ToM impairment
 * HOW:  Check inflammation duration/level, reduce empathy capacity
 *
 * @param bridge ToM-immune bridge
 * @return 0 on success
 */
int tom_immune_apply_inflammation_effects(tom_immune_bridge_t* bridge);

/**
 * @brief Compute ToM impairment level from inflammation
 *
 * WHAT: Calculate social cognition impairment from immune state
 * WHY:  Inflammation reduces mentalizing accuracy
 * HOW:  Map inflammation level/duration to impairment [0-1]
 *
 * @param bridge ToM-immune bridge
 * @return Impairment level [0-1]
 */
float tom_immune_compute_impairment(const tom_immune_bridge_t* bridge);

/**
 * @brief Reduce perspective-taking score from cytokines
 *
 * WHAT: Impair ability to distinguish self vs. other perspectives
 * WHY:  IL-6 and TNF-α reduce TPJ and mPFC function
 * HOW:  Scale perspective score by cytokine levels
 *
 * @param bridge ToM-immune bridge
 * @return 0 on success
 */
int tom_immune_impair_perspective_taking(tom_immune_bridge_t* bridge);

/**
 * @brief Reduce empathy accuracy from inflammation
 *
 * WHAT: Impair emotional empathy and mirroring
 * WHY:  Inflammation reduces mirror neuron activation
 * HOW:  Query inflammation, reduce empathy confidence
 *
 * @param bridge ToM-immune bridge
 * @return 0 on success
 */
int tom_immune_impair_empathy(tom_immune_bridge_t* bridge);

/* ============================================================================
 * ToM → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from social rejection
 *
 * WHAT: Activate immune system from social rejection event
 * WHY:  Social rejection activates inflammatory response
 * HOW:  Release IL-6 and other pro-inflammatory cytokines
 *
 * @param bridge ToM-immune bridge
 * @param rejection_severity Severity of rejection [0-1]
 * @return 0 on success
 */
int tom_immune_trigger_from_rejection(
    tom_immune_bridge_t* bridge,
    float rejection_severity
);

/**
 * @brief Trigger immune response from social prediction error
 *
 * WHAT: Activate immune from failed social prediction
 * WHY:  Prediction errors trigger stress-induced cytokine release
 * HOW:  Release IL-1β proportional to prediction error
 *
 * @param bridge ToM-immune bridge
 * @param prediction_error Magnitude of error [0-1]
 * @return 0 on success
 */
int tom_immune_trigger_from_prediction_error(
    tom_immune_bridge_t* bridge,
    float prediction_error
);

/**
 * @brief Trigger chronic inflammation from isolation
 *
 * WHAT: Activate sustained inflammation from social isolation
 * WHY:  Chronic loneliness increases pro-inflammatory gene expression
 * HOW:  Track isolation duration, trigger chronic inflammation
 *
 * @param bridge ToM-immune bridge
 * @param isolation_duration_sec How long isolated
 * @return 0 on success
 */
int tom_immune_trigger_from_isolation(
    tom_immune_bridge_t* bridge,
    float isolation_duration_sec
);

/**
 * @brief Boost immune function from social connection
 *
 * WHAT: Enhance immunity from positive social cognition
 * WHY:  Successful mentalizing and social bonds reduce inflammation
 * HOW:  Track social success, release IL-10, reduce inflammation
 *
 * @param bridge ToM-immune bridge
 * @param connection_strength Social connection quality [0-1]
 * @return 0 on success
 */
int tom_immune_boost_from_social_connection(
    tom_immune_bridge_t* bridge,
    float connection_strength
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update ToM-immune bridge (both directions)
 *
 * WHAT: Process all ToM-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from social stress
 *
 * @param bridge ToM-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int tom_immune_bridge_update(
    tom_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine ToM effects
 *
 * @param bridge ToM-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int tom_immune_get_cytokine_effects(
    const tom_immune_bridge_t* bridge,
    cytokine_tom_effects_t* effects
);

/**
 * @brief Get current inflammation ToM state
 *
 * @param bridge ToM-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int tom_immune_get_inflammation_state(
    const tom_immune_bridge_t* bridge,
    inflammation_tom_state_t* state
);

/**
 * @brief Check if experiencing sickness behavior social withdrawal
 *
 * WHAT: Determine if cytokines inducing social withdrawal
 * WHY:  Sickness behavior is distinct reduction in social interest
 * HOW:  Check cytokine levels and social motivation loss
 *
 * @param bridge ToM-immune bridge
 * @return true if experiencing social withdrawal
 */
bool tom_immune_is_social_withdrawal(const tom_immune_bridge_t* bridge);

/**
 * @brief Get current ToM impairment severity
 *
 * @param bridge ToM-immune bridge
 * @return Impairment level [0-1]
 */
float tom_immune_get_impairment_severity(const tom_immune_bridge_t* bridge);

/**
 * @brief Get perspective-taking reduction
 *
 * @param bridge ToM-immune bridge
 * @return Perspective score reduction [0-1]
 */
float tom_immune_get_perspective_impairment(const tom_immune_bridge_t* bridge);

/**
 * @brief Get empathy accuracy reduction
 *
 * @param bridge ToM-immune bridge
 * @return Empathy impairment [0-1]
 */
float tom_immune_get_empathy_impairment(const tom_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_TOM
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int tom_immune_connect_bio_async(tom_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int tom_immune_disconnect_bio_async(tom_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool tom_immune_is_bio_async_connected(const tom_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOM_IMMUNE_BRIDGE_H */

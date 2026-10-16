/**
 * @file nimcp_nlp_immune_bridge.h
 * @brief NLP-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and NLP processing
 * WHY:  Biological evidence shows inflammation impairs language processing and
 *       language errors/confusion can trigger stress responses. Essential for
 *       realistic cognitive-immune modeling in language systems.
 * HOW:  Cytokines reduce language capacity and increase errors; language processing
 *       failures trigger immune responses; successful comprehension reduces inflammation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → NLP PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair language production and comprehension
 *    - Reduce vocabulary access (word-finding difficulties)
 *    - Increase semantic errors
 *    - Impair syntactic processing
 *    - Reference: Krabbe et al. (2005) "Brain-derived neurotrophic factor inhibits
 *      tumor necrosis factor-α-induced apoptosis"
 *    - Reference: Reichenberg et al. (2001) "Cytokine-associated emotional and
 *      cognitive disturbances in humans"
 *
 * 2. Chronic Inflammation:
 *    - Sustained elevation → language deficits
 *    - Reduced verbal fluency
 *    - Impaired comprehension
 *    - Aphasia-like symptoms in severe cases
 *    - Reference: Marsland et al. (2006) "Brain morphology links systemic
 *      inflammation to cognitive function"
 *
 * 3. Inflammation-Induced Language Errors:
 *    - High inflammation → increased paraphasias
 *    - Semantic confusion (using wrong words)
 *    - Grammatical simplification
 *    - Reduced sentence complexity
 *    - Reference: Easterbrook (1959) "The effect of emotion on cue utilization"
 *
 * 4. Cytokine Storm Effects:
 *    - Severe inflammation → language breakdown
 *    - Incoherent speech production
 *    - Comprehension failure
 *    - Reference: Pandharipande et al. (2013) "Inflammation and delirium"
 *
 * NLP → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Language Processing Errors:
 *    - Failed comprehension → stress response
 *    - Semantic confusion → cortisol elevation
 *    - Syntactic complexity → cognitive load → inflammation
 *    - Reference: Elenkov et al. (2000) "The sympathetic nerve - an integrative
 *      interface between two supersystems"
 *
 * 2. Successful Language Processing:
 *    - Comprehension success → anti-inflammatory IL-10 release
 *    - Clear communication → reduced stress
 *    - Language mastery → improved immune regulation
 *    - Reference: Davidson et al. (2003) "Alterations in brain and immune
 *      function produced by mindfulness meditation"
 *
 * 3. Language-Mediated Stress:
 *    - Communication failures → chronic inflammation
 *    - Ambiguity processing → elevated inflammatory markers
 *    - Language barriers → sustained cortisol elevation
 *    - Reference: Brosschot et al. (2006) "The perseverative cognition hypothesis"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                        NLP-IMMUNE BRIDGE                                   ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → NLP PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → -0.2 │         │                                       │  ║
 * ║   │   │ TNF-α → -0.4 │         ├──→ Language Impairment                │  ║
 * ║   │   │              │         │    (Reduced vocab, errors)            │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     NLP SYSTEM                  │                             │  ║
 * ║   │   │  - Vocab access reduction       │                             │  ║
 * ║   │   │  - Semantic error increase      │                             │  ║
 * ║   │   │  - Syntactic simplification     │                             │  ║
 * ║   │   │  - Comprehension deficit        │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -10% capacity │                                     │  ║
 * ║   │   │ REGIONAL → -30% capacity │                                     │  ║
 * ║   │   │ SYSTEMIC → -50% capacity │                                     │  ║
 * ║   │   │ STORM    → -80% capacity │                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  NLP → IMMUNE PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ ERRORS       │ ──→ Stress Response                             │  ║
 * ║   │   │ CONFUSION    │ ──→ Inflammatory Cytokine Release               │  ║
 * ║   │   │ COMPLEXITY   │ ──→ Cognitive Load → Inflammation               │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ SUCCESS      │ ──→ IL-10 Release                               │  ║
 * ║   │   │ CLARITY      │ ──→ Reduced Inflammation                        │  ║
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

#ifndef NIMCP_NLP_IMMUNE_BRIDGE_H
#define NIMCP_NLP_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "nlp/nimcp_nlp.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine language impact factors */
#define CYTOKINE_IL1_LANGUAGE_IMPACT      -0.3f   /**< IL-1β → language deficit */
#define CYTOKINE_IL6_LANGUAGE_IMPACT      -0.2f   /**< IL-6 → language deficit */
#define CYTOKINE_TNF_LANGUAGE_IMPACT      -0.4f   /**< TNF-α → strong deficit */
#define CYTOKINE_IFN_GAMMA_LANGUAGE_IMPACT -0.15f /**< IFN-γ → mild deficit */
#define CYTOKINE_IL10_LANGUAGE_IMPACT      0.2f   /**< IL-10 → recovery boost */

/* Inflammation language capacity reduction */
#define INFLAMMATION_NONE_LANG_CAPACITY     1.0f   /**< No reduction */
#define INFLAMMATION_LOCAL_LANG_CAPACITY    0.9f   /**< -10% capacity */
#define INFLAMMATION_REGIONAL_LANG_CAPACITY 0.7f   /**< -30% capacity */
#define INFLAMMATION_SYSTEMIC_LANG_CAPACITY 0.5f   /**< -50% capacity */
#define INFLAMMATION_STORM_LANG_CAPACITY    0.2f   /**< -80% capacity (aphasia) */

/* Language error rates per inflammation level */
#define INFLAMMATION_ERROR_BASE           0.05f    /**< Base error rate */
#define INFLAMMATION_ERROR_PER_LEVEL      0.10f    /**< Per inflammation level */

/* Language processing thresholds */
#define LANGUAGE_ERROR_IMMUNE_THRESHOLD   0.3f     /**< Error rate → immune activation */
#define LANGUAGE_SUCCESS_IL10_BOOST       0.25f    /**< Success → IL-10 */
#define LANGUAGE_COMPLEXITY_THRESHOLD     0.7f     /**< Complexity → stress */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine language effects
 *
 * Represents how cytokine levels impair language processing
 */
typedef struct {
    /* Pro-inflammatory effects */
    float il1_language_deficit;        /**< IL-1β induced deficit */
    float il6_language_deficit;        /**< IL-6 induced deficit */
    float tnf_language_deficit;        /**< TNF-α induced deficit */
    float ifn_gamma_language_deficit;  /**< IFN-γ induced deficit */

    /* Anti-inflammatory effects */
    float il10_language_recovery;      /**< IL-10 recovery boost */

    /* Aggregate effects */
    float total_capacity_reduction;     /**< Combined capacity loss [0-1] */
    float vocab_access_impairment;      /**< Vocabulary access deficit [0-1] */
    float semantic_error_rate;          /**< Semantic error probability [0-1] */
    float syntactic_impairment;         /**< Syntactic processing deficit [0-1] */
    float comprehension_deficit;        /**< Comprehension impairment [0-1] */
} nlp_cytokine_effects_t;

/**
 * @brief Inflammation language state
 *
 * How chronic inflammation affects language processing
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */
    bool is_chronic;                    /**< >= threshold */

    /* Language impacts */
    float capacity_factor;              /**< Overall capacity [0-1] */
    float vocab_reduction;              /**< Vocabulary accessibility [0-1] */
    float error_rate;                   /**< Error probability [0-1] */
    float fluency_impairment;           /**< Production fluency [0-1] */
    float complexity_reduction;         /**< Syntactic complexity [0-1] */

    /* Error tracking */
    uint32_t semantic_errors;           /**< Count of semantic errors */
    uint32_t syntactic_errors;          /**< Count of syntactic errors */
    uint32_t comprehension_failures;    /**< Count of comprehension failures */
} nlp_inflammation_state_t;

/**
 * @brief Language-driven immune modulation
 *
 * How language processing affects immune function
 */
typedef struct {
    /* Language state */
    float processing_success_rate;      /**< Success rate [0-1] */
    float error_rate;                   /**< Error rate [0-1] */
    float complexity_level;             /**< Input complexity [0-1] */
    float cognitive_load;               /**< Processing load [0-1] */

    /* Immune effects */
    bool error_induced_inflammation;    /**< Errors triggered inflammation */
    float il10_release_from_success;    /**< IL-10 from success */
    float inflammation_from_complexity; /**< Inflammation from load */

    /* Statistics */
    uint32_t total_processed;           /**< Total inputs processed */
    uint32_t successful_parses;         /**< Successful comprehensions */
    uint32_t failed_parses;             /**< Failed comprehensions */
} nlp_immune_modulation_t;

/**
 * @brief NLP-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_language_impairment;
    bool enable_inflammation_errors;
    bool enable_error_immune_activation;
    bool enable_success_il10_release;
    bool enable_complexity_inflammation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;     /**< Inflammation effect multiplier [0.5-2.0] */
    float error_immune_sensitivity;     /**< Error→immune multiplier [0.5-2.0] */

    /* Thresholds */
    float error_threshold;              /**< Error rate for immune activation [0.1-0.5] */
    float complexity_threshold;         /**< Complexity for inflammation [0.5-0.9] */
    float success_threshold;            /**< Success rate for IL-10 [0.6-0.95] */
} nlp_immune_config_t;

/**
 * @brief Complete NLP-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    nlp_network_t nlp_network;

    /* Current state */
    nlp_cytokine_effects_t cytokine_effects;
    nlp_inflammation_state_t inflammation_state;
    nlp_immune_modulation_t nlp_modulation;

    /* Configuration */
    nlp_immune_config_t config;

    /* Integration flags */
    bool enable_cytokine_language_impairment;
    bool enable_inflammation_errors;
    bool enable_error_immune_activation;
    bool enable_success_il10_release;
    bool enable_complexity_inflammation;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_impairments;
    uint32_t error_triggers;
    uint32_t success_boosts;
    uint32_t complexity_inflammation_events;

    } nlp_immune_bridge_t;

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
int nlp_immune_default_config(nlp_immune_config_t* config);

/**
 * @brief Create NLP-immune bridge
 *
 * WHAT: Initialize bidirectional NLP-immune integration
 * WHY:  Enable realistic immune-language coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param nlp_network NLP network
 * @return New bridge or NULL on failure
 */
nlp_immune_bridge_t* nlp_immune_bridge_create(
    const nlp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    nlp_network_t nlp_network
);

/**
 * @brief Destroy NLP-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void nlp_immune_bridge_destroy(nlp_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → NLP API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to language processing
 *
 * WHAT: Impair language based on cytokine levels
 * WHY:  Pro-inflammatory cytokines reduce language capacity
 * HOW:  Query immune system cytokines, reduce vocab/fluency
 *
 * @param bridge NLP-immune bridge
 * @return 0 on success
 */
int nlp_immune_apply_cytokine_effects(nlp_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to language
 *
 * WHAT: Increase errors and reduce capacity from inflammation
 * WHY:  Inflammation reduces cognitive capacity
 * HOW:  Check inflammation level/duration, adjust NLP parameters
 *
 * @param bridge NLP-immune bridge
 * @return 0 on success
 */
int nlp_immune_apply_inflammation_effects(nlp_immune_bridge_t* bridge);

/**
 * @brief Compute language capacity from immune state
 *
 * WHAT: Calculate overall language capacity given immune status
 * WHY:  Inflammation reduces language resources
 * HOW:  Map inflammation level to capacity factor [0-1]
 *
 * @param bridge NLP-immune bridge
 * @return Capacity factor [0-1] (1.0 = normal, 0.0 = complete impairment)
 */
float nlp_immune_compute_capacity(const nlp_immune_bridge_t* bridge);

/**
 * @brief Compute error rate from inflammation
 *
 * WHAT: Calculate how much inflammation increases language errors
 * WHY:  High inflammation increases semantic/syntactic errors
 * HOW:  Map inflammation level to error probability
 *
 * @param bridge NLP-immune bridge
 * @return Error rate [0-1] (0 = no errors, 1 = maximal errors)
 */
float nlp_immune_compute_error_rate(const nlp_immune_bridge_t* bridge);

/* ============================================================================
 * NLP → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from language errors
 *
 * WHAT: Activate inflammatory response from processing failures
 * WHY:  Language errors signal cognitive distress → stress response
 * HOW:  Track error rate, trigger cytokine release if threshold exceeded
 *
 * @param bridge NLP-immune bridge
 * @param error_rate Current error rate [0-1]
 * @return 0 on success
 */
int nlp_immune_trigger_error_inflammation(
    nlp_immune_bridge_t* bridge,
    float error_rate
);

/**
 * @brief Release IL-10 from language success
 *
 * WHAT: Trigger anti-inflammatory response from successful processing
 * WHY:  Successful comprehension reduces stress and inflammation
 * HOW:  Detect high success rate, release IL-10 cytokine
 *
 * @param bridge NLP-immune bridge
 * @param success_rate Current success rate [0-1]
 * @return 0 on success
 */
int nlp_immune_release_il10_from_success(
    nlp_immune_bridge_t* bridge,
    float success_rate
);

/**
 * @brief Trigger inflammation from language complexity
 *
 * WHAT: Activate inflammatory response from high cognitive load
 * WHY:  Complex language processing → cognitive stress → inflammation
 * HOW:  Monitor complexity, trigger cytokine release if sustained
 *
 * @param bridge NLP-immune bridge
 * @param complexity_level Current complexity [0-1]
 * @return 0 on success
 */
int nlp_immune_trigger_complexity_inflammation(
    nlp_immune_bridge_t* bridge,
    float complexity_level
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update NLP-immune bridge (both directions)
 *
 * WHAT: Process all NLP-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from errors, adjust parameters
 *
 * @param bridge NLP-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int nlp_immune_bridge_update(
    nlp_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply modulation to NLP processing
 *
 * WHAT: Apply immune-derived modulation to NLP network
 * WHY:  Modify NLP behavior based on immune state
 * HOW:  Adjust learning rates, error injection, capacity limits
 *
 * @param bridge NLP-immune bridge
 * @return 0 on success
 */
int nlp_immune_apply_modulation(nlp_immune_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine language effects
 *
 * @param bridge NLP-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int nlp_immune_get_cytokine_effects(
    const nlp_immune_bridge_t* bridge,
    nlp_cytokine_effects_t* effects
);

/**
 * @brief Get current inflammation language state
 *
 * @param bridge NLP-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int nlp_immune_get_inflammation_state(
    const nlp_immune_bridge_t* bridge,
    nlp_inflammation_state_t* state
);

/**
 * @brief Check if experiencing language deficit from inflammation
 *
 * WHAT: Determine if inflammation causing significant language impairment
 * WHY:  Detect clinically significant language effects
 * HOW:  Check capacity reduction threshold
 *
 * @param bridge NLP-immune bridge
 * @return true if significant deficit (>30% capacity loss)
 */
bool nlp_immune_has_language_deficit(const nlp_immune_bridge_t* bridge);

/**
 * @brief Get current language capacity factor
 *
 * @param bridge NLP-immune bridge
 * @return Capacity factor [0-1]
 */
float nlp_immune_get_capacity_factor(const nlp_immune_bridge_t* bridge);

/**
 * @brief Get current language error rate
 *
 * @param bridge NLP-immune bridge
 * @return Error rate [0-1]
 */
float nlp_immune_get_error_rate(const nlp_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_NLP_CORE
 *
 * @param bridge NLP-immune bridge
 * @return 0 on success, -1 on error
 */
int nlp_immune_connect_bio_async(nlp_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge NLP-immune bridge
 * @return 0 on success
 */
int nlp_immune_disconnect_bio_async(nlp_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge NLP-immune bridge
 * @return true if connected
 */
bool nlp_immune_is_bio_async_connected(const nlp_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NLP_IMMUNE_BRIDGE_H */

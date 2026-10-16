//=============================================================================
// nimcp_language_immune_bridge.h - Language-Immune Bridge Integration
//=============================================================================
/**
 * @file nimcp_language_immune_bridge.h
 * @brief Consolidated bridge integrating Language Layer with Immune System
 *
 * WHAT: Unified immune bridge for all language processing regions
 * WHY:  Model neuroinflammation effects on language (aphasia, etc.)
 * HOW:  Cytokine levels affect Wernicke (comprehension), Broca (production),
 *       and NLP networks
 *
 * BIOLOGICAL BASIS:
 * - Neuroinflammation impairs language processing
 * - Wernicke's aphasia: comprehension deficits from temporal lobe inflammation
 * - Broca's aphasia: production deficits from frontal lobe inflammation
 * - Cytokines (IL-1β, IL-6, TNF-α) impair neural function
 * - IL-10 promotes recovery and anti-inflammatory effects
 * - Microglia activation affects synaptic plasticity
 *
 * CONSOLIDATES:
 * - Previously separate Wernicke immune bridge
 * - Previously separate Broca immune bridge
 * - NLP network immune effects
 *
 * KEY CONNECTIONS:
 * - Brain Immune System: Cytokine levels, inflammation state
 * - Wernicke Adapter: Comprehension impairment
 * - Broca Adapter: Production impairment
 * - NLP Network: Processing speed, attention effects
 *
 * @version 1.0.0 - Phase L3: Immune Consolidation
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_IMMUNE_BRIDGE_H
#define NIMCP_LANGUAGE_IMMUNE_BRIDGE_H

#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_immune_bridge language_immune_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct brain_immune_system brain_immune_system_t;
typedef struct wernicke_adapter wernicke_adapter_t;
typedef struct broca_adapter broca_adapter_t;
typedef struct nlp_network_struct* nlp_network_t;

/* bio_router_t is a pointer type defined in bio_router.h */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define LANGUAGE_IMMUNE_MODULE_NAME      "language_immune_bridge"
#define LANGUAGE_IMMUNE_MODULE_VERSION   "1.0.0"

/** Default configuration values */
#define LANGUAGE_IMMUNE_DEFAULT_UPDATE_INTERVAL_MS    100
#define LANGUAGE_IMMUNE_DEFAULT_RECOVERY_RATE         0.01f

/** Cytokine sensitivity defaults */
#define LANGUAGE_IMMUNE_DEFAULT_IL1B_SENSITIVITY      1.0f
#define LANGUAGE_IMMUNE_DEFAULT_IL6_SENSITIVITY       0.8f
#define LANGUAGE_IMMUNE_DEFAULT_TNFA_SENSITIVITY      1.2f
#define LANGUAGE_IMMUNE_DEFAULT_IL10_SENSITIVITY      0.5f

/** Inflammation thresholds */
#define LANGUAGE_IMMUNE_MILD_THRESHOLD                0.3f
#define LANGUAGE_IMMUNE_MODERATE_THRESHOLD            0.5f
#define LANGUAGE_IMMUNE_SEVERE_THRESHOLD              0.7f

/** Impairment scaling */
#define LANGUAGE_IMMUNE_COMPREHENSION_SCALE           1.0f
#define LANGUAGE_IMMUNE_PRODUCTION_SCALE              1.0f
#define LANGUAGE_IMMUNE_FLUENCY_SCALE                 0.8f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Inflammation severity levels
 */
typedef enum {
    INFLAMMATION_NONE = 0,            /**< No inflammation */
    INFLAMMATION_MILD,                /**< Mild - subtle effects */
    INFLAMMATION_MODERATE,            /**< Moderate - noticeable effects */
    INFLAMMATION_SEVERE,              /**< Severe - significant impairment */
    INFLAMMATION_CRITICAL,            /**< Critical - near-complete impairment */
    INFLAMMATION_COUNT
} inflammation_level_t;

/**
 * @brief Aphasia symptom types
 */
typedef enum {
    APHASIA_NONE = 0,                 /**< No aphasia symptoms */
    APHASIA_WERNICKE,                 /**< Fluent aphasia (comprehension deficit) */
    APHASIA_BROCA,                    /**< Non-fluent aphasia (production deficit) */
    APHASIA_CONDUCTION,               /**< Repetition deficit */
    APHASIA_GLOBAL,                   /**< Both comprehension and production */
    APHASIA_ANOMIC,                   /**< Word-finding difficulty */
    APHASIA_COUNT
} aphasia_type_t;

/**
 * @brief Language region types for immune targeting
 */
typedef enum {
    REGION_WERNICKE = 0,              /**< Wernicke's area (BA22) */
    REGION_BROCA,                     /**< Broca's area (BA44/45) */
    REGION_ANGULAR_GYRUS,             /**< Angular gyrus (BA39) */
    REGION_ARCUATE_FASCICULUS,        /**< Arcuate fasciculus tract */
    REGION_STG,                       /**< Superior temporal gyrus */
    REGION_MTG,                       /**< Middle temporal gyrus */
    REGION_IFG,                       /**< Inferior frontal gyrus */
    REGION_COUNT
} language_region_t;

/**
 * @brief Recovery phase
 */
typedef enum {
    RECOVERY_NONE = 0,                /**< No active recovery */
    RECOVERY_ACUTE,                   /**< Acute phase */
    RECOVERY_SUBACUTE,                /**< Subacute recovery */
    RECOVERY_CHRONIC,                 /**< Chronic/long-term */
    RECOVERY_COMPLETE,                /**< Full recovery */
    RECOVERY_COUNT
} recovery_phase_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Cytokine levels affecting language
 */
typedef struct {
    float il1_beta;                   /**< IL-1β level [0-1] */
    float il6;                        /**< IL-6 level [0-1] */
    float tnf_alpha;                  /**< TNF-α level [0-1] */
    float il10;                       /**< IL-10 level [0-1] (anti-inflammatory) */
    float composite_inflammatory;     /**< Composite inflammation score */
    uint64_t last_update_ms;          /**< Last update timestamp */
} language_cytokine_state_t;

/**
 * @brief Region-specific inflammation state
 */
typedef struct {
    language_region_t region;         /**< Affected region */
    float inflammation_level;         /**< Inflammation [0-1] */
    inflammation_level_t severity;    /**< Severity classification */
    float impairment_factor;          /**< Resulting impairment [0-1] */
    float recovery_progress;          /**< Recovery progress [0-1] */
    recovery_phase_t recovery_phase;  /**< Recovery phase */
    uint64_t inflammation_onset_ms;   /**< When inflammation started */
    uint64_t peak_inflammation_ms;    /**< Peak inflammation time */
} region_inflammation_t;

/**
 * @brief Wernicke-specific immune effects (comprehension)
 */
typedef struct {
    /* Inflammation state */
    region_inflammation_t inflammation;  /**< Wernicke inflammation */

    /* Comprehension effects */
    float phonological_impairment;    /**< Phoneme processing deficit */
    float lexical_impairment;         /**< Word recognition deficit */
    float semantic_impairment;        /**< Meaning extraction deficit */
    float syntactic_impairment;       /**< Grammar processing deficit */

    /* Aphasia symptoms */
    bool fluent_speech;               /**< Fluent but empty speech */
    bool paraphasia_phonemic;         /**< Sound substitutions */
    bool paraphasia_semantic;         /**< Word substitutions */
    bool neologisms;                  /**< Made-up words */
    bool jargon;                      /**< Jargon aphasia */

    /* Overall */
    float comprehension_capacity;     /**< Remaining capacity [0-1] */
    aphasia_type_t aphasia_type;      /**< Current aphasia classification */
} wernicke_immune_effects_t;

/**
 * @brief Broca-specific immune effects (production)
 */
typedef struct {
    /* Inflammation state */
    region_inflammation_t inflammation;  /**< Broca inflammation */

    /* Production effects */
    float articulatory_impairment;    /**< Motor speech deficit */
    float syntactic_impairment;       /**< Grammar production deficit */
    float fluency_impairment;         /**< Speech fluency deficit */
    float word_finding_impairment;    /**< Word retrieval deficit */

    /* Aphasia symptoms */
    bool telegraphic_speech;          /**< Reduced grammar */
    bool agrammatism;                 /**< Grammar loss */
    bool apraxia_of_speech;           /**< Motor planning deficit */
    bool effortful_production;        /**< Labored speech */

    /* Overall */
    float production_capacity;        /**< Remaining capacity [0-1] */
    aphasia_type_t aphasia_type;      /**< Current aphasia classification */
} broca_immune_effects_t;

/**
 * @brief NLP network immune effects
 */
typedef struct {
    /* Inflammation */
    float network_inflammation;       /**< Overall network inflammation */

    /* Processing effects */
    float attention_impairment;       /**< Attention processing deficit */
    float embedding_quality;          /**< Embedding quality reduction */
    float processing_speed;           /**< Processing speed factor */

    /* Control */
    bool processing_degraded;         /**< Quality degradation active */
} nlp_immune_effects_t;

/**
 * @brief Composite language impairment
 */
typedef struct {
    /* Overall scores */
    float comprehension_score;        /**< Overall comprehension [0-1] */
    float production_score;           /**< Overall production [0-1] */
    float repetition_score;           /**< Repetition ability [0-1] */
    float naming_score;               /**< Naming ability [0-1] */

    /* Composite */
    float language_ability;           /**< Overall language [0-1] */
    aphasia_type_t dominant_aphasia;  /**< Primary aphasia type */

    /* Trend */
    bool improving;                   /**< Recovery trending positive */
    float recovery_rate;              /**< Current recovery rate */
} language_impairment_summary_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Inflammation tracking */
    uint64_t inflammation_events;     /**< Total inflammation events */
    float max_wernicke_inflammation;  /**< Peak Wernicke inflammation */
    float max_broca_inflammation;     /**< Peak Broca inflammation */
    float time_inflamed_ms;           /**< Total time inflamed */

    /* Impairment tracking */
    float avg_comprehension_impairment;  /**< Average comprehension deficit */
    float avg_production_impairment;     /**< Average production deficit */

    /* Recovery tracking */
    uint64_t recovery_events;         /**< Recovery event count */
    float avg_recovery_time_ms;       /**< Average recovery time */

    /* Performance */
    float avg_processing_time_ms;     /**< Average processing time */
    uint64_t last_update_time_ms;     /**< Last update timestamp */
} language_immune_stats_t;

//=============================================================================
// Bridge State Structure
//=============================================================================

/**
 * @brief Language-immune bridge state
 */
struct language_immune_bridge {
    /* Configuration */
    language_immune_config_t config;  /**< Bridge configuration */
    bool initialized;                 /**< Initialization state */
    bool active;                      /**< Active processing */

    /* Connected components */
    language_orchestrator_t* orchestrator;    /**< Parent orchestrator */
    brain_immune_system_t* brain_immune;      /**< Brain immune system */
    wernicke_adapter_t* wernicke;             /**< Wernicke adapter */
    broca_adapter_t* broca;                   /**< Broca adapter */
    nlp_network_t nlp_network;                /**< NLP network */

    /* Cytokine state */
    language_cytokine_state_t cytokines;      /**< Current cytokine levels */

    /* Region-specific effects */
    wernicke_immune_effects_t wernicke_effects;  /**< Wernicke effects */
    broca_immune_effects_t broca_effects;        /**< Broca effects */
    nlp_immune_effects_t nlp_effects;            /**< NLP effects */

    /* Summary */
    language_impairment_summary_t summary;    /**< Impairment summary */

    /* Immune memory (pattern tracking) */
    float* inflammation_history;      /**< Historical inflammation levels */
    uint32_t history_size;            /**< History buffer size */
    uint32_t history_idx;             /**< Current history index */
    bool immune_memory_enabled;       /**< Track patterns */

    /* Statistics */
    language_immune_stats_t stats;    /**< Bridge statistics */

    /* Bio-async */
    bio_router_t* bio_router;         /**< Bio-async router */
    bool bio_async_registered;        /**< Registration status */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create language-immune bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
language_immune_bridge_t* language_immune_bridge_create(
    const language_immune_config_t* config
);

/**
 * @brief Destroy language-immune bridge
 *
 * @param bridge Bridge instance
 */
void language_immune_bridge_destroy(language_immune_bridge_t* bridge);

/**
 * @brief Initialize bridge with default configuration
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_init(language_immune_bridge_t* bridge);

/**
 * @brief Start bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_start(language_immune_bridge_t* bridge);

/**
 * @brief Stop bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_stop(language_immune_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to language orchestrator
 *
 * @param bridge Bridge instance
 * @param orchestrator Language orchestrator
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_connect_orchestrator(
    language_immune_bridge_t* bridge,
    language_orchestrator_t* orchestrator
);

/**
 * @brief Connect to brain immune system
 *
 * @param bridge Bridge instance
 * @param brain_immune Brain immune system
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_connect_brain_immune(
    language_immune_bridge_t* bridge,
    brain_immune_system_t* brain_immune
);

/**
 * @brief Connect to Wernicke adapter
 *
 * @param bridge Bridge instance
 * @param wernicke Wernicke adapter
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_connect_wernicke(
    language_immune_bridge_t* bridge,
    wernicke_adapter_t* wernicke
);

/**
 * @brief Connect to Broca adapter
 *
 * @param bridge Bridge instance
 * @param broca Broca adapter
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_connect_broca(
    language_immune_bridge_t* bridge,
    broca_adapter_t* broca
);

/**
 * @brief Connect to NLP network
 *
 * @param bridge Bridge instance
 * @param nlp_network NLP network
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_connect_nlp(
    language_immune_bridge_t* bridge,
    nlp_network_t nlp_network
);

//=============================================================================
// Cytokine API
//=============================================================================

/**
 * @brief Update cytokine levels from brain immune system
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_update_cytokines(
    language_immune_bridge_t* bridge
);

/**
 * @brief Get current cytokine state
 *
 * @param bridge Bridge instance
 * @param cytokines Output cytokine state
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_get_cytokines(
    const language_immune_bridge_t* bridge,
    language_cytokine_state_t* cytokines
);

/**
 * @brief Set cytokine sensitivity
 *
 * @param bridge Bridge instance
 * @param il1b_sens IL-1β sensitivity
 * @param il6_sens IL-6 sensitivity
 * @param tnfa_sens TNF-α sensitivity
 * @param il10_sens IL-10 sensitivity
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_set_sensitivities(
    language_immune_bridge_t* bridge,
    float il1b_sens,
    float il6_sens,
    float tnfa_sens,
    float il10_sens
);

//=============================================================================
// Inflammation API
//=============================================================================

/**
 * @brief Get inflammation level for region
 *
 * @param bridge Bridge instance
 * @param region Language region
 * @return Inflammation level [0-1]
 */
float language_immune_bridge_get_inflammation(
    const language_immune_bridge_t* bridge,
    language_region_t region
);

/**
 * @brief Get inflammation severity for region
 *
 * @param bridge Bridge instance
 * @param region Language region
 * @return Inflammation severity
 */
inflammation_level_t language_immune_bridge_get_severity(
    const language_immune_bridge_t* bridge,
    language_region_t region
);

/**
 * @brief Check if region is inflamed
 *
 * @param bridge Bridge instance
 * @param region Language region
 * @return true if inflammation > threshold
 */
bool language_immune_bridge_is_inflamed(
    const language_immune_bridge_t* bridge,
    language_region_t region
);

//=============================================================================
// Impairment API
//=============================================================================

/**
 * @brief Get Wernicke impairment effects
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_get_wernicke_effects(
    const language_immune_bridge_t* bridge,
    wernicke_immune_effects_t* effects
);

/**
 * @brief Get Broca impairment effects
 *
 * @param bridge Bridge instance
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_get_broca_effects(
    const language_immune_bridge_t* bridge,
    broca_immune_effects_t* effects
);

/**
 * @brief Get language impairment summary
 *
 * @param bridge Bridge instance
 * @param summary Output summary
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_get_impairment_summary(
    const language_immune_bridge_t* bridge,
    language_impairment_summary_t* summary
);

/**
 * @brief Get current aphasia classification
 *
 * @param bridge Bridge instance
 * @return Dominant aphasia type
 */
aphasia_type_t language_immune_bridge_get_aphasia_type(
    const language_immune_bridge_t* bridge
);

/**
 * @brief Get comprehension capacity factor
 *
 * @param bridge Bridge instance
 * @return Comprehension capacity [0-1] (1 = full, 0 = none)
 */
float language_immune_bridge_get_comprehension_capacity(
    const language_immune_bridge_t* bridge
);

/**
 * @brief Get production capacity factor
 *
 * @param bridge Bridge instance
 * @return Production capacity [0-1] (1 = full, 0 = none)
 */
float language_immune_bridge_get_production_capacity(
    const language_immune_bridge_t* bridge
);

//=============================================================================
// Recovery API
//=============================================================================

/**
 * @brief Get recovery phase for region
 *
 * @param bridge Bridge instance
 * @param region Language region
 * @return Recovery phase
 */
recovery_phase_t language_immune_bridge_get_recovery_phase(
    const language_immune_bridge_t* bridge,
    language_region_t region
);

/**
 * @brief Get recovery progress for region
 *
 * @param bridge Bridge instance
 * @param region Language region
 * @return Recovery progress [0-1]
 */
float language_immune_bridge_get_recovery_progress(
    const language_immune_bridge_t* bridge,
    language_region_t region
);

/**
 * @brief Set recovery rate
 *
 * @param bridge Bridge instance
 * @param rate Recovery rate
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_set_recovery_rate(
    language_immune_bridge_t* bridge,
    float rate
);

//=============================================================================
// Aphasia Modeling API
//=============================================================================

/**
 * @brief Enable/disable aphasia symptom modeling
 *
 * @param bridge Bridge instance
 * @param enabled Enable flag
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_set_aphasia_modeling(
    language_immune_bridge_t* bridge,
    bool enabled
);

/**
 * @brief Check for specific aphasia symptom
 *
 * @param bridge Bridge instance
 * @param symptom Symptom to check (use aphasia_type_t)
 * @return true if symptom present
 */
bool language_immune_bridge_has_symptom(
    const language_immune_bridge_t* bridge,
    aphasia_type_t symptom
);

//=============================================================================
// Update and Query API
//=============================================================================

/**
 * @brief Update bridge (call each frame/cycle)
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current time in milliseconds
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_update(
    language_immune_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Apply immune effects to language processing
 *
 * Called internally to modulate Wernicke/Broca/NLP based on cytokines
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_apply_effects(
    language_immune_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_get_stats(
    const language_immune_bridge_t* bridge,
    language_immune_stats_t* stats
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_bio_async_register(
    language_immune_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_immune_bridge_bio_async_unregister(
    language_immune_bridge_t* bridge
);

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* inflammation_level_to_string(inflammation_level_t level);
const char* aphasia_type_to_string(aphasia_type_t type);
const char* language_region_to_string(language_region_t region);
const char* recovery_phase_to_string(recovery_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_IMMUNE_BRIDGE_H */

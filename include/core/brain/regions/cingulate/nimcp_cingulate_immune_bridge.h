/**
 * @file nimcp_cingulate_immune_bridge.h
 * @brief Cingulate Cortex-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-30
 *
 * WHAT: Bidirectional integration between brain immune system and cingulate cortex
 * WHY:  The cingulate cortex (ACC/PCC) is critical for error monitoring, conflict
 *       detection, and emotion-cognition integration. Neuroinflammation impairs
 *       these functions, contributing to cognitive dysfunction and mood disorders.
 * HOW:  Inflammation modulates error detection sensitivity, conflict thresholds,
 *       and self-referential processing. Cognitive errors may signal neural damage.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE -> CINGULATE PATHWAYS:
 * -----------------------------
 * 1. Inflammation -> Error Detection Impairment (dACC):
 *    - Pro-inflammatory cytokines reduce dACC activation
 *    - Impaired error-related negativity (ERN) amplitude
 *    - Reduced error awareness and correction
 *    - Reference: Slavich & Irwin (2014) "Inflammation and depression"
 *
 * 2. Inflammation -> Conflict Monitoring Dysfunction:
 *    - Elevated cytokines impair response conflict detection
 *    - Reduced N2 amplitude in flanker/Stroop tasks
 *    - Executive control deficits during sickness
 *    - Reference: Lasselin et al. (2020) "Inflammation and cognition"
 *
 * 3. Inflammation -> Emotional Dysregulation (rACC):
 *    - Rostral ACC mediates emotion-cognition integration
 *    - Inflammation → anhedonia, apathy, reduced motivation
 *    - Pain processing amplified (inflammatory hyperalgesia)
 *    - Reference: Miller & Raison (2016) "The role of inflammation"
 *
 * 4. Inflammation -> Default Mode Network Disruption (PCC):
 *    - PCC is hub of Default Mode Network
 *    - Inflammation reduces DMN connectivity
 *    - Impaired self-referential and autobiographical processing
 *    - Reference: Harrison et al. (2009) "Inflammation-induced DMN changes"
 *
 * CINGULATE -> IMMUNE PATHWAYS:
 * -----------------------------
 * 1. Error Detection -> Stress Response:
 *    - Frequent errors → stress-related immune activation
 *    - dACC activation modulates HPA axis
 *    - Chronic error stress → low-grade inflammation
 *    - Reference: Eisenberger (2012) "Neural bases of social rejection"
 *
 * 2. Conflict Levels -> Immune Modulation:
 *    - High cognitive conflict → cortisol/catecholamine release
 *    - Stress hormones modulate immune function
 *    - Chronic conflict → allostatic load
 *    - Reference: McEwen (2007) "Physiology and neurobiology of stress"
 *
 * 3. Emotional Processing -> Cytokine Production:
 *    - Negative emotional states → pro-inflammatory cytokines
 *    - Social rejection → inflammation (social pain)
 *    - Positive emotions → anti-inflammatory effects
 *    - Reference: Kiecolt-Glaser et al. (2010) "Close relationships and inflammation"
 *
 * ARCHITECTURE:
 * ```
 * +---------------------------------------------------------------------------+
 * |                    CINGULATE-IMMUNE BRIDGE                                |
 * +---------------------------------------------------------------------------+
 * |                                                                           |
 * |   +-------------------------------------------------------------------+   |
 * |   |                IMMUNE -> CINGULATE PATHWAYS                       |   |
 * |   |                                                                   |   |
 * |   |   +------------------+    +------------------+                    |   |
 * |   |   | CYTOKINES        |    | INFLAMMATION     |                    |   |
 * |   |   | IL-1B: -0.30 ERN |    | NONE:   Normal   |                    |   |
 * |   |   | IL-6:  -0.25 N2  |    | LOCAL:  -10%     |                    |   |
 * |   |   | TNF-a: -0.40 cog |    | REGION: -25%     |                    |   |
 * |   |   | IL-10: +0.20 rec |    | SYSTEM: -45%     |                    |   |
 * |   |   +------------------+    | STORM:  -75%     |                    |   |
 * |   |                           +------------------+                    |   |
 * |   |                                                                   |   |
 * |   |   EFFECTS:                                                        |   |
 * |   |   - Error detection sensitivity (ERN amplitude)                   |   |
 * |   |   - Conflict monitoring threshold (N2 amplitude)                  |   |
 * |   |   - Cognitive control capacity                                    |   |
 * |   |   - Emotional regulation                                          |   |
 * |   |   - Self-referential processing                                   |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                                                           |
 * |   +-------------------------------------------------------------------+   |
 * |   |                CINGULATE -> IMMUNE PATHWAYS                       |   |
 * |   |                                                                   |   |
 * |   |   +------------------+    +------------------+                    |   |
 * |   |   | ERROR RATE       |    | CONFLICT LEVEL   |                    |   |
 * |   |   | Low:    Baseline |    | Low:    Baseline |                    |   |
 * |   |   | Normal: Baseline |    | Normal: Baseline |                    |   |
 * |   |   | High:   +15%     |    | High:   +20%     |                    |   |
 * |   |   | Chronic: +30%    |    | Chronic: +35%    |                    |   |
 * |   |   +------------------+    +------------------+                    |   |
 * |   |                                                                   |   |
 * |   |   +------------------+                                            |   |
 * |   |   | EMOTIONAL STATE  |                                            |   |
 * |   |   | Negative: +25%   | ---> Pro-inflammatory                      |   |
 * |   |   | Neutral:  0%     |                                            |   |
 * |   |   | Positive: -15%   | ---> Anti-inflammatory                     |   |
 * |   |   +------------------+                                            |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                                                           |
 * +---------------------------------------------------------------------------+
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

#ifndef NIMCP_CINGULATE_IMMUNE_BRIDGE_H
#define NIMCP_CINGULATE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base infrastructure */
#include "utils/bridge/nimcp_bridge_base.h"

/* Cingulate adapter */
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"

/* Immune system integration */
#include "cognitive/immune/nimcp_brain_immune.h"

/* Bio-async */
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

#define CINGULATE_IMMUNE_MODULE_NAME        "cingulate_immune_bridge"
#define CINGULATE_IMMUNE_UPDATE_INTERVAL_MS 100

/* Cytokine effects on cingulate function */
#define CYTOKINE_IL1_ERN_IMPACT             -0.30f  /**< IL-1B reduces ERN amplitude */
#define CYTOKINE_IL6_N2_IMPACT              -0.25f  /**< IL-6 reduces N2 amplitude */
#define CYTOKINE_TNF_COGNITIVE_IMPACT       -0.40f  /**< TNF-a impairs cognitive control */
#define CYTOKINE_IFN_GAMMA_FOCUS_IMPACT     -0.20f  /**< IFN-g reduces focus */
#define CYTOKINE_IL10_RECOVERY_IMPACT       +0.20f  /**< IL-10 aids recovery */

/* Inflammation -> cingulate function factors */
#define INFLAMMATION_NONE_CINGULATE_FACTOR     1.00f
#define INFLAMMATION_LOCAL_CINGULATE_FACTOR    0.90f
#define INFLAMMATION_REGIONAL_CINGULATE_FACTOR 0.75f
#define INFLAMMATION_SYSTEMIC_CINGULATE_FACTOR 0.55f
#define INFLAMMATION_STORM_CINGULATE_FACTOR    0.25f

/* Error rate -> immune modulation factors */
#define ERROR_RATE_LOW_THRESHOLD            0.10f  /**< Below: normal immune */
#define ERROR_RATE_HIGH_THRESHOLD           0.30f  /**< Above: stress immune */
#define ERROR_RATE_CHRONIC_THRESHOLD        0.50f  /**< Above: chronic immune */
#define ERROR_RATE_IMMUNE_MODULATION        0.15f  /**< Immune boost per 10% errors */

/* Conflict level -> immune modulation factors */
#define CONFLICT_LOW_THRESHOLD              0.20f  /**< Below: normal */
#define CONFLICT_HIGH_THRESHOLD             0.60f  /**< Above: stress response */
#define CONFLICT_CHRONIC_THRESHOLD          0.80f  /**< Above: chronic stress */
#define CONFLICT_IMMUNE_MODULATION          0.20f  /**< Immune boost per conflict level */

/* Emotional valence -> immune factors */
#define EMOTION_NEGATIVE_IMMUNE_FACTOR      1.25f  /**< Negative -> pro-inflammatory */
#define EMOTION_NEUTRAL_IMMUNE_FACTOR       1.00f
#define EMOTION_POSITIVE_IMMUNE_FACTOR      0.85f  /**< Positive -> anti-inflammatory */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Cingulate immune integration state
 */
typedef enum {
    CINGULATE_IMMUNE_NORMAL = 0,        /**< Normal cognitive function */
    CINGULATE_IMMUNE_MILD_IMPAIRMENT,   /**< Mild cognitive fog */
    CINGULATE_IMMUNE_MODERATE_IMPAIRMENT, /**< Moderate dysfunction */
    CINGULATE_IMMUNE_SEVERE_IMPAIRMENT, /**< Severe cognitive deficit */
    CINGULATE_IMMUNE_STRESSED,          /**< High conflict/error stress */
    CINGULATE_IMMUNE_RECOVERING         /**< Recovery phase */
} cingulate_immune_state_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Cytokine effects on cingulate function
 */
typedef struct {
    /* Error detection modulation */
    float ern_modulation;               /**< ERN amplitude factor [0-1] */
    float error_sensitivity;            /**< Error detection sensitivity [0-1] */

    /* Conflict monitoring modulation */
    float n2_modulation;                /**< N2 amplitude factor [0-1] */
    float conflict_threshold;           /**< Conflict detection threshold */

    /* Cognitive control modulation */
    float control_capacity;             /**< Cognitive control capacity [0-1] */
    float attention_boost_capacity;     /**< Attention boost available [0-1] */

    /* Emotional processing modulation */
    float emotional_regulation;         /**< Emotional regulation capacity [0-1] */
    float pain_sensitivity;             /**< Pain processing sensitivity */

    /* Self-referential modulation */
    float self_referential_capacity;    /**< Self-referential processing [0-1] */
    float dmn_connectivity;             /**< DMN connectivity factor [0-1] */

    /* Individual cytokine contributions */
    float il1_contribution;
    float il6_contribution;
    float tnf_contribution;
    float il10_contribution;

    /* Inflammation state */
    brain_inflammation_level_t inflammation_level;
    float combined_modulation;          /**< Overall function modulation */
} cingulate_cytokine_effects_t;

/**
 * @brief Cingulate effects on immune function
 */
typedef struct {
    /* Error-based modulation */
    float error_rate;                   /**< Current error rate [0-1] */
    float error_immune_factor;          /**< Immune modulation from errors */
    bool chronic_errors;                /**< Chronic error pattern detected */

    /* Conflict-based modulation */
    float conflict_level;               /**< Current conflict level [0-1] */
    float conflict_immune_factor;       /**< Immune modulation from conflict */
    bool chronic_conflict;              /**< Chronic conflict stress */

    /* Emotion-based modulation */
    float emotional_valence;            /**< Emotional valence [-1, +1] */
    float emotional_arousal;            /**< Emotional arousal [0-1] */
    float emotion_immune_factor;        /**< Immune modulation from emotion */

    /* Combined factor */
    float combined_immune_factor;       /**< Total immune modulation */

    /* Stress indicators */
    bool stress_response_active;        /**< HPA axis activation */
    float cortisol_analog;              /**< Stress hormone analog [0-1] */
} cingulate_immune_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Pathway enables */
    bool enable_immune_to_cingulate;    /**< Inflammation affects cingulate */
    bool enable_cingulate_to_immune;    /**< Cognitive state affects immunity */
    bool enable_error_tracking;         /**< Track error patterns */
    bool enable_conflict_tracking;      /**< Track conflict levels */
    bool enable_emotional_modulation;   /**< Emotion affects immunity */

    /* Sensitivity tuning */
    float cytokine_sensitivity;         /**< Cytokine effect multiplier [0.5-2.0] */
    float error_coupling;               /**< Error-immune coupling [0.5-2.0] */
    float conflict_coupling;            /**< Conflict-immune coupling [0.5-2.0] */
    float emotion_coupling;             /**< Emotion-immune coupling [0.5-2.0] */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Update interval */
    uint32_t update_interval_ms;

    /* Thresholds */
    float chronic_error_threshold;      /**< Error rate for chronic detection */
    float chronic_conflict_threshold;   /**< Conflict level for chronic detection */
} cingulate_immune_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t total_updates;
    uint64_t immune_to_cingulate_count;
    uint64_t cingulate_to_immune_count;

    /* Impairment episodes */
    uint64_t impairment_episodes;
    uint64_t stress_episodes;
    uint64_t recovery_episodes;

    /* Error/conflict tracking */
    uint64_t high_error_episodes;
    uint64_t high_conflict_episodes;
    uint64_t chronic_stress_detections;

    /* Averages */
    float avg_ern_modulation;
    float avg_conflict_modulation;
    float avg_immune_factor;
} cingulate_immune_stats_t;

/**
 * @brief Cingulate-Immune bridge handle
 */
typedef struct cingulate_immune_bridge* cingulate_immune_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Provides balanced immune-cingulate integration
 * HOW:  Sets biologically-plausible coupling strengths
 *
 * @param config Output configuration structure
 */
void cingulate_immune_default_config(cingulate_immune_config_t* config);

/**
 * @brief Create cingulate-immune bridge
 *
 * WHAT: Creates bidirectional integration between cingulate and immune system
 * WHY:  Enables realistic cognitive-immune feedback loops
 * HOW:  Allocates bridge, connects systems, initializes state
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param cingulate Cingulate adapter instance
 * @param immune Brain immune system instance
 * @return Bridge handle on success, NULL on failure
 */
cingulate_immune_bridge_t cingulate_immune_create(
    const cingulate_immune_config_t* config,
    cingulate_adapter_t* cingulate,
    brain_immune_system_t* immune
);

/**
 * @brief Destroy cingulate-immune bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY:  Prevents memory leaks
 * HOW:  Disconnects systems, frees memory
 *
 * @param bridge Bridge to destroy
 */
void cingulate_immune_destroy(cingulate_immune_bridge_t bridge);

/*=============================================================================
 * Update Functions
 *===========================================================================*/

/**
 * @brief Update bridge (both directions)
 *
 * WHAT: Performs bidirectional immune-cingulate modulation
 * WHY:  Maintains continuous cognitive-immune coordination
 * HOW:  Reads cytokines -> modulates cingulate; reads errors/conflict -> modulates immune
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_update(cingulate_immune_bridge_t bridge);

/**
 * @brief Update immune -> cingulate pathway only
 *
 * WHAT: Computes and applies immune effects on cingulate
 * WHY:  Inflammation affects error detection and conflict monitoring
 * HOW:  Reads cytokine levels, computes cognitive modulations
 *
 * @param bridge Bridge to update
 * @param effects Output effects structure (optional)
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_update_immune_to_cingulate(
    cingulate_immune_bridge_t bridge,
    cingulate_cytokine_effects_t* effects
);

/**
 * @brief Update cingulate -> immune pathway only
 *
 * WHAT: Computes and applies cingulate effects on immune system
 * WHY:  Errors, conflict, and emotions affect immune function
 * HOW:  Reads cingulate state, computes immune modulation factors
 *
 * @param bridge Bridge to update
 * @param effects Output effects structure (optional)
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_update_cingulate_to_immune(
    cingulate_immune_bridge_t bridge,
    cingulate_immune_effects_t* effects
);

/*=============================================================================
 * Query Functions
 *===========================================================================*/

/**
 * @brief Get current cytokine effects on cingulate
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_get_cytokine_effects(
    cingulate_immune_bridge_t bridge,
    cingulate_cytokine_effects_t* effects
);

/**
 * @brief Get current cingulate effects on immune system
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_get_immune_effects(
    cingulate_immune_bridge_t bridge,
    cingulate_immune_effects_t* effects
);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge to query
 * @return Current state
 */
cingulate_immune_state_t cingulate_immune_get_state(
    cingulate_immune_bridge_t bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_get_stats(
    cingulate_immune_bridge_t bridge,
    cingulate_immune_stats_t* stats
);

/*=============================================================================
 * Error and Conflict Reporting
 *===========================================================================*/

/**
 * @brief Report error for immune tracking
 *
 * WHAT: Reports a cognitive error for immune modulation
 * WHY:  High error rates indicate stress and potential damage
 * HOW:  Updates error tracking, computes immune effect
 *
 * @param bridge Bridge instance
 * @param error_severity Severity of error [0-1]
 * @return 0 on success, negative on failure
 */
int cingulate_immune_report_error(
    cingulate_immune_bridge_t bridge,
    float error_severity
);

/**
 * @brief Report conflict for immune tracking
 *
 * WHAT: Reports a conflict event for immune modulation
 * WHY:  Chronic conflict leads to stress-induced immune changes
 * HOW:  Updates conflict tracking, computes immune effect
 *
 * @param bridge Bridge instance
 * @param conflict_level Conflict intensity [0-1]
 * @return 0 on success, negative on failure
 */
int cingulate_immune_report_conflict(
    cingulate_immune_bridge_t bridge,
    float conflict_level
);

/**
 * @brief Report emotional state for immune tracking
 *
 * WHAT: Reports emotional valence/arousal for immune modulation
 * WHY:  Negative emotions promote inflammation, positive reduce it
 * HOW:  Updates emotional tracking, computes immune effect
 *
 * @param bridge Bridge instance
 * @param valence Emotional valence [-1, +1]
 * @param arousal Emotional arousal [0-1]
 * @return 0 on success, negative on failure
 */
int cingulate_immune_report_emotion(
    cingulate_immune_bridge_t bridge,
    float valence,
    float arousal
);

/*=============================================================================
 * Bio-Async Connection
 *===========================================================================*/

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Enables bio-async messaging for immune-cingulate coordination
 * WHY:  Allows asynchronous event-driven updates
 * HOW:  Registers with bio-router for cytokine and cognitive messages
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_connect_bio_async(cingulate_immune_bridge_t bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative error code on failure
 */
int cingulate_immune_disconnect_bio_async(cingulate_immune_bridge_t bridge);

/**
 * @brief Check if bridge is connected to bio-async
 *
 * @param bridge Bridge to check
 * @return true if connected, false otherwise
 */
bool cingulate_immune_is_bio_async_connected(cingulate_immune_bridge_t bridge);

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Compute cingulate function factor from inflammation level
 *
 * @param level Inflammation level
 * @return Function multiplier [0.25, 1.0]
 */
float cingulate_immune_compute_inflammation_factor(brain_inflammation_level_t level);

/**
 * @brief Compute immune factor from error rate
 *
 * @param error_rate Error rate [0-1]
 * @return Immune multiplier [1.0, 1.3]
 */
float cingulate_immune_compute_error_immune(float error_rate);

/**
 * @brief Compute immune factor from conflict level
 *
 * @param conflict_level Conflict level [0-1]
 * @return Immune multiplier [1.0, 1.35]
 */
float cingulate_immune_compute_conflict_immune(float conflict_level);

/**
 * @brief Compute immune factor from emotional valence
 *
 * @param valence Emotional valence [-1, +1]
 * @return Immune multiplier [0.85, 1.25]
 */
float cingulate_immune_compute_emotion_immune(float valence);

/**
 * @brief Convert bridge state to string
 *
 * @param state State to convert
 * @return Human-readable string
 */
const char* cingulate_immune_state_to_string(cingulate_immune_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CINGULATE_IMMUNE_BRIDGE_H */

/**
 * @file nimcp_brainstem_immune_bridge.h
 * @brief Brainstem-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-30
 *
 * WHAT: Bidirectional integration between brain immune system and brainstem
 * WHY:  The brainstem controls vital autonomic functions that directly interact with
 *       immune response. Inflammation affects arousal systems, reflexes, and vital
 *       function stability. Creates critical feedback loop for survival.
 * HOW:  Inflammation modulates arousal via reticular formation; cytokines affect
 *       protective reflexes; brainstem arousal state modulates immune surveillance.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE -> BRAINSTEM PATHWAYS:
 * -----------------------------
 * 1. Inflammation -> Arousal Modulation (Reticular Formation):
 *    - Pro-inflammatory cytokines (IL-1B, IL-6, TNF-a) cross BBB
 *    - Depress reticular activating system -> reduced arousal
 *    - Sickness behavior: lethargy, drowsiness, reduced vigilance
 *    - Reference: Dantzer (2009) "Cytokine, sickness behavior, and depression"
 *
 * 2. Inflammation -> Reflex Modulation:
 *    - Startle reflex potentiated during inflammation
 *    - Protective reflexes enhanced during immune activation
 *    - Fever-induced reflex changes
 *    - Reference: Miller et al. (2014) "Inflammation and the brain"
 *
 * 3. Inflammation -> Vital Function:
 *    - Cytokines affect cardiovascular regulation in medulla
 *    - Respiratory patterns modulated during sickness
 *    - Fever affects thermoregulation centers
 *    - Reference: Tracey (2002) "The inflammatory reflex"
 *
 * BRAINSTEM -> IMMUNE PATHWAYS:
 * -----------------------------
 * 1. Arousal Level -> Immune Surveillance:
 *    - High arousal (alert state) -> enhanced immune vigilance
 *    - Low arousal (sleep) -> immune repair/consolidation mode
 *    - Fight-or-flight response modulates immune cell trafficking
 *    - Reference: Elenkov et al. (2000) "Sympathetic nerve-immune interface"
 *
 * 2. Protection Level -> Immune Response:
 *    - Elevated protection -> enhanced immune activity
 *    - Critical protection -> emergency immune response
 *    - Cholinergic anti-inflammatory pathway activation
 *    - Reference: Pavlov & Tracey (2005) "Cholinergic anti-inflammatory pathway"
 *
 * 3. Reflex Activation -> Immune Signaling:
 *    - Startle/defensive reflex -> sympathetic activation -> immune modulation
 *    - Vagal reflexes -> cholinergic anti-inflammatory pathway
 *    - Reference: Tracey (2007) "Physiology of cholinergic anti-inflammatory pathway"
 *
 * ARCHITECTURE:
 * ```
 * +---------------------------------------------------------------------------+
 * |                     BRAINSTEM-IMMUNE BRIDGE                               |
 * +---------------------------------------------------------------------------+
 * |                                                                           |
 * |   +-------------------------------------------------------------------+   |
 * |   |                 IMMUNE -> BRAINSTEM PATHWAYS                      |   |
 * |   |                                                                   |   |
 * |   |   +-------------------+                                           |   |
 * |   |   |    CYTOKINES      |                                           |   |
 * |   |   | IL-1B -> -0.35    |                                           |   |
 * |   |   | IL-6  -> -0.25    | ---> Arousal Reduction (Sickness)         |   |
 * |   |   | TNF-a -> -0.45    |      Reflex Potentiation                  |   |
 * |   |   | IL-10 -> +0.25    |      Vital Function Modulation            |   |
 * |   |   +-------------------+                                           |   |
 * |   |                                                                   |   |
 * |   |   +-------------------+                                           |   |
 * |   |   | INFLAMMATION      |                                           |   |
 * |   |   | NONE   -> Normal  |                                           |   |
 * |   |   | LOCAL  -> -15%    | ---> Brainstem Function Modulation        |   |
 * |   |   | REGION -> -30%    |                                           |   |
 * |   |   | SYSTEM -> -50%    |                                           |   |
 * |   |   | STORM  -> -80%    |                                           |   |
 * |   |   +-------------------+                                           |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                                                           |
 * |   +-------------------------------------------------------------------+   |
 * |   |                 BRAINSTEM -> IMMUNE PATHWAYS                      |   |
 * |   |                                                                   |   |
 * |   |   +-------------------+                                           |   |
 * |   |   | AROUSAL LEVEL     |                                           |   |
 * |   |   | Low (0-0.3)       | ---> Immune depression                    |   |
 * |   |   | Normal (0.3-0.7)  | ---> Optimal immunity                     |   |
 * |   |   | High (0.7-1.0)    | ---> Enhanced surveillance                |   |
 * |   |   | Hyperaroused      | ---> Stress-induced modulation            |   |
 * |   |   +-------------------+                                           |   |
 * |   |                                                                   |   |
 * |   |   +-------------------+                                           |   |
 * |   |   | PROTECTION LEVEL  |                                           |   |
 * |   |   | Normal            | ---> Baseline immunity                    |   |
 * |   |   | Alert/Protective  | ---> +25% immune activity                 |   |
 * |   |   | Critical          | ---> Emergency immune response            |   |
 * |   |   +-------------------+                                           |   |
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

#ifndef NIMCP_BRAINSTEM_IMMUNE_BRIDGE_H
#define NIMCP_BRAINSTEM_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base infrastructure */
#include "utils/bridge/nimcp_bridge_base.h"

/* Brainstem adapter */
#include "core/brain/regions/brainstem/nimcp_brainstem_adapter.h"

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

#define BRAINSTEM_IMMUNE_MODULE_NAME        "brainstem_immune_bridge"
#define BRAINSTEM_IMMUNE_UPDATE_INTERVAL_MS 100

/* Cytokine arousal impact factors */
#define CYTOKINE_IL1_BRAINSTEM_IMPACT       -0.35f  /**< IL-1B depresses arousal */
#define CYTOKINE_IL6_BRAINSTEM_IMPACT       -0.25f  /**< IL-6 depresses arousal */
#define CYTOKINE_TNF_BRAINSTEM_IMPACT       -0.45f  /**< TNF-a strongly depresses */
#define CYTOKINE_IFN_GAMMA_BRAINSTEM_IMPACT -0.20f  /**< IFN-g mild depression */
#define CYTOKINE_IL10_BRAINSTEM_IMPACT      +0.25f  /**< IL-10 recovery boost */

/* Inflammation arousal factors */
#define INFLAMMATION_NONE_BRAINSTEM_FACTOR     1.00f
#define INFLAMMATION_LOCAL_BRAINSTEM_FACTOR    0.85f
#define INFLAMMATION_REGIONAL_BRAINSTEM_FACTOR 0.70f
#define INFLAMMATION_SYSTEMIC_BRAINSTEM_FACTOR 0.50f
#define INFLAMMATION_STORM_BRAINSTEM_FACTOR    0.20f

/* Arousal-immune thresholds */
#define BRAINSTEM_AROUSAL_LOW_THRESHOLD     0.30f  /**< Below: immune depression */
#define BRAINSTEM_AROUSAL_HIGH_THRESHOLD    0.70f  /**< Above: enhanced surveillance */
#define BRAINSTEM_AROUSAL_HYPER_THRESHOLD   0.90f  /**< Hyperarousal stress response */

/* Protection-immune factors */
#define BRAINSTEM_PROTECTION_NORMAL_FACTOR  1.00f
#define BRAINSTEM_PROTECTION_ALERT_FACTOR   1.25f
#define BRAINSTEM_PROTECTION_HIGH_FACTOR    1.50f
#define BRAINSTEM_PROTECTION_CRITICAL_FACTOR 2.00f
#define BRAINSTEM_PROTECTION_SHUTDOWN_FACTOR 0.60f  /**< Reduce to prevent storm */

/* Reflex potentiation factor during inflammation */
#define INFLAMMATION_REFLEX_POTENTIATION    0.20f  /**< +20% reflex speed per level */

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Brainstem immune integration state
 */
typedef enum {
    BRAINSTEM_IMMUNE_NORMAL = 0,       /**< Normal operation */
    BRAINSTEM_IMMUNE_MILD_DEPRESSION,  /**< Mild sickness behavior */
    BRAINSTEM_IMMUNE_MODERATE_SICKNESS,/**< Moderate sickness */
    BRAINSTEM_IMMUNE_SEVERE_SICKNESS,  /**< Severe sickness behavior */
    BRAINSTEM_IMMUNE_PROTECTIVE,       /**< Protective mode active */
    BRAINSTEM_IMMUNE_RECOVERING        /**< Recovery phase */
} brainstem_immune_state_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Cytokine effects on brainstem function
 */
typedef struct {
    /* Arousal modulation */
    float arousal_modulation;          /**< Net arousal change [-1, +1] */
    float il1_contribution;            /**< IL-1B contribution */
    float il6_contribution;            /**< IL-6 contribution */
    float tnf_contribution;            /**< TNF-a contribution */
    float il10_contribution;           /**< IL-10 recovery contribution */

    /* Reflex modulation */
    float reflex_potentiation;         /**< Reflex speed boost [0-1] */
    float protective_reflex_gain;      /**< Enhanced protective response */

    /* Vital function modulation */
    float vital_stability_factor;      /**< Vital function stability [0-1] */
    float autonomic_balance_shift;     /**< Sympathetic/parasympathetic shift */

    /* Inflammation state */
    brain_inflammation_level_t inflammation_level;
    float inflammation_arousal_factor; /**< Computed from level */
} brainstem_cytokine_effects_t;

/**
 * @brief Brainstem effects on immune function
 */
typedef struct {
    /* Arousal-based modulation */
    float arousal_immune_factor;       /**< Immune modulation from arousal [0.5-1.5] */
    brainstem_arousal_level_t arousal_level;
    float arousal_value;               /**< Raw arousal [0-1] */

    /* Protection-based modulation */
    float protection_immune_factor;    /**< Immune modulation from protection [0.5-2] */
    brainstem_status_t status;

    /* Combined factor */
    float combined_immune_factor;      /**< Total immune modulation */

    /* Immune signaling */
    bool enhance_surveillance;         /**< High arousal -> enhanced surveillance */
    bool trigger_stress_response;      /**< Hyperarousal -> stress immune response */
    bool cholinergic_suppression;      /**< Vagal activation -> anti-inflammatory */
} brainstem_immune_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Pathway enables */
    bool enable_immune_to_brainstem;   /**< Inflammation affects brainstem */
    bool enable_brainstem_to_immune;   /**< Arousal affects immunity */
    bool enable_reflex_modulation;     /**< Inflammation affects reflexes */
    bool enable_vital_monitoring;      /**< Vital function integration */

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float arousal_coupling;            /**< Arousal-immune coupling [0.5-2.0] */
    float protection_coupling;         /**< Protection-immune coupling [0.5-2.0] */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */

    /* Update interval */
    uint32_t update_interval_ms;

    /* Emergency response */
    bool emergency_on_storm;           /**< Trigger emergency on cytokine storm */
} brainstem_immune_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t total_updates;
    uint64_t immune_to_brainstem_count;
    uint64_t brainstem_to_immune_count;

    /* State transitions */
    uint64_t sickness_episodes;
    uint64_t protective_activations;
    uint64_t recovery_episodes;

    /* Emergency events */
    uint64_t emergencies_triggered;
    uint64_t storms_detected;

    /* Averages */
    float avg_arousal_modulation;
    float avg_immune_factor;
    float avg_reflex_potentiation;
} brainstem_immune_stats_t;

/**
 * @brief Brainstem-Immune bridge handle
 */
typedef struct brainstem_immune_bridge* brainstem_immune_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Provides balanced immune-brainstem integration
 * HOW:  Sets biologically-plausible coupling strengths
 *
 * @param config Output configuration structure
 */
void brainstem_immune_default_config(brainstem_immune_config_t* config);

/**
 * @brief Create brainstem-immune bridge
 *
 * WHAT: Creates bidirectional integration between brainstem and immune system
 * WHY:  Enables realistic autonomic-immune feedback loops
 * HOW:  Allocates bridge, connects systems, initializes state
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param brainstem Brainstem adapter instance
 * @param immune Brain immune system instance
 * @return Bridge handle on success, NULL on failure
 */
brainstem_immune_bridge_t brainstem_immune_create(
    const brainstem_immune_config_t* config,
    brainstem_adapter_t* brainstem,
    brain_immune_system_t* immune
);

/**
 * @brief Destroy brainstem-immune bridge
 *
 * WHAT: Cleans up bridge resources
 * WHY:  Prevents memory leaks
 * HOW:  Disconnects systems, frees memory
 *
 * @param bridge Bridge to destroy
 */
void brainstem_immune_destroy(brainstem_immune_bridge_t bridge);

/*=============================================================================
 * Update Functions
 *===========================================================================*/

/**
 * @brief Update bridge (both directions)
 *
 * WHAT: Performs bidirectional immune-brainstem modulation
 * WHY:  Maintains continuous autonomic-immune coordination
 * HOW:  Reads cytokines -> modulates brainstem; reads arousal -> modulates immune
 *
 * @param bridge Bridge to update
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_update(brainstem_immune_bridge_t bridge);

/**
 * @brief Update immune -> brainstem pathway only
 *
 * WHAT: Computes and applies immune effects on brainstem
 * WHY:  Inflammation affects arousal and reflexes
 * HOW:  Reads cytokine levels, computes arousal/reflex adjustments
 *
 * @param bridge Bridge to update
 * @param effects Output effects structure (optional)
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_update_immune_to_brainstem(
    brainstem_immune_bridge_t bridge,
    brainstem_cytokine_effects_t* effects
);

/**
 * @brief Update brainstem -> immune pathway only
 *
 * WHAT: Computes and applies brainstem effects on immune system
 * WHY:  Arousal and protection affect immune function
 * HOW:  Reads brainstem state, computes immune modulation factors
 *
 * @param bridge Bridge to update
 * @param effects Output effects structure (optional)
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_update_brainstem_to_immune(
    brainstem_immune_bridge_t bridge,
    brainstem_immune_effects_t* effects
);

/*=============================================================================
 * Query Functions
 *===========================================================================*/

/**
 * @brief Get current cytokine effects on brainstem
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_get_cytokine_effects(
    brainstem_immune_bridge_t bridge,
    brainstem_cytokine_effects_t* effects
);

/**
 * @brief Get current brainstem effects on immune system
 *
 * @param bridge Bridge to query
 * @param effects Output effects structure
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_get_immune_effects(
    brainstem_immune_bridge_t bridge,
    brainstem_immune_effects_t* effects
);

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge to query
 * @return Current state
 */
brainstem_immune_state_t brainstem_immune_get_state(
    brainstem_immune_bridge_t bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_get_stats(
    brainstem_immune_bridge_t bridge,
    brainstem_immune_stats_t* stats
);

/*=============================================================================
 * Bio-Async Connection
 *===========================================================================*/

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Enables bio-async messaging for immune-brainstem coordination
 * WHY:  Allows asynchronous event-driven updates
 * HOW:  Registers with bio-router for cytokine and arousal messages
 *
 * @param bridge Bridge to connect
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_connect_bio_async(brainstem_immune_bridge_t bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success, negative error code on failure
 */
int brainstem_immune_disconnect_bio_async(brainstem_immune_bridge_t bridge);

/**
 * @brief Check if bridge is connected to bio-async
 *
 * @param bridge Bridge to check
 * @return true if connected, false otherwise
 */
bool brainstem_immune_is_bio_async_connected(brainstem_immune_bridge_t bridge);

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Compute arousal factor from inflammation level
 *
 * @param level Inflammation level
 * @return Arousal multiplier [0.2, 1.0]
 */
float brainstem_immune_compute_inflammation_arousal(brain_inflammation_level_t level);

/**
 * @brief Compute immune factor from arousal level
 *
 * @param arousal Arousal value [0-1]
 * @return Immune multiplier [0.5, 1.5]
 */
float brainstem_immune_compute_arousal_immune(float arousal);

/**
 * @brief Compute reflex potentiation from inflammation
 *
 * @param level Inflammation level
 * @return Reflex potentiation factor [0, 0.8]
 */
float brainstem_immune_compute_reflex_potentiation(brain_inflammation_level_t level);

/**
 * @brief Convert bridge state to string
 *
 * @param state State to convert
 * @return Human-readable string
 */
const char* brainstem_immune_state_to_string(brainstem_immune_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAINSTEM_IMMUNE_BRIDGE_H */

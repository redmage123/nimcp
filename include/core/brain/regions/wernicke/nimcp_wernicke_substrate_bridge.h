/**
 * @file nimcp_wernicke_substrate_bridge.h
 * @brief Bridge between Wernicke's area and neural substrate
 *
 * WHAT: Links language comprehension to metabolic state
 * WHY:  Speech comprehension requires sustained temporal lobe activation
 * HOW:  Monitors ATP/fatigue; modulates recognition speed, disambiguation, working memory
 *
 * BIOLOGICAL BASIS:
 * - Wernicke's area (STG/BA22) has high metabolic demands for speech processing
 * - ATP depletion causes word recognition difficulty
 * - Fatigue reduces phonological working memory capacity
 * - Metabolic stress impairs disambiguation and semantic integration
 * - Sustained attention to speech requires glucose availability
 *
 * APHASIA-LIKE EFFECTS:
 * - Low ATP: Slowed word recognition, increased latency
 * - High fatigue: Reduced working memory span, poor disambiguation
 * - Metabolic stress: Comprehension errors, semantic confusion
 *
 * @version Phase W3: Wernicke's Area Bridges
 * @date 2026-01-04
 */

#ifndef NIMCP_WERNICKE_SUBSTRATE_BRIDGE_H
#define NIMCP_WERNICKE_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Bio-async module ID for this bridge
 */
#define BIO_MODULE_SUBSTRATE_WERNICKE 0x1275

/**
 * @brief Metabolic effects on Wernicke's area
 */
typedef struct {
    float phoneme_recognition;       /**< Phoneme recognition speed [0-1] */
    float word_recognition;          /**< Word recognition accuracy [0-1] */
    float semantic_access;           /**< Semantic memory access speed [0-1] */
    float disambiguation_capacity;   /**< Ability to disambiguate [0-1] */
    float working_memory_span;       /**< Phonological WM capacity [0-1] */
    float context_integration;       /**< Context integration ability [0-1] */
    float overall_comprehension;     /**< Combined comprehension [0-1] */
} wernicke_substrate_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_atp_modulation;      /**< ATP affects recognition/access */
    bool enable_fatigue_modulation;  /**< Fatigue affects WM/disambiguation */
    bool enable_bio_async;           /**< Enable bio-async messaging */

    float atp_sensitivity;           /**< How much ATP affects output [0-2] */
    float fatigue_sensitivity;       /**< How much fatigue affects output [0-2] */
    float min_capacity;              /**< Minimum capacity floor [0-1] */

    /* Specific weights */
    float phoneme_atp_weight;        /**< ATP weight for phoneme recognition */
    float semantic_atp_weight;       /**< ATP weight for semantic access */
    float wm_fatigue_weight;         /**< Fatigue weight for working memory */
    float disambig_fatigue_weight;   /**< Fatigue weight for disambiguation */
} wernicke_substrate_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates_processed;      /**< Total substrate updates */
    uint64_t low_atp_events;         /**< Times ATP dropped below threshold */
    uint64_t high_fatigue_events;    /**< Times fatigue exceeded threshold */
    float avg_comprehension;         /**< Average comprehension level */
    float avg_wm_span;               /**< Average working memory span */
    float min_observed_capacity;     /**< Minimum capacity observed */
} wernicke_substrate_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct wernicke_substrate_bridge wernicke_substrate_bridge_t;

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * @return Default config with sensible values
 */
wernicke_substrate_config_t wernicke_substrate_default_config(void);

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Create Wernicke substrate bridge
 *
 * @param wernicke Wernicke adapter handle (void* for flexibility)
 * @param substrate Neural substrate handle (void* for flexibility)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wernicke_substrate_bridge_t* wernicke_substrate_bridge_create(
    void* wernicke,
    void* substrate,
    const wernicke_substrate_config_t* config
);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void wernicke_substrate_bridge_destroy(wernicke_substrate_bridge_t* bridge);

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

/**
 * @brief Update effects from current substrate state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * Reads ATP/fatigue from substrate, computes modulation effects
 */
int wernicke_substrate_bridge_update(wernicke_substrate_bridge_t* bridge);

/**
 * @brief Get current metabolic effects
 *
 * @param bridge Bridge handle
 * @param effects Output: current effects
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_get_effects(
    const wernicke_substrate_bridge_t* bridge,
    wernicke_substrate_effects_t* effects
);

/**
 * @brief Apply effects to Wernicke adapter
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * Modulates Wernicke's processing parameters based on metabolic state
 */
int wernicke_substrate_bridge_apply(wernicke_substrate_bridge_t* bridge);

/*=============================================================================
 * MANUAL OVERRIDE API
 *===========================================================================*/

/**
 * @brief Set ATP level manually (for testing)
 *
 * @param bridge Bridge handle
 * @param atp ATP level [0-1]
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_set_atp(
    wernicke_substrate_bridge_t* bridge,
    float atp
);

/**
 * @brief Set fatigue level manually (for testing)
 *
 * @param bridge Bridge handle
 * @param fatigue Fatigue level [0-1]
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_set_fatigue(
    wernicke_substrate_bridge_t* bridge,
    float fatigue
);

/**
 * @brief Set temperature manually (for testing)
 *
 * @param bridge Bridge handle
 * @param temperature Temperature (Celsius)
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_set_temperature(
    wernicke_substrate_bridge_t* bridge,
    float temperature
);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @param router Bio-async router (void* for flexibility)
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_connect_bio_async(
    wernicke_substrate_bridge_t* bridge,
    void* router
);

/**
 * @brief Handle incoming bio-async message
 *
 * @param bridge Bridge handle
 * @param message Bio message (void* for flexibility)
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_handle_message(
    wernicke_substrate_bridge_t* bridge,
    void* message
);

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_get_stats(
    const wernicke_substrate_bridge_t* bridge,
    wernicke_substrate_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void wernicke_substrate_bridge_reset_stats(wernicke_substrate_bridge_t* bridge);

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_get_config(
    const wernicke_substrate_bridge_t* bridge,
    wernicke_substrate_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int wernicke_substrate_bridge_set_config(
    wernicke_substrate_bridge_t* bridge,
    const wernicke_substrate_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_SUBSTRATE_BRIDGE_H */

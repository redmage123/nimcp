/**
 * @file nimcp_broca_substrate_bridge.h
 * @brief Bridge between Broca's region and neural substrate
 *
 * WHAT: Links language production to metabolic state
 * WHY: Speech production requires sustained prefrontal-motor activation
 * HOW: Monitors ATP/fatigue; modulates fluency, articulation, syntax complexity
 *
 * BIOLOGICAL BASIS:
 * - Broca's area (BA44/45) has high metabolic demands for speech
 * - ATP depletion causes word-finding difficulty (anomia-like symptoms)
 * - Fatigue reduces syntactic complexity and speech rate
 * - Metabolic stress impairs motor planning precision
 * - Prefrontal-motor circuits require sustained energy
 *
 * APHASIA-LIKE EFFECTS:
 * - Low ATP: Telegraphic speech, reduced fluency
 * - High fatigue: Simplified syntax, increased pauses
 * - Metabolic stress: Motor speech errors (like mild apraxia)
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BROCA_SUBSTRATE_BRIDGE_H
#define NIMCP_BROCA_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_SUBSTRATE_BROCA 0x1250

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Metabolic effects on Broca's region
 */
typedef struct {
    float speech_fluency;         /**< Speech fluency/rate [0-1] */
    float word_retrieval;         /**< Lexical access speed [0-1] */
    float syntax_complexity;      /**< Syntactic elaboration capacity [0-1] */
    float articulation_precision; /**< Motor planning precision [0-1] */
    float phonological_accuracy;  /**< Phoneme sequencing accuracy [0-1] */
    float overall_capacity;       /**< Combined language production [0-1] */
} broca_substrate_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_atp_modulation;      /**< ATP affects fluency/retrieval */
    bool enable_fatigue_modulation;  /**< Fatigue affects complexity/precision */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float atp_sensitivity;           /**< How much ATP affects output [0-2] */
    float fatigue_sensitivity;       /**< How much fatigue affects output [0-2] */
    float min_capacity;              /**< Minimum capacity floor [0-1] */
    float fluency_atp_weight;        /**< ATP weight for fluency (default 1.0) */
    float syntax_fatigue_weight;     /**< Fatigue weight for syntax (default 1.0) */
} broca_substrate_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates_processed;      /**< Total substrate updates */
    uint64_t low_atp_events;         /**< Times ATP dropped below threshold */
    uint64_t high_fatigue_events;    /**< Times fatigue exceeded threshold */
    float avg_speech_fluency;        /**< Average fluency level */
    float avg_syntax_complexity;     /**< Average syntax capacity */
    float min_observed_capacity;     /**< Minimum capacity observed */
} broca_substrate_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct broca_substrate_bridge broca_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default config with sensible values
 */
broca_substrate_config_t broca_substrate_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Broca substrate bridge
 * @param broca Broca adapter handle (void* for flexibility)
 * @param substrate Neural substrate handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
broca_substrate_bridge_t* broca_substrate_bridge_create(
    void* broca,
    neural_substrate_t* substrate,
    const broca_substrate_config_t* config
);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void broca_substrate_bridge_destroy(broca_substrate_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update effects from current substrate state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * Reads ATP/fatigue from substrate, computes modulation effects
 */
int broca_substrate_bridge_update(broca_substrate_bridge_t* bridge);

/**
 * @brief Get current metabolic effects
 * @param bridge Bridge handle
 * @param effects Output: current effects
 * @return 0 on success, -1 on error
 */
int broca_substrate_bridge_get_effects(
    const broca_substrate_bridge_t* bridge,
    broca_substrate_effects_t* effects
);

/**
 * @brief Apply effects to Broca adapter
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * Modulates Broca adapter parameters based on metabolic state
 */
int broca_substrate_bridge_apply_effects(broca_substrate_bridge_t* bridge);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Register with bio-async router
 * @param bridge Bridge handle
 * @param router Bio router handle
 * @return 0 on success, -1 on error
 */
int broca_substrate_bridge_register_bio_async(
    broca_substrate_bridge_t* bridge,
    bio_router_t* router
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int broca_substrate_bridge_get_stats(
    const broca_substrate_bridge_t* bridge,
    broca_substrate_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 */
void broca_substrate_bridge_reset_stats(broca_substrate_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BROCA_SUBSTRATE_BRIDGE_H */

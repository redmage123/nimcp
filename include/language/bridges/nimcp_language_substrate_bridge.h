//=============================================================================
// nimcp_language_substrate_bridge.h - Language Layer Neural Substrate Integration
//=============================================================================
/**
 * @file nimcp_language_substrate_bridge.h
 * @brief Bridge between Language Layer and Neural Substrate
 *
 * WHAT: Metabolic modulation of language processing
 * WHY:  Language speed/accuracy depends on ATP, fatigue, and metabolic state
 * HOW:  Reads substrate state, modulates processing parameters accordingly
 *
 * BIOLOGICAL BASIS:
 * - Language areas require high metabolic activity (glucose, oxygen)
 * - ATP depletion → Slower word retrieval, reduced fluency
 * - Fatigue → Increased speech errors, comprehension difficulties
 * - Stress hormones → Can impair or enhance language depending on level
 * - Neurotransmitter levels affect language production and comprehension
 *
 * METABOLIC EFFECTS ON LANGUAGE:
 * - Low ATP → Slower phoneme recognition, word retrieval
 * - High fatigue → Increased tip-of-tongue states, pauses
 * - Stress → Reduced working memory for complex syntax
 * - Dehydration → Slower processing, reduced attention
 * - Glucose availability → Impacts semantic retrieval speed
 *
 * @version 1.0.0 - Phase L8: Substrate Integration
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_SUBSTRATE_BRIDGE_H
#define NIMCP_LANGUAGE_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_orchestrator language_orchestrator_t;
typedef struct neural_substrate neural_substrate_t;

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Metabolic state affecting language
 */
typedef struct {
    float atp_level;                   /**< ATP availability [0-1] */
    float fatigue_level;               /**< Fatigue [0-1] (0=fresh, 1=exhausted) */
    float stress_level;                /**< Stress [0-1] */
    float glucose_level;               /**< Glucose availability [0-1] */
    float oxygen_level;                /**< Oxygen saturation [0-1] */
    float dopamine_level;              /**< Dopamine (affects fluency) [0-1] */
    float acetylcholine_level;         /**< ACh (affects attention) [0-1] */
    float norepinephrine_level;        /**< NE (affects alertness) [0-1] */
} language_metabolic_state_t;

/**
 * @brief Language processing modulation
 */
typedef struct {
    float phoneme_speed_factor;        /**< Phoneme processing speed [0-2] */
    float word_retrieval_factor;       /**< Word retrieval speed [0-2] */
    float semantic_activation_factor;  /**< Semantic spreading speed [0-2] */
    float comprehension_accuracy;      /**< Comprehension accuracy [0-1] */
    float production_fluency;          /**< Production fluency [0-1] */
    float working_memory_capacity;     /**< Effective WM capacity [0-1] */
    float attention_capacity;          /**< Attention capacity [0-1] */
    float error_rate_factor;           /**< Error rate multiplier [1-5] */
} language_modulation_t;

/**
 * @brief Bridge configuration
 */
#ifndef LANGUAGE_SUBSTRATE_CONFIG_T_DEFINED
#define LANGUAGE_SUBSTRATE_CONFIG_T_DEFINED
typedef struct {
    /* Enable flags */
    bool enable_atp_modulation;        /**< Modulate by ATP level */
    bool enable_fatigue_effects;       /**< Apply fatigue effects */
    bool enable_stress_effects;        /**< Apply stress effects */
    bool enable_neurotransmitter_effects; /**< Apply NT effects */

    /* Sensitivity settings */
    float atp_sensitivity;             /**< ATP effect strength [0-2] */
    float fatigue_sensitivity;         /**< Fatigue effect strength [0-2] */
    float stress_sensitivity;          /**< Stress effect strength [0-2] */

    /* Thresholds */
    float critical_atp_threshold;      /**< ATP level causing impairment */
    float high_fatigue_threshold;      /**< Fatigue level causing impairment */
    float optimal_stress_level;        /**< Optimal stress (Yerkes-Dodson) */

    /* Timing */
    uint32_t update_interval_ms;       /**< How often to update */

    /* Bio-async */
    bool enable_bio_async;             /**< Enable bio-async messaging */
} language_substrate_config_t;
#endif /* LANGUAGE_SUBSTRATE_CONFIG_T_DEFINED */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t modulation_updates;       /**< Times modulation applied */
    uint64_t low_atp_events;           /**< Low ATP occurrences */
    uint64_t high_fatigue_events;      /**< High fatigue occurrences */
    uint64_t stress_impacts;           /**< Stress impact events */
    float avg_speed_factor;            /**< Average processing speed */
    float avg_accuracy;                /**< Average accuracy */
    float min_atp_observed;            /**< Minimum ATP observed */
    float max_fatigue_observed;        /**< Maximum fatigue observed */
} language_substrate_stats_t;

/**
 * @brief Bridge state
 */
struct language_substrate_bridge {
    language_substrate_config_t config;
    bool initialized;
    bool active;

    language_orchestrator_t* orchestrator;
    neural_substrate_t* substrate;

    language_metabolic_state_t current_state;
    language_modulation_t current_modulation;

    language_substrate_stats_t stats;
    uint64_t last_update_us;
};

typedef struct language_substrate_bridge language_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

void language_substrate_default_config(language_substrate_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

language_substrate_bridge_t* language_substrate_bridge_create(
    language_orchestrator_t* orchestrator,
    const language_substrate_config_t* config);

void language_substrate_bridge_destroy(language_substrate_bridge_t* bridge);

int language_substrate_bridge_connect_substrate(
    language_substrate_bridge_t* bridge,
    neural_substrate_t* substrate);

//=============================================================================
// Modulation API
//=============================================================================

int language_substrate_bridge_update(language_substrate_bridge_t* bridge);

int language_substrate_bridge_get_modulation(
    const language_substrate_bridge_t* bridge,
    language_modulation_t* modulation);

float language_substrate_bridge_get_speed_factor(
    const language_substrate_bridge_t* bridge);

float language_substrate_bridge_get_accuracy_factor(
    const language_substrate_bridge_t* bridge);

float language_substrate_bridge_get_fluency_factor(
    const language_substrate_bridge_t* bridge);

//=============================================================================
// Metabolic State API
//=============================================================================

int language_substrate_bridge_get_metabolic_state(
    const language_substrate_bridge_t* bridge,
    language_metabolic_state_t* state);

bool language_substrate_bridge_is_impaired(
    const language_substrate_bridge_t* bridge);

const char* language_substrate_bridge_get_impairment_reason(
    const language_substrate_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

int language_substrate_bridge_get_stats(
    const language_substrate_bridge_t* bridge,
    language_substrate_stats_t* stats);

void language_substrate_bridge_reset_stats(language_substrate_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_SUBSTRATE_BRIDGE_H */

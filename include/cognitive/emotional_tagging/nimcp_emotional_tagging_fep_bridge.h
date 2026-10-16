/**
 * @file nimcp_emotional_tagging_fep_bridge.h
 * @brief Free Energy Principle - Emotional Tagging Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between FEP and emotional tagging system
 * WHY:  Emotions arise from precision-weighted prediction errors. Valence reflects
 *       expectation satisfaction; arousal reflects precision/uncertainty.
 * HOW:  FEP prediction errors generate valenced emotional tags; emotions modulate
 *       FEP precision weighting for memory encoding and salience.
 *
 * BIOLOGICAL BASIS:
 * - Barrett & Simmons (2015): Interoceptive predictions generate affective states
 * - Positive PE (better than expected) → Positive valence (joy, satisfaction)
 * - Negative PE (worse than expected) → Negative valence (disappointment, fear)
 * - High precision/uncertainty → High arousal
 * - Low precision/certainty → Low arousal
 *
 * FEP → EMOTIONAL TAGGING:
 * - Prediction error valence generates emotional valence
 * - Precision magnitude generates emotional arousal
 * - Surprise triggers emotional intensity
 *
 * EMOTIONAL TAGGING → FEP:
 * - Emotional arousal modulates precision weighting
 * - Emotional valence influences expected value estimates
 * - Emotional tags enhance memory encoding precision
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMOTIONAL_TAGGING_FEP_BRIDGE_H
#define NIMCP_EMOTIONAL_TAGGING_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for health agent (Phase 8) */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define EMOTIONAL_TAGGING_FEP_PE_VALENCE_SCALING    1.0f
#define EMOTIONAL_TAGGING_FEP_PRECISION_AROUSAL_SCALING  1.0f
#define EMOTIONAL_TAGGING_FEP_SURPRISE_INTENSITY_SCALING 1.0f

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * @brief Configuration for emotional tagging FEP bridge
 *
 * WHAT: Control parameters for bidirectional FEP-emotion integration
 * WHY:  Enable tuning of how prediction errors generate emotions
 * HOW:  Gain parameters and enable flags for each direction
 */
typedef struct {
    /* FEP → Emotional Tagging */
    float pe_valence_gain;              /**< PE to valence scaling */
    float precision_arousal_gain;       /**< Precision to arousal scaling */
    float surprise_intensity_gain;      /**< Surprise to intensity scaling */
    bool enable_pe_valence_generation;  /**< Generate valence from PE */
    bool enable_precision_arousal;      /**< Generate arousal from precision */
    bool enable_surprise_intensity;     /**< Generate intensity from surprise */

    /* Emotional Tagging → FEP */
    float arousal_precision_modulation; /**< How arousal affects precision */
    float valence_value_modulation;     /**< How valence affects value estimates */
    float intensity_encoding_boost;     /**< How intensity boosts memory encoding */
    bool enable_arousal_precision;      /**< Enable arousal → precision */
    bool enable_valence_value;          /**< Enable valence → value */
    bool enable_intensity_encoding;     /**< Enable intensity → encoding */

    /* Sensitivity */
    float fe_sensitivity;               /**< Overall FEP sensitivity */
    float emotion_sensitivity;          /**< Overall emotion sensitivity */
} emotional_tagging_fep_config_t;

/* ============================================================================
 * EFFECTS STRUCTURES
 * ============================================================================ */

/**
 * @brief FEP effects on emotional tagging
 *
 * WHAT: How FEP influences emotional tag generation
 * WHY:  Prediction errors drive emotional responses
 * HOW:  PE valence, precision arousal, surprise intensity
 */
typedef struct {
    float prediction_error_valence;     /**< PE-derived valence [-1, +1] */
    float precision_arousal;            /**< Precision-derived arousal [0, 1] */
    float surprise_intensity;           /**< Surprise-derived intensity [0, 1] */
    emotional_tag_t generated_tag;      /**< Complete emotional tag from FEP */
    bool tag_generated;                 /**< Whether tag was generated */
} emotional_tagging_fep_effects_t;

/**
 * @brief Emotional tagging effects on FEP
 *
 * WHAT: How emotions influence FEP processing
 * WHY:  Emotions modulate precision and value expectations
 * HOW:  Arousal precision, valence value, intensity encoding
 */
typedef struct {
    float precision_modifier;           /**< Arousal-based precision weight */
    float value_modifier;               /**< Valence-based value estimate */
    float encoding_boost;               /**< Intensity-based encoding strength */
    float emotional_salience;           /**< Overall emotional salience boost */
} fep_emotional_tagging_effects_t;

/* ============================================================================
 * STATE AND STATISTICS
 * ============================================================================ */

/**
 * @brief Current bridge state
 */
typedef struct {
    float current_prediction_error;     /**< Latest PE magnitude */
    float current_precision;            /**< Latest precision value */
    float current_valence;              /**< Current emotional valence */
    float current_arousal;              /**< Current emotional arousal */
    float current_intensity;            /**< Current emotional intensity */
    bool emotion_active;                /**< Emotion currently active */
    uint64_t last_emotion_time;         /**< Last emotion generation time */
} emotional_tagging_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t emotion_generation_events; /**< Total emotions generated from PE */
    uint64_t precision_modulation_events; /**< Total precision modulations */
    uint64_t value_modulation_events;   /**< Total value modulations */
    float avg_valence;                  /**< Average emotional valence */
    float avg_arousal;                  /**< Average emotional arousal */
    float avg_intensity;                /**< Average emotional intensity */
    float avg_prediction_error;         /**< Average PE magnitude */
    float avg_precision;                /**< Average precision */
} emotional_tagging_fep_stats_t;

/* ============================================================================
 * BRIDGE STRUCTURE
 * ============================================================================ */

/**
 * @brief Emotional tagging FEP bridge
 *
 * WHAT: Bidirectional integration structure
 * WHY:  Coordinate FEP and emotional tagging systems
 * HOW:  Pointers to both systems + effects + state + stats
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    emotional_tagging_fep_config_t config;
    fep_system_t* fep_system;
    emotional_tag_t* current_tag;       /**< Pointer to current tag (not opaque) */
    emotional_tagging_fep_effects_t fep_effects;
    fep_emotional_tagging_effects_t emotion_effects;
    emotional_tagging_fep_state_t state;
    emotional_tagging_fep_stats_t stats;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent (Phase 8) */
} emotional_tagging_fep_bridge_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible defaults for emotional tagging FEP bridge
 * WHY:  Convenient initialization
 * HOW:  Fill config with default values
 */
int emotional_tagging_fep_default_config(emotional_tagging_fep_config_t* config);

/**
 * @brief Create emotional tagging FEP bridge
 *
 * WHAT: Allocate and initialize bridge structure
 * WHY:  Enable FEP-emotion integration
 * HOW:  Allocate memory, set config, create mutex
 */
emotional_tagging_fep_bridge_t* emotional_tagging_fep_create(
    const emotional_tagging_fep_config_t* config
);

/**
 * @brief Destroy emotional tagging FEP bridge
 *
 * WHAT: Free bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 */
void emotional_tagging_fep_destroy(emotional_tagging_fep_bridge_t* bridge);

/* ============================================================================
 * CONNECTION API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link FEP system to bridge
 * WHY:  Enable FEP → emotion direction
 * HOW:  Store FEP pointer
 */
int emotional_tagging_fep_connect_fep(
    emotional_tagging_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect emotional tag
 *
 * WHAT: Link emotional tag to bridge
 * WHY:  Enable emotion → FEP direction
 * HOW:  Store tag pointer
 */
int emotional_tagging_fep_connect_tag(
    emotional_tagging_fep_bridge_t* bridge,
    emotional_tag_t* tag
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Clear all system pointers
 * WHY:  Safe shutdown
 * HOW:  Set pointers to NULL
 */
int emotional_tagging_fep_disconnect(emotional_tagging_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → EMOTIONAL TAGGING DIRECTION
 * ============================================================================ */

/**
 * @brief Generate emotional valence from prediction error
 *
 * WHAT: Convert PE to emotional valence
 * WHY:  Positive PE = positive emotion, negative PE = negative emotion
 * HOW:  Signed PE magnitude → valence [-1, +1]
 */
int emotional_tagging_fep_generate_pe_valence(
    emotional_tagging_fep_bridge_t* bridge,
    float pe_magnitude
);

/**
 * @brief Generate emotional arousal from precision
 *
 * WHAT: Convert precision to emotional arousal
 * WHY:  High precision/uncertainty = high arousal
 * HOW:  Precision magnitude → arousal [0, 1]
 */
int emotional_tagging_fep_generate_precision_arousal(
    emotional_tagging_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Generate emotional intensity from surprise
 *
 * WHAT: Convert surprise to emotional intensity
 * WHY:  High surprise = intense emotion
 * HOW:  Surprise (free energy) → intensity [0, 1]
 */
int emotional_tagging_fep_generate_surprise_intensity(
    emotional_tagging_fep_bridge_t* bridge,
    float surprise
);

/**
 * @brief Generate complete emotional tag from FEP state
 *
 * WHAT: Create emotional tag from current FEP values
 * WHY:  Unified emotion generation from prediction errors
 * HOW:  Combine PE valence, precision arousal, surprise intensity
 */
int emotional_tagging_fep_generate_tag(
    emotional_tagging_fep_bridge_t* bridge,
    uint64_t timestamp_ms
);

/* ============================================================================
 * EMOTIONAL TAGGING → FEP DIRECTION
 * ============================================================================ */

/**
 * @brief Modulate FEP precision by emotional arousal
 *
 * WHAT: Adjust precision weighting based on arousal
 * WHY:  High arousal events receive higher precision
 * HOW:  Arousal multiplies precision weight
 */
int emotional_tagging_fep_modulate_precision(
    emotional_tagging_fep_bridge_t* bridge
);

/**
 * @brief Modulate FEP value estimates by emotional valence
 *
 * WHAT: Adjust expected values based on valence
 * WHY:  Positive emotions increase expected value
 * HOW:  Valence modifies value predictions
 */
int emotional_tagging_fep_modulate_value(
    emotional_tagging_fep_bridge_t* bridge
);

/**
 * @brief Boost memory encoding by emotional intensity
 *
 * WHAT: Enhance encoding precision for intense emotions
 * WHY:  Emotional events are remembered better
 * HOW:  Intensity increases encoding strength
 */
int emotional_tagging_fep_boost_encoding(
    emotional_tagging_fep_bridge_t* bridge
);

/* ============================================================================
 * UPDATE API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Run all bidirectional updates
 * WHY:  Maintain FEP-emotion coupling
 * HOW:  Call all modulation functions
 */
int emotional_tagging_fep_update(
    emotional_tagging_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 */
int emotional_tagging_fep_get_state(
    const emotional_tagging_fep_bridge_t* bridge,
    emotional_tagging_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int emotional_tagging_fep_get_stats(
    const emotional_tagging_fep_bridge_t* bridge,
    emotional_tagging_fep_stats_t* stats
);

/* ============================================================================
 * BIO-ASYNC INTEGRATION
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 */
int emotional_tagging_fep_connect_bio_async(
    emotional_tagging_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 */
int emotional_tagging_fep_disconnect_bio_async(
    emotional_tagging_fep_bridge_t* bridge
);

/**
 * @brief Check bio-async connection status
 */
bool emotional_tagging_fep_is_bio_async_connected(
    const emotional_tagging_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTIONAL_TAGGING_FEP_BRIDGE_H */

/**
 * @file nimcp_mirror_habituation.h
 * @brief Mirror Neuron Habituation Module
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Implements response attenuation for repeated/familiar stimuli
 * WHY:  Efficient processing requires reduced response to predictable stimuli
 * HOW:  Tracks stimulus history, computes novelty, modulates mirror response
 *
 * THEORETICAL FOUNDATIONS:
 * - Thompson & Spencer (1966): Habituation learning principles
 * - Rankin et al. (2009): Habituation revisited
 * - Press et al. (2011): Action prediction and habituation
 * - Cross et al. (2006): Motor experience modulates action observation
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons show decreased response to repeated actions
 * - Superior temporal sulcus (STS) exhibits habituation to biological motion
 * - Repetition suppression in premotor cortex
 * - Novel actions produce stronger resonance than familiar ones
 * - Dishabituation occurs with stimulus change or sensitizing event
 *
 * HABITUATION MECHANISMS:
 * 1. Exponential decay of response with repetition
 * 2. Spontaneous recovery over time
 * 3. Stimulus-specific adaptation (generalization gradient)
 * 4. Dishabituation with novel/strong stimuli
 * 5. Potentiation for highly trained/expected actions
 *
 * PARAMETERS:
 * - Habituation rate: How fast response decreases
 * - Recovery rate: How fast response recovers
 * - Generalization width: How similar stimuli must be to habituate
 * - Dishabituation threshold: What triggers dishabituation
 *
 * BIO-ASYNC MESSAGES:
 * - BIO_MSG_MIRROR_HABITUATION_UPDATE: Habituation level changed
 * - BIO_MSG_MIRROR_DISHABITUATION: Dishabituation event
 *
 * @see nimcp_mirror_neurons.h
 * @see nimcp_mirror_attention_bridge.h
 */

#ifndef NIMCP_MIRROR_HABITUATION_H
#define NIMCP_MIRROR_HABITUATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum tracked stimulus patterns */
#define HABITUATION_MAX_PATTERNS        128

/** @brief Feature vector dimension for stimulus encoding */
#define HABITUATION_FEATURE_DIM         16

/** @brief Maximum history entries per pattern */
#define HABITUATION_HISTORY_SIZE        32

/** @brief SIMD batch threshold */
#define HABITUATION_SIMD_THRESHOLD      8

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Habituation state for a stimulus
 */
typedef enum {
    HABITUATION_STATE_NOVEL = 0,        /**< First encounter, full response */
    HABITUATION_STATE_HABITUATING,      /**< Response decreasing */
    HABITUATION_STATE_HABITUATED,       /**< Response at minimum */
    HABITUATION_STATE_RECOVERING,       /**< Response increasing (spontaneous) */
    HABITUATION_STATE_DISHABITUATED,    /**< Response restored by novel event */
    HABITUATION_STATE_SENSITIZED        /**< Response enhanced above baseline */
} habituation_state_t;

/**
 * @brief Stimulus category for habituation
 */
typedef enum {
    STIMULUS_CATEGORY_ACTION = 0,       /**< Observed action */
    STIMULUS_CATEGORY_AGENT,            /**< Specific agent */
    STIMULUS_CATEGORY_CONTEXT,          /**< Environmental context */
    STIMULUS_CATEGORY_OUTCOME,          /**< Action outcome */
    STIMULUS_CATEGORY_COMPOSITE         /**< Multi-dimensional */
} stimulus_category_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Stimulus encoding for habituation tracking
 *
 * WHAT: Encoded representation of stimulus for comparison
 */
typedef struct {
    uint32_t stimulus_id;               /**< Unique identifier */
    stimulus_category_t category;

    /** Feature encoding */
    float features[HABITUATION_FEATURE_DIM];
    uint32_t feature_count;

    /** Action-specific */
    uint32_t action_type;               /**< Action category if applicable */
    uint32_t effector;                  /**< Body part if applicable */
    uint32_t agent_id;                  /**< Agent performing action */

    /** Context */
    uint32_t context_hash;              /**< Hash of context features */
} stimulus_encoding_t;

/**
 * @brief Habituation record for a stimulus pattern
 */
typedef struct {
    stimulus_encoding_t stimulus;

    /** Habituation state */
    habituation_state_t state;
    float habituation_level;            /**< Current level [0=full, 1=habituated] */
    float response_gain;                /**< Actual gain = 1 - habituation_level */

    /** Exposure tracking */
    uint32_t exposure_count;            /**< Total exposures */
    uint64_t first_exposure_us;
    uint64_t last_exposure_us;
    float inter_stimulus_interval_ms;   /**< Average ISI */

    /** History */
    float response_history[HABITUATION_HISTORY_SIZE];
    uint64_t time_history[HABITUATION_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;

    /** Recovery tracking */
    float time_since_last_ms;
    float recovery_rate;                /**< Pattern-specific recovery */

    bool active;
} habituation_record_t;

/**
 * @brief Habituation query result
 */
typedef struct {
    float response_gain;                /**< Gain to apply [0-1] */
    float novelty;                      /**< Novelty score [0-1] */
    habituation_state_t state;

    bool is_new_pattern;                /**< First exposure */
    bool dishabituated;                 /**< Dishabituation occurred */
    bool sensitized;                    /**< Enhanced response */

    uint32_t matched_pattern_id;        /**< Which pattern matched */
    float match_similarity;             /**< How similar to stored pattern */

    /** Prediction */
    float expected_next_exposure_ms;    /**< When do we expect next exposure */
    float temporal_surprise;            /**< Deviation from expected timing */
} habituation_result_t;

/**
 * @brief Configuration
 */
typedef struct {
    /** Habituation parameters */
    float habituation_rate;             /**< How fast to habituate [0-1] */
    float asymptote;                    /**< Minimum response level */
    float recovery_rate;                /**< Spontaneous recovery rate */
    float recovery_time_constant_ms;    /**< Time constant for recovery */

    /** Generalization */
    float generalization_width;         /**< Similarity threshold for generalization */
    bool enable_generalization;         /**< Allow cross-stimulus habituation */

    /** Dishabituation */
    float dishabituation_threshold;     /**< Novelty threshold for dishabituation */
    float dishabituation_boost;         /**< How much dishabituation restores */

    /** Sensitization */
    float sensitization_threshold;      /**< Intensity for sensitization */
    float sensitization_gain;           /**< Enhanced response factor */
    bool enable_sensitization;

    /** Temporal */
    float temporal_window_ms;           /**< Window for ISI computation */
    float expected_isi_ms;              /**< Expected inter-stimulus interval */

    /** SIMD */
    bool enable_simd;

    /** Bio-async */
    bool bio_async_enabled;
} habituation_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t total_stimuli;
    uint64_t novel_stimuli;
    uint64_t habituated_stimuli;
    uint64_t dishabituation_events;
    uint64_t sensitization_events;

    float avg_response_gain;
    float avg_habituation_level;
    float avg_novelty;

    uint32_t active_patterns;
    uint32_t pattern_capacity;

    uint64_t simd_operations;
} habituation_stats_t;

/** Forward declaration */
typedef struct habituation_system habituation_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 */
habituation_config_t habituation_config_default(void);

/**
 * @brief Create habituation system
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL
 */
habituation_system_t* habituation_create(const habituation_config_t* config);

/**
 * @brief Destroy system
 */
void habituation_destroy(habituation_system_t* system);

//=============================================================================
// Core Processing API
//=============================================================================

/**
 * @brief Process stimulus and get habituation-modulated response
 *
 * WHAT: Main entry - compute response gain for stimulus
 * WHY:  Modulate mirror response based on familiarity
 *
 * @param system System handle
 * @param stimulus Stimulus encoding
 * @param intensity Stimulus intensity [0-1]
 * @param result Output: Habituation result
 * @return true on success
 */
bool habituation_process(
    habituation_system_t* system,
    const stimulus_encoding_t* stimulus,
    float intensity,
    habituation_result_t* result
);

/**
 * @brief Query habituation level without updating
 *
 * @param system System handle
 * @param stimulus Stimulus to query
 * @return Response gain [0-1]
 */
float habituation_query(
    const habituation_system_t* system,
    const stimulus_encoding_t* stimulus
);

/**
 * @brief Batch process stimuli (SIMD)
 */
uint32_t habituation_process_batch(
    habituation_system_t* system,
    const stimulus_encoding_t* stimuli,
    const float* intensities,
    habituation_result_t* results,
    uint32_t count
);

//=============================================================================
// Stimulus Encoding API
//=============================================================================

/**
 * @brief Create stimulus encoding from action
 *
 * @param action_type Action category
 * @param effector Body part
 * @param agent_id Agent identifier
 * @param features Optional feature vector
 * @param feature_count Feature count
 * @param encoding Output encoding
 */
void habituation_encode_action(
    uint32_t action_type,
    uint32_t effector,
    uint32_t agent_id,
    const float* features,
    uint32_t feature_count,
    stimulus_encoding_t* encoding
);

/**
 * @brief Compute stimulus similarity
 *
 * @param a First stimulus
 * @param b Second stimulus
 * @return Similarity [0-1]
 */
float habituation_stimulus_similarity(
    const stimulus_encoding_t* a,
    const stimulus_encoding_t* b
);

//=============================================================================
// Dishabituation API
//=============================================================================

/**
 * @brief Trigger dishabituation for all patterns
 *
 * WHAT: Reset habituation (e.g., after surprising event)
 *
 * @param system System handle
 * @param strength Dishabituation strength [0-1]
 */
void habituation_trigger_dishabituation(
    habituation_system_t* system,
    float strength
);

/**
 * @brief Trigger dishabituation for specific category
 */
void habituation_dishabituate_category(
    habituation_system_t* system,
    stimulus_category_t category,
    float strength
);

/**
 * @brief Check if stimulus would cause dishabituation
 */
bool habituation_would_dishabituate(
    const habituation_system_t* system,
    const stimulus_encoding_t* stimulus,
    float intensity
);

//=============================================================================
// Recovery API
//=============================================================================

/**
 * @brief Update spontaneous recovery
 *
 * WHAT: Process time-based recovery of habituated responses
 * WHY:  Called periodically to restore response potential
 *
 * @param system System handle
 * @param dt_ms Time since last update
 */
void habituation_update_recovery(
    habituation_system_t* system,
    float dt_ms
);

/**
 * @brief Get pattern record
 */
const habituation_record_t* habituation_get_record(
    const habituation_system_t* system,
    uint32_t pattern_id
);

/**
 * @brief Clear pattern (force novelty on next exposure)
 */
void habituation_clear_pattern(
    habituation_system_t* system,
    uint32_t pattern_id
);

//=============================================================================
// SIMD Optimization API
//=============================================================================

/**
 * @brief SIMD batch similarity computation
 */
void habituation_simd_similarities(
    const float* query_features,
    const float* pattern_features,
    float* similarities,
    uint32_t pattern_count,
    uint32_t feature_dim
);

/**
 * @brief SIMD batch habituation update
 */
void habituation_simd_update(
    float* habituation_levels,
    const float* intensities,
    float habituation_rate,
    float asymptote,
    uint32_t count
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async
 */
bool habituation_register_bio_async(habituation_system_t* system);

/**
 * @brief Unregister from bio-async
 */
void habituation_unregister_bio_async(habituation_system_t* system);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get statistics
 */
bool habituation_get_stats(
    const habituation_system_t* system,
    habituation_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void habituation_reset_stats(habituation_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_HABITUATION_H */

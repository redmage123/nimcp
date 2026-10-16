/**
 * @file nimcp_mirror_emotion_bridge.h
 * @brief Mirror Neuron - Emotion Recognition Bidirectional Bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Bidirectional integration between mirror neuron system and emotion recognition
 * WHY:  Emotional contagion and empathy arise from mirroring observed emotional expressions
 * HOW:  Mirror neurons detect emotional actions/expressions; emotion recognition provides context
 *
 * THEORETICAL FOUNDATIONS:
 * - Gallese (2003): Mirror neurons and emotional understanding
 * - Preston & de Waal (2002): Perception-Action Model of empathy
 * - Carr et al. (2003): Neural mechanisms for empathy
 * - Iacoboni (2009): Mirror neurons and social cognition
 *
 * BIOLOGICAL BASIS:
 * - Mirror neurons fire for both observed and executed emotional expressions
 * - Insula mediates emotional contagion via mirror-premotor-insula pathway
 * - Facial mimicry facilitates emotion recognition (facial feedback hypothesis)
 * - Emotional resonance enables rapid understanding of others' feelings
 *
 * INTEGRATION FLOW:
 * Mirror → Emotion Recognition:
 *   1. Mirror neurons detect emotional expression (facial/vocal/bodily)
 *   2. Resonance pattern activates emotion recognition from embodied simulation
 *   3. Recognized emotion tagged with mirror-derived confidence boost
 *
 * Emotion Recognition → Mirror:
 *   1. Recognized emotion context modulates mirror sensitivity
 *   2. Emotional valence affects resonance gain (empathetic vs. counter-empathetic)
 *   3. Crisis detection triggers mirror suppression for controlled response
 *
 * SIMD OPTIMIZATIONS:
 * - Vectorized emotion feature comparison using tensor_simd_dot_f32
 * - Batch processing of facial action unit patterns
 * - SIMD emotional distance computation for contagion strength
 *
 * BIO-ASYNC MESSAGES:
 * - BIO_MSG_MIRROR_EMOTION_OBSERVATION: Mirror detected emotional expression
 * - BIO_MSG_MIRROR_EMOTION_RESONANCE: Emotional resonance triggered
 * - BIO_MSG_MIRROR_EMOTION_CONTAGION: Emotional contagion event
 * - BIO_MSG_MIRROR_EMOTION_MODULATE: Emotion modulating mirror activity
 *
 * @see nimcp_emotion_recognition.h
 * @see nimcp_mirror_neurons.h
 * @see nimcp_mirror_tom_bridge.h
 */

#ifndef NIMCP_MIRROR_EMOTION_BRIDGE_H
#define NIMCP_MIRROR_EMOTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Note: BIO_MODULE_MIRROR_EMOTION_BRIDGE is defined in nimcp_bio_messages.h */

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum dimension of emotion feature vector */
#define MIRROR_EMOTION_FEATURE_DIM          64

/** @brief Maximum number of tracked emotional agents */
#define MIRROR_EMOTION_MAX_AGENTS           32

/** @brief SIMD batch threshold for vectorized operations */
#define MIRROR_EMOTION_SIMD_THRESHOLD       16

/** @brief Number of facial action units tracked */
#define MIRROR_EMOTION_ACTION_UNITS         17

/** @brief Number of basic emotions for resonance */
#define MIRROR_EMOTION_BASIC_COUNT          6

/** @brief Number of extended emotions */
#define MIRROR_EMOTION_EXTENDED_COUNT       13

/** @brief Total emotion categories */
#define MIRROR_EMOTION_TOTAL_COUNT          19

/** @brief Maximum history entries for emotional trajectory */
#define MIRROR_EMOTION_HISTORY_SIZE         16

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Emotional expression modality detected by mirror neurons
 *
 * WHAT: Which channel detected the emotional expression
 * WHY:  Different modalities have different mirror pathways
 */
typedef enum {
    MIRROR_EMOTION_MODALITY_NONE = 0,
    MIRROR_EMOTION_MODALITY_FACIAL,      /**< Facial expression */
    MIRROR_EMOTION_MODALITY_VOCAL,       /**< Vocal prosody/intonation */
    MIRROR_EMOTION_MODALITY_BODILY,      /**< Body posture/gesture */
    MIRROR_EMOTION_MODALITY_GAZE,        /**< Eye gaze/direction */
    MIRROR_EMOTION_MODALITY_MULTIMODAL   /**< Multiple channels fused */
} mirror_emotion_modality_t;

/**
 * @brief Contagion response type
 *
 * WHAT: How the observer responds to perceived emotion
 * WHY:  Not all emotional observation leads to matching emotion
 */
typedef enum {
    MIRROR_CONTAGION_NONE = 0,           /**< No contagion effect */
    MIRROR_CONTAGION_EMPATHIC,           /**< Match observed emotion (empathy) */
    MIRROR_CONTAGION_SYMPATHIC,          /**< Feel for but not same (sympathy) */
    MIRROR_CONTAGION_COUNTER,            /**< Counter-empathetic response */
    MIRROR_CONTAGION_REGULATED           /**< Regulated/controlled response */
} mirror_contagion_type_t;

/**
 * @brief Mirror-emotion bridge processing state
 */
typedef enum {
    MIRROR_EMOTION_STATE_IDLE = 0,
    MIRROR_EMOTION_STATE_OBSERVING,
    MIRROR_EMOTION_STATE_RESONATING,
    MIRROR_EMOTION_STATE_CONTAGION_ACTIVE,
    MIRROR_EMOTION_STATE_REGULATED
} mirror_emotion_state_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Emotional expression observation from mirror neurons
 *
 * WHAT: Data captured when mirror neurons detect emotional expression
 * WHY:  Bridge input from mirror neuron observations
 */
typedef struct {
    uint32_t agent_id;                                   /**< Observed agent ID */
    mirror_emotion_modality_t modality;                  /**< Detection modality */

    /** Facial action unit activations [0.0-1.0] */
    float action_units[MIRROR_EMOTION_ACTION_UNITS];
    uint32_t active_au_count;                            /**< Number of active AUs */

    /** Emotion feature vector from mirror resonance */
    float emotion_features[MIRROR_EMOTION_FEATURE_DIM];
    uint32_t feature_dim;                                /**< Active dimensions */

    /** Mirror neuron resonance metrics */
    float resonance_strength;                            /**< Overall resonance [0.0-1.0] */
    float motor_priming;                                 /**< Motor mimicry tendency */
    float observation_confidence;                        /**< Detection confidence */

    /** Temporal data */
    uint64_t timestamp_us;                               /**< Observation timestamp */
    uint64_t duration_us;                                /**< Expression duration */

    /** Context */
    bool is_genuine;                                     /**< Genuine vs posed expression */
    bool is_directed_at_self;                            /**< Directed at observer */
} mirror_emotion_observation_t;

/**
 * @brief Emotional resonance result
 *
 * WHAT: Result of processing emotional observation through mirror-emotion bridge
 * WHY:  Output structure for emotion understanding from mirroring
 */
typedef struct {
    /** Recognized emotion */
    uint32_t emotion_category;                           /**< Detected emotion category */
    float confidence;                                    /**< Recognition confidence */
    float intensity;                                     /**< Emotion intensity [0.0-1.0] */

    /** Dimensional representation */
    float valence;                                       /**< Pleasure [-1.0, +1.0] */
    float arousal;                                       /**< Activation [0.0, 1.0] */
    float dominance;                                     /**< Control [-1.0, +1.0] */

    /** Contagion response */
    mirror_contagion_type_t contagion_type;
    float contagion_strength;                            /**< How much emotion transferred */
    float empathy_level;                                 /**< Empathic resonance [0.0-1.0] */

    /** Mirror-derived metrics */
    float embodied_confidence;                           /**< Confidence from embodiment */
    float facial_mimicry_strength;                       /**< Automatic mimicry level */
    float motor_resonance_contribution;                  /**< Motor system contribution */

    /** Safety flags */
    bool requires_regulation;                            /**< Should regulate contagion */
    bool crisis_detected;                                /**< Crisis emotion detected */
    float distress_level;                                /**< Observed distress [0.0-1.0] */

    uint64_t timestamp_us;
} mirror_emotion_resonance_t;

/**
 * @brief Per-agent emotional state tracking
 *
 * WHAT: Track emotional state of individual agents over time
 * WHY:  Continuous emotional understanding requires history
 */
typedef struct {
    uint32_t agent_id;
    bool active;

    /** Current emotional state */
    uint32_t current_emotion;
    float current_intensity;
    float current_valence;
    float current_arousal;

    /** Emotional history */
    uint32_t emotion_history[MIRROR_EMOTION_HISTORY_SIZE];
    float intensity_history[MIRROR_EMOTION_HISTORY_SIZE];
    uint64_t timestamp_history[MIRROR_EMOTION_HISTORY_SIZE];
    uint32_t history_count;
    uint32_t history_index;

    /** Relationship metrics */
    float familiarity;                                   /**< How well we know this agent */
    float empathy_tendency;                              /**< Tendency to empathize */
    float contagion_susceptibility;                      /**< Susceptibility to their emotions */

    /** Temporal metrics */
    uint64_t first_observation_us;
    uint64_t last_observation_us;
    uint32_t total_observations;

    /** Emotion statistics */
    float avg_valence;
    float avg_arousal;
    float valence_variance;
    float arousal_variance;
} mirror_emotion_agent_state_t;

/**
 * @brief Mirror-emotion bridge configuration
 *
 * WHAT: Configuration parameters for bridge behavior
 * WHY:  Tune emotional contagion and empathy sensitivity
 */
typedef struct {
    /** Resonance parameters */
    float resonance_threshold;           /**< Min resonance for emotion detection */
    float contagion_threshold;           /**< Min for contagion trigger */
    float empathy_gain;                  /**< Empathy response scaling */
    float mimicry_suppression;           /**< How much to suppress automatic mimicry */

    /** Modality weights */
    float facial_weight;                 /**< Weight for facial expressions */
    float vocal_weight;                  /**< Weight for vocal prosody */
    float bodily_weight;                 /**< Weight for body language */
    float gaze_weight;                   /**< Weight for gaze cues */

    /** Safety parameters */
    float crisis_threshold;              /**< Threshold for crisis detection */
    float regulation_threshold;          /**< When to engage emotional regulation */
    bool enable_contagion_regulation;    /**< Enable automatic regulation */
    bool enable_crisis_suppression;      /**< Suppress contagion during crisis */

    /** SIMD optimization */
    bool enable_simd;                    /**< Enable SIMD optimizations */
    uint32_t simd_batch_size;            /**< Batch size for SIMD ops */

    /** Integration */
    bool bidirectional_enabled;          /**< Enable emotion→mirror feedback */
    bool bio_async_enabled;              /**< Enable bio-async messaging */
    bool immune_integration;             /**< Enable immune system integration */
} mirror_emotion_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_observations;
    uint64_t resonance_events;
    uint64_t contagion_events;
    uint64_t regulation_events;
    uint64_t crisis_detections;

    float avg_resonance_strength;
    float avg_contagion_strength;
    float avg_empathy_level;

    uint64_t simd_operations;
    uint64_t scalar_fallbacks;

    uint32_t active_agents;
    float avg_agent_familiarity;
} mirror_emotion_stats_t;

/** Forward declaration */
typedef struct mirror_emotion_bridge mirror_emotion_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration with balanced empathy settings
 */
mirror_emotion_config_t mirror_emotion_config_default(void);

/**
 * @brief Create mirror-emotion bridge
 *
 * WHAT: Initialize bidirectional bridge between mirror neurons and emotion recognition
 * WHY:  Enable emotional understanding through embodied simulation
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
mirror_emotion_bridge_t* mirror_emotion_create(const mirror_emotion_config_t* config);

/**
 * @brief Destroy mirror-emotion bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mirror_emotion_destroy(mirror_emotion_bridge_t* bridge);

//=============================================================================
// Observation Processing API
//=============================================================================

/**
 * @brief Process mirror neuron observation of emotional expression
 *
 * WHAT: Main entry point - mirror neurons detected emotional expression
 * WHY:  Convert mirror observation to emotional understanding
 * HOW:  Apply embodied simulation, compute resonance, detect emotion
 *
 * @param bridge Bridge handle
 * @param observation Mirror neuron observation data
 * @param result Output: Emotional resonance result
 * @return true on success
 */
bool mirror_emotion_process_observation(
    mirror_emotion_bridge_t* bridge,
    const mirror_emotion_observation_t* observation,
    mirror_emotion_resonance_t* result
);

/**
 * @brief Batch process multiple observations (SIMD-optimized)
 *
 * WHAT: Process multiple observations efficiently
 * WHY:  Enable high-throughput emotional processing
 *
 * @param bridge Bridge handle
 * @param observations Array of observations
 * @param results Output array of results
 * @param count Number of observations
 * @return Number successfully processed
 */
uint32_t mirror_emotion_process_batch(
    mirror_emotion_bridge_t* bridge,
    const mirror_emotion_observation_t* observations,
    mirror_emotion_resonance_t* results,
    uint32_t count
);

//=============================================================================
// Contagion and Empathy API
//=============================================================================

/**
 * @brief Compute emotional contagion strength
 *
 * WHAT: Calculate how much observed emotion transfers to observer
 * WHY:  Emotional contagion is basis of empathy
 * HOW:  Use SIMD-optimized similarity and familiarity weighting
 *
 * @param bridge Bridge handle
 * @param agent_id Observed agent
 * @param observed_emotion Detected emotion category
 * @param intensity Emotion intensity
 * @return Contagion strength [0.0-1.0]
 */
float mirror_emotion_compute_contagion(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id,
    uint32_t observed_emotion,
    float intensity
);

/**
 * @brief Trigger empathic response
 *
 * WHAT: Generate empathic response to observed emotion
 * WHY:  Empathy requires both understanding and affective sharing
 *
 * @param bridge Bridge handle
 * @param resonance Emotional resonance from observation
 * @return true if empathic response triggered
 */
bool mirror_emotion_trigger_empathy(
    mirror_emotion_bridge_t* bridge,
    const mirror_emotion_resonance_t* resonance
);

/**
 * @brief Regulate emotional contagion
 *
 * WHAT: Apply regulation to prevent excessive contagion
 * WHY:  Unregulated contagion can be overwhelming (empathic distress)
 *
 * @param bridge Bridge handle
 * @param agent_id Agent whose emotion to regulate against
 * @param regulation_level How much to regulate [0.0-1.0]
 * @return New contagion level after regulation
 */
float mirror_emotion_regulate_contagion(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id,
    float regulation_level
);

//=============================================================================
// Agent State API
//=============================================================================

/**
 * @brief Get or create agent state
 *
 * @param bridge Bridge handle
 * @param agent_id Agent identifier
 * @return Agent state pointer or NULL
 */
mirror_emotion_agent_state_t* mirror_emotion_get_agent(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id
);

/**
 * @brief Update agent familiarity
 *
 * WHAT: Adjust how familiar we are with an agent
 * WHY:  Familiarity affects empathy and contagion strength
 *
 * @param bridge Bridge handle
 * @param agent_id Agent identifier
 * @param delta Familiarity change
 */
void mirror_emotion_update_familiarity(
    mirror_emotion_bridge_t* bridge,
    uint32_t agent_id,
    float delta
);

//=============================================================================
// SIMD Optimization API
//=============================================================================

/**
 * @brief SIMD-optimized emotion feature similarity
 *
 * WHAT: Compute cosine similarity between emotion feature vectors
 * WHY:  Fast comparison for emotion matching
 *
 * @param features_a First feature vector
 * @param features_b Second feature vector
 * @param dim Vector dimension
 * @return Cosine similarity [-1.0, +1.0]
 */
float mirror_emotion_simd_similarity(
    const float* features_a,
    const float* features_b,
    uint32_t dim
);

/**
 * @brief SIMD batch facial action unit comparison
 *
 * WHAT: Compare multiple AU patterns efficiently
 * WHY:  Batch AU comparison for expression matching
 *
 * @param observed_aus Observed AU patterns [count * MIRROR_EMOTION_ACTION_UNITS]
 * @param template_aus Template patterns to match against
 * @param similarities Output similarities
 * @param count Number of patterns
 */
void mirror_emotion_simd_au_compare(
    const float* observed_aus,
    const float* template_aus,
    float* similarities,
    uint32_t count
);

//=============================================================================
// Modulation API (Emotion Recognition → Mirror)
//=============================================================================

/**
 * @brief Modulate mirror neuron sensitivity based on emotional context
 *
 * WHAT: Emotion recognition feeds back to mirror neurons
 * WHY:  Emotional state affects mirror neuron responsivity
 *
 * @param bridge Bridge handle
 * @param emotion Current emotional state
 * @param intensity Emotion intensity
 * @param valence Emotional valence
 * @return New mirror sensitivity multiplier
 */
float mirror_emotion_modulate_sensitivity(
    mirror_emotion_bridge_t* bridge,
    uint32_t emotion,
    float intensity,
    float valence
);

/**
 * @brief Set crisis mode
 *
 * WHAT: Engage crisis mode to regulate emotional processing
 * WHY:  Crisis requires controlled, not contagious, response
 *
 * @param bridge Bridge handle
 * @param crisis_active Whether crisis mode is active
 */
void mirror_emotion_set_crisis_mode(
    mirror_emotion_bridge_t* bridge,
    bool crisis_active
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register bridge with bio-async system
 *
 * @param bridge Bridge handle
 * @return true on success
 */
bool mirror_emotion_register_bio_async(mirror_emotion_bridge_t* bridge);

/**
 * @brief Unregister from bio-async system
 *
 * @param bridge Bridge handle
 */
void mirror_emotion_unregister_bio_async(mirror_emotion_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return true on success
 */
bool mirror_emotion_get_stats(
    const mirror_emotion_bridge_t* bridge,
    mirror_emotion_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void mirror_emotion_reset_stats(mirror_emotion_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_EMOTION_BRIDGE_H */

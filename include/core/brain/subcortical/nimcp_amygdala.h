/**
 * @file nimcp_amygdala.h
 * @brief Amygdala implementation for emotion processing and fear conditioning
 *
 * WHAT: Biologically-inspired amygdala model with nuclei-specific processing
 * WHY:  Enable emotion-driven learning, threat detection, and fear responses
 * HOW:  Implements lateral, basal, central nuclei with fear conditioning circuits
 *
 * Biological basis:
 * - Lateral nucleus (LA): Primary sensory input, CS-US association site
 * - Basal nucleus (BA): Integration with hippocampus for contextual fear
 * - Central nucleus (CeA): Primary output for autonomic fear responses
 * - Intercalated cells (ITC): Inhibitory control for fear extinction
 * - Medial nucleus: Olfactory and social processing
 *
 * Key features:
 * - Pavlovian fear conditioning (CS-US associations)
 * - Contextual fear (hippocampal integration)
 * - Fear extinction (prefrontal cortex inhibition via ITC)
 * - Emotion valence computation
 * - Threat detection and graded fear response
 * - Anxiety state maintenance
 *
 * References:
 * - LeDoux, J. (2007). The amygdala. Current Biology
 * - Pape & Pare (2010). Plastic synaptic networks of the amygdala
 * - Duvarci & Pare (2014). Amygdala microcircuits controlling fear
 */

#ifndef NIMCP_AMYGDALA_H
#define NIMCP_AMYGDALA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for emotional system integration */
typedef struct emotional_system emotional_system_t;

/* ============================================================================
 * Constants and Limits
 * ============================================================================ */

/* Memory and dimensionality limits */
#define AMYG_MAX_FEAR_MEMORIES      256   /**< Maximum stored fear memories */
#define AMYG_MAX_CS_FEATURES        64    /**< Max features in conditioned stimulus */
#define AMYG_MAX_CONTEXT_DIM        32    /**< Context vector dimensionality */

/* Default configuration parameters */
#define AMYG_DEFAULT_FEAR_THRESHOLD     0.5f  /**< Default threshold for fear response */
#define AMYG_DEFAULT_ANXIETY_THRESHOLD  0.3f  /**< Threshold for anxiety state */
#define AMYG_DEFAULT_THREAT_THRESHOLD   0.4f  /**< Sensitivity to threats */
#define AMYG_DEFAULT_ANXIETY_DECAY      0.01f /**< Anxiety decay rate per step */
#define AMYG_DEFAULT_ACTIVATION_DECAY   0.02f /**< Nuclear activation decay rate */
#define AMYG_DEFAULT_EXTINCTION_RATE    0.05f /**< Fear extinction learning rate */
#define AMYG_DEFAULT_CONDITIONING_RATE  0.1f  /**< Fear conditioning learning rate */
#define AMYG_DEFAULT_GENERALIZATION     0.3f  /**< Default generalization width */
#define AMYG_DEFAULT_PFC_INHIBITION_WEIGHT 0.7f /**< Prefrontal inhibition strength */
#define AMYG_DEFAULT_SPONTANEOUS_RECOVERY  0.001f /**< Rate of fear return */
#define AMYG_DEFAULT_RECONSOLIDATION_MS (6 * 60 * 60 * 1000) /**< 6 hours */
#define AMYG_DEFAULT_BIO_INBOX_CAPACITY 32 /**< Message queue size */

/* Nucleus default parameters */
#define AMYG_NUCLEUS_BASELINE           0.05f /**< Baseline activation */
#define AMYG_NUCLEUS_GAIN               1.0f  /**< Default activation gain */
#define AMYG_NUCLEUS_PLASTICITY_RATE    0.01f /**< Default plasticity rate */
#define AMYG_NUCLEUS_ITC_PLASTICITY     0.05f /**< ITC faster plasticity for extinction */
#define AMYG_NUCLEUS_DOPAMINE_INIT      0.5f  /**< Initial dopamine level */
#define AMYG_NUCLEUS_NE_INIT            0.3f  /**< Initial norepinephrine level */
#define AMYG_NUCLEUS_CORTISOL_INIT      0.2f  /**< Initial cortisol level */

/* Inter-nucleus connection weights */
#define AMYG_WEIGHT_LA_TO_BA            0.3f  /**< LA -> BA projection */
#define AMYG_WEIGHT_LA_TO_CEA           0.4f  /**< LA -> CeA projection */
#define AMYG_WEIGHT_BA_FROM_LA          0.5f  /**< LA -> BA projection (BA perspective) */
#define AMYG_WEIGHT_BA_TO_CEA           0.4f  /**< BA -> CeA projection */
#define AMYG_WEIGHT_ITC_INHIBITION     -0.6f  /**< ITC inhibitory effect on CeA */
#define AMYG_WEIGHT_MEA_FROM_LA         0.2f  /**< LA -> MeA projection */
#define AMYG_WEIGHT_ITC_FROM_LA         0.3f  /**< LA -> ITC projection */

/* Activation dynamics */
#define AMYG_ACTIVATION_TAU_MS          50.0f /**< Time constant in ms */
#define AMYG_NE_MODULATION_BASE         0.5f  /**< NE modulation baseline */
#define AMYG_NE_MODULATION_SCALE        0.5f  /**< NE modulation scaling */
#define AMYG_DA_MODULATION_BASE         0.5f  /**< DA modulation baseline */
#define AMYG_DA_MODULATION_SCALE        0.5f  /**< DA modulation scaling */
#define AMYG_SIMILARITY_EPSILON         1e-10f /**< Epsilon for cosine similarity */

/* Threat level thresholds */
#define AMYG_THREAT_NONE_INTENSITY      0.0f  /**< Intensity for no threat */
#define AMYG_THREAT_LOW_INTENSITY       0.2f  /**< Intensity for low threat */
#define AMYG_THREAT_MODERATE_INTENSITY  0.5f  /**< Intensity for moderate threat */
#define AMYG_THREAT_HIGH_INTENSITY      0.75f /**< Intensity for high threat */
#define AMYG_THREAT_SEVERE_INTENSITY    1.0f  /**< Intensity for severe threat */
#define AMYG_THREAT_LOW_THRESHOLD       0.15f /**< Threshold for low threat */
#define AMYG_THREAT_MODERATE_THRESHOLD  0.4f  /**< Threshold for moderate threat */
#define AMYG_THREAT_HIGH_THRESHOLD      0.65f /**< Threshold for high threat */
#define AMYG_THREAT_SEVERE_THRESHOLD    0.9f  /**< Threshold for severe threat */

/* Fear output multipliers */
#define AMYG_OUTPUT_FREEZING_MULT       0.8f  /**< Freezing output multiplier */
#define AMYG_OUTPUT_STARTLE_MULT        0.6f  /**< Startle output multiplier */
#define AMYG_OUTPUT_AUTONOMIC_MULT      0.9f  /**< Autonomic output multiplier */
#define AMYG_OUTPUT_HORMONAL_MULT       0.7f  /**< Hormonal output multiplier */
#define AMYG_OUTPUT_ATTENTION_MULT      1.0f  /**< Attention output multiplier */

/* Stimulus processing */
#define AMYG_CS_MATCH_THRESHOLD         0.9f  /**< Threshold for CS match in conditioning */
#define AMYG_EXTINCTION_MATCH_THRESHOLD 0.7f  /**< Threshold for extinction match */
#define AMYG_CONTEXT_MIN_FACTOR         0.3f  /**< Minimum context factor */
#define AMYG_CONTEXT_SCALE_FACTOR       0.7f  /**< Context scaling factor */
#define AMYG_ANXIETY_INCREMENT          0.1f  /**< Anxiety increment on fear event */
#define AMYG_LA_INPUT_CS_WEIGHT         0.5f  /**< CS contribution to LA input */
#define AMYG_LA_INPUT_MEMORY_WEIGHT     0.5f  /**< Memory contribution to LA input */
#define AMYG_BA_MEMORY_WEIGHT           0.3f  /**< Memory contribution to BA */
#define AMYG_BA_UNFAMILIAR_WEIGHT       0.2f  /**< Unfamiliar context contribution */
#define AMYG_US_LA_BOOST                0.5f  /**< US boost to LA activation */
#define AMYG_US_CEA_BOOST               0.7f  /**< US boost to CeA activation */

/* Emotional system synchronization */
#define AMYG_AROUSAL_FEAR_WEIGHT        0.7f  /**< Fear contribution to arousal */
#define AMYG_AROUSAL_ANXIETY_WEIGHT     0.5f  /**< Anxiety contribution to arousal */
#define AMYG_PFC_REGULATION_INCREMENT   0.1f  /**< PFC inhibition increment from regulation */
#define AMYG_EMOTIONAL_STABILITY_THRESH 0.7f  /**< Stability threshold for anxiety reduction */
#define AMYG_EMOTIONAL_ANXIETY_DECR     0.05f /**< Anxiety decrement from high stability */

/* Default timestep for stimulus processing */
#define AMYG_STIMULUS_DT_MS             10.0f /**< Default dt for stimulus processing */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Amygdala nucleus types
 *
 * WHAT: Different nuclei within the amygdala
 * WHY:  Each nucleus has distinct functions and connectivity
 */
typedef enum {
    AMYG_NUCLEUS_LATERAL = 0,     /**< LA: Sensory input, CS-US learning */
    AMYG_NUCLEUS_BASAL,           /**< BA: Hippocampal integration, context */
    AMYG_NUCLEUS_CENTRAL,         /**< CeA: Output to brainstem/hypothalamus */
    AMYG_NUCLEUS_MEDIAL,          /**< MeA: Olfactory, social processing */
    AMYG_NUCLEUS_ITC,             /**< Intercalated cells: Inhibitory control */
    AMYG_NUCLEUS_COUNT
} amyg_nucleus_type_t;

/**
 * @brief Fear conditioning phases
 *
 * WHAT: Stages of fear learning and extinction
 * WHY:  Different mechanisms operate in each phase
 */
typedef enum {
    AMYG_PHASE_NAIVE = 0,         /**< No prior conditioning */
    AMYG_PHASE_ACQUISITION,       /**< Learning CS-US association */
    AMYG_PHASE_CONSOLIDATION,     /**< Memory stabilization */
    AMYG_PHASE_EXPRESSION,        /**< Fear response to CS */
    AMYG_PHASE_EXTINCTION,        /**< Unlearning fear response */
    AMYG_PHASE_RENEWAL,           /**< Fear return in new context */
    AMYG_PHASE_SPONTANEOUS_RECOVERY /**< Fear return after time */
} amyg_conditioning_phase_t;

/**
 * @brief Emotion valence types
 *
 * WHAT: Positive vs negative emotional valence
 * WHY:  Amygdala processes both aversive and appetitive stimuli
 */
typedef enum {
    AMYG_VALENCE_NEGATIVE = -1,   /**< Aversive, threatening */
    AMYG_VALENCE_NEUTRAL = 0,     /**< Neither positive nor negative */
    AMYG_VALENCE_POSITIVE = 1     /**< Appetitive, rewarding */
} amyg_valence_t;

/**
 * @brief Threat level classification
 *
 * WHAT: Graded threat assessment
 * WHY:  Different threat levels trigger different responses
 */
typedef enum {
    AMYG_THREAT_NONE = 0,         /**< No threat detected */
    AMYG_THREAT_LOW,              /**< Mild threat, vigilance */
    AMYG_THREAT_MODERATE,         /**< Clear threat, caution */
    AMYG_THREAT_HIGH,             /**< Significant threat, avoidance */
    AMYG_THREAT_SEVERE            /**< Imminent danger, fight/flight */
} amyg_threat_level_t;

/**
 * @brief Fear output type
 *
 * WHAT: Different autonomic/behavioral fear responses
 * WHY:  CeA projects to different targets for different responses
 */
typedef enum {
    AMYG_OUTPUT_FREEZING = 0,     /**< Immobility response */
    AMYG_OUTPUT_STARTLE,          /**< Enhanced startle reflex */
    AMYG_OUTPUT_AUTONOMIC,        /**< Heart rate, blood pressure */
    AMYG_OUTPUT_HORMONAL,         /**< Cortisol, adrenaline release */
    AMYG_OUTPUT_ATTENTION,        /**< Attentional bias to threat */
    AMYG_OUTPUT_COUNT
} amyg_output_type_t;

/* ============================================================================
 * Core Data Structures
 * ============================================================================ */

/**
 * @brief Conditioned stimulus representation
 *
 * WHAT: Features that predict an unconditioned stimulus
 * WHY:  Enable pattern-based fear triggering
 */
typedef struct {
    float features[AMYG_MAX_CS_FEATURES];  /**< Stimulus feature vector */
    uint32_t n_features;                    /**< Number of active features */
    uint32_t modality;                      /**< Sensory modality (0=visual, 1=auditory, etc.) */
    float salience;                         /**< How salient/attention-grabbing */
} amyg_conditioned_stimulus_t;

/**
 * @brief Unconditioned stimulus representation
 *
 * WHAT: Inherently aversive or appetitive stimulus
 * WHY:  Drives learning of CS-US associations
 */
typedef struct {
    float intensity;              /**< US intensity [0-1] */
    amyg_valence_t valence;       /**< Aversive or appetitive */
    uint32_t type;                /**< US type (pain, loud noise, food, etc.) */
    float duration_ms;            /**< Duration of US */
} amyg_unconditioned_stimulus_t;

/**
 * @brief Context representation
 *
 * WHAT: Environmental/situational context
 * WHY:  Contextual fear depends on hippocampal input
 */
typedef struct {
    float context_vector[AMYG_MAX_CONTEXT_DIM];  /**< Context embedding */
    uint32_t context_id;                          /**< Unique context identifier */
    float familiarity;                            /**< How familiar [0-1] */
    bool is_safe;                                 /**< Explicitly safe context */
} amyg_context_t;

/**
 * @brief Fear memory structure
 *
 * WHAT: Stored CS-US association with context
 * WHY:  Enable pattern-matched fear retrieval
 */
typedef struct {
    uint32_t memory_id;                        /**< Unique memory identifier */
    amyg_conditioned_stimulus_t cs;            /**< Conditioned stimulus */
    amyg_unconditioned_stimulus_t us;          /**< Unconditioned stimulus */
    amyg_context_t acquisition_context;        /**< Where fear was learned */

    float association_strength;                /**< CS-US association [0-1] */
    float extinction_strength;                 /**< Extinction learning [0-1] */
    float generalization_width;                /**< How much it generalizes */

    uint64_t acquisition_time_ms;              /**< When learned */
    uint64_t last_retrieval_ms;                /**< Last activation time */
    uint32_t retrieval_count;                  /**< How often retrieved */

    amyg_conditioning_phase_t phase;           /**< Current phase */
    bool is_consolidated;                      /**< Whether memory is stable */
} amyg_fear_memory_t;

/**
 * @brief Individual amygdala nucleus
 *
 * WHAT: Single nucleus within the amygdala
 * WHY:  Enable nucleus-specific processing and connectivity
 */
typedef struct {
    amyg_nucleus_type_t type;     /**< Which nucleus */
    float activation;             /**< Current activation level [0-1] */
    float baseline;               /**< Baseline activity */
    float gain;                   /**< Activation gain/sensitivity */

    /* Input weights from other nuclei */
    float input_weights[AMYG_NUCLEUS_COUNT];

    /* Plasticity */
    float plasticity_rate;        /**< LTP/LTD rate */
    bool plasticity_enabled;      /**< Whether plastic */

    /* Neuromodulation */
    float dopamine_level;         /**< DA modulation */
    float norepinephrine_level;   /**< NE modulation (arousal) */
    float cortisol_level;         /**< Stress hormone level */
} amyg_nucleus_t;

/**
 * @brief Amygdala configuration
 *
 * WHAT: Parameters for amygdala behavior
 * WHY:  Enable tuning for different applications
 */
typedef struct {
    /* Fear conditioning parameters */
    float conditioning_rate;      /**< How fast fear is learned */
    float extinction_rate;        /**< How fast fear extinguishes */
    float reconsolidation_window_ms;  /**< Time window for reconsolidation */
    float spontaneous_recovery_rate;  /**< Rate of fear return */

    /* Threshold parameters */
    float fear_threshold;         /**< Threshold for fear response */
    float anxiety_threshold;      /**< Threshold for anxiety state */
    float threat_detection_threshold; /**< Sensitivity to threats */

    /* Decay parameters */
    float anxiety_decay_rate;     /**< How fast anxiety reduces */
    float activation_decay_rate;  /**< Nuclear activation decay */

    /* Generalization */
    float generalization_default; /**< Default generalization width */
    bool context_dependent;       /**< Require context match for retrieval */

    /* Prefrontal regulation */
    float prefrontal_inhibition_weight;  /**< PFC inhibition strength */
    bool extinction_enabled;      /**< Allow fear extinction */

    /* Bio-async */
    bool bio_async_enabled;       /**< Enable inter-module messaging */
    uint32_t bio_inbox_capacity;  /**< Message queue size */
} amyg_config_t;

/**
 * @brief Fear output response
 *
 * WHAT: Output signals for fear-related behaviors
 * WHY:  Drive downstream behavioral/autonomic responses
 */
typedef struct {
    float outputs[AMYG_OUTPUT_COUNT];  /**< Output strengths */
    amyg_threat_level_t threat_level;  /**< Overall threat assessment */
    float fear_intensity;              /**< Overall fear level [0-1] */
    float anxiety_level;               /**< Background anxiety [0-1] */

    /* Triggering memory if any */
    uint32_t triggering_memory_id;     /**< Which memory triggered response */
    float memory_match_score;          /**< How well stimulus matched */
} amyg_fear_response_t;

/**
 * @brief Main amygdala structure
 *
 * WHAT: Complete amygdala system
 * WHY:  Unified emotion/fear processing module
 */
typedef struct {
    /* Nuclei */
    amyg_nucleus_t nuclei[AMYG_NUCLEUS_COUNT];

    /* Fear memories */
    amyg_fear_memory_t* fear_memories;
    uint32_t fear_memory_count;
    uint32_t fear_memory_capacity;

    /* Current state */
    float current_fear_level;     /**< Instantaneous fear [0-1] */
    float current_anxiety_level;  /**< Background anxiety [0-1] */
    amyg_threat_level_t current_threat;
    amyg_valence_t current_valence;

    /* Current context (from hippocampus) */
    amyg_context_t current_context;
    bool context_valid;

    /* Prefrontal regulation state */
    float prefrontal_inhibition;  /**< Current PFC inhibition [0-1] */

    /* Configuration */
    amyg_config_t config;

    /* Current output */
    amyg_fear_response_t last_response;

    /* Timestamps */
    uint64_t current_time_ms;
    uint64_t last_update_ms;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;

    /* Emotional system integration */
    emotional_system_t* emotion_system;  /**< Connected emotional system */
    bool emotion_system_connected;       /**< Whether emotional system is connected */

    /* External connections (opaque pointers) */
    void* hippocampus;                   /**< Hippocampus for contextual fear */
    void* prefrontal;                    /**< PFC for emotion regulation */
    void* hypothalamus;                  /**< Hypothalamus for autonomic output */
    void* thalamus;                      /**< Thalamus for sensory relay */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Statistics */
    uint64_t total_fear_events;
    uint64_t total_extinction_events;
    uint64_t total_conditioning_events;
} amygdala_t;

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

/**
 * @brief Initialize default amygdala configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Provide starting point for customization
 *
 * @param config Configuration to initialize
 * @return 0 on success, error code on failure
 */
int amygdala_default_config(amyg_config_t* config);

/**
 * @brief Validate amygdala configuration
 *
 * WHAT: Check configuration parameters are valid
 * WHY:  Prevent runtime errors from bad config
 *
 * @param config Configuration to validate
 * @return 0 if valid, error code if invalid
 */
int amygdala_validate_config(const amyg_config_t* config);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create amygdala instance
 *
 * WHAT: Allocate and initialize amygdala
 * WHY:  Entry point for amygdala usage
 *
 * @param config Configuration (NULL for defaults)
 * @return Amygdala instance or NULL on failure
 */
amygdala_t* amygdala_create(const amyg_config_t* config);

/**
 * @brief Destroy amygdala instance
 *
 * WHAT: Free all amygdala resources
 * WHY:  Clean shutdown
 *
 * @param amyg Amygdala to destroy
 */
void amygdala_destroy(amygdala_t* amyg);

/**
 * @brief Reset amygdala to initial state
 *
 * WHAT: Clear memories and reset activations
 * WHY:  Restart without reallocation
 *
 * @param amyg Amygdala to reset
 * @return 0 on success, error code on failure
 */
int amygdala_reset(amygdala_t* amyg);

/* ============================================================================
 * Core Processing Functions
 * ============================================================================ */

/**
 * @brief Process sensory input through amygdala
 *
 * WHAT: Evaluate stimulus for threat/emotional content
 * WHY:  Main pathway for threat detection
 *
 * @param amyg Amygdala instance
 * @param cs Conditioned stimulus input
 * @param response Output fear response (can be NULL)
 * @return 0 on success, error code on failure
 */
int amygdala_process_stimulus(amygdala_t* amyg,
                              const amyg_conditioned_stimulus_t* cs,
                              amyg_fear_response_t* response);

/**
 * @brief Update amygdala state (one timestep)
 *
 * WHAT: Decay activations, update anxiety, process internal dynamics
 * WHY:  Maintain temporal dynamics
 *
 * @param amyg Amygdala instance
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, error code on failure
 */
int amygdala_step(amygdala_t* amyg, float dt_ms);

/**
 * @brief Get current fear response
 *
 * WHAT: Retrieve current output state
 * WHY:  Query amygdala output for downstream systems
 *
 * @param amyg Amygdala instance
 * @param response Output response structure
 * @return 0 on success, error code on failure
 */
int amygdala_get_response(const amygdala_t* amyg, amyg_fear_response_t* response);

/* ============================================================================
 * Fear Conditioning Functions
 * ============================================================================ */

/**
 * @brief Perform fear conditioning trial
 *
 * WHAT: Associate CS with US to create fear memory
 * WHY:  Core fear learning mechanism
 *
 * @param amyg Amygdala instance
 * @param cs Conditioned stimulus
 * @param us Unconditioned stimulus
 * @param memory_id Output: ID of created/updated memory (can be NULL)
 * @return 0 on success, error code on failure
 */
int amygdala_condition_fear(amygdala_t* amyg,
                            const amyg_conditioned_stimulus_t* cs,
                            const amyg_unconditioned_stimulus_t* us,
                            uint32_t* memory_id);

/**
 * @brief Perform extinction trial
 *
 * WHAT: Present CS without US to reduce fear
 * WHY:  Fear extinction learning
 *
 * @param amyg Amygdala instance
 * @param cs Conditioned stimulus (without US)
 * @return 0 on success, error code on failure
 */
int amygdala_extinction_trial(amygdala_t* amyg,
                              const amyg_conditioned_stimulus_t* cs);

/**
 * @brief Retrieve fear memory by CS similarity
 *
 * WHAT: Find matching fear memory for a stimulus
 * WHY:  Pattern-complete fear retrieval
 *
 * @param amyg Amygdala instance
 * @param cs Query stimulus
 * @param memory Output memory (can be NULL)
 * @param match_score Output similarity score (can be NULL)
 * @return 0 on success, error code on failure
 */
int amygdala_retrieve_fear_memory(amygdala_t* amyg,
                                  const amyg_conditioned_stimulus_t* cs,
                                  amyg_fear_memory_t* memory,
                                  float* match_score);

/**
 * @brief Add explicit fear memory
 *
 * WHAT: Directly insert a fear memory
 * WHY:  Allow external fear memory creation
 *
 * @param amyg Amygdala instance
 * @param memory Memory to add
 * @param memory_id Output: assigned memory ID (can be NULL)
 * @return 0 on success, error code on failure
 */
int amygdala_add_fear_memory(amygdala_t* amyg,
                             const amyg_fear_memory_t* memory,
                             uint32_t* memory_id);

/**
 * @brief Get fear memory by ID
 *
 * WHAT: Retrieve specific fear memory
 * WHY:  Direct access to stored memories
 *
 * @param amyg Amygdala instance
 * @param memory_id Memory to retrieve
 * @param memory Output memory structure
 * @return 0 on success, error code on failure
 */
int amygdala_get_fear_memory(const amygdala_t* amyg,
                             uint32_t memory_id,
                             amyg_fear_memory_t* memory);

/**
 * @brief Clear all fear memories
 *
 * WHAT: Remove all learned fears
 * WHY:  Complete amnesia for fear
 *
 * @param amyg Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_clear_fear_memories(amygdala_t* amyg);

/* ============================================================================
 * Context Functions
 * ============================================================================ */

/**
 * @brief Set current context (from hippocampus)
 *
 * WHAT: Update environmental context
 * WHY:  Enable contextual fear processing
 *
 * @param amyg Amygdala instance
 * @param context New context
 * @return 0 on success, error code on failure
 */
int amygdala_set_context(amygdala_t* amyg, const amyg_context_t* context);

/**
 * @brief Get current context
 *
 * WHAT: Query current context state
 * WHY:  Access current environmental context
 *
 * @param amyg Amygdala instance
 * @param context Output context
 * @return 0 on success, error code on failure
 */
int amygdala_get_context(const amygdala_t* amyg, amyg_context_t* context);

/**
 * @brief Check if context matches fear memory acquisition context
 *
 * WHAT: Compare current context to memory context
 * WHY:  Contextual fear gating
 *
 * @param amyg Amygdala instance
 * @param memory_id Fear memory to check
 * @param match_score Output similarity [0-1]
 * @return 0 on success, error code on failure
 */
int amygdala_context_match(const amygdala_t* amyg,
                           uint32_t memory_id,
                           float* match_score);

/* ============================================================================
 * Regulation Functions
 * ============================================================================ */

/**
 * @brief Set prefrontal inhibition level
 *
 * WHAT: Adjust top-down emotion regulation
 * WHY:  Model PFC control over amygdala
 *
 * @param amyg Amygdala instance
 * @param inhibition Inhibition level [0-1]
 * @return 0 on success, error code on failure
 */
int amygdala_set_prefrontal_inhibition(amygdala_t* amyg, float inhibition);

/**
 * @brief Set neuromodulator levels
 *
 * WHAT: Update dopamine, norepinephrine, cortisol
 * WHY:  Model stress/arousal effects on amygdala
 *
 * @param amyg Amygdala instance
 * @param dopamine DA level [0-1]
 * @param norepinephrine NE level [0-1]
 * @param cortisol Cortisol level [0-1]
 * @return 0 on success, error code on failure
 */
int amygdala_set_neuromodulators(amygdala_t* amyg,
                                 float dopamine,
                                 float norepinephrine,
                                 float cortisol);

/**
 * @brief Set anxiety level directly
 *
 * WHAT: Override current anxiety state
 * WHY:  External anxiety induction
 *
 * @param amyg Amygdala instance
 * @param anxiety Anxiety level [0-1]
 * @return 0 on success, error code on failure
 */
int amygdala_set_anxiety(amygdala_t* amyg, float anxiety);

/* ============================================================================
 * Nucleus Access Functions
 * ============================================================================ */

/**
 * @brief Get nucleus activation
 *
 * WHAT: Query specific nucleus activity
 * WHY:  Fine-grained monitoring
 *
 * @param amyg Amygdala instance
 * @param nucleus Which nucleus
 * @param activation Output activation level
 * @return 0 on success, error code on failure
 */
int amygdala_get_nucleus_activation(const amygdala_t* amyg,
                                    amyg_nucleus_type_t nucleus,
                                    float* activation);

/**
 * @brief Set nucleus activation
 *
 * WHAT: Directly set nucleus activity
 * WHY:  External input to specific nuclei
 *
 * @param amyg Amygdala instance
 * @param nucleus Which nucleus
 * @param activation Activation level [0-1]
 * @return 0 on success, error code on failure
 */
int amygdala_set_nucleus_activation(amygdala_t* amyg,
                                    amyg_nucleus_type_t nucleus,
                                    float activation);

/**
 * @brief Set nucleus plasticity enabled
 *
 * WHAT: Enable/disable learning in nucleus
 * WHY:  Control where plasticity occurs
 *
 * @param amyg Amygdala instance
 * @param nucleus Which nucleus
 * @param enabled Whether plasticity is enabled
 * @return 0 on success, error code on failure
 */
int amygdala_set_nucleus_plasticity(amygdala_t* amyg,
                                    amyg_nucleus_type_t nucleus,
                                    bool enabled);

/* ============================================================================
 * Integration Functions
 * ============================================================================ */

/**
 * @brief Connect to hippocampus for contextual processing
 *
 * WHAT: Establish hippocampus connection
 * WHY:  Enable contextual fear
 *
 * @param amyg Amygdala instance
 * @param hippocampus Hippocampus instance (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_connect_hippocampus(amygdala_t* amyg, void* hippocampus);

/**
 * @brief Connect to prefrontal cortex for regulation
 *
 * WHAT: Establish PFC connection
 * WHY:  Enable emotion regulation
 *
 * @param amyg Amygdala instance
 * @param prefrontal PFC instance (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_connect_prefrontal(amygdala_t* amyg, void* prefrontal);

/**
 * @brief Connect to hypothalamus for autonomic output
 *
 * WHAT: Establish hypothalamus connection
 * WHY:  Enable autonomic fear responses
 *
 * @param amyg Amygdala instance
 * @param hypothalamus Hypothalamus instance (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_connect_hypothalamus(amygdala_t* amyg, void* hypothalamus);

/**
 * @brief Connect to thalamus for sensory relay
 *
 * WHAT: Establish thalamus connection
 * WHY:  Receive sensory input
 *
 * @param amyg Amygdala instance
 * @param thalamus Thalamus instance (opaque pointer)
 * @return 0 on success, error code on failure
 */
int amygdala_connect_thalamus(amygdala_t* amyg, void* thalamus);

/**
 * @brief Connect to emotional system for bidirectional integration
 *
 * WHAT: Establish emotional system connection
 * WHY:  Amygdala drives fear/anxiety in emotional system; receives regulation signals
 * HOW:  Store pointer and enable bidirectional updates
 *
 * Integration behavior:
 * - Amygdala fear → Emotional system negative valence + high arousal
 * - Amygdala anxiety → Emotional system baseline arousal elevation
 * - Emotional system regulation → Amygdala prefrontal inhibition
 * - Emotional system decay → Synchronized with amygdala anxiety decay
 *
 * @param amyg Amygdala instance
 * @param emotion_system Emotional system instance
 * @return 0 on success, error code on failure
 */
int amygdala_connect_emotion_system(amygdala_t* amyg, emotional_system_t* emotion_system);

/**
 * @brief Disconnect from emotional system
 *
 * WHAT: Remove emotional system connection
 * WHY:  Clean shutdown or reconfiguration
 *
 * @param amyg Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_disconnect_emotion_system(amygdala_t* amyg);

/**
 * @brief Check if emotional system is connected
 *
 * WHAT: Query connection status
 * WHY:  Conditional integration behavior
 *
 * @param amyg Amygdala instance
 * @return true if connected
 */
bool amygdala_is_emotion_system_connected(const amygdala_t* amyg);

/**
 * @brief Synchronize amygdala state to emotional system
 *
 * WHAT: Push current fear/anxiety to emotional system
 * WHY:  Keep emotional system aware of amygdala state
 * HOW:  Convert fear → negative valence, anxiety → arousal
 *
 * @param amyg Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_sync_to_emotion_system(amygdala_t* amyg);

/**
 * @brief Synchronize emotional system state to amygdala
 *
 * WHAT: Pull regulation signals from emotional system
 * WHY:  Emotional regulation affects amygdala activity
 * HOW:  Read regulation state, apply as prefrontal inhibition
 *
 * @param amyg Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_sync_from_emotion_system(amygdala_t* amyg);

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with inter-module messaging
 * WHY:  Enable async communication
 *
 * @param amyg Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_connect_bio_async(amygdala_t* amyg);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from messaging
 * WHY:  Clean shutdown
 *
 * @param amyg Amygdala instance
 * @return 0 on success, error code on failure
 */
int amygdala_disconnect_bio_async(amygdala_t* amyg);

/**
 * @brief Check if bio-async connected
 *
 * WHAT: Query connection status
 * WHY:  Conditional messaging
 *
 * @param amyg Amygdala instance
 * @return true if connected
 */
bool amygdala_is_bio_async_connected(const amygdala_t* amyg);

/* ============================================================================
 * Statistics and Debug
 * ============================================================================ */

/**
 * @brief Get fear memory count
 *
 * WHAT: Query number of stored fear memories
 * WHY:  Monitoring and debugging
 *
 * @param amyg Amygdala instance
 * @return Number of fear memories
 */
uint32_t amygdala_get_fear_memory_count(const amygdala_t* amyg);

/**
 * @brief Get current fear level
 *
 * WHAT: Query instantaneous fear
 * WHY:  Monitor fear state
 *
 * @param amyg Amygdala instance
 * @return Current fear level [0-1]
 */
float amygdala_get_fear_level(const amygdala_t* amyg);

/**
 * @brief Set current fear level directly
 *
 * WHAT: Directly set fear level (bypassing nucleus activation)
 * WHY:  Needed by bridges that modulate fear without full circuit
 * HOW:  Directly updates current_fear_level field
 *
 * @param amyg Amygdala instance
 * @param fear Fear level [0-1], clamped
 * @return 0 on success, error code on failure
 */
int amygdala_set_fear_level(amygdala_t* amyg, float fear);

/**
 * @brief Get current anxiety level
 *
 * WHAT: Query background anxiety
 * WHY:  Monitor anxiety state
 *
 * @param amyg Amygdala instance
 * @return Current anxiety level [0-1]
 */
float amygdala_get_anxiety_level(const amygdala_t* amyg);

/**
 * @brief Get current threat level
 *
 * WHAT: Query threat assessment
 * WHY:  Monitor threat state
 *
 * @param amyg Amygdala instance
 * @return Current threat level
 */
amyg_threat_level_t amygdala_get_threat_level(const amygdala_t* amyg);

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve operational statistics
 * WHY:  Performance monitoring
 *
 * @param amyg Amygdala instance
 * @param fear_events Output: total fear events
 * @param extinction_events Output: total extinction events
 * @param conditioning_events Output: total conditioning events
 * @return 0 on success, error code on failure
 */
int amygdala_get_statistics(const amygdala_t* amyg,
                            uint64_t* fear_events,
                            uint64_t* extinction_events,
                            uint64_t* conditioning_events);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Compute stimulus similarity
 *
 * WHAT: Calculate cosine similarity between stimuli
 * WHY:  Fear generalization based on similarity
 *
 * @param cs1 First stimulus
 * @param cs2 Second stimulus
 * @return Similarity score [0-1]
 */
float amygdala_stimulus_similarity(const amyg_conditioned_stimulus_t* cs1,
                                   const amyg_conditioned_stimulus_t* cs2);

/**
 * @brief Compute context similarity
 *
 * WHAT: Calculate similarity between contexts
 * WHY:  Contextual fear gating
 *
 * @param ctx1 First context
 * @param ctx2 Second context
 * @return Similarity score [0-1]
 */
float amygdala_context_similarity(const amyg_context_t* ctx1,
                                  const amyg_context_t* ctx2);

/**
 * @brief Get nucleus name as string
 *
 * WHAT: Convert nucleus type to string
 * WHY:  Debugging and logging
 *
 * @param nucleus Nucleus type
 * @return String name
 */
const char* amygdala_nucleus_name(amyg_nucleus_type_t nucleus);

/**
 * @brief Get threat level name as string
 *
 * WHAT: Convert threat level to string
 * WHY:  Debugging and logging
 *
 * @param threat Threat level
 * @return String name
 */
const char* amygdala_threat_level_name(amyg_threat_level_t threat);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMYGDALA_H */

/**
 * @file nimcp_omni_wernicke_bridge.h
 * @brief Omnidirectional Inference to Wernicke's Area Bridge
 * @version 1.0.0
 * @date 2026-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with Wernicke's area
 * WHY:  Enable predictive language comprehension and semantic processing
 * HOW:  Bidirectional prediction-error flow with cross-modal speech integration
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE COMPREHENSION:
 * -------------------------
 * Wernicke's area (posterior STG, BA 22) as a predictive system:
 *
 *   1. PHONOLOGICAL PREDICTION (Forward):
 *      - Predict next phoneme based on context
 *      - Coarticulation expectations
 *      - Prosodic contour prediction
 *
 *   2. LEXICAL PREDICTION (Forward):
 *      - Predict next word based on syntax/semantics
 *      - Word frequency priors
 *      - Cohort narrowing predictions
 *
 *   3. SEMANTIC INFERENCE (Backward):
 *      - Infer meaning from word sequence
 *      - Context-dependent disambiguation
 *      - Thematic role assignment
 *
 *   4. CROSS-MODAL INTEGRATION (Lateral):
 *      - Audio-visual speech fusion
 *      - McGurk effect processing
 *      - Lip reading integration
 *
 * INFERENCE DIRECTION MAPPING:
 * ----------------------------
 *   Direction        Language Operation
 *   ─────────────────────────────────────────────
 *   Forward          Prediction: Context → Phoneme/Word expectation
 *   Backward         Inference: Sound → Phoneme → Word → Meaning
 *   Lateral          Wernicke ↔ Visual (lip reading, audiovisual)
 *   Hierarchical     Phoneme → Syllable → Word → Phrase → Meaning
 *
 * N400 PREDICTION ERROR:
 * ----------------------
 * - Semantic predictions from context
 * - Unexpected words generate N400-like PE
 * - PE magnitude indicates semantic surprise
 * - Drives belief update in world model
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_WERNICKE_BRIDGE_H
#define NIMCP_OMNI_WERNICKE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct omni_wernicke_bridge omni_wernicke_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct hopfield_memory hopfield_memory_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-wernicke bridge */
#define BIO_MODULE_OMNI_WERNICKE_BRIDGE            0x0E59

/** @brief Maximum phoneme prediction horizon */
#define OMNI_WERNICKE_MAX_PHONEME_HORIZON          5

/** @brief Maximum word prediction candidates */
#define OMNI_WERNICKE_MAX_WORD_CANDIDATES          10

/** @brief Default semantic prediction depth */
#define OMNI_WERNICKE_DEFAULT_SEMANTIC_DEPTH       3

/** @brief Maximum concepts for semantic prediction */
#define OMNI_WERNICKE_MAX_CONCEPTS                 64

/** @brief Default N400 threshold for semantic surprise */
#define OMNI_WERNICKE_N400_THRESHOLD               0.5f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Comprehension processing level
 */
typedef enum {
    OMNI_WERNICKE_LEVEL_PHONEME = 0,  /**< Phoneme level (lowest) */
    OMNI_WERNICKE_LEVEL_SYLLABLE,     /**< Syllable level */
    OMNI_WERNICKE_LEVEL_WORD,         /**< Word level */
    OMNI_WERNICKE_LEVEL_PHRASE,       /**< Phrase level */
    OMNI_WERNICKE_LEVEL_SENTENCE,     /**< Sentence level */
    OMNI_WERNICKE_LEVEL_DISCOURSE,    /**< Discourse level (highest) */
    OMNI_WERNICKE_LEVEL_COUNT         /**< Number of levels */
} omni_wernicke_level_t;

/**
 * @brief Inference mode
 */
typedef enum {
    OMNI_WERNICKE_MODE_LISTENING = 0, /**< Passive listening */
    OMNI_WERNICKE_MODE_COMPREHENDING, /**< Active comprehension */
    OMNI_WERNICKE_MODE_PREDICTING,    /**< Predictive mode */
    OMNI_WERNICKE_MODE_MONITORING,    /**< Self-monitoring (Broca feedback) */
    OMNI_WERNICKE_MODE_REHEARSING     /**< Working memory rehearsal */
} omni_wernicke_mode_t;

/**
 * @brief Cross-modal integration source
 */
typedef enum {
    OMNI_WERNICKE_AUDIO_ONLY = 0,     /**< Audio only */
    OMNI_WERNICKE_VISUAL_ONLY,        /**< Visual only (lip reading) */
    OMNI_WERNICKE_AUDIOVISUAL,        /**< Audio-visual fusion */
    OMNI_WERNICKE_MOTOR_FEEDBACK      /**< Motor prediction (Broca) */
} omni_wernicke_modality_t;

/**
 * @brief Semantic disambiguation strategy
 */
typedef enum {
    OMNI_WERNICKE_DISAMBIG_FREQ = 0,  /**< Frequency-based (most common) */
    OMNI_WERNICKE_DISAMBIG_CONTEXT,   /**< Context-based */
    OMNI_WERNICKE_DISAMBIG_BAYESIAN,  /**< Bayesian optimal */
    OMNI_WERNICKE_DISAMBIG_SPREADING  /**< Spreading activation */
} omni_wernicke_disambig_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Phoneme prediction state
 */
typedef struct {
    float* phoneme_probs;             /**< P(next phoneme) distribution */
    uint32_t num_phonemes;            /**< Number of phoneme categories */
    uint32_t predicted_phoneme;       /**< Most likely next phoneme */
    float confidence;                 /**< Prediction confidence */
    float coarticulation_bias;        /**< Coarticulation expectation */
} omni_phoneme_prediction_t;

/**
 * @brief Word prediction state
 */
typedef struct {
    char** word_candidates;           /**< Top word candidates */
    float* word_probs;                /**< P(word) for each candidate */
    uint32_t num_candidates;          /**< Number of candidates */
    uint32_t cohort_size;             /**< Current cohort size */
    float uniqueness_point;           /**< Uniqueness point reached [0-1] */
    bool recognition_complete;        /**< Word fully recognized */
} omni_word_prediction_t;

/**
 * @brief Semantic prediction state
 */
typedef struct {
    uint32_t* predicted_concepts;     /**< Predicted concept IDs */
    float* concept_activations;       /**< Activation levels */
    uint32_t num_concepts;            /**< Number of active concepts */
    float semantic_coherence;         /**< Semantic coherence score */
    float n400_magnitude;             /**< N400-like prediction error */
    bool semantic_violation;          /**< Semantic violation detected */
} omni_semantic_prediction_t;

/**
 * @brief Cross-modal prediction (audiovisual speech)
 */
typedef struct {
    float* audio_prediction;          /**< Predicted audio features */
    uint32_t audio_dim;               /**< Audio dimension */
    float* visual_prediction;         /**< Predicted visual features (lips) */
    uint32_t visual_dim;              /**< Visual dimension */
    float audiovisual_coherence;      /**< AV coherence [0-1] */
    bool mcgurk_conflict;             /**< McGurk-like conflict detected */
    float fusion_weight_audio;        /**< Weight for audio in fusion */
    float fusion_weight_visual;       /**< Weight for visual in fusion */
} omni_crossmodal_prediction_t;

/**
 * @brief Omni effects on Wernicke's area (top-down)
 */
typedef struct {
    /* Phonological level */
    omni_phoneme_prediction_t phoneme_pred;   /**< Phoneme predictions */
    float phoneme_precision;                   /**< Phoneme precision weight */

    /* Lexical level */
    omni_word_prediction_t word_pred;         /**< Word predictions */
    float word_precision;                      /**< Word precision weight */

    /* Semantic level */
    omni_semantic_prediction_t semantic_pred; /**< Semantic predictions */
    float semantic_precision;                  /**< Semantic precision weight */

    /* Cross-modal */
    omni_crossmodal_prediction_t crossmodal;  /**< Cross-modal predictions */

    /* Processing mode */
    omni_wernicke_mode_t mode;                /**< Current processing mode */
    omni_wernicke_level_t processing_level;   /**< Current processing level */
} omni_to_wernicke_effects_t;

/**
 * @brief Wernicke effects on omni (bottom-up prediction errors)
 */
typedef struct {
    /* Prediction errors per level */
    float phoneme_pe;                 /**< Phoneme prediction error */
    float syllable_pe;                /**< Syllable prediction error */
    float word_pe;                    /**< Word prediction error */
    float semantic_pe;                /**< Semantic prediction error (N400) */

    /* Aggregated metrics */
    float combined_pe;                /**< Combined comprehension PE */
    float free_energy;                /**< Language comprehension FE */
    float comprehension_confidence;   /**< Overall comprehension confidence */

    /* Event flags */
    bool phoneme_surprise;            /**< Unexpected phoneme */
    bool word_surprise;               /**< Unexpected word */
    bool semantic_anomaly;            /**< Semantic anomaly (N400) */
    bool garden_path;                 /**< Garden-path sentence detected */

    /* Recognized content */
    char* recognized_word;            /**< Most recently recognized word */
    uint32_t recognized_concept;      /**< Most recently activated concept */
} wernicke_to_omni_effects_t;

/**
 * @brief World model update from comprehension
 */
typedef struct {
    uint32_t* updated_concepts;       /**< Concepts to update in world model */
    float* concept_deltas;            /**< Belief changes per concept */
    uint32_t num_updates;             /**< Number of updates */
    float entropy_change;             /**< Change in belief entropy */
    bool significant_update;          /**< Significant world model change */
} omni_wernicke_world_update_t;

/**
 * @brief Knowledge graph synchronization
 */
typedef struct {
    uint32_t wernicke_kg_id;          /**< Wernicke node ID in KG */
    bool kg_registered;               /**< Registered with brain KG */
    bool omni_sync_active;            /**< Omni KG sync active */
    float comprehension_capability;   /**< Reported capability [0-1] */
    float prediction_capability;      /**< Reported capability [0-1] */
} omni_wernicke_kg_sync_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Prediction horizons */
    uint32_t phoneme_horizon;         /**< Phoneme prediction lookahead */
    uint32_t word_candidates;         /**< Max word candidates to track */
    uint32_t semantic_depth;          /**< Semantic prediction depth */

    /* Precision defaults */
    float default_phoneme_precision;  /**< Default phoneme precision */
    float default_word_precision;     /**< Default word precision */
    float default_semantic_precision; /**< Default semantic precision */

    /* Thresholds */
    float pe_threshold;               /**< PE threshold for update */
    float n400_threshold;             /**< N400 surprise threshold */
    float recognition_threshold;      /**< Word recognition threshold */

    /* Cross-modal */
    bool enable_audiovisual;          /**< Enable AV integration */
    float av_coherence_threshold;     /**< AV coherence threshold */
    omni_wernicke_disambig_t disambig_strategy; /**< Disambiguation strategy */

    /* Broca integration */
    bool enable_broca_feedback;       /**< Enable Broca efference copy */
    float broca_feedback_weight;      /**< Broca feedback weight */

    /* KG integration */
    bool enable_kg_sync;              /**< Enable knowledge graph sync */
    bool enable_world_model;          /**< Enable world model updates */

    /* Bio-async */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    bool enable_logging;              /**< Enable logging */
} omni_wernicke_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;           /**< Total bridge updates */
    uint64_t phoneme_predictions;     /**< Phoneme predictions made */
    uint64_t word_predictions;        /**< Word predictions made */
    uint64_t semantic_predictions;    /**< Semantic predictions made */
    uint64_t words_recognized;        /**< Words successfully recognized */
    uint64_t semantic_anomalies;      /**< N400-like anomalies detected */
    uint64_t garden_paths;            /**< Garden-path reanalyses */
    uint64_t av_conflicts;            /**< Audiovisual conflicts */
    float avg_phoneme_pe;             /**< Average phoneme PE */
    float avg_word_pe;                /**< Average word PE */
    float avg_semantic_pe;            /**< Average semantic PE (N400) */
    float avg_free_energy;            /**< Average comprehension FE */
    float avg_comprehension;          /**< Average comprehension confidence */
} omni_wernicke_stats_t;

/**
 * @brief Omni-Wernicke bridge structure
 */
struct omni_wernicke_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge */

    omni_wernicke_config_t config;    /**< Configuration */

    /* Connected systems (using void* for flexibility) */
    void* jepa;                       /**< Bidirectional JEPA */
    void* pred_hier;                  /**< Predictive hierarchy */
    void* hopfield;                   /**< Associative memory */
    void* wernicke;                   /**< Wernicke adapter */
    void* broca_bridge;               /**< Wernicke-Broca bridge */
    void* audiovisual;                /**< Audiovisual bridge */
    void* semantic_memory;            /**< Semantic memory system */
    void* brain_kg;                   /**< Brain knowledge graph */

    /* Computed effects */
    omni_to_wernicke_effects_t omni_effects;      /**< Omni → Wernicke */
    wernicke_to_omni_effects_t wernicke_effects;  /**< Wernicke → Omni */

    /* World model updates */
    omni_wernicke_world_update_t world_update;    /**< World model updates */

    /* KG synchronization */
    omni_wernicke_kg_sync_t kg_sync;              /**< KG sync state */

    /* Internal prediction state */
    float* phoneme_history;           /**< Recent phoneme observations */
    uint32_t phoneme_history_len;     /**< Length of phoneme history */
    float* word_context;              /**< Current word context embedding */
    uint32_t context_dim;             /**< Context embedding dimension */

    /* Statistics */
    omni_wernicke_stats_t stats;

    /* Bio-async integration */
    void* bio_context;                /**< Bio-async module context */
    bool bio_async_connected;         /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int omni_wernicke_default_config(omni_wernicke_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni-wernicke bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
omni_wernicke_bridge_t* omni_wernicke_bridge_create(
    const omni_wernicke_config_t* config);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy
 */
void omni_wernicke_bridge_destroy(omni_wernicke_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bidirectional JEPA
 */
int omni_wernicke_connect_jepa(omni_wernicke_bridge_t* bridge,
                                void* jepa);

/**
 * @brief Connect predictive hierarchy
 */
int omni_wernicke_connect_pred_hier(omni_wernicke_bridge_t* bridge,
                                     void* pred_hier);

/**
 * @brief Connect Hopfield memory
 */
int omni_wernicke_connect_hopfield(omni_wernicke_bridge_t* bridge,
                                    void* hopfield);

/**
 * @brief Connect Wernicke adapter
 */
int omni_wernicke_connect_wernicke(omni_wernicke_bridge_t* bridge,
                                    void* wernicke);

/**
 * @brief Connect Wernicke-Broca bridge
 */
int omni_wernicke_connect_broca_bridge(omni_wernicke_bridge_t* bridge,
                                        void* broca_bridge);

/**
 * @brief Connect audiovisual bridge
 */
int omni_wernicke_connect_audiovisual(omni_wernicke_bridge_t* bridge,
                                       void* audiovisual);

/**
 * @brief Connect semantic memory
 */
int omni_wernicke_connect_semantic(omni_wernicke_bridge_t* bridge,
                                    void* semantic_memory);

/**
 * @brief Connect brain knowledge graph
 */
int omni_wernicke_connect_kg(omni_wernicke_bridge_t* bridge,
                              void* brain_kg);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge (full cycle)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int omni_wernicke_update(omni_wernicke_bridge_t* bridge);

/**
 * @brief Apply omni effects to Wernicke (top-down predictions)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int omni_wernicke_apply_to_wernicke(omni_wernicke_bridge_t* bridge);

/**
 * @brief Apply Wernicke effects to omni (bottom-up PEs)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int omni_wernicke_apply_to_omni(omni_wernicke_bridge_t* bridge);

/* ============================================================================
 * Forward Prediction API (Top-Down)
 * ============================================================================ */

/**
 * @brief Predict next phoneme
 *
 * @param bridge Bridge handle
 * @param prediction Output phoneme prediction
 * @return 0 on success, -1 on error
 */
int omni_wernicke_predict_phoneme(omni_wernicke_bridge_t* bridge,
                                   omni_phoneme_prediction_t* prediction);

/**
 * @brief Predict next word (or narrow cohort)
 *
 * @param bridge Bridge handle
 * @param prediction Output word prediction
 * @return 0 on success, -1 on error
 */
int omni_wernicke_predict_word(omni_wernicke_bridge_t* bridge,
                                omni_word_prediction_t* prediction);

/**
 * @brief Predict semantic content
 *
 * @param bridge Bridge handle
 * @param prediction Output semantic prediction
 * @return 0 on success, -1 on error
 */
int omni_wernicke_predict_semantic(omni_wernicke_bridge_t* bridge,
                                    omni_semantic_prediction_t* prediction);

/**
 * @brief Generate cross-modal prediction (audiovisual)
 *
 * @param bridge Bridge handle
 * @param prediction Output cross-modal prediction
 * @return 0 on success, -1 on error
 */
int omni_wernicke_predict_crossmodal(omni_wernicke_bridge_t* bridge,
                                      omni_crossmodal_prediction_t* prediction);

/* ============================================================================
 * Backward Inference API (Bottom-Up)
 * ============================================================================ */

/**
 * @brief Process observed phoneme and compute PE
 *
 * @param bridge Bridge handle
 * @param phoneme_id Observed phoneme ID
 * @param features Phoneme features (optional)
 * @param feature_dim Feature dimension
 * @return Phoneme prediction error
 */
float omni_wernicke_observe_phoneme(omni_wernicke_bridge_t* bridge,
                                     uint32_t phoneme_id,
                                     const float* features,
                                     uint32_t feature_dim);

/**
 * @brief Process recognized word and compute PE
 *
 * @param bridge Bridge handle
 * @param word Recognized word
 * @return Word prediction error
 */
float omni_wernicke_observe_word(omni_wernicke_bridge_t* bridge,
                                  const char* word);

/**
 * @brief Process semantic activation and compute N400
 *
 * @param bridge Bridge handle
 * @param concept_id Activated concept ID
 * @param activation Activation strength
 * @return Semantic prediction error (N400 magnitude)
 */
float omni_wernicke_observe_semantic(omni_wernicke_bridge_t* bridge,
                                      uint32_t concept_id,
                                      float activation);

/**
 * @brief Infer meaning from current context (backward inference)
 *
 * @param bridge Bridge handle
 * @param concepts Output inferred concepts
 * @param activations Output activation levels
 * @param max_concepts Maximum concepts to return
 * @param num_concepts Output actual count
 * @return 0 on success, -1 on error
 */
int omni_wernicke_infer_meaning(omni_wernicke_bridge_t* bridge,
                                 uint32_t* concepts,
                                 float* activations,
                                 uint32_t max_concepts,
                                 uint32_t* num_concepts);

/* ============================================================================
 * Cross-Modal API
 * ============================================================================ */

/**
 * @brief Process audiovisual input
 *
 * @param bridge Bridge handle
 * @param audio_features Audio features
 * @param audio_dim Audio dimension
 * @param visual_features Visual features (lip shapes)
 * @param visual_dim Visual dimension
 * @return Audiovisual coherence [0-1]
 */
float omni_wernicke_process_audiovisual(omni_wernicke_bridge_t* bridge,
                                         const float* audio_features,
                                         uint32_t audio_dim,
                                         const float* visual_features,
                                         uint32_t visual_dim);

/**
 * @brief Check for McGurk-like conflict
 *
 * @param bridge Bridge handle
 * @return true if audiovisual conflict detected
 */
bool omni_wernicke_has_mcgurk_conflict(const omni_wernicke_bridge_t* bridge);

/**
 * @brief Get fused audiovisual percept
 *
 * @param bridge Bridge handle
 * @param fused_phoneme Output fused phoneme ID
 * @param confidence Output fusion confidence
 * @return 0 on success, -1 on error
 */
int omni_wernicke_get_fused_percept(const omni_wernicke_bridge_t* bridge,
                                     uint32_t* fused_phoneme,
                                     float* confidence);

/* ============================================================================
 * Precision API
 * ============================================================================ */

/**
 * @brief Set precision for processing level
 *
 * @param bridge Bridge handle
 * @param level Processing level
 * @param precision Precision weight [0-1]
 * @return 0 on success, -1 on error
 */
int omni_wernicke_set_precision(omni_wernicke_bridge_t* bridge,
                                 omni_wernicke_level_t level,
                                 float precision);

/**
 * @brief Get precision for processing level
 *
 * @param bridge Bridge handle
 * @param level Processing level
 * @return Precision weight
 */
float omni_wernicke_get_precision(const omni_wernicke_bridge_t* bridge,
                                   omni_wernicke_level_t level);

/**
 * @brief Update precision based on context and reliability
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int omni_wernicke_update_precision(omni_wernicke_bridge_t* bridge);

/* ============================================================================
 * World Model API
 * ============================================================================ */

/**
 * @brief Get pending world model updates
 *
 * @param bridge Bridge handle
 * @param update Output world update
 * @return 0 on success, -1 on error
 */
int omni_wernicke_get_world_update(const omni_wernicke_bridge_t* bridge,
                                    omni_wernicke_world_update_t* update);

/**
 * @brief Apply world model updates
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int omni_wernicke_apply_world_update(omni_wernicke_bridge_t* bridge);

/**
 * @brief Clear pending world model updates
 *
 * @param bridge Bridge handle
 */
void omni_wernicke_clear_world_update(omni_wernicke_bridge_t* bridge);

/* ============================================================================
 * Knowledge Graph API
 * ============================================================================ */

/**
 * @brief Register with brain knowledge graph
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int omni_wernicke_kg_register(omni_wernicke_bridge_t* bridge);

/**
 * @brief Synchronize with omni KG
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int omni_wernicke_kg_sync(omni_wernicke_bridge_t* bridge);

/**
 * @brief Get KG sync state
 *
 * @param bridge Bridge handle
 * @param sync Output sync state
 * @return 0 on success, -1 on error
 */
int omni_wernicke_get_kg_sync(const omni_wernicke_bridge_t* bridge,
                               omni_wernicke_kg_sync_t* sync);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get omni effects on Wernicke
 */
int omni_wernicke_get_omni_effects(const omni_wernicke_bridge_t* bridge,
                                    omni_to_wernicke_effects_t* effects);

/**
 * @brief Get Wernicke effects on omni
 */
int omni_wernicke_get_wernicke_effects(const omni_wernicke_bridge_t* bridge,
                                        wernicke_to_omni_effects_t* effects);

/**
 * @brief Get bridge statistics
 */
int omni_wernicke_get_stats(const omni_wernicke_bridge_t* bridge,
                             omni_wernicke_stats_t* stats);

/**
 * @brief Reset statistics
 */
int omni_wernicke_reset_stats(omni_wernicke_bridge_t* bridge);

/**
 * @brief Get current processing mode
 */
omni_wernicke_mode_t omni_wernicke_get_mode(const omni_wernicke_bridge_t* bridge);

/**
 * @brief Set processing mode
 */
int omni_wernicke_set_mode(omni_wernicke_bridge_t* bridge,
                            omni_wernicke_mode_t mode);

/**
 * @brief Get current processing level
 */
omni_wernicke_level_t omni_wernicke_get_level(const omni_wernicke_bridge_t* bridge);

/**
 * @brief Get current comprehension confidence
 */
float omni_wernicke_get_comprehension(const omni_wernicke_bridge_t* bridge);

/**
 * @brief Get current free energy
 */
float omni_wernicke_get_free_energy(const omni_wernicke_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_wernicke_connect_bio_async(omni_wernicke_bridge_t* bridge);
int omni_wernicke_disconnect_bio_async(omni_wernicke_bridge_t* bridge);
bool omni_wernicke_is_bio_async_connected(const omni_wernicke_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_wernicke_level_to_string(omni_wernicke_level_t level);
const char* omni_wernicke_mode_to_string(omni_wernicke_mode_t mode);
const char* omni_wernicke_modality_to_string(omni_wernicke_modality_t modality);
const char* omni_wernicke_disambig_to_string(omni_wernicke_disambig_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WERNICKE_BRIDGE_H */

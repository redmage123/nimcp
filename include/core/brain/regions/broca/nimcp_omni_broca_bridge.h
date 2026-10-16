/**
 * @file nimcp_omni_broca_bridge.h
 * @brief Omnidirectional Inference to Broca's Region Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with Broca's area
 * WHY:  Enable predictive language production and syntactic processing
 * HOW:  Forward inference guides production, backward inference enables parsing
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE LANGUAGE PRODUCTION:
 * -------------------------------
 * Broca's area (BA44/45) as a predictive system:
 *
 *   1. SYNTACTIC PREDICTION (BA45):
 *      - Predict next syntactic category
 *      - "After determiner, expect noun or adjective"
 *      - Hierarchical phrase structure predictions
 *
 *   2. MOTOR PREDICTION (BA44):
 *      - Predict articulatory gestures
 *      - Coarticulation planning
 *      - Forward model for speech production
 *
 *   3. PHONOLOGICAL PREDICTION:
 *      - Phoneme sequence prediction
 *      - Prosodic contour planning
 *      - Working memory for phonological buffer
 *
 * INFERENCE DIRECTION MAPPING:
 * ----------------------------
 *   Direction        Language Operation
 *   ─────────────────────────────────────────────
 *   Forward          Production: Intent → Syntax → Phonology → Motor
 *   Backward         Parsing: Motor → Phonology → Syntax → Semantics
 *   Lateral          Wernicke's ↔ Broca's (arcuate fasciculus)
 *   Hierarchical     Phrase structure (embedded clauses)
 *
 * WERNICKE-BROCA LOOP:
 * --------------------
 * - Broca predicts phonological output
 * - Wernicke monitors auditory feedback
 * - Prediction error drives correction
 * - "Efference copy" mechanism
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_BROCA_BRIDGE_H
#define NIMCP_OMNI_BROCA_BRIDGE_H

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

typedef struct omni_broca_bridge omni_broca_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct broca_adapter broca_adapter_t;
typedef struct speech_cortex speech_cortex_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-broca bridge */
#define BIO_MODULE_OMNI_BROCA_BRIDGE               0x0E58

/** @brief Maximum phonological working memory slots (7±2) */
#define OMNI_BROCA_MAX_WM_SLOTS                    9

/** @brief Default syntactic prediction depth */
#define OMNI_BROCA_DEFAULT_SYNTAX_DEPTH            3

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Broca's area subregion
 */
typedef enum {
    OMNI_BROCA_BA44 = 0,         /**< Pars opercularis (motor) */
    OMNI_BROCA_BA45              /**< Pars triangularis (syntax) */
} omni_broca_area_t;

/**
 * @brief Language processing mode
 */
typedef enum {
    OMNI_LANG_PRODUCTION = 0,    /**< Speech production (forward) */
    OMNI_LANG_PARSING,           /**< Speech parsing (backward) */
    OMNI_LANG_REPETITION,        /**< Repetition (bidirectional) */
    OMNI_LANG_MONITORING         /**< Self-monitoring (feedback) */
} omni_language_mode_t;

/**
 * @brief Syntactic category (simplified)
 */
typedef enum {
    OMNI_SYN_NOUN = 0,           /**< Noun */
    OMNI_SYN_VERB,               /**< Verb */
    OMNI_SYN_ADJ,                /**< Adjective */
    OMNI_SYN_ADV,                /**< Adverb */
    OMNI_SYN_DET,                /**< Determiner */
    OMNI_SYN_PREP,               /**< Preposition */
    OMNI_SYN_CONJ,               /**< Conjunction */
    OMNI_SYN_PRON,               /**< Pronoun */
    OMNI_SYN_OTHER               /**< Other */
} omni_syntactic_category_t;

/**
 * @brief Motor command type
 */
typedef enum {
    OMNI_MOTOR_LIPS = 0,         /**< Lip movement */
    OMNI_MOTOR_TONGUE,           /**< Tongue movement */
    OMNI_MOTOR_JAW,              /**< Jaw movement */
    OMNI_MOTOR_LARYNX,           /**< Laryngeal (voicing) */
    OMNI_MOTOR_VELUM             /**< Velum (nasality) */
} omni_motor_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Syntactic prediction state
 */
typedef struct {
    omni_syntactic_category_t predicted_category; /**< Predicted next category */
    float* category_probs;           /**< Probability distribution */
    uint32_t num_categories;         /**< Number of categories */
    float confidence;                /**< Prediction confidence */
    uint32_t phrase_depth;           /**< Current phrase nesting depth */
} omni_syntactic_prediction_t;

/**
 * @brief Motor prediction state
 */
typedef struct {
    float* motor_commands;           /**< Predicted motor commands */
    uint32_t num_commands;           /**< Number of motor commands */
    float* coarticulation;           /**< Coarticulation context */
    uint32_t coart_window;           /**< Coarticulation window */
    float execution_confidence;      /**< Motor execution confidence */
} omni_motor_prediction_t;

/**
 * @brief Phonological working memory
 */
typedef struct {
    float** slots;                   /**< WM slots (phoneme embeddings) */
    uint32_t slot_dim;               /**< Embedding dimension */
    uint32_t num_slots;              /**< Number of slots */
    uint32_t head;                   /**< Current head position */
    float decay_rate;                /**< Memory decay rate */
} omni_phon_wm_t;

/**
 * @brief Omni effects on Broca's area
 */
typedef struct {
    omni_syntactic_prediction_t syntax_pred;  /**< Syntactic prediction */
    omni_motor_prediction_t motor_pred;       /**< Motor prediction */
    float* phoneme_prediction;       /**< Predicted phoneme sequence */
    uint32_t phoneme_dim;            /**< Phoneme dimension */
    omni_language_mode_t mode;       /**< Current language mode */
    float precision;                 /**< Overall precision */
} omni_to_broca_effects_t;

/**
 * @brief Broca's area effects on omni
 */
typedef struct {
    float* syntax_pe;                /**< Syntactic prediction error */
    uint32_t syntax_dim;             /**< Syntax PE dimension */
    float* motor_pe;                 /**< Motor prediction error */
    uint32_t motor_dim;              /**< Motor PE dimension */
    float* phoneme_pe;               /**< Phoneme prediction error */
    uint32_t phoneme_dim;            /**< Phoneme PE dimension */
    float combined_pe;               /**< Combined language PE */
    float free_energy;               /**< Language free energy */
    bool production_error;           /**< Production error detected */
    bool syntax_violation;           /**< Syntax violation detected */
} broca_to_omni_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Syntax prediction */
    uint32_t syntax_prediction_depth;    /**< Lookahead depth */
    float syntax_pe_threshold;           /**< Syntax PE threshold */
    bool enable_hierarchical_syntax;     /**< Hierarchical phrases */

    /* Motor prediction */
    uint32_t coarticulation_window;      /**< Coarticulation span */
    float motor_pe_threshold;            /**< Motor PE threshold */
    bool enable_forward_model;           /**< Motor forward model */

    /* Working memory */
    uint32_t wm_capacity;                /**< WM slot count (default 7) */
    float wm_decay_rate;                 /**< WM decay rate */
    bool enable_rehearsal;               /**< Enable rehearsal loop */

    /* Wernicke integration */
    bool enable_wernicke_loop;           /**< Wernicke feedback loop */
    float auditory_feedback_weight;      /**< Feedback weight */

    /* Integration */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    bool enable_logging;                 /**< Enable logging */
} omni_broca_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;              /**< Total bridge updates */
    uint64_t production_events;          /**< Production events */
    uint64_t parsing_events;             /**< Parsing events */
    uint64_t syntax_violations;          /**< Syntax violation count */
    uint64_t motor_errors;               /**< Motor error count */
    float avg_syntax_pe;                 /**< Average syntax PE */
    float avg_motor_pe;                  /**< Average motor PE */
    float avg_wm_load;                   /**< Average WM load */
    float avg_free_energy;               /**< Average language FE */
} omni_broca_stats_t;

/**
 * @brief Omni-Broca bridge structure
 */
struct omni_broca_bridge {
    bridge_base_t base;                  /**< MUST be first: base bridge */

    omni_broca_config_t config;          /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;          /**< Bidirectional JEPA */
    predictive_hierarchy_t* pred_hier;   /**< Predictive hierarchy */
    hopfield_memory_t* hopfield;         /**< Associative memory */
    broca_adapter_t* broca;              /**< Broca adapter */
    speech_cortex_t* speech_cortex;      /**< Speech cortex */

    /* Working memory */
    omni_phon_wm_t* phon_wm;             /**< Phonological WM */

    /* Computed effects */
    omni_to_broca_effects_t omni_effects;  /**< Omni → Broca */
    broca_to_omni_effects_t broca_effects; /**< Broca → omni */

    /* Statistics */
    omni_broca_stats_t stats;

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_broca_default_config(omni_broca_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_broca_bridge_t* omni_broca_bridge_create(
    const omni_broca_config_t* config);

void omni_broca_bridge_destroy(omni_broca_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_broca_connect_jepa(omni_broca_bridge_t* bridge,
                             jepa_bidirectional_t* jepa);

int omni_broca_connect_pred_hier(omni_broca_bridge_t* bridge,
                                  predictive_hierarchy_t* pred_hier);

int omni_broca_connect_hopfield(omni_broca_bridge_t* bridge,
                                 hopfield_memory_t* hopfield);

int omni_broca_connect_broca(omni_broca_bridge_t* bridge,
                              broca_adapter_t* broca);

int omni_broca_connect_speech_cortex(omni_broca_bridge_t* bridge,
                                      speech_cortex_t* speech);

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_broca_update(omni_broca_bridge_t* bridge);

int omni_broca_apply_to_broca(omni_broca_bridge_t* bridge);

int omni_broca_apply_to_omni(omni_broca_bridge_t* bridge);

/* ============================================================================
 * Production API (Forward Inference)
 * ============================================================================ */

/**
 * @brief Begin utterance production
 */
int omni_broca_begin_production(omni_broca_bridge_t* bridge,
                                 const float* semantic_repr,
                                 uint32_t repr_dim);

/**
 * @brief Predict next syntactic category
 */
int omni_broca_predict_syntax(omni_broca_bridge_t* bridge,
                               omni_syntactic_prediction_t* prediction);

/**
 * @brief Predict motor commands for phoneme
 */
int omni_broca_predict_motor(omni_broca_bridge_t* bridge,
                              const float* phoneme,
                              uint32_t phoneme_dim,
                              omni_motor_prediction_t* prediction);

/**
 * @brief Get next production output
 */
int omni_broca_get_production_output(omni_broca_bridge_t* bridge,
                                      float* motor_output,
                                      uint32_t* output_dim);

/* ============================================================================
 * Parsing API (Backward Inference)
 * ============================================================================ */

/**
 * @brief Parse incoming phonemes
 */
int omni_broca_parse_phonemes(omni_broca_bridge_t* bridge,
                               const float* phonemes,
                               uint32_t phoneme_dim,
                               uint32_t num_phonemes);

/**
 * @brief Infer syntactic structure
 */
int omni_broca_infer_syntax(omni_broca_bridge_t* bridge,
                             omni_syntactic_category_t* categories,
                             uint32_t* num_categories);

/**
 * @brief Check for syntax violations
 */
bool omni_broca_has_syntax_violation(const omni_broca_bridge_t* bridge);

/* ============================================================================
 * Working Memory API
 * ============================================================================ */

/**
 * @brief Push to phonological WM
 */
int omni_broca_wm_push(omni_broca_bridge_t* bridge,
                        const float* phoneme,
                        uint32_t dim);

/**
 * @brief Pop from phonological WM
 */
int omni_broca_wm_pop(omni_broca_bridge_t* bridge,
                       float* phoneme,
                       uint32_t* dim);

/**
 * @brief Get WM load (0-1)
 */
float omni_broca_wm_get_load(const omni_broca_bridge_t* bridge);

/**
 * @brief Rehearse WM contents
 */
int omni_broca_wm_rehearse(omni_broca_bridge_t* bridge);

/**
 * @brief Clear WM
 */
int omni_broca_wm_clear(omni_broca_bridge_t* bridge);

/* ============================================================================
 * Wernicke Loop API
 * ============================================================================ */

/**
 * @brief Send efference copy to Wernicke
 */
int omni_broca_send_efference_copy(omni_broca_bridge_t* bridge,
                                    const float* motor_commands,
                                    uint32_t num_commands);

/**
 * @brief Receive auditory feedback from Wernicke
 */
int omni_broca_receive_feedback(omni_broca_bridge_t* bridge,
                                 const float* auditory_feedback,
                                 uint32_t feedback_dim);

/**
 * @brief Compute production-feedback mismatch
 */
float omni_broca_compute_feedback_pe(const omni_broca_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_broca_get_omni_effects(const omni_broca_bridge_t* bridge,
                                 omni_to_broca_effects_t* effects);

int omni_broca_get_broca_effects(const omni_broca_bridge_t* bridge,
                                  broca_to_omni_effects_t* effects);

int omni_broca_get_stats(const omni_broca_bridge_t* bridge,
                          omni_broca_stats_t* stats);

int omni_broca_reset_stats(omni_broca_bridge_t* bridge);

/**
 * @brief Get current language mode
 */
omni_language_mode_t omni_broca_get_mode(const omni_broca_bridge_t* bridge);

/**
 * @brief Set language mode
 */
int omni_broca_set_mode(omni_broca_bridge_t* bridge,
                         omni_language_mode_t mode);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_broca_connect_bio_async(omni_broca_bridge_t* bridge);
int omni_broca_disconnect_bio_async(omni_broca_bridge_t* bridge);
bool omni_broca_is_bio_async_connected(const omni_broca_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_broca_area_to_string(omni_broca_area_t area);
const char* omni_broca_mode_to_string(omni_language_mode_t mode);
const char* omni_broca_syntax_to_string(omni_syntactic_category_t cat);
const char* omni_broca_motor_to_string(omni_motor_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_BROCA_BRIDGE_H */

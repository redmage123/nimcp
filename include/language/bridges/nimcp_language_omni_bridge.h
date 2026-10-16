//=============================================================================
// nimcp_language_omni_bridge.h - Language-Omni Inference Bridge Integration
//=============================================================================
/**
 * @file nimcp_language_omni_bridge.h
 * @brief Bridge integrating Language Layer with Omnidirectional Inference
 *
 * WHAT: Bridge connecting language processing with predictive processing systems
 * WHY:  Enable prediction-driven language processing (predictive coding, JEPA)
 * HOW:  Generate predictions, compute prediction errors, precision-weight updates
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding in language: Predict upcoming words/phonemes
 * - N400 as semantic prediction error
 * - P600 as syntactic prediction error
 * - Precision-weighting: Confidence-scaled prediction influence
 * - Bidirectional inference: Top-down (predictions) + bottom-up (input)
 *
 * KEY CONNECTIONS:
 * - JEPA Bidirectional: Joint embedding predictive architecture
 * - Predictive Hierarchy: Hierarchical predictive coding
 * - Hopfield Memory: Associative memory for predictions
 * - FEP Orchestrator: Free energy principle integration
 *
 * DATA FLOW:
 * - Omni → Language: Predictions, precision weights
 * - Language → Omni: Prediction errors, confidence signals
 *
 * @version 1.0.0 - Phase L4: Advanced Language Bridges
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_LANGUAGE_OMNI_BRIDGE_H
#define NIMCP_LANGUAGE_OMNI_BRIDGE_H

#include "language/nimcp_language_types.h"
#include "language/nimcp_language_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct language_omni_bridge language_omni_bridge_t;
typedef struct language_orchestrator language_orchestrator_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct fep_orchestrator fep_orchestrator_t;

/* bio_router_t is a pointer type defined in bio_router.h */
#ifndef NIMCP_BIO_ROUTER_H
typedef void* bio_router_t;
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identification */
#define LANGUAGE_OMNI_MODULE_NAME      "language_omni_bridge"
#define LANGUAGE_OMNI_MODULE_VERSION   "1.0.0"

/** Default configuration values */
#define LANGUAGE_OMNI_DEFAULT_UPDATE_INTERVAL_MS    20
#define LANGUAGE_OMNI_DEFAULT_PHONEME_HORIZON       5
#define LANGUAGE_OMNI_DEFAULT_WORD_HORIZON          3
#define LANGUAGE_OMNI_DEFAULT_PRECISION             0.8f
#define LANGUAGE_OMNI_DEFAULT_PRECISION_LR          0.01f

/** Prediction error thresholds */
#define LANGUAGE_OMNI_PE_PHONEME_THRESHOLD          0.3f
#define LANGUAGE_OMNI_PE_WORD_THRESHOLD             0.4f
#define LANGUAGE_OMNI_PE_SEMANTIC_THRESHOLD         0.5f

/** Maximum predictions */
#define LANGUAGE_OMNI_MAX_PHONEME_PREDICTIONS       32
#define LANGUAGE_OMNI_MAX_WORD_PREDICTIONS          16
#define LANGUAGE_OMNI_MAX_SEMANTIC_PREDICTIONS      8

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Prediction level types
 */
typedef enum {
    PREDICTION_LEVEL_PHONEME = 0,     /**< Phoneme-level predictions */
    PREDICTION_LEVEL_WORD,            /**< Word-level predictions */
    PREDICTION_LEVEL_PHRASE,          /**< Phrase-level predictions */
    PREDICTION_LEVEL_SEMANTIC,        /**< Semantic-level predictions */
    PREDICTION_LEVEL_DISCOURSE,       /**< Discourse-level predictions */
    PREDICTION_LEVEL_COUNT
} prediction_level_t;

/**
 * @brief Prediction error types (ERP-like)
 */
typedef enum {
    PE_TYPE_MISMATCH = 0,             /**< General mismatch */
    PE_TYPE_N400,                     /**< Semantic prediction error */
    PE_TYPE_P600,                     /**< Syntactic prediction error */
    PE_TYPE_MMN,                      /**< Phoneme mismatch negativity */
    PE_TYPE_COUNT
} prediction_error_type_t;

/**
 * @brief Inference direction
 */
typedef enum {
    LANG_INFER_DIR_BOTTOM_UP = 0,     /**< Input-driven (bottom-up) */
    LANG_INFER_DIR_TOP_DOWN,          /**< Prediction-driven (top-down) */
    LANG_INFER_DIR_BIDIRECTIONAL,     /**< Both directions */
    LANG_INFER_DIR_COUNT
} language_inference_direction_t;

/**
 * @brief Precision modulation source
 */
typedef enum {
    PRECISION_SOURCE_ATTENTION = 0,   /**< From attention system */
    PRECISION_SOURCE_CONTEXT,         /**< From context certainty */
    PRECISION_SOURCE_NOISE,           /**< From input quality */
    PRECISION_SOURCE_LEARNING,        /**< From learning rate */
    PRECISION_SOURCE_COUNT
} precision_source_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Single prediction
 */
typedef struct {
    prediction_level_t level;         /**< Prediction level */
    uint32_t predicted_id;            /**< Predicted element ID */
    float probability;                /**< Prediction probability [0-1] */
    float precision;                  /**< Precision weight [0-1] */
    uint32_t horizon_position;        /**< How far ahead */
    uint64_t generation_time_ms;      /**< When generated */
    bool confirmed;                   /**< Prediction confirmed */
    bool violated;                    /**< Prediction violated */
} language_prediction_t;

/**
 * @brief Prediction error signal
 */
typedef struct {
    prediction_level_t level;         /**< Error level */
    prediction_error_type_t type;     /**< Error type (N400, P600, etc.) */
    uint32_t predicted_id;            /**< What was predicted */
    uint32_t actual_id;               /**< What was observed */
    float error_magnitude;            /**< Error magnitude [0-1] */
    float precision;                  /**< Precision weight of this error */
    uint32_t position;                /**< Position in sequence */
    uint64_t timestamp_ms;            /**< Error detection time */
} language_prediction_error_t;

/**
 * @brief Phoneme-level prediction state
 */
typedef struct {
    /* Predictions */
    language_prediction_t* predictions;  /**< Current predictions */
    uint32_t num_predictions;            /**< Number of predictions */
    uint32_t max_predictions;            /**< Maximum capacity */
    uint32_t horizon;                    /**< Prediction horizon */

    /* Precision */
    float precision_phoneme;             /**< Current phoneme precision */

    /* Performance */
    float hit_rate;                      /**< Prediction accuracy */
    uint64_t total_predictions;          /**< Total predictions made */
    uint64_t confirmed_predictions;      /**< Correct predictions */
} phoneme_prediction_state_t;

/**
 * @brief Word-level prediction state
 */
typedef struct {
    /* Predictions */
    language_prediction_t* predictions;  /**< Current predictions */
    uint32_t num_predictions;            /**< Number of predictions */
    uint32_t max_predictions;            /**< Maximum capacity */
    uint32_t horizon;                    /**< Prediction horizon */

    /* Precision */
    float precision_word;                /**< Current word precision */

    /* Contextual predictions */
    float* context_vector;               /**< Context for predictions */
    uint32_t context_dim;                /**< Context dimension */

    /* Performance */
    float hit_rate;                      /**< Prediction accuracy */
    uint64_t total_predictions;          /**< Total predictions */
    uint64_t confirmed_predictions;      /**< Correct predictions */
} word_prediction_state_t;

/**
 * @brief Semantic prediction state
 */
typedef struct {
    /* Predictions */
    language_prediction_t* predictions;  /**< Semantic predictions */
    uint32_t num_predictions;            /**< Number of predictions */
    uint32_t max_predictions;            /**< Maximum capacity */

    /* Semantic embedding */
    float* predicted_semantic;           /**< Predicted semantic vector */
    uint32_t semantic_dim;               /**< Semantic dimension */

    /* Precision */
    float precision_semantic;            /**< Semantic precision */

    /* N400-like */
    float semantic_surprise;             /**< Current semantic surprise */
    float n400_amplitude;                /**< N400 response amplitude */
} semantic_prediction_state_t;

/**
 * @brief Prediction error queue
 */
typedef struct {
    language_prediction_error_t* errors; /**< Error queue */
    uint32_t num_errors;                 /**< Current error count */
    uint32_t max_errors;                 /**< Queue capacity */

    /* Aggregate statistics */
    float avg_phoneme_error;             /**< Average phoneme PE */
    float avg_word_error;                /**< Average word PE */
    float avg_semantic_error;            /**< Average semantic PE */
    float total_free_energy;             /**< Total prediction error (FE) */
} prediction_error_queue_t;

/**
 * @brief Precision state
 */
typedef struct {
    /* Per-level precision */
    float precision_phoneme;             /**< Phoneme-level precision */
    float precision_word;                /**< Word-level precision */
    float precision_semantic;            /**< Semantic precision */
    float precision_syntactic;           /**< Syntactic precision */

    /* Precision learning */
    float precision_lr;                  /**< Precision learning rate */
    bool precision_modulation_enabled;   /**< Allow precision updates */

    /* Source contributions */
    float attention_contribution;        /**< Attention → precision */
    float context_contribution;          /**< Context → precision */
    float quality_contribution;          /**< Input quality → precision */
} precision_state_t;

/**
 * @brief JEPA connection state
 */
typedef struct {
    /* Connection */
    bool connected;                      /**< JEPA connected */
    jepa_bidirectional_t* jepa;          /**< JEPA instance */

    /* Embedding */
    float* current_embedding;            /**< Current JEPA embedding */
    float* predicted_embedding;          /**< Predicted next embedding */
    uint32_t embedding_dim;              /**< Embedding dimension */

    /* JEPA prediction error */
    float jepa_prediction_error;         /**< JEPA PE magnitude */
} jepa_connection_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Prediction counts */
    uint64_t phoneme_predictions;        /**< Total phoneme predictions */
    uint64_t word_predictions;           /**< Total word predictions */
    uint64_t semantic_predictions;       /**< Total semantic predictions */

    /* Accuracy */
    float phoneme_accuracy;              /**< Phoneme prediction accuracy */
    float word_accuracy;                 /**< Word prediction accuracy */
    float semantic_accuracy;             /**< Semantic prediction accuracy */

    /* Prediction errors */
    uint64_t n400_events;                /**< N400 error events */
    uint64_t p600_events;                /**< P600 error events */
    float avg_prediction_error;          /**< Average PE magnitude */

    /* Free energy */
    float current_free_energy;           /**< Current free energy */
    float avg_free_energy;               /**< Average free energy */

    /* Performance */
    float avg_processing_time_ms;        /**< Average processing time */
    uint64_t last_update_time_ms;        /**< Last update timestamp */
} language_omni_stats_t;

//=============================================================================
// Bridge State Structure
//=============================================================================

/**
 * @brief Language-omni bridge state
 */
struct language_omni_bridge {
    /* Configuration */
    language_omni_config_t config;       /**< Bridge configuration */
    bool initialized;                    /**< Initialization state */
    bool active;                         /**< Active processing */

    /* Connected components */
    language_orchestrator_t* orchestrator;       /**< Parent orchestrator */
    jepa_bidirectional_t* jepa;                  /**< JEPA system */
    predictive_hierarchy_t* pred_hierarchy;      /**< Predictive hierarchy */
    hopfield_memory_t* hopfield;                 /**< Hopfield memory */
    fep_orchestrator_t* fep;                     /**< FEP orchestrator */

    /* Prediction states */
    phoneme_prediction_state_t phoneme_pred;     /**< Phoneme predictions */
    word_prediction_state_t word_pred;           /**< Word predictions */
    semantic_prediction_state_t semantic_pred;   /**< Semantic predictions */

    /* Prediction errors */
    prediction_error_queue_t error_queue;        /**< Prediction errors */

    /* Precision */
    precision_state_t precision;                 /**< Precision state */

    /* JEPA connection */
    jepa_connection_state_t jepa_state;          /**< JEPA state */

    /* Inference mode */
    language_inference_direction_t inference_mode; /**< Current inference mode */

    /* Statistics */
    language_omni_stats_t stats;                 /**< Bridge statistics */

    /* Bio-async */
    bio_router_t* bio_router;                    /**< Bio-async router */
    bool bio_async_registered;                   /**< Registration status */
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create language-omni bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on error
 */
language_omni_bridge_t* language_omni_bridge_create(
    const language_omni_config_t* config
);

/**
 * @brief Destroy language-omni bridge
 *
 * @param bridge Bridge instance
 */
void language_omni_bridge_destroy(language_omni_bridge_t* bridge);

/**
 * @brief Initialize bridge with default configuration
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_init(language_omni_bridge_t* bridge);

/**
 * @brief Start bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_start(language_omni_bridge_t* bridge);

/**
 * @brief Stop bridge processing
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_stop(language_omni_bridge_t* bridge);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect to language orchestrator
 *
 * @param bridge Bridge instance
 * @param orchestrator Language orchestrator
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_connect_orchestrator(
    language_omni_bridge_t* bridge,
    language_orchestrator_t* orchestrator
);

/**
 * @brief Connect to JEPA bidirectional
 *
 * @param bridge Bridge instance
 * @param jepa JEPA instance
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_connect_jepa(
    language_omni_bridge_t* bridge,
    jepa_bidirectional_t* jepa
);

/**
 * @brief Connect to predictive hierarchy
 *
 * @param bridge Bridge instance
 * @param pred_hierarchy Predictive hierarchy
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_connect_predictive_hierarchy(
    language_omni_bridge_t* bridge,
    predictive_hierarchy_t* pred_hierarchy
);

/**
 * @brief Connect to Hopfield memory
 *
 * @param bridge Bridge instance
 * @param hopfield Hopfield memory
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_connect_hopfield(
    language_omni_bridge_t* bridge,
    hopfield_memory_t* hopfield
);

/**
 * @brief Connect to FEP orchestrator
 *
 * @param bridge Bridge instance
 * @param fep FEP orchestrator
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_connect_fep(
    language_omni_bridge_t* bridge,
    fep_orchestrator_t* fep
);

//=============================================================================
// Prediction API
//=============================================================================

/**
 * @brief Generate phoneme predictions
 *
 * @param bridge Bridge instance
 * @param context_phonemes Recent phoneme sequence
 * @param context_length Number of context phonemes
 * @return Number of predictions generated, or -1 on error
 */
int language_omni_bridge_predict_phonemes(
    language_omni_bridge_t* bridge,
    const uint32_t* context_phonemes,
    uint32_t context_length
);

/**
 * @brief Generate word predictions
 *
 * @param bridge Bridge instance
 * @param context_words Recent word sequence
 * @param context_length Number of context words
 * @return Number of predictions generated, or -1 on error
 */
int language_omni_bridge_predict_words(
    language_omni_bridge_t* bridge,
    const uint32_t* context_words,
    uint32_t context_length
);

/**
 * @brief Generate semantic predictions
 *
 * @param bridge Bridge instance
 * @param context_vector Current semantic context
 * @param context_dim Context dimension
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_predict_semantic(
    language_omni_bridge_t* bridge,
    const float* context_vector,
    uint32_t context_dim
);

/**
 * @brief Get current predictions for level
 *
 * @param bridge Bridge instance
 * @param level Prediction level
 * @param predictions Output predictions
 * @param max_predictions Maximum to retrieve
 * @return Number of predictions, or -1 on error
 */
int language_omni_bridge_get_predictions(
    const language_omni_bridge_t* bridge,
    prediction_level_t level,
    language_prediction_t* predictions,
    uint32_t max_predictions
);

//=============================================================================
// Prediction Error API
//=============================================================================

/**
 * @brief Compute prediction error for observation
 *
 * @param bridge Bridge instance
 * @param level Prediction level
 * @param observed_id Observed element ID
 * @return Prediction error structure
 */
language_prediction_error_t language_omni_bridge_compute_error(
    language_omni_bridge_t* bridge,
    prediction_level_t level,
    uint32_t observed_id
);

/**
 * @brief Report prediction error
 *
 * @param bridge Bridge instance
 * @param error Prediction error
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_report_error(
    language_omni_bridge_t* bridge,
    const language_prediction_error_t* error
);

/**
 * @brief Get pending prediction errors
 *
 * @param bridge Bridge instance
 * @param errors Output error array
 * @param max_errors Maximum to retrieve
 * @return Number of errors, or -1 on error
 */
int language_omni_bridge_get_errors(
    language_omni_bridge_t* bridge,
    language_prediction_error_t* errors,
    uint32_t max_errors
);

/**
 * @brief Get current free energy (total prediction error)
 *
 * @param bridge Bridge instance
 * @return Free energy value
 */
float language_omni_bridge_get_free_energy(
    const language_omni_bridge_t* bridge
);

//=============================================================================
// Precision API
//=============================================================================

/**
 * @brief Set precision for level
 *
 * @param bridge Bridge instance
 * @param level Prediction level
 * @param precision Precision value [0-1]
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_set_precision(
    language_omni_bridge_t* bridge,
    prediction_level_t level,
    float precision
);

/**
 * @brief Get precision for level
 *
 * @param bridge Bridge instance
 * @param level Prediction level
 * @return Precision value [0-1]
 */
float language_omni_bridge_get_precision(
    const language_omni_bridge_t* bridge,
    prediction_level_t level
);

/**
 * @brief Update precision based on prediction errors
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_update_precision(
    language_omni_bridge_t* bridge
);

/**
 * @brief Set precision learning rate
 *
 * @param bridge Bridge instance
 * @param lr Learning rate
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_set_precision_lr(
    language_omni_bridge_t* bridge,
    float lr
);

//=============================================================================
// Inference Mode API
//=============================================================================

/**
 * @brief Set inference direction
 *
 * @param bridge Bridge instance
 * @param direction Inference direction
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_set_inference_mode(
    language_omni_bridge_t* bridge,
    language_inference_direction_t direction
);

/**
 * @brief Get current inference direction
 *
 * @param bridge Bridge instance
 * @return Current inference direction
 */
language_inference_direction_t language_omni_bridge_get_inference_mode(
    const language_omni_bridge_t* bridge
);

//=============================================================================
// Update and Query API
//=============================================================================

/**
 * @brief Update bridge (call each frame/cycle)
 *
 * @param bridge Bridge instance
 * @param current_time_ms Current time in milliseconds
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_update(
    language_omni_bridge_t* bridge,
    uint64_t current_time_ms
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_get_stats(
    const language_omni_bridge_t* bridge,
    language_omni_stats_t* stats
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_bio_async_register(
    language_omni_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int language_omni_bridge_bio_async_unregister(
    language_omni_bridge_t* bridge
);

//=============================================================================
// String Conversion Utilities
//=============================================================================

const char* prediction_level_to_string(prediction_level_t level);
const char* prediction_error_type_to_string(prediction_error_type_t type);
const char* language_inference_direction_to_string(language_inference_direction_t direction);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LANGUAGE_OMNI_BRIDGE_H */

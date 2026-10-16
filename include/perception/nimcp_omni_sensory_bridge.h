/**
 * @file nimcp_omni_sensory_bridge.h
 * @brief Omnidirectional Inference to Sensory Cortex Bridge
 * @version 1.0.0
 * @date 2025-01-04
 *
 * WHAT: Bridge integrating omnidirectional inference with sensory cortices
 * WHY:  Enable predictive perception across audio, visual, and speech modalities
 * HOW:  Bidirectional prediction-error flow with cross-modal integration
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * PREDICTIVE PERCEPTION:
 * ----------------------
 * Sensory processing as active inference:
 *
 *   1. TOP-DOWN PREDICTIONS (BACKWARD INFERENCE):
 *      - High-level expectations → Sensory predictions
 *      - "I expect to hear a dog bark" → Auditory template activation
 *      - "I expect to see a face" → Visual feature priming
 *
 *   2. BOTTOM-UP PREDICTION ERRORS (FORWARD INFERENCE):
 *      - Sensory input → Prediction error computation
 *      - Prediction errors propagate up hierarchy
 *      - Only surprising information ascends
 *
 *   3. CROSS-MODAL INFERENCE (LATERAL):
 *      - Audio predicts visual (audiovisual integration)
 *      - Visual predicts speech (lip reading)
 *      - Speech predicts audio (phoneme expectations)
 *
 * SENSORY HIERARCHY MAPPING:
 * --------------------------
 *   Hierarchy Level    Audio          Visual         Speech
 *   ─────────────────────────────────────────────────────────
 *   Low (L1)           Cochlea        V1 edges       Phonemes
 *   Mid (L2)           A1 features    V2 contours    Syllables
 *   High (L3)          Auditory obj   V4 objects     Words
 *   Abstract (L4)      Sound meaning  Scene parsing  Semantics
 *
 * PRECISION WEIGHTING BY MODALITY:
 * --------------------------------
 * - Context-dependent precision (e.g., dark room → audio precision ↑)
 * - Attention modulates per-modality precision
 * - Cross-modal priors sharpen predictions
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_OMNI_SENSORY_BRIDGE_H
#define NIMCP_OMNI_SENSORY_BRIDGE_H

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

typedef struct omni_sensory_bridge omni_sensory_bridge_t;
typedef struct jepa_bidirectional jepa_bidirectional_t;
typedef struct predictive_hierarchy predictive_hierarchy_t;
typedef struct hopfield_memory hopfield_memory_t;
typedef struct audio_cortex audio_cortex_t;
typedef struct visual_cortex_struct visual_cortex_t;
typedef struct speech_cortex speech_cortex_t;
typedef struct omni_precision_ctx omni_precision_ctx_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for omni-sensory bridge */
#define BIO_MODULE_OMNI_SENSORY_BRIDGE         0x0E55

/** @brief Maximum number of modalities */
#define OMNI_SENSORY_MAX_MODALITIES            3

/** @brief Default prediction error threshold */
#define OMNI_SENSORY_PE_THRESHOLD              1.0f

/** @brief Default cross-modal integration weight */
#define OMNI_SENSORY_CROSSMODAL_WEIGHT         0.3f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Sensory modality type
 */
typedef enum {
    OMNI_MODALITY_AUDIO = 0,     /**< Auditory modality */
    OMNI_MODALITY_VISUAL,        /**< Visual modality */
    OMNI_MODALITY_SPEECH,        /**< Speech modality */
    OMNI_MODALITY_COUNT          /**< Number of modalities */
} omni_modality_t;

/**
 * @brief Cross-modal integration mode
 */
typedef enum {
    OMNI_CROSSMODAL_NONE = 0,    /**< No cross-modal integration */
    OMNI_CROSSMODAL_AUDIO_VISUAL,/**< Audio-visual binding */
    OMNI_CROSSMODAL_VISUAL_SPEECH,/**< Visual-speech (lip reading) */
    OMNI_CROSSMODAL_AUDIO_SPEECH,/**< Audio-speech integration */
    OMNI_CROSSMODAL_ALL          /**< Full multimodal integration */
} omni_crossmodal_mode_t;

/**
 * @brief Sensory prediction direction
 */
typedef enum {
    OMNI_SENS_BOTTOM_UP = 0,     /**< Bottom-up (forward) */
    OMNI_SENS_TOP_DOWN,          /**< Top-down (backward) */
    OMNI_SENS_LATERAL,           /**< Cross-modal (lateral) */
    OMNI_SENS_BIDIRECTIONAL      /**< Both directions */
} omni_sensory_direction_t;

/**
 * @brief Attention mode for sensory processing
 */
typedef enum {
    OMNI_SENS_ATTN_NONE = 0,     /**< No attention modulation */
    OMNI_SENS_ATTN_SPATIAL,      /**< Spatial attention (visual) */
    OMNI_SENS_ATTN_TEMPORAL,     /**< Temporal attention (audio) */
    OMNI_SENS_ATTN_FEATURE,      /**< Feature-based attention */
    OMNI_SENS_ATTN_OBJECT        /**< Object-based attention */
} omni_sensory_attention_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Per-modality state
 */
typedef struct {
    float precision;             /**< Current precision weight */
    float prediction_error;      /**< Current PE magnitude */
    float* prediction;           /**< Top-down prediction */
    float* features;             /**< Bottom-up features */
    uint32_t dim;                /**< Feature dimension */
    bool active;                 /**< Modality currently active */
} omni_modality_state_t;

/**
 * @brief Omni effects on sensory cortices
 */
typedef struct {
    float* top_down_audio;       /**< Top-down audio prediction */
    float* top_down_visual;      /**< Top-down visual prediction */
    float* top_down_speech;      /**< Top-down speech prediction */
    uint32_t audio_dim;          /**< Audio prediction dimension */
    uint32_t visual_dim;         /**< Visual prediction dimension */
    uint32_t speech_dim;         /**< Speech prediction dimension */
    float precision_audio;       /**< Audio precision weight */
    float precision_visual;      /**< Visual precision weight */
    float precision_speech;      /**< Speech precision weight */
    omni_sensory_attention_t attention_mode; /**< Current attention */
} omni_to_sensory_effects_t;

/**
 * @brief Sensory cortices effects on omni
 */
typedef struct {
    float* audio_features;       /**< Bottom-up audio features */
    float* visual_features;      /**< Bottom-up visual features */
    float* speech_features;      /**< Bottom-up speech features */
    uint32_t audio_dim;          /**< Audio feature dimension */
    uint32_t visual_dim;         /**< Visual feature dimension */
    uint32_t speech_dim;         /**< Speech feature dimension */
    float audio_pe;              /**< Audio prediction error */
    float visual_pe;             /**< Visual prediction error */
    float speech_pe;             /**< Speech prediction error */
    float combined_free_energy;  /**< Total sensory free energy */
    bool audio_novelty;          /**< Audio novelty detected */
    bool visual_novelty;         /**< Visual novelty detected */
    bool speech_novelty;         /**< Speech novelty detected */
} sensory_to_omni_effects_t;

/**
 * @brief Cross-modal binding result
 */
typedef struct {
    float binding_strength;      /**< Strength of cross-modal bind [0-1] */
    float coherence;             /**< Cross-modal coherence */
    bool audiovisual_bound;      /**< Audio-visual binding active */
    bool visualspeech_bound;     /**< Visual-speech binding active */
    bool audiospeech_bound;      /**< Audio-speech binding active */
    float* integrated_repr;      /**< Integrated multimodal repr */
    uint32_t repr_dim;           /**< Integrated repr dimension */
} omni_crossmodal_binding_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Precision control */
    float default_audio_precision;   /**< Default audio precision */
    float default_visual_precision;  /**< Default visual precision */
    float default_speech_precision;  /**< Default speech precision */

    /* PE thresholds */
    float pe_threshold;              /**< PE threshold for update */
    float novelty_threshold;         /**< Novelty detection threshold */

    /* Cross-modal integration */
    omni_crossmodal_mode_t crossmodal_mode; /**< Cross-modal mode */
    float crossmodal_weight;         /**< Cross-modal integration weight */
    bool enable_binding;             /**< Enable cross-modal binding */

    /* Attention */
    bool enable_attention;           /**< Enable attention modulation */
    float attention_gain;            /**< Attention gain factor */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    bool enable_logging;             /**< Enable logging */
} omni_sensory_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;          /**< Total bridge updates */
    uint64_t audio_predictions;      /**< Audio top-down predictions */
    uint64_t visual_predictions;     /**< Visual top-down predictions */
    uint64_t speech_predictions;     /**< Speech top-down predictions */
    uint64_t crossmodal_bindings;    /**< Cross-modal binding events */
    uint64_t novelty_events;         /**< Novelty detection events */
    float avg_audio_pe;              /**< Average audio PE */
    float avg_visual_pe;             /**< Average visual PE */
    float avg_speech_pe;             /**< Average speech PE */
    float avg_free_energy;           /**< Average sensory free energy */
} omni_sensory_stats_t;

/**
 * @brief Omni-sensory bridge structure
 */
struct omni_sensory_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge */

    omni_sensory_config_t config;    /**< Configuration */

    /* Connected systems */
    jepa_bidirectional_t* jepa;      /**< Bidirectional JEPA */
    predictive_hierarchy_t* pred_hier; /**< Predictive hierarchy */
    hopfield_memory_t* hopfield;     /**< Associative memory */
    audio_cortex_t* audio_cortex;    /**< Audio cortex */
    visual_cortex_t* visual_cortex;  /**< Visual cortex */
    speech_cortex_t* speech_cortex;  /**< Speech cortex */

    /* Per-modality state */
    omni_modality_state_t modality_states[OMNI_MODALITY_COUNT];

    /* Computed effects */
    omni_to_sensory_effects_t omni_effects;     /**< Omni → sensory */
    sensory_to_omni_effects_t sensory_effects;  /**< Sensory → omni */
    omni_crossmodal_binding_t binding;          /**< Cross-modal binding */

    /* Statistics */
    omni_sensory_stats_t stats;

    /* Precision weighting (Phase 6) */
    omni_precision_ctx_t* precision_ctx; /**< Precision context */

    /* Bio-async integration */
    void* bio_context;               /**< Bio-async module context */
    bool bio_async_connected;        /**< Bio-async connection state */

    /* Thread safety */
    void* mutex;
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int omni_sensory_default_config(omni_sensory_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create omni-sensory bridge
 */
omni_sensory_bridge_t* omni_sensory_bridge_create(
    const omni_sensory_config_t* config);

/**
 * @brief Destroy bridge
 */
void omni_sensory_bridge_destroy(omni_sensory_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_sensory_connect_jepa(omni_sensory_bridge_t* bridge,
                               jepa_bidirectional_t* jepa);

int omni_sensory_connect_pred_hier(omni_sensory_bridge_t* bridge,
                                    predictive_hierarchy_t* pred_hier);

int omni_sensory_connect_hopfield(omni_sensory_bridge_t* bridge,
                                   hopfield_memory_t* hopfield);

int omni_sensory_connect_audio(omni_sensory_bridge_t* bridge,
                                audio_cortex_t* audio);

int omni_sensory_connect_visual(omni_sensory_bridge_t* bridge,
                                 visual_cortex_t* visual);

int omni_sensory_connect_speech(omni_sensory_bridge_t* bridge,
                                 speech_cortex_t* speech);

int omni_sensory_connect_precision(omni_sensory_bridge_t* bridge,
                                    omni_precision_ctx_t* precision_ctx);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge
 */
int omni_sensory_update(omni_sensory_bridge_t* bridge);

/**
 * @brief Apply omni effects to sensory cortices
 */
int omni_sensory_apply_to_sensory(omni_sensory_bridge_t* bridge);

/**
 * @brief Apply sensory effects to omni
 */
int omni_sensory_apply_to_omni(omni_sensory_bridge_t* bridge);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Generate top-down prediction for modality
 *
 * @param bridge Bridge
 * @param modality Target modality
 * @param prediction Output prediction [dim]
 * @param dim Prediction dimension
 * @return NIMCP_SUCCESS on success
 */
int omni_sensory_predict_modality(omni_sensory_bridge_t* bridge,
                                   omni_modality_t modality,
                                   float* prediction,
                                   uint32_t dim);

/**
 * @brief Compute prediction error for modality
 *
 * @param bridge Bridge
 * @param modality Target modality
 * @param observation Observed sensory input [dim]
 * @param dim Observation dimension
 * @param pe Output prediction error
 * @return NIMCP_SUCCESS on success
 */
int omni_sensory_compute_pe(omni_sensory_bridge_t* bridge,
                             omni_modality_t modality,
                             const float* observation,
                             uint32_t dim,
                             float* pe);

/**
 * @brief Generate cross-modal prediction
 *
 * @param bridge Bridge
 * @param source_modality Source modality
 * @param target_modality Target modality
 * @param prediction Output cross-modal prediction [dim]
 * @param dim Prediction dimension
 * @return NIMCP_SUCCESS on success
 */
int omni_sensory_crossmodal_predict(omni_sensory_bridge_t* bridge,
                                     omni_modality_t source_modality,
                                     omni_modality_t target_modality,
                                     float* prediction,
                                     uint32_t dim);

/* ============================================================================
 * Binding API
 * ============================================================================ */

/**
 * @brief Compute cross-modal binding
 *
 * @param bridge Bridge
 * @param binding Output binding result
 * @return NIMCP_SUCCESS on success
 */
int omni_sensory_compute_binding(omni_sensory_bridge_t* bridge,
                                  omni_crossmodal_binding_t* binding);

/**
 * @brief Check if modalities are bound
 */
bool omni_sensory_are_bound(const omni_sensory_bridge_t* bridge,
                             omni_modality_t m1,
                             omni_modality_t m2);

/**
 * @brief Get binding strength between modalities
 */
float omni_sensory_get_binding_strength(const omni_sensory_bridge_t* bridge,
                                         omni_modality_t m1,
                                         omni_modality_t m2);

/* ============================================================================
 * Precision API
 * ============================================================================ */

/**
 * @brief Set precision for modality
 */
int omni_sensory_set_precision(omni_sensory_bridge_t* bridge,
                                omni_modality_t modality,
                                float precision);

/**
 * @brief Get precision for modality
 */
float omni_sensory_get_precision(const omni_sensory_bridge_t* bridge,
                                  omni_modality_t modality);

/**
 * @brief Update precision based on context
 */
int omni_sensory_update_precision(omni_sensory_bridge_t* bridge);

/* ============================================================================
 * Attention API
 * ============================================================================ */

/**
 * @brief Set attention mode
 */
int omni_sensory_set_attention(omni_sensory_bridge_t* bridge,
                                omni_sensory_attention_t mode);

/**
 * @brief Apply attention to modality
 */
int omni_sensory_apply_attention(omni_sensory_bridge_t* bridge,
                                  omni_modality_t modality,
                                  const float* attention_weights,
                                  uint32_t num_weights);

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_sensory_get_omni_effects(const omni_sensory_bridge_t* bridge,
                                   omni_to_sensory_effects_t* effects);

int omni_sensory_get_sensory_effects(const omni_sensory_bridge_t* bridge,
                                      sensory_to_omni_effects_t* effects);

int omni_sensory_get_modality_state(const omni_sensory_bridge_t* bridge,
                                     omni_modality_t modality,
                                     omni_modality_state_t* state);

int omni_sensory_get_stats(const omni_sensory_bridge_t* bridge,
                            omni_sensory_stats_t* stats);

int omni_sensory_reset_stats(omni_sensory_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_sensory_connect_bio_async(omni_sensory_bridge_t* bridge);
int omni_sensory_disconnect_bio_async(omni_sensory_bridge_t* bridge);
bool omni_sensory_is_bio_async_connected(const omni_sensory_bridge_t* bridge);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_sensory_modality_to_string(omni_modality_t modality);
const char* omni_sensory_crossmodal_to_string(omni_crossmodal_mode_t mode);
const char* omni_sensory_direction_to_string(omni_sensory_direction_t dir);
const char* omni_sensory_attention_to_string(omni_sensory_attention_t attn);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_SENSORY_BRIDGE_H */

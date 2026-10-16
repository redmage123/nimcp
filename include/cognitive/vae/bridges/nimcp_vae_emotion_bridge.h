/**
 * @file nimcp_vae_emotion_bridge.h
 * @brief Bridge between VAE and Emotional System for Affect Modulation
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE with emotional processing for affect-aware generation
 *
 * WHY:  Emotion modulates VAE operations:
 *       - Emotional valence/arousal as conditional labels
 *       - Affect-driven sampling temperature
 *       - Emotional tagging of latent representations
 *       - Mood-congruent memory retrieval
 *       - Emotional interpolation in latent space
 *
 * HOW:  Bridge couples emotional state with VAE:
 *       - Conditional VAE: emotion → latent prior shift
 *       - Sampling temperature: arousal → noise level
 *       - Latent tagging: valence/arousal stored with codes
 *       - Emotional generation: sample toward emotional targets
 *
 * EMOTIONAL DIMENSIONS:
 * =====================
 * - Valence: Negative (-1) to Positive (+1)
 * - Arousal: Low (0) to High (1)
 * - Dominance: Submissive (0) to Dominant (1) [optional]
 * - Intensity: Overall emotional magnitude
 *
 * BIO_MODULE: 0x1F18 (VAE-Emotion Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_EMOTION_BRIDGE_H
#define NIMCP_VAE_EMOTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VAE_EMOTION_BRIDGE_VERSION      "1.0.0"
#define BIO_MODULE_VAE_EMOTION_BRIDGE   0x1F18

/** Emotional dimension indices */
#define VAE_EMOTION_DIM_VALENCE         0
#define VAE_EMOTION_DIM_AROUSAL         1
#define VAE_EMOTION_DIM_DOMINANCE       2
#define VAE_EMOTION_DIM_INTENSITY       3
#define VAE_EMOTION_DIM_COUNT           4

/** Default arousal-to-temperature mapping */
#define VAE_EMOTION_MIN_TEMPERATURE     0.3f
#define VAE_EMOTION_MAX_TEMPERATURE     2.0f

/** Error code range (32490-32499) */
#define NIMCP_ERROR_VAE_EMOTION_BASE        32490
#define NIMCP_ERROR_VAE_EMOTION_NULL        32491
#define NIMCP_ERROR_VAE_EMOTION_NOT_CONNECTED 32492
#define NIMCP_ERROR_VAE_EMOTION_ENCODE_FAILED 32493
#define NIMCP_ERROR_VAE_EMOTION_INVALID_STATE 32494
#define NIMCP_ERROR_VAE_EMOTION_NO_MEMORY   32495

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Discrete emotion categories (for categorical conditioning)
 */
typedef enum {
    VAE_EMOTION_NEUTRAL = 0,
    VAE_EMOTION_HAPPY,
    VAE_EMOTION_SAD,
    VAE_EMOTION_ANGRY,
    VAE_EMOTION_FEARFUL,
    VAE_EMOTION_SURPRISED,
    VAE_EMOTION_DISGUSTED,
    VAE_EMOTION_COUNT
} vae_emotion_category_t;

/**
 * @brief Emotion conditioning mode
 */
typedef enum {
    VAE_EMOTION_COND_NONE = 0,        /**< No emotional conditioning */
    VAE_EMOTION_COND_CONTINUOUS,       /**< Valence/arousal continuous */
    VAE_EMOTION_COND_CATEGORICAL,      /**< Discrete emotion categories */
    VAE_EMOTION_COND_MIXED             /**< Both continuous and categorical */
} vae_emotion_cond_mode_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_EMOTION_STATE_DISCONNECTED = 0,
    VAE_EMOTION_STATE_CONNECTED,
    VAE_EMOTION_STATE_ENCODING,
    VAE_EMOTION_STATE_GENERATING,
    VAE_EMOTION_STATE_ERROR
} vae_emotion_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Emotional state representation
 */
typedef struct {
    float valence;                    /**< [-1, 1]: negative to positive */
    float arousal;                    /**< [0, 1]: low to high energy */
    float dominance;                  /**< [0, 1]: submissive to dominant */
    float intensity;                  /**< [0, 1]: overall magnitude */
    vae_emotion_category_t category;  /**< Discrete category */
    uint64_t timestamp_ms;            /**< When emotion was recorded */
} vae_emotion_state_t;

/**
 * @brief Temperature mapping configuration
 */
typedef struct {
    float min_temperature;            /**< Temperature at zero arousal */
    float max_temperature;            /**< Temperature at max arousal */
    float valence_weight;             /**< How much valence affects temp */
    bool use_logarithmic;             /**< Log scale for temperature */
} vae_emotion_temp_config_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_emotion_cond_mode_t cond_mode;
    vae_emotion_temp_config_t temp_config;

    /* Conditioning parameters */
    uint32_t emotion_embed_dim;       /**< Dimension for emotion embedding */
    float conditioning_strength;      /**< How strongly emotion affects prior */

    /* Latent tagging */
    bool enable_latent_tagging;       /**< Tag latents with emotion state */
    bool enable_mood_congruent;       /**< Bias retrieval toward current mood */

    /* Decay */
    float emotion_decay_rate;         /**< How fast emotion decays */
    float emotion_smoothing;          /**< Temporal smoothing factor */

    bool enable_logging;
} vae_emotion_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

typedef struct {
    float* latent;
    uint32_t latent_dim;
    vae_emotion_state_t emotion_tag;  /**< Emotion state at encoding */
    float emotional_coherence;        /**< How well latent matches emotion */
    uint64_t encoding_time_us;
} vae_emotion_encode_result_t;

typedef struct {
    float* generated;
    uint32_t generated_dim;
    float* latent;
    uint32_t latent_dim;
    vae_emotion_state_t target_emotion;
    float emotion_alignment;          /**< How well output matches emotion */
    float temperature_used;
    uint64_t generation_time_us;
} vae_emotion_generate_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_encodes;
    uint64_t total_generations;
    float avg_emotional_coherence;
    float avg_emotion_alignment;
    float per_category_count[VAE_EMOTION_COUNT];
    vae_emotion_state_t current_emotion;
    uint64_t creation_time_us;
    uint64_t last_operation_us;
} vae_emotion_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_emotion_bridge {
    vae_emotion_bridge_config_t config;
    vae_system_t* vae;
    void* emotion_system;
    vae_emotion_bridge_state_t state;
    bool is_initialized;

    /* Current emotional state */
    vae_emotion_state_t current_emotion;
    vae_emotion_state_t smoothed_emotion;

    /* Emotion embeddings for conditioning */
    float* emotion_embeddings;        /**< [VAE_EMOTION_COUNT, embed_dim] */
    uint32_t embed_dim;

    /* Latent baselines per emotion */
    float* emotion_latent_means[VAE_EMOTION_COUNT];
    uint64_t emotion_sample_counts[VAE_EMOTION_COUNT];

    /* Working buffers */
    float* latent_buffer;
    float* condition_buffer;

    /* Statistics */
    vae_emotion_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_emotion_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_emotion_bridge_default_config(vae_emotion_bridge_config_t* config);
vae_emotion_bridge_t* vae_emotion_bridge_create(const vae_emotion_bridge_config_t* config);
void vae_emotion_bridge_destroy(vae_emotion_bridge_t* bridge);
int vae_emotion_bridge_connect_vae(vae_emotion_bridge_t* bridge, vae_system_t* vae);
int vae_emotion_bridge_connect_emotion(vae_emotion_bridge_t* bridge, void* emotion_system);
int vae_emotion_bridge_disconnect(vae_emotion_bridge_t* bridge);
bool vae_emotion_bridge_is_connected(const vae_emotion_bridge_t* bridge);

/* ============================================================================
 * Emotion State API
 * ============================================================================ */

int vae_emotion_set_state(vae_emotion_bridge_t* bridge, const vae_emotion_state_t* state);
int vae_emotion_get_state(const vae_emotion_bridge_t* bridge, vae_emotion_state_t* state);
int vae_emotion_update_from_system(vae_emotion_bridge_t* bridge);
int vae_emotion_decay(vae_emotion_bridge_t* bridge, float dt);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_emotion_encode(vae_emotion_bridge_t* bridge,
                        const float* input, uint32_t input_dim,
                        vae_emotion_encode_result_t* result);

int vae_emotion_encode_with_emotion(vae_emotion_bridge_t* bridge,
                                     const float* input, uint32_t input_dim,
                                     const vae_emotion_state_t* emotion,
                                     vae_emotion_encode_result_t* result);

/* ============================================================================
 * Generation API
 * ============================================================================ */

int vae_emotion_generate(vae_emotion_bridge_t* bridge,
                          vae_emotion_generate_result_t* result);

int vae_emotion_generate_with_emotion(vae_emotion_bridge_t* bridge,
                                       const vae_emotion_state_t* target_emotion,
                                       vae_emotion_generate_result_t* result);

int vae_emotion_generate_toward_category(vae_emotion_bridge_t* bridge,
                                          vae_emotion_category_t category,
                                          float intensity,
                                          vae_emotion_generate_result_t* result);

int vae_emotion_interpolate_emotions(vae_emotion_bridge_t* bridge,
                                      const vae_emotion_state_t* emotion_a,
                                      const vae_emotion_state_t* emotion_b,
                                      float t,
                                      vae_emotion_generate_result_t* result);

/* ============================================================================
 * Temperature Mapping API
 * ============================================================================ */

float vae_emotion_compute_temperature(const vae_emotion_bridge_t* bridge,
                                       const vae_emotion_state_t* emotion);

int vae_emotion_set_temp_config(vae_emotion_bridge_t* bridge,
                                 const vae_emotion_temp_config_t* config);

/* ============================================================================
 * Latent Tagging API
 * ============================================================================ */

int vae_emotion_tag_latent(vae_emotion_bridge_t* bridge,
                            const float* latent, uint32_t latent_dim,
                            const vae_emotion_state_t* emotion);

int vae_emotion_find_by_emotion(vae_emotion_bridge_t* bridge,
                                 const vae_emotion_state_t* target,
                                 float tolerance,
                                 float* latent, uint32_t* latent_dim);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_emotion_bridge_state_t vae_emotion_bridge_get_state(const vae_emotion_bridge_t* bridge);
int vae_emotion_bridge_get_stats(const vae_emotion_bridge_t* bridge,
                                  vae_emotion_bridge_stats_t* stats);
const char* vae_emotion_category_to_string(vae_emotion_category_t category);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_emotion_encode_result_free(vae_emotion_encode_result_t* result);
void vae_emotion_generate_result_free(vae_emotion_generate_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_EMOTION_BRIDGE_H */

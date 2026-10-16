/**
 * @file nimcp_vae_visual_bridge.h
 * @brief Bridge between VAE and Visual Cortex (Occipital Lobe)
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE decoder outputs with V1-V5 visual hierarchy
 *
 * WHY:  VAE provides hierarchical visual representation:
 *       - Low-level features → V1 (edges, orientation)
 *       - Mid-level features → V2-V3 (contours, texture, form)
 *       - High-level features → V4-V5 (color, shape, motion)
 *       - Retinotopic organization in latent space
 *       - Metabolic modulation of reconstruction quality
 *
 * HOW:  Bridge maps VAE latent dimensions to visual areas:
 *       - Encoder: Visual input → hierarchical latent
 *       - Decoder: Latent → visual reconstruction by area
 *       - Attention: Latent precision → visual salience
 *
 * SPECTRAL ANALYSIS:
 * ==================
 * - FFT: Frequency analysis of visual patterns
 * - Gabor: Orientation-selective filtering (V1-like)
 * - Spatial pyramid: Multi-scale representation
 *
 * BIO_MODULE: 0x1F16 (VAE-Visual Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_VISUAL_BRIDGE_H
#define NIMCP_VAE_VISUAL_BRIDGE_H

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

/** Bridge version */
#define VAE_VISUAL_BRIDGE_VERSION       "1.0.0"

/** Bio-async module ID */
#define BIO_MODULE_VAE_VISUAL_BRIDGE    0x1F16

/** Visual areas */
#define VAE_VISUAL_AREA_V1              0
#define VAE_VISUAL_AREA_V2              1
#define VAE_VISUAL_AREA_V3              2
#define VAE_VISUAL_AREA_V4              3
#define VAE_VISUAL_AREA_V5              4
#define VAE_VISUAL_AREA_COUNT           5

/** Default latent dimensions per visual area */
#define VAE_VISUAL_V1_LATENT_DIM        64
#define VAE_VISUAL_V2_LATENT_DIM        48
#define VAE_VISUAL_V3_LATENT_DIM        32
#define VAE_VISUAL_V4_LATENT_DIM        24
#define VAE_VISUAL_V5_LATENT_DIM        16

/** Error code range (32470-32479) */
#define NIMCP_ERROR_VAE_VISUAL_BASE         32470
#define NIMCP_ERROR_VAE_VISUAL_NULL         32471
#define NIMCP_ERROR_VAE_VISUAL_NOT_CONNECTED 32472
#define NIMCP_ERROR_VAE_VISUAL_ENCODE_FAILED 32473
#define NIMCP_ERROR_VAE_VISUAL_DECODE_FAILED 32474
#define NIMCP_ERROR_VAE_VISUAL_NO_MEMORY    32475
#define NIMCP_ERROR_VAE_VISUAL_DIM_MISMATCH 32476

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Visual processing mode
 */
typedef enum {
    VAE_VISUAL_MODE_ENCODE = 0,      /**< Encode visual input */
    VAE_VISUAL_MODE_DECODE,           /**< Decode to visual output */
    VAE_VISUAL_MODE_RECONSTRUCT,      /**< Full encode-decode */
    VAE_VISUAL_MODE_GENERATE          /**< Generate from prior */
} vae_visual_mode_t;

/**
 * @brief Feature extraction type
 */
typedef enum {
    VAE_VISUAL_FEAT_RAW = 0,         /**< Raw pixel features */
    VAE_VISUAL_FEAT_GABOR,            /**< Gabor filter features */
    VAE_VISUAL_FEAT_SPECTRAL,         /**< FFT spectral features */
    VAE_VISUAL_FEAT_PYRAMID           /**< Spatial pyramid features */
} vae_visual_feature_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_VISUAL_STATE_DISCONNECTED = 0,
    VAE_VISUAL_STATE_CONNECTED,
    VAE_VISUAL_STATE_ENCODING,
    VAE_VISUAL_STATE_DECODING,
    VAE_VISUAL_STATE_ERROR
} vae_visual_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Per-area configuration
 */
typedef struct {
    uint32_t latent_dim;              /**< Latent dimensions for this area */
    float metabolic_weight;           /**< Metabolic constraint weight */
    float attention_weight;           /**< Attention modulation weight */
    bool enable_gabor;                /**< Enable Gabor filtering */
    uint32_t gabor_orientations;      /**< Number of Gabor orientations */
} vae_visual_area_config_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_visual_area_config_t areas[VAE_VISUAL_AREA_COUNT];

    /* Image parameters */
    uint32_t input_width;
    uint32_t input_height;
    uint32_t input_channels;

    /* Retinotopic mapping */
    bool enable_retinotopy;
    float foveal_scale;               /**< Foveal magnification factor */

    /* Feature extraction */
    vae_visual_feature_t feature_type;
    bool enable_spectral_analysis;
    uint32_t fft_size;

    /* Metabolic constraints */
    bool enable_metabolic_modulation;
    float min_metabolic_capacity;

    /* Logging */
    bool enable_logging;
} vae_visual_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Per-area encoding result
 */
typedef struct {
    float* latent;                    /**< Latent code for this area */
    uint32_t latent_dim;
    float* variance;                  /**< Latent variance */
    float activation_level;           /**< Average activation */
    float metabolic_cost;             /**< Metabolic cost for this area */
} vae_visual_area_result_t;

/**
 * @brief Full visual encoding result
 */
typedef struct {
    vae_visual_area_result_t areas[VAE_VISUAL_AREA_COUNT];
    float* combined_latent;           /**< Combined latent across areas */
    uint32_t combined_dim;
    float total_metabolic_cost;
    float encoding_quality;
    uint64_t encoding_time_us;
} vae_visual_encode_result_t;

/**
 * @brief Visual decoding result
 */
typedef struct {
    float* reconstruction;            /**< Reconstructed image */
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    float reconstruction_error;
    float vividness;                  /**< Reconstruction vividness */
    uint64_t decoding_time_us;
} vae_visual_decode_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_encodes;
    uint64_t total_decodes;
    float avg_encoding_quality;
    float avg_reconstruction_error;
    float avg_metabolic_cost;
    float per_area_usage[VAE_VISUAL_AREA_COUNT];
    uint64_t creation_time_us;
    uint64_t last_operation_us;
} vae_visual_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_visual_bridge {
    vae_visual_bridge_config_t config;
    vae_system_t* vae;
    void* visual_cortex;              /**< Visual cortex/occipital lobe */
    vae_visual_bridge_state_t state;
    bool is_initialized;

    /* Dimension mapping */
    uint32_t area_offsets[VAE_VISUAL_AREA_COUNT];
    uint32_t total_latent_dim;

    /* Working buffers */
    float* encode_buffer;
    float* decode_buffer;
    float* gabor_buffer;
    float* fft_buffer;

    /* Metabolic state */
    float current_metabolic_capacity[VAE_VISUAL_AREA_COUNT];

    /* Statistics */
    vae_visual_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_visual_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_visual_bridge_default_config(vae_visual_bridge_config_t* config);
vae_visual_bridge_t* vae_visual_bridge_create(const vae_visual_bridge_config_t* config);
void vae_visual_bridge_destroy(vae_visual_bridge_t* bridge);
int vae_visual_bridge_connect_vae(vae_visual_bridge_t* bridge, vae_system_t* vae);
int vae_visual_bridge_connect_cortex(vae_visual_bridge_t* bridge, void* visual_cortex);
int vae_visual_bridge_disconnect(vae_visual_bridge_t* bridge);
bool vae_visual_bridge_is_connected(const vae_visual_bridge_t* bridge);

/* ============================================================================
 * Encoding API
 * ============================================================================ */

int vae_visual_encode(vae_visual_bridge_t* bridge,
                       const float* image, uint32_t width, uint32_t height,
                       uint32_t channels,
                       vae_visual_encode_result_t* result);

int vae_visual_encode_area(vae_visual_bridge_t* bridge,
                            const float* image, uint32_t width, uint32_t height,
                            uint32_t channels, uint32_t area,
                            vae_visual_area_result_t* result);

int vae_visual_encode_with_attention(vae_visual_bridge_t* bridge,
                                      const float* image, uint32_t width, uint32_t height,
                                      uint32_t channels,
                                      const float* attention_map,
                                      vae_visual_encode_result_t* result);

/* ============================================================================
 * Decoding API
 * ============================================================================ */

int vae_visual_decode(vae_visual_bridge_t* bridge,
                       const float* latent, uint32_t latent_dim,
                       vae_visual_decode_result_t* result);

int vae_visual_decode_area(vae_visual_bridge_t* bridge,
                            const float* latent, uint32_t latent_dim,
                            uint32_t area,
                            vae_visual_decode_result_t* result);

int vae_visual_generate(vae_visual_bridge_t* bridge,
                         float temperature,
                         vae_visual_decode_result_t* result);

/* ============================================================================
 * Feature Extraction API
 * ============================================================================ */

int vae_visual_extract_gabor(vae_visual_bridge_t* bridge,
                              const float* image, uint32_t width, uint32_t height,
                              float* features, uint32_t* feature_dim);

int vae_visual_extract_spectral(vae_visual_bridge_t* bridge,
                                 const float* image, uint32_t width, uint32_t height,
                                 float* features, uint32_t* feature_dim);

/* ============================================================================
 * Metabolic Modulation API
 * ============================================================================ */

int vae_visual_set_metabolic_capacity(vae_visual_bridge_t* bridge,
                                       uint32_t area, float capacity);

int vae_visual_get_metabolic_capacity(const vae_visual_bridge_t* bridge,
                                       uint32_t area, float* capacity);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_visual_bridge_state_t vae_visual_bridge_get_state(const vae_visual_bridge_t* bridge);
int vae_visual_bridge_get_stats(const vae_visual_bridge_t* bridge,
                                 vae_visual_bridge_stats_t* stats);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_visual_encode_result_free(vae_visual_encode_result_t* result);
void vae_visual_decode_result_free(vae_visual_decode_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_VISUAL_BRIDGE_H */

//=============================================================================
// nimcp_gan_bridge.h - GAN Model Integration
//=============================================================================
/**
 * @file nimcp_gan_bridge.h
 * @brief Bridge to GAN models for image generation
 *
 * WHAT: Interface to StyleGAN and other GAN architectures
 * WHY:  Alternative generation method with different characteristics
 * HOW:  Wraps ONNX Runtime with GAN-specific operations
 *
 * SUPPORTED MODELS:
 * - StyleGAN2/3: High-quality face/art generation
 * - BigGAN: Class-conditional generation
 * - VQGAN: Vector-quantized generation
 * - Custom GANs
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_GAN_BRIDGE_H
#define NIMCP_GAN_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/external/nimcp_creative_onnx_runtime.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// GAN Model Types
//=============================================================================

/**
 * @brief GAN architecture types
 */
typedef enum {
    GAN_TYPE_STYLEGAN2 = 0,        /**< StyleGAN2 */
    GAN_TYPE_STYLEGAN3,            /**< StyleGAN3 */
    GAN_TYPE_BIGGAN,               /**< BigGAN */
    GAN_TYPE_VQGAN,                /**< VQGAN */
    GAN_TYPE_PGAN,                 /**< Progressive GAN */
    GAN_TYPE_CUSTOM                /**< Custom GAN */
} gan_type_t;

/**
 * @brief Latent space types
 */
typedef enum {
    LATENT_SPACE_Z = 0,            /**< Z space (raw latent) */
    LATENT_SPACE_W,                /**< W space (mapped latent) */
    LATENT_SPACE_W_PLUS,           /**< W+ space (per-layer latent) */
    LATENT_SPACE_S                 /**< S space (style space) */
} latent_space_t;

//=============================================================================
// Latent Types
//=============================================================================

/**
 * @brief Latent vector
 */
typedef struct {
    float* data;                   /**< Latent data */
    uint32_t dim;                  /**< Latent dimension */
    latent_space_t space;          /**< Latent space type */
    uint32_t num_layers;           /**< Number of layers (for W+) */
    bool owns_data;                /**< true if owns data */
} gan_latent_t;

/**
 * @brief Truncation parameters
 */
typedef struct {
    float psi;                     /**< Truncation psi (0-1) */
    uint32_t cutoff;               /**< Layer cutoff for truncation */
} truncation_params_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief GAN bridge configuration
 */
typedef struct {
    gan_type_t type;               /**< GAN architecture type */

    /* Model paths */
    char generator_path[512];      /**< Generator ONNX path */
    char encoder_path[512];        /**< Encoder ONNX path (optional) */
    char discriminator_path[512];  /**< Discriminator ONNX path (optional) */

    /* Model properties */
    uint32_t latent_dim;           /**< Z latent dimension */
    uint32_t w_dim;                /**< W latent dimension */
    uint32_t num_layers;           /**< Number of style layers */
    uint32_t output_size;          /**< Output image size */

    /* Device */
    onnx_device_t device;          /**< ONNX device */
    int32_t device_id;             /**< GPU device ID */

    /* Generation defaults */
    latent_space_t default_space;  /**< Default latent space */
    truncation_params_t truncation; /**< Default truncation */

    /* BigGAN specific */
    uint32_t num_classes;          /**< Number of classes (BigGAN) */
} gan_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void gan_bridge_config_defaults(gan_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief GAN model bridge
 */
struct gan_bridge {
    gan_bridge_config_t config;

    /* ONNX Runtime */
    creative_onnx_runtime_t* onnx_runtime;

    /* Model sessions */
    void* generator;               /**< Generator session */
    void* encoder;                 /**< Encoder session (optional) */
    void* discriminator;           /**< Discriminator session (optional) */
    void* mapping_network;         /**< Mapping network (Z->W) */

    /* Mean latent (for truncation) */
    gan_latent_t mean_latent;

    /* Statistics */
    uint64_t images_generated;
    float avg_generation_time_ms;
};

/** @brief Typedef for gan_bridge */
typedef struct gan_bridge gan_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create GAN bridge
 *
 * @param config Configuration
 * @return Bridge or NULL on error
 */
gan_bridge_t* gan_bridge_create(const gan_bridge_config_t* config);

/**
 * @brief Destroy GAN bridge
 *
 * @param bridge Bridge to destroy
 */
void gan_bridge_destroy(gan_bridge_t* bridge);

/**
 * @brief Load model
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int gan_bridge_load_model(gan_bridge_t* bridge);

/**
 * @brief Unload model
 *
 * @param bridge Bridge
 */
void gan_bridge_unload_model(gan_bridge_t* bridge);

//=============================================================================
// Latent API
//=============================================================================

/**
 * @brief Sample random latent
 *
 * @param bridge Bridge
 * @param space Latent space
 * @param seed Random seed (0 for random)
 * @param latent Output latent
 * @return 0 on success, -1 on error
 */
int gan_sample_latent(gan_bridge_t* bridge,
                       latent_space_t space,
                       uint64_t seed,
                       gan_latent_t* latent);

/**
 * @brief Map Z to W space
 *
 * @param bridge Bridge
 * @param z Z latent
 * @param w Output W latent
 * @return 0 on success, -1 on error
 */
int gan_map_z_to_w(gan_bridge_t* bridge,
                    const gan_latent_t* z,
                    gan_latent_t* w);

/**
 * @brief Apply truncation to latent
 *
 * @param bridge Bridge
 * @param latent Latent to truncate (modified in place)
 * @param params Truncation parameters
 * @return 0 on success, -1 on error
 */
int gan_truncate_latent(gan_bridge_t* bridge,
                         gan_latent_t* latent,
                         const truncation_params_t* params);

/**
 * @brief Interpolate between latents
 *
 * @param a First latent
 * @param b Second latent
 * @param t Interpolation factor [0-1]
 * @param result Output latent
 * @return 0 on success, -1 on error
 */
int gan_interpolate_latent(const gan_latent_t* a,
                            const gan_latent_t* b,
                            float t,
                            gan_latent_t* result);

/**
 * @brief Free latent
 *
 * @param latent Latent to free
 */
void gan_latent_free(gan_latent_t* latent);

//=============================================================================
// Generation API
//=============================================================================

/**
 * @brief Generate image from latent
 *
 * @param bridge Bridge
 * @param latent Latent vector
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int gan_generate(gan_bridge_t* bridge,
                  const gan_latent_t* latent,
                  visual_image_t* output);

/**
 * @brief Generate random image
 *
 * @param bridge Bridge
 * @param truncation Truncation psi
 * @param seed Random seed (0 for random)
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int gan_generate_random(gan_bridge_t* bridge,
                         float truncation,
                         uint64_t seed,
                         visual_image_t* output);

/**
 * @brief Generate class-conditional image (BigGAN)
 *
 * @param bridge Bridge
 * @param class_idx Class index
 * @param truncation Truncation psi
 * @param seed Random seed
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int gan_generate_class(gan_bridge_t* bridge,
                        uint32_t class_idx,
                        float truncation,
                        uint64_t seed,
                        visual_image_t* output);

//=============================================================================
// Encoding API (GAN Inversion)
//=============================================================================

/**
 * @brief Encode image to latent (GAN inversion)
 *
 * @param bridge Bridge
 * @param image Input image
 * @param space Target latent space
 * @param latent Output latent
 * @return 0 on success, -1 on error
 */
int gan_encode(gan_bridge_t* bridge,
                const visual_image_t* image,
                latent_space_t space,
                gan_latent_t* latent);

//=============================================================================
// Style Mixing API
//=============================================================================

/**
 * @brief Style mixing between two latents
 *
 * @param bridge Bridge
 * @param source Source latent (coarse features)
 * @param target Target latent (fine features)
 * @param crossover_layer Layer to switch styles
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int gan_style_mix(gan_bridge_t* bridge,
                   const gan_latent_t* source,
                   const gan_latent_t* target,
                   uint32_t crossover_layer,
                   visual_image_t* output);

/**
 * @brief Blend styles at specific layers
 *
 * @param bridge Bridge
 * @param latent_a First latent
 * @param latent_b Second latent
 * @param layer_weights Blend weight per layer [0-1]
 * @param num_layers Number of layers
 * @param result Output blended latent
 * @return 0 on success, -1 on error
 */
int gan_blend_styles(gan_bridge_t* bridge,
                      const gan_latent_t* latent_a,
                      const gan_latent_t* latent_b,
                      const float* layer_weights,
                      uint32_t num_layers,
                      gan_latent_t* result);

//=============================================================================
// Editing API
//=============================================================================

/**
 * @brief Edit latent with direction
 *
 * @param latent Input latent
 * @param direction Edit direction (same dim as latent)
 * @param magnitude Edit magnitude
 * @param result Output edited latent
 * @return 0 on success, -1 on error
 */
int gan_edit_latent(const gan_latent_t* latent,
                     const float* direction,
                     float magnitude,
                     gan_latent_t* result);

//=============================================================================
// Batch API
//=============================================================================

/**
 * @brief Generate batch of images
 *
 * @param bridge Bridge
 * @param latents Array of latents
 * @param num_latents Number of latents
 * @param outputs Output images array (caller allocated)
 * @return 0 on success, -1 on error
 */
int gan_generate_batch(gan_bridge_t* bridge,
                        const gan_latent_t* latents,
                        uint32_t num_latents,
                        visual_image_t* outputs);

/**
 * @brief Generate interpolation sequence
 *
 * @param bridge Bridge
 * @param start Start latent
 * @param end End latent
 * @param num_steps Number of steps
 * @param outputs Output images array (caller allocated, size = num_steps)
 * @return 0 on success, -1 on error
 */
int gan_generate_interpolation(gan_bridge_t* bridge,
                                const gan_latent_t* start,
                                const gan_latent_t* end,
                                uint32_t num_steps,
                                visual_image_t* outputs);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Check if bridge is ready
 *
 * @param bridge Bridge
 * @return true if ready
 */
bool gan_bridge_ready(const gan_bridge_t* bridge);

/**
 * @brief Get model info
 *
 * @param bridge Bridge
 * @return Info string
 */
const char* gan_bridge_model_info(const gan_bridge_t* bridge);

/**
 * @brief Get last error
 *
 * @param bridge Bridge
 * @return Error message or NULL
 */
const char* gan_bridge_get_error(const gan_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAN_BRIDGE_H */

//=============================================================================
// nimcp_creative_neural_bridge.h - Neural Network Generation Bridge
//=============================================================================
/**
 * @file nimcp_creative_neural_bridge.h
 * @brief Bridge connecting creative system to neural network backends
 *
 * WHAT: Unified interface to various neural generation backends
 * WHY:  Abstract away backend differences for creative system
 * HOW:  Common API that routes to diffusion, GAN, or API backends
 *
 * BACKENDS:
 * - Local Diffusion (ONNX)
 * - Local GAN (ONNX)
 * - TensorRT optimized
 * - Cloud APIs
 * - Custom models
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_NEURAL_BRIDGE_H
#define NIMCP_CREATIVE_NEURAL_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/external/nimcp_diffusion_bridge.h"
#include "cognitive/creative/external/nimcp_gan_bridge.h"
#include "cognitive/creative/external/nimcp_creative_api_client.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Backend Types
//=============================================================================

/**
 * @brief Neural backend types
 */
typedef enum {
    NEURAL_BACKEND_DIFFUSION_LOCAL = 0,  /**< Local diffusion model */
    NEURAL_BACKEND_GAN_LOCAL,            /**< Local GAN model */
    NEURAL_BACKEND_API_CLOUD,            /**< Cloud API */
    NEURAL_BACKEND_HYBRID,               /**< Try local first, fallback to API */
    NEURAL_BACKEND_AUTO                  /**< Auto-select best available */
} neural_backend_t;

/**
 * @brief Backend status
 */
typedef struct {
    neural_backend_t backend;      /**< Backend type */
    bool available;                /**< Is backend available? */
    bool loaded;                   /**< Is model loaded? */
    uint64_t vram_usage;           /**< Current VRAM usage */
    float avg_gen_time_ms;         /**< Average generation time */
    uint32_t pending_requests;     /**< Pending async requests */
} backend_status_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Neural bridge configuration
 */
typedef struct {
    /* Backend preference */
    neural_backend_t preferred_backend;
    bool enable_fallback;          /**< Fall back to other backends */

    /* Local diffusion */
    diffusion_bridge_config_t diffusion_config;
    bool enable_diffusion;

    /* Local GAN */
    gan_bridge_config_t gan_config;
    bool enable_gan;

    /* Cloud API */
    creative_api_client_config_t api_config;
    bool enable_api;

    /* Quality settings */
    float quality_threshold;       /**< Regenerate below this quality */
    uint32_t max_regeneration;     /**< Max regeneration attempts */

    /* Resource management */
    uint64_t max_vram_total;       /**< Max total VRAM across backends */
    bool lazy_load_models;         /**< Load models on demand */
    bool unload_unused;            /**< Unload models when not in use */
    uint32_t unload_after_ms;      /**< Unload after inactivity (ms) */
} creative_neural_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_neural_bridge_config_defaults(creative_neural_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Creative neural bridge
 */
struct creative_neural_bridge {
    creative_neural_bridge_config_t config;

    /* Backends */
    diffusion_bridge_t* diffusion;
    gan_bridge_t* gan;
    creative_api_client_t* api;

    /* State */
    neural_backend_t active_backend;
    uint64_t last_use_time[4];     /**< Last use time per backend */

    /* Quality checker */
    void* aesthetic_evaluator;

    /* Statistics */
    uint64_t generations;
    uint64_t regenerations;
    uint64_t fallbacks;
    float avg_quality;
};

/** @brief Typedef for creative_neural_bridge */
typedef struct creative_neural_bridge creative_neural_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neural bridge
 *
 * @param config Configuration
 * @return Bridge or NULL on error
 */
creative_neural_bridge_t* creative_neural_bridge_create(
    const creative_neural_bridge_config_t* config);

/**
 * @brief Destroy neural bridge
 *
 * @param bridge Bridge to destroy
 */
void creative_neural_bridge_destroy(creative_neural_bridge_t* bridge);

/**
 * @brief Load backend
 *
 * @param bridge Bridge
 * @param backend Backend to load
 * @return 0 on success, -1 on error
 */
int creative_neural_bridge_load_backend(creative_neural_bridge_t* bridge,
                                         neural_backend_t backend);

/**
 * @brief Unload backend
 *
 * @param bridge Bridge
 * @param backend Backend to unload
 */
void creative_neural_bridge_unload_backend(creative_neural_bridge_t* bridge,
                                            neural_backend_t backend);

//=============================================================================
// Generation API
//=============================================================================

/**
 * @brief Generate image from text
 *
 * @param bridge Bridge
 * @param prompt Prompt
 * @param negative_prompt Negative prompt
 * @param width Width
 * @param height Height
 * @param steps Steps
 * @param guidance CFG guidance
 * @param seed Seed (0 for random)
 * @param backend_hint Preferred backend (or AUTO)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int neural_generate_image(creative_neural_bridge_t* bridge,
                           const char* prompt,
                           const char* negative_prompt,
                           uint32_t width, uint32_t height,
                           uint32_t steps,
                           float guidance,
                           uint64_t seed,
                           neural_backend_t backend_hint,
                           visual_generation_result_t* result);

/**
 * @brief Generate with style embedding
 *
 * @param bridge Bridge
 * @param prompt Prompt
 * @param style Style embedding
 * @param width Width
 * @param height Height
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int neural_generate_styled(creative_neural_bridge_t* bridge,
                            const char* prompt,
                            const style_embedding_t* style,
                            uint32_t width, uint32_t height,
                            visual_generation_result_t* result);

/**
 * @brief Image-to-image transformation
 *
 * @param bridge Bridge
 * @param init_image Initial image
 * @param prompt Prompt
 * @param strength Denoising strength
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int neural_img2img(creative_neural_bridge_t* bridge,
                    const visual_image_t* init_image,
                    const char* prompt,
                    float strength,
                    visual_generation_result_t* result);

/**
 * @brief Inpainting
 *
 * @param bridge Bridge
 * @param image Image
 * @param mask Mask
 * @param prompt Prompt
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int neural_inpaint(creative_neural_bridge_t* bridge,
                    const visual_image_t* image,
                    const visual_image_t* mask,
                    const char* prompt,
                    visual_generation_result_t* result);

//=============================================================================
// GAN-Specific API
//=============================================================================

/**
 * @brief Generate from latent
 *
 * @param bridge Bridge
 * @param latent Latent vector
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int neural_generate_from_latent(creative_neural_bridge_t* bridge,
                                 const gan_latent_t* latent,
                                 visual_generation_result_t* result);

/**
 * @brief Encode image to latent (GAN inversion)
 *
 * @param bridge Bridge
 * @param image Image
 * @param latent Output latent
 * @return 0 on success, -1 on error
 */
int neural_encode_to_latent(creative_neural_bridge_t* bridge,
                             const visual_image_t* image,
                             gan_latent_t* latent);

//=============================================================================
// Batch API
//=============================================================================

/**
 * @brief Generate batch of images
 *
 * @param bridge Bridge
 * @param prompt Prompt
 * @param width Width
 * @param height Height
 * @param seeds Seeds array
 * @param batch_size Batch size
 * @param results Output results (caller allocated)
 * @return 0 on success, -1 on error
 */
int neural_generate_batch(creative_neural_bridge_t* bridge,
                           const char* prompt,
                           uint32_t width, uint32_t height,
                           const uint64_t* seeds,
                           uint32_t batch_size,
                           visual_generation_result_t* results);

//=============================================================================
// Backend Management API
//=============================================================================

/**
 * @brief Get backend status
 *
 * @param bridge Bridge
 * @param backend Backend
 * @param status Output status
 * @return 0 on success, -1 on error
 */
int neural_get_backend_status(const creative_neural_bridge_t* bridge,
                               neural_backend_t backend,
                               backend_status_t* status);

/**
 * @brief Select best backend for task
 *
 * @param bridge Bridge
 * @param width Target width
 * @param height Target height
 * @param need_speed Prioritize speed
 * @return Best backend
 */
neural_backend_t neural_select_backend(const creative_neural_bridge_t* bridge,
                                        uint32_t width, uint32_t height,
                                        bool need_speed);

/**
 * @brief Warm up backend
 *
 * @param bridge Bridge
 * @param backend Backend to warm up
 * @return 0 on success, -1 on error
 */
int neural_warmup_backend(creative_neural_bridge_t* bridge,
                           neural_backend_t backend);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Set aesthetic evaluator for quality checking
 *
 * @param bridge Bridge
 * @param evaluator Evaluator
 */
void neural_bridge_set_evaluator(creative_neural_bridge_t* bridge,
                                  void* evaluator);

/**
 * @brief Get diffusion bridge
 *
 * @param bridge Bridge
 * @return Diffusion bridge or NULL
 */
diffusion_bridge_t* neural_bridge_get_diffusion(creative_neural_bridge_t* bridge);

/**
 * @brief Get GAN bridge
 *
 * @param bridge Bridge
 * @return GAN bridge or NULL
 */
gan_bridge_t* neural_bridge_get_gan(creative_neural_bridge_t* bridge);

/**
 * @brief Get API client
 *
 * @param bridge Bridge
 * @return API client or NULL
 */
creative_api_client_t* neural_bridge_get_api(creative_neural_bridge_t* bridge);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get backend name
 *
 * @param backend Backend
 * @return Name string
 */
const char* neural_backend_name(neural_backend_t backend);

/**
 * @brief Check if backend is available
 *
 * @param bridge Bridge
 * @param backend Backend
 * @return true if available
 */
bool neural_backend_available(const creative_neural_bridge_t* bridge,
                               neural_backend_t backend);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_NEURAL_BRIDGE_H */

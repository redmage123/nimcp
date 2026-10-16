//=============================================================================
// nimcp_diffusion_bridge.h - Diffusion Model Integration
//=============================================================================
/**
 * @file nimcp_diffusion_bridge.h
 * @brief Bridge to diffusion models for image generation
 *
 * WHAT: Interface to Stable Diffusion and other diffusion models
 * WHY:  Enable high-quality image generation
 * HOW:  Wraps ONNX Runtime with diffusion-specific pipeline
 *
 * SUPPORTED MODELS:
 * - Stable Diffusion 1.5, 2.1
 * - Stable Diffusion XL (SDXL)
 * - SDXL Turbo (fast)
 * - Custom fine-tuned models
 *
 * PIPELINE COMPONENTS:
 * - Text Encoder: CLIP text encoder
 * - U-Net: Denoising network
 * - VAE: Image encoder/decoder
 * - Safety Checker: NSFW detection (optional)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_DIFFUSION_BRIDGE_H
#define NIMCP_DIFFUSION_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/external/nimcp_creative_onnx_runtime.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Diffusion Model Types
//=============================================================================

/**
 * @brief Diffusion model variants
 */
typedef enum {
    DIFFUSION_MODEL_SD_15 = 0,     /**< Stable Diffusion 1.5 */
    DIFFUSION_MODEL_SD_21,         /**< Stable Diffusion 2.1 */
    DIFFUSION_MODEL_SDXL,          /**< Stable Diffusion XL */
    DIFFUSION_MODEL_SDXL_TURBO,    /**< SDXL Turbo */
    DIFFUSION_MODEL_SD3,           /**< Stable Diffusion 3 */
    DIFFUSION_MODEL_CUSTOM         /**< Custom model */
} diffusion_model_type_t;

/**
 * @brief Backend types
 */
typedef enum {
    DIFFUSION_BACKEND_ONNX = 0,    /**< Local ONNX Runtime */
    DIFFUSION_BACKEND_TENSORRT,    /**< TensorRT optimized */
    DIFFUSION_BACKEND_API_STABILITY, /**< Stability AI API */
    DIFFUSION_BACKEND_API_OPENAI,  /**< OpenAI DALL-E API */
    DIFFUSION_BACKEND_API_REPLICATE /**< Replicate API */
} diffusion_backend_t;

/**
 * @brief Scheduler/sampler types
 */
typedef enum {
    SCHEDULER_PNDM = 0,            /**< PNDM scheduler */
    SCHEDULER_DDIM,                /**< DDIM scheduler */
    SCHEDULER_DPM_SOLVER,          /**< DPM-Solver++ */
    SCHEDULER_EULER,               /**< Euler scheduler */
    SCHEDULER_EULER_A,             /**< Euler ancestral */
    SCHEDULER_LMS,                 /**< LMS scheduler */
    SCHEDULER_HEUN,                /**< Heun scheduler */
    SCHEDULER_UNIPC                /**< UniPC scheduler */
} diffusion_scheduler_t;

//=============================================================================
// Pipeline Component Paths
//=============================================================================

/**
 * @brief Model component paths
 */
typedef struct {
    char text_encoder_path[512];   /**< Text encoder ONNX */
    char text_encoder_2_path[512]; /**< Second text encoder (SDXL) */
    char unet_path[512];           /**< U-Net ONNX */
    char vae_encoder_path[512];    /**< VAE encoder ONNX */
    char vae_decoder_path[512];    /**< VAE decoder ONNX */
    char safety_checker_path[512]; /**< Safety checker ONNX (optional) */
    char controlnet_path[512];     /**< ControlNet ONNX (optional) */
    char lora_path[512];           /**< LoRA weights (optional) */
    char scheduler_config_path[512]; /**< Scheduler config JSON */
    char tokenizer_path[512];      /**< Tokenizer vocab */
} diffusion_model_paths_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Diffusion bridge configuration
 */
typedef struct {
    /* Backend */
    diffusion_backend_t backend;   /**< Execution backend */
    diffusion_model_type_t model;  /**< Model variant */

    /* Local model paths */
    diffusion_model_paths_t paths; /**< Component paths */

    /* Device */
    onnx_device_t device;          /**< ONNX device */
    int32_t device_id;             /**< GPU device ID */

    /* Generation defaults */
    diffusion_scheduler_t scheduler; /**< Default scheduler */
    uint32_t default_steps;        /**< Default inference steps */
    float default_guidance;        /**< Default CFG scale */
    uint32_t default_width;        /**< Default width */
    uint32_t default_height;       /**< Default height */

    /* Safety */
    bool enable_safety_checker;    /**< Enable NSFW filtering */

    /* Memory optimization */
    bool enable_attention_slicing; /**< Reduce VRAM with attention slicing */
    bool enable_vae_slicing;       /**< Reduce VRAM with VAE slicing */
    bool enable_vae_tiling;        /**< Enable VAE tiling for large images */
    bool enable_model_offload;     /**< Offload to CPU when not in use */
    uint64_t max_vram_bytes;       /**< Max VRAM to use */

    /* API configuration (for cloud backends) */
    char api_key[256];             /**< API key */
    char api_base_url[256];        /**< API base URL */
    uint32_t api_timeout_ms;       /**< API timeout */
} diffusion_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void diffusion_bridge_config_defaults(diffusion_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Diffusion model bridge
 */
struct diffusion_bridge {
    diffusion_bridge_config_t config;

    /* ONNX Runtime (for local models) */
    creative_onnx_runtime_t* onnx_runtime;

    /* Model sessions */
    void* text_encoder;            /**< CLIP text encoder session */
    void* text_encoder_2;          /**< Second encoder (SDXL) */
    void* unet;                    /**< U-Net session */
    void* vae_encoder;             /**< VAE encoder session */
    void* vae_decoder;             /**< VAE decoder session */
    void* safety_checker;          /**< Safety checker session */
    void* controlnet;              /**< ControlNet session */

    /* Tokenizer */
    void* tokenizer;               /**< Text tokenizer */
    void* tokenizer_2;             /**< Second tokenizer (SDXL) */

    /* Scheduler state */
    void* scheduler;               /**< Scheduler instance */

    /* Loaded LoRA */
    void* lora_weights;
    float lora_scale;

    /* Statistics */
    uint64_t images_generated;
    float avg_generation_time_ms;
    uint64_t peak_vram_bytes;
};

/** @brief Typedef for diffusion_bridge */
typedef struct diffusion_bridge diffusion_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create diffusion bridge
 *
 * @param config Configuration
 * @return Bridge or NULL on error
 */
diffusion_bridge_t* diffusion_bridge_create(const diffusion_bridge_config_t* config);

/**
 * @brief Destroy diffusion bridge
 *
 * @param bridge Bridge to destroy
 */
void diffusion_bridge_destroy(diffusion_bridge_t* bridge);

/**
 * @brief Load model components
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int diffusion_bridge_load_model(diffusion_bridge_t* bridge);

/**
 * @brief Unload model components
 *
 * @param bridge Bridge
 */
void diffusion_bridge_unload_model(diffusion_bridge_t* bridge);

//=============================================================================
// Generation API
//=============================================================================

/**
 * @brief Generate image from text prompt
 *
 * @param bridge Bridge
 * @param prompt Positive prompt
 * @param negative_prompt Negative prompt (optional)
 * @param width Output width
 * @param height Output height
 * @param steps Inference steps
 * @param guidance_scale CFG scale
 * @param seed Random seed (0 for random)
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int diffusion_text_to_image(diffusion_bridge_t* bridge,
                             const char* prompt,
                             const char* negative_prompt,
                             uint32_t width, uint32_t height,
                             uint32_t steps,
                             float guidance_scale,
                             uint64_t seed,
                             visual_image_t* output);

/**
 * @brief Image-to-image transformation
 *
 * @param bridge Bridge
 * @param init_image Initial image
 * @param prompt Prompt
 * @param negative_prompt Negative prompt
 * @param strength Denoising strength (0-1)
 * @param steps Inference steps
 * @param guidance_scale CFG scale
 * @param seed Random seed
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int diffusion_img2img(diffusion_bridge_t* bridge,
                       const visual_image_t* init_image,
                       const char* prompt,
                       const char* negative_prompt,
                       float strength,
                       uint32_t steps,
                       float guidance_scale,
                       uint64_t seed,
                       visual_image_t* output);

/**
 * @brief Inpainting
 *
 * @param bridge Bridge
 * @param image Original image
 * @param mask Mask (white = generate)
 * @param prompt Prompt
 * @param negative_prompt Negative prompt
 * @param steps Inference steps
 * @param guidance_scale CFG scale
 * @param seed Random seed
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int diffusion_inpaint(diffusion_bridge_t* bridge,
                       const visual_image_t* image,
                       const visual_image_t* mask,
                       const char* prompt,
                       const char* negative_prompt,
                       uint32_t steps,
                       float guidance_scale,
                       uint64_t seed,
                       visual_image_t* output);

/**
 * @brief Generate with ControlNet
 *
 * @param bridge Bridge
 * @param prompt Prompt
 * @param negative_prompt Negative prompt
 * @param control_image Control image
 * @param control_type Control type name
 * @param conditioning_scale Control strength
 * @param width Output width
 * @param height Output height
 * @param steps Inference steps
 * @param guidance_scale CFG scale
 * @param seed Random seed
 * @param output Output image
 * @return 0 on success, -1 on error
 */
int diffusion_controlnet(diffusion_bridge_t* bridge,
                          const char* prompt,
                          const char* negative_prompt,
                          const visual_image_t* control_image,
                          const char* control_type,
                          float conditioning_scale,
                          uint32_t width, uint32_t height,
                          uint32_t steps,
                          float guidance_scale,
                          uint64_t seed,
                          visual_image_t* output);

//=============================================================================
// Batch Generation API
//=============================================================================

/**
 * @brief Generate batch of images
 *
 * @param bridge Bridge
 * @param prompt Prompt
 * @param negative_prompt Negative prompt
 * @param width Width
 * @param height Height
 * @param steps Steps
 * @param guidance_scale CFG scale
 * @param seeds Seeds array (one per image)
 * @param batch_size Number of images
 * @param outputs Output images array (caller allocated)
 * @return 0 on success, -1 on error
 */
int diffusion_generate_batch(diffusion_bridge_t* bridge,
                              const char* prompt,
                              const char* negative_prompt,
                              uint32_t width, uint32_t height,
                              uint32_t steps,
                              float guidance_scale,
                              const uint64_t* seeds,
                              uint32_t batch_size,
                              visual_image_t* outputs);

//=============================================================================
// LoRA API
//=============================================================================

/**
 * @brief Load LoRA weights
 *
 * @param bridge Bridge
 * @param lora_path Path to LoRA file
 * @param scale LoRA scale (typically 0.5-1.0)
 * @return 0 on success, -1 on error
 */
int diffusion_load_lora(diffusion_bridge_t* bridge,
                         const char* lora_path,
                         float scale);

/**
 * @brief Unload LoRA weights
 *
 * @param bridge Bridge
 */
void diffusion_unload_lora(diffusion_bridge_t* bridge);

//=============================================================================
// Scheduler API
//=============================================================================

/**
 * @brief Set scheduler
 *
 * @param bridge Bridge
 * @param scheduler Scheduler type
 * @return 0 on success, -1 on error
 */
int diffusion_set_scheduler(diffusion_bridge_t* bridge,
                             diffusion_scheduler_t scheduler);

/**
 * @brief Get current scheduler
 *
 * @param bridge Bridge
 * @return Current scheduler
 */
diffusion_scheduler_t diffusion_get_scheduler(const diffusion_bridge_t* bridge);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Check if bridge is ready
 *
 * @param bridge Bridge
 * @return true if ready
 */
bool diffusion_bridge_ready(const diffusion_bridge_t* bridge);

/**
 * @brief Get model info string
 *
 * @param bridge Bridge
 * @return Info string
 */
const char* diffusion_bridge_model_info(const diffusion_bridge_t* bridge);

/**
 * @brief Get VRAM usage
 *
 * @param bridge Bridge
 * @return VRAM usage in bytes
 */
uint64_t diffusion_bridge_vram_usage(const diffusion_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @param bridge Bridge
 * @return Error message or NULL
 */
const char* diffusion_bridge_get_error(const diffusion_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DIFFUSION_BRIDGE_H */

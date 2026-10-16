//=============================================================================
// nimcp_visual_generation.h - Creative Visual Generation
//=============================================================================
/**
 * @file nimcp_visual_generation.h
 * @brief Generates visual art and images
 *
 * WHAT: Creates still images, digital art, and visual content
 * WHY:  Enable AI to produce visual artwork
 * HOW:  Diffusion models, GANs, style transfer, and compositional AI
 *
 * GENERATION METHODS:
 * - Diffusion: Stable Diffusion, SDXL variants
 * - GAN: StyleGAN, BigGAN variants
 * - Style Transfer: Neural style transfer
 * - Procedural: Rule-based composition
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_VISUAL_GENERATION_H
#define NIMCP_VISUAL_GENERATION_H

#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/external/nimcp_diffusion_bridge.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Generation Method Types
//=============================================================================

/**
 * @brief Visual generation methods
 */
typedef enum {
    VISUAL_METHOD_DIFFUSION = 0,   /**< Diffusion models (recommended) */
    VISUAL_METHOD_GAN,             /**< GAN-based generation */
    VISUAL_METHOD_STYLE_TRANSFER,  /**< Neural style transfer */
    VISUAL_METHOD_INPAINTING,      /**< Fill masked regions */
    VISUAL_METHOD_OUTPAINTING,     /**< Extend beyond borders */
    VISUAL_METHOD_IMG2IMG,         /**< Image to image transformation */
    VISUAL_METHOD_PROCEDURAL       /**< Procedural generation */
} visual_method_t;

/* Note: diffusion_model_type_type_t and diffusion_scheduler_t are defined in nimcp_diffusion_bridge.h */

/**
 * @brief Sampler types for diffusion (visual generation specific aliases)
 */
typedef enum {
    SAMPLER_EULER = 0,             /**< Euler sampler */
    SAMPLER_EULER_A,               /**< Euler ancestral */
    SAMPLER_DPM_2,                 /**< DPM-Solver++ 2 */
    SAMPLER_DPM_2_A,               /**< DPM-Solver++ 2 ancestral */
    SAMPLER_DPM_PLUS_PLUS,         /**< DPM++ 2M */
    SAMPLER_DDIM,                  /**< DDIM sampler */
    SAMPLER_PNDM,                  /**< PNDM sampler */
    SAMPLER_HEUN,                  /**< Heun sampler */
    SAMPLER_LMS,                   /**< Linear multi-step */
    SAMPLER_COUNT
} visual_sampler_t;

//=============================================================================
// Composition Types
//=============================================================================

/**
 * @brief Composition rules
 */
typedef enum {
    COMPOSITION_RULE_NONE = 0,     /**< No specific rule */
    COMPOSITION_RULE_THIRDS,       /**< Rule of thirds */
    COMPOSITION_RULE_GOLDEN,       /**< Golden ratio */
    COMPOSITION_RULE_SYMMETRY,     /**< Symmetrical */
    COMPOSITION_RULE_LEADING,      /**< Leading lines */
    COMPOSITION_RULE_FRAME,        /**< Frame within frame */
    COMPOSITION_RULE_CENTERED,     /**< Centered subject */
    COMPOSITION_RULE_DIAGONAL      /**< Diagonal composition */
} composition_rule_t;

/**
 * @brief Color palette types
 */
typedef enum {
    PALETTE_AUTO = 0,              /**< Automatic/model default */
    PALETTE_WARM,                  /**< Warm colors */
    PALETTE_COOL,                  /**< Cool colors */
    PALETTE_MONOCHROME,            /**< Single color variations */
    PALETTE_COMPLEMENTARY,         /**< Complementary colors */
    PALETTE_ANALOGOUS,             /**< Adjacent colors */
    PALETTE_TRIADIC,               /**< Evenly spaced colors */
    PALETTE_CUSTOM                 /**< Custom palette */
} color_palette_t;

/**
 * @brief Custom color specification
 */
typedef struct {
    uint8_t r, g, b;               /**< RGB values */
    float weight;                  /**< Weight in palette */
} color_spec_t;

//=============================================================================
// Extended Request Types
//=============================================================================

/**
 * @brief ControlNet input types
 */
typedef enum {
    CONTROL_NONE = 0,              /**< No ControlNet */
    CONTROL_CANNY,                 /**< Canny edge detection */
    CONTROL_DEPTH,                 /**< Depth map */
    CONTROL_POSE,                  /**< Human pose */
    CONTROL_SCRIBBLE,              /**< Scribble/sketch */
    CONTROL_SEGMENTATION,          /**< Semantic segmentation */
    CONTROL_LINEART,               /**< Line art */
    CONTROL_SOFTEDGE,              /**< Soft edges */
    CONTROL_TILE                   /**< Tile pattern */
} control_type_t;

/**
 * @brief ControlNet input
 */
typedef struct {
    control_type_t type;           /**< Control type */
    visual_image_t* image;         /**< Control image */
    float strength;                /**< [0-1] Control strength */
} control_input_t;

/**
 * @brief Extended visual generation request
 */
typedef struct {
    /* Basic prompt */
    const char* prompt;            /**< Main prompt */
    const char* negative_prompt;   /**< What to avoid */

    /* Style */
    style_embedding_t* style;      /**< Target style embedding */
    visual_style_archetype_t archetype; /**< Or archetype (-1 for none) */

    /* Dimensions */
    uint32_t width;                /**< Output width */
    uint32_t height;               /**< Output height */

    /* Diffusion settings */
    diffusion_model_type_t model;       /**< Model to use */
    visual_sampler_t sampler;   /**< Sampler */
    uint32_t steps;                /**< Denoising steps */
    float guidance_scale;          /**< CFG scale (7.5 typical) */
    float denoise_strength;        /**< [0-1] For img2img (1.0 = ignore input) */

    /* Seed */
    uint64_t seed;                 /**< Random seed (0 for random) */
    bool vary_seed;                /**< Vary seed per batch */

    /* Input images */
    visual_image_t* init_image;    /**< Initial image (for img2img) */
    visual_image_t* mask;          /**< Mask (for inpainting, white=generate) */

    /* ControlNet */
    control_input_t* controls;     /**< ControlNet inputs */
    uint32_t num_controls;         /**< Number of controls */

    /* Composition */
    composition_rule_t composition; /**< Composition rule */
    color_palette_t palette;       /**< Color palette */
    color_spec_t* custom_colors;   /**< Custom palette colors */
    uint32_t num_custom_colors;    /**< Number of custom colors */

    /* Batch */
    uint32_t batch_size;           /**< Number of images to generate */

    /* Quality */
    bool upscale;                  /**< Apply upscaling */
    float upscale_factor;          /**< Upscale factor (2x, 4x) */
    bool refine;                   /**< Apply refinement pass */

    /* Method override */
    visual_method_t method;        /**< Generation method (or -1 for auto) */
} visual_generation_request_ext_t;

/**
 * @brief Batch generation result
 */
typedef struct {
    visual_generation_result_t* results; /**< Array of results */
    uint32_t num_results;          /**< Number of results */
    float total_time_ms;           /**< Total generation time */
    uint64_t seeds_used[16];       /**< Seeds used (max 16) */
} visual_batch_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Visual generator configuration
 */
typedef struct {
    /* Model settings */
    char model_dir[256];           /**< Model directory */
    diffusion_model_type_t default_model;
    bool use_gpu;
    int32_t gpu_device_id;
    bool use_tensorrt;             /**< Use TensorRT optimization */

    /* Default generation settings */
    uint32_t default_width;
    uint32_t default_height;
    uint32_t default_steps;
    float default_guidance;
    visual_sampler_t default_sampler;

    /* Quality settings */
    bool enable_self_evaluation;
    float min_quality_threshold;
    uint32_t max_regeneration_attempts;

    /* Resource limits */
    uint64_t max_vram_bytes;       /**< Max VRAM to use */
    bool enable_attention_slicing; /**< Reduce VRAM with sliced attention */
    bool enable_vae_tiling;        /**< Enable VAE tiling for large images */
} visual_generator_config_t;

/**
 * @brief Initialize config with defaults
 */
void visual_generator_config_defaults(visual_generator_config_t* config);

//=============================================================================
// Generator Structure
//=============================================================================

/**
 * @brief Visual generator
 */
struct visual_generator {
    visual_generator_config_t config;

    /* Models */
    void* diffusion_pipeline;      /**< Diffusion pipeline */
    void* gan_model;               /**< GAN model (optional) */
    void* style_transfer_model;    /**< Style transfer (optional) */
    void* upscaler;                /**< Upscaling model */
    void* refiner;                 /**< Refinement model */

    /* ControlNet */
    void* controlnet_canny;
    void* controlnet_depth;
    void* controlnet_pose;

    /* Style control */
    style_embedding_t* current_style;

    /* Integration */
    void* aesthetic_evaluator;
    void* creative_bridge;

    /* Cortical integration */
    void* visual_cortex;           /**< Visual cortex for style feedback */
    void* cortical_columns;        /**< Cortical columns for feature processing */

    /* Statistics */
    uint64_t images_generated;
    float avg_quality_score;
    float avg_generation_time_ms;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create visual generator
 *
 * @param config Configuration (NULL for defaults)
 * @return Generator or NULL on error
 */
visual_generator_t* visual_generator_create(const visual_generator_config_t* config);

/**
 * @brief Destroy visual generator
 *
 * @param gen Generator to destroy
 */
void visual_generator_destroy(visual_generator_t* gen);

//=============================================================================
// Generation API
//=============================================================================

/**
 * @brief Generate image from request
 *
 * @param gen Generator
 * @param request Generation request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_generate(visual_generator_t* gen,
                    const visual_generation_request_t* request,
                    visual_generation_result_t* result);

/**
 * @brief Generate image with extended options
 *
 * @param gen Generator
 * @param request Extended request
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_generate_extended(visual_generator_t* gen,
                             const visual_generation_request_ext_t* request,
                             visual_generation_result_t* result);

/**
 * @brief Generate batch of images
 *
 * @param gen Generator
 * @param request Extended request (batch_size determines count)
 * @param result Output batch result
 * @return 0 on success, -1 on error
 */
int visual_generate_batch(visual_generator_t* gen,
                          const visual_generation_request_ext_t* request,
                          visual_batch_result_t* result);

//=============================================================================
// Image-to-Image API
//=============================================================================

/**
 * @brief Transform image with prompt
 *
 * @param gen Generator
 * @param input Input image
 * @param prompt Transformation prompt
 * @param strength [0-1] How much to transform (1.0 = ignore input)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_img2img(visual_generator_t* gen,
                   const visual_image_t* input,
                   const char* prompt,
                   float strength,
                   visual_generation_result_t* result);

/**
 * @brief Inpaint masked region
 *
 * @param gen Generator
 * @param image Original image
 * @param mask Binary mask (white = generate)
 * @param prompt What to generate in masked area
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_inpaint(visual_generator_t* gen,
                   const visual_image_t* image,
                   const visual_image_t* mask,
                   const char* prompt,
                   visual_generation_result_t* result);

/**
 * @brief Outpaint (extend image beyond borders)
 *
 * @param gen Generator
 * @param image Original image
 * @param direction "left", "right", "up", "down", or "all"
 * @param pixels Pixels to extend
 * @param prompt Continuation prompt
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_outpaint(visual_generator_t* gen,
                    const visual_image_t* image,
                    const char* direction,
                    uint32_t pixels,
                    const char* prompt,
                    visual_generation_result_t* result);

//=============================================================================
// Style Transfer API
//=============================================================================

/**
 * @brief Apply style transfer
 *
 * @param gen Generator
 * @param content Content image
 * @param style_source Style source image
 * @param strength [0-1] Style strength
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_style_transfer(visual_generator_t* gen,
                          const visual_image_t* content,
                          const visual_image_t* style_source,
                          float strength,
                          visual_generation_result_t* result);

/**
 * @brief Apply archetype style
 *
 * @param gen Generator
 * @param content Content image
 * @param archetype Visual style archetype
 * @param strength [0-1] Style strength
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_apply_archetype(visual_generator_t* gen,
                           const visual_image_t* content,
                           visual_style_archetype_t archetype,
                           float strength,
                           visual_generation_result_t* result);

//=============================================================================
// Enhancement API
//=============================================================================

/**
 * @brief Upscale image
 *
 * @param gen Generator
 * @param image Input image
 * @param factor Upscale factor (2, 4)
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_upscale(visual_generator_t* gen,
                   const visual_image_t* image,
                   uint32_t factor,
                   visual_generation_result_t* result);

/**
 * @brief Refine/enhance image
 *
 * @param gen Generator
 * @param image Input image
 * @param prompt Refinement prompt
 * @param strength [0-1] Refinement strength
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int visual_refine(visual_generator_t* gen,
                  const visual_image_t* image,
                  const char* prompt,
                  float strength,
                  visual_generation_result_t* result);

//=============================================================================
// Export API
//=============================================================================

/**
 * @brief Export image to file
 *
 * @param image Image to export
 * @param path Output path
 * @param format Format ("png", "jpg", "bmp", "webp")
 * @return 0 on success, -1 on error
 */
int visual_export_image(const visual_image_t* image,
                        const char* path,
                        const char* format);

/**
 * @brief Export with metadata
 *
 * @param result Generation result
 * @param path Output path
 * @param format Format
 * @param include_metadata Include generation metadata
 * @return 0 on success, -1 on error
 */
int visual_export_with_metadata(const visual_generation_result_t* result,
                                const char* path,
                                const char* format,
                                bool include_metadata);

//=============================================================================
// Cortical Integration API
//=============================================================================

/**
 * @brief Set visual cortex for style feedback
 *
 * @param gen Generator
 * @param visual_cortex Visual cortex pointer (V1-IT features)
 */
void visual_generator_set_visual_cortex(visual_generator_t* gen, void* visual_cortex);

/**
 * @brief Set cortical columns for hierarchical feature processing
 *
 * @param gen Generator
 * @param cortical_columns Cortical columns pointer
 */
void visual_generator_set_cortical_columns(visual_generator_t* gen, void* cortical_columns);

//=============================================================================
// Cleanup
//=============================================================================

/**
 * @brief Free visual image
 *
 * @param image Image to free
 */
void visual_image_free(visual_image_t* image);

/**
 * @brief Free batch result
 *
 * @param result Batch result to free
 */
void visual_batch_result_free(visual_batch_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_GENERATION_H */

//=============================================================================
// nimcp_visual_generation.c - Creative Visual Generation Implementation
//=============================================================================
/**
 * @file nimcp_visual_generation.c
 * @brief Implements visual art generation via diffusion/GAN models
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/generation/nimcp_visual_generation.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define LOG_MODULE "VISUAL_GEN"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(visual_generation, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Configuration Defaults
//=============================================================================

void visual_generator_config_defaults(visual_generator_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(visual_generator_config_t));

    /* Model settings */
    strncpy(config->model_dir, "/models/diffusion", sizeof(config->model_dir) - 1);
    config->default_model = DIFFUSION_MODEL_SDXL;
    config->use_gpu = true;
    config->gpu_device_id = 0;
    config->use_tensorrt = false;

    /* Default generation settings */
    config->default_width = 1024;
    config->default_height = 1024;
    config->default_steps = 30;
    config->default_guidance = 7.5f;
    config->default_sampler = SAMPLER_EULER_A;

    /* Quality settings */
    config->enable_self_evaluation = true;
    config->min_quality_threshold = 0.6f;
    config->max_regeneration_attempts = 3;

    /* Resource limits */
    config->max_vram_bytes = 8ULL * 1024 * 1024 * 1024;  /* 8GB */
    config->enable_attention_slicing = true;
    config->enable_vae_tiling = false;
}

//=============================================================================
// Lifecycle
//=============================================================================

visual_generator_t* visual_generator_create(const visual_generator_config_t* config)
{
    visual_generator_t* gen = nimcp_calloc(1, sizeof(visual_generator_t));
    if (!gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_generator_create: gen is NULL");
        return NULL;
    }

    /* Apply config */
    if (config) {
        gen->config = *config;
    } else {
        visual_generator_config_defaults(&gen->config);
    }

    /* Initialize statistics */
    gen->images_generated = 0;
    gen->avg_quality_score = 0.0f;
    gen->avg_generation_time_ms = 0.0f;

    /* Models loaded on demand */
    gen->diffusion_pipeline = NULL;
    gen->gan_model = NULL;
    gen->style_transfer_model = NULL;
    gen->upscaler = NULL;
    gen->refiner = NULL;

    /* ControlNet models loaded on demand */
    gen->controlnet_canny = NULL;
    gen->controlnet_depth = NULL;
    gen->controlnet_pose = NULL;

    gen->current_style = NULL;
    gen->aesthetic_evaluator = NULL;
    gen->creative_bridge = NULL;

    return gen;
}

void visual_generator_destroy(visual_generator_t* gen)
{
    if (!gen) return;

    /* Free style embedding */
    if (gen->current_style) {
        if (gen->current_style->embedding) {
            nimcp_free(gen->current_style->embedding);
        }
        nimcp_free(gen->current_style);
    }

    /* Model cleanup would happen here in production */

    nimcp_free(gen);
    gen = NULL;
}

//=============================================================================
// Internal: Diffusion Pipeline
//=============================================================================

/**
 * @brief Generate noise for diffusion process
 */
static void generate_noise(float* noise, uint32_t width, uint32_t height,
                           uint32_t channels, uint64_t seed)
{
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    uint64_t state = seed;
    size_t total = (size_t)width * height * channels;

    /* Box-Muller transform for Gaussian noise */
    for (size_t i = 0; i < total; i += 2) {
        /* LCG random */
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        float u1 = (float)(state >> 33) / (float)(1ULL << 31);
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        float u2 = (float)(state >> 33) / (float)(1ULL << 31);

        u1 = fmaxf(u1, 1e-10f);
        float radius = sqrtf(-2.0f * logf(u1));
        float theta = NIMCP_TWO_PI_F * u2;

        noise[i] = radius * cosf(theta);
        if (i + 1 < total) {
            noise[i + 1] = radius * sinf(theta);
        }
    }
}

/**
 * @brief Simulate denoising step (placeholder for real model)
 */
static void denoise_step(float* latent, const float* noise_pred,
                         uint32_t width, uint32_t height, uint32_t channels,
                         float alpha, float sigma)
{
    size_t total = (size_t)width * height * channels;
    for (size_t i = 0; i < total; i++) {
        latent[i] = (latent[i] - sigma * noise_pred[i]) / alpha;
    }
}

/**
 * @brief Apply sampler scheduling
 */
static void get_sampler_schedule(visual_sampler_t sampler, uint32_t steps,
                                 float* alphas, float* sigmas)
{
    /* Simple linear schedule for placeholder */
    for (uint32_t i = 0; i < steps; i++) {
        float t = (steps > 1) ? (float)i / (float)(steps - 1) : 0.0f;
        alphas[i] = sqrtf(1.0f - t);
        sigmas[i] = sqrtf(t);
    }

    /* Adjust for ancestral samplers */
    if (sampler == SAMPLER_EULER_A || sampler == SAMPLER_DPM_2_A) {
        for (uint32_t i = 0; i < steps; i++) {
            sigmas[i] *= 1.1f;
        }
    }
}

/**
 * @brief Apply composition rule to generated image
 */
static void apply_composition(uint8_t* pixels, uint32_t width, uint32_t height,
                              composition_rule_t rule)
{
    /* Composition rules would adjust crop/focus in production */
    (void)pixels;
    (void)width;
    (void)height;
    (void)rule;
}

/**
 * @brief Apply color palette constraints
 */
static void apply_palette(uint8_t* pixels, uint32_t width, uint32_t height,
                          color_palette_t palette, const color_spec_t* custom,
                          uint32_t num_custom)
{
    (void)custom;
    (void)num_custom;

    if (palette == PALETTE_AUTO) return;

    size_t num_pixels = (size_t)width * height;

    for (size_t i = 0; i < num_pixels; i++) {
        uint8_t* p = &pixels[i * 3];

        switch (palette) {
            case PALETTE_WARM:
                /* Boost red, reduce blue */
                p[0] = (uint8_t)fminf(255.0f, p[0] * 1.1f);
                p[2] = (uint8_t)(p[2] * 0.9f);
                break;

            case PALETTE_COOL:
                /* Boost blue, reduce red */
                p[2] = (uint8_t)fminf(255.0f, p[2] * 1.1f);
                p[0] = (uint8_t)(p[0] * 0.9f);
                break;

            case PALETTE_MONOCHROME: {
                /* Convert to grayscale */
                uint8_t gray = (uint8_t)(0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]);
                p[0] = p[1] = p[2] = gray;
                break;
            }

            default:
                break;
        }
    }
}

//=============================================================================
// Generation API
//=============================================================================

int visual_generate(visual_generator_t* gen,
                    const visual_generation_request_t* request,
                    visual_generation_result_t* result)
{
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_generate: required parameter is NULL (gen, request, result)");
        return -1;
    }

    memset(result, 0, sizeof(visual_generation_result_t));

    /* Determine dimensions */
    uint32_t width = request->width > 0 ? request->width : gen->config.default_width;
    uint32_t height = request->height > 0 ? request->height : gen->config.default_height;

    /* Allocate image */
    result->image.width = width;
    result->image.height = height;
    result->image.channels = 3;
    result->image.pixels = nimcp_calloc((size_t)width * (size_t)height * 3, sizeof(uint8_t));
    if (!result->image.pixels) {
        result->success = false;
        strncpy(result->error_message, "Failed to allocate image buffer",
                sizeof(result->error_message) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_generate: result->image is NULL");
        return -1;
    }

    /* Generate using diffusion (placeholder simulation) */
    uint64_t seed = request->seed;
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }
    result->seed_used = seed;

    /* Latent space dimensions (1/8 of image for SD) */
    uint32_t latent_w = width / 8;
    uint32_t latent_h = height / 8;
    uint32_t latent_c = 4;

    float* latent = nimcp_calloc(latent_w * latent_h * latent_c, sizeof(float));
    float* noise = nimcp_calloc(latent_w * latent_h * latent_c, sizeof(float));

    if (!latent || !noise) {
        nimcp_free(latent);
        latent = NULL;
        nimcp_free(noise);
        noise = NULL;
        nimcp_free(result->image.pixels);
        result->image.pixels = NULL;
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_generate: required parameter is NULL (latent, noise)");
        return -1;
    }

    /* Initialize with noise */
    generate_noise(latent, latent_w, latent_h, latent_c, seed);

    /* Get sampler schedule */
    uint32_t steps = request->steps > 0 ? request->steps : gen->config.default_steps;
    float* alphas = nimcp_calloc(steps, sizeof(float));
    float* sigmas = nimcp_calloc(steps, sizeof(float));

    if (!alphas || !sigmas) {
        nimcp_free(latent);
        latent = NULL;
        nimcp_free(noise);
        noise = NULL;
        nimcp_free(alphas);
        alphas = NULL;
        nimcp_free(sigmas);
        sigmas = NULL;
        nimcp_free(result->image.pixels);
        result->image.pixels = NULL;
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_generate: required parameter is NULL (alphas, sigmas)");
        return -1;
    }

    visual_sampler_t sampler = gen->config.default_sampler;
    get_sampler_schedule(sampler, steps, alphas, sigmas);

    /* Simulate diffusion denoising steps */
    for (uint32_t step = 0; step < steps; step++) {
        /* In production: run UNet with text conditioning */
        generate_noise(noise, latent_w, latent_h, latent_c, seed + step);
        denoise_step(latent, noise, latent_w, latent_h, latent_c,
                     alphas[step], sigmas[step]);
    }

    /* Decode latent to pixels (placeholder: generate pattern) */
    uint64_t state = seed;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 3;

            /* Sample from latent with bilinear interpolation */
            float lx = (float)x / (float)width * latent_w;
            float ly = (float)y / (float)height * latent_h;

            uint32_t li = (uint32_t)ly * latent_w + (uint32_t)lx;
            li = li % (latent_w * latent_h * latent_c);

            /* Map latent values to colors */
            float val = latent[li];
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            float noise_val = (float)(state >> 40) / (float)(1ULL << 24) * 0.1f;

            /* Generate artistic patterns based on style */
            float pattern = sinf(x * 0.02f + val * 10.0f) *
                           cosf(y * 0.02f + val * 10.0f);
            pattern = pattern * 0.5f + 0.5f;

            result->image.pixels[idx + 0] = (uint8_t)(fminf(255.0f, fmaxf(0.0f,
                (pattern * 0.7f + noise_val + 0.15f) * 255.0f)));
            result->image.pixels[idx + 1] = (uint8_t)(fminf(255.0f, fmaxf(0.0f,
                (pattern * 0.6f + noise_val + 0.2f) * 255.0f)));
            result->image.pixels[idx + 2] = (uint8_t)(fminf(255.0f, fmaxf(0.0f,
                (pattern * 0.8f + noise_val + 0.1f) * 255.0f)));
        }
    }

    /* Cleanup */
    nimcp_free(latent);
    latent = NULL;
    nimcp_free(noise);
    noise = NULL;
    nimcp_free(alphas);
    alphas = NULL;
    nimcp_free(sigmas);
    sigmas = NULL;

    /* Evaluate quality */
    result->evaluation.overall_quality = 0.75f;  /* Placeholder */
    result->generation_time_ms = (float)steps * 10.0f;  /* Simulated timing */
    result->success = true;

    /* Update statistics */
    gen->images_generated++;
    float n = (float)gen->images_generated;
    gen->avg_quality_score = gen->avg_quality_score * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                             result->evaluation.overall_quality / (fabsf(n) > 1e-7f ? n : 1e-7f);
    gen->avg_generation_time_ms = gen->avg_generation_time_ms * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                                   result->generation_time_ms / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return 0;
}

int visual_generate_extended(visual_generator_t* gen,
                             const visual_generation_request_ext_t* request,
                             visual_generation_result_t* result)
{
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_generate_extended: required parameter is NULL (gen, request, result)");
        return -1;
    }

    /* Convert to basic request and call visual_generate */
    visual_generation_request_t basic_req;
    memset(&basic_req, 0, sizeof(basic_req));

    basic_req.prompt = request->prompt;
    basic_req.negative_prompt = request->negative_prompt;
    basic_req.width = request->width;
    basic_req.height = request->height;
    basic_req.steps = request->steps;
    basic_req.guidance_scale = request->guidance_scale;
    basic_req.seed = request->seed;
    basic_req.style = request->style;

    int rc = visual_generate(gen, &basic_req, result);
    if (rc != 0) return rc;

    /* Apply extended options */

    /* Apply composition rule */
    if (request->composition != COMPOSITION_RULE_NONE) {
        apply_composition(result->image.pixels, result->image.width,
                         result->image.height, request->composition);
    }

    /* Apply color palette */
    if (request->palette != PALETTE_AUTO) {
        apply_palette(result->image.pixels, result->image.width,
                     result->image.height, request->palette,
                     request->custom_colors, request->num_custom_colors);
    }

    /* Apply upscaling if requested */
    if (request->upscale && request->upscale_factor > 1.0f) {
        /* In production: call upscaler model */
    }

    /* Apply refinement if requested */
    if (request->refine) {
        /* In production: call refiner model */
    }

    return 0;
}

int visual_generate_batch(visual_generator_t* gen,
                          const visual_generation_request_ext_t* request,
                          visual_batch_result_t* result)
{
    if (!gen || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_generate_batch: required parameter is NULL (gen, request, result)");
        return -1;
    }

    uint32_t batch_size = request->batch_size > 0 ? request->batch_size : 1;
    if (batch_size > 16) batch_size = 16;

    result->results = nimcp_calloc(batch_size, sizeof(visual_generation_result_t));
    if (!result->results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_generate_batch: result->results is NULL");
        return -1;
    }

    result->num_results = batch_size;
    result->total_time_ms = 0.0f;

    uint64_t seed = request->seed;
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    for (uint32_t i = 0; i < batch_size; i++) {
        visual_generation_request_ext_t req_copy = *request;
        req_copy.seed = request->vary_seed ? seed + i : seed;

        int rc = visual_generate_extended(gen, &req_copy, &result->results[i]);
        if (rc != 0) {
            result->results[i].success = false;
        }

        result->seeds_used[i] = result->results[i].seed_used;
        result->total_time_ms += result->results[i].generation_time_ms;
    }

    return 0;
}

//=============================================================================
// Image-to-Image API
//=============================================================================

int visual_img2img(visual_generator_t* gen,
                   const visual_image_t* input,
                   const char* prompt,
                   float strength,
                   visual_generation_result_t* result)
{
    if (!gen || !input || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_img2img: required parameter is NULL (gen, input, result)");
        return -1;
    }

    memset(result, 0, sizeof(visual_generation_result_t));

    /* Allocate output image */
    result->image.width = input->width;
    result->image.height = input->height;
    result->image.channels = input->channels;

    size_t pixel_count = (size_t)input->width * input->height * input->channels;
    result->image.pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
    if (!result->image.pixels) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_img2img: result->image is NULL");
        return -1;
    }

    /* Blend input with generated content based on strength */
    strength = fmaxf(0.0f, fminf(1.0f, strength));

    /* Generate base pattern with seed from input */
    uint64_t seed = 0;
    for (size_t i = 0; i < fminf(pixel_count, 1000); i++) {
        seed = seed * 31 + input->pixels[i];
    }

    uint64_t state = seed;
    for (size_t i = 0; i < pixel_count; i++) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        float noise = (float)(state >> 40) / (float)(1ULL << 24);

        float input_val = (float)input->pixels[i] / 255.0f;
        float gen_val = noise;

        /* Blend based on strength */
        float blended = input_val * (1.0f - strength) + gen_val * strength;
        result->image.pixels[i] = (uint8_t)(blended * 255.0f);
    }

    result->seed_used = seed;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = 200.0f;
    result->success = true;

    (void)prompt;  /* Used in production for conditioning */

    return 0;
}

int visual_inpaint(visual_generator_t* gen,
                   const visual_image_t* image,
                   const visual_image_t* mask,
                   const char* prompt,
                   visual_generation_result_t* result)
{
    if (!gen || !image || !mask || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_inpaint: required parameter is NULL (gen, image, mask, result)");
        return -1;
    }
    if (image->width != mask->width || image->height != mask->height) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_inpaint: validation failed");
        return -1;
    }

    memset(result, 0, sizeof(visual_generation_result_t));

    /* Copy source image */
    result->image.width = image->width;
    result->image.height = image->height;
    result->image.channels = image->channels;

    size_t pixel_count = (size_t)image->width * image->height * image->channels;
    result->image.pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
    if (!result->image.pixels) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_inpaint: result->image is NULL");
        return -1;
    }

    memcpy(result->image.pixels, image->pixels, pixel_count);

    /* Generate content for masked regions */
    uint64_t state = (uint64_t)time(NULL);

    for (uint32_t y = 0; y < image->height; y++) {
        for (uint32_t x = 0; x < image->width; x++) {
            size_t mask_idx = y * mask->width + x;

            /* Check if pixel is masked (white = generate) */
            bool is_masked = mask->pixels[mask_idx * mask->channels] > 127;

            if (is_masked) {
                size_t idx = (y * image->width + x) * image->channels;

                /* Generate content for this pixel */
                state = state * 6364136223846793005ULL + 1442695040888963407ULL;

                /* Sample from surrounding non-masked pixels for coherence */
                uint32_t sum_r = 0, sum_g = 0, sum_b = 0;
                uint32_t count = 0;

                for (int dy = -3; dy <= 3; dy++) {
                    for (int dx = -3; dx <= 3; dx++) {
                        int nx = (int)x + dx;
                        int ny = (int)y + dy;
                        if (nx < 0 || nx >= (int)image->width ||
                            ny < 0 || ny >= (int)image->height) continue;

                        size_t ni = (uint32_t)ny * mask->width + (uint32_t)nx;
                        if (mask->pixels[ni * mask->channels] <= 127) {
                            size_t pi = (ny * image->width + nx) * image->channels;
                            sum_r += image->pixels[pi + 0];
                            sum_g += image->pixels[pi + 1];
                            sum_b += image->pixels[pi + 2];
                            count++;
                        }
                    }
                }

                if (count > 0) {
                    /* Use surrounding colors with variation */
                    float noise = (float)(state >> 40) / (float)(1ULL << 24) * 0.2f - 0.1f;
                    result->image.pixels[idx + 0] = (uint8_t)fminf(255.0f,
                        fmaxf(0.0f, sum_r / count + noise * 255.0f));
                    result->image.pixels[idx + 1] = (uint8_t)fminf(255.0f,
                        fmaxf(0.0f, sum_g / count + noise * 255.0f));
                    if (image->channels > 2) {
                        result->image.pixels[idx + 2] = (uint8_t)fminf(255.0f,
                            fmaxf(0.0f, sum_b / count + noise * 255.0f));
                    }
                } else {
                    /* Random fill */
                    result->image.pixels[idx + 0] = (uint8_t)(state >> 48);
                    result->image.pixels[idx + 1] = (uint8_t)(state >> 40);
                    if (image->channels > 2) {
                        result->image.pixels[idx + 2] = (uint8_t)(state >> 32);
                    }
                }
            }
        }
    }

    result->seed_used = state;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = 300.0f;
    result->success = true;

    (void)prompt;

    return 0;
}

int visual_outpaint(visual_generator_t* gen,
                    const visual_image_t* image,
                    const char* direction,
                    uint32_t pixels,
                    const char* prompt,
                    visual_generation_result_t* result)
{
    if (!gen || !image || !direction || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_outpaint: required parameter is NULL (gen, image, direction, result)");
        return -1;
    }

    memset(result, 0, sizeof(visual_generation_result_t));

    /* Calculate new dimensions */
    uint32_t new_width = image->width;
    uint32_t new_height = image->height;
    uint32_t offset_x = 0, offset_y = 0;

    if (strcmp(direction, "left") == 0) {
        new_width += pixels;
        offset_x = pixels;
    } else if (strcmp(direction, "right") == 0) {
        new_width += pixels;
    } else if (strcmp(direction, "up") == 0) {
        new_height += pixels;
        offset_y = pixels;
    } else if (strcmp(direction, "down") == 0) {
        new_height += pixels;
    } else if (strcmp(direction, "all") == 0) {
        new_width += pixels * 2;
        new_height += pixels * 2;
        offset_x = offset_y = pixels;
    }

    /* Allocate new image */
    result->image.width = new_width;
    result->image.height = new_height;
    result->image.channels = image->channels;

    size_t pixel_count = (size_t)new_width * new_height * image->channels;
    result->image.pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
    if (!result->image.pixels) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_outpaint: result->image is NULL");
        return -1;
    }

    /* Copy original image to correct position */
    for (uint32_t y = 0; y < image->height; y++) {
        for (uint32_t x = 0; x < image->width; x++) {
            size_t src_idx = (y * image->width + x) * image->channels;
            size_t dst_idx = ((y + offset_y) * new_width + (x + offset_x)) *
                             image->channels;

            for (uint32_t c = 0; c < image->channels; c++) {
                result->image.pixels[dst_idx + c] = image->pixels[src_idx + c];
            }
        }
    }

    /* Generate content for extended regions */
    uint64_t state = (uint64_t)time(NULL);

    for (uint32_t y = 0; y < new_height; y++) {
        for (uint32_t x = 0; x < new_width; x++) {
            /* Check if this pixel is outside original */
            bool is_extended = (x < offset_x || x >= offset_x + image->width ||
                               y < offset_y || y >= offset_y + image->height);

            if (is_extended) {
                size_t idx = (y * new_width + x) * image->channels;

                /* Find nearest original pixel for continuation */
                uint32_t near_x = x < offset_x ? offset_x :
                                 (x >= offset_x + image->width ?
                                  offset_x + image->width - 1 : x);
                uint32_t near_y = y < offset_y ? offset_y :
                                 (y >= offset_y + image->height ?
                                  offset_y + image->height - 1 : y);

                size_t near_idx = (near_y * new_width + near_x) * image->channels;

                /* Fade from original with noise */
                state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                float noise = (float)(state >> 40) / (float)(1ULL << 24) * 0.3f;

                float dist = sqrtf((float)((int)x - (int)near_x) *
                                  ((int)x - (int)near_x) +
                                  ((int)y - (int)near_y) *
                                  ((int)y - (int)near_y));
                float fade = fminf(1.0f, dist / (float)pixels);

                for (uint32_t c = 0; c < image->channels; c++) {
                    float orig = (float)result->image.pixels[near_idx + c] / 255.0f;
                    float gen = orig * (1.0f - fade * 0.5f) + noise * fade;
                    result->image.pixels[idx + c] = (uint8_t)(fminf(255.0f,
                        fmaxf(0.0f, gen * 255.0f)));
                }
            }
        }
    }

    result->seed_used = state;
    result->evaluation.overall_quality = 0.65f;
    result->generation_time_ms = 400.0f;
    result->success = true;

    (void)prompt;

    return 0;
}

//=============================================================================
// Style Transfer API
//=============================================================================

int visual_style_transfer(visual_generator_t* gen,
                          const visual_image_t* content,
                          const visual_image_t* style_source,
                          float strength,
                          visual_generation_result_t* result)
{
    if (!gen || !content || !style_source || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_style_transfer: required parameter is NULL (gen, content, style_source, result)");
        return -1;
    }

    memset(result, 0, sizeof(visual_generation_result_t));

    /* Allocate output */
    result->image.width = content->width;
    result->image.height = content->height;
    result->image.channels = content->channels;

    size_t pixel_count = (size_t)content->width * content->height * content->channels;
    result->image.pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
    if (!result->image.pixels) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_style_transfer: result->image is NULL");
        return -1;
    }

    strength = fmaxf(0.0f, fminf(1.0f, strength));

    /* Compute style statistics from style image */
    float style_mean[3] = {0, 0, 0};
    float style_std[3] = {0, 0, 0};

    size_t style_pixels = (size_t)style_source->width * style_source->height;
    for (size_t i = 0; i < style_pixels; i++) {
        for (uint32_t c = 0; c < 3 && c < style_source->channels; c++) {
            style_mean[c] += style_source->pixels[i * style_source->channels + c];
        }
    }
    for (int c = 0; c < 3; c++) {
        style_mean[c] /= (float)style_pixels;
    }

    for (size_t i = 0; i < style_pixels; i++) {
        for (uint32_t c = 0; c < 3 && c < style_source->channels; c++) {
            float diff = style_source->pixels[i * style_source->channels + c] -
                        style_mean[c];
            style_std[c] += diff * diff;
        }
    }
    for (int c = 0; c < 3; c++) {
        style_std[c] = sqrtf(style_std[c] / (float)style_pixels);
    }

    /* Compute content statistics */
    float content_mean[3] = {0, 0, 0};
    float content_std[3] = {0, 0, 0};

    size_t content_pixels = (size_t)content->width * content->height;
    for (size_t i = 0; i < content_pixels; i++) {
        for (uint32_t c = 0; c < 3 && c < content->channels; c++) {
            content_mean[c] += content->pixels[i * content->channels + c];
        }
    }
    for (int c = 0; c < 3; c++) {
        content_mean[c] /= (float)content_pixels;
    }

    for (size_t i = 0; i < content_pixels; i++) {
        for (uint32_t c = 0; c < 3 && c < content->channels; c++) {
            float diff = content->pixels[i * content->channels + c] - content_mean[c];
            content_std[c] += diff * diff;
        }
    }
    for (int c = 0; c < 3; c++) {
        content_std[c] = sqrtf(content_std[c] / (float)content_pixels);
        if (content_std[c] < 1.0f) content_std[c] = 1.0f;
    }

    /* Apply style transfer via histogram matching */
    for (size_t i = 0; i < content_pixels; i++) {
        for (uint32_t c = 0; c < content->channels; c++) {
            float val = (float)content->pixels[i * content->channels + c];

            if (c < 3) {
                /* Normalize, apply style statistics, denormalize */
                float normalized = (val - content_mean[c]) / content_std[c];
                float styled = normalized * style_std[c] + style_mean[c];

                /* Blend based on strength */
                float final_val = val * (1.0f - strength) + styled * strength;
                result->image.pixels[i * content->channels + c] =
                    (uint8_t)fminf(255.0f, fmaxf(0.0f, final_val));
            } else {
                result->image.pixels[i * content->channels + c] =
                    content->pixels[i * content->channels + c];
            }
        }
    }

    result->seed_used = 0;
    result->evaluation.overall_quality = 0.75f;
    result->generation_time_ms = 150.0f;
    result->success = true;

    return 0;
}

int visual_apply_archetype(visual_generator_t* gen,
                           const visual_image_t* content,
                           visual_style_archetype_t archetype,
                           float strength,
                           visual_generation_result_t* result)
{
    if (!gen || !content || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_apply_archetype: required parameter is NULL (gen, content, result)");
        return -1;
    }
    if ((int)archetype < 0 || archetype >= STYLE_VIS_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "visual_apply_archetype: capacity exceeded");
        return -1;
    }

    memset(result, 0, sizeof(visual_generation_result_t));

    /* Allocate output */
    result->image.width = content->width;
    result->image.height = content->height;
    result->image.channels = content->channels;

    size_t pixel_count = (size_t)content->width * content->height * content->channels;
    result->image.pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
    if (!result->image.pixels) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_apply_archetype: result->image is NULL");
        return -1;
    }

    strength = fmaxf(0.0f, fminf(1.0f, strength));

    /* Archetype-specific color transforms */
    typedef struct {
        float hue_shift;
        float saturation_mult;
        float brightness_mult;
        float contrast_mult;
    } archetype_transform_t;

    static const archetype_transform_t transforms[STYLE_VIS_COUNT] = {
        [STYLE_VIS_VAN_GOGH]   = { 30.0f,  1.3f,  0.9f,  1.2f },
        [STYLE_VIS_MONET]      = { 10.0f,  0.9f,  1.1f,  0.9f },
        [STYLE_VIS_PICASSO]    = { 0.0f,   1.1f,  1.0f,  1.4f },
        [STYLE_VIS_DALI]       = { -20.0f, 1.2f,  0.95f, 1.3f },
        [STYLE_VIS_WARHOL]     = { 45.0f,  1.5f,  1.1f,  1.4f },
        [STYLE_VIS_REMBRANDT]  = { -10.0f, 0.7f,  0.8f,  1.3f },
        [STYLE_VIS_KLIMT]      = { 35.0f,  1.1f,  1.05f, 1.1f },
        [STYLE_VIS_ESCHER]     = { 0.0f,   1.0f,  1.0f,  1.3f },
        [STYLE_VIS_HOKUSAI]    = { -5.0f,  0.85f, 1.0f,  1.15f },
        [STYLE_VIS_BASQUIAT]   = { 20.0f,  1.4f,  1.05f, 1.5f },
        [STYLE_VIS_CARAVAGGIO] = { 5.0f,   0.8f,  0.9f,  1.1f },
        [STYLE_VIS_KANDINSKY]  = { 60.0f,  1.2f,  1.0f,  1.2f },
    };

    const archetype_transform_t* xform = &transforms[archetype];

    /* Apply transform */
    size_t num_pixels = (size_t)content->width * content->height;
    for (size_t i = 0; i < num_pixels; i++) {
        uint8_t* src = &content->pixels[i * content->channels];
        uint8_t* dst = &result->image.pixels[i * content->channels];

        if (content->channels >= 3) {
            /* Convert RGB to HSV */
            float r = src[0] / 255.0f;
            float g = src[1] / 255.0f;
            float b = src[2] / 255.0f;

            float max_val = fmaxf(r, fmaxf(g, b));
            float min_val = fminf(r, fminf(g, b));
            float delta = max_val - min_val;

            float h = 0, s = 0, v = max_val;

            if (delta > 0.00001f) {
                s = delta / max_val;
                if (r >= max_val) {
                    h = (g - b) / delta;
                } else if (g >= max_val) {
                    h = 2.0f + (b - r) / delta;
                } else {
                    h = 4.0f + (r - g) / delta;
                }
                h *= 60.0f;
                if (h < 0) h += 360.0f;
            }

            /* Apply archetype transform with strength blending */
            h = h + xform->hue_shift * strength;
            if (h >= 360.0f) h -= 360.0f;
            if (h < 0) h += 360.0f;

            s = s * (1.0f + (xform->saturation_mult - 1.0f) * strength);
            s = fminf(1.0f, fmaxf(0.0f, s));

            v = v * (1.0f + (xform->brightness_mult - 1.0f) * strength);
            v = fminf(1.0f, fmaxf(0.0f, v));

            /* Apply contrast */
            v = ((v - 0.5f) * (1.0f + (xform->contrast_mult - 1.0f) * strength)) + 0.5f;
            v = fminf(1.0f, fmaxf(0.0f, v));

            /* Convert back to RGB */
            float c = v * s;
            float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
            float m = v - c;

            float r_out, g_out, b_out;
            if (h < 60) {
                r_out = c; g_out = x; b_out = 0;
            } else if (h < 120) {
                r_out = x; g_out = c; b_out = 0;
            } else if (h < 180) {
                r_out = 0; g_out = c; b_out = x;
            } else if (h < 240) {
                r_out = 0; g_out = x; b_out = c;
            } else if (h < 300) {
                r_out = x; g_out = 0; b_out = c;
            } else {
                r_out = c; g_out = 0; b_out = x;
            }

            dst[0] = (uint8_t)fminf(255.0f, (r_out + m) * 255.0f);
            dst[1] = (uint8_t)fminf(255.0f, (g_out + m) * 255.0f);
            dst[2] = (uint8_t)fminf(255.0f, (b_out + m) * 255.0f);
        }

        /* Copy alpha if present */
        if (content->channels == 4) {
            dst[3] = src[3];
        }
    }

    result->seed_used = 0;
    result->evaluation.overall_quality = 0.8f;
    result->generation_time_ms = 100.0f;
    result->success = true;

    return 0;
}

//=============================================================================
// Enhancement API
//=============================================================================

int visual_upscale(visual_generator_t* gen,
                   const visual_image_t* image,
                   uint32_t factor,
                   visual_generation_result_t* result)
{
    if (!gen || !image || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_upscale: required parameter is NULL (gen, image, result)");
        return -1;
    }
    if (factor < 2 || factor > 4) factor = 2;

    memset(result, 0, sizeof(visual_generation_result_t));

    uint32_t new_width = image->width * factor;
    uint32_t new_height = image->height * factor;

    result->image.width = new_width;
    result->image.height = new_height;
    result->image.channels = image->channels;

    size_t pixel_count = (size_t)new_width * new_height * image->channels;
    result->image.pixels = nimcp_calloc(pixel_count, sizeof(uint8_t));
    if (!result->image.pixels) {
        result->success = false;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "visual_upscale: result->image is NULL");
        return -1;
    }

    /* Bilinear interpolation upscale */
    for (uint32_t y = 0; y < new_height; y++) {
        for (uint32_t x = 0; x < new_width; x++) {
            float src_x = (float)x / (float)factor;
            float src_y = (float)y / (float)factor;

            uint32_t x0 = (uint32_t)src_x;
            uint32_t y0 = (uint32_t)src_y;
            uint32_t x1 = fminf(x0 + 1, image->width - 1);
            uint32_t y1 = fminf(y0 + 1, image->height - 1);

            float fx = src_x - x0;
            float fy = src_y - y0;

            size_t dst_idx = (y * new_width + x) * image->channels;

            for (uint32_t c = 0; c < image->channels; c++) {
                float p00 = image->pixels[(y0 * image->width + x0) * image->channels + c];
                float p10 = image->pixels[(y0 * image->width + x1) * image->channels + c];
                float p01 = image->pixels[(y1 * image->width + x0) * image->channels + c];
                float p11 = image->pixels[(y1 * image->width + x1) * image->channels + c];

                float val = p00 * (1-fx) * (1-fy) + p10 * fx * (1-fy) +
                           p01 * (1-fx) * fy + p11 * fx * fy;

                result->image.pixels[dst_idx + c] = (uint8_t)fminf(255.0f, val);
            }
        }
    }

    result->seed_used = 0;
    result->evaluation.overall_quality = 0.7f;
    result->generation_time_ms = 50.0f * factor;
    result->success = true;

    return 0;
}

int visual_refine(visual_generator_t* gen,
                  const visual_image_t* image,
                  const char* prompt,
                  float strength,
                  visual_generation_result_t* result)
{
    /* Refinement is a light img2img pass */
    return visual_img2img(gen, image, prompt, strength * 0.3f, result);
}

//=============================================================================
// Export API
//=============================================================================

int visual_export_image(const visual_image_t* image,
                        const char* path,
                        const char* format)
{
    if (!image || !path || !format) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_export_image: required parameter is NULL (image, path, format)");
        return -1;
    }
    if (!image->pixels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_export_image: image->pixels is NULL");
        return -1;
    }

    /* Placeholder: would use stb_image_write or similar in production */
    FILE* fp = fopen(path, "wb");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_export_image: fp is NULL");
        return -1;
    }

    /* Write simple PPM format for now */
    if (strcmp(format, "ppm") == 0 || strcmp(format, "png") == 0) {
        fprintf(fp, "P6\n%u %u\n255\n", image->width, image->height);

        if (image->channels == 3) {
            fwrite(image->pixels, 1, image->width * image->height * 3, fp);
        } else if (image->channels == 4) {
            /* Strip alpha */
            for (uint32_t i = 0; i < image->width * image->height; i++) {
                fwrite(&image->pixels[i * 4], 1, 3, fp);
            }
        } else if (image->channels == 1) {
            /* Grayscale to RGB */
            for (uint32_t i = 0; i < image->width * image->height; i++) {
                uint8_t gray = image->pixels[i];
                uint8_t rgb[3] = {gray, gray, gray};
                fwrite(rgb, 1, 3, fp);
            }
        }
    }

    fclose(fp);
    return 0;
}

int visual_export_with_metadata(const visual_generation_result_t* result,
                                const char* path,
                                const char* format,
                                bool include_metadata)
{
    if (!result || !path || !format) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "visual_export_with_metadata: required parameter is NULL (result, path, format)");
        return -1;
    }

    int rc = visual_export_image(&result->image, path, format);
    if (rc != 0) return rc;

    if (include_metadata) {
        /* Write sidecar metadata file */
        char meta_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(meta_path, sizeof(meta_path), "%s.json", path);

        FILE* fp = fopen(meta_path, "w");
        if (fp) {
            fprintf(fp, "{\n");
            fprintf(fp, "  \"seed\": %lu,\n", (unsigned long)result->seed_used);
            fprintf(fp, "  \"width\": %u,\n", result->image.width);
            fprintf(fp, "  \"height\": %u,\n", result->image.height);
            fprintf(fp, "  \"quality_score\": %.3f,\n", result->evaluation.overall_quality);
            fprintf(fp, "  \"generation_time_ms\": %.1f\n", result->generation_time_ms);
            fprintf(fp, "}\n");
            fclose(fp);
        }
    }

    return 0;
}

//=============================================================================
// Cortical Integration API
//=============================================================================

void visual_generator_set_visual_cortex(visual_generator_t* gen, void* visual_cortex) {
    if (!gen) return;
    gen->visual_cortex = visual_cortex;
}

void visual_generator_set_cortical_columns(visual_generator_t* gen, void* cortical_columns) {
    if (!gen) return;
    gen->cortical_columns = cortical_columns;
}

//=============================================================================
// Cleanup
//=============================================================================

void visual_image_free(visual_image_t* image)
{
    if (!image) return;

    if (image->pixels) {
        nimcp_free(image->pixels);
        image->pixels = NULL;
    }

    image->width = 0;
    image->height = 0;
    image->channels = 0;
}

void visual_batch_result_free(visual_batch_result_t* result)
{
    if (!result) return;

    if (result->results) {
        for (uint32_t i = 0; i < result->num_results; i++) {
            visual_image_free(&result->results[i].image);
        }
        nimcp_free(result->results);
        result->results = NULL;
    }

    result->num_results = 0;
}

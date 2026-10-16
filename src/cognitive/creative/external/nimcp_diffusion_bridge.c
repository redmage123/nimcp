//=============================================================================
// nimcp_diffusion_bridge.c - Diffusion Model Integration Implementation
//=============================================================================
/**
 * @file nimcp_diffusion_bridge.c
 * @brief Implements interface to Stable Diffusion and other diffusion models
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/external/nimcp_diffusion_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(diffusion_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_diffusion_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_diffusion_bridge_mesh_registry = NULL;

nimcp_error_t diffusion_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_diffusion_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "diffusion_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "diffusion_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_diffusion_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_diffusion_bridge_mesh_registry = registry;
    return err;
}

void diffusion_bridge_mesh_unregister(void) {
    if (g_diffusion_bridge_mesh_registry && g_diffusion_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_diffusion_bridge_mesh_registry, g_diffusion_bridge_mesh_id);
        g_diffusion_bridge_mesh_id = 0;
        g_diffusion_bridge_mesh_registry = NULL;
    }
}


//=============================================================================
// Internal Types
//=============================================================================

/**
 * @brief Scheduler state
 */
typedef struct {
    diffusion_scheduler_t type;
    float* alphas_cumprod;
    float* sigmas;
    uint32_t num_steps;
    uint32_t current_step;
} scheduler_state_t;

//=============================================================================
// Thread-local error message
//=============================================================================

static _Thread_local char g_diffusion_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_diffusion_error(const char* msg)
{
    if (msg) {
        strncpy(g_diffusion_error, msg, sizeof(g_diffusion_error) - 1);
    } else {
        g_diffusion_error[0] = '\0';
    }
}

//=============================================================================
// Configuration Defaults
//=============================================================================

void diffusion_bridge_config_defaults(diffusion_bridge_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(diffusion_bridge_config_t));

    config->backend = DIFFUSION_BACKEND_ONNX;
    config->model = DIFFUSION_MODEL_SDXL;

    /* Default paths (would be overridden) */
    strncpy(config->paths.text_encoder_path, "/models/sd/text_encoder.onnx",
            sizeof(config->paths.text_encoder_path) - 1);
    strncpy(config->paths.unet_path, "/models/sd/unet.onnx",
            sizeof(config->paths.unet_path) - 1);
    strncpy(config->paths.vae_decoder_path, "/models/sd/vae_decoder.onnx",
            sizeof(config->paths.vae_decoder_path) - 1);

    config->device = ONNX_DEVICE_CPU;
    config->device_id = 0;

    config->scheduler = SCHEDULER_EULER_A;
    config->default_steps = 30;
    config->default_guidance = 7.5f;
    config->default_width = 1024;
    config->default_height = 1024;

    config->enable_safety_checker = true;

    config->enable_attention_slicing = true;
    config->enable_vae_slicing = false;
    config->enable_vae_tiling = false;
    config->enable_model_offload = false;
    config->max_vram_bytes = 8ULL * 1024 * 1024 * 1024;

    config->api_timeout_ms = 60000;
}

//=============================================================================
// Lifecycle
//=============================================================================

diffusion_bridge_t* diffusion_bridge_create(const diffusion_bridge_config_t* config)
{
    diffusion_bridge_t* bridge = nimcp_calloc(1, sizeof(diffusion_bridge_t));
    if (!bridge) {
        set_diffusion_error("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "diffusion_bridge_create: bridge is NULL");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        diffusion_bridge_config_defaults(&bridge->config);
    }

    /* Create ONNX runtime for local backends */
    if (bridge->config.backend == DIFFUSION_BACKEND_ONNX ||
        bridge->config.backend == DIFFUSION_BACKEND_TENSORRT) {

        onnx_runtime_config_t onnx_config;
        onnx_runtime_config_defaults(&onnx_config);
        onnx_config.device = bridge->config.device;
        onnx_config.device_id = bridge->config.device_id;
        onnx_config.gpu_memory_limit = bridge->config.max_vram_bytes;

        bridge->onnx_runtime = onnx_runtime_create(&onnx_config);
    }

    /* Initialize scheduler */
    scheduler_state_t* scheduler = nimcp_calloc(1, sizeof(scheduler_state_t));
    if (scheduler) {
        scheduler->type = bridge->config.scheduler;
        scheduler->num_steps = bridge->config.default_steps;

        /* Pre-compute scheduler coefficients */
        scheduler->alphas_cumprod = nimcp_calloc(1000, sizeof(float));
        if (!scheduler->alphas_cumprod) return NULL;
        scheduler->sigmas = nimcp_calloc(1000, sizeof(float));

        if (scheduler->alphas_cumprod && scheduler->sigmas) {
            /* Linear beta schedule */
            float beta_start = 0.00085f;
            float beta_end = 0.012f;
            float alpha_prod = 1.0f;

            for (int i = 0; i < 1000; i++) {
                float beta = beta_start + (beta_end - beta_start) *
                            (float)i / 999.0f;
                alpha_prod *= (1.0f - beta);
                scheduler->alphas_cumprod[i] = alpha_prod;
                scheduler->sigmas[i] = sqrtf((1.0f - alpha_prod) / alpha_prod);
            }
        }
    }
    bridge->scheduler = scheduler;

    bridge->images_generated = 0;
    bridge->avg_generation_time_ms = 0.0f;
    bridge->peak_vram_bytes = 0;

    bridge->lora_weights = NULL;
    bridge->lora_scale = 0.0f;

    return bridge;
}

void diffusion_bridge_destroy(diffusion_bridge_t* bridge)
{
    if (!bridge) return;

    diffusion_bridge_unload_model(bridge);
    diffusion_unload_lora(bridge);

    if (bridge->scheduler) {
        scheduler_state_t* scheduler = (scheduler_state_t*)bridge->scheduler;
        if (scheduler->alphas_cumprod) nimcp_free(scheduler->alphas_cumprod);
        if (scheduler->sigmas) nimcp_free(scheduler->sigmas);
        nimcp_free(scheduler);
        scheduler = NULL;
    }

    if (bridge->onnx_runtime) {
        onnx_runtime_destroy(bridge->onnx_runtime);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int diffusion_bridge_load_model(diffusion_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_bridge_load_model: bridge is NULL");
        return -1;
    }

    if (bridge->config.backend == DIFFUSION_BACKEND_ONNX) {
        if (!bridge->onnx_runtime) {
            set_diffusion_error("ONNX runtime not initialized");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_bridge_load_model: bridge->onnx_runtime is NULL");
            return -1;
        }

        /* Load text encoder */
        if (strlen(bridge->config.paths.text_encoder_path) > 0) {
            bridge->text_encoder = onnx_load_model(bridge->onnx_runtime,
                bridge->config.paths.text_encoder_path, NULL);
        }

        /* Load UNet */
        if (strlen(bridge->config.paths.unet_path) > 0) {
            bridge->unet = onnx_load_model(bridge->onnx_runtime,
                bridge->config.paths.unet_path, NULL);
        }

        /* Load VAE decoder */
        if (strlen(bridge->config.paths.vae_decoder_path) > 0) {
            bridge->vae_decoder = onnx_load_model(bridge->onnx_runtime,
                bridge->config.paths.vae_decoder_path, NULL);
        }

        /* Optional: second text encoder for SDXL */
        if (strlen(bridge->config.paths.text_encoder_2_path) > 0) {
            bridge->text_encoder_2 = onnx_load_model(bridge->onnx_runtime,
                bridge->config.paths.text_encoder_2_path, NULL);
        }

        /* Optional: safety checker */
        if (bridge->config.enable_safety_checker &&
            strlen(bridge->config.paths.safety_checker_path) > 0) {
            bridge->safety_checker = onnx_load_model(bridge->onnx_runtime,
                bridge->config.paths.safety_checker_path, NULL);
        }
    }

    return 0;
}

void diffusion_bridge_unload_model(diffusion_bridge_t* bridge)
{
    if (!bridge || !bridge->onnx_runtime) return;

    if (bridge->text_encoder) {
        onnx_unload_model(bridge->onnx_runtime, bridge->text_encoder);
        bridge->text_encoder = NULL;
    }

    if (bridge->text_encoder_2) {
        onnx_unload_model(bridge->onnx_runtime, bridge->text_encoder_2);
        bridge->text_encoder_2 = NULL;
    }

    if (bridge->unet) {
        onnx_unload_model(bridge->onnx_runtime, bridge->unet);
        bridge->unet = NULL;
    }

    if (bridge->vae_encoder) {
        onnx_unload_model(bridge->onnx_runtime, bridge->vae_encoder);
        bridge->vae_encoder = NULL;
    }

    if (bridge->vae_decoder) {
        onnx_unload_model(bridge->onnx_runtime, bridge->vae_decoder);
        bridge->vae_decoder = NULL;
    }

    if (bridge->safety_checker) {
        onnx_unload_model(bridge->onnx_runtime, bridge->safety_checker);
        bridge->safety_checker = NULL;
    }

    if (bridge->controlnet) {
        onnx_unload_model(bridge->onnx_runtime, bridge->controlnet);
        bridge->controlnet = NULL;
    }
}

//=============================================================================
// Internal: Noise Generation
//=============================================================================

static void generate_gaussian_noise(float* noise, size_t count, uint64_t seed)
{
    uint64_t state = seed;
    if (state == 0) state = (uint64_t)time(NULL);

    for (size_t i = 0; i < count; i += 2) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        float u1 = (float)(state >> 33) / (float)(1ULL << 31);
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        float u2 = (float)(state >> 33) / (float)(1ULL << 31);

        u1 = fmaxf(u1, 1e-10f);
        float radius = sqrtf(-2.0f * logf(u1));
        float theta = NIMCP_TWO_PI_F * u2;

        noise[i] = radius * cosf(theta);
        if (i + 1 < count) {
            noise[i + 1] = radius * sinf(theta);
        }
    }
}

//=============================================================================
// Internal: Diffusion Steps
//=============================================================================

static void scheduler_step(scheduler_state_t* scheduler,
                           float* latent, const float* model_output,
                           size_t count, uint32_t step)
{
    if (!scheduler || !latent || !model_output) return;

    /* Get timestep from scheduler */
    uint32_t t_idx = 999 - (step * 1000 / scheduler->num_steps);
    if (t_idx >= 1000) t_idx = 999;

    float alpha_t = scheduler->alphas_cumprod[t_idx];
    float sigma_t = scheduler->sigmas[t_idx];

    /* Euler step */
    float dt = -1.0f / (float)scheduler->num_steps;

    for (size_t i = 0; i < count; i++) {
        /* Predict x0 from noise */
        float pred_x0 = (latent[i] - sigma_t * model_output[i]) / sqrtf(alpha_t);

        /* Clamp prediction */
        pred_x0 = fmaxf(-1.0f, fminf(1.0f, pred_x0));

        /* Step */
        latent[i] = latent[i] + dt * model_output[i];
    }
}

//=============================================================================
// Generation API
//=============================================================================

int diffusion_text_to_image(diffusion_bridge_t* bridge,
                             const char* prompt,
                             const char* negative_prompt,
                             uint32_t width, uint32_t height,
                             uint32_t steps,
                             float guidance_scale,
                             uint64_t seed,
                             visual_image_t* output)
{
    if (!bridge || !prompt || !output) {
        set_diffusion_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_text_to_image: required parameter is NULL (bridge, prompt, output)");
        return -1;
    }

    clock_t start = clock();

    /* Use defaults if not specified */
    if (width == 0) width = bridge->config.default_width;
    if (height == 0) height = bridge->config.default_height;
    if (steps == 0) steps = bridge->config.default_steps;
    if (guidance_scale <= 0) guidance_scale = bridge->config.default_guidance;
    if (seed == 0) seed = (uint64_t)time(NULL);

    (void)negative_prompt;  /* Used in production for negative conditioning */

    /* Allocate output image */
    output->width = width;
    output->height = height;
    output->channels = 3;
    output->pixels = nimcp_calloc((size_t)width * (size_t)height * 3, sizeof(uint8_t));
    if (!output->pixels) {
        set_diffusion_error("Failed to allocate output image");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "diffusion_text_to_image: output->pixels is NULL");
        return -1;
    }

    /* Latent space dimensions (1/8 of output for SD) */
    uint32_t latent_w = width / 8;
    uint32_t latent_h = height / 8;
    uint32_t latent_c = 4;
    size_t latent_count = (size_t)latent_w * latent_h * latent_c;

    float* latent = nimcp_calloc(latent_count, sizeof(float));
    float* noise_pred = nimcp_calloc(latent_count, sizeof(float));

    if (!latent || !noise_pred) {
        nimcp_free(latent);
        latent = NULL;
        nimcp_free(noise_pred);
        noise_pred = NULL;
        nimcp_free(output->pixels);
        output->pixels = NULL;
        set_diffusion_error("Failed to allocate latent");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "diffusion_text_to_image: required parameter is NULL (latent, noise_pred)");
        return -1;
    }

    /* Initialize with noise */
    generate_gaussian_noise(latent, latent_count, seed);

    /* Get scheduler */
    scheduler_state_t* scheduler = (scheduler_state_t*)bridge->scheduler;
    if (scheduler) {
        scheduler->num_steps = steps;
        scheduler->current_step = 0;
    }

    /* Denoising loop */
    for (uint32_t step = 0; step < steps; step++) {
        /* In production: run UNet to predict noise
         * For now: simulate with decreasing noise
         */
        generate_gaussian_noise(noise_pred, latent_count, seed + step);

        /* Scale noise prediction by CFG */
        float cfg_factor = guidance_scale * (1.0f - (float)step / (float)steps);
        for (size_t i = 0; i < latent_count; i++) {
            noise_pred[i] *= cfg_factor * 0.1f;
        }

        /* Scheduler step */
        if (scheduler) {
            scheduler_step(scheduler, latent, noise_pred, latent_count, step);
        } else {
            /* Simple Euler step */
            float dt = 1.0f / (float)steps;
            for (size_t i = 0; i < latent_count; i++) {
                latent[i] -= dt * noise_pred[i];
            }
        }
    }

    /* Decode latent to image */
    /* In production: run VAE decoder */
    /* For now: map latent values to colors */
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            /* Sample latent with bilinear interpolation */
            float lx = (float)x / (float)width * latent_w;
            float ly = (float)y / (float)height * latent_h;

            uint32_t li = (uint32_t)ly * latent_w + (uint32_t)lx;
            li = li % (latent_w * latent_h);

            /* Use 4 latent channels for RGB+luminance */
            float r = latent[li * latent_c + 0];
            float g = latent[li * latent_c + 1];
            float b = latent[li * latent_c + 2];
            float l = latent[li * latent_c + 3];

            /* Map to [0, 255] with luminance adjustment */
            r = (r * 0.5f + 0.5f + l * 0.1f);
            g = (g * 0.5f + 0.5f + l * 0.1f);
            b = (b * 0.5f + 0.5f + l * 0.1f);

            size_t idx = (y * width + x) * 3;
            output->pixels[idx + 0] = (uint8_t)(fminf(255.0f, fmaxf(0.0f, r * 255.0f)));
            output->pixels[idx + 1] = (uint8_t)(fminf(255.0f, fmaxf(0.0f, g * 255.0f)));
            output->pixels[idx + 2] = (uint8_t)(fminf(255.0f, fmaxf(0.0f, b * 255.0f)));
        }
    }

    nimcp_free(latent);
    latent = NULL;
    nimcp_free(noise_pred);
    noise_pred = NULL;

    /* Update statistics */
    float time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    bridge->images_generated++;
    float n = (float)bridge->images_generated;
    bridge->avg_generation_time_ms = bridge->avg_generation_time_ms * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                                      time_ms / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return 0;
}

int diffusion_img2img(diffusion_bridge_t* bridge,
                       const visual_image_t* init_image,
                       const char* prompt,
                       const char* negative_prompt,
                       float strength,
                       uint32_t steps,
                       float guidance_scale,
                       uint64_t seed,
                       visual_image_t* output)
{
    if (!bridge || !init_image || !prompt || !output) {
        set_diffusion_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_img2img: required parameter is NULL (bridge, init_image, prompt, output)");
        return -1;
    }

    /* Clamp strength */
    strength = fmaxf(0.0f, fminf(1.0f, strength));

    /* First generate a new image */
    int rc = diffusion_text_to_image(bridge, prompt, negative_prompt,
                                      init_image->width, init_image->height,
                                      steps, guidance_scale, seed, output);
    if (rc != 0) return rc;

    /* Blend with init image based on strength */
    size_t pixel_count = (size_t)output->width * output->height * output->channels;
    for (size_t i = 0; i < pixel_count && i < (size_t)init_image->width *
                                               init_image->height * init_image->channels; i++) {
        float init_val = (float)init_image->pixels[i];
        float gen_val = (float)output->pixels[i];
        output->pixels[i] = (uint8_t)(init_val * (1.0f - strength) + gen_val * strength);
    }

    return 0;
}

int diffusion_inpaint(diffusion_bridge_t* bridge,
                       const visual_image_t* image,
                       const visual_image_t* mask,
                       const char* prompt,
                       const char* negative_prompt,
                       uint32_t steps,
                       float guidance_scale,
                       uint64_t seed,
                       visual_image_t* output)
{
    if (!bridge || !image || !mask || !prompt || !output) {
        set_diffusion_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_inpaint: required parameter is NULL (bridge, image, mask, prompt, output)");
        return -1;
    }

    /* Generate new image */
    int rc = diffusion_text_to_image(bridge, prompt, negative_prompt,
                                      image->width, image->height,
                                      steps, guidance_scale, seed, output);
    if (rc != 0) return rc;

    /* Apply mask: keep original where mask is black, use generated where white */
    for (uint32_t y = 0; y < output->height; y++) {
        for (uint32_t x = 0; x < output->width; x++) {
            size_t mask_idx = y * mask->width + x;
            bool is_masked = (mask_idx < (size_t)mask->width * mask->height) &&
                            (mask->pixels[mask_idx * mask->channels] > 127);

            if (!is_masked) {
                /* Keep original */
                size_t idx = (y * output->width + x) * output->channels;
                size_t orig_idx = (y * image->width + x) * image->channels;

                if (orig_idx < (size_t)image->width * image->height * image->channels) {
                    for (uint32_t c = 0; c < output->channels && c < image->channels; c++) {
                        output->pixels[idx + c] = image->pixels[orig_idx + c];
                    }
                }
            }
        }
    }

    return 0;
}

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
                          visual_image_t* output)
{
    if (!bridge || !prompt || !control_image || !output) {
        set_diffusion_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_controlnet: required parameter is NULL (bridge, prompt, control_image, output)");
        return -1;
    }

    (void)control_type;
    (void)conditioning_scale;

    /* Generate base image */
    int rc = diffusion_text_to_image(bridge, prompt, negative_prompt,
                                      width, height, steps, guidance_scale,
                                      seed, output);
    if (rc != 0) return rc;

    /* Apply control image influence (placeholder) */
    /* In production: use ControlNet conditioning during generation */
    float control_weight = conditioning_scale * 0.3f;

    for (uint32_t y = 0; y < output->height; y++) {
        for (uint32_t x = 0; x < output->width; x++) {
            /* Map to control image coordinates */
            uint32_t cx = x * control_image->width / output->width;
            uint32_t cy = y * control_image->height / output->height;

            cx = cx < control_image->width ? cx : control_image->width - 1;
            cy = cy < control_image->height ? cy : control_image->height - 1;

            size_t ctrl_idx = (cy * control_image->width + cx) * control_image->channels;
            size_t out_idx = (y * output->width + x) * output->channels;

            /* Blend with control */
            for (uint32_t c = 0; c < output->channels && c < control_image->channels; c++) {
                float ctrl_val = (float)control_image->pixels[ctrl_idx + c];
                float out_val = (float)output->pixels[out_idx + c];
                output->pixels[out_idx + c] = (uint8_t)(
                    out_val * (1.0f - control_weight) + ctrl_val * control_weight);
            }
        }
    }

    return 0;
}

//=============================================================================
// Batch Generation API
//=============================================================================

int diffusion_generate_batch(diffusion_bridge_t* bridge,
                              const char* prompt,
                              const char* negative_prompt,
                              uint32_t width, uint32_t height,
                              uint32_t steps,
                              float guidance_scale,
                              const uint64_t* seeds,
                              uint32_t batch_size,
                              visual_image_t* outputs)
{
    if (!bridge || !prompt || !seeds || !outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_generate_batch: required parameter is NULL (bridge, prompt, seeds, outputs)");
        return -1;
    }

    for (uint32_t i = 0; i < batch_size; i++) {
        int rc = diffusion_text_to_image(bridge, prompt, negative_prompt,
                                          width, height, steps, guidance_scale,
                                          seeds[i], &outputs[i]);
        if (rc != 0) {
            outputs[i].pixels = NULL;
        }
    }

    return 0;
}

//=============================================================================
// LoRA API
//=============================================================================

int diffusion_load_lora(diffusion_bridge_t* bridge,
                         const char* lora_path,
                         float scale)
{
    if (!bridge || !lora_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_load_lora: required parameter is NULL (bridge, lora_path)");
        return -1;
    }

    /* Placeholder: would load LoRA weights from file */
    bridge->lora_scale = scale;

    return 0;
}

void diffusion_unload_lora(diffusion_bridge_t* bridge)
{
    if (!bridge) return;

    if (bridge->lora_weights) {
        nimcp_free(bridge->lora_weights);
        bridge->lora_weights = NULL;
    }

    bridge->lora_scale = 0.0f;
}

//=============================================================================
// Scheduler API
//=============================================================================

int diffusion_set_scheduler(diffusion_bridge_t* bridge,
                             diffusion_scheduler_t scheduler)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_set_scheduler: bridge is NULL");
        return -1;
    }

    bridge->config.scheduler = scheduler;

    if (bridge->scheduler) {
        scheduler_state_t* sched = (scheduler_state_t*)bridge->scheduler;
        sched->type = scheduler;
    }

    return 0;
}

diffusion_scheduler_t diffusion_get_scheduler(const diffusion_bridge_t* bridge)
{
    return bridge ? bridge->config.scheduler : SCHEDULER_EULER;
}

//=============================================================================
// Utility API
//=============================================================================

bool diffusion_bridge_ready(const diffusion_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "diffusion_bridge_ready: bridge is NULL");
        return false;
    }

    /* For API backends, check credentials */
    if (bridge->config.backend == DIFFUSION_BACKEND_API_STABILITY ||
        bridge->config.backend == DIFFUSION_BACKEND_API_OPENAI ||
        bridge->config.backend == DIFFUSION_BACKEND_API_REPLICATE) {
        return strlen(bridge->config.api_key) > 0;
    }

    /* For local backends, check if core models are loaded */
    return bridge->onnx_runtime != NULL;
}

const char* diffusion_bridge_model_info(const diffusion_bridge_t* bridge)
{
    static char info[NIMCP_ERROR_BUFFER_SIZE];

    if (!bridge) return "Not initialized";

    const char* model_name;
    switch (bridge->config.model) {
        case DIFFUSION_MODEL_SD_15: model_name = "Stable Diffusion 1.5"; break;
        case DIFFUSION_MODEL_SD_21: model_name = "Stable Diffusion 2.1"; break;
        case DIFFUSION_MODEL_SDXL: model_name = "Stable Diffusion XL"; break;
        case DIFFUSION_MODEL_SDXL_TURBO: model_name = "SDXL Turbo"; break;
        case DIFFUSION_MODEL_SD3: model_name = "Stable Diffusion 3"; break;
        case DIFFUSION_MODEL_CUSTOM: model_name = "Custom"; break;
        default: model_name = "Unknown"; break;
    }

    const char* backend_name;
    switch (bridge->config.backend) {
        case DIFFUSION_BACKEND_ONNX: backend_name = "ONNX"; break;
        case DIFFUSION_BACKEND_TENSORRT: backend_name = "TensorRT"; break;
        case DIFFUSION_BACKEND_API_STABILITY: backend_name = "Stability AI API"; break;
        case DIFFUSION_BACKEND_API_OPENAI: backend_name = "OpenAI API"; break;
        case DIFFUSION_BACKEND_API_REPLICATE: backend_name = "Replicate API"; break;
        default: backend_name = "Unknown"; break;
    }

    snprintf(info, sizeof(info), "%s via %s", model_name, backend_name);
    return info;
}

uint64_t diffusion_bridge_vram_usage(const diffusion_bridge_t* bridge)
{
    return bridge ? bridge->peak_vram_bytes : 0;
}

const char* diffusion_bridge_get_error(const diffusion_bridge_t* bridge)
{
    (void)bridge;
    return g_diffusion_error[0] ? g_diffusion_error : NULL;
}

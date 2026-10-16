//=============================================================================
// nimcp_creative_neural_bridge.c - Neural Network Generation Bridge
//=============================================================================
/**
 * @file nimcp_creative_neural_bridge.c
 * @brief Bridge connecting creative system to neural network backends
 *
 * Implementation of unified neural generation interface that routes
 * to diffusion, GAN, or API backends based on availability and preference.
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/bridges/nimcp_creative_neural_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(creative_neural_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Thread-local error handling
//=============================================================================

static _Thread_local char g_neural_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_neural_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_neural_error, sizeof(g_neural_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Helper: Get current time in milliseconds
//=============================================================================

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

//=============================================================================
// Configuration defaults
//=============================================================================

void creative_neural_bridge_config_defaults(creative_neural_bridge_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Backend preferences */
    config->preferred_backend = NEURAL_BACKEND_AUTO;
    config->enable_fallback = true;

    /* Enable backends by default - will check availability at runtime */
    config->enable_diffusion = true;
    config->enable_gan = true;
    config->enable_api = true;

    /* Initialize sub-configs with defaults */
    diffusion_bridge_config_defaults(&config->diffusion_config);
    gan_bridge_config_defaults(&config->gan_config);
    creative_api_client_config_defaults(&config->api_config);

    /* Quality settings */
    config->quality_threshold = 0.7f;
    config->max_regeneration = 3;

    /* Resource management */
    config->max_vram_total = 8ULL * 1024 * 1024 * 1024; /* 8GB default */
    config->lazy_load_models = true;
    config->unload_unused = true;
    config->unload_after_ms = 300000; /* 5 minutes */
}

//=============================================================================
// Lifecycle API
//=============================================================================

creative_neural_bridge_t* creative_neural_bridge_create(
    const creative_neural_bridge_config_t* config) {

    if (!config) {
        set_neural_error("NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_neural_bridge_config_defaults: config is NULL");
        return NULL;
    }

    creative_neural_bridge_t* bridge = nimcp_calloc(1, sizeof(creative_neural_bridge_t));
    if (!bridge) {
        set_neural_error("Failed to allocate neural bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_neural_bridge_config_defaults: bridge is NULL");
        return NULL;
    }

    /* Copy config */
    memcpy(&bridge->config, config, sizeof(creative_neural_bridge_config_t));

    /* Initialize backends (lazy load if configured) */
    if (!config->lazy_load_models) {
        if (config->enable_diffusion) {
            bridge->diffusion = diffusion_bridge_create(&config->diffusion_config);
            if (bridge->diffusion) {
                bridge->last_use_time[NEURAL_BACKEND_DIFFUSION_LOCAL] = get_time_ms();
            }
        }

        if (config->enable_gan) {
            bridge->gan = gan_bridge_create(&config->gan_config);
            if (bridge->gan) {
                bridge->last_use_time[NEURAL_BACKEND_GAN_LOCAL] = get_time_ms();
            }
        }

        if (config->enable_api) {
            bridge->api = creative_api_client_create(&config->api_config);
            if (bridge->api) {
                bridge->last_use_time[NEURAL_BACKEND_API_CLOUD] = get_time_ms();
            }
        }
    }

    bridge->active_backend = NEURAL_BACKEND_AUTO;
    bridge->generations = 0;
    bridge->regenerations = 0;
    bridge->fallbacks = 0;
    bridge->avg_quality = 0.0f;

    return bridge;
}

void creative_neural_bridge_destroy(creative_neural_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->diffusion) {
        diffusion_bridge_destroy(bridge->diffusion);
    }
    if (bridge->gan) {
        gan_bridge_destroy(bridge->gan);
    }
    if (bridge->api) {
        creative_api_client_destroy(bridge->api);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int creative_neural_bridge_load_backend(creative_neural_bridge_t* bridge,
                                         neural_backend_t backend) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_neural_bridge_destroy: bridge is NULL");
        return -1;
    }

    switch (backend) {
        case NEURAL_BACKEND_DIFFUSION_LOCAL:
            if (!bridge->diffusion && bridge->config.enable_diffusion) {
                bridge->diffusion = diffusion_bridge_create(&bridge->config.diffusion_config);
                if (!bridge->diffusion) {
                    set_neural_error("Failed to load diffusion backend");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_neural_bridge_destroy: bridge->diffusion is NULL");
                    return -1;
                }
            }
            bridge->last_use_time[NEURAL_BACKEND_DIFFUSION_LOCAL] = get_time_ms();
            break;

        case NEURAL_BACKEND_GAN_LOCAL:
            if (!bridge->gan && bridge->config.enable_gan) {
                bridge->gan = gan_bridge_create(&bridge->config.gan_config);
                if (!bridge->gan) {
                    set_neural_error("Failed to load GAN backend");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_neural_bridge_destroy: bridge->gan is NULL");
                    return -1;
                }
            }
            bridge->last_use_time[NEURAL_BACKEND_GAN_LOCAL] = get_time_ms();
            break;

        case NEURAL_BACKEND_API_CLOUD:
            if (!bridge->api && bridge->config.enable_api) {
                bridge->api = creative_api_client_create(&bridge->config.api_config);
                if (!bridge->api) {
                    set_neural_error("Failed to load API backend");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_neural_bridge_destroy: bridge->api is NULL");
                    return -1;
                }
            }
            bridge->last_use_time[NEURAL_BACKEND_API_CLOUD] = get_time_ms();
            break;

        case NEURAL_BACKEND_HYBRID:
        case NEURAL_BACKEND_AUTO:
            /* Load all enabled backends */
            if (bridge->config.enable_diffusion) {
                creative_neural_bridge_load_backend(bridge, NEURAL_BACKEND_DIFFUSION_LOCAL);
            }
            if (bridge->config.enable_gan) {
                creative_neural_bridge_load_backend(bridge, NEURAL_BACKEND_GAN_LOCAL);
            }
            if (bridge->config.enable_api) {
                creative_neural_bridge_load_backend(bridge, NEURAL_BACKEND_API_CLOUD);
            }
            break;
    }

    return 0;
}

void creative_neural_bridge_unload_backend(creative_neural_bridge_t* bridge,
                                            neural_backend_t backend) {
    if (!bridge) return;

    switch (backend) {
        case NEURAL_BACKEND_DIFFUSION_LOCAL:
            if (bridge->diffusion) {
                diffusion_bridge_destroy(bridge->diffusion);
                bridge->diffusion = NULL;
            }
            break;

        case NEURAL_BACKEND_GAN_LOCAL:
            if (bridge->gan) {
                gan_bridge_destroy(bridge->gan);
                bridge->gan = NULL;
            }
            break;

        case NEURAL_BACKEND_API_CLOUD:
            if (bridge->api) {
                creative_api_client_destroy(bridge->api);
                bridge->api = NULL;
            }
            break;

        case NEURAL_BACKEND_HYBRID:
        case NEURAL_BACKEND_AUTO:
            creative_neural_bridge_unload_backend(bridge, NEURAL_BACKEND_DIFFUSION_LOCAL);
            creative_neural_bridge_unload_backend(bridge, NEURAL_BACKEND_GAN_LOCAL);
            creative_neural_bridge_unload_backend(bridge, NEURAL_BACKEND_API_CLOUD);
            break;
    }
}

//=============================================================================
// Helper: Check for unused backends to unload
//=============================================================================

static void check_unload_unused(creative_neural_bridge_t* bridge) {
    if (!bridge || !bridge->config.unload_unused) return;

    uint64_t now = get_time_ms();
    uint64_t threshold = bridge->config.unload_after_ms;

    if (bridge->diffusion &&
        (now - bridge->last_use_time[NEURAL_BACKEND_DIFFUSION_LOCAL]) > threshold) {
        creative_neural_bridge_unload_backend(bridge, NEURAL_BACKEND_DIFFUSION_LOCAL);
    }

    if (bridge->gan &&
        (now - bridge->last_use_time[NEURAL_BACKEND_GAN_LOCAL]) > threshold) {
        creative_neural_bridge_unload_backend(bridge, NEURAL_BACKEND_GAN_LOCAL);
    }

    /* Don't unload API client as it has no significant memory footprint */
}

//=============================================================================
// Generation API
//=============================================================================

int neural_generate_image(creative_neural_bridge_t* bridge,
                           const char* prompt,
                           const char* negative_prompt,
                           uint32_t width, uint32_t height,
                           uint32_t steps,
                           float guidance,
                           uint64_t seed,
                           neural_backend_t backend_hint,
                           visual_generation_result_t* result) {
    if (!bridge || !prompt || !result) {
        set_neural_error("Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_unload_unused: required parameter is NULL (bridge, prompt, result)");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Check for unused backends to unload */
    check_unload_unused(bridge);

    /* Select backend */
    neural_backend_t backend = backend_hint;
    if (backend == NEURAL_BACKEND_AUTO) {
        backend = neural_select_backend(bridge, width, height, false);
    }

    /* Ensure backend is loaded */
    if (creative_neural_bridge_load_backend(bridge, backend) != 0) {
        if (!bridge->config.enable_fallback) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_unload_unused: bridge->config is NULL");
            return -1;
        }
        /* Try fallback backends */
        if (backend != NEURAL_BACKEND_DIFFUSION_LOCAL && bridge->config.enable_diffusion) {
            backend = NEURAL_BACKEND_DIFFUSION_LOCAL;
            if (creative_neural_bridge_load_backend(bridge, backend) != 0) {
                backend = NEURAL_BACKEND_API_CLOUD;
            } else {
                bridge->fallbacks++;
            }
        }
        if (backend == NEURAL_BACKEND_API_CLOUD && bridge->config.enable_api) {
            if (creative_neural_bridge_load_backend(bridge, backend) != 0) {
                set_neural_error("No backend available");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_unload_unused: validation failed");
                return -1;
            }
            bridge->fallbacks++;
        }
    }

    int ret = -1;
    uint64_t start_time = get_time_ms();

    /* Generate based on backend */
    switch (backend) {
        case NEURAL_BACKEND_DIFFUSION_LOCAL:
            if (bridge->diffusion) {
                visual_image_t image = {0};
                ret = diffusion_text_to_image(bridge->diffusion,
                                               prompt, negative_prompt,
                                               width, height,
                                               steps, guidance, seed,
                                               &image);
                if (ret == 0) {
                    result->image = image;
                    result->success = true;
                    result->seed_used = (seed != 0) ? seed : (uint64_t)nimcp_tl_rand();
                }
                bridge->last_use_time[NEURAL_BACKEND_DIFFUSION_LOCAL] = get_time_ms();
            }
            break;

        case NEURAL_BACKEND_GAN_LOCAL:
            if (bridge->gan) {
                /* GANs typically don't use text prompts - generate from random latent */
                gan_latent_t latent = {0};
                latent.space = LATENT_SPACE_Z;
                latent.dim = 512;
                latent.data = nimcp_calloc(512, sizeof(float));
                if (latent.data) {
                    /* Random latent vector */
                    for (uint32_t i = 0; i < 512; i++) {
                        latent.data[i] = ((float)nimcp_tl_rand() / RAND_MAX) * 2.0f - 1.0f;
                    }

                    visual_image_t image = {0};
                    ret = gan_generate(bridge->gan, &latent, &image);
                    if (ret == 0) {
                        result->image = image;
                        result->success = true;
                        result->seed_used = seed;
                    }
                    nimcp_free(latent.data);
                }
                bridge->last_use_time[NEURAL_BACKEND_GAN_LOCAL] = get_time_ms();
            }
            break;

        case NEURAL_BACKEND_API_CLOUD:
            if (bridge->api) {
                api_image_request_t api_request = {0};
                api_request.prompt = prompt;
                api_request.negative_prompt = negative_prompt;
                api_request.width = width;
                api_request.height = height;
                api_request.steps = steps;
                api_request.guidance_scale = guidance;
                api_request.seed = seed;

                api_image_response_t api_response = {0};
                ret = api_generate_image(bridge->api, &api_request, &api_response);
                if (ret == 0 && api_response.status == API_STATUS_SUCCESS &&
                    api_response.num_images > 0) {
                    /* Copy first image */
                    result->image = api_response.images[0];
                    result->success = true;
                    result->seed_used = api_response.seed_used;
                    /* Free any additional images */
                    for (uint32_t i = 1; i < api_response.num_images; i++) {
                        nimcp_free(api_response.images[i].pixels);
                    }
                }
                bridge->last_use_time[NEURAL_BACKEND_API_CLOUD] = get_time_ms();
            }
            break;

        case NEURAL_BACKEND_HYBRID:
            /* Try local first, fall back to API */
            ret = neural_generate_image(bridge, prompt, negative_prompt,
                                         width, height, steps, guidance, seed,
                                         NEURAL_BACKEND_DIFFUSION_LOCAL, result);
            if (ret != 0 && bridge->config.enable_api) {
                ret = neural_generate_image(bridge, prompt, negative_prompt,
                                             width, height, steps, guidance, seed,
                                             NEURAL_BACKEND_API_CLOUD, result);
                if (ret == 0) bridge->fallbacks++;
            }
            break;

        case NEURAL_BACKEND_AUTO:
            /* Should not reach here - already selected */
            break;
    }

    if (ret == 0) {
        uint64_t elapsed = get_time_ms() - start_time;
        result->generation_time_ms = (float)elapsed;
        bridge->generations++;
        bridge->active_backend = backend;

        /* Update average generation time for this backend */
        /* (simplified - just record the generation) */
    }

    return ret;
}

int neural_generate_styled(creative_neural_bridge_t* bridge,
                            const char* prompt,
                            const style_embedding_t* style,
                            uint32_t width, uint32_t height,
                            visual_generation_result_t* result) {
    if (!bridge || !prompt || !style || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_unload_unused: required parameter is NULL (bridge, prompt, style, result)");
        return -1;
    }

    /* For now, append style name to prompt */
    char styled_prompt[NIMCP_JSON_BUFFER_SIZE];
    if (style->style_name[0]) {
        snprintf(styled_prompt, sizeof(styled_prompt),
                 "%s, in the style of %s", prompt, style->style_name);
    } else {
        snprintf(styled_prompt, sizeof(styled_prompt), "%s", prompt);
    }

    /* Use default generation parameters */
    return neural_generate_image(bridge, styled_prompt, NULL,
                                  width, height, 30, 7.5f, 0,
                                  NEURAL_BACKEND_AUTO, result);
}

int neural_img2img(creative_neural_bridge_t* bridge,
                    const visual_image_t* init_image,
                    const char* prompt,
                    float strength,
                    visual_generation_result_t* result) {
    if (!bridge || !init_image || !prompt || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_img2img: required parameter is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Prefer diffusion for img2img */
    if (creative_neural_bridge_load_backend(bridge, NEURAL_BACKEND_DIFFUSION_LOCAL) == 0 &&
        bridge->diffusion) {

        visual_image_t output = {0};
        int ret = diffusion_img2img(bridge->diffusion,
                                     init_image, prompt, NULL,
                                     strength, 30, 7.5f, 0,
                                     &output);
        if (ret == 0) {
            result->image = output;
            result->success = true;
            bridge->generations++;
            return 0;
        }
    }

    /* Fall back to API */
    if (bridge->config.enable_api && bridge->config.enable_fallback) {
        /* API img2img would need different implementation */
        set_neural_error("API img2img not implemented");
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_img2img: no backend available");
    return -1;
}

int neural_inpaint(creative_neural_bridge_t* bridge,
                    const visual_image_t* image,
                    const visual_image_t* mask,
                    const char* prompt,
                    visual_generation_result_t* result) {
    if (!bridge || !image || !mask || !prompt || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_inpaint: required parameter is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Prefer diffusion for inpainting */
    if (creative_neural_bridge_load_backend(bridge, NEURAL_BACKEND_DIFFUSION_LOCAL) == 0 &&
        bridge->diffusion) {

        visual_image_t output = {0};
        int ret = diffusion_inpaint(bridge->diffusion,
                                     image, mask, prompt, NULL,
                                     30, 7.5f, 0,
                                     &output);
        if (ret == 0) {
            result->image = output;
            result->success = true;
            bridge->generations++;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_inpaint: no backend available");
    return -1;
}

//=============================================================================
// GAN-Specific API
//=============================================================================

int neural_generate_from_latent(creative_neural_bridge_t* bridge,
                                 const gan_latent_t* latent,
                                 visual_generation_result_t* result) {
    if (!bridge || !latent || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_generate_from_latent: required parameter is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    if (creative_neural_bridge_load_backend(bridge, NEURAL_BACKEND_GAN_LOCAL) != 0 ||
        !bridge->gan) {
        set_neural_error("GAN backend not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_generate_from_latent: GAN backend not available");
        return -1;
    }

    visual_image_t output = {0};
    int ret = gan_generate(bridge->gan, latent, &output);
    if (ret == 0) {
        result->image = output;
        result->success = true;
        bridge->generations++;
    }

    return ret;
}

int neural_encode_to_latent(creative_neural_bridge_t* bridge,
                             const visual_image_t* image,
                             gan_latent_t* latent) {
    if (!bridge || !image || !latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_encode_to_latent: required parameter is NULL");
        return -1;
    }

    if (creative_neural_bridge_load_backend(bridge, NEURAL_BACKEND_GAN_LOCAL) != 0 ||
        !bridge->gan) {
        set_neural_error("GAN backend not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_encode_to_latent: GAN backend not available");
        return -1;
    }

    return gan_encode(bridge->gan, image, LATENT_SPACE_W, latent);
}

//=============================================================================
// Batch API
//=============================================================================

int neural_generate_batch(creative_neural_bridge_t* bridge,
                           const char* prompt,
                           uint32_t width, uint32_t height,
                           const uint64_t* seeds,
                           uint32_t batch_size,
                           visual_generation_result_t* results) {
    if (!bridge || !prompt || !seeds || !results || batch_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_generate_batch: required parameter is NULL");
        return -1;
    }

    int failures = 0;

    for (uint32_t i = 0; i < batch_size; i++) {
        int ret = neural_generate_image(bridge, prompt, NULL,
                                          width, height, 30, 7.5f, seeds[i],
                                          NEURAL_BACKEND_AUTO, &results[i]);
        if (ret != 0) {
            failures++;
            results[i].success = false;
        }
    }

    return (failures == (int)batch_size) ? -1 : 0;
}

//=============================================================================
// Backend Management API
//=============================================================================

int neural_get_backend_status(const creative_neural_bridge_t* bridge,
                               neural_backend_t backend,
                               backend_status_t* status) {
    if (!bridge || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_get_backend_status: bridge or status is NULL");
        return -1;
    }

    memset(status, 0, sizeof(*status));
    status->backend = backend;

    switch (backend) {
        case NEURAL_BACKEND_DIFFUSION_LOCAL:
            status->available = bridge->config.enable_diffusion;
            status->loaded = (bridge->diffusion != NULL);
            if (bridge->diffusion) {
                /* Get VRAM usage from diffusion bridge if available */
                status->vram_usage = 4ULL * 1024 * 1024 * 1024; /* Estimate 4GB */
            }
            break;

        case NEURAL_BACKEND_GAN_LOCAL:
            status->available = bridge->config.enable_gan;
            status->loaded = (bridge->gan != NULL);
            if (bridge->gan) {
                status->vram_usage = 2ULL * 1024 * 1024 * 1024; /* Estimate 2GB */
            }
            break;

        case NEURAL_BACKEND_API_CLOUD:
            status->available = bridge->config.enable_api;
            status->loaded = (bridge->api != NULL);
            status->vram_usage = 0; /* No VRAM for API */
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_get_backend_status: unknown backend type");
            return -1;
    }

    return 0;
}

neural_backend_t neural_select_backend(const creative_neural_bridge_t* bridge,
                                        uint32_t width, uint32_t height,
                                        bool need_speed) {
    if (!bridge) {
        return NEURAL_BACKEND_API_CLOUD;
    }

    /* Check preferred backend first */
    if (bridge->config.preferred_backend != NEURAL_BACKEND_AUTO &&
        bridge->config.preferred_backend != NEURAL_BACKEND_HYBRID) {

        switch (bridge->config.preferred_backend) {
            case NEURAL_BACKEND_DIFFUSION_LOCAL:
                if (bridge->config.enable_diffusion) {
                    return NEURAL_BACKEND_DIFFUSION_LOCAL;
                }
                break;
            case NEURAL_BACKEND_GAN_LOCAL:
                if (bridge->config.enable_gan) {
                    return NEURAL_BACKEND_GAN_LOCAL;
                }
                break;
            case NEURAL_BACKEND_API_CLOUD:
                if (bridge->config.enable_api) {
                    return NEURAL_BACKEND_API_CLOUD;
                }
                break;
            default:
                break;
        }
    }

    /* Auto-select based on criteria */

    /* For speed, prefer API (no local compute time) */
    if (need_speed && bridge->config.enable_api) {
        return NEURAL_BACKEND_API_CLOUD;
    }

    /* For high resolution, prefer diffusion */
    if ((width >= 1024 || height >= 1024) && bridge->config.enable_diffusion) {
        return NEURAL_BACKEND_DIFFUSION_LOCAL;
    }

    /* Default priority: diffusion > GAN > API */
    if (bridge->config.enable_diffusion) {
        return NEURAL_BACKEND_DIFFUSION_LOCAL;
    }
    if (bridge->config.enable_gan) {
        return NEURAL_BACKEND_GAN_LOCAL;
    }
    if (bridge->config.enable_api) {
        return NEURAL_BACKEND_API_CLOUD;
    }

    /* No backend available */
    return NEURAL_BACKEND_DIFFUSION_LOCAL; /* Will fail gracefully */
}

int neural_warmup_backend(creative_neural_bridge_t* bridge,
                           neural_backend_t backend) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_warmup_backend: bridge is NULL");
        return -1;
    }

    /* Load the backend */
    int ret = creative_neural_bridge_load_backend(bridge, backend);
    if (ret != 0) {
        return ret;
    }

    /* Perform a small test generation to warm up GPU/model caches */
    switch (backend) {
        case NEURAL_BACKEND_DIFFUSION_LOCAL:
            if (bridge->diffusion) {
                visual_image_t dummy = {0};
                /* Very small warmup generation */
                diffusion_text_to_image(bridge->diffusion,
                                         "warmup test", NULL,
                                         64, 64, 1, 7.5f, 42,
                                         &dummy);
                if (dummy.pixels) {
                    nimcp_free(dummy.pixels);
                }
            }
            break;

        case NEURAL_BACKEND_GAN_LOCAL:
            if (bridge->gan) {
                gan_latent_t latent = {0};
                latent.space = LATENT_SPACE_Z;
                latent.dim = 512;
                latent.data = nimcp_calloc(512, sizeof(float));
                if (latent.data) {
                    visual_image_t dummy = {0};
                    gan_generate(bridge->gan, &latent, &dummy);
                    if (dummy.pixels) {
                        nimcp_free(dummy.pixels);
                    }
                    nimcp_free(latent.data);
                }
            }
            break;

        case NEURAL_BACKEND_API_CLOUD:
            /* No warmup needed for API */
            break;

        default:
            break;
    }

    return 0;
}

//=============================================================================
// Integration API
//=============================================================================

void neural_bridge_set_evaluator(creative_neural_bridge_t* bridge,
                                  void* evaluator) {
    if (bridge) {
        bridge->aesthetic_evaluator = evaluator;
    }
}

diffusion_bridge_t* neural_bridge_get_diffusion(creative_neural_bridge_t* bridge) {
    return bridge ? bridge->diffusion : NULL;
}

gan_bridge_t* neural_bridge_get_gan(creative_neural_bridge_t* bridge) {
    return bridge ? bridge->gan : NULL;
}

creative_api_client_t* neural_bridge_get_api(creative_neural_bridge_t* bridge) {
    return bridge ? bridge->api : NULL;
}

//=============================================================================
// Utility API
//=============================================================================

const char* neural_backend_name(neural_backend_t backend) {
    switch (backend) {
        case NEURAL_BACKEND_DIFFUSION_LOCAL: return "diffusion_local";
        case NEURAL_BACKEND_GAN_LOCAL:       return "gan_local";
        case NEURAL_BACKEND_API_CLOUD:       return "api_cloud";
        case NEURAL_BACKEND_HYBRID:          return "hybrid";
        case NEURAL_BACKEND_AUTO:            return "auto";
        default:                             return "unknown";
    }
}

bool neural_backend_available(const creative_neural_bridge_t* bridge,
                               neural_backend_t backend) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_backend_name: bridge is NULL");
        return false;
    }

    switch (backend) {
        case NEURAL_BACKEND_DIFFUSION_LOCAL:
            return bridge->config.enable_diffusion;
        case NEURAL_BACKEND_GAN_LOCAL:
            return bridge->config.enable_gan;
        case NEURAL_BACKEND_API_CLOUD:
            return bridge->config.enable_api;
        case NEURAL_BACKEND_HYBRID:
        case NEURAL_BACKEND_AUTO:
            return bridge->config.enable_diffusion ||
                   bridge->config.enable_gan ||
                   bridge->config.enable_api;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "neural_backend_name: operation failed");
            return false;
    }
}

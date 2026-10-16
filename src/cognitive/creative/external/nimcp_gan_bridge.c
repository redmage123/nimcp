//=============================================================================
// nimcp_gan_bridge.c - GAN Model Integration Implementation
//=============================================================================
/**
 * @file nimcp_gan_bridge.c
 * @brief Implements interface to StyleGAN and other GAN architectures
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/external/nimcp_gan_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gan_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_gan_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_gan_bridge_mesh_registry = NULL;

nimcp_error_t gan_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_gan_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "gan_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "gan_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_gan_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_gan_bridge_mesh_registry = registry;
    return err;
}

void gan_bridge_mesh_unregister(void) {
    if (g_gan_bridge_mesh_registry && g_gan_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_gan_bridge_mesh_registry, g_gan_bridge_mesh_id);
        g_gan_bridge_mesh_id = 0;
        g_gan_bridge_mesh_registry = NULL;
    }
}


//=============================================================================
// Thread-local error message
//=============================================================================

static _Thread_local char g_gan_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_gan_error(const char* msg)
{
    if (msg) {
        strncpy(g_gan_error, msg, sizeof(g_gan_error) - 1);
    } else {
        g_gan_error[0] = '\0';
    }
}

//=============================================================================
// Configuration Defaults
//=============================================================================

void gan_bridge_config_defaults(gan_bridge_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(gan_bridge_config_t));

    config->type = GAN_TYPE_STYLEGAN2;

    strncpy(config->generator_path, "/models/gan/generator.onnx",
            sizeof(config->generator_path) - 1);

    config->latent_dim = 512;
    config->w_dim = 512;
    config->num_layers = 18;
    config->output_size = 1024;

    config->device = ONNX_DEVICE_CPU;
    config->device_id = 0;

    config->default_space = LATENT_SPACE_W;
    config->truncation.psi = 0.7f;
    config->truncation.cutoff = 8;

    config->num_classes = 0;  /* Not class-conditional by default */
}

//=============================================================================
// Lifecycle
//=============================================================================

gan_bridge_t* gan_bridge_create(const gan_bridge_config_t* config)
{
    gan_bridge_t* bridge = nimcp_calloc(1, sizeof(gan_bridge_t));
    if (!bridge) {
        set_gan_error("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_bridge_create: bridge is NULL");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        gan_bridge_config_defaults(&bridge->config);
    }

    /* Create ONNX runtime */
    onnx_runtime_config_t onnx_config;
    onnx_runtime_config_defaults(&onnx_config);
    onnx_config.device = bridge->config.device;
    onnx_config.device_id = bridge->config.device_id;

    bridge->onnx_runtime = onnx_runtime_create(&onnx_config);

    /* Initialize mean latent for truncation */
    memset(&bridge->mean_latent, 0, sizeof(gan_latent_t));
    bridge->mean_latent.dim = bridge->config.w_dim;
    bridge->mean_latent.space = LATENT_SPACE_W;
    bridge->mean_latent.num_layers = bridge->config.num_layers;
    bridge->mean_latent.owns_data = true;
    bridge->mean_latent.data = nimcp_calloc(bridge->config.w_dim *
                                             bridge->config.num_layers, sizeof(float));

    /* Initialize mean latent to zeros (would be computed from samples) */

    bridge->images_generated = 0;
    bridge->avg_generation_time_ms = 0.0f;

    return bridge;
}

void gan_bridge_destroy(gan_bridge_t* bridge)
{
    if (!bridge) return;

    gan_bridge_unload_model(bridge);

    if (bridge->mean_latent.owns_data && bridge->mean_latent.data) {
        nimcp_free(bridge->mean_latent.data);
    }

    if (bridge->onnx_runtime) {
        onnx_runtime_destroy(bridge->onnx_runtime);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

int gan_bridge_load_model(gan_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_bridge_load_model: bridge is NULL");
        return -1;
    }
    if (!bridge->onnx_runtime) {
        set_gan_error("ONNX runtime not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_bridge_load_model: bridge->onnx_runtime is NULL");
        return -1;
    }

    /* Load generator */
    if (strlen(bridge->config.generator_path) > 0) {
        bridge->generator = onnx_load_model(bridge->onnx_runtime,
            bridge->config.generator_path, NULL);
    }

    /* Load encoder if available */
    if (strlen(bridge->config.encoder_path) > 0) {
        bridge->encoder = onnx_load_model(bridge->onnx_runtime,
            bridge->config.encoder_path, NULL);
    }

    return 0;
}

void gan_bridge_unload_model(gan_bridge_t* bridge)
{
    if (!bridge || !bridge->onnx_runtime) return;

    if (bridge->generator) {
        onnx_unload_model(bridge->onnx_runtime, bridge->generator);
        bridge->generator = NULL;
    }

    if (bridge->encoder) {
        onnx_unload_model(bridge->onnx_runtime, bridge->encoder);
        bridge->encoder = NULL;
    }

    if (bridge->discriminator) {
        onnx_unload_model(bridge->onnx_runtime, bridge->discriminator);
        bridge->discriminator = NULL;
    }

    if (bridge->mapping_network) {
        onnx_unload_model(bridge->onnx_runtime, bridge->mapping_network);
        bridge->mapping_network = NULL;
    }
}

//=============================================================================
// Latent API
//=============================================================================

int gan_sample_latent(gan_bridge_t* bridge,
                       latent_space_t space,
                       uint64_t seed,
                       gan_latent_t* latent)
{
    if (!bridge || !latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_sample_latent: required parameter is NULL (bridge, latent)");
        return -1;
    }

    memset(latent, 0, sizeof(gan_latent_t));

    if (seed == 0) seed = (uint64_t)time(NULL);

    uint32_t dim = 0;
    uint32_t num_layers = 1;

    switch (space) {
        case LATENT_SPACE_Z:
            dim = bridge->config.latent_dim;
            break;

        case LATENT_SPACE_W:
            dim = bridge->config.w_dim;
            break;

        case LATENT_SPACE_W_PLUS:
            dim = bridge->config.w_dim;
            num_layers = bridge->config.num_layers;
            break;

        case LATENT_SPACE_S:
            dim = bridge->config.w_dim * 2;  /* Style space typically larger */
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gan_sample_latent: operation failed");
            return -1;
    }

    latent->dim = dim;
    latent->space = space;
    latent->num_layers = num_layers;
    latent->owns_data = true;

    size_t total_dim = (size_t)dim * num_layers;
    latent->data = nimcp_calloc(total_dim, sizeof(float));
    if (!latent->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_sample_latent: latent->data is NULL");
        return -1;
    }

    /* Sample from standard normal */
    uint64_t state = seed;
    for (size_t i = 0; i < total_dim; i += 2) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        float u1 = (float)(state >> 33) / (float)(1ULL << 31);
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        float u2 = (float)(state >> 33) / (float)(1ULL << 31);

        u1 = fmaxf(u1, 1e-10f);
        float radius = sqrtf(-2.0f * logf(u1));
        float theta = NIMCP_TWO_PI_F * u2;

        latent->data[i] = radius * cosf(theta);
        if (i + 1 < total_dim) {
            latent->data[i + 1] = radius * sinf(theta);
        }
    }

    return 0;
}

int gan_map_z_to_w(gan_bridge_t* bridge,
                    const gan_latent_t* z,
                    gan_latent_t* w)
{
    if (!bridge || !z || !w) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_map_z_to_w: required parameter is NULL (bridge, z, w)");
        return -1;
    }
    if (z->space != LATENT_SPACE_Z) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gan_map_z_to_w: validation failed");
        return -1;
    }

    memset(w, 0, sizeof(gan_latent_t));

    w->dim = bridge->config.w_dim;
    w->space = LATENT_SPACE_W;
    w->num_layers = 1;
    w->owns_data = true;
    w->data = nimcp_calloc(w->dim, sizeof(float));

    if (!w->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_map_z_to_w: w->data is NULL");
        return -1;
    }

    /* In production: run mapping network */
    /* Placeholder: apply simple transformation */
    for (uint32_t i = 0; i < w->dim && i < z->dim; i++) {
        /* Simple normalization */
        w->data[i] = z->data[i] * 0.9f;
    }

    return 0;
}

int gan_truncate_latent(gan_bridge_t* bridge,
                         gan_latent_t* latent,
                         const truncation_params_t* params)
{
    if (!bridge || !latent || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_truncate_latent: required parameter is NULL (bridge, latent, params)");
        return -1;
    }
    if (!bridge->mean_latent.data || !latent->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_truncate_latent: required parameter is NULL (bridge->mean_latent, latent->data)");
        return -1;
    }

    float psi = params->psi;
    uint32_t cutoff = params->cutoff;

    /* Apply truncation: w' = mean + psi * (w - mean) */
    size_t per_layer = latent->dim;

    for (uint32_t layer = 0; layer < latent->num_layers; layer++) {
        /* Only truncate layers below cutoff */
        if (layer >= cutoff) continue;

        float* w = &latent->data[layer * per_layer];
        float* mean = bridge->mean_latent.data;

        /* If mean_latent has fewer layers, use first layer's mean */
        if (layer < bridge->mean_latent.num_layers) {
            mean = &bridge->mean_latent.data[layer * bridge->mean_latent.dim];
        }

        for (uint32_t i = 0; i < per_layer && i < bridge->mean_latent.dim; i++) {
            w[i] = mean[i] + psi * (w[i] - mean[i]);
        }
    }

    return 0;
}

int gan_interpolate_latent(const gan_latent_t* a,
                            const gan_latent_t* b,
                            float t,
                            gan_latent_t* result)
{
    if (!a || !b || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_interpolate_latent: required parameter is NULL (a, b, result)");
        return -1;
    }
    if (a->space != b->space || a->dim != b->dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gan_interpolate_latent: validation failed");
        return -1;
    }

    memset(result, 0, sizeof(gan_latent_t));

    result->dim = a->dim;
    result->space = a->space;
    result->num_layers = a->num_layers > b->num_layers ? a->num_layers : b->num_layers;
    result->owns_data = true;

    size_t total_dim = (size_t)result->dim * result->num_layers;
    result->data = nimcp_calloc(total_dim, sizeof(float));
    if (!result->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_interpolate_latent: result->data is NULL");
        return -1;
    }

    t = fmaxf(0.0f, fminf(1.0f, t));

    /* Use spherical linear interpolation for better results */
    float dot = 0.0f;
    float norm_a = 0.0f, norm_b = 0.0f;

    for (size_t i = 0; i < total_dim; i++) {
        float va = i < (size_t)a->dim * a->num_layers ? a->data[i] : 0.0f;
        float vb = i < (size_t)b->dim * b->num_layers ? b->data[i] : 0.0f;
        dot += va * vb;
        norm_a += va * va;
        norm_b += vb * vb;
    }

    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 1e-8f || norm_b < 1e-8f) {
        /* Fallback to linear interpolation */
        for (size_t i = 0; i < total_dim; i++) {
            float va = i < (size_t)a->dim * a->num_layers ? a->data[i] : 0.0f;
            float vb = i < (size_t)b->dim * b->num_layers ? b->data[i] : 0.0f;
            result->data[i] = va * (1.0f - t) + vb * t;
        }
    } else {
        /* Spherical interpolation */
        float omega = acosf(fmaxf(-1.0f, fminf(1.0f, dot / (norm_a * norm_b))));
        float sin_omega = sinf(omega);

        if (sin_omega < 1e-8f) {
            /* Nearly parallel - use linear */
            for (size_t i = 0; i < total_dim; i++) {
                float va = i < (size_t)a->dim * a->num_layers ? a->data[i] : 0.0f;
                float vb = i < (size_t)b->dim * b->num_layers ? b->data[i] : 0.0f;
                result->data[i] = va * (1.0f - t) + vb * t;
            }
        } else {
            float scale_a = sinf((1.0f - t) * omega) / sin_omega;
            float scale_b = sinf(t * omega) / sin_omega;

            for (size_t i = 0; i < total_dim; i++) {
                float va = i < (size_t)a->dim * a->num_layers ? a->data[i] : 0.0f;
                float vb = i < (size_t)b->dim * b->num_layers ? b->data[i] : 0.0f;
                result->data[i] = va * scale_a + vb * scale_b;
            }
        }
    }

    return 0;
}

void gan_latent_free(gan_latent_t* latent)
{
    if (!latent) return;

    if (latent->owns_data && latent->data) {
        nimcp_free(latent->data);
    }

    memset(latent, 0, sizeof(gan_latent_t));
}

//=============================================================================
// Generation API
//=============================================================================

int gan_generate(gan_bridge_t* bridge,
                  const gan_latent_t* latent,
                  visual_image_t* output)
{
    if (!bridge || !latent || !output) {
        set_gan_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_generate: required parameter is NULL (bridge, latent, output)");
        return -1;
    }

    clock_t start = clock();

    uint32_t size = bridge->config.output_size;

    output->width = size;
    output->height = size;
    output->channels = 3;
    output->pixels = nimcp_calloc((size_t)size * (size_t)size * 3, sizeof(uint8_t));

    if (!output->pixels) {
        set_gan_error("Failed to allocate output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_generate: output->pixels is NULL");
        return -1;
    }

    /* In production: run generator network */
    /* Placeholder: generate pattern from latent */

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            size_t idx = (y * size + x) * 3;

            /* Use latent values to generate colors */
            float fx = (float)x / (float)size;
            float fy = (float)y / (float)size;

            float r = 0.0f, g = 0.0f, b = 0.0f;

            /* Sample from different latent dimensions for RGB */
            uint32_t dim = latent->dim;
            if (dim > 0 && latent->data) {
                uint32_t ri = (uint32_t)(fx * dim) % dim;
                uint32_t gi = (uint32_t)(fy * dim) % dim;
                uint32_t bi = (uint32_t)((fx + fy) * dim) % dim;

                r = latent->data[ri] * 0.5f + 0.5f;
                g = latent->data[gi] * 0.5f + 0.5f;
                b = latent->data[bi] * 0.5f + 0.5f;

                /* Add position-based variation */
                r += sinf(fx * 10.0f + latent->data[0] * 5.0f) * 0.1f;
                g += cosf(fy * 10.0f + latent->data[dim/2] * 5.0f) * 0.1f;
                b += sinf((fx + fy) * 7.0f + latent->data[dim-1] * 5.0f) * 0.1f;
            }

            output->pixels[idx + 0] = (uint8_t)(fminf(255.0f, fmaxf(0.0f, r * 255.0f)));
            output->pixels[idx + 1] = (uint8_t)(fminf(255.0f, fmaxf(0.0f, g * 255.0f)));
            output->pixels[idx + 2] = (uint8_t)(fminf(255.0f, fmaxf(0.0f, b * 255.0f)));
        }
    }

    float time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    bridge->images_generated++;
    float n = (float)bridge->images_generated;
    bridge->avg_generation_time_ms = bridge->avg_generation_time_ms * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                                      time_ms / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return 0;
}

int gan_generate_random(gan_bridge_t* bridge,
                         float truncation,
                         uint64_t seed,
                         visual_image_t* output)
{
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_generate_random: required parameter is NULL (bridge, output)");
        return -1;
    }

    gan_latent_t z, w;

    /* Sample in Z space */
    int rc = gan_sample_latent(bridge, LATENT_SPACE_Z, seed, &z);
    if (rc != 0) return rc;

    /* Map to W space */
    rc = gan_map_z_to_w(bridge, &z, &w);
    gan_latent_free(&z);
    if (rc != 0) return rc;

    /* Apply truncation */
    if (truncation < 1.0f) {
        truncation_params_t params = {
            .psi = truncation,
            .cutoff = bridge->config.truncation.cutoff
        };
        gan_truncate_latent(bridge, &w, &params);
    }

    /* Generate */
    rc = gan_generate(bridge, &w, output);
    gan_latent_free(&w);

    return rc;
}

int gan_generate_class(gan_bridge_t* bridge,
                        uint32_t class_idx,
                        float truncation,
                        uint64_t seed,
                        visual_image_t* output)
{
    if (!bridge || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_generate_class: required parameter is NULL (bridge, output)");
        return -1;
    }
    if (class_idx >= bridge->config.num_classes && bridge->config.num_classes > 0) {
        set_gan_error("Invalid class index");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gan_generate_class: capacity exceeded");
        return -1;
    }

    /* For BigGAN: embed class and concatenate with latent */
    gan_latent_t z;
    int rc = gan_sample_latent(bridge, LATENT_SPACE_Z, seed, &z);
    if (rc != 0) return rc;

    /* Modify latent based on class (placeholder) */
    if (z.data && z.dim > 0 && bridge->config.num_classes > 0) {
        float class_signal = (float)class_idx / (float)bridge->config.num_classes;
        for (uint32_t i = 0; i < z.dim; i++) {
            z.data[i] += class_signal * 0.1f * ((i % 2) ? 1.0f : -1.0f);
        }
    }

    /* Apply truncation */
    if (truncation < 1.0f) {
        truncation_params_t params = {
            .psi = truncation,
            .cutoff = bridge->config.truncation.cutoff
        };
        gan_truncate_latent(bridge, &z, &params);
    }

    rc = gan_generate(bridge, &z, output);
    gan_latent_free(&z);

    return rc;
}

//=============================================================================
// Encoding API
//=============================================================================

int gan_encode(gan_bridge_t* bridge,
                const visual_image_t* image,
                latent_space_t space,
                gan_latent_t* latent)
{
    if (!bridge || !image || !latent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_encode: required parameter is NULL (bridge, image, latent)");
        return -1;
    }
    if (!bridge->encoder) {
        set_gan_error("Encoder not loaded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_encode: bridge->encoder is NULL");
        return -1;
    }

    memset(latent, 0, sizeof(gan_latent_t));

    /* In production: run encoder network */
    /* Placeholder: compute latent from image statistics */

    uint32_t dim = 0;
    switch (space) {
        case LATENT_SPACE_Z:
            dim = bridge->config.latent_dim;
            break;
        case LATENT_SPACE_W:
        case LATENT_SPACE_W_PLUS:
            dim = bridge->config.w_dim;
            break;
        default:
            dim = bridge->config.w_dim;
            break;
    }

    latent->dim = dim;
    latent->space = space;
    latent->num_layers = (space == LATENT_SPACE_W_PLUS) ?
                         bridge->config.num_layers : 1;
    latent->owns_data = true;

    size_t total_dim = (size_t)dim * latent->num_layers;
    latent->data = nimcp_calloc(total_dim, sizeof(float));
    if (!latent->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_encode: latent->data is NULL");
        return -1;
    }

    /* Compute simple statistics from image */
    float mean_r = 0, mean_g = 0, mean_b = 0;
    size_t num_pixels = (size_t)image->width * image->height;

    for (size_t i = 0; i < num_pixels; i++) {
        mean_r += image->pixels[i * image->channels + 0];
        if (image->channels > 1) mean_g += image->pixels[i * image->channels + 1];
        if (image->channels > 2) mean_b += image->pixels[i * image->channels + 2];
    }

    mean_r /= (float)num_pixels * 255.0f;
    mean_g /= (float)num_pixels * 255.0f;
    mean_b /= (float)num_pixels * 255.0f;

    /* Fill latent with statistics-derived values */
    for (size_t i = 0; i < total_dim; i++) {
        float base = (mean_r + mean_g + mean_b) / 3.0f - 0.5f;
        float variation = sinf((float)i * 0.1f) * 0.5f;
        latent->data[i] = base * 2.0f + variation;
    }

    return 0;
}

//=============================================================================
// Style Mixing API
//=============================================================================

int gan_style_mix(gan_bridge_t* bridge,
                   const gan_latent_t* source,
                   const gan_latent_t* target,
                   uint32_t crossover_layer,
                   visual_image_t* output)
{
    if (!bridge || !source || !target || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_style_mix: required parameter is NULL (bridge, source, target, output)");
        return -1;
    }

    /* Create mixed latent */
    gan_latent_t mixed;
    memset(&mixed, 0, sizeof(gan_latent_t));

    mixed.dim = source->dim;
    mixed.space = LATENT_SPACE_W_PLUS;
    mixed.num_layers = bridge->config.num_layers;
    mixed.owns_data = true;

    size_t per_layer = mixed.dim;
    size_t total_dim = per_layer * mixed.num_layers;
    mixed.data = nimcp_calloc(total_dim, sizeof(float));
    if (!mixed.data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_style_mix: mixed is NULL");
        return -1;
    }

    /* Copy from source for coarse layers, target for fine layers */
    for (uint32_t layer = 0; layer < mixed.num_layers; layer++) {
        const gan_latent_t* src_latent = (layer < crossover_layer) ? source : target;
        float* dst = &mixed.data[layer * per_layer];

        /* Handle source latent potentially having fewer layers */
        uint32_t src_layer = layer;
        if (src_layer >= src_latent->num_layers) {
            src_layer = src_latent->num_layers > 0 ? src_latent->num_layers - 1 : 0;
        }

        const float* src = &src_latent->data[src_layer * src_latent->dim];

        for (uint32_t i = 0; i < per_layer && i < src_latent->dim; i++) {
            dst[i] = src[i];
        }
    }

    /* Generate */
    int rc = gan_generate(bridge, &mixed, output);
    gan_latent_free(&mixed);

    return rc;
}

int gan_blend_styles(gan_bridge_t* bridge,
                      const gan_latent_t* latent_a,
                      const gan_latent_t* latent_b,
                      const float* layer_weights,
                      uint32_t num_layers,
                      gan_latent_t* result)
{
    if (!bridge || !latent_a || !latent_b || !layer_weights || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_blend_styles: required parameter is NULL (bridge, latent_a, latent_b, layer_weights, result)");
        return -1;
    }

    memset(result, 0, sizeof(gan_latent_t));

    result->dim = latent_a->dim;
    result->space = LATENT_SPACE_W_PLUS;
    result->num_layers = num_layers;
    result->owns_data = true;

    size_t per_layer = result->dim;
    size_t total_dim = per_layer * num_layers;
    result->data = nimcp_calloc(total_dim, sizeof(float));
    if (!result->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_blend_styles: result->data is NULL");
        return -1;
    }

    for (uint32_t layer = 0; layer < num_layers; layer++) {
        float w = layer_weights[layer];
        w = fmaxf(0.0f, fminf(1.0f, w));

        float* dst = &result->data[layer * per_layer];

        uint32_t layer_a = layer < latent_a->num_layers ? layer :
                          (latent_a->num_layers > 0 ? latent_a->num_layers - 1 : 0);
        uint32_t layer_b = layer < latent_b->num_layers ? layer :
                          (latent_b->num_layers > 0 ? latent_b->num_layers - 1 : 0);

        const float* src_a = &latent_a->data[layer_a * latent_a->dim];
        const float* src_b = &latent_b->data[layer_b * latent_b->dim];

        for (uint32_t i = 0; i < per_layer; i++) {
            float va = i < latent_a->dim ? src_a[i] : 0.0f;
            float vb = i < latent_b->dim ? src_b[i] : 0.0f;
            dst[i] = va * (1.0f - w) + vb * w;
        }
    }

    return 0;
}

//=============================================================================
// Editing API
//=============================================================================

int gan_edit_latent(const gan_latent_t* latent,
                     const float* direction,
                     float magnitude,
                     gan_latent_t* result)
{
    if (!latent || !direction || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_edit_latent: required parameter is NULL (latent, direction, result)");
        return -1;
    }

    memset(result, 0, sizeof(gan_latent_t));

    result->dim = latent->dim;
    result->space = latent->space;
    result->num_layers = latent->num_layers;
    result->owns_data = true;

    size_t total_dim = (size_t)result->dim * result->num_layers;
    result->data = nimcp_calloc(total_dim, sizeof(float));
    if (!result->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gan_edit_latent: result->data is NULL");
        return -1;
    }

    /* Add direction scaled by magnitude */
    for (size_t i = 0; i < total_dim; i++) {
        result->data[i] = latent->data[i] + direction[i % latent->dim] * magnitude;
    }

    return 0;
}

//=============================================================================
// Batch API
//=============================================================================

int gan_generate_batch(gan_bridge_t* bridge,
                        const gan_latent_t* latents,
                        uint32_t num_latents,
                        visual_image_t* outputs)
{
    if (!bridge || !latents || !outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_generate_batch: required parameter is NULL (bridge, latents, outputs)");
        return -1;
    }

    for (uint32_t i = 0; i < num_latents; i++) {
        int rc = gan_generate(bridge, &latents[i], &outputs[i]);
        if (rc != 0) {
            outputs[i].pixels = NULL;
        }
    }

    return 0;
}

int gan_generate_interpolation(gan_bridge_t* bridge,
                                const gan_latent_t* start,
                                const gan_latent_t* end,
                                uint32_t num_steps,
                                visual_image_t* outputs)
{
    if (!bridge || !start || !end || !outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gan_generate_interpolation: required parameter is NULL (bridge, start, end, outputs)");
        return -1;
    }

    for (uint32_t i = 0; i < num_steps; i++) {
        float t = (num_steps > 1) ? (float)i / (float)(num_steps - 1) : 0.0f;

        gan_latent_t interp;
        int rc = gan_interpolate_latent(start, end, t, &interp);
        if (rc != 0) continue;

        rc = gan_generate(bridge, &interp, &outputs[i]);
        gan_latent_free(&interp);

        if (rc != 0) {
            outputs[i].pixels = NULL;
        }
    }

    return 0;
}

//=============================================================================
// Utility API
//=============================================================================

bool gan_bridge_ready(const gan_bridge_t* bridge)
{
    return bridge && bridge->onnx_runtime;
}

const char* gan_bridge_model_info(const gan_bridge_t* bridge)
{
    static char info[NIMCP_ERROR_BUFFER_SIZE];

    if (!bridge) return "Not initialized";

    const char* type_name;
    switch (bridge->config.type) {
        case GAN_TYPE_STYLEGAN2: type_name = "StyleGAN2"; break;
        case GAN_TYPE_STYLEGAN3: type_name = "StyleGAN3"; break;
        case GAN_TYPE_BIGGAN:    type_name = "BigGAN"; break;
        case GAN_TYPE_VQGAN:     type_name = "VQGAN"; break;
        case GAN_TYPE_PGAN:      type_name = "Progressive GAN"; break;
        case GAN_TYPE_CUSTOM:    type_name = "Custom"; break;
        default:                 type_name = "Unknown"; break;
    }

    snprintf(info, sizeof(info), "%s (z=%u, w=%u, %u layers, %ux%u output)",
             type_name, bridge->config.latent_dim, bridge->config.w_dim,
             bridge->config.num_layers, bridge->config.output_size,
             bridge->config.output_size);

    return info;
}

const char* gan_bridge_get_error(const gan_bridge_t* bridge)
{
    (void)bridge;
    return g_gan_error[0] ? g_gan_error : NULL;
}

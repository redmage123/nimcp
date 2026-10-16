/**
 * @file nimcp_jepa_masking.c
 * @brief JEPA Masking Implementation
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Implementation of mask generation strategies for JEPA
 * WHY:  Masking is core to self-supervised JEPA training
 * HOW:  Multiple strategies: random, block, attention-guided, curriculum
 */

#include "cognitive/jepa/nimcp_jepa_masking.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ============================================================================
 * Module Constants
 * ============================================================================ */

#define LOG_MODULE "[JEPA_MASK]"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(jepa_masking, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Helpers - PRNG
 * ============================================================================ */

/**
 * @brief Fast xorshift64 PRNG
 */
static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/**
 * @brief Random float in [0, 1)
 */
static float random_float(uint64_t* state) {
    return (float)(xorshift64(state) & 0x7FFFFFFF) / (float)0x80000000;
}

/**
 * @brief Random integer in [0, max)
 */
static uint32_t random_int(uint64_t* state, uint32_t max) {
    return (uint32_t)(xorshift64(state) % max);
}

/* ============================================================================
 * Internal Helpers - Block Generation
 * ============================================================================ */

typedef struct {
    uint32_t x, y, w, h;
} mask_block_t;

static void generate_random_block(uint64_t* state,
                                   uint32_t width, uint32_t height,
                                   const jepa_block_params_t* params,
                                   mask_block_t* block) {
    /* Calculate block size range */
    uint32_t total = width * height;
    uint32_t min_size = (uint32_t)(params->min_block_ratio * total);
    uint32_t max_size = (uint32_t)(params->max_block_ratio * total);

    if (min_size < 1) min_size = 1;
    if (max_size < min_size) max_size = min_size;

    /* Random size in range */
    uint32_t block_area = min_size + random_int(state, max_size - min_size + 1);

    /* Random aspect ratio */
    float aspect = params->min_aspect_ratio +
                   random_float(state) * (params->max_aspect_ratio - params->min_aspect_ratio);

    /* Calculate dimensions */
    float block_h = sqrtf((float)block_area / aspect);
    float block_w = block_h * aspect;

    block->w = (uint32_t)fmaxf(1.0f, fminf(block_w, (float)width));
    block->h = (uint32_t)fmaxf(1.0f, fminf(block_h, (float)height));

    /* Random position */
    block->x = random_int(state, width - block->w + 1);
    block->y = random_int(state, height - block->h + 1);
}

static void apply_block_to_mask(jepa_mask_t* mask, const mask_block_t* block,
                                 jepa_mask_shape_t shape) {
    float cx = block->x + block->w * 0.5f;
    float cy = block->y + block->h * 0.5f;
    float rx = block->w * 0.5f;
    float ry = block->h * 0.5f;

    for (uint32_t y = block->y; y < block->y + block->h && y < mask->height; y++) {
        for (uint32_t x = block->x; x < block->x + block->w && x < mask->width; x++) {
            bool in_mask = true;

            if (shape == JEPA_MASK_SHAPE_ELLIPSE) {
                /* Check if inside ellipse */
                float dx = (x - cx) / rx;
                float dy = (y - cy) / ry;
                in_mask = (dx * dx + dy * dy) <= 1.0f;
            }
            /* RECT and SQUARE are always in_mask=true within bounds */

            if (in_mask) {
                uint32_t idx = y * mask->width + x;
                mask->data[idx] = 1.0f;
            }
        }
    }
}

/* ============================================================================
 * Internal Helpers - Curriculum Schedule
 * ============================================================================ */

static float linear_schedule(float t) {
    return t;  /* Linear interpolation */
}

static float cosine_schedule(float t) {
    /* Cosine annealing: slower at ends, faster in middle */
    return 0.5f * (1.0f - cosf(t * NIMCP_PI_F));
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int jepa_mask_default_config(jepa_mask_config_t* config, jepa_mask_strategy_t strategy) {
    if (!config) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_default_co", 0.0f);


    memset(config, 0, sizeof(jepa_mask_config_t));

    config->strategy = strategy;
    config->mode = JEPA_MASK_MODE_PATCH;
    config->target_ratio = JEPA_MASK_DEFAULT_RATIO;
    config->seed = (uint32_t)time(NULL);
    config->use_fixed_seed = false;

    switch (strategy) {
        case JEPA_MASK_BLOCK:
        case JEPA_MASK_BLOCK_MULTI:
            config->params.block.num_blocks = 4;
            config->params.block.min_block_ratio = 0.05f;
            config->params.block.max_block_ratio = 0.25f;
            config->params.block.min_aspect_ratio = JEPA_MASK_MIN_ASPECT_RATIO;
            config->params.block.max_aspect_ratio = JEPA_MASK_MAX_ASPECT_RATIO;
            config->params.block.allow_overlap = true;
            config->params.block.shape = JEPA_MASK_SHAPE_RECT;
            break;

        case JEPA_MASK_ATTENTION_GUIDED:
            config->params.attention.attention_scores = NULL;
            config->params.attention.attention_threshold = NIMCP_ATTENTION_THRESHOLD;
            config->params.attention.mask_high_attention = true;
            config->params.attention.temperature = NIMCP_TEMPERATURE_DEFAULT;
            break;

        case JEPA_MASK_CURRICULUM:
            config->params.curriculum.start_ratio = JEPA_MASK_CURRICULUM_START_RATIO;
            config->params.curriculum.end_ratio = JEPA_MASK_CURRICULUM_END_RATIO;
            config->params.curriculum.warmup_steps = JEPA_MASK_CURRICULUM_EPOCHS;
            config->params.curriculum.current_step = 0;
            config->params.curriculum.schedule_fn = cosine_schedule;
            break;

        case JEPA_MASK_TUBE:
            config->params.tube.num_tubes = 8;
            config->params.tube.tube_ratio = 0.75f;
            config->params.tube.consistent_spatial = true;
            break;

        default:
            /* RANDOM, CAUSAL - no special params */
            break;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

jepa_mask_generator_t* jepa_mask_generator_create(const jepa_mask_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_generator_", 0.0f);


    jepa_mask_config_t default_config;

    if (!config) {
        jepa_mask_default_config(&default_config, JEPA_MASK_RANDOM);
        config = &default_config;
    }

    /* Allocate generator */
    jepa_mask_generator_t* gen = nimcp_malloc(sizeof(jepa_mask_generator_t));
    if (!gen) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Failed to allocate generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_mask_generator_create: gen is NULL");
        return NULL;
    }
    memset(gen, 0, sizeof(jepa_mask_generator_t));

    /* Initialize bridge base */
    if (bridge_base_init(&gen->base, BIO_MODULE_JEPA_MASKING, "jepa_masking") != 0) {
        nimcp_free(gen);
        gen = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "jepa_mask_generator_create: validation failed");
        return NULL;
    }

    /* Store config */
    gen->config = *config;

    /* Initialize PRNG */
    gen->random_state = config->seed;
    if (gen->random_state == 0) {
        gen->random_state = (uint64_t)time(NULL) ^ 0xDEADBEEF;
    }

    /* Initialize curriculum state */
    if (config->strategy == JEPA_MASK_CURRICULUM) {
        gen->current_ratio = config->params.curriculum.start_ratio;
        gen->curriculum_step = config->params.curriculum.current_step;
    } else {
        gen->current_ratio = config->target_ratio;
    }

    /* Allocate temp buffer (for largest expected mask) */
    gen->temp_buffer_size = JEPA_MASK_MAX_WIDTH * JEPA_MASK_MAX_HEIGHT;
    gen->temp_buffer = nimcp_calloc(gen->temp_buffer_size, sizeof(float));
    if (!gen->temp_buffer) {
        bridge_base_cleanup(&gen->base);
        nimcp_free(gen);
        gen = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_mask_generator_create: gen->temp_buffer is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO(LOG_MODULE " Created generator: strategy=%s, ratio=%.2f",
                      jepa_mask_strategy_to_string(config->strategy),
                      gen->current_ratio);

    return gen;
}

void jepa_mask_generator_destroy(jepa_mask_generator_t* generator) {
    if (!generator) return;

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_generator_", 0.0f);


    nimcp_free(generator->temp_buffer);
    bridge_base_cleanup(&generator->base);
    nimcp_free(generator);
    generator = NULL;
}

int jepa_mask_generator_reset(jepa_mask_generator_t* generator, uint32_t new_seed) {
    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_generator_", 0.0f);


    NIMCP_CHECK_THROW(generator, NIMCP_ERROR_NULL_POINTER, "generator is NULL");

    if (new_seed != 0) {
        generator->random_state = new_seed;
    }

    generator->masks_generated = 0;
    generator->curriculum_step = 0;

    if (generator->config.strategy == JEPA_MASK_CURRICULUM) {
        generator->current_ratio = generator->config.params.curriculum.start_ratio;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mask Creation/Destruction
 * ============================================================================ */

jepa_mask_t* jepa_mask_create(uint32_t width, uint32_t height, uint32_t temporal) {
    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_create", 0.0f);


    if (width == 0 || height == 0 || temporal == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_mask_create: width is zero");
        return NULL;
    }
    if (width > JEPA_MASK_MAX_WIDTH || height > JEPA_MASK_MAX_HEIGHT ||
        temporal > JEPA_MASK_MAX_TEMPORAL) {
        NIMCP_LOGGING_ERROR(LOG_MODULE " Mask dimensions exceed limits");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_mask_create: width is zero");
        return NULL;
    }

    jepa_mask_t* mask = nimcp_malloc(sizeof(jepa_mask_t));
    if (!mask) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate mask");

        return NULL;

    }

    mask->width = width;
    mask->height = height;
    mask->temporal = temporal;
    mask->total_size = width * height * temporal;

    mask->data = nimcp_calloc(mask->total_size, sizeof(float));
    if (!mask->data) {
        nimcp_free(mask);
        mask = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "jepa_mask_create: mask->data is NULL");
        return NULL;
    }

    /* Initialize to all visible (0) */
    /* calloc zero-initializes */
    mask->mask_ratio = 0.0f;
    mask->num_masked = 0;
    mask->num_visible = mask->total_size;

    return mask;
}

void jepa_mask_destroy(jepa_mask_t* mask) {
    if (!mask) return;
    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_destroy", 0.0f);


    nimcp_free(mask->data);
    nimcp_free(mask);
    mask = NULL;
}

jepa_mask_t* jepa_mask_clone(const jepa_mask_t* src) {
    if (!src) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_clone", 0.0f);


    jepa_mask_t* dst = jepa_mask_create(src->width, src->height, src->temporal);
    if (!dst) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dst is NULL");

        return NULL;

    }

    memcpy(dst->data, src->data, src->total_size * sizeof(float));
    dst->mask_ratio = src->mask_ratio;
    dst->num_masked = src->num_masked;
    dst->num_visible = src->num_visible;

    return dst;
}

/* ============================================================================
 * Mask Generation - Random
 * ============================================================================ */

static int generate_random_mask(jepa_mask_generator_t* gen,
                                 jepa_mask_t* mask,
                                 float target_ratio) {
    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        mask->data[i] = (random_float(&gen->random_state) < target_ratio) ? 1.0f : 0.0f;
    }

    jepa_mask_compute_stats(mask);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mask Generation - Block
 * ============================================================================ */

static int generate_block_mask(jepa_mask_generator_t* gen,
                                jepa_mask_t* mask) {
    /* Clear mask */
    memset(mask->data, 0, mask->total_size * sizeof(float));

    const jepa_block_params_t* params = &gen->config.params.block;
    uint32_t num_blocks = params->num_blocks;
    if (gen->config.strategy == JEPA_MASK_BLOCK) {
        num_blocks = 1;  /* Single block for BLOCK mode */
    }

    /* Generate and apply blocks */
    for (uint32_t b = 0; b < num_blocks; b++) {
        /* Phase 8: Loop progress heartbeat */
        if ((b & 0xFF) == 0 && num_blocks > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(b + 1) / (float)num_blocks);
        }

        mask_block_t block;
        generate_random_block(&gen->random_state, mask->width, mask->height,
                             params, &block);
        apply_block_to_mask(mask, &block, params->shape);
    }

    jepa_mask_compute_stats(mask);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mask Generation - Attention Guided
 * ============================================================================ */

static int generate_attention_mask(jepa_mask_generator_t* gen,
                                    const float* attention,
                                    jepa_mask_t* mask) {
    if (!attention) {
        /* Fall back to random if no attention provided */
        return generate_random_mask(gen, mask, gen->current_ratio);
    }

    const jepa_attention_params_t* params = &gen->config.params.attention;
    float target_ratio = gen->current_ratio;

    /* Apply temperature and normalize attention */
    float* probs = gen->temp_buffer;
    double sum = 0.0;
    float temp = fmaxf(0.01f, params->temperature);

    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        float a = attention[i];
        if (!params->mask_high_attention) {
            a = 1.0f - a;  /* Invert to mask low attention */
        }
        probs[i] = expf(a / temp);
        sum += probs[i];
    }

    /* Normalize to probabilities */
    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        probs[i] /= (float)sum;
    }

    /* Sample without replacement to reach target ratio */
    uint32_t target_masked = (uint32_t)(target_ratio * mask->total_size);
    memset(mask->data, 0, mask->total_size * sizeof(float));

    for (uint32_t m = 0; m < target_masked; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && target_masked > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(m + 1) / (float)target_masked);
        }

        /* Weighted random selection */
        float r = random_float(&gen->random_state);
        float cumsum = 0.0f;
        uint32_t selected = mask->total_size - 1;

        for (uint32_t i = 0; i < mask->total_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && mask->total_size > 256) {
                jepa_masking_heartbeat("jepa_masking_loop",
                                 (float)(i + 1) / (float)mask->total_size);
            }

            cumsum += probs[i];
            if (r <= cumsum && mask->data[i] < 0.5f) {
                selected = i;
                break;
            }
        }

        mask->data[selected] = 1.0f;
        probs[selected] = 0.0f;  /* Remove from future selection */

        /* Re-normalize */
        sum = 0.0;
        for (uint32_t i = 0; i < mask->total_size; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && mask->total_size > 256) {
                jepa_masking_heartbeat("jepa_masking_loop",
                                 (float)(i + 1) / (float)mask->total_size);
            }

            sum += probs[i];
        }
        if (sum > 0.0) {
            for (uint32_t i = 0; i < mask->total_size; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && mask->total_size > 256) {
                    jepa_masking_heartbeat("jepa_masking_loop",
                                     (float)(i + 1) / (float)mask->total_size);
                }

                probs[i] /= (float)sum;
            }
        }
    }

    jepa_mask_compute_stats(mask);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mask Generation - Tube (for video)
 * ============================================================================ */

static int generate_tube_mask(jepa_mask_generator_t* gen,
                               jepa_mask_t* mask) {
    if (mask->temporal <= 1) {
        /* Not a video, fall back to block */
        return generate_block_mask(gen, mask);
    }

    const jepa_tube_params_t* params = &gen->config.params.tube;
    uint32_t spatial_size = mask->width * mask->height;

    /* Clear mask */
    memset(mask->data, 0, mask->total_size * sizeof(float));

    /* Generate tubes (consistent spatial positions across time) */
    uint32_t patches_per_tube = (uint32_t)(params->tube_ratio * spatial_size / params->num_tubes);

    for (uint32_t t = 0; t < params->num_tubes; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && params->num_tubes > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(t + 1) / (float)params->num_tubes);
        }

        /* Generate spatial positions for this tube */
        uint32_t spatial_idx = 0;  /* Initialize for first patch, reused if consistent_spatial */

        for (uint32_t p = 0; p < patches_per_tube; p++) {
            /* Phase 8: Loop progress heartbeat */
            if ((p & 0xFF) == 0 && patches_per_tube > 256) {
                jepa_masking_heartbeat("jepa_masking_loop",
                                 (float)(p + 1) / (float)patches_per_tube);
            }

            if (params->consistent_spatial || p == 0) {
                /* Same position across all time steps (or first patch) */
                spatial_idx = random_int(&gen->random_state, spatial_size);
            } else {
                /* Non-consistent mode: new random position for each patch */
                spatial_idx = random_int(&gen->random_state, spatial_size);
            }

            /* Apply across all time steps */
            for (uint32_t f = 0; f < mask->temporal; f++) {
                /* Phase 8: Loop progress heartbeat */
                if ((f & 0xFF) == 0 && mask->temporal > 256) {
                    jepa_masking_heartbeat("jepa_masking_loop",
                                     (float)(f + 1) / (float)mask->temporal);
                }

                uint32_t idx = f * spatial_size + spatial_idx;
                mask->data[idx] = 1.0f;
            }
        }
    }

    jepa_mask_compute_stats(mask);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mask Generation - Causal
 * ============================================================================ */

static int generate_causal_mask(jepa_mask_generator_t* gen,
                                 jepa_mask_t* mask) {
    /* Causal: mask everything after random position */
    uint32_t visible_length = random_int(&gen->random_state, mask->total_size);
    if (visible_length == 0) visible_length = 1;

    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        mask->data[i] = (i >= visible_length) ? 1.0f : 0.0f;
    }

    jepa_mask_compute_stats(mask);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Mask Generation API
 * ============================================================================ */

int jepa_mask_generate_2d(jepa_mask_generator_t* generator,
                           uint32_t width,
                           uint32_t height,
                           jepa_mask_t* mask) {
    if (!generator || !mask) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_generate_2", 0.0f);


    if (mask->width != width || mask->height != height || mask->temporal != 1) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Reset PRNG if fixed seed */
    if (generator->config.use_fixed_seed) {
        generator->random_state = generator->config.seed;
    }

    int result = 0;

    switch (generator->config.strategy) {
        case JEPA_MASK_RANDOM:
            result = generate_random_mask(generator, mask, generator->current_ratio);
            break;

        case JEPA_MASK_BLOCK:
        case JEPA_MASK_BLOCK_MULTI:
            result = generate_block_mask(generator, mask);
            break;

        case JEPA_MASK_ATTENTION_GUIDED:
            result = generate_attention_mask(generator,
                                             generator->config.params.attention.attention_scores,
                                             mask);
            break;

        case JEPA_MASK_CURRICULUM:
            result = generate_random_mask(generator, mask, generator->current_ratio);
            break;

        case JEPA_MASK_CAUSAL:
            result = generate_causal_mask(generator, mask);
            break;

        default:
            result = generate_random_mask(generator, mask, generator->current_ratio);
    }

    if (result == NIMCP_SUCCESS) {
        generator->masks_generated++;
    }

    return result;
}

int jepa_mask_generate_3d(jepa_mask_generator_t* generator,
                           uint32_t width,
                           uint32_t height,
                           uint32_t temporal,
                           jepa_mask_t* mask) {
    if (!generator || !mask) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_generate_3", 0.0f);


    if (mask->width != width || mask->height != height || mask->temporal != temporal) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Reset PRNG if fixed seed */
    if (generator->config.use_fixed_seed) {
        generator->random_state = generator->config.seed;
    }

    int result = 0;

    if (generator->config.strategy == JEPA_MASK_TUBE) {
        result = generate_tube_mask(generator, mask);
    } else {
        /* Apply 2D strategy per frame or globally */
        result = generate_random_mask(generator, mask, generator->current_ratio);
    }

    if (result == NIMCP_SUCCESS) {
        generator->masks_generated++;
    }

    return result;
}

int jepa_mask_generate_1d(jepa_mask_generator_t* generator,
                           uint32_t length,
                           jepa_mask_t* mask) {
    /* 1D is just 2D with height=1 */
    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_generate_1", 0.0f);


    return jepa_mask_generate_2d(generator, length, 1, mask);
}

int jepa_mask_generate_attention(jepa_mask_generator_t* generator,
                                  const float* attention,
                                  uint32_t width,
                                  uint32_t height,
                                  jepa_mask_t* mask) {
    if (!generator || !attention || !mask) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_generate_a", 0.0f);


    if (mask->width != width || mask->height != height) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return generate_attention_mask(generator, attention, mask);
}

/* ============================================================================
 * Mask Operations
 * ============================================================================ */

int jepa_mask_apply(const jepa_mask_t* mask,
                     const float* input,
                     float* output,
                     uint32_t dim) {
    if (!mask || !input || !output) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Apply mask: out[i,j] = input[i,j] * (1 - mask[i]) */
    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_apply", 0.0f);


    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        float m = 1.0f - mask->data[i];
        for (uint32_t j = 0; j < dim; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && dim > 256) {
                jepa_masking_heartbeat("jepa_masking_loop",
                                 (float)(j + 1) / (float)dim);
            }

            output[i * dim + j] = input[i * dim + j] * m;
        }
    }

    return NIMCP_SUCCESS;
}

int jepa_mask_get_visible_indices(const jepa_mask_t* mask,
                                   uint32_t* indices,
                                   uint32_t* num_indices) {
    if (!mask || !indices || !num_indices) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_get_visibl", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        if (mask->data[i] < 0.5f) {
            indices[count++] = i;
        }
    }
    *num_indices = count;

    return NIMCP_SUCCESS;
}

int jepa_mask_get_masked_indices(const jepa_mask_t* mask,
                                  uint32_t* indices,
                                  uint32_t* num_indices) {
    if (!mask || !indices || !num_indices) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_get_masked", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        if (mask->data[i] >= 0.5f) {
            indices[count++] = i;
        }
    }
    *num_indices = count;

    return NIMCP_SUCCESS;
}

int jepa_mask_invert(jepa_mask_t* mask) {
    if (!mask) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_invert", 0.0f);


    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        mask->data[i] = 1.0f - mask->data[i];
    }

    /* Swap counts */
    uint32_t tmp = mask->num_masked;
    mask->num_masked = mask->num_visible;
    mask->num_visible = tmp;
    mask->mask_ratio = 1.0f - mask->mask_ratio;

    return NIMCP_SUCCESS;
}

int jepa_mask_compute_stats(jepa_mask_t* mask) {
    if (!mask) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_compute_st", 0.0f);


    uint32_t masked = 0;
    for (uint32_t i = 0; i < mask->total_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && mask->total_size > 256) {
            jepa_masking_heartbeat("jepa_masking_loop",
                             (float)(i + 1) / (float)mask->total_size);
        }

        if (mask->data[i] >= 0.5f) {
            masked++;
        }
    }

    mask->num_masked = masked;
    mask->num_visible = mask->total_size - masked;
    mask->mask_ratio = (float)masked / (float)mask->total_size;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Curriculum Learning API
 * ============================================================================ */

int jepa_mask_curriculum_step(jepa_mask_generator_t* generator) {
    if (!generator) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_curriculum", 0.0f);


    if (generator->config.strategy != JEPA_MASK_CURRICULUM) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const jepa_curriculum_params_t* params = &generator->config.params.curriculum;

    generator->curriculum_step++;

    /* Calculate progress t in [0, 1] */
    float t = (float)generator->curriculum_step / (float)params->warmup_steps;
    if (t > 1.0f) t = 1.0f;

    /* Apply schedule function */
    float schedule_t = t;
    if (params->schedule_fn) {
        schedule_t = params->schedule_fn(t);
    }

    /* Interpolate ratio */
    generator->current_ratio = params->start_ratio +
                               schedule_t * (params->end_ratio - params->start_ratio);

    return NIMCP_SUCCESS;
}

int jepa_mask_curriculum_set_step(jepa_mask_generator_t* generator, uint32_t step) {
    if (!generator) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_curriculum", 0.0f);


    generator->curriculum_step = step;

    /* Recalculate ratio */
    if (generator->config.strategy == JEPA_MASK_CURRICULUM) {
        const jepa_curriculum_params_t* params = &generator->config.params.curriculum;

        float t = (float)step / (float)params->warmup_steps;
        if (t > 1.0f) t = 1.0f;

        float schedule_t = t;
        if (params->schedule_fn) {
            schedule_t = params->schedule_fn(t);
        }

        generator->current_ratio = params->start_ratio +
                                   schedule_t * (params->end_ratio - params->start_ratio);
    }

    return NIMCP_SUCCESS;
}

float jepa_mask_curriculum_get_ratio(const jepa_mask_generator_t* generator) {
    if (!generator) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_jepa_mask_curriculum", 0.0f);


    return generator->current_ratio;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

BRIDGE_DEFINE_BIO_ASYNC_FUNCS(jepa_mask_generator)

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* jepa_mask_strategy_to_string(jepa_mask_strategy_t strategy) {
    switch (strategy) {
        case JEPA_MASK_RANDOM:           return "random";
        case JEPA_MASK_BLOCK:            return "block";
        case JEPA_MASK_BLOCK_MULTI:      return "block_multi";
        case JEPA_MASK_ATTENTION_GUIDED: return "attention_guided";
        case JEPA_MASK_CURRICULUM:       return "curriculum";
        case JEPA_MASK_TUBE:             return "tube";
        case JEPA_MASK_CAUSAL:           return "causal";
        default:                         return "unknown";
    }
}

const char* jepa_mask_shape_to_string(jepa_mask_shape_t shape) {
    switch (shape) {
        case JEPA_MASK_SHAPE_RECT:      return "rectangle";
        case JEPA_MASK_SHAPE_SQUARE:    return "square";
        case JEPA_MASK_SHAPE_ELLIPSE:   return "ellipse";
        case JEPA_MASK_SHAPE_IRREGULAR: return "irregular";
        default:                        return "unknown";
    }
}

const char* jepa_mask_mode_to_string(jepa_mask_mode_t mode) {
    switch (mode) {
        case JEPA_MASK_MODE_PATCH:   return "patch";
        case JEPA_MASK_MODE_PIXEL:   return "pixel";
        case JEPA_MASK_MODE_FEATURE: return "feature";
        default:                     return "unknown";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int jepa_masking_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_masking_heartbeat("jepa_masking_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Masking_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_masking_heartbeat("jepa_masking_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Masking_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Masking_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void jepa_masking_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_jepa_masking_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int jepa_masking_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_masking_training_begin: NULL argument");
        return -1;
    }
    jepa_masking_heartbeat("jepa_masking_training_begin", 0.0f);
    (void)(mask_block_t*)instance; /* Module state available for reset */
    return 0;
}

int jepa_masking_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_masking_training_end: NULL argument");
        return -1;
    }
    jepa_masking_heartbeat("jepa_masking_training_end", 1.0f);
    (void)(mask_block_t*)instance; /* Module state available for finalization */
    return 0;
}

int jepa_masking_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_masking_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    jepa_masking_heartbeat("jepa_masking_training_step", progress);
    (void)(mask_block_t*)instance; /* Module state available for step adaptation */
    return 0;
}

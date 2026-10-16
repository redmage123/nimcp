#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_snn_ternary.c - Ternary Weights for Spiking Neural Networks
//=============================================================================
/**
 * @file nimcp_snn_ternary.c
 * @brief Implementation of ternary synaptic weight support for SNN
 *
 * WHAT: Ternary {-1, 0, +1} weight representation for SNN synapses
 * WHY:  20x memory reduction for large-scale SNN simulations
 * HOW:  Pack 5 ternary weights per byte, discrete STDP state machine
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include "snn/nimcp_snn_ternary.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_constants.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_ternary)

//=============================================================================
// Lifecycle Functions
//=============================================================================

void snn_ternary_default_config(snn_ternary_config_t* config) {
    if (!config) return;

    config->ltp_threshold = SNN_TERNARY_LTP_THRESHOLD;
    config->ltd_threshold = SNN_TERNARY_LTD_THRESHOLD;
    config->positive_scale = SNN_TERNARY_WEIGHT_POSITIVE_SCALE;
    config->negative_scale = SNN_TERNARY_WEIGHT_NEGATIVE_SCALE;
    config->accumulation_decay = NIMCP_EMA_DECAY_DEFAULT;
    config->use_stochastic_round = false;
    config->update_interval = 100;
    config->pack_mode = TERNARY_PACK_BASE243;
}

snn_ternary_weight_matrix_t* snn_ternary_create(
    uint32_t pre_size,
    uint32_t post_size,
    const snn_ternary_config_t* config
) {
    if (pre_size == 0 || post_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid dimensions: pre_size=%u, post_size=%u", pre_size, post_size);
        return NULL;
    }

    snn_ternary_weight_matrix_t* weights = (snn_ternary_weight_matrix_t*)
        nimcp_calloc(1, sizeof(snn_ternary_weight_matrix_t));
    if (!weights) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(snn_ternary_weight_matrix_t),
                          "Failed to allocate snn_ternary_weight_matrix_t");
        return NULL;
    }

    /* Apply config or defaults */
    snn_ternary_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        snn_ternary_default_config(&cfg);
    }

    weights->magic = SNN_TERNARY_MAGIC;
    weights->pre_size = pre_size;
    weights->post_size = post_size;
    weights->mode = cfg.pack_mode;
    weights->positive_scale = cfg.positive_scale;
    weights->negative_scale = cfg.negative_scale;
    weights->ltp_threshold = cfg.ltp_threshold;
    weights->ltd_threshold = cfg.ltd_threshold;

    /* Create weight matrix */
    weights->weights = trit_matrix_create(pre_size, post_size, cfg.pack_mode);
    if (!weights->weights) {
        nimcp_free(weights);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_ternary_create: weights->weights is NULL");
        return NULL;
    }

    /* Create accumulator for STDP */
    size_t numel = (size_t)pre_size * post_size;
    weights->accumulated_delta = (float*)nimcp_calloc(numel, sizeof(float));
    if (!weights->accumulated_delta) {
        trit_matrix_destroy(weights->weights);
        nimcp_free(weights);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_ternary_create: weights->accumulated_delta is NULL");
        return NULL;
    }

    return weights;
}

void snn_ternary_destroy(snn_ternary_weight_matrix_t* weights) {
    if (!weights) return;
    if (weights->magic != SNN_TERNARY_MAGIC) return;

    trit_matrix_destroy(weights->weights);
    nimcp_free(weights->accumulated_delta);

    weights->magic = 0;
    nimcp_free(weights);
}

snn_ternary_weight_matrix_t* snn_ternary_from_floats(
    const float* float_weights,
    uint32_t pre_size,
    uint32_t post_size,
    float threshold,
    const snn_ternary_config_t* config
) {
    if (!float_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL float_weights in snn_ternary_from_floats");
        return NULL;
    }
    if (pre_size == 0 || post_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid dimensions: pre_size=%u, post_size=%u", pre_size, post_size);
        return NULL;
    }

    snn_ternary_weight_matrix_t* weights = snn_ternary_create(pre_size, post_size, config);
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_ternary_from_floats: weights is NULL");
        return NULL;
    }

    /* Quantize float weights to ternary */
    for (uint32_t pre = 0; pre < pre_size; pre++) {
        for (uint32_t post = 0; post < post_size; post++) {
            float w = float_weights[pre * post_size + post];
            trit_t t = trit_from_float_threshold(w, threshold);
            trit_matrix_set(weights->weights, pre, post, t);
        }
    }

    return weights;
}

int snn_ternary_to_floats(
    const snn_ternary_weight_matrix_t* weights,
    float* output
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Invalid weights in snn_ternary_to_floats");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL output in snn_ternary_to_floats");
        return -1;
    }

    for (uint32_t pre = 0; pre < weights->pre_size; pre++) {
        for (uint32_t post = 0; post < weights->post_size; post++) {
            trit_t t = trit_matrix_get(weights->weights, pre, post);
            float w = 0.0f;
            if (t == TRIT_POSITIVE) w = weights->positive_scale;
            else if (t == TRIT_NEGATIVE) w = weights->negative_scale;
            output[pre * weights->post_size + post] = w;
        }
    }

    return 0;
}

//=============================================================================
// Forward Pass
//=============================================================================

int snn_ternary_forward(
    const snn_ternary_weight_matrix_t* weights,
    const uint8_t* input_spikes,
    float* output
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Invalid weights in snn_ternary_forward");
        return -1;
    }
    if (!input_spikes || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL input_spikes or output in snn_ternary_forward");
        return -1;
    }

    /* Clear output */
    memset(output, 0, weights->post_size * sizeof(float));

    /* Sparse accumulation: only process spiking neurons */
    for (uint32_t pre = 0; pre < weights->pre_size; pre++) {
        if (!input_spikes[pre]) continue;  /* Skip non-spiking neurons */

        for (uint32_t post = 0; post < weights->post_size; post++) {
            trit_t t = trit_matrix_get(weights->weights, pre, post);
            if (t == TRIT_POSITIVE) {
                output[post] += weights->positive_scale;
            } else if (t == TRIT_NEGATIVE) {
                output[post] += weights->negative_scale;
            }
            /* TRIT_UNKNOWN (0) contributes nothing */
        }
    }

    return 0;
}

int snn_ternary_forward_float(
    const snn_ternary_weight_matrix_t* weights,
    const float* input,
    float* output
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Invalid weights in snn_ternary_forward_float");
        return -1;
    }
    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL input or output in snn_ternary_forward_float");
        return -1;
    }

    /* Clear output */
    memset(output, 0, weights->post_size * sizeof(float));

    /* Full matrix-vector multiply */
    for (uint32_t pre = 0; pre < weights->pre_size; pre++) {
        float in_val = input[pre];
        if (fabsf(in_val) < 1e-7f) continue;  /* Skip near-zero inputs */

        for (uint32_t post = 0; post < weights->post_size; post++) {
            trit_t t = trit_matrix_get(weights->weights, pre, post);
            if (t == TRIT_POSITIVE) {
                output[post] += weights->positive_scale * in_val;
            } else if (t == TRIT_NEGATIVE) {
                output[post] += weights->negative_scale * in_val;
            }
        }
    }

    return 0;
}

//=============================================================================
// STDP Learning
//=============================================================================

int snn_ternary_stdp_update(
    snn_ternary_weight_matrix_t* weights,
    uint32_t pre_idx,
    uint32_t post_idx,
    float delta
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_ternary_stdp_update: weights is NULL");
        return -1;
    }
    if (pre_idx >= weights->pre_size || post_idx >= weights->post_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_ternary_stdp_update: capacity exceeded");
        return -1;
    }

    size_t idx = (size_t)pre_idx * weights->post_size + post_idx;
    weights->accumulated_delta[idx] += delta;
    weights->n_updates++;

    /* Check if threshold crossed */
    float acc = weights->accumulated_delta[idx];
    trit_t current = trit_matrix_get(weights->weights, pre_idx, post_idx);
    trit_t new_weight = current;

    if (current == TRIT_UNKNOWN) {
        /* From silent state */
        if (acc >= weights->ltp_threshold) {
            new_weight = TRIT_POSITIVE;
            weights->accumulated_delta[idx] = 0.0f;
            weights->n_ltp_events++;
        } else if (acc <= weights->ltd_threshold) {
            new_weight = TRIT_NEGATIVE;
            weights->accumulated_delta[idx] = 0.0f;
            weights->n_ltd_events++;
        }
    } else if (current == TRIT_POSITIVE) {
        /* From excitatory state */
        if (acc <= weights->ltd_threshold) {
            new_weight = TRIT_UNKNOWN;
            weights->accumulated_delta[idx] = 0.0f;
            weights->n_ltd_events++;
        }
    } else if (current == TRIT_NEGATIVE) {
        /* From inhibitory state */
        if (acc >= weights->ltp_threshold) {
            new_weight = TRIT_UNKNOWN;
            weights->accumulated_delta[idx] = 0.0f;
            weights->n_ltp_events++;
        }
    }

    if (new_weight != current) {
        trit_matrix_set(weights->weights, pre_idx, post_idx, new_weight);
        return 1;  /* Weight changed */
    }

    return 0;  /* No change */
}

int snn_ternary_stdp_batch(
    snn_ternary_weight_matrix_t* weights,
    const float* deltas
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_ternary_stdp_batch: weights is NULL");
        return -1;
    }
    if (!deltas) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_ternary_stdp_batch: deltas is NULL");
        return -1;
    }

    int n_changes = 0;
    for (uint32_t pre = 0; pre < weights->pre_size; pre++) {
        for (uint32_t post = 0; post < weights->post_size; post++) {
            float delta = deltas[pre * weights->post_size + post];
            if (fabsf(delta) > 1e-7f) {
                int result = snn_ternary_stdp_update(weights, pre, post, delta);
                if (result > 0) n_changes++;
            }
        }
    }

    return n_changes;
}

void snn_ternary_decay(
    snn_ternary_weight_matrix_t* weights,
    float decay_factor
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) return;
    if (!weights->accumulated_delta) return;

    size_t numel = (size_t)weights->pre_size * weights->post_size;
    for (size_t i = 0; i < numel; i++) {
        weights->accumulated_delta[i] *= decay_factor;
    }
}

int snn_ternary_discretize(snn_ternary_weight_matrix_t* weights) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_ternary_discretize: weights is NULL");
        return -1;
    }

    int n_changes = 0;
    for (uint32_t pre = 0; pre < weights->pre_size; pre++) {
        for (uint32_t post = 0; post < weights->post_size; post++) {
            size_t idx = (size_t)pre * weights->post_size + post;
            float acc = weights->accumulated_delta[idx];

            if (fabsf(acc) < 1e-7f) continue;

            trit_t current = trit_matrix_get(weights->weights, pre, post);
            trit_t new_weight = current;

            if (current == TRIT_UNKNOWN) {
                if (acc >= weights->ltp_threshold) {
                    new_weight = TRIT_POSITIVE;
                    weights->n_ltp_events++;
                } else if (acc <= weights->ltd_threshold) {
                    new_weight = TRIT_NEGATIVE;
                    weights->n_ltd_events++;
                }
            } else if (current == TRIT_POSITIVE) {
                if (acc <= weights->ltd_threshold) {
                    new_weight = TRIT_UNKNOWN;
                    weights->n_ltd_events++;
                }
            } else if (current == TRIT_NEGATIVE) {
                if (acc >= weights->ltp_threshold) {
                    new_weight = TRIT_UNKNOWN;
                    weights->n_ltp_events++;
                }
            }

            if (new_weight != current) {
                trit_matrix_set(weights->weights, pre, post, new_weight);
                n_changes++;
            }

            weights->accumulated_delta[idx] = 0.0f;
        }
    }

    return n_changes;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_ternary_get_stats(
    const snn_ternary_weight_matrix_t* weights,
    snn_ternary_stats_t* stats
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_ternary_get_stats: weights is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_ternary_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(snn_ternary_stats_t));

    size_t n_pos, n_unk, n_neg;
    trit_matrix_count(weights->weights, &n_pos, &n_unk, &n_neg);

    stats->n_positive = (uint32_t)n_pos;
    stats->n_unknown = (uint32_t)n_unk;
    stats->n_negative = (uint32_t)n_neg;

    size_t total = n_pos + n_unk + n_neg;
    stats->sparsity = (total > 0) ? (float)n_unk / (float)total : 0.0f;
    stats->balance = (n_neg > 0) ? (float)n_pos / (float)n_neg : 0.0f;

    stats->n_ltp_total = weights->n_ltp_events;
    stats->n_ltd_total = weights->n_ltd_events;

    /* Memory calculation */
    size_t numel = (size_t)weights->pre_size * weights->post_size;
    stats->memory_bytes = sizeof(snn_ternary_weight_matrix_t)
                        + trit_packed_bytes(numel, weights->mode)
                        + numel * sizeof(float);  /* accumulated_delta */

    size_t float_bytes = numel * sizeof(float);
    stats->compression_ratio = (float)float_bytes / (float)stats->memory_bytes;

    return 0;
}

void snn_ternary_print_stats(const snn_ternary_weight_matrix_t* weights) {
    snn_ternary_stats_t stats;
    if (snn_ternary_get_stats(weights, &stats) != 0) {
        printf("snn_ternary: Invalid weight matrix\n");
        return;
    }

    printf("=== SNN Ternary Weight Statistics ===\n");
    printf("Dimensions: %u x %u (%u weights)\n",
           weights->pre_size, weights->post_size,
           weights->pre_size * weights->post_size);
    printf("Distribution:\n");
    printf("  +1 (excitatory): %u (%.1f%%)\n",
           stats.n_positive,
           100.0f * stats.n_positive / (stats.n_positive + stats.n_unknown + stats.n_negative));
    printf("   0 (silent):     %u (%.1f%%)\n",
           stats.n_unknown,
           100.0f * stats.n_unknown / (stats.n_positive + stats.n_unknown + stats.n_negative));
    printf("  -1 (inhibitory): %u (%.1f%%)\n",
           stats.n_negative,
           100.0f * stats.n_negative / (stats.n_positive + stats.n_unknown + stats.n_negative));
    printf("Sparsity: %.1f%%\n", 100.0f * stats.sparsity);
    printf("E/I balance: %.2f\n", stats.balance);
    printf("Learning events: LTP=%lu, LTD=%lu\n",
           (unsigned long)stats.n_ltp_total,
           (unsigned long)stats.n_ltd_total);
    printf("Memory: %zu bytes (%.1fx compression)\n",
           stats.memory_bytes, stats.compression_ratio);
}

//=============================================================================
// Serialization
//=============================================================================

size_t snn_ternary_serialize(
    const snn_ternary_weight_matrix_t* weights,
    uint8_t* buffer,
    size_t buffer_size
) {
    if (!weights || weights->magic != SNN_TERNARY_MAGIC) return 0;

    size_t numel = (size_t)weights->pre_size * weights->post_size;
    size_t packed_bytes = trit_packed_bytes(numel, weights->mode);

    /* Calculate total size */
    size_t total_size = sizeof(uint32_t) * 4  /* magic, pre, post, mode */
                      + sizeof(float) * 4     /* scales, thresholds */
                      + packed_bytes;          /* weight data */

    if (!buffer) return total_size;
    if (buffer_size < total_size) return 0;

    uint8_t* ptr = buffer;

    /* Header */
    memcpy(ptr, &weights->magic, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    memcpy(ptr, &weights->pre_size, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    memcpy(ptr, &weights->post_size, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    uint32_t mode = (uint32_t)weights->mode;
    memcpy(ptr, &mode, sizeof(uint32_t)); ptr += sizeof(uint32_t);

    /* Scales and thresholds */
    memcpy(ptr, &weights->positive_scale, sizeof(float)); ptr += sizeof(float);
    memcpy(ptr, &weights->negative_scale, sizeof(float)); ptr += sizeof(float);
    memcpy(ptr, &weights->ltp_threshold, sizeof(float)); ptr += sizeof(float);
    memcpy(ptr, &weights->ltd_threshold, sizeof(float)); ptr += sizeof(float);

    /* Weight data */
    if (weights->mode == TERNARY_PACK_NONE) {
        for (size_t i = 0; i < numel; i++) {
            trit_t t = trit_matrix_get(weights->weights, i / weights->post_size,
                                        i % weights->post_size);
            *ptr++ = (uint8_t)(t + 1);
        }
    } else {
        /* Copy packed data directly */
        memcpy(ptr, weights->weights->data.packed, packed_bytes);
    }

    return total_size;
}

snn_ternary_weight_matrix_t* snn_ternary_deserialize(
    const uint8_t* buffer,
    size_t buffer_size
) {
    if (!buffer || buffer_size < sizeof(uint32_t) * 4 + sizeof(float) * 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_ternary_deserialize: buffer is NULL");
        return NULL;
    }

    const uint8_t* ptr = buffer;

    /* Read header */
    uint32_t magic, pre_size, post_size, mode;
    memcpy(&magic, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    memcpy(&pre_size, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    memcpy(&post_size, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
    memcpy(&mode, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);

    if (magic != SNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_ternary_deserialize: validation failed");
        return NULL;
    }

    /* Read scales and thresholds */
    float pos_scale, neg_scale, ltp_thresh, ltd_thresh;
    memcpy(&pos_scale, ptr, sizeof(float)); ptr += sizeof(float);
    memcpy(&neg_scale, ptr, sizeof(float)); ptr += sizeof(float);
    memcpy(&ltp_thresh, ptr, sizeof(float)); ptr += sizeof(float);
    memcpy(&ltd_thresh, ptr, sizeof(float)); ptr += sizeof(float);

    /* Create weight matrix */
    snn_ternary_config_t config;
    snn_ternary_default_config(&config);
    config.pack_mode = (ternary_pack_mode_t)mode;
    config.positive_scale = pos_scale;
    config.negative_scale = neg_scale;
    config.ltp_threshold = ltp_thresh;
    config.ltd_threshold = ltd_thresh;

    snn_ternary_weight_matrix_t* weights = snn_ternary_create(pre_size, post_size, &config);
    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_ternary_deserialize: weights is NULL");
        return NULL;
    }

    /* Read weight data */
    size_t numel = (size_t)pre_size * post_size;
    if (mode == TERNARY_PACK_NONE) {
        for (size_t i = 0; i < numel; i++) {
            trit_t t = (trit_t)(*ptr++ - 1);
            trit_matrix_set(weights->weights, i / post_size, i % post_size, t);
        }
    } else {
        size_t packed_bytes = trit_packed_bytes(numel, (ternary_pack_mode_t)mode);
        memcpy(weights->weights->data.packed, ptr, packed_bytes);
    }

    return weights;
}

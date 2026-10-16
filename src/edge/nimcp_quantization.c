/**
 * @file nimcp_quantization.c
 * @brief Tensor quantization for edge deployment (INT8/FP16 weight compression).
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "nimcp.h"
#include "api/nimcp_api_internal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* Internal access */
extern neural_network_t adaptive_network_get_base_network(adaptive_network_t network);

/* ============================================================================
 * Helper: compute min/max of a float array
 * ============================================================================ */

static void compute_min_max(const float* data, uint32_t n, float* out_min, float* out_max) {
    float mn = FLT_MAX;
    float mx = -FLT_MAX;
    for (uint32_t i = 0; i < n; i++) {
        if (data[i] < mn) mn = data[i];
        if (data[i] > mx) mx = data[i];
    }
    *out_min = mn;
    *out_max = mx;
}

/* ============================================================================
 * nimcp_quantize_tensor
 * ============================================================================ */

nimcp_quantized_tensor_t* nimcp_quantize_tensor(
    const float* data, uint32_t num_elements,
    nimcp_quantization_t precision,
    const float* calibration_min, const float* calibration_max)
{
    if (!data || num_elements == 0) {
        return NULL;
    }

    if (precision == NIMCP_QUANT_NONE) {
        return NULL;
    }

    nimcp_quantized_tensor_t* qt = (nimcp_quantized_tensor_t*)nimcp_calloc(1, sizeof(nimcp_quantized_tensor_t));
    if (!qt) {
        return NULL;
    }

    qt->num_elements = num_elements;
    qt->num_channels = 1;
    qt->precision = precision;
    qt->per_channel_params = NULL;

    /* Compute or use provided min/max */
    float mn, mx;
    if (calibration_min && calibration_max) {
        mn = *calibration_min;
        mx = *calibration_max;
    } else {
        compute_min_max(data, num_elements, &mn, &mx);
    }

    /* Allocate quantized data */
    qt->data = (int8_t*)nimcp_malloc(num_elements * sizeof(int8_t));
    if (!qt->data) {
        nimcp_free(qt);
        return NULL;
    }

    /* Allocate per-tensor params (single entry) */
    qt->per_channel_params = (nimcp_quant_params_t*)nimcp_calloc(1, sizeof(nimcp_quant_params_t));
    if (!qt->per_channel_params) {
        nimcp_free(qt->data);
        nimcp_free(qt);
        return NULL;
    }

    nimcp_quant_params_t* params = &qt->per_channel_params[0];
    params->min_val = mn;
    params->max_val = mx;

    switch (precision) {
        case NIMCP_QUANT_INT8_SYMMETRIC: {
            /* Symmetric: scale = max(|min|, |max|) / 127, zero_point = 0 */
            float abs_max = fabsf(mn);
            if (fabsf(mx) > abs_max) {
                abs_max = fabsf(mx);
            }
            if (abs_max < 1e-10f) {
                abs_max = 1e-10f;
            }
            params->scale = abs_max / 127.0f;
            params->zero_point = 0;

            float inv_scale = 1.0f / params->scale;
            for (uint32_t i = 0; i < num_elements; i++) {
                float val = data[i] * inv_scale;
                if (val > 127.0f) val = 127.0f;
                if (val < -127.0f) val = -127.0f;
                qt->data[i] = (int8_t)roundf(val);
            }
            break;
        }

        case NIMCP_QUANT_INT8_AFFINE: {
            /* Affine: scale = (max - min) / 255, zero_point = round(-min / scale) */
            float range = mx - mn;
            if (range < 1e-10f) {
                range = 1e-10f;
            }
            params->scale = range / 255.0f;
            params->zero_point = (int32_t)roundf(-mn / params->scale);

            /* Clamp zero_point to [0, 255] */
            if (params->zero_point < 0) params->zero_point = 0;
            if (params->zero_point > 255) params->zero_point = 255;

            float inv_scale = 1.0f / params->scale;
            for (uint32_t i = 0; i < num_elements; i++) {
                float val = data[i] * inv_scale + (float)params->zero_point;
                if (val > 127.0f) val = 127.0f;
                if (val < -128.0f) val = -128.0f;
                qt->data[i] = (int8_t)roundf(val);
            }
            break;
        }

        case NIMCP_QUANT_FP16: {
            /*
             * FP16 emulation: reduce precision by zeroing lower mantissa bits.
             * Store as INT8 with scale = range/255 (lossy approximation).
             * A real FP16 path would use __fp16 or _Float16 types.
             */
            float abs_max = fabsf(mn);
            if (fabsf(mx) > abs_max) {
                abs_max = fabsf(mx);
            }
            if (abs_max < 1e-10f) {
                abs_max = 1e-10f;
            }
            params->scale = abs_max / 127.0f;
            params->zero_point = 0;

            float inv_scale = 1.0f / params->scale;
            for (uint32_t i = 0; i < num_elements; i++) {
                float val = data[i] * inv_scale;
                if (val > 127.0f) val = 127.0f;
                if (val < -127.0f) val = -127.0f;
                qt->data[i] = (int8_t)roundf(val);
            }
            break;
        }

        case NIMCP_QUANT_INT4:
        case NIMCP_QUANT_TERNARY:
            /* Fallback to symmetric INT8 for unsupported modes */
            {
                float abs_max = fabsf(mn);
                if (fabsf(mx) > abs_max) abs_max = fabsf(mx);
                if (abs_max < 1e-10f) abs_max = 1e-10f;
                params->scale = abs_max / 127.0f;
                params->zero_point = 0;
                float inv_scale = 1.0f / params->scale;
                for (uint32_t i = 0; i < num_elements; i++) {
                    float val = data[i] * inv_scale;
                    if (val > 127.0f) val = 127.0f;
                    if (val < -127.0f) val = -127.0f;
                    qt->data[i] = (int8_t)roundf(val);
                }
            }
            break;

        default:
            nimcp_free(qt->per_channel_params);
            nimcp_free(qt->data);
            nimcp_free(qt);
            return NULL;
    }

    return qt;
}

/* ============================================================================
 * nimcp_quantized_tensor_destroy
 * ============================================================================ */

void nimcp_quantized_tensor_destroy(nimcp_quantized_tensor_t* qt) {
    if (!qt) {
        return;
    }
    if (qt->data) {
        nimcp_free(qt->data);
    }
    if (qt->per_channel_params) {
        nimcp_free(qt->per_channel_params);
    }
    nimcp_free(qt);
}

/* ============================================================================
 * nimcp_dequantize_tensor
 * ============================================================================ */

int nimcp_dequantize_tensor(const nimcp_quantized_tensor_t* qt, float* output) {
    if (!qt || !output || !qt->data) {
        return -1;
    }

    if (!qt->per_channel_params) {
        return -1;
    }

    const nimcp_quant_params_t* params = &qt->per_channel_params[0];
    float scale = params->scale;
    int32_t zp = params->zero_point;

    for (uint32_t i = 0; i < qt->num_elements; i++) {
        output[i] = ((float)qt->data[i] - (float)zp) * scale;
    }

    return 0;
}

/* ============================================================================
 * nimcp_brain_quantize — quantize synapse weights in-place
 * ============================================================================ */

static void quantize_weight_int8(float* w) {
    /* Round to INT8 precision: 256 levels in [-1, 1] range */
    float clamped = *w;
    if (clamped > 1.0f) clamped = 1.0f;
    if (clamped < -1.0f) clamped = -1.0f;
    *w = roundf(clamped * 127.0f) / 127.0f;
}

static void quantize_weight_ternary(float* w) {
    if (*w > 0.5f) *w = 1.0f;
    else if (*w < -0.5f) *w = -1.0f;
    else *w = 0.0f;
}

int nimcp_brain_quantize(nimcp_brain_t brain, const nimcp_quantize_config_t* config) {
    if (!brain || !config) {
        return -1;
    }

    /* BBB input validation */
    if (brain->internal_brain && brain->internal_brain->bbb_enabled &&
        brain->internal_brain->bbb_system) {
        bbb_validation_result_t bbb_result;
        if (!bbb_validate_input(brain->internal_brain->bbb_system,
                                 config, sizeof(*config), &bbb_result)) {
            LOG_WARN("Edge API: BBB rejected quantize input");
            return -1;
        }
    }

    if (!brain->internal_brain || !brain->internal_brain->network) {
        LOG_WARN("Edge: brain has no network to quantize");
        return -1;
    }

    neural_network_t nn = adaptive_network_get_base_network(
        brain->internal_brain->network);
    if (!nn) {
        LOG_WARN("Edge: could not access base neural network");
        return -1;
    }

    uint32_t num_neurons = neural_network_get_num_neurons(nn);
    uint32_t quantized = 0;
    bool is_ternary = (config->weight_precision == NIMCP_QUANT_TERNARY);

    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* n = neural_network_get_neuron(nn, i);
        if (!n) continue;

        /* Quantize embedded outgoing synapse weights */
        for (uint32_t j = 0; j < n->outgoing.embedded_count; j++) {
            if (is_ternary) {
                quantize_weight_ternary(&n->outgoing.embedded[j].weight);
            } else {
                quantize_weight_int8(&n->outgoing.embedded[j].weight);
            }
            quantized++;
        }

        /* Quantize overflow synapse weights */
        for (uint32_t j = 0; j < n->outgoing.overflow_count; j++) {
            if (is_ternary) {
                quantize_weight_ternary(&n->outgoing.overflow[j].weight);
            } else {
                quantize_weight_int8(&n->outgoing.overflow[j].weight);
            }
            quantized++;
        }
    }

    LOG_INFO("Edge: quantized %u synapse weights (precision=%d) across %u neurons",
             quantized, (int)config->weight_precision, num_neurons);

    return 0;
}

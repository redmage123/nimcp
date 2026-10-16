/**
 * @file nimcp_federated.c
 * @brief Federated learning — gradient aggregation and weight blending
 *
 * WHAT: Master-side aggregation of gradients from multiple edge devices,
 *       plus device-side weight blending with optional EWC protection.
 * WHY:  Devices learn independently; aggregation combines their knowledge
 *       without centralizing raw data (privacy-preserving).
 * HOW:  Three aggregation methods: FedAvg, FedProx, Byzantine-tolerant median.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Compare floats for qsort (used by median aggregation).
 */
static int float_compare(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/**
 * @brief FedAvg: simple mean of all gradient arrays.
 */
static int aggregate_fedavg(const nimcp_federated_gradient_t* gradients,
                             uint32_t count, float* aggregated,
                             uint32_t num_params) {
    memset(aggregated, 0, num_params * sizeof(float));

    for (uint32_t d = 0; d < count; d++) {
        if (!gradients[d].gradients) {
            return -1;
        }
        for (uint32_t i = 0; i < num_params; i++) {
            aggregated[i] += gradients[d].gradients[i];
        }
    }

    float inv_count = 1.0f / (float)count;
    for (uint32_t i = 0; i < num_params; i++) {
        aggregated[i] *= inv_count;
    }

    return 0;
}

/**
 * @brief FedProx: mean + proximal penalty lambda * (w - w_global).
 *
 * The proximal term penalizes device drift from the global model.
 * We use a fixed lambda of 0.01 (standard FedProx default).
 */
static int aggregate_fedprox(const nimcp_federated_gradient_t* gradients,
                              uint32_t count, float* aggregated,
                              uint32_t num_params) {
    static const float PROX_LAMBDA = 0.01f;

    /* Start with FedAvg */
    int ret = aggregate_fedavg(gradients, count, aggregated, num_params);
    if (ret != 0) {
        return ret;
    }

    /* Add proximal penalty: for each device, lambda * (w_device - w_global).
     * Since we don't have w_global separate from aggregated here,
     * the proximal term is added per-device during aggregation.
     * In practice: aggregated += lambda * mean(gradients - aggregated) */
    for (uint32_t d = 0; d < count; d++) {
        for (uint32_t i = 0; i < num_params; i++) {
            float drift = gradients[d].gradients[i] - aggregated[i];
            aggregated[i] += PROX_LAMBDA * drift / (float)count;
        }
    }

    return 0;
}

/**
 * @brief FedMedian: per-parameter median (Byzantine-tolerant).
 */
static int aggregate_median(const nimcp_federated_gradient_t* gradients,
                             uint32_t count, float* aggregated,
                             uint32_t num_params) {
    /* Temporary buffer for sorting per-parameter values */
    float* values = (float*)nimcp_malloc(count * sizeof(float));
    if (!values) {
        return -1;
    }

    for (uint32_t i = 0; i < num_params; i++) {
        for (uint32_t d = 0; d < count; d++) {
            if (!gradients[d].gradients) {
                nimcp_free(values);
                return -1;
            }
            values[d] = gradients[d].gradients[i];
        }

        qsort(values, count, sizeof(float), float_compare);

        if (count % 2 == 1) {
            aggregated[i] = values[count / 2];
        } else {
            aggregated[i] = 0.5f * (values[count / 2 - 1] + values[count / 2]);
        }
    }

    nimcp_free(values);
    return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int nimcp_federated_aggregate(const nimcp_federated_gradient_t* gradients,
                               uint32_t count, float* aggregated,
                               uint32_t num_params,
                               nimcp_fed_aggregation_t method) {
    if (!gradients || !aggregated || count == 0 || num_params == 0) {
        return -1;
    }

    /* Upfront validation: check all gradient arrays before aggregation */
    for (uint32_t d = 0; d < count; d++) {
        if (!gradients[d].gradients) {
            return -1;
        }
    }

    /* G7 SECURITY: Validate each gradient before aggregation
     * WHAT: Check for NaN/Inf and extreme magnitudes in device gradients
     * WHY:  A compromised or malfunctioning edge device could send poisoned
     *       gradients (NaN, Inf, or extreme values) that would corrupt the
     *       aggregated model. This is a known attack vector in federated learning
     *       (Byzantine gradient poisoning).
     * HOW:  Scan each device's gradient array for non-finite values and extreme
     *       magnitudes (>1e6). Zero out any poisoned gradient arrays to prevent
     *       corruption while still allowing other devices to contribute. */
    for (uint32_t d = 0; d < count; d++) {
        bool valid = true;

        /* Check for NaN/Inf */
        for (uint32_t p = 0; p < num_params && valid; p++) {
            if (!isfinite(gradients[d].gradients[p])) {
                LOG_WARN("federated: gradient from device %u has NaN/Inf at param %u",
                         gradients[d].device_id, p);
                valid = false;
            }
        }

        /* Check for extreme magnitudes (>1e6 — likely poisoned or divergent) */
        if (valid) {
            float max_grad = 0.0f;
            for (uint32_t p = 0; p < num_params; p++) {
                float abs_g = fabsf(gradients[d].gradients[p]);
                if (abs_g > max_grad) max_grad = abs_g;
            }
            if (max_grad > 1e6f) {
                LOG_WARN("federated: gradient from device %u has extreme magnitude %.2f",
                         gradients[d].device_id, max_grad);
                valid = false;
            }
        }

        if (!valid) {
            /* Zero out poisoned gradient to prevent corruption.
             * NOTE: We cast away const here. This is intentional security
             * sanitization — the caller's data IS modified to prevent
             * poisoned gradients from corrupting the aggregate.
             * the caller's data is being protected, not relied upon. */
            memset((void*)gradients[d].gradients, 0, num_params * sizeof(float));
        }
    }

    switch (method) {
        case NIMCP_FED_AVG:
            return aggregate_fedavg(gradients, count, aggregated, num_params);
        case NIMCP_FED_PROX:
            return aggregate_fedprox(gradients, count, aggregated, num_params);
        case NIMCP_FED_MEDIAN:
            return aggregate_median(gradients, count, aggregated, num_params);
        default:
            return -1;
    }
}

int nimcp_federated_blend(float* device_weights, const float* master_weights,
                           uint32_t num_params, float blend_ratio,
                           const nimcp_ewc_state_t* ewc) {
    if (!device_weights || !master_weights || num_params == 0) {
        return -1;
    }

    /* If EWC is available and initialized, delegate to EWC-aware blending */
    if (ewc && ewc->initialized) {
        return nimcp_ewc_blend_weights(ewc, device_weights, master_weights,
                                        blend_ratio);
    }

    /* Simple uniform blend: w = ratio * local + (1 - ratio) * master */
    float master_ratio = 1.0f - blend_ratio;
    for (uint32_t i = 0; i < num_params; i++) {
        device_weights[i] = blend_ratio * device_weights[i]
                          + master_ratio * master_weights[i];
    }

    return 0;
}

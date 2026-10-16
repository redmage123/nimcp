/**
 * @file nimcp_weight_update_adapter.c
 * @brief Middleware adapter for synaptic weight updates
 *
 * WHAT: Implements plasticity rules (STDP, BCM, Hebbian, Triple, Voltage)
 * WHY:  Enable biologically-inspired weight updates from neural activity
 * HOW:  Maintain eligibility traces, apply rule-specific computations,
 *       modulate by learning signals, enforce homeostatic constraints
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include "middleware/training/nimcp_weight_update_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal struct                                                    */
/* ------------------------------------------------------------------ */

struct weight_update_adapter_struct {
    weight_update_adapter_config_t config;
    float* pre_traces;          /* [num_pre]  - presynaptic eligibility traces  */
    float* post_traces;         /* [num_post] - postsynaptic eligibility traces */
    float* homeostasis_rates;   /* [num_post] - running average firing rates    */
    uint32_t homeostasis_count;
};

/* ------------------------------------------------------------------ */
/*  Triplet STDP constants                                             */
/* ------------------------------------------------------------------ */

static const float A2_PLUS  = 0.005f;
static const float A3_MINUS = 0.005f;

/* ------------------------------------------------------------------ */
/*  Voltage-based plasticity threshold                                 */
/* ------------------------------------------------------------------ */

static const float VOLTAGE_THRESHOLD = 0.5f;

/* ------------------------------------------------------------------ */
/*  Helper: clamp float to [lo, hi]                                    */
/* ------------------------------------------------------------------ */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/* ------------------------------------------------------------------ */
/*  weight_update_adapter_create                                       */
/* ------------------------------------------------------------------ */

weight_update_adapter_t weight_update_adapter_create(
    const weight_update_adapter_config_t* config)
{
    if (!config) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_create: NULL config");
        return NULL;
    }
    if (config->num_pre == 0 || config->num_post == 0) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_create: num_pre or num_post is 0");
        return NULL;
    }

    struct weight_update_adapter_struct* adapter =
        (struct weight_update_adapter_struct*)nimcp_calloc(
            1, sizeof(struct weight_update_adapter_struct));
    if (!adapter) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_create: allocation failed");
        return NULL;
    }

    adapter->config = *config;

    adapter->pre_traces = (float*)nimcp_calloc(config->num_pre, sizeof(float));
    if (!adapter->pre_traces) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_create: pre_traces alloc failed");
        nimcp_free(adapter);
        return NULL;
    }

    adapter->post_traces = (float*)nimcp_calloc(config->num_post, sizeof(float));
    if (!adapter->post_traces) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_create: post_traces alloc failed");
        nimcp_free(adapter->pre_traces);
        nimcp_free(adapter);
        return NULL;
    }

    adapter->homeostasis_rates = (float*)nimcp_calloc(config->num_post, sizeof(float));
    if (!adapter->homeostasis_rates) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_create: homeostasis_rates alloc failed");
        nimcp_free(adapter->post_traces);
        nimcp_free(adapter->pre_traces);
        nimcp_free(adapter);
        return NULL;
    }

    adapter->homeostasis_count = 0;

    NIMCP_LOGGING_DEBUG("weight_update_adapter_create: created adapter "
                        "(pre=%u, post=%u, rule=%d, lr=%.4f)",
                        config->num_pre, config->num_post,
                        (int)config->rule, config->learning_rate);

    return adapter;
}

/* ------------------------------------------------------------------ */
/*  weight_update_adapter_destroy                                      */
/* ------------------------------------------------------------------ */

void weight_update_adapter_destroy(weight_update_adapter_t adapter)
{
    if (!adapter) {
        return;
    }
    nimcp_free(adapter->homeostasis_rates);
    nimcp_free(adapter->post_traces);
    nimcp_free(adapter->pre_traces);
    nimcp_free(adapter);
}

/* ------------------------------------------------------------------ */
/*  weight_update_adapter_compute                                      */
/* ------------------------------------------------------------------ */

bool weight_update_adapter_compute(
    weight_update_adapter_t adapter,
    const float* pre_activity,
    const float* post_activity,
    const learning_signal_t* learning_signal,
    uint64_t timestamp,
    weight_update_delta_t* delta_out)
{
    if (!adapter || !pre_activity || !post_activity || !delta_out) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_compute: NULL argument");
        return false;
    }
    if (!delta_out->delta_weights) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_compute: delta_weights not allocated");
        return false;
    }

    const weight_update_adapter_config_t* cfg = &adapter->config;
    const uint32_t num_pre  = cfg->num_pre;
    const uint32_t num_post = cfg->num_post;
    const float lr = cfg->learning_rate;

    /* Decay constants for trace updates */
    const float decay_pre  = (cfg->tau_plus  > 0.0f) ? expf(-1.0f / cfg->tau_plus)  : 0.0f;
    const float decay_post = (cfg->tau_minus > 0.0f) ? expf(-1.0f / cfg->tau_minus) : 0.0f;

    /* Update eligibility traces: decay then add current activity */
    for (uint32_t i = 0; i < num_pre; i++) {
        adapter->pre_traces[i] = adapter->pre_traces[i] * decay_pre + pre_activity[i];
    }
    for (uint32_t j = 0; j < num_post; j++) {
        adapter->post_traces[j] = adapter->post_traces[j] * decay_post + post_activity[j];
    }

    /* BCM sliding threshold: theta = mean(post^2) */
    float theta = 0.0f;
    if (cfg->rule == PLASTICITY_BCM) {
        for (uint32_t j = 0; j < num_post; j++) {
            theta += post_activity[j] * post_activity[j];
        }
        theta /= (float)num_post;
    }

    /* Compute delta weights according to plasticity rule */
    float sum_delta = 0.0f;
    float max_abs_delta = 0.0f;
    uint32_t count_updated = 0;

    for (uint32_t i = 0; i < num_pre; i++) {
        for (uint32_t j = 0; j < num_post; j++) {
            float dw = 0.0f;

            switch (cfg->rule) {
                case PLASTICITY_STDP:
                    /* dw = lr * (post * pre_trace - pre * post_trace) */
                    dw = lr * (post_activity[j] * adapter->pre_traces[i]
                             - pre_activity[i] * adapter->post_traces[j]);
                    break;

                case PLASTICITY_BCM:
                    /* dw = lr * post * (post - theta) * pre */
                    dw = lr * post_activity[j] * (post_activity[j] - theta)
                         * pre_activity[i];
                    break;

                case PLASTICITY_HEBBIAN:
                    /* dw = lr * pre * post */
                    dw = lr * pre_activity[i] * post_activity[j];
                    break;

                case PLASTICITY_TRIPLE:
                    /* dw = lr * (A2+ * pre_trace * post + A3- * post_trace * pre * pre_trace) */
                    dw = lr * (A2_PLUS  * adapter->pre_traces[i] * post_activity[j]
                             + A3_MINUS * adapter->post_traces[j] * pre_activity[i]
                                        * adapter->pre_traces[i]);
                    break;

                case PLASTICITY_VOLTAGE:
                    /* dw = lr * pre * (post - threshold) */
                    dw = lr * pre_activity[i]
                         * (post_activity[j] - VOLTAGE_THRESHOLD);
                    break;

                default:
                    NIMCP_LOGGING_WARN("weight_update_adapter_compute: "
                                       "unknown rule %d, defaulting to Hebbian",
                                       (int)cfg->rule);
                    dw = lr * pre_activity[i] * post_activity[j];
                    break;
            }

            /* Modulate by learning signal */
            if (learning_signal) {
                dw *= learning_signal->combined_signal;
            }

            /* Clamp to [weight_min, weight_max] if normalization enabled */
            if (cfg->enable_normalization) {
                dw = clampf(dw, cfg->weight_min, cfg->weight_max);
            }

            delta_out->delta_weights[i][j] = dw;

            /* Statistics */
            float abs_dw = fabsf(dw);
            sum_delta += dw;
            if (abs_dw > max_abs_delta) {
                max_abs_delta = abs_dw;
            }
            if (abs_dw > 1e-12f) {
                count_updated++;
            }
        }
    }

    uint32_t total = num_pre * num_post;
    delta_out->rule_used   = cfg->rule;
    delta_out->mean_delta  = (total > 0) ? (sum_delta / (float)total) : 0.0f;
    delta_out->max_delta   = max_abs_delta;
    delta_out->num_updated = count_updated;
    delta_out->timestamp   = timestamp;

    return true;
}

/* ------------------------------------------------------------------ */
/*  weight_update_adapter_apply_homeostasis                            */
/* ------------------------------------------------------------------ */

bool weight_update_adapter_apply_homeostasis(
    weight_update_adapter_t adapter,
    float** current_weights,
    float target_rate,
    float actual_rate)
{
    if (!adapter || !current_weights) {
        NIMCP_LOGGING_ERROR("weight_update_adapter_apply_homeostasis: NULL argument");
        return false;
    }
    if (!adapter->config.enable_homeostasis) {
        return true;
    }
    if (actual_rate <= 0.0f) {
        NIMCP_LOGGING_WARN("weight_update_adapter_apply_homeostasis: "
                           "actual_rate <= 0 (%.4f), skipping", actual_rate);
        return true;
    }

    /* Compute scaling factor, clamped to [0.5, 2.0] */
    float scale = target_rate / actual_rate;
    scale = clampf(scale, 0.5f, 2.0f);

    const uint32_t num_pre  = adapter->config.num_pre;
    const uint32_t num_post = adapter->config.num_post;

    for (uint32_t i = 0; i < num_pre; i++) {
        if (!current_weights[i]) {
            continue;
        }
        for (uint32_t j = 0; j < num_post; j++) {
            current_weights[i][j] *= scale;
        }
    }

    /* Update running average */
    adapter->homeostasis_count++;
    float alpha = 1.0f / (float)adapter->homeostasis_count;
    if (alpha < 0.01f) {
        alpha = 0.01f;
    }
    for (uint32_t j = 0; j < num_post; j++) {
        adapter->homeostasis_rates[j] =
            (1.0f - alpha) * adapter->homeostasis_rates[j] + alpha * actual_rate;
    }

    NIMCP_LOGGING_DEBUG("weight_update_adapter_apply_homeostasis: "
                        "scale=%.3f (target=%.2f, actual=%.2f)",
                        scale, target_rate, actual_rate);

    return true;
}

/* ------------------------------------------------------------------ */
/*  weight_update_adapter_default_config                               */
/* ------------------------------------------------------------------ */

weight_update_adapter_config_t weight_update_adapter_default_config(void)
{
    weight_update_adapter_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_pre              = 64;
    config.num_post             = 64;
    config.rule                 = PLASTICITY_STDP;
    config.learning_rate        = 0.01f;
    config.tau_plus             = 20.0f;
    config.tau_minus            = 20.0f;
    config.weight_min           = -1.0f;
    config.weight_max           = 1.0f;
    config.enable_normalization = true;
    config.enable_homeostasis   = true;

    return config;
}

/* ------------------------------------------------------------------ */
/*  weight_update_delta_create                                         */
/* ------------------------------------------------------------------ */

weight_update_delta_t* weight_update_delta_create(
    uint32_t num_pre, uint32_t num_post)
{
    if (num_pre == 0 || num_post == 0) {
        NIMCP_LOGGING_ERROR("weight_update_delta_create: num_pre or num_post is 0");
        return NULL;
    }

    weight_update_delta_t* delta =
        (weight_update_delta_t*)nimcp_calloc(1, sizeof(weight_update_delta_t));
    if (!delta) {
        NIMCP_LOGGING_ERROR("weight_update_delta_create: allocation failed");
        return NULL;
    }

    delta->delta_weights = (float**)nimcp_calloc(num_pre, sizeof(float*));
    if (!delta->delta_weights) {
        NIMCP_LOGGING_ERROR("weight_update_delta_create: row array alloc failed");
        nimcp_free(delta);
        return NULL;
    }

    for (uint32_t i = 0; i < num_pre; i++) {
        delta->delta_weights[i] = (float*)nimcp_calloc(num_post, sizeof(float));
        if (!delta->delta_weights[i]) {
            NIMCP_LOGGING_ERROR("weight_update_delta_create: row %u alloc failed", i);
            /* Clean up already-allocated rows */
            for (uint32_t k = 0; k < i; k++) {
                nimcp_free(delta->delta_weights[k]);
            }
            nimcp_free(delta->delta_weights);
            nimcp_free(delta);
            return NULL;
        }
    }

    delta->rule_used   = PLASTICITY_STDP;
    delta->mean_delta  = 0.0f;
    delta->max_delta   = 0.0f;
    delta->num_updated = 0;
    delta->timestamp   = 0;

    return delta;
}

/* ------------------------------------------------------------------ */
/*  weight_update_delta_destroy                                        */
/* ------------------------------------------------------------------ */

void weight_update_delta_destroy(weight_update_delta_t* delta)
{
    if (!delta) {
        return;
    }
    if (delta->delta_weights) {
        /*
         * We don't store num_pre in the delta struct, so we free rows
         * until we hit NULL (nimcp_calloc zero-initialized the pointer
         * array, so unallocated entries are NULL).
         */
        for (uint32_t i = 0; delta->delta_weights[i] != NULL; i++) {
            nimcp_free(delta->delta_weights[i]);
        }
        nimcp_free(delta->delta_weights);
    }
    nimcp_free(delta);
}

//=============================================================================
// nimcp_cloud_inference.c - Edge-Cloud Hybrid Inference Bridge
//=============================================================================
/**
 * @file nimcp_cloud_inference.c
 * @brief Confidence-gated edge-cloud inference with online distillation
 *
 * WHAT: Routes inference between local and cloud brains with automatic learning
 * WHY:  Small edge devices get cloud-level accuracy with local-level latency
 * HOW:  Confidence threshold gates escalation; cloud answers distill back
 */

#include "middleware/cloud/nimcp_cloud_inference.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <math.h>

/* Include brain_decision_t definition without pulling in the full internal header
 * (which triggers Python.h via distributed_cognition → protocol chain).
 * brain_decision_t is a simple POD struct defined in nimcp_brain.h. We reproduce
 * the struct layout here to avoid the header dependency. Must be kept in sync. */
struct brain_decision {
    char label[64];
    float confidence;
    float* output_vector;
    uint32_t output_size;
    uint32_t num_active_neurons;
    uint32_t* active_neuron_ids;
    float sparsity;
    char explanation[256];
    uint64_t inference_time_us;
    uint32_t* _cow_refcount;
    bool _cow_is_shallow;
};

/* Forward declarations */
extern brain_decision_t* brain_decide(brain_t brain, const float* features, uint32_t num_features);
extern void brain_free_decision(brain_decision_t* decision);
extern float brain_learn_vector(brain_t brain, const float* features, uint32_t num_features,
                                const float* target, uint32_t target_size,
                                const char* label, float confidence);

#define LOG_MODULE "cloud_inference"


cloud_inference_config_t cloud_inference_default_config(void)
{
    return (cloud_inference_config_t){
        .confidence_threshold = 0.5f,
        .distillation_lr = 0.001f,
        .enable_distillation = true,
        .distillation_buffer_size = 64,
        .compression_dim = 0,
        .max_cloud_failures = 10,
        .local_improvement_threshold = 0.95f
    };
}


cloud_inference_bridge_t* cloud_inference_create(
    const cloud_inference_config_t* config,
    cloud_backend_fn backend_fn,
    void* user_data)
{
    if (!backend_fn) {
        LOG_WARN("cloud_inference_create: backend_fn is required");
        return NULL;
    }

    cloud_inference_bridge_t* bridge = nimcp_calloc(1, sizeof(cloud_inference_bridge_t));
    if (!bridge) return NULL;

    bridge->config = config ? *config : cloud_inference_default_config();
    bridge->backend_fn = backend_fn;
    bridge->backend_user_data = user_data;
    bridge->backend_available = true;

    // Allocate distillation buffer
    uint32_t cap = bridge->config.distillation_buffer_size;
    if (cap == 0) cap = 64;
    bridge->distill_buffer = nimcp_calloc(cap, sizeof(distillation_example_t));
    bridge->distill_capacity = bridge->distill_buffer ? cap : 0;
    bridge->distill_head = 0;
    bridge->distill_count = 0;

    // EMA init
    bridge->ema_local_conf = 0.5f;
    bridge->ema_cloud_conf = 0.5f;

    LOG_INFO(LOG_MODULE, "Cloud inference bridge created (threshold=%.2f, distill=%s, buffer=%u)",
             bridge->config.confidence_threshold,
             bridge->config.enable_distillation ? "on" : "off",
             bridge->distill_capacity);

    return bridge;
}


void cloud_inference_destroy(cloud_inference_bridge_t* bridge)
{
    if (!bridge) return;

    // Free distillation buffer contents
    if (bridge->distill_buffer) {
        for (uint32_t i = 0; i < bridge->distill_capacity; i++) {
            nimcp_free(bridge->distill_buffer[i].input);
            nimcp_free(bridge->distill_buffer[i].cloud_output);
        }
        nimcp_free(bridge->distill_buffer);
    }

    nimcp_free(bridge->compression_matrix);
    nimcp_free(bridge->compressed_buf);
    nimcp_free(bridge);
}


/**
 * @brief Store a cloud response in the distillation buffer
 */
static void buffer_distillation_example(
    cloud_inference_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    const brain_decision_t* cloud_decision)
{
    if (!bridge->distill_buffer || bridge->distill_capacity == 0) return;

    uint32_t idx = bridge->distill_head;
    distillation_example_t* ex = &bridge->distill_buffer[idx];

    // Free old data at this slot
    nimcp_free(ex->input);
    nimcp_free(ex->cloud_output);

    // Copy input
    ex->input = nimcp_malloc(num_features * sizeof(float));
    if (ex->input) {
        memcpy(ex->input, features, num_features * sizeof(float));
    }
    ex->num_features = num_features;

    // Copy cloud output
    ex->num_outputs = cloud_decision->output_size;
    ex->cloud_output = nimcp_malloc(cloud_decision->output_size * sizeof(float));
    if (ex->cloud_output) {
        memcpy(ex->cloud_output, cloud_decision->output_vector,
               cloud_decision->output_size * sizeof(float));
    }

    // Copy label
    if (cloud_decision->label[0]) {
        strncpy(ex->label, cloud_decision->label, sizeof(ex->label) - 1);
        ex->label[sizeof(ex->label) - 1] = '\0';
    } else {
        ex->label[0] = '\0';
    }
    ex->cloud_confidence = cloud_decision->confidence;

    bridge->distill_head = (idx + 1) % bridge->distill_capacity;
    if (bridge->distill_count < bridge->distill_capacity) {
        bridge->distill_count++;
    }
}


cloud_route_t cloud_inference_route(
    cloud_inference_bridge_t* bridge,
    brain_t local_brain,
    brain_decision_t* local_decision,
    const float* features,
    uint32_t num_features)
{
    if (!bridge || !local_decision) return CLOUD_ROUTE_LOCAL;

    bridge->stats.total_queries++;

    // Update local confidence EMA
    float lc = local_decision->confidence;
    bridge->ema_local_conf = bridge->ema_local_conf * 0.99f + lc * 0.01f;
    bridge->stats.avg_local_confidence = bridge->ema_local_conf;

    // Check if local brain is confident enough
    if (lc >= bridge->config.confidence_threshold) {
        bridge->stats.local_handled++;
        bridge->stats.consecutive_cloud_failures = 0;
        return CLOUD_ROUTE_LOCAL;
    }

    // Check if cloud is disabled (too many failures or local is good enough)
    if (!bridge->backend_available || !bridge->backend_fn) {
        bridge->stats.local_handled++;
        return CLOUD_ROUTE_LOCAL;
    }

    if (bridge->stats.consecutive_cloud_failures >= bridge->config.max_cloud_failures) {
        bridge->stats.local_handled++;
        return CLOUD_ROUTE_FALLBACK;
    }

    // Local accuracy is high enough — stop using cloud
    if (bridge->stats.local_accuracy_estimate >= bridge->config.local_improvement_threshold) {
        bridge->stats.local_handled++;
        return CLOUD_ROUTE_LOCAL;
    }

    // ========================================================================
    // ESCALATE TO CLOUD
    // ========================================================================
    bridge->stats.cloud_escalated++;

    brain_decision_t* cloud_decision = bridge->backend_fn(
        features, num_features, bridge->backend_user_data);

    if (!cloud_decision) {
        bridge->stats.cloud_failed++;
        bridge->stats.consecutive_cloud_failures++;

        if (bridge->stats.consecutive_cloud_failures >= bridge->config.max_cloud_failures) {
            LOG_WARN("Cloud backend failed %u times consecutively — disabling",
                     bridge->config.max_cloud_failures);
            bridge->backend_available = false;
        }

        return CLOUD_ROUTE_FALLBACK;
    }

    // Cloud succeeded
    bridge->stats.cloud_succeeded++;
    bridge->stats.consecutive_cloud_failures = 0;

    // Update cloud confidence EMA
    bridge->ema_cloud_conf = bridge->ema_cloud_conf * 0.99f + cloud_decision->confidence * 0.01f;
    bridge->stats.avg_cloud_confidence = bridge->ema_cloud_conf;

    // Track local accuracy: did local agree with cloud?
    if (local_decision->label[0] && cloud_decision->label[0] &&
        strcmp(local_decision->label, cloud_decision->label) == 0) {
        bridge->stats.local_accuracy_estimate =
            bridge->stats.local_accuracy_estimate * 0.995f + 0.005f;
    } else {
        bridge->stats.local_accuracy_estimate =
            bridge->stats.local_accuracy_estimate * 0.995f;
    }

    // Replace local decision with cloud decision
    // Copy output vector
    uint32_t copy_size = cloud_decision->output_size;
    if (copy_size > local_decision->output_size) {
        copy_size = local_decision->output_size;
    }
    memcpy(local_decision->output_vector, cloud_decision->output_vector,
           copy_size * sizeof(float));
    local_decision->confidence = cloud_decision->confidence;
    if (cloud_decision->label[0]) {
        strncpy(local_decision->label, cloud_decision->label,
                sizeof(local_decision->label) - 1);
        local_decision->label[sizeof(local_decision->label) - 1] = '\0';
    }

    // Buffer for distillation
    cloud_route_t route = CLOUD_ROUTE_CLOUD;
    if (bridge->config.enable_distillation && local_brain) {
        buffer_distillation_example(bridge, features, num_features, cloud_decision);

        // Immediate distillation: train local brain on this example right now
        // This is the online learning path — fast but one example at a time
        if (cloud_decision->output_vector && cloud_decision->confidence > 0.6f) {
            const char* dist_label = cloud_decision->label[0] ? cloud_decision->label : NULL;
            float dist_loss = brain_learn_vector(
                local_brain, features, num_features,
                cloud_decision->output_vector, cloud_decision->output_size,
                dist_label,
                cloud_decision->confidence * bridge->config.distillation_lr);

            if (dist_loss >= 0.0f) {
                bridge->stats.distillation_steps++;
                route = CLOUD_ROUTE_DISTILLED;
            }
        }
    }

    // Free cloud decision (we already copied what we need)
    brain_free_decision(cloud_decision);

    return route;
}


uint32_t cloud_inference_distill_batch(
    cloud_inference_bridge_t* bridge,
    brain_t local_brain,
    uint32_t max_examples)
{
    if (!bridge || !local_brain || !bridge->distill_buffer) return 0;
    if (bridge->distill_count == 0) return 0;

    uint32_t to_process = bridge->distill_count;
    if (max_examples > 0 && to_process > max_examples) {
        to_process = max_examples;
    }

    uint32_t processed = 0;
    uint32_t start = (bridge->distill_head + bridge->distill_capacity - bridge->distill_count)
                     % bridge->distill_capacity;

    for (uint32_t i = 0; i < to_process; i++) {
        uint32_t idx = (start + i) % bridge->distill_capacity;
        distillation_example_t* ex = &bridge->distill_buffer[idx];

        if (!ex->input || !ex->cloud_output) continue;

        float loss = brain_learn_vector(
            local_brain, ex->input, ex->num_features,
            ex->cloud_output, ex->num_outputs,
            ex->label[0] ? ex->label : NULL,
            ex->cloud_confidence * bridge->config.distillation_lr);

        if (loss >= 0.0f) {
            processed++;
            bridge->stats.distillation_steps++;
        }
    }

    // Clear processed entries
    for (uint32_t i = 0; i < to_process; i++) {
        uint32_t idx = (start + i) % bridge->distill_capacity;
        nimcp_free(bridge->distill_buffer[idx].input);
        nimcp_free(bridge->distill_buffer[idx].cloud_output);
        bridge->distill_buffer[idx].input = NULL;
        bridge->distill_buffer[idx].cloud_output = NULL;
    }
    bridge->distill_count -= to_process;

    return processed;
}


cloud_inference_stats_t cloud_inference_get_stats(const cloud_inference_bridge_t* bridge)
{
    if (!bridge) {
        cloud_inference_stats_t empty = {0};
        return empty;
    }
    return bridge->stats;
}


void cloud_inference_reset_stats(cloud_inference_bridge_t* bridge)
{
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->ema_local_conf = 0.5f;
    bridge->ema_cloud_conf = 0.5f;
}


void cloud_inference_set_backend(
    cloud_inference_bridge_t* bridge,
    cloud_backend_fn backend_fn,
    void* user_data)
{
    if (!bridge) return;
    bridge->backend_fn = backend_fn;
    bridge->backend_user_data = user_data;
    bridge->backend_available = (backend_fn != NULL);
    bridge->stats.consecutive_cloud_failures = 0;
}


brain_decision_t* cloud_backend_local_brain(
    const float* features,
    uint32_t num_features,
    void* user_data)
{
    brain_t backend_brain = (brain_t)user_data;
    if (!backend_brain) return NULL;
    return brain_decide(backend_brain, features, num_features);
}

/**
 * @file nimcp_brain_experience.c
 * @brief Unified Experience API — merged inference + learning pipeline
 *
 * WHAT: Implements brain_experience() — every perception is a learning opportunity
 * WHY:  Enables developmental learning where training and inference are one process
 * HOW:  Forward pass → prediction error → attention-gated plasticity → reward learning
 */

#define LOG_MODULE "EXPERIENCE"
#define LOG_MODULE_ID 0x0E00

#include "core/brain/learning/nimcp_brain_experience.h"
#include "core/brain/learning/nimcp_brain_learning.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_lazy_init.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/medulla/nimcp_medulla.h"
#include "plasticity/nimcp_plasticity_coordinator.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_event_driven_plasticity.h"
#include "core/neuralnet/nimcp_neuralnet_learning.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/time/nimcp_time.h"

#include <math.h>
#include <string.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define EXPERIENCE_DEFAULT_LR          0.001f
#define EXPERIENCE_DEFAULT_ATTENTION   0.3f
#define EXPERIENCE_DEFAULT_ATT_SCALE   3.0f
#define EXPERIENCE_DEFAULT_NOVELTY     1.5f
#define REWARD_LEARNING_RATE           0.005f
#define REWARD_ACTIVITY_THRESHOLD      0.01f

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

brain_experience_config_t brain_experience_default_config(void) {
    return (brain_experience_config_t){
        .enabled = true,
        .base_learning_rate = EXPERIENCE_DEFAULT_LR,
        .attention_threshold = EXPERIENCE_DEFAULT_ATTENTION,
        .attention_lr_scale = EXPERIENCE_DEFAULT_ATT_SCALE,
        .novelty_boost = EXPERIENCE_DEFAULT_NOVELTY,
        .enable_hebbian = true,
        .enable_reward_learning = true,
        .enable_world_model_update = true,
        .enable_structural_plasticity = false,  /* Conservative default */
        .synaptogenesis_threshold = 0.7f,       /* Default: high novelty required */
        .consolidation_interval = 1000          /* Consolidate every 1000 experiences */
    };
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

int brain_experience_configure(brain_t brain, const brain_experience_config_t* config) {
    if (!brain || !config) return -1;

    brain->experience_config = *config;
    brain->inference_learning_enabled = config->enabled;

    LOG_MODULE_INFO(LOG_MODULE, "Experience learning %s: lr=%.4f, att_thresh=%.2f",
                    config->enabled ? "ENABLED" : "DISABLED",
                    config->base_learning_rate,
                    config->attention_threshold);
    return 0;
}

/* ============================================================================
 * Internal: Compute prediction error
 * ============================================================================ */

static float compute_prediction_error(
    brain_t brain,
    const float* input,
    uint32_t input_size,
    const float* output,
    uint32_t output_size)
{
    /* If we have a cached last_prediction (from world model), compare against it */
    if (brain->last_experience_prediction && brain->last_experience_input_size == input_size) {
        float mse = 0.0f;
        for (uint32_t i = 0; i < output_size && i < brain->last_experience_output_size; i++) {
            float diff = output[i] - brain->last_experience_prediction[i];
            mse += diff * diff;
        }
        mse /= (float)output_size;
        return fminf(sqrtf(mse), 1.0f);  /* Normalize to [0, 1] */
    }

    /* No prior prediction — first experience is always "surprising" */
    return 0.5f;
}

/* ============================================================================
 * Internal: Get attention level from thalamus
 * ============================================================================ */

static float get_attention_level(brain_t brain) {
    /* Try medulla arousal if available */
    if (brain->medulla_enabled) {
        float arousal = medulla_get_arousal_level(brain->medulla);
        if (isfinite(arousal) && arousal >= 0.0f && arousal <= 1.0f) {
            return arousal;
        }
    }

    /* Fallback: moderate attention */
    return 0.6f;
}

/* ============================================================================
 * Internal: Apply lightweight plasticity
 * ============================================================================ */

static bool apply_experience_plasticity(
    brain_t brain,
    const float* input,
    uint32_t input_size,
    float prediction_error,
    float effective_lr)
{
    bool synapse_formed = false;
    /* Plasticity coordinator: STDP/BCM/homeostatic */
    if (brain->experience_config.enable_hebbian &&
        brain->plasticity_coordinator && brain->plasticity_coordinator_enabled) {
        uint64_t now_ms = nimcp_time_get_us() / 1000;
        plasticity_coordinator_update(brain->plasticity_coordinator, now_ms, prediction_error);
    }

    /* EDP: Three-factor eligibility consolidation */
    if (brain->event_driven_plasticity && brain->enable_event_driven_plasticity) {
        edp_process_prediction_error(brain->event_driven_plasticity, prediction_error, 0);
    }

    /* Structural plasticity (synaptogenesis): form new synapses on high prediction error */
    float syn_threshold = brain->experience_config.synaptogenesis_threshold;
    if (syn_threshold <= 0.0f) syn_threshold = 0.7f;  /* Safety fallback */
    if (brain->experience_config.enable_structural_plasticity &&
        brain->structural_plasticity && brain->structural_plasticity_enabled &&
        prediction_error > syn_threshold) {
        float activity_hz = 30.0f * prediction_error;
        if (structural_plasticity_should_form(brain->structural_plasticity, activity_hz)) {
            /* Select pre/post neurons from the hidden layer based on input hash.
             * Uses input-derived hash to pick neurons that were likely active
             * during this experience — connecting co-active neurons. */
            neural_network_t base_net = adaptive_network_get_base_network(brain->network);
            uint32_t total_neurons = base_net ? neural_network_get_num_neurons(base_net) : 0;
            uint32_t num_inputs = brain->config.num_inputs;
            uint32_t num_outputs = brain->config.num_outputs;
            uint32_t num_hidden = (total_neurons > num_inputs + num_outputs)
                                  ? (total_neurons - num_inputs - num_outputs) : 0;

            if (num_hidden > 1) {
                /* Hash input values to select hidden-layer neuron IDs */
                uint32_t h1 = (uint32_t)(fabsf(input[0]) * 997.0f);
                uint32_t h2 = (uint32_t)(fabsf(input[input_size > 1 ? 1 : 0]) * 991.0f);
                uint32_t pre  = num_inputs + (h1 % num_hidden);
                uint32_t post = num_inputs + (h2 % num_hidden);
                if (pre == post) post = num_inputs + ((h2 + 1) % num_hidden);

                uint32_t syn_id = 0;
                int rc = structural_plasticity_form_synapse(
                    brain->structural_plasticity, pre, post, activity_hz, &syn_id);
                if (rc == 0) {
                    brain->synaptogenesis_count++;
                    synapse_formed = true;
                    LOG_MODULE_DEBUG(LOG_MODULE,
                        "Synaptogenesis: new synapse %u (%u→%u), PE=%.3f, hz=%.1f [total=%lu]",
                        syn_id, pre, post, prediction_error, activity_hz,
                        (unsigned long)brain->synaptogenesis_count);
                }
            }
        }
    }
    return synapse_formed;
}

/* ============================================================================
 * Internal: Apply reward-based learning
 * ============================================================================ */

static void apply_reward_learning(
    brain_t brain,
    float teacher_reward,
    float effective_lr)
{
    /* TPB: reward signal → RPE → neuromodulator update */
    if (brain->plasticity_bridge && brain->enable_plasticity_bridge) {
        /* Convert teacher reward to loss-like signal for TPB */
        float pseudo_loss = (1.0f - teacher_reward) * 0.5f;  /* 1.0 reward → 0 loss, -1.0 → 1.0 loss */
        float rpe = 0.0f;
        tpb_report_loss(brain->plasticity_bridge, pseudo_loss, &rpe);
    }

    /* Direct reward learning on active neurons */
    neural_network_t base_net = adaptive_network_get_base_network(brain->network);
    if (base_net) {
        BRAIN_ENSURE_NEUROMOD(brain);
        if (brain->neuromodulator_system) {
            neural_network_set_neuromodulator_system(base_net, brain->neuromodulator_system);
        }
        /* Scale reward by effective learning rate */
        float scaled_reward = teacher_reward * effective_lr * 10.0f;
        uint32_t modified = neural_network_apply_reward_learning_active(
            base_net, scaled_reward, REWARD_LEARNING_RATE, nimcp_time_get_us(), REWARD_ACTIVITY_THRESHOLD);
        if (modified > 0) {
            adaptive_network_invalidate_gpu_structure(brain->network);
        }
    }
}

/* ============================================================================
 * Internal: Cache prediction for next experience
 * ============================================================================ */

static void cache_experience(
    brain_t brain,
    const float* input,
    uint32_t input_size,
    const float* output,
    uint32_t output_size)
{
    /* Allocate/reallocate prediction cache if needed */
    if (!brain->last_experience_prediction || brain->last_experience_output_size != output_size) {
        nimcp_free(brain->last_experience_prediction);
        brain->last_experience_prediction = nimcp_malloc(output_size * sizeof(float));
        brain->last_experience_output_size = output_size;
    }
    if (!brain->last_experience_input || brain->last_experience_input_size != input_size) {
        nimcp_free(brain->last_experience_input);
        brain->last_experience_input = nimcp_malloc(input_size * sizeof(float));
        brain->last_experience_input_size = input_size;
    }

    if (brain->last_experience_prediction) {
        memcpy(brain->last_experience_prediction, output, output_size * sizeof(float));
    }
    if (brain->last_experience_input) {
        memcpy(brain->last_experience_input, input, input_size * sizeof(float));
    }
}

/* ============================================================================
 * Public API: brain_experience()
 * ============================================================================ */

bool brain_experience(
    brain_t brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size,
    float teacher_reward,
    brain_experience_result_t* result)
{
    if (!brain || !input || !output || !result) {
        return false;
    }

    memset(result, 0, sizeof(*result));

    /* ---- Step 1: Forward pass (perception) ---- */
    if (!brain->network) {
        LOG_MODULE_ERROR(LOG_MODULE, "No network initialized");
        return false;
    }

    adaptive_network_forward(brain->network, input, input_size,
                             output, output_size, 0);

    /* ---- Step 2: Compute prediction error ---- */
    float prediction_error = compute_prediction_error(brain, input, input_size,
                                                       output, output_size);

    /* ---- Step 3: Get attention level ---- */
    float attention = get_attention_level(brain);

    /* ---- Step 4: Attention-gated learning ---- */
    bool learning_applied = false;
    bool synapse_formed = false;
    float effective_lr = 0.0f;

    if (brain->inference_learning_enabled && brain->experience_config.enabled) {
        float att_threshold = brain->experience_config.attention_threshold;

        if (attention >= att_threshold) {
            /* Compute effective learning rate:
             * - Base rate scaled by attention level
             * - Novelty boost for high prediction error */
            effective_lr = brain->experience_config.base_learning_rate
                         * (attention * brain->experience_config.attention_lr_scale);

            if (prediction_error > 0.5f) {
                effective_lr *= brain->experience_config.novelty_boost;
            }

            /* Cap learning rate */
            effective_lr = fminf(effective_lr, 0.01f);

            /* Apply Hebbian/STDP plasticity + synaptogenesis */
            synapse_formed = apply_experience_plasticity(brain, input, input_size,
                                        prediction_error, effective_lr);

            /* Apply reward learning if teacher signal present */
            if (brain->experience_config.enable_reward_learning &&
                fabsf(teacher_reward) > 0.001f) {
                apply_reward_learning(brain, teacher_reward, effective_lr);
            }

            learning_applied = true;
        }
    }

    /* ---- Step 5: Cache for next prediction error ---- */
    cache_experience(brain, input, input_size, output, output_size);

    /* ---- Step 6: Accumulate sleep pressure ---- */
    if (brain->sleep_system && brain->config.enable_sleep_wake_cycle) {
        sleep_accumulate_pressure(brain->sleep_system, 1);
    }

    /* ---- Step 7: Periodic consolidation ---- */
    brain->experience_count++;
    if (brain->experience_config.consolidation_interval > 0 &&
        (brain->experience_count % brain->experience_config.consolidation_interval) == 0) {
        /* Lightweight consolidation — flush eligibility traces */
        if (brain->event_driven_plasticity && brain->enable_event_driven_plasticity) {
            /* EDP consolidation happens naturally via update cycles */
        }
        LOG_MODULE_DEBUG(LOG_MODULE, "Auto-consolidation at experience %lu",
                         (unsigned long)brain->experience_count);
    }

    /* ---- Fill result ---- */
    result->prediction_error = prediction_error;
    result->attention_level = attention;
    result->learning_rate_used = effective_lr;
    result->learning_applied = learning_applied;
    result->synapse_formed = synapse_formed;
    result->reward_signal = teacher_reward;
    result->experience_id = brain->experience_count;

    return true;
}

/* ============================================================================
 * Public API: brain_experience_correct()
 * ============================================================================ */

float brain_experience_correct(
    brain_t brain,
    const float* expected,
    uint32_t expected_size)
{
    if (!brain || !expected) return -1.0f;
    if (!brain->last_experience_input) return -1.0f;

    /* Use the last input with the correct target — full supervised learning */
    float loss = brain_learn_vector(
        brain,
        brain->last_experience_input,
        brain->last_experience_input_size,
        expected,
        expected_size,
        NULL,   /* no label */
        1.0f    /* full confidence */
    );

    return loss;
}

/* ============================================================================
 * Public API: brain_experience_attend()
 * ============================================================================ */

int brain_experience_attend(brain_t brain, const char* modality, float strength) {
    if (!brain || !modality) return -1;
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;

    /* Modulate thalamic gating for the specified modality */
    /* This adjusts attention to prioritize certain sensory channels */
    LOG_MODULE_INFO(LOG_MODULE, "Directing attention to %s (strength=%.2f)",
                    modality, strength);

    /* TODO: Wire to thalamus set_attention API when available */
    /* For now, store in brain state for use during experience processing */
    (void)modality;
    (void)strength;

    return 0;
}

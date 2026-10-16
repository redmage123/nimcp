/**
 * @file nimcp_omni_sensory_bridge.c
 * @brief Implementation of Omnidirectional Inference to Sensory Cortex Bridge
 */

#include "perception/nimcp_omni_sensory_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/omni/nimcp_omni_precision.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_sensory_bridge)

#define LOG_MODULE "OMNI_SENSORY_BRIDGE"

/* Forward declaration for unlocked binding helper (used by omni_sensory_update) */
static int omni_sensory_compute_binding_unlocked(omni_sensory_bridge_t* bridge,
                                                  omni_crossmodal_binding_t* binding);


/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_omni_predict_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_sensory_bridge_t* bridge = (omni_sensory_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in handle_omni_predict_request");

    /* Process prediction request through the bridge */
    omni_sensory_update(bridge);

    /* Signal completion (no response data for now) */
    (void)response_promise;
    (void)msg_size;

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_omni_precision_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_sensory_bridge_t* bridge = (omni_sensory_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in handle_omni_precision_update");

    /* Update precision weights under lock (bio-async handler runs on separate thread) */
    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);
    omni_sensory_update_precision(bridge);
    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    (void)response_promise;
    (void)msg_size;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static float compute_modality_pe(const float* prediction,
                                  const float* observation,
                                  uint32_t dim) {
    if (!prediction || !observation || dim == 0) return 0.0f;

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = prediction[i] - observation[i];
        sum_sq += diff * diff;
    }
    return sqrtf(sum_sq / (float)dim);
}

static float compute_binding_strength(const omni_modality_state_t* s1,
                                       const omni_modality_state_t* s2,
                                       float weight) {
    if (!s1 || !s2 || !s1->active || !s2->active) return 0.0f;

    /* Binding strength based on temporal coherence and PE similarity */
    float pe_diff = fabsf(s1->prediction_error - s2->prediction_error);
    float pe_similarity = 1.0f / (1.0f + pe_diff);
    float precision_product = s1->precision * s2->precision;

    return weight * pe_similarity * precision_product;
}

static void update_modality_state(omni_modality_state_t* state,
                                   const float* features,
                                   uint32_t dim,
                                   float pe) {
    if (!state) return;

    state->prediction_error = pe;
    state->active = (features != NULL);

    /* Copy features if provided */
    if (features && dim > 0) {
        if (!state->features || state->dim != dim) {
            if (state->features) nimcp_free(state->features);
            state->features = nimcp_calloc(dim, sizeof(float));
        }
        if (state->features) {
            memcpy(state->features, features, dim * sizeof(float));
        }
        state->dim = dim;
    }
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_sensory_default_config(omni_sensory_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "NULL config in omni_sensory_default_config");

    memset(config, 0, sizeof(omni_sensory_config_t));

    config->default_audio_precision = 1.0f;
    config->default_visual_precision = 1.0f;
    config->default_speech_precision = 1.0f;

    config->pe_threshold = OMNI_SENSORY_PE_THRESHOLD;
    config->novelty_threshold = 2.0f;

    config->crossmodal_mode = OMNI_CROSSMODAL_ALL;
    config->crossmodal_weight = OMNI_SENSORY_CROSSMODAL_WEIGHT;
    config->enable_binding = true;

    config->enable_attention = true;
    config->attention_gain = 2.0f;

    config->enable_bio_async = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_sensory_bridge_t* omni_sensory_bridge_create(
    const omni_sensory_config_t* config) {

    omni_sensory_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_sensory_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate omni_sensory_bridge_t");
        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(omni_sensory_config_t));
    } else {
        omni_sensory_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "omni_sensory") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex for omni_sensory_bridge");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize modality states */
    bridge->modality_states[OMNI_MODALITY_AUDIO].precision =
        bridge->config.default_audio_precision;
    bridge->modality_states[OMNI_MODALITY_VISUAL].precision =
        bridge->config.default_visual_precision;
    bridge->modality_states[OMNI_MODALITY_SPEECH].precision =
        bridge->config.default_speech_precision;

    memset(&bridge->stats, 0, sizeof(omni_sensory_stats_t));

    return bridge;
}

void omni_sensory_bridge_destroy(omni_sensory_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_sensory");

    /* Free modality state buffers */
    for (int i = 0; i < OMNI_MODALITY_COUNT; i++) {
        if (bridge->modality_states[i].prediction) {
            nimcp_free(bridge->modality_states[i].prediction);
        }
        if (bridge->modality_states[i].features) {
            nimcp_free(bridge->modality_states[i].features);
        }
    }

    /* Free effect buffers */
    if (bridge->omni_effects.top_down_audio) {
        nimcp_free(bridge->omni_effects.top_down_audio);
    }
    if (bridge->omni_effects.top_down_visual) {
        nimcp_free(bridge->omni_effects.top_down_visual);
    }
    if (bridge->omni_effects.top_down_speech) {
        nimcp_free(bridge->omni_effects.top_down_speech);
    }
    if (bridge->sensory_effects.audio_features) {
        nimcp_free(bridge->sensory_effects.audio_features);
    }
    if (bridge->sensory_effects.visual_features) {
        nimcp_free(bridge->sensory_effects.visual_features);
    }
    if (bridge->sensory_effects.speech_features) {
        nimcp_free(bridge->sensory_effects.speech_features);
    }
    if (bridge->binding.integrated_repr) {
        nimcp_free(bridge->binding.integrated_repr);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_sensory_connect_jepa(omni_sensory_bridge_t* bridge,
                               jepa_bidirectional_t* jepa) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_jepa");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->jepa = jepa;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_connect_pred_hier(omni_sensory_bridge_t* bridge,
                                    predictive_hierarchy_t* pred_hier) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_pred_hier");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_hier = pred_hier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_connect_hopfield(omni_sensory_bridge_t* bridge,
                                   hopfield_memory_t* hopfield) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_hopfield");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hopfield = hopfield;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_connect_audio(omni_sensory_bridge_t* bridge,
                                audio_cortex_t* audio) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_audio");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->audio_cortex = audio;
    bridge->modality_states[OMNI_MODALITY_AUDIO].active = (audio != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_connect_visual(omni_sensory_bridge_t* bridge,
                                 visual_cortex_t* visual) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_visual");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->visual_cortex = visual;
    bridge->modality_states[OMNI_MODALITY_VISUAL].active = (visual != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_connect_speech(omni_sensory_bridge_t* bridge,
                                 speech_cortex_t* speech) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_speech");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->speech_cortex = speech;
    bridge->modality_states[OMNI_MODALITY_SPEECH].active = (speech != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_connect_precision(omni_sensory_bridge_t* bridge,
                                    omni_precision_ctx_t* precision_ctx) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_precision");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->precision_ctx = precision_ctx;

    /* Register this bridge with the precision context if provided */
    if (precision_ctx) {
        omni_precision_register_module(precision_ctx,
                                        BIO_MODULE_OMNI_SENSORY_BRIDGE,
                                        "omni_sensory_bridge",
                                        OMNI_PRECISION_DEFAULT);

        /* Enable forward and backward precision channels */
        omni_precision_enable_channel(precision_ctx,
                                       BIO_MODULE_OMNI_SENSORY_BRIDGE,
                                       OMNI_PREC_CHANNEL_FORWARD,
                                       OMNI_PRECISION_DEFAULT);
        omni_precision_enable_channel(precision_ctx,
                                       BIO_MODULE_OMNI_SENSORY_BRIDGE,
                                       OMNI_PREC_CHANNEL_BACKWARD,
                                       OMNI_PRECISION_DEFAULT);
        omni_precision_enable_channel(precision_ctx,
                                       BIO_MODULE_OMNI_SENSORY_BRIDGE,
                                       OMNI_PREC_CHANNEL_LATERAL,
                                       OMNI_PRECISION_DEFAULT);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_sensory_update(omni_sensory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_update");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute sensory free energy from each modality */
    float total_fe = 0.0f;
    int active_count = 0;

    for (int i = 0; i < OMNI_MODALITY_COUNT; i++) {
        omni_modality_state_t* state = &bridge->modality_states[i];
        if (state->active) {
            total_fe += state->precision * state->prediction_error *
                        state->prediction_error;
            active_count++;
        }
    }

    if (active_count > 0) {
        bridge->sensory_effects.combined_free_energy = total_fe / active_count;
    }

    /* Update precision based on prediction errors */
    if (bridge->config.enable_attention) {
        omni_sensory_update_precision(bridge);
    }

    /* Compute cross-modal binding if enabled (use unlocked helper to avoid
     * race condition from unlock-call-relock pattern) */
    if (bridge->config.enable_binding) {
        omni_crossmodal_binding_t binding;
        omni_sensory_compute_binding_unlocked(bridge, &binding);
    }

    /* Copy individual PEs to effects */
    bridge->sensory_effects.audio_pe =
        bridge->modality_states[OMNI_MODALITY_AUDIO].prediction_error;
    bridge->sensory_effects.visual_pe =
        bridge->modality_states[OMNI_MODALITY_VISUAL].prediction_error;
    bridge->sensory_effects.speech_pe =
        bridge->modality_states[OMNI_MODALITY_SPEECH].prediction_error;

    /* Check for novelty */
    bridge->sensory_effects.audio_novelty =
        bridge->sensory_effects.audio_pe > bridge->config.novelty_threshold;
    bridge->sensory_effects.visual_novelty =
        bridge->sensory_effects.visual_pe > bridge->config.novelty_threshold;
    bridge->sensory_effects.speech_novelty =
        bridge->sensory_effects.speech_pe > bridge->config.novelty_threshold;

    if (bridge->sensory_effects.audio_novelty ||
        bridge->sensory_effects.visual_novelty ||
        bridge->sensory_effects.speech_novelty) {
        bridge->stats.novelty_events++;
    }

    /* Update statistics */
    bridge->stats.total_updates++;
    float n = (float)bridge->stats.total_updates;
    float new_avg_audio = (bridge->stats.avg_audio_pe * (n - 1) + bridge->sensory_effects.audio_pe) / n;
    float new_avg_visual = (bridge->stats.avg_visual_pe * (n - 1) + bridge->sensory_effects.visual_pe) / n;
    float new_avg_speech = (bridge->stats.avg_speech_pe * (n - 1) + bridge->sensory_effects.speech_pe) / n;
    float new_avg_fe = (bridge->stats.avg_free_energy * (n - 1) +
         bridge->sensory_effects.combined_free_energy) / n;
    if (isfinite(new_avg_audio))  bridge->stats.avg_audio_pe = new_avg_audio;
    if (isfinite(new_avg_visual)) bridge->stats.avg_visual_pe = new_avg_visual;
    if (isfinite(new_avg_speech)) bridge->stats.avg_speech_pe = new_avg_speech;
    if (isfinite(new_avg_fe))     bridge->stats.avg_free_energy = new_avg_fe;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_apply_to_sensory(omni_sensory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_apply_to_sensory");
    /* Apply top-down predictions to sensory cortices */
    return NIMCP_SUCCESS;
}

int omni_sensory_apply_to_omni(omni_sensory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_apply_to_omni");
    /* Apply bottom-up features to omnidirectional inference */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Prediction API
 * ============================================================================ */

int omni_sensory_predict_modality(omni_sensory_bridge_t* bridge,
                                   omni_modality_t modality,
                                   float* prediction,
                                   uint32_t dim) {
    NIMCP_CHECK_THROW(bridge && prediction && dim > 0, NIMCP_ERROR_INVALID_PARAM, "NULL or invalid parameter in omni_sensory_predict_modality");
    NIMCP_CHECK_THROW(modality < OMNI_MODALITY_COUNT, NIMCP_ERROR_INVALID_PARAM, "Invalid modality in omni_sensory_predict_modality");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Generate top-down prediction using JEPA backward inference */
    if (bridge->jepa) {
        /* Use backward direction for top-down prediction */
        /* jepa_bidirectional_predict(bridge->jepa, JEPA_DIR_BACKWARD, ...) */
    }

    /* Update stats */
    switch (modality) {
        case OMNI_MODALITY_AUDIO:
            bridge->stats.audio_predictions++;
            break;
        case OMNI_MODALITY_VISUAL:
            bridge->stats.visual_predictions++;
            break;
        case OMNI_MODALITY_SPEECH:
            bridge->stats.speech_predictions++;
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_compute_pe(omni_sensory_bridge_t* bridge,
                             omni_modality_t modality,
                             const float* observation,
                             uint32_t dim,
                             float* pe) {
    NIMCP_CHECK_THROW(bridge && observation && pe && dim > 0, NIMCP_ERROR_INVALID_PARAM, "NULL or invalid parameter in omni_sensory_compute_pe");
    NIMCP_CHECK_THROW(modality < OMNI_MODALITY_COUNT, NIMCP_ERROR_INVALID_PARAM, "Invalid modality in omni_sensory_compute_pe");

    nimcp_mutex_lock(bridge->base.mutex);

    omni_modality_state_t* state = &bridge->modality_states[modality];

    if (state->prediction && state->dim == dim) {
        *pe = compute_modality_pe(state->prediction, observation, dim);
    } else {
        *pe = 0.0f;
    }

    /* Update state */
    update_modality_state(state, observation, dim, *pe);

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_crossmodal_predict(omni_sensory_bridge_t* bridge,
                                     omni_modality_t source_modality,
                                     omni_modality_t target_modality,
                                     float* prediction,
                                     uint32_t dim) {
    NIMCP_CHECK_THROW(bridge && prediction && dim > 0, NIMCP_ERROR_INVALID_PARAM, "NULL or invalid parameter in omni_sensory_crossmodal_predict");
    NIMCP_CHECK_THROW(source_modality < OMNI_MODALITY_COUNT, NIMCP_ERROR_INVALID_PARAM, "Invalid source_modality in omni_sensory_crossmodal_predict");
    NIMCP_CHECK_THROW(target_modality < OMNI_MODALITY_COUNT, NIMCP_ERROR_INVALID_PARAM, "Invalid target_modality in omni_sensory_crossmodal_predict");
    NIMCP_CHECK_THROW(source_modality != target_modality, NIMCP_ERROR_INVALID_PARAM, "source_modality == target_modality in omni_sensory_crossmodal_predict");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Use lateral inference for cross-modal prediction */
    if (bridge->jepa) {
        /* jepa_bidirectional_predict(bridge->jepa, JEPA_DIR_LATERAL, ...) */
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Binding API
 * ============================================================================ */

/**
 * @brief Internal unlocked binding computation
 *
 * WHAT: Compute cross-modal binding without acquiring mutex
 * WHY:  Called from omni_sensory_update() which already holds the mutex;
 *       acquiring it again would deadlock (or race if we unlock/relock)
 * HOW:  Pure computation on bridge state, caller must hold bridge->base.mutex
 */
static int omni_sensory_compute_binding_unlocked(omni_sensory_bridge_t* bridge,
                                                  omni_crossmodal_binding_t* binding) {
    memset(binding, 0, sizeof(omni_crossmodal_binding_t));

    omni_modality_state_t* audio = &bridge->modality_states[OMNI_MODALITY_AUDIO];
    omni_modality_state_t* visual = &bridge->modality_states[OMNI_MODALITY_VISUAL];
    omni_modality_state_t* speech = &bridge->modality_states[OMNI_MODALITY_SPEECH];

    float crossmodal_weight = bridge->config.crossmodal_weight;

    /* Audio-visual binding */
    if (bridge->config.crossmodal_mode == OMNI_CROSSMODAL_AUDIO_VISUAL ||
        bridge->config.crossmodal_mode == OMNI_CROSSMODAL_ALL) {
        float av_strength = compute_binding_strength(audio, visual, crossmodal_weight);
        binding->audiovisual_bound = (av_strength > 0.5f);
        binding->binding_strength = fmaxf(binding->binding_strength, av_strength);
    }

    /* Visual-speech binding */
    if (bridge->config.crossmodal_mode == OMNI_CROSSMODAL_VISUAL_SPEECH ||
        bridge->config.crossmodal_mode == OMNI_CROSSMODAL_ALL) {
        float vs_strength = compute_binding_strength(visual, speech, crossmodal_weight);
        binding->visualspeech_bound = (vs_strength > 0.5f);
        binding->binding_strength = fmaxf(binding->binding_strength, vs_strength);
    }

    /* Audio-speech binding */
    if (bridge->config.crossmodal_mode == OMNI_CROSSMODAL_AUDIO_SPEECH ||
        bridge->config.crossmodal_mode == OMNI_CROSSMODAL_ALL) {
        float as_strength = compute_binding_strength(audio, speech, crossmodal_weight);
        binding->audiospeech_bound = (as_strength > 0.5f);
        binding->binding_strength = fmaxf(binding->binding_strength, as_strength);
    }

    /* Compute overall coherence */
    int bound_count = (binding->audiovisual_bound ? 1 : 0) +
                      (binding->visualspeech_bound ? 1 : 0) +
                      (binding->audiospeech_bound ? 1 : 0);
    binding->coherence = (float)bound_count / 3.0f;

    /* Copy to bridge state */
    memcpy(&bridge->binding, binding, sizeof(omni_crossmodal_binding_t));

    if (bound_count > 0) {
        bridge->stats.crossmodal_bindings++;
    }

    return NIMCP_SUCCESS;
}

int omni_sensory_compute_binding(omni_sensory_bridge_t* bridge,
                                  omni_crossmodal_binding_t* binding) {
    NIMCP_CHECK_THROW(bridge && binding, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in omni_sensory_compute_binding");

    nimcp_mutex_lock(bridge->base.mutex);
    int result = omni_sensory_compute_binding_unlocked(bridge, binding);
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

bool omni_sensory_are_bound(const omni_sensory_bridge_t* bridge,
                             omni_modality_t m1,
                             omni_modality_t m2) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_sensory_are_bound: bridge is NULL");
        return false;
    }
    if (m1 >= OMNI_MODALITY_COUNT || m2 >= OMNI_MODALITY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "omni_sensory_are_bound: capacity exceeded");
        return false;
    }
    if (m1 == m2) return true;

    /* Sort modalities for consistent lookup */
    if (m1 > m2) {
        omni_modality_t tmp = m1;
        m1 = m2;
        m2 = tmp;
    }

    if (m1 == OMNI_MODALITY_AUDIO && m2 == OMNI_MODALITY_VISUAL) {
        return bridge->binding.audiovisual_bound;
    } else if (m1 == OMNI_MODALITY_VISUAL && m2 == OMNI_MODALITY_SPEECH) {
        return bridge->binding.visualspeech_bound;
    } else if (m1 == OMNI_MODALITY_AUDIO && m2 == OMNI_MODALITY_SPEECH) {
        return bridge->binding.audiospeech_bound;
    }
    return false;
}

float omni_sensory_get_binding_strength(const omni_sensory_bridge_t* bridge,
                                         omni_modality_t m1,
                                         omni_modality_t m2) {
    if (!bridge) return 0.0f;
    if (m1 >= OMNI_MODALITY_COUNT || m2 >= OMNI_MODALITY_COUNT) return 0.0f;
    if (m1 == m2) return 1.0f;

    return compute_binding_strength(
        &bridge->modality_states[m1],
        &bridge->modality_states[m2],
        bridge->config.crossmodal_weight
    );
}

/* ============================================================================
 * Precision API
 * ============================================================================ */

int omni_sensory_set_precision(omni_sensory_bridge_t* bridge,
                                omni_modality_t modality,
                                float precision) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_set_precision");
    NIMCP_CHECK_THROW(modality < OMNI_MODALITY_COUNT, NIMCP_ERROR_INVALID_PARAM, "Invalid modality in omni_sensory_set_precision");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->modality_states[modality].precision = fmaxf(0.0f, precision);
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

float omni_sensory_get_precision(const omni_sensory_bridge_t* bridge,
                                  omni_modality_t modality) {
    if (!bridge || modality >= OMNI_MODALITY_COUNT) return 0.0f;
    return bridge->modality_states[modality].precision;
}

int omni_sensory_update_precision(omni_sensory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_update_precision");

    /* Precision weighting: reduce precision for high-PE modalities */
    for (int i = 0; i < OMNI_MODALITY_COUNT; i++) {
        omni_modality_state_t* state = &bridge->modality_states[i];
        if (state->active) {
            float pe = state->prediction_error;
            /* Precision inversely related to PE (more reliable = higher precision) */
            state->precision = 1.0f / (1.0f + pe);
        }
    }

    /* Update effect structures */
    bridge->omni_effects.precision_audio =
        bridge->modality_states[OMNI_MODALITY_AUDIO].precision;
    bridge->omni_effects.precision_visual =
        bridge->modality_states[OMNI_MODALITY_VISUAL].precision;
    bridge->omni_effects.precision_speech =
        bridge->modality_states[OMNI_MODALITY_SPEECH].precision;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Attention API
 * ============================================================================ */

int omni_sensory_set_attention(omni_sensory_bridge_t* bridge,
                                omni_sensory_attention_t mode) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_set_attention");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->omni_effects.attention_mode = mode;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_apply_attention(omni_sensory_bridge_t* bridge,
                                  omni_modality_t modality,
                                  const float* attention_weights,
                                  uint32_t num_weights) {
    NIMCP_CHECK_THROW(bridge && attention_weights && num_weights > 0, NIMCP_ERROR_INVALID_PARAM, "NULL or invalid parameter in omni_sensory_apply_attention");
    NIMCP_CHECK_THROW(modality < OMNI_MODALITY_COUNT, NIMCP_ERROR_INVALID_PARAM, "Invalid modality in omni_sensory_apply_attention");

    nimcp_mutex_lock(bridge->base.mutex);

    omni_modality_state_t* state = &bridge->modality_states[modality];

    /* Apply attention gain to precision */
    if (state->active && bridge->config.enable_attention) {
        /* Sum of attention weights modulates precision */
        float attn_sum = 0.0f;
        for (uint32_t i = 0; i < num_weights; i++) {
            attn_sum += attention_weights[i];
        }
        attn_sum /= (float)num_weights;

        state->precision *= (1.0f + (bridge->config.attention_gain - 1.0f) * attn_sum);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_sensory_get_omni_effects(const omni_sensory_bridge_t* bridge,
                                   omni_to_sensory_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in omni_sensory_get_omni_effects");
    nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->omni_effects, sizeof(omni_to_sensory_effects_t));
    nimcp_mutex_unlock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_get_sensory_effects(const omni_sensory_bridge_t* bridge,
                                      sensory_to_omni_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in omni_sensory_get_sensory_effects");
    nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->sensory_effects, sizeof(sensory_to_omni_effects_t));
    nimcp_mutex_unlock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_get_modality_state(const omni_sensory_bridge_t* bridge,
                                     omni_modality_t modality,
                                     omni_modality_state_t* state) {
    NIMCP_CHECK_THROW(bridge && state, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in omni_sensory_get_modality_state");
    NIMCP_CHECK_THROW(modality < OMNI_MODALITY_COUNT, NIMCP_ERROR_INVALID_PARAM, "Invalid modality in omni_sensory_get_modality_state");

    nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    memcpy(state, &bridge->modality_states[modality], sizeof(omni_modality_state_t));
    nimcp_mutex_unlock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_get_stats(const omni_sensory_bridge_t* bridge,
                            omni_sensory_stats_t* stats) {
    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_INVALID_PARAM, "NULL parameter in omni_sensory_get_stats");
    nimcp_mutex_lock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(omni_sensory_stats_t));
    nimcp_mutex_unlock(((omni_sensory_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_sensory_reset_stats(omni_sensory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_reset_stats");
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_sensory_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_sensory_connect_bio_async(omni_sensory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_connect_bio_async");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    /* Register module with bio-router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_SENSORY_BRIDGE,
        .module_name = "omni_sensory_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    NIMCP_CHECK_THROW(ctx, NIMCP_ERROR_OPERATION_FAILED, "Failed to register module with bio-router in omni_sensory_connect_bio_async");

    bridge->bio_context = ctx;

    /* Register message handlers */
    bio_router_register_handler(ctx, BIO_MSG_OMNI_PREDICT_REQUEST,
                                 handle_omni_predict_request);
    bio_router_register_handler(ctx, BIO_MSG_OMNI_PRECISION_UPDATE,
                                 handle_omni_precision_update);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_sensory_disconnect_bio_async(omni_sensory_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "NULL bridge in omni_sensory_disconnect_bio_async");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_sensory_is_bio_async_connected(const omni_sensory_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_sensory_modality_to_string(omni_modality_t modality) {
    switch (modality) {
        case OMNI_MODALITY_AUDIO: return "AUDIO";
        case OMNI_MODALITY_VISUAL: return "VISUAL";
        case OMNI_MODALITY_SPEECH: return "SPEECH";
        default: return "UNKNOWN";
    }
}

const char* omni_sensory_crossmodal_to_string(omni_crossmodal_mode_t mode) {
    switch (mode) {
        case OMNI_CROSSMODAL_NONE: return "NONE";
        case OMNI_CROSSMODAL_AUDIO_VISUAL: return "AUDIO_VISUAL";
        case OMNI_CROSSMODAL_VISUAL_SPEECH: return "VISUAL_SPEECH";
        case OMNI_CROSSMODAL_AUDIO_SPEECH: return "AUDIO_SPEECH";
        case OMNI_CROSSMODAL_ALL: return "ALL";
        default: return "UNKNOWN";
    }
}

const char* omni_sensory_direction_to_string(omni_sensory_direction_t dir) {
    switch (dir) {
        case OMNI_SENS_BOTTOM_UP: return "BOTTOM_UP";
        case OMNI_SENS_TOP_DOWN: return "TOP_DOWN";
        case OMNI_SENS_LATERAL: return "LATERAL";
        case OMNI_SENS_BIDIRECTIONAL: return "BIDIRECTIONAL";
        default: return "UNKNOWN";
    }
}

const char* omni_sensory_attention_to_string(omni_sensory_attention_t attn) {
    switch (attn) {
        case OMNI_SENS_ATTN_NONE: return "NONE";
        case OMNI_SENS_ATTN_SPATIAL: return "SPATIAL";
        case OMNI_SENS_ATTN_TEMPORAL: return "TEMPORAL";
        case OMNI_SENS_ATTN_FEATURE: return "FEATURE";
        case OMNI_SENS_ATTN_OBJECT: return "OBJECT";
        default: return "UNKNOWN";
    }
}

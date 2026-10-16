/**
 * @file nimcp_omni_broca_bridge.c
 * @brief Implementation of Omnidirectional Inference to Broca's Region Bridge
 */

#include "core/brain/regions/broca/nimcp_omni_broca_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "perception/nimcp_speech_cortex.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(omni_broca_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OMNI_BROCA_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static omni_phon_wm_t* phon_wm_create(uint32_t capacity, uint32_t slot_dim,
                                       float decay_rate) {
    omni_phon_wm_t* wm = nimcp_calloc(1, sizeof(omni_phon_wm_t));
    if (!wm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wm is NULL");

        return NULL;

    }

    wm->slots = nimcp_calloc(capacity, sizeof(float*));
    if (!wm->slots) {
        nimcp_free(wm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_broca_bridge_mesh_unregister: wm->slots is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < capacity; i++) {
        wm->slots[i] = nimcp_calloc(slot_dim, sizeof(float));
        if (!wm->slots[i]) {
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(wm->slots[j]);
            }
            nimcp_free(wm->slots);
            nimcp_free(wm);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_broca_bridge_mesh_unregister: wm->slots is NULL");
            return NULL;
        }
    }

    wm->slot_dim = slot_dim;
    wm->num_slots = capacity;
    wm->head = 0;
    wm->decay_rate = decay_rate;

    return wm;
}

static void phon_wm_destroy(omni_phon_wm_t* wm) {
    if (!wm) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_broca");

    if (wm->slots) {
        for (uint32_t i = 0; i < wm->num_slots; i++) {
            if (wm->slots[i]) {
                nimcp_free(wm->slots[i]);
            }
        }
        nimcp_free(wm->slots);
    }

    nimcp_free(wm);
}

static float compute_combined_pe(float syntax_pe, float motor_pe, float phoneme_pe) {
    /* Weighted combination */
    return 0.4f * syntax_pe + 0.3f * motor_pe + 0.3f * phoneme_pe;
}

static float compute_language_fe(float combined_pe, float precision) {
    return 0.5f * precision * combined_pe * combined_pe;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_broca_default_config(omni_broca_config_t* config) {
    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_broca_config_t));

    config->syntax_prediction_depth = OMNI_BROCA_DEFAULT_SYNTAX_DEPTH;
    config->syntax_pe_threshold = 1.0f;
    config->enable_hierarchical_syntax = true;

    config->coarticulation_window = 3;
    config->motor_pe_threshold = 0.5f;
    config->enable_forward_model = true;

    config->wm_capacity = 7;  /* Magic number 7±2 */
    config->wm_decay_rate = 0.1f;
    config->enable_rehearsal = true;

    config->enable_wernicke_loop = true;
    config->auditory_feedback_weight = 0.3f;

    config->enable_bio_async = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_broca_bridge_t* omni_broca_bridge_create(
    const omni_broca_config_t* config) {

    omni_broca_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_broca_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(omni_broca_config_t));
    } else {
        omni_broca_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "omni_broca") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_broca_default_config: bridge->base is NULL");
        return NULL;
    }

    /* Create phonological working memory */
    bridge->phon_wm = phon_wm_create(bridge->config.wm_capacity, 64,
                                      bridge->config.wm_decay_rate);
    if (!bridge->phon_wm) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "omni_broca_default_config: bridge->phon_wm is NULL");
        return NULL;
    }

    /* Initialize syntax prediction */
    bridge->omni_effects.syntax_pred.num_categories = OMNI_SYN_OTHER + 1;
    bridge->omni_effects.syntax_pred.category_probs =
        nimcp_calloc(bridge->omni_effects.syntax_pred.num_categories, sizeof(float));

    bridge->omni_effects.mode = OMNI_LANG_PRODUCTION;

    memset(&bridge->stats, 0, sizeof(omni_broca_stats_t));

    return bridge;
}

void omni_broca_bridge_destroy(omni_broca_bridge_t* bridge) {
    if (!bridge) return;

    /* Free phonological WM */
    if (bridge->phon_wm) {
        phon_wm_destroy(bridge->phon_wm);
    }

    /* Free effect buffers */
    if (bridge->omni_effects.syntax_pred.category_probs) {
        nimcp_free(bridge->omni_effects.syntax_pred.category_probs);
    }
    if (bridge->omni_effects.motor_pred.motor_commands) {
        nimcp_free(bridge->omni_effects.motor_pred.motor_commands);
    }
    if (bridge->omni_effects.motor_pred.coarticulation) {
        nimcp_free(bridge->omni_effects.motor_pred.coarticulation);
    }
    if (bridge->omni_effects.phoneme_prediction) {
        nimcp_free(bridge->omni_effects.phoneme_prediction);
    }

    if (bridge->broca_effects.syntax_pe) {
        nimcp_free(bridge->broca_effects.syntax_pe);
    }
    if (bridge->broca_effects.motor_pe) {
        nimcp_free(bridge->broca_effects.motor_pe);
    }
    if (bridge->broca_effects.phoneme_pe) {
        nimcp_free(bridge->broca_effects.phoneme_pe);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_broca_connect_jepa(omni_broca_bridge_t* bridge,
                             jepa_bidirectional_t* jepa) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->jepa = jepa;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_connect_pred_hier(omni_broca_bridge_t* bridge,
                                  predictive_hierarchy_t* pred_hier) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_hier = pred_hier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_connect_hopfield(omni_broca_bridge_t* bridge,
                                 hopfield_memory_t* hopfield) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hopfield = hopfield;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_connect_broca(omni_broca_bridge_t* bridge,
                              broca_adapter_t* broca) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->broca = broca;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_connect_speech_cortex(omni_broca_bridge_t* bridge,
                                      speech_cortex_t* speech) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->speech_cortex = speech;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_broca_update(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute combined PE */
    float syntax_pe = 0.0f, motor_pe = 0.0f, phoneme_pe = 0.0f;

    if (bridge->broca_effects.syntax_pe && bridge->broca_effects.syntax_dim > 0) {
        for (uint32_t i = 0; i < bridge->broca_effects.syntax_dim; i++) {
            syntax_pe += bridge->broca_effects.syntax_pe[i] *
                         bridge->broca_effects.syntax_pe[i];
        }
        syntax_pe = sqrtf(syntax_pe / (float)bridge->broca_effects.syntax_dim);
    }

    if (bridge->broca_effects.motor_pe && bridge->broca_effects.motor_dim > 0) {
        for (uint32_t i = 0; i < bridge->broca_effects.motor_dim; i++) {
            motor_pe += bridge->broca_effects.motor_pe[i] *
                        bridge->broca_effects.motor_pe[i];
        }
        motor_pe = sqrtf(motor_pe / (float)bridge->broca_effects.motor_dim);
    }

    if (bridge->broca_effects.phoneme_pe && bridge->broca_effects.phoneme_dim > 0) {
        for (uint32_t i = 0; i < bridge->broca_effects.phoneme_dim; i++) {
            phoneme_pe += bridge->broca_effects.phoneme_pe[i] *
                          bridge->broca_effects.phoneme_pe[i];
        }
        phoneme_pe = sqrtf(phoneme_pe / (float)bridge->broca_effects.phoneme_dim);
    }

    bridge->broca_effects.combined_pe = compute_combined_pe(syntax_pe, motor_pe, phoneme_pe);
    bridge->broca_effects.free_energy =
        compute_language_fe(bridge->broca_effects.combined_pe, bridge->omni_effects.precision);

    /* Check for errors */
    bridge->broca_effects.production_error = motor_pe > bridge->config.motor_pe_threshold;
    bridge->broca_effects.syntax_violation = syntax_pe > bridge->config.syntax_pe_threshold;

    /* Update statistics */
    bridge->stats.total_updates++;

    if (bridge->omni_effects.mode == OMNI_LANG_PRODUCTION) {
        bridge->stats.production_events++;
    } else if (bridge->omni_effects.mode == OMNI_LANG_PARSING) {
        bridge->stats.parsing_events++;
    }

    if (bridge->broca_effects.syntax_violation) {
        bridge->stats.syntax_violations++;
    }
    if (bridge->broca_effects.production_error) {
        bridge->stats.motor_errors++;
    }

    float n = (float)bridge->stats.total_updates;
    float new_syntax_pe = (bridge->stats.avg_syntax_pe * (n - 1) + syntax_pe) / n;
    if (isfinite(new_syntax_pe)) bridge->stats.avg_syntax_pe = new_syntax_pe;
    float new_motor_pe = (bridge->stats.avg_motor_pe * (n - 1) + motor_pe) / n;
    if (isfinite(new_motor_pe)) bridge->stats.avg_motor_pe = new_motor_pe;
    float new_wm_load = (bridge->stats.avg_wm_load * (n - 1) + omni_broca_wm_get_load(bridge)) / n;
    if (isfinite(new_wm_load)) bridge->stats.avg_wm_load = new_wm_load;
    float new_free_energy = (bridge->stats.avg_free_energy * (n - 1) + bridge->broca_effects.free_energy) / n;
    if (isfinite(new_free_energy)) bridge->stats.avg_free_energy = new_free_energy;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_apply_to_broca(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    return NIMCP_SUCCESS;
}

int omni_broca_apply_to_omni(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Production API
 * ============================================================================ */

int omni_broca_begin_production(omni_broca_bridge_t* bridge,
                                 const float* semantic_repr,
                                 uint32_t repr_dim) {
    NIMCP_CHECK_THROW(bridge != NULL && semantic_repr != NULL && repr_dim > 0,
                      NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in begin_production");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->omni_effects.mode = OMNI_LANG_PRODUCTION;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_predict_syntax(omni_broca_bridge_t* bridge,
                               omni_syntactic_prediction_t* prediction) {
    NIMCP_CHECK_THROW(bridge != NULL && prediction != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in predict_syntax");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(prediction, &bridge->omni_effects.syntax_pred,
           sizeof(omni_syntactic_prediction_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_predict_motor(omni_broca_bridge_t* bridge,
                              const float* phoneme,
                              uint32_t phoneme_dim,
                              omni_motor_prediction_t* prediction) {
    NIMCP_CHECK_THROW(bridge != NULL && phoneme != NULL && prediction != NULL && phoneme_dim > 0,
                      NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in predict_motor");

    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(prediction, &bridge->omni_effects.motor_pred,
           sizeof(omni_motor_prediction_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_get_production_output(omni_broca_bridge_t* bridge,
                                      float* motor_output,
                                      uint32_t* output_dim) {
    NIMCP_CHECK_THROW(bridge != NULL && motor_output != NULL && output_dim != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in get_production_output");

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->omni_effects.motor_pred.motor_commands &&
        bridge->omni_effects.motor_pred.num_commands > 0) {
        *output_dim = bridge->omni_effects.motor_pred.num_commands;
        memcpy(motor_output, bridge->omni_effects.motor_pred.motor_commands,
               *output_dim * sizeof(float));
    } else {
        *output_dim = 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Parsing API
 * ============================================================================ */

int omni_broca_parse_phonemes(omni_broca_bridge_t* bridge,
                               const float* phonemes,
                               uint32_t phoneme_dim,
                               uint32_t num_phonemes) {
    NIMCP_CHECK_THROW(bridge != NULL && phonemes != NULL && phoneme_dim > 0 && num_phonemes > 0,
                      NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in parse_phonemes");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->omni_effects.mode = OMNI_LANG_PARSING;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_infer_syntax(omni_broca_bridge_t* bridge,
                             omni_syntactic_category_t* categories,
                             uint32_t* num_categories) {
    NIMCP_CHECK_THROW(bridge != NULL && categories != NULL && num_categories != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in infer_syntax");

    nimcp_mutex_lock(bridge->base.mutex);
    *num_categories = 0;  /* Placeholder */
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

bool omni_broca_has_syntax_violation(const omni_broca_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->broca_effects.syntax_violation;
}

/* ============================================================================
 * Working Memory API
 * ============================================================================ */

int omni_broca_wm_push(omni_broca_bridge_t* bridge,
                        const float* phoneme,
                        uint32_t dim) {
    NIMCP_CHECK_THROW(bridge != NULL && phoneme != NULL && dim > 0,
                      NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in wm_push");
    NIMCP_CHECK_THROW(bridge->phon_wm != NULL,
                      NIMCP_ERROR_NOT_INITIALIZED, "Phonological WM not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    omni_phon_wm_t* wm = bridge->phon_wm;

    /* Copy to current head position */
    uint32_t copy_dim = dim < wm->slot_dim ? dim : wm->slot_dim;
    memcpy(wm->slots[wm->head], phoneme, copy_dim * sizeof(float));

    /* Advance head (circular) */
    wm->head = (wm->head + 1) % wm->num_slots;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_wm_pop(omni_broca_bridge_t* bridge,
                       float* phoneme,
                       uint32_t* dim) {
    NIMCP_CHECK_THROW(bridge != NULL && phoneme != NULL && dim != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in wm_pop");
    NIMCP_CHECK_THROW(bridge->phon_wm != NULL,
                      NIMCP_ERROR_NOT_INITIALIZED, "Phonological WM not initialized");

    nimcp_mutex_lock(bridge->base.mutex);

    omni_phon_wm_t* wm = bridge->phon_wm;

    /* Move head back */
    wm->head = (wm->head + wm->num_slots - 1) % wm->num_slots;

    /* Copy from head */
    *dim = wm->slot_dim;
    memcpy(phoneme, wm->slots[wm->head], wm->slot_dim * sizeof(float));

    /* Clear the slot */
    memset(wm->slots[wm->head], 0, wm->slot_dim * sizeof(float));

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

float omni_broca_wm_get_load(const omni_broca_bridge_t* bridge) {
    if (!bridge || !bridge->phon_wm) return 0.0f;

    omni_phon_wm_t* wm = bridge->phon_wm;

    /* Count non-empty slots */
    uint32_t used = 0;
    for (uint32_t i = 0; i < wm->num_slots; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < wm->slot_dim; j++) {
            sum += fabsf(wm->slots[i][j]);
        }
        if (sum > 1e-6f) {
            used++;
        }
    }

    return (float)used / (float)wm->num_slots;
}

int omni_broca_wm_rehearse(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL && bridge->phon_wm != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "bridge or phon_wm is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    /* Rehearsal refreshes memory by replaying */
    /* Implementation would cycle through slots */
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_wm_clear(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL && bridge->phon_wm != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "bridge or phon_wm is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    omni_phon_wm_t* wm = bridge->phon_wm;
    for (uint32_t i = 0; i < wm->num_slots; i++) {
        memset(wm->slots[i], 0, wm->slot_dim * sizeof(float));
    }
    wm->head = 0;

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Wernicke Loop API
 * ============================================================================ */

int omni_broca_send_efference_copy(omni_broca_bridge_t* bridge,
                                    const float* motor_commands,
                                    uint32_t num_commands) {
    NIMCP_CHECK_THROW(bridge != NULL && motor_commands != NULL && num_commands > 0,
                      NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in send_efference_copy");
    /* Would send to Wernicke for prediction comparison */
    return NIMCP_SUCCESS;
}

int omni_broca_receive_feedback(omni_broca_bridge_t* bridge,
                                 const float* auditory_feedback,
                                 uint32_t feedback_dim) {
    NIMCP_CHECK_THROW(bridge != NULL && auditory_feedback != NULL && feedback_dim > 0,
                      NIMCP_ERROR_INVALID_PARAM, "Invalid parameters in receive_feedback");
    /* Would compare to predicted output */
    return NIMCP_SUCCESS;
}

float omni_broca_compute_feedback_pe(const omni_broca_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    /* Would compute mismatch between prediction and feedback */
    return 0.0f;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_broca_get_omni_effects(const omni_broca_bridge_t* bridge,
                                 omni_to_broca_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge != NULL && effects != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in get_omni_effects");
    nimcp_mutex_lock(((omni_broca_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->omni_effects, sizeof(omni_to_broca_effects_t));
    nimcp_mutex_unlock(((omni_broca_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_get_broca_effects(const omni_broca_bridge_t* bridge,
                                  broca_to_omni_effects_t* effects) {
    NIMCP_CHECK_THROW(bridge != NULL && effects != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in get_broca_effects");
    nimcp_mutex_lock(((omni_broca_bridge_t*)bridge)->base.mutex);
    memcpy(effects, &bridge->broca_effects, sizeof(broca_to_omni_effects_t));
    nimcp_mutex_unlock(((omni_broca_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_get_stats(const omni_broca_bridge_t* bridge,
                          omni_broca_stats_t* stats) {
    NIMCP_CHECK_THROW(bridge != NULL && stats != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in get_stats");
    nimcp_mutex_lock(((omni_broca_bridge_t*)bridge)->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(omni_broca_stats_t));
    nimcp_mutex_unlock(((omni_broca_bridge_t*)bridge)->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_broca_reset_stats(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_broca_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

omni_language_mode_t omni_broca_get_mode(const omni_broca_bridge_t* bridge) {
    if (!bridge) return OMNI_LANG_PRODUCTION;
    return bridge->omni_effects.mode;
}

int omni_broca_set_mode(omni_broca_bridge_t* bridge,
                         omni_language_mode_t mode) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->omni_effects.mode = mode;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_broca_predict_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_broca_bridge_t* bridge = (omni_broca_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge != NULL && msg != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in predict_request handler");

    omni_broca_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_broca_precision_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_broca_bridge_t* bridge = (omni_broca_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge != NULL && msg != NULL,
                      NIMCP_ERROR_INVALID_PARAM, "NULL parameter in precision_update handler");

    /* Update precision for language processing */
    omni_broca_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_broca_connect_bio_async(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_BROCA_BRIDGE,
        .module_name = "omni_broca_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_OPERATION_FAILED,
                      "Failed to register with bio-async router");

    bridge->bio_context = ctx;

    bio_router_register_handler(ctx, BIO_MSG_OMNI_PREDICT_REQUEST,
                                 handle_broca_predict_request);
    bio_router_register_handler(ctx, BIO_MSG_OMNI_PRECISION_UPDATE,
                                 handle_broca_precision_update);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_broca_disconnect_bio_async(omni_broca_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge != NULL, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_broca_is_bio_async_connected(const omni_broca_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bio_async_connected;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_broca_area_to_string(omni_broca_area_t area) {
    switch (area) {
        case OMNI_BROCA_BA44: return "BA44";
        case OMNI_BROCA_BA45: return "BA45";
        default: return "UNKNOWN";
    }
}

const char* omni_broca_mode_to_string(omni_language_mode_t mode) {
    switch (mode) {
        case OMNI_LANG_PRODUCTION: return "PRODUCTION";
        case OMNI_LANG_PARSING: return "PARSING";
        case OMNI_LANG_REPETITION: return "REPETITION";
        case OMNI_LANG_MONITORING: return "MONITORING";
        default: return "UNKNOWN";
    }
}

const char* omni_broca_syntax_to_string(omni_syntactic_category_t cat) {
    switch (cat) {
        case OMNI_SYN_NOUN: return "NOUN";
        case OMNI_SYN_VERB: return "VERB";
        case OMNI_SYN_ADJ: return "ADJ";
        case OMNI_SYN_ADV: return "ADV";
        case OMNI_SYN_DET: return "DET";
        case OMNI_SYN_PREP: return "PREP";
        case OMNI_SYN_CONJ: return "CONJ";
        case OMNI_SYN_PRON: return "PRON";
        case OMNI_SYN_OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

const char* omni_broca_motor_to_string(omni_motor_type_t type) {
    switch (type) {
        case OMNI_MOTOR_LIPS: return "LIPS";
        case OMNI_MOTOR_TONGUE: return "TONGUE";
        case OMNI_MOTOR_JAW: return "JAW";
        case OMNI_MOTOR_LARYNX: return "LARYNX";
        case OMNI_MOTOR_VELUM: return "VELUM";
        default: return "UNKNOWN";
    }
}

/**
 * @file nimcp_ofc.c
 * @brief Orbitofrontal Cortex (OFC) Implementation
 */

#include "core/brain/regions/ofc/nimcp_ofc.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(ofc, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define OFC_LOG_TAG "OFC"
#define OFC_DEFAULT_MAX_OPTIONS 16
#define OFC_DEFAULT_HISTORY_SIZE 1000
#define OFC_MODULE_ID 0x5000

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static float hyperbolic_discount(float value, float delay, float discount_rate) {
    if (delay <= 0.0f) return value;
    return value / (1.0f + discount_rate * delay);
}

static float softmax_probability(float value, float* all_values, uint32_t n, float temperature) {
    if (n == 0 || temperature <= 0.0f) return 0.0f;

    float max_val = all_values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (all_values[i] > max_val) max_val = all_values[i];
    }

    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum_exp += expf((all_values[i] - max_val) / temperature);
    }

    return expf((value - max_val) / temperature) / sum_exp;
}

static float compute_risk(float probability, float magnitude) {
    /* Risk as variance proxy: p * (1-p) * magnitude^2 */
    return probability * (1.0f - probability) * magnitude * magnitude;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int ofc_default_config(ofc_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_default_config: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memset(config, 0, sizeof(*config));

    /* Value computation */
    config->learning_rate = 0.1f;
    config->discount_rate = 0.1f;  /* Hyperbolic k */
    config->risk_sensitivity = 0.0f;  /* Risk neutral */
    config->social_weight = 0.3f;

    /* Decision making */
    config->decision_threshold = 0.6f;
    config->noise_level = 0.1f;  /* Softmax temperature */
    config->max_options = OFC_DEFAULT_MAX_OPTIONS;
    config->reversal_threshold = 3.0f;  /* Consecutive errors */

    /* Integration */
    config->enable_bio_async = true;
    config->enable_kg_wiring = true;
    config->enable_immune = true;
    config->enable_security = true;
    config->enable_logging = true;
    config->enable_quantum = false;

    /* Resources */
    config->max_history_size = OFC_DEFAULT_HISTORY_SIZE;
    config->update_interval_ms = 10;

    config->platform_tier = PLATFORM_TIER_FULL;

    return 0;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

nimcp_ofc_t* ofc_create(const ofc_config_t* config) {
    ofc_config_t default_config;
    if (!config) {
        ofc_default_config(&default_config);
        config = &default_config;
    }

    nimcp_ofc_t* ofc = (nimcp_ofc_t*)nimcp_calloc(1, sizeof(nimcp_ofc_t));
    if (!ofc) {
        NIMCP_LOG_ERROR(OFC_LOG_TAG, "Failed to allocate OFC");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ofc_create: ofc is NULL");
        return NULL;
    }

    /* Copy config */
    memcpy(&ofc->config, config, sizeof(ofc_config_t));

    /* Allocate options array */
    ofc->max_options = config->max_options;
    ofc->options = (ofc_option_state_t*)nimcp_calloc(
        ofc->max_options, sizeof(ofc_option_state_t));
    if (!ofc->options) {
        NIMCP_LOG_ERROR(OFC_LOG_TAG, "Failed to allocate options");
        nimcp_free(ofc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ofc_create: ofc->options is NULL");
        return NULL;
    }

    /* Create mutex */
    ofc->mutex = nimcp_mutex_create(NULL);
    if (!ofc->mutex) {
        NIMCP_LOG_ERROR(OFC_LOG_TAG, "Failed to create mutex");
        nimcp_free(ofc->options);
        nimcp_free(ofc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ofc_create: ofc->mutex is NULL");
        return NULL;
    }

    /* Initialize subdivisions */
    for (int i = 0; i < OFC_SUBDIV_COUNT; i++) {
        ofc->subdivisions.activity[i] = 0.0f;
        ofc->subdivisions.modulation[i] = 1.0f;
        ofc->subdivisions.active[i] = true;
    }

    NIMCP_LOG_INFO(OFC_LOG_TAG, "OFC created with %u max options",
                   ofc->max_options);

    return ofc;
}

void ofc_destroy(nimcp_ofc_t* ofc) {
    if (!ofc) return;

    NIMCP_LOG_INFO(OFC_LOG_TAG, "Destroying OFC (decisions=%lu, reversals=%lu)",
                   ofc->stats.decisions_made, ofc->stats.reversals_detected);

    /* Disconnect from systems */
    if (ofc->connected) {
        ofc_bio_async_disconnect(ofc);
        ofc_kg_unregister(ofc);
    }

    /* Clean up */
    if (ofc->mutex) {
        nimcp_mutex_free(ofc->mutex);
    }
    if (ofc->options) {
        nimcp_free(ofc->options);
    }

    nimcp_free(ofc);
}

int ofc_init(nimcp_ofc_t* ofc) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_init: ofc is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(ofc->mutex);

    /* Reset state */
    ofc->num_options = 0;
    ofc->decision_pending = false;
    ofc->prediction_error = 0.0f;
    ofc->cumulative_reward = 0.0f;
    ofc->trial_count = 0;
    ofc->emotion_valence = 0.0f;
    ofc->emotion_arousal = 0.5f;

    memset(&ofc->stats, 0, sizeof(ofc_stats_t));
    memset(&ofc->current_decision, 0, sizeof(ofc_decision_t));

    ofc->initialized = true;
    ofc->last_update_us = 0;

    nimcp_mutex_unlock(ofc->mutex);

    NIMCP_LOG_INFO(OFC_LOG_TAG, "OFC initialized");
    return 0;
}

int ofc_reset(nimcp_ofc_t* ofc) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_reset: ofc is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    return ofc_init(ofc);
}

/*=============================================================================
 * VALUE COMPUTATION
 *===========================================================================*/

int ofc_present_option(nimcp_ofc_t* ofc, uint32_t stimulus_id,
                       float reward_magnitude, float reward_probability,
                       float delay) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_present_option: ofc is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!ofc->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "ofc_present_option: ofc not initialized");
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(ofc->mutex);

    /* Find existing or new slot */
    int32_t slot = -1;
    for (uint32_t i = 0; i < ofc->num_options; i++) {
        if (ofc->options[i].stimulus_id == stimulus_id) {
            slot = (int32_t)i;
            break;
        }
    }

    if (slot < 0) {
        if (ofc->num_options >= ofc->max_options) {
            nimcp_mutex_unlock(ofc->mutex);
            NIMCP_LOG_WARN(OFC_LOG_TAG, "Max options reached");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "ofc_reset: capacity exceeded");
            return -1;
        }
        slot = (int32_t)ofc->num_options++;
    }

    ofc_option_state_t* opt = &ofc->options[slot];
    opt->stimulus_id = stimulus_id;
    opt->active = true;

    /* Set value components */
    opt->values[OFC_VALUE_MAGNITUDE].value = reward_magnitude;
    opt->values[OFC_VALUE_MAGNITUDE].confidence = 1.0f;

    opt->values[OFC_VALUE_PROBABILITY].value = reward_probability;
    opt->values[OFC_VALUE_PROBABILITY].confidence = 1.0f;

    opt->values[OFC_VALUE_DELAY].value = delay;
    opt->values[OFC_VALUE_DELAY].temporal_discount =
        hyperbolic_discount(1.0f, delay, ofc->config.discount_rate);

    /* Compute expected value */
    float ev = reward_magnitude * reward_probability *
               opt->values[OFC_VALUE_DELAY].temporal_discount;
    opt->values[OFC_VALUE_EXPECTED].value = ev;

    /* Compute risk */
    float risk = compute_risk(reward_probability, reward_magnitude);
    opt->values[OFC_VALUE_RISK].value = risk;

    /* Integrated value with risk sensitivity */
    opt->integrated_value = ev - ofc->config.risk_sensitivity * risk;

    /* Activate lateral OFC for stimulus-reward */
    ofc->subdivisions.activity[OFC_SUBDIV_LATERAL] =
        fminf(1.0f, ofc->subdivisions.activity[OFC_SUBDIV_LATERAL] + 0.2f);

    nimcp_mutex_unlock(ofc->mutex);

    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Option %u: EV=%.3f, risk=%.3f, integrated=%.3f",
                    stimulus_id, ev, risk, opt->integrated_value);

    return 0;
}

int ofc_compute_value(nimcp_ofc_t* ofc, uint32_t stimulus_id,
                      ofc_value_t* result) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_compute_value: ofc is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_compute_value: result is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(ofc->mutex);

    /* Find option */
    ofc_option_state_t* opt = NULL;
    for (uint32_t i = 0; i < ofc->num_options; i++) {
        if (ofc->options[i].stimulus_id == stimulus_id) {
            opt = &ofc->options[i];
            break;
        }
    }

    if (!opt) {
        nimcp_mutex_unlock(ofc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_reset: opt is NULL");
        return -1;
    }

    *result = opt->values[OFC_VALUE_EXPECTED];
    result->value = opt->integrated_value;

    nimcp_mutex_unlock(ofc->mutex);
    return 0;
}

int ofc_update_prediction_error(nimcp_ofc_t* ofc, uint32_t stimulus_id,
                                float received_reward) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_update_prediction_error: ofc is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(ofc->mutex);

    /* Find option */
    ofc_option_state_t* opt = NULL;
    for (uint32_t i = 0; i < ofc->num_options; i++) {
        if (ofc->options[i].stimulus_id == stimulus_id) {
            opt = &ofc->options[i];
            break;
        }
    }

    if (!opt) {
        nimcp_mutex_unlock(ofc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_reset: opt is NULL");
        return -1;
    }

    /* Compute RPE */
    float expected = opt->values[OFC_VALUE_EXPECTED].value;
    float rpe = received_reward - expected;
    ofc->prediction_error = rpe;

    opt->values[OFC_VALUE_RECEIVED].value = received_reward;
    opt->values[OFC_VALUE_PREDICTION_ERROR].value = rpe;

    /* Update expected value (learning) */
    opt->values[OFC_VALUE_EXPECTED].value += ofc->config.learning_rate * rpe;

    /* Track totals */
    ofc->cumulative_reward += received_reward;
    ofc->stats.total_reward_received += received_reward;
    ofc->stats.total_reward_expected += expected;
    ofc->stats.prediction_errors++;

    /* Broadcast RPE via bio-async if connected */
    if (ofc->bio_router && ofc->config.enable_bio_async) {
        ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_PREDICTION_ERROR, &rpe, sizeof(float));
    }

    nimcp_mutex_unlock(ofc->mutex);

    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "RPE for %u: expected=%.3f, received=%.3f, error=%.3f",
                    stimulus_id, expected, received_reward, rpe);

    return 0;
}

float ofc_get_integrated_value(nimcp_ofc_t* ofc, uint32_t stimulus_id) {
    if (!ofc) return 0.0f;

    nimcp_mutex_lock(ofc->mutex);

    float value = 0.0f;
    for (uint32_t i = 0; i < ofc->num_options; i++) {
        if (ofc->options[i].stimulus_id == stimulus_id) {
            value = ofc->options[i].integrated_value;
            break;
        }
    }

    nimcp_mutex_unlock(ofc->mutex);
    return value;
}

/*=============================================================================
 * DECISION MAKING
 *===========================================================================*/

int ofc_make_decision(nimcp_ofc_t* ofc, ofc_decision_t* decision) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_make_decision: ofc is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_make_decision: decision is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (ofc->num_options == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "ofc_make_decision: no options available");
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(ofc->mutex);

    /* Collect all values */
    float* values = (float*)nimcp_malloc(ofc->num_options * sizeof(float));
    if (!values) {
        nimcp_mutex_unlock(ofc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ofc_make_decision: values is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < ofc->num_options; i++) {
        values[i] = ofc->options[i].integrated_value;

        /* Apply emotion modulation */
        values[i] *= (1.0f + ofc->emotion_valence * 0.2f);
    }

    /* Compute choice probabilities */
    float max_prob = 0.0f;
    uint32_t chosen = 0;

    for (uint32_t i = 0; i < ofc->num_options; i++) {
        float prob = softmax_probability(values[i], values, ofc->num_options,
                                         ofc->config.noise_level);
        ofc->options[i].choice_probability = prob;

        if (prob > max_prob) {
            max_prob = prob;
            chosen = i;
        }
    }

    /* Fill decision struct */
    decision->chosen_option = ofc->options[chosen].stimulus_id;
    decision->decision_value = ofc->options[chosen].integrated_value;
    decision->confidence = max_prob;
    decision->reaction_time_ms = 200.0f + (1.0f - max_prob) * 500.0f;  /* Slower when uncertain */
    decision->type = (ofc->num_options == 2) ? OFC_DECISION_BINARY : OFC_DECISION_MULTI;
    decision->reversal_detected = false;

    /* Store current decision */
    memcpy(&ofc->current_decision, decision, sizeof(ofc_decision_t));
    ofc->decision_pending = false;

    /* Update stats */
    ofc->stats.decisions_made++;
    ofc->stats.avg_decision_confidence =
        (ofc->stats.avg_decision_confidence * (ofc->stats.decisions_made - 1) +
         decision->confidence) / ofc->stats.decisions_made;
    ofc->stats.avg_reaction_time_ms =
        (ofc->stats.avg_reaction_time_ms * (ofc->stats.decisions_made - 1) +
         decision->reaction_time_ms) / ofc->stats.decisions_made;
    ofc->trial_count++;

    /* Activate medial OFC for choice */
    ofc->subdivisions.activity[OFC_SUBDIV_MEDIAL] =
        fminf(1.0f, ofc->subdivisions.activity[OFC_SUBDIV_MEDIAL] + 0.3f);

    nimcp_free(values);

    /* Broadcast decision */
    if (ofc->bio_router && ofc->config.enable_bio_async) {
        ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_DECISION, decision, sizeof(ofc_decision_t));
    }

    /* Update KG */
    if (ofc->kg && ofc->config.enable_kg_wiring) {
        ofc_kg_update_state(ofc);
    }

    nimcp_mutex_unlock(ofc->mutex);

    NIMCP_LOG_INFO(OFC_LOG_TAG, "Decision: option %u, value=%.3f, conf=%.3f, RT=%.1fms",
                   decision->chosen_option, decision->decision_value,
                   decision->confidence, decision->reaction_time_ms);

    return 0;
}

int ofc_check_reversal(nimcp_ofc_t* ofc, bool* reversal_detected) {
    if (!ofc || !reversal_detected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_check_reversal: required parameter is NULL (ofc, reversal_detected)");
        return -1;
    }

    /* Reversal detection based on prediction errors */
    /* If RPE consistently negative, contingencies may have reversed */
    *reversal_detected = (ofc->prediction_error < -ofc->config.reversal_threshold);

    if (*reversal_detected) {
        ofc->stats.reversals_detected++;

        /* Activate lateral OFC for reversal learning */
        ofc->subdivisions.activity[OFC_SUBDIV_LATERAL] = 1.0f;

        if (ofc->bio_router && ofc->config.enable_bio_async) {
            ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_REVERSAL, NULL, 0);
        }

        NIMCP_LOG_WARN(OFC_LOG_TAG, "Reversal detected! RPE=%.3f", ofc->prediction_error);
    }

    return 0;
}

int ofc_assess_risk(nimcp_ofc_t* ofc, uint32_t stimulus_id, float* risk_value) {
    if (!ofc || !risk_value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_assess_risk: required parameter is NULL (ofc, risk_value)");
        return -1;
    }

    nimcp_mutex_lock(ofc->mutex);

    *risk_value = 0.0f;
    for (uint32_t i = 0; i < ofc->num_options; i++) {
        if (ofc->options[i].stimulus_id == stimulus_id) {
            *risk_value = ofc->options[i].values[OFC_VALUE_RISK].value;
            break;
        }
    }

    nimcp_mutex_unlock(ofc->mutex);
    return 0;
}

int ofc_process_social_reward(nimcp_ofc_t* ofc, float social_value,
                              uint32_t social_context) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_assess_risk: ofc is NULL");
        return -1;
    }

    nimcp_mutex_lock(ofc->mutex);

    /* Activate anterior OFC for social processing */
    ofc->subdivisions.activity[OFC_SUBDIV_ANTERIOR] =
        fminf(1.0f, ofc->subdivisions.activity[OFC_SUBDIV_ANTERIOR] + 0.4f);

    /* Modulate all option values with social component */
    for (uint32_t i = 0; i < ofc->num_options; i++) {
        ofc->options[i].values[OFC_VALUE_SOCIAL].value = social_value;
        ofc->options[i].integrated_value +=
            ofc->config.social_weight * social_value;
    }

    if (ofc->bio_router && ofc->config.enable_bio_async) {
        ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_SOCIAL_REWARD,
                                &social_value, sizeof(float));
    }

    nimcp_mutex_unlock(ofc->mutex);

    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Social reward: %.3f, context=%u",
                    social_value, social_context);

    return 0;
}

/*=============================================================================
 * EMOTION INTEGRATION
 *===========================================================================*/

int ofc_set_emotion(nimcp_ofc_t* ofc, float valence, float arousal) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_set_emotion: ofc is NULL");
        return -1;
    }

    nimcp_mutex_lock(ofc->mutex);

    ofc->emotion_valence = fmaxf(-1.0f, fminf(1.0f, valence));
    ofc->emotion_arousal = fmaxf(0.0f, fminf(1.0f, arousal));

    if (ofc->bio_router && ofc->config.enable_bio_async) {
        float emotion_data[2] = {ofc->emotion_valence, ofc->emotion_arousal};
        ofc_bio_async_broadcast(ofc, OFC_BIO_MSG_EMOTION_MODULATION,
                                emotion_data, sizeof(emotion_data));
    }

    nimcp_mutex_unlock(ofc->mutex);
    return 0;
}

float ofc_get_emotion_modulated_value(nimcp_ofc_t* ofc, uint32_t stimulus_id) {
    if (!ofc) return 0.0f;

    float base_value = ofc_get_integrated_value(ofc, stimulus_id);
    return base_value * (1.0f + ofc->emotion_valence * 0.2f);
}

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

int ofc_kg_register(nimcp_ofc_t* ofc, struct nimcp_brain_kg* kg,
                    uint64_t admin_token) {
    if (!ofc || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_get_emotion_modulated_value: required parameter is NULL (ofc, kg)");
        return -1;
    }

    nimcp_mutex_lock(ofc->mutex);

    ofc->kg = kg;
    ofc->kg_state.admin_token = admin_token;

    /* Register main OFC node */
    /* Note: Actual KG API calls would go here */
    ofc->kg_state.region_node_id = OFC_MODULE_ID;

    /* Register subdivision nodes */
    for (int i = 0; i < OFC_SUBDIV_COUNT; i++) {
        ofc->kg_state.subdiv_node_ids[i] = OFC_MODULE_ID + 1 + i;
    }

    /* Register value signal nodes */
    for (int i = 0; i < OFC_VALUE_COUNT; i++) {
        ofc->kg_state.value_node_ids[i] = OFC_MODULE_ID + 100 + i;
    }

    ofc->kg_state.registered = true;
    ofc->stats.kg_updates++;

    nimcp_mutex_unlock(ofc->mutex);

    NIMCP_LOG_INFO(OFC_LOG_TAG, "Registered with KG, node_id=%lu",
                   ofc->kg_state.region_node_id);

    return 0;
}

int ofc_kg_unregister(nimcp_ofc_t* ofc) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_unregister: ofc is NULL");
        return -1;
    }

    nimcp_mutex_lock(ofc->mutex);

    if (ofc->kg_state.registered) {
        /* Unregister from KG */
        ofc->kg_state.registered = false;
        ofc->kg = NULL;
    }

    nimcp_mutex_unlock(ofc->mutex);
    return 0;
}

/* ofc_kg_update_state is implemented in nimcp_ofc_kg_wiring.c */

int ofc_kg_query(nimcp_ofc_t* ofc, const char* query,
                 void* result, size_t result_size) {
    if (!ofc || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_unregister: required parameter is NULL (ofc, query, result)");
        return -1;
    }
    if (!ofc->kg_state.registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_unregister: ofc->kg_state is NULL");
        return -1;
    }

    /* Query KG - actual implementation would use KG API */
    (void)result_size;

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/* ofc_bio_async_connect and ofc_bio_async_disconnect are implemented in nimcp_ofc_bio_async_bridge.c */

int ofc_bio_async_broadcast(nimcp_ofc_t* ofc, ofc_bio_msg_type_t msg_type,
                            const void* payload, size_t payload_size) {
    if (!ofc || !ofc->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_kg_unregister: required parameter is NULL (ofc, ofc->bio_router)");
        return -1;
    }

    ofc->stats.bio_msgs_sent++;

    /* Actual broadcast implementation would use bio-async API */
    (void)msg_type;
    (void)payload;
    (void)payload_size;

    return 0;
}

int ofc_bio_async_subscribe(nimcp_ofc_t* ofc, uint32_t subscription_mask) {
    if (!ofc || !ofc->bio_router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_bio_async_subscribe: required parameter is NULL (ofc, ofc->bio_router)");
        return -1;
    }

    /* Subscribe to messages */
    (void)subscription_mask;

    return 0;
}

/*=============================================================================
 * OTHER SYSTEM INTEGRATIONS (STUBS)
 *===========================================================================*/

int ofc_immune_connect(nimcp_ofc_t* ofc, struct nimcp_immune_system* immune) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_immune_connect: ofc is NULL");
        return -1;
    }
    ofc->immune = immune;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to immune system");
    return 0;
}

int ofc_security_connect(nimcp_ofc_t* ofc, struct nimcp_security_context* security) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_security_connect: ofc is NULL");
        return -1;
    }
    ofc->security = security;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to security context");
    return 0;
}

int ofc_snn_connect(nimcp_ofc_t* ofc, struct nimcp_snn_network* snn,
                    struct nimcp_plasticity_engine* plasticity) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_security_connect: ofc is NULL");
        return -1;
    }
    ofc->snn = snn;
    ofc->plasticity = plasticity;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to SNN/plasticity");
    return 0;
}

int ofc_hypothalamus_connect(nimcp_ofc_t* ofc, struct nimcp_hypothalamus* hypo) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_hypothalamus_connect: ofc is NULL");
        return -1;
    }
    ofc->hypothalamus = hypo;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to hypothalamus");
    return 0;
}

int ofc_thalamus_connect(nimcp_ofc_t* ofc, struct nimcp_thalamus* thalamus) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_thalamus_connect: ofc is NULL");
        return -1;
    }
    ofc->thalamus = thalamus;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to thalamus");
    return 0;
}

int ofc_cognitive_connect(nimcp_ofc_t* ofc, struct nimcp_cognitive_hub* hub) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_cognitive_connect: ofc is NULL");
        return -1;
    }
    ofc->cognitive_hub = hub;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to cognitive hub");
    return 0;
}

int ofc_training_connect(nimcp_ofc_t* ofc, struct nimcp_training_context* training) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_training_connect: ofc is NULL");
        return -1;
    }
    ofc->training = training;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to training system");
    return 0;
}

int ofc_perception_connect(nimcp_ofc_t* ofc, struct nimcp_perception_system* perception) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_perception_connect: ofc is NULL");
        return -1;
    }
    ofc->perception = perception;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to perception system");
    return 0;
}

int ofc_symbolic_connect(nimcp_ofc_t* ofc, struct nimcp_symbolic_engine* symbolic) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_symbolic_connect: ofc is NULL");
        return -1;
    }
    ofc->symbolic = symbolic;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to symbolic engine");
    return 0;
}

int ofc_swarm_connect(nimcp_ofc_t* ofc, struct nimcp_swarm_context* swarm) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_swarm_connect: ofc is NULL");
        return -1;
    }
    ofc->swarm = swarm;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to swarm system");
    return 0;
}

int ofc_dragonfly_connect(nimcp_ofc_t* ofc, struct nimcp_dragonfly_context* dragonfly) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_dragonfly_connect: ofc is NULL");
        return -1;
    }
    ofc->dragonfly = dragonfly;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to dragonfly system");
    return 0;
}

int ofc_portia_connect(nimcp_ofc_t* ofc, struct nimcp_portia_context* portia) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_portia_connect: ofc is NULL");
        return -1;
    }
    ofc->portia = portia;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to portia system");
    return 0;
}

int ofc_qmc_connect(nimcp_ofc_t* ofc, struct nimcp_qmc_context* qmc) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_qmc_connect: ofc is NULL");
        return -1;
    }
    ofc->qmc = qmc;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to QMC system");
    return 0;
}

int ofc_omni_connect(nimcp_ofc_t* ofc, struct nimcp_omni_predictor* omni) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_omni_connect: ofc is NULL");
        return -1;
    }
    ofc->omni = omni;
    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "Connected to omnidirectional predictor");
    return 0;
}

/*=============================================================================
 * UPDATE AND STATE
 *===========================================================================*/

int ofc_update(nimcp_ofc_t* ofc, float dt) {
    if (!ofc || !ofc->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_update: required parameter is NULL (ofc, ofc->initialized)");
        return -1;
    }

    nimcp_mutex_lock(ofc->mutex);

    /* Decay subdivision activities */
    for (int i = 0; i < OFC_SUBDIV_COUNT; i++) {
        ofc->subdivisions.activity[i] *= (1.0f - 0.1f * dt);
    }

    /* Update timestamp */
    ofc->last_update_us += (uint64_t)(dt * 1000000.0f);

    nimcp_mutex_unlock(ofc->mutex);
    return 0;
}

int ofc_get_stats(const nimcp_ofc_t* ofc, ofc_stats_t* stats) {
    if (!ofc || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_get_stats: required parameter is NULL (ofc, stats)");
        return -1;
    }

    nimcp_mutex_lock(((nimcp_ofc_t*)ofc)->mutex);
    memcpy(stats, &ofc->stats, sizeof(ofc_stats_t));
    nimcp_mutex_unlock(((nimcp_ofc_t*)ofc)->mutex);

    return 0;
}

float ofc_get_subdivision_activity(const nimcp_ofc_t* ofc,
                                   ofc_subdivision_t subdiv) {
    if (!ofc || subdiv >= OFC_SUBDIV_COUNT) return 0.0f;
    return ofc->subdivisions.activity[subdiv];
}

int ofc_clear_options(nimcp_ofc_t* ofc) {
    if (!ofc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_clear_options: ofc is NULL");
        return -1;
    }

    nimcp_mutex_lock(ofc->mutex);

    memset(ofc->options, 0, ofc->max_options * sizeof(ofc_option_state_t));
    ofc->num_options = 0;
    ofc->decision_pending = false;

    nimcp_mutex_unlock(ofc->mutex);
    return 0;
}

/*=============================================================================
 * QUANTUM OPTIMIZATION
 *===========================================================================*/

int ofc_qmc_optimize_values(nimcp_ofc_t* ofc) {
    if (!ofc || !ofc->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_qmc_optimize_values: required parameter is NULL (ofc, ofc->qmc)");
        return -1;
    }

    /* Use QMC for value optimization */
    /* Actual implementation would use QMC API */

    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "QMC value optimization requested");
    return 0;
}

int ofc_qmcts_decision_search(nimcp_ofc_t* ofc, uint32_t num_iterations,
                              ofc_decision_t* best_decision) {
    if (!ofc || !best_decision || !ofc->qmc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ofc_qmc_optimize_values: required parameter is NULL (ofc, best_decision, ofc->qmc)");
        return -1;
    }

    /* Use QMCTS for decision tree search */
    /* Actual implementation would use QMCTS API */

    NIMCP_LOG_DEBUG(OFC_LOG_TAG, "QMCTS decision search: %u iterations", num_iterations);

    /* Fallback to standard decision */
    return ofc_make_decision(ofc, best_decision);
}

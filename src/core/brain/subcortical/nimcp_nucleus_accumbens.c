//=============================================================================
// nimcp_nucleus_accumbens.c - Nucleus Accumbens Implementation
//=============================================================================

#include "core/brain/subcortical/nimcp_nucleus_accumbens.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(nucleus_accumbens, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


struct nucleus_accumbens {
    nac_config_t config;
    nac_reward_t rewards[NAC_MAX_REWARDS];
    uint32_t num_rewards;
    nac_cue_t cues[NAC_MAX_CUES];
    uint32_t num_cues;
    float dopamine;
    float wanting;
    float liking;
    nac_motivation_state_t state;
    float* core_activity;
    float* shell_activity;
    nac_pit_state_t pit;
    nac_stats_t stats;
    nimcp_mutex_t* mutex;
};

void nac_default_config(nac_config_t* config) {
    if (!config) return;
    config->core_neurons = 128;
    config->shell_neurons = 128;
    config->da_sensitivity = 1.0f;
    config->learning_rate = 0.1f;
    config->satiation_decay = 0.01f;
    config->pavlovian_weight = NAC_PIT_PAVLOVIAN_WEIGHT;
    config->instrumental_weight = NAC_PIT_INSTRUMENTAL_WEIGHT;
    config->pit_threshold = 0.3f;
    config->enable_craving = true;
    config->craving_threshold = 0.7f;
    config->craving_decay = 0.05f;
    config->enable_hedonic_hotspots = true;
}

nucleus_accumbens_t* nac_create(const nac_config_t* config) {
    nucleus_accumbens_t* nac = nimcp_calloc(1, sizeof(nucleus_accumbens_t));
    if (!nac) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nac is NULL");

        return NULL;

    }
    if (config) nac->config = *config; else nac_default_config(&nac->config);
    nac->core_activity = nimcp_calloc(nac->config.core_neurons, sizeof(float));
    nac->shell_activity = nimcp_calloc(nac->config.shell_neurons, sizeof(float));
    nac->dopamine = NAC_DA_BASELINE;
    nac->state = NAC_STATE_NEUTRAL;
    nac->mutex = nimcp_mutex_create(NULL);
    if (!nac->core_activity || !nac->shell_activity || !nac->mutex) { nac_destroy(nac); return NULL; }
    return nac;
}

void nac_destroy(nucleus_accumbens_t* nac) {
    if (!nac) return;
    if (nac->mutex) { nimcp_mutex_free(nac->mutex); }
    nimcp_free(nac->core_activity);
    nimcp_free(nac->shell_activity);
    nimcp_free(nac->pit.action_biases);
    nimcp_free(nac);
}

int nac_reset(nucleus_accumbens_t* nac) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_reset: nac is NULL");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    nac->dopamine = NAC_DA_BASELINE;
    nac->wanting = 0;
    nac->liking = 0;
    nac->state = NAC_STATE_NEUTRAL;
    memset(nac->core_activity, 0, nac->config.core_neurons * sizeof(float));
    memset(nac->shell_activity, 0, nac->config.shell_neurons * sizeof(float));
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

int nac_register_reward(nucleus_accumbens_t* nac, const nac_reward_t* reward, uint32_t* out_id) {
    if (!nac || !reward || nac->num_rewards >= NAC_MAX_REWARDS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_register_reward: required parameter is NULL (nac, reward)");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    nac->rewards[nac->num_rewards] = *reward;
    nac->rewards[nac->num_rewards].id = nac->num_rewards;
    if (out_id) *out_id = nac->num_rewards;
    nac->num_rewards++;
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

int nac_process_reward(nucleus_accumbens_t* nac, uint32_t reward_id, float magnitude) {
    if (!nac || reward_id >= nac->num_rewards) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nac_process_reward: nac is NULL");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    nac_reward_t* r = &nac->rewards[reward_id];
    r->current_satiation += magnitude * r->satiation_rate;
    r->current_satiation = nimcp_clampf(r->current_satiation, 0, 1);
    nac->liking = magnitude * (1.0f - r->current_satiation);
    nac->stats.rewards_processed++;
    if (nac->liking > 0.5f) nac->state = NAC_STATE_LIKING;
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

int nac_process_reward_prediction(nucleus_accumbens_t* nac, uint32_t reward_id, float predicted_value) {
    if (!nac || reward_id >= nac->num_rewards) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nac_process_reward_prediction: nac is NULL");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    nac->wanting = predicted_value * nac->dopamine;
    if (nac->wanting > nac->config.craving_threshold && nac->config.enable_craving) {
        nac->state = NAC_STATE_CRAVING;
    } else if (nac->wanting > 0.3f) {
        nac->state = NAC_STATE_WANTING;
    }
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

float nac_get_wanting(const nucleus_accumbens_t* nac, uint32_t reward_id) {
    if (!nac || reward_id >= nac->num_rewards) return 0;
    return nac->wanting * (1.0f - nac->rewards[reward_id].current_satiation);
}

float nac_get_liking(const nucleus_accumbens_t* nac, uint32_t reward_id) {
    if (!nac || reward_id >= nac->num_rewards) return 0;
    return nac->liking;
}

int nac_register_cue(nucleus_accumbens_t* nac, const nac_cue_t* cue, uint32_t* out_id) {
    if (!nac || !cue || nac->num_cues >= NAC_MAX_CUES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_register_cue: required parameter is NULL (nac, cue)");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    nac->cues[nac->num_cues] = *cue;
    nac->cues[nac->num_cues].id = nac->num_cues;
    if (out_id) *out_id = nac->num_cues;
    nac->num_cues++;
    nac->stats.cue_associations++;
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

int nac_process_cue(nucleus_accumbens_t* nac, uint32_t cue_id, float intensity) {
    if (!nac || cue_id >= nac->num_cues) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nac_process_cue: nac is NULL");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    nac_cue_t* c = &nac->cues[cue_id];
    c->conditioned_response = c->association_strength * intensity * nac->dopamine;
    nac->wanting += c->conditioned_response * nac->config.pavlovian_weight;
    nac->wanting = nimcp_clampf(nac->wanting, 0, 1);
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

int nac_associate_cue_reward(nucleus_accumbens_t* nac, uint32_t cue_id, uint32_t reward_id, float reward_magnitude) {
    if (!nac || cue_id >= nac->num_cues || reward_id >= nac->num_rewards) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nac_associate_cue_reward: nac is NULL");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    nac_cue_t* c = &nac->cues[cue_id];
    c->associated_reward = reward_id;
    c->association_strength += (reward_magnitude - c->association_strength) * nac->config.learning_rate;
    c->pairings++;
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

float nac_get_conditioned_response(const nucleus_accumbens_t* nac, uint32_t cue_id) {
    if (!nac || cue_id >= nac->num_cues) return 0;
    return nac->cues[cue_id].conditioned_response;
}

int nac_compute_pit(nucleus_accumbens_t* nac, const float* cue_activations, uint32_t num_cues, nac_pit_state_t* out_pit) {
    if (!nac || !out_pit) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_compute_pit: required parameter is NULL (nac, out_pit)");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    out_pit->pavlovian_bias = 0;
    out_pit->general_activation = 0;
    for (uint32_t i = 0; i < num_cues && i < nac->num_cues; i++) {
        if (cue_activations) {
            out_pit->pavlovian_bias += nac->cues[i].association_strength * cue_activations[i];
            out_pit->general_activation += cue_activations[i] * nac->dopamine;
        }
    }
    out_pit->instrumental_control = 1.0f - out_pit->pavlovian_bias * nac->config.pavlovian_weight;
    nac->pit = *out_pit;
    nac->stats.pit_effect = out_pit->pavlovian_bias;
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

int nac_get_action_bias(const nucleus_accumbens_t* nac, float* action_biases, uint32_t num_actions) {
    if (!nac || !action_biases) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_get_action_bias: required parameter is NULL (nac, action_biases)");
        return -1;
    }
    for (uint32_t i = 0; i < num_actions; i++) {
        action_biases[i] = nac->pit.pavlovian_bias * nac->config.pavlovian_weight;
    }
    return 0;
}

int nac_set_pit_balance(nucleus_accumbens_t* nac, float pavlovian_weight, float instrumental_weight) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_set_pit_balance: nac is NULL");
        return -1;
    }
    nac->config.pavlovian_weight = nimcp_clampf(pavlovian_weight, 0, 1);
    nac->config.instrumental_weight = nimcp_clampf(instrumental_weight, 0, 1);
    return 0;
}

int nac_set_dopamine(nucleus_accumbens_t* nac, float level) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_set_dopamine: nac is NULL");
        return -1;
    }
    nac->dopamine = nimcp_clampf(level, 0, 1);
    return 0;
}

int nac_trigger_dopamine_burst(nucleus_accumbens_t* nac, float magnitude) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_trigger_dopamine_burst: nac is NULL");
        return -1;
    }
    nac->dopamine = nimcp_clampf(nac->dopamine + magnitude * NAC_DA_BURST_MULTIPLIER * 0.1f, 0, 1);
    return 0;
}

int nac_trigger_dopamine_pause(nucleus_accumbens_t* nac, float magnitude) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_trigger_dopamine_pause: nac is NULL");
        return -1;
    }
    nac->dopamine = nimcp_clampf(nac->dopamine * NAC_DA_PAUSE_MULTIPLIER, 0, 1);
    return 0;
}

float nac_get_dopamine(const nucleus_accumbens_t* nac) {
    return nac ? nac->dopamine : 0;
}

int nac_step(nucleus_accumbens_t* nac, float dt_ms) {
    if (!nac || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nac_step: nac is NULL");
        return -1;
    }
    nimcp_mutex_lock(nac->mutex);
    float decay = nac->config.satiation_decay * dt_ms / 1000.0f;
    for (uint32_t i = 0; i < nac->num_rewards; i++) {
        nac->rewards[i].current_satiation -= decay;
        if (nac->rewards[i].current_satiation < 0) nac->rewards[i].current_satiation = 0;
    }
    nac->dopamine += (NAC_DA_BASELINE - nac->dopamine) * 0.01f;
    nac->wanting *= (1.0f - 0.1f * dt_ms / 1000.0f);
    nac->liking *= (1.0f - 0.2f * dt_ms / 1000.0f);
    if (nac->wanting < 0.1f && nac->liking < 0.1f) nac->state = NAC_STATE_NEUTRAL;
    nac->stats.state = nac->state;
    nac->stats.wanting_level = nac->wanting;
    nac->stats.liking_level = nac->liking;
    nac->stats.dopamine_level = nac->dopamine;
    nimcp_mutex_unlock(nac->mutex);
    return 0;
}

int nac_get_output(const nucleus_accumbens_t* nac, float* core_output, float* shell_output) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_get_output: nac is NULL");
        return -1;
    }
    if (core_output) *core_output = nac->wanting;
    if (shell_output) *shell_output = nac->liking;
    return 0;
}

nac_motivation_state_t nac_get_motivation_state(const nucleus_accumbens_t* nac) {
    return nac ? nac->state : NAC_STATE_NEUTRAL;
}

int nac_get_stats(const nucleus_accumbens_t* nac, nac_stats_t* stats) {
    if (!nac || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_get_stats: required parameter is NULL (nac, stats)");
        return -1;
    }
    *stats = nac->stats;
    return 0;
}

int nac_receive_vta_input(nucleus_accumbens_t* nac, float dopamine, float rpe) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_receive_vta_input: nac is NULL");
        return -1;
    }
    nac_set_dopamine(nac, dopamine);
    if (rpe > 0) nac_trigger_dopamine_burst(nac, rpe);
    else if (rpe < 0) nac_trigger_dopamine_pause(nac, -rpe);
    return 0;
}

int nac_receive_amygdala_input(nucleus_accumbens_t* nac, float valence, float arousal) {
    if (!nac) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nac_receive_amygdala_input: nac is NULL");
        return -1;
    }
    if (valence < 0) nac->state = NAC_STATE_AVERSIVE;
    nac->wanting *= (1.0f + valence * arousal);
    return 0;
}

int nac_receive_hippocampal_input(nucleus_accumbens_t* nac, const float* context, uint32_t context_dim) {
    (void)context; (void)context_dim;
    return nac ? 0 : -1;
}

int nac_receive_pfc_input(nucleus_accumbens_t* nac, const float* goal_vector, uint32_t goal_dim) {
    (void)goal_vector; (void)goal_dim;
    return nac ? 0 : -1;
}

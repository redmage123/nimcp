//=============================================================================
// nimcp_striatal_interneurons.c - Striatal Interneuron Networks
//=============================================================================

#include "core/brain/subcortical/nimcp_striatal_interneurons.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(striatal_interneurons, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct striatal_interneurons {
    /* Configuration */
    sint_config_t config;

    /* FSI population */
    sint_fsi_state_t* fsi_units;
    uint32_t num_fsi;

    /* TAN population */
    sint_tan_unit_t* tan_units;
    uint32_t num_tan;

    /* LTS population */
    sint_lts_state_t* lts_units;
    uint32_t num_lts;

    /* NGF population */
    sint_ngf_state_t* ngf_units;
    uint32_t num_ngf;

    /* MSN output arrays */
    float* msn_inhibition;
    uint32_t num_msn;

    /* Global state */
    float cortical_input_level;
    float dopamine_level;
    float thalamic_input;
    float salience_level;

    /* Population rates */
    float fsi_population_rate;
    float tan_population_rate;
    float ach_level;

    /* WTA state */
    uint32_t wta_winner;
    float wta_strength;

    /* Statistics */
    sint_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void sint_default_config(sint_config_t* config) {
    if (!config) return;

    config->num_fsi = SINT_DEFAULT_FSI_COUNT;
    config->num_tan = SINT_DEFAULT_TAN_COUNT;
    config->num_lts = SINT_DEFAULT_LTS_COUNT;
    config->num_ngf = SINT_DEFAULT_NGF_COUNT;
    config->num_msn = 128;

    config->fsi_cortical_weight = 0.8f;
    config->fsi_lateral_inhibition = 0.3f;
    config->tan_pause_threshold = 0.6f;
    config->tan_da_sensitivity = 0.5f;

    config->enable_fsi_wta = true;
    config->enable_tan_gating = true;
}

striatal_interneurons_t* sint_create(const sint_config_t* config) {
    striatal_interneurons_t* sint = nimcp_calloc(1, sizeof(striatal_interneurons_t));
    if (!sint) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sint is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        sint->config = *config;
    } else {
        sint_default_config(&sint->config);
    }

    /* Allocate FSI units */
    sint->num_fsi = sint->config.num_fsi;
    sint->fsi_units = nimcp_calloc(sint->num_fsi, sizeof(sint_fsi_state_t));
    if (!sint->fsi_units) goto cleanup;

    /* Allocate TAN units */
    sint->num_tan = sint->config.num_tan;
    sint->tan_units = nimcp_calloc(sint->num_tan, sizeof(sint_tan_unit_t));
    if (!sint->tan_units) goto cleanup;

    /* Initialize TAN tonic firing */
    for (uint32_t i = 0; i < sint->num_tan; i++) {
        sint->tan_units[i].state = SINT_TAN_STATE_TONIC;
        sint->tan_units[i].tonic_rate = SINT_TAN_TONIC_RATE_HZ;
        sint->tan_units[i].ach_level = SINT_TAN_ACH_RELEASE;
    }

    /* Allocate LTS units */
    sint->num_lts = sint->config.num_lts;
    sint->lts_units = nimcp_calloc(sint->num_lts, sizeof(sint_lts_state_t));
    if (!sint->lts_units) goto cleanup;

    /* Allocate NGF units */
    sint->num_ngf = sint->config.num_ngf;
    sint->ngf_units = nimcp_calloc(sint->num_ngf, sizeof(sint_ngf_state_t));
    if (!sint->ngf_units) goto cleanup;

    /* Allocate MSN inhibition array */
    sint->num_msn = sint->config.num_msn;
    sint->msn_inhibition = nimcp_calloc(sint->num_msn, sizeof(float));
    if (!sint->msn_inhibition) goto cleanup;

    /* Initialize global state */
    sint->dopamine_level = 0.5f;
    sint->ach_level = SINT_TAN_ACH_RELEASE;
    sint->wta_winner = UINT32_MAX;

    /* Create mutex */
    sint->mutex = nimcp_mutex_create(NULL);
    if (!sint->mutex) goto cleanup;

    return sint;

cleanup:
    sint_destroy(sint);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_create: operation failed");
    return NULL;
}

void sint_destroy(striatal_interneurons_t* sint) {
    if (!sint) return;

    if (sint->mutex) {
        nimcp_mutex_free(sint->mutex);
    }

    nimcp_free(sint->fsi_units);
    nimcp_free(sint->tan_units);
    nimcp_free(sint->lts_units);
    nimcp_free(sint->ngf_units);
    nimcp_free(sint->msn_inhibition);
    nimcp_free(sint);
}

int sint_reset(striatal_interneurons_t* sint) {
    if (!sint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_reset: sint is NULL");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);

    /* Reset FSI units */
    for (uint32_t i = 0; i < sint->num_fsi; i++) {
        sint->fsi_units[i].activation = 0.0f;
        sint->fsi_units[i].firing_rate = 0.0f;
    }

    /* Reset TAN units to tonic firing */
    for (uint32_t i = 0; i < sint->num_tan; i++) {
        sint->tan_units[i].state = SINT_TAN_STATE_TONIC;
        sint->tan_units[i].pause_timer = 0.0f;
        sint->tan_units[i].ach_level = SINT_TAN_ACH_RELEASE;
        sint->tan_units[i].salience_triggered = false;
    }

    /* Reset LTS units */
    for (uint32_t i = 0; i < sint->num_lts; i++) {
        sint->lts_units[i].activation = 0.0f;
        sint->lts_units[i].is_bursting = false;
    }

    /* Reset NGF units */
    for (uint32_t i = 0; i < sint->num_ngf; i++) {
        sint->ngf_units[i].activation = 0.0f;
        sint->ngf_units[i].gaba_volume = 0.0f;
    }

    /* Reset MSN inhibition */
    memset(sint->msn_inhibition, 0, sint->num_msn * sizeof(float));

    /* Reset global state */
    sint->cortical_input_level = 0.0f;
    sint->dopamine_level = 0.5f;
    sint->thalamic_input = 0.0f;
    sint->salience_level = 0.0f;
    sint->fsi_population_rate = 0.0f;
    sint->tan_population_rate = SINT_TAN_TONIC_RATE_HZ;
    sint->ach_level = SINT_TAN_ACH_RELEASE;
    sint->wta_winner = UINT32_MAX;
    sint->wta_strength = 0.0f;

    /* Reset statistics */
    memset(&sint->stats, 0, sizeof(sint_stats_t));

    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

/* ============================================================================
 * INPUT IMPLEMENTATION
 * ============================================================================ */

int sint_set_cortical_input(striatal_interneurons_t* sint,
                             const float* cortical,
                             uint32_t size) {
    if (!sint || !cortical) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_reset: required parameter is NULL (sint, cortical)");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);

    /* Compute mean cortical input level */
    float mean = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        mean += cortical[i];
    }
    mean /= (float)size;

    sint->cortical_input_level = nimcp_clampf(mean, 0.0f, 1.0f);

    /* Distribute to FSI units */
    for (uint32_t i = 0; i < sint->num_fsi; i++) {
        uint32_t cortical_idx = i % size;
        sint->fsi_units[i].activation =
            cortical[cortical_idx] * sint->config.fsi_cortical_weight;
    }

    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

int sint_set_dopamine(striatal_interneurons_t* sint, float dopamine) {
    if (!sint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_set_dopamine: sint is NULL");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);
    sint->dopamine_level = nimcp_clampf(dopamine, 0.0f, 1.0f);
    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

int sint_signal_salience(striatal_interneurons_t* sint, float salience) {
    if (!sint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_signal_salience: sint is NULL");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);

    sint->salience_level = nimcp_clampf(salience, 0.0f, 1.0f);

    /* Check if salience triggers TAN pause */
    if (salience > sint->config.tan_pause_threshold) {
        for (uint32_t i = 0; i < sint->num_tan; i++) {
            if (sint->tan_units[i].state == SINT_TAN_STATE_TONIC) {
                sint->tan_units[i].state = SINT_TAN_STATE_PAUSING;
                sint->tan_units[i].pause_timer = SINT_TAN_PAUSE_DURATION_MS;
                sint->tan_units[i].salience_triggered = true;
                sint->stats.pause_count++;
            }
        }
    }

    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

int sint_set_thalamic_input(striatal_interneurons_t* sint, float input) {
    if (!sint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_set_thalamic_input: sint is NULL");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);
    sint->thalamic_input = nimcp_clampf(input, 0.0f, 1.0f);
    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

/* ============================================================================
 * PROCESSING IMPLEMENTATION
 * ============================================================================ */

int sint_step(striatal_interneurons_t* sint, float dt_ms) {
    if (!sint || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sint_step: sint is NULL");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);

    /* Update FSI units */
    float fsi_rate_sum = 0.0f;
    for (uint32_t i = 0; i < sint->num_fsi; i++) {
        sint_fsi_state_t* fsi = &sint->fsi_units[i];

        /* Update activation */
        float target_act = sint->cortical_input_level * sint->config.fsi_cortical_weight;
        fsi->activation += (target_act - fsi->activation) * 0.1f * dt_ms / 10.0f;
        fsi->activation = nimcp_clampf(fsi->activation, 0.0f, 1.0f);

        /* Compute firing rate */
        if (fsi->activation > SINT_FSI_THRESHOLD) {
            fsi->firing_rate = SINT_FSI_MAX_RATE_HZ *
                               (fsi->activation - SINT_FSI_THRESHOLD) /
                               (1.0f - SINT_FSI_THRESHOLD);
        } else {
            fsi->firing_rate = 0.0f;
        }

        fsi_rate_sum += fsi->firing_rate;
    }
    sint->fsi_population_rate = fsi_rate_sum / sint->num_fsi;

    /* Update TAN units */
    float tan_rate_sum = 0.0f;
    float ach_sum = 0.0f;
    bool any_pausing = false;

    for (uint32_t i = 0; i < sint->num_tan; i++) {
        sint_tan_unit_t* tan = &sint->tan_units[i];

        switch (tan->state) {
            case SINT_TAN_STATE_TONIC:
                tan->ach_level = SINT_TAN_ACH_RELEASE;
                tan_rate_sum += tan->tonic_rate;
                break;

            case SINT_TAN_STATE_PAUSING:
                tan->pause_timer -= dt_ms;
                tan->ach_level *= (1.0f - 0.1f * dt_ms / 10.0f);
                tan->pause_depth = 1.0f - (tan->pause_timer / SINT_TAN_PAUSE_DURATION_MS);
                any_pausing = true;

                if (tan->pause_timer <= 0) {
                    tan->state = SINT_TAN_STATE_BURST;
                    tan->pause_timer = 50.0f;  /* Burst duration */
                }
                break;

            case SINT_TAN_STATE_BURST:
                tan->pause_timer -= dt_ms;
                tan->ach_level = SINT_TAN_ACH_RELEASE * 2.0f;  /* Enhanced ACh release */
                tan_rate_sum += tan->tonic_rate * 3.0f;

                if (tan->pause_timer <= 0) {
                    tan->state = SINT_TAN_STATE_RECOVERING;
                    tan->pause_timer = 100.0f;
                }
                break;

            case SINT_TAN_STATE_RECOVERING:
                tan->pause_timer -= dt_ms;
                tan->ach_level = SINT_TAN_ACH_RELEASE * (1.0f - tan->pause_timer / 100.0f);
                tan_rate_sum += tan->tonic_rate * (1.0f - tan->pause_timer / 100.0f);

                if (tan->pause_timer <= 0) {
                    tan->state = SINT_TAN_STATE_TONIC;
                    tan->salience_triggered = false;
                }
                break;

            default:
                break;
        }

        ach_sum += tan->ach_level;
    }

    sint->tan_population_rate = tan_rate_sum / sint->num_tan;
    sint->ach_level = ach_sum / sint->num_tan;

    /* Update LTS units */
    for (uint32_t i = 0; i < sint->num_lts; i++) {
        sint_lts_state_t* lts = &sint->lts_units[i];

        /* LTS activation depends on cortical and thalamic input */
        float target = (sint->cortical_input_level + sint->thalamic_input) * 0.5f;
        lts->activation += (target - lts->activation) * 0.05f * dt_ms / 10.0f;

        lts->is_bursting = lts->activation > SINT_LTS_THRESHOLD;
        lts->dendritic_inhibition = lts->is_bursting ? 0.5f : 0.1f;
    }

    /* Update NGF units */
    for (uint32_t i = 0; i < sint->num_ngf; i++) {
        sint_ngf_state_t* ngf = &sint->ngf_units[i];

        /* NGF slow activation */
        ngf->activation += (sint->fsi_population_rate / SINT_FSI_MAX_RATE_HZ - ngf->activation) *
                           0.01f * dt_ms / 10.0f;
        ngf->gaba_volume = ngf->activation * 0.3f;
        ngf->decay_rate = 0.02f;
    }

    /* Compute MSN inhibition */
    for (uint32_t m = 0; m < sint->num_msn; m++) {
        float inhibition = 0.0f;

        /* FSI contribution (strong, fast) */
        uint32_t fsi_idx = m % sint->num_fsi;
        inhibition += sint->fsi_units[fsi_idx].firing_rate / SINT_FSI_MAX_RATE_HZ *
                      SINT_FSI_INHIBITION_WEIGHT;

        /* LTS contribution (dendritic) */
        uint32_t lts_idx = m % sint->num_lts;
        inhibition += sint->lts_units[lts_idx].dendritic_inhibition * 0.3f;

        /* NGF contribution (volume) */
        uint32_t ngf_idx = m % sint->num_ngf;
        inhibition += sint->ngf_units[ngf_idx].gaba_volume;

        sint->msn_inhibition[m] = nimcp_clampf(inhibition, 0.0f, 1.0f);
    }

    /* Compute WTA winner if enabled */
    if (sint->config.enable_fsi_wta) {
        float max_activity = 0.0f;
        sint->wta_winner = UINT32_MAX;

        for (uint32_t i = 0; i < sint->num_fsi; i++) {
            if (sint->fsi_units[i].activation > max_activity) {
                max_activity = sint->fsi_units[i].activation;
                sint->wta_winner = i;
            }
        }
        sint->wta_strength = max_activity;
    }

    /* Update statistics */
    sint->stats.avg_fsi_rate = sint->fsi_population_rate;
    sint->stats.avg_tan_rate = sint->tan_population_rate;
    sint->stats.ach_modulation = sint->ach_level;
    sint->stats.inhibition_strength = 0.0f;
    for (uint32_t m = 0; m < sint->num_msn; m++) {
        sint->stats.inhibition_strength += sint->msn_inhibition[m];
    }
    sint->stats.inhibition_strength /= sint->num_msn;

    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

int sint_get_output(const striatal_interneurons_t* sint, sint_output_t* output) {
    if (!sint || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_get_output: required parameter is NULL (sint, output)");
        return -1;
    }

    output->msn_inhibition = sint->msn_inhibition;
    output->ach_level = sint->ach_level;
    output->fsi_population_rate = sint->fsi_population_rate;
    output->tan_population_rate = sint->tan_population_rate;

    /* Check if any TANs are pausing */
    output->tan_pausing = false;
    for (uint32_t i = 0; i < sint->num_tan; i++) {
        if (sint->tan_units[i].state == SINT_TAN_STATE_PAUSING) {
            output->tan_pausing = true;
            break;
        }
    }

    output->wta_strength = sint->wta_strength;
    return 0;
}

float sint_get_msn_inhibition(const striatal_interneurons_t* sint, uint32_t msn_idx) {
    if (!sint || msn_idx >= sint->num_msn) return 0.0f;
    return sint->msn_inhibition[msn_idx];
}

float sint_get_ach_level(const striatal_interneurons_t* sint) {
    if (!sint) return 0.0f;
    return sint->ach_level;
}

bool sint_is_tan_pausing(const striatal_interneurons_t* sint) {
    if (!sint) {
        return false;
    }

    for (uint32_t i = 0; i < sint->num_tan; i++) {
        if (sint->tan_units[i].state == SINT_TAN_STATE_PAUSING) {
            return true;
        }
    }
    return false;
}

/* ============================================================================
 * FSI-SPECIFIC IMPLEMENTATION
 * ============================================================================ */

int sint_get_fsi_activity(const striatal_interneurons_t* sint,
                           float* activity,
                           uint32_t* count) {
    if (!sint || !activity || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_is_tan_pausing: required parameter is NULL (sint, activity, count)");
        return -1;
    }

    *count = sint->num_fsi;
    for (uint32_t i = 0; i < sint->num_fsi; i++) {
        activity[i] = sint->fsi_units[i].activation;
    }
    return 0;
}

int sint_apply_fsi_lateral_inhibition(striatal_interneurons_t* sint) {
    if (!sint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_apply_fsi_lateral_inhibition: sint is NULL");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);

    /* Find mean activity */
    float mean = 0.0f;
    for (uint32_t i = 0; i < sint->num_fsi; i++) {
        mean += sint->fsi_units[i].activation;
    }
    mean /= sint->num_fsi;

    /* Apply lateral inhibition */
    for (uint32_t i = 0; i < sint->num_fsi; i++) {
        float inhibition = mean * sint->config.fsi_lateral_inhibition;
        sint->fsi_units[i].activation -= inhibition;
        sint->fsi_units[i].activation = nimcp_clampf(sint->fsi_units[i].activation, 0.0f, 1.0f);
    }

    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

int sint_get_wta_winner(const striatal_interneurons_t* sint,
                         uint32_t* winner_idx,
                         float* winner_strength) {
    if (!sint || !winner_idx || !winner_strength) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_apply_fsi_lateral_inhibition: required parameter is NULL (sint, winner_idx, winner_strength)");
        return -1;
    }

    *winner_idx = sint->wta_winner;
    *winner_strength = sint->wta_strength;
    return 0;
}

/* ============================================================================
 * TAN-SPECIFIC IMPLEMENTATION
 * ============================================================================ */

sint_tan_state_t sint_get_tan_state(const striatal_interneurons_t* sint) {
    if (!sint || sint->num_tan == 0) return SINT_TAN_STATE_TONIC;

    /* Return state of first TAN as representative */
    return sint->tan_units[0].state;
}

int sint_force_tan_pause(striatal_interneurons_t* sint, float duration_ms) {
    if (!sint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_force_tan_pause: sint is NULL");
        return -1;
    }

    nimcp_mutex_lock(sint->mutex);

    for (uint32_t i = 0; i < sint->num_tan; i++) {
        sint->tan_units[i].state = SINT_TAN_STATE_PAUSING;
        sint->tan_units[i].pause_timer = duration_ms;
        sint->tan_units[i].salience_triggered = false;
    }
    sint->stats.pause_count++;

    nimcp_mutex_unlock(sint->mutex);
    return 0;
}

float sint_get_tan_pause_depth(const striatal_interneurons_t* sint) {
    if (!sint || sint->num_tan == 0) return 0.0f;

    float max_depth = 0.0f;
    for (uint32_t i = 0; i < sint->num_tan; i++) {
        if (sint->tan_units[i].pause_depth > max_depth) {
            max_depth = sint->tan_units[i].pause_depth;
        }
    }
    return max_depth;
}

/* ============================================================================
 * QUERY IMPLEMENTATION
 * ============================================================================ */

int sint_get_stats(const striatal_interneurons_t* sint, sint_stats_t* stats) {
    if (!sint || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sint_get_stats: required parameter is NULL (sint, stats)");
        return -1;
    }
    *stats = sint->stats;
    return 0;
}

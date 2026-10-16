/**
 * @file nimcp_dopamine_release.c
 * @brief Dopamine release dynamics implementation
 * @date 2026-01-11
 */

#include "core/brain/regions/vta/nimcp_dopamine_release.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(dopamine_release, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float michaelis_menten(float substrate, float vmax, float km) {
    if (substrate <= 0.0f) return 0.0f;
    return (vmax * substrate) / (km + substrate);
}

static float hill_equation(float ligand, float kd, float n) {
    if (ligand <= 0.0f) return 0.0f;
    float ln = powf(ligand, n);
    float kdn = powf(kd, n);
    return ln / (ln + kdn);
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_da_release_config_t nimcp_da_release_default_config(void) {
    nimcp_da_release_config_t config;
    memset(&config, 0, sizeof(config));

    config.release_probability = DA_RELEASE_PROBABILITY;
    config.quanta_per_vesicle = DA_QUANTA_PER_VESICLE;
    config.dat_vmax = DA_REUPTAKE_VMAX;
    config.dat_km = DA_REUPTAKE_KM;
    config.mao_rate = DA_MAO_RATE;
    config.comt_rate = DA_COMT_RATE;
    config.enable_autoreceptors = true;
    config.autoreceptor_ic50 = 50.0f;

    return config;
}

int nimcp_da_release_init(
    nimcp_da_release_system_t* system,
    const nimcp_da_release_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(*system));

    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_da_release_default_config();
    }

    /* Initialize vesicle pools */
    system->vesicles.readily_releasable = DA_VESICLE_POOL_SIZE / 10;
    system->vesicles.recycling = 0;
    system->vesicles.reserve = DA_VESICLE_POOL_SIZE - system->vesicles.readily_releasable;
    system->vesicles.refill_rate = 10.0f;
    system->vesicles.recycling_time = 100.0f;

    /* Initialize concentrations */
    system->concentrations.vesicular = 100.0f;
    system->concentrations.cytosolic = 50.0f;
    system->concentrations.synaptic = 50.0f;
    system->concentrations.extrasynaptic = 20.0f;

    /* Initialize receptors */
    for (int i = 0; i < DA_RECEPTOR_TYPE_COUNT; i++) {
        system->receptors[i].density = 100.0f;
        system->receptors[i].activation = 0.0f;
        system->receptors[i].kd = 20.0f + i * 10.0f;
        system->receptors[i].hill_coeff = 1.0f;
        system->receptors[i].desensitization = 0.0f;
        system->receptors[i].internalized = 0.0f;
    }

    /* D2 has higher affinity */
    system->receptors[DA_RECEPTOR_TYPE_D2].kd = 10.0f;

    /* Initialize transporter */
    system->transporter.vmax = system->config.dat_vmax;
    system->transporter.km = system->config.dat_km;
    system->transporter.inhibition = 0.0f;
    system->transporter.expression = 1.0f;
    system->transporter.enabled = true;

    system->release_efficacy = 1.0f;
    system->initialized = true;

    return 0;
}

int nimcp_da_release_shutdown(nimcp_da_release_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(*system));
    return 0;
}

int nimcp_da_release_reset(nimcp_da_release_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    nimcp_da_release_config_t config = system->config;
    nimcp_da_release_shutdown(system);
    return nimcp_da_release_init(system, &config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_da_release_update(
    nimcp_da_release_system_t* system,
    float dt,
    float firing_rate
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_release_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_da_release_update: validation failed");
        return -1;
    }

    /* Compute spikes */
    float spikes_per_update = firing_rate * dt / 1000.0f;
    uint32_t spikes = (uint32_t)spikes_per_update;
    if (spikes > 0) {
        nimcp_da_release_trigger(system, spikes);
    }

    /* Diffusion from synaptic to extrasynaptic */
    float diffusion_rate = DA_DIFFUSION_COEFF * dt;
    float diff = (system->concentrations.synaptic - system->concentrations.extrasynaptic) *
                  diffusion_rate * 0.01f;
    system->concentrations.synaptic -= diff;
    system->concentrations.extrasynaptic += diff;

    /* DAT-mediated uptake */
    if (system->transporter.enabled) {
        float effective_vmax = system->transporter.vmax *
                               (1.0f - system->transporter.inhibition) *
                               system->transporter.expression;
        float uptake_rate = michaelis_menten(
            system->concentrations.synaptic,
            effective_vmax,
            system->transporter.km
        );
        float uptake = uptake_rate * dt * 0.001f;
        system->concentrations.synaptic -= uptake;
        system->concentrations.cytosolic += uptake * 0.8f;
    }

    /* MAO degradation */
    float mao_deg = system->concentrations.cytosolic * system->config.mao_rate * dt * 0.001f;
    system->concentrations.cytosolic -= mao_deg;

    /* COMT degradation */
    float comt_deg = system->concentrations.extrasynaptic * system->config.comt_rate * dt * 0.001f;
    system->concentrations.extrasynaptic -= comt_deg;

    /* Update autoreceptor feedback */
    if (system->config.enable_autoreceptors) {
        system->autoreceptor_feedback = hill_equation(
            system->concentrations.synaptic,
            system->config.autoreceptor_ic50,
            1.0f
        );
    }

    /* Update receptor activations */
    for (int i = 0; i < DA_RECEPTOR_TYPE_COUNT; i++) {
        float da = system->concentrations.synaptic;
        float new_activation = hill_equation(
            da,
            system->receptors[i].kd,
            system->receptors[i].hill_coeff
        );
        /* Smooth update */
        float tau = 50.0f;
        system->receptors[i].activation += (new_activation - system->receptors[i].activation) * (dt / tau);
    }

    /* Vesicle pool refilling */
    float refill = system->vesicles.refill_rate * dt * 0.001f;
    if (system->vesicles.reserve > 0 && system->vesicles.readily_releasable < DA_VESICLE_POOL_SIZE / 5) {
        uint32_t to_refill = (uint32_t)refill;
        if (to_refill > system->vesicles.reserve) {
            to_refill = system->vesicles.reserve;
        }
        system->vesicles.readily_releasable += to_refill;
        system->vesicles.reserve -= to_refill;
    }

    /* Clamp concentrations */
    system->concentrations.synaptic = nimcp_clampf(system->concentrations.synaptic, 0.0f, 1000.0f);
    system->concentrations.extrasynaptic = nimcp_clampf(system->concentrations.extrasynaptic, 0.0f, 500.0f);
    system->concentrations.cytosolic = nimcp_clampf(system->concentrations.cytosolic, 0.0f, 500.0f);

    system->spikes_processed += spikes;
    return 0;
}

int nimcp_da_release_trigger(
    nimcp_da_release_system_t* system,
    uint32_t spikes
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_release_trigger: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Calculate release */
    float efficacy = system->release_efficacy;
    if (system->config.enable_autoreceptors) {
        efficacy *= (1.0f - system->autoreceptor_feedback * 0.5f);
    }

    uint32_t vesicles_to_release = (uint32_t)(spikes * system->config.release_probability * efficacy);
    if (vesicles_to_release > system->vesicles.readily_releasable) {
        vesicles_to_release = system->vesicles.readily_releasable;
    }

    if (vesicles_to_release > 0) {
        float da_released = vesicles_to_release * system->config.quanta_per_vesicle * 0.001f;
        system->concentrations.synaptic += da_released;
        system->vesicles.readily_releasable -= vesicles_to_release;
        system->vesicles.recycling += vesicles_to_release;
        system->total_released += da_released;
        system->release_events++;
    }

    /* Update efficacy based on vesicle availability */
    float pool_ratio = (float)system->vesicles.readily_releasable / (DA_VESICLE_POOL_SIZE / 10);
    system->release_efficacy = nimcp_clampf(pool_ratio, 0.1f, 1.0f);

    return 0;
}

int nimcp_da_release_burst(
    nimcp_da_release_system_t* system,
    uint32_t spikes,
    float enhancement
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_release_burst: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Temporarily boost release probability for burst */
    float orig_prob = system->config.release_probability;
    system->config.release_probability *= (1.0f + enhancement);
    if (system->config.release_probability > 0.9f) {
        system->config.release_probability = 0.9f;
    }

    int result = nimcp_da_release_trigger(system, spikes);

    system->config.release_probability = orig_prob;
    return result;
}

/*=============================================================================
 * Concentration API
 *===========================================================================*/

int nimcp_da_get_concentration(
    nimcp_da_release_system_t* system,
    nimcp_da_compartment_t compartment,
    float* concentration
) {
    if (!system || !concentration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_concentration: required parameter is NULL (system, concentration)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_concentration: system->initialized is NULL");
        return -1;
    }

    switch (compartment) {
        case DA_COMPARTMENT_VESICULAR:
            *concentration = system->concentrations.vesicular;
            break;
        case DA_COMPARTMENT_CYTOSOLIC:
            *concentration = system->concentrations.cytosolic;
            break;
        case DA_COMPARTMENT_SYNAPTIC:
            *concentration = system->concentrations.synaptic;
            break;
        case DA_COMPARTMENT_EXTRASYNAPTIC:
            *concentration = system->concentrations.extrasynaptic;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_da_get_concentration: operation failed");
            return -1;
    }

    return 0;
}

int nimcp_da_set_concentration(
    nimcp_da_release_system_t* system,
    nimcp_da_compartment_t compartment,
    float concentration
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_set_concentration: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    concentration = nimcp_clampf(concentration, 0.0f, 1000.0f);

    switch (compartment) {
        case DA_COMPARTMENT_VESICULAR:
            system->concentrations.vesicular = concentration;
            break;
        case DA_COMPARTMENT_CYTOSOLIC:
            system->concentrations.cytosolic = concentration;
            break;
        case DA_COMPARTMENT_SYNAPTIC:
            system->concentrations.synaptic = concentration;
            break;
        case DA_COMPARTMENT_EXTRASYNAPTIC:
            system->concentrations.extrasynaptic = concentration;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_da_set_concentration: operation failed");
            return -1;
    }

    return 0;
}

int nimcp_da_get_total_extracellular(
    nimcp_da_release_system_t* system,
    float* total
) {
    if (!system || !total) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_total_extracellular: required parameter is NULL (system, total)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_total_extracellular: system->initialized is NULL");
        return -1;
    }

    *total = system->concentrations.synaptic + system->concentrations.extrasynaptic;
    return 0;
}

/*=============================================================================
 * Receptor API
 *===========================================================================*/

int nimcp_da_get_receptor_activation(
    nimcp_da_release_system_t* system,
    nimcp_da_receptor_type_t type,
    float* activation
) {
    if (!system || !activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_receptor_activation: required parameter is NULL (system, activation)");
        return -1;
    }

    if (!system->initialized || type >= DA_RECEPTOR_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_receptor_activation: system->initialized is NULL");
        return -1;
    }

    *activation = system->receptors[type].activation;
    return 0;
}

int nimcp_da_get_d1_d2_balance(
    nimcp_da_release_system_t* system,
    float* balance
) {
    if (!system || !balance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_d1_d2_balance: required parameter is NULL (system, balance)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_d1_d2_balance: system->initialized is NULL");
        return -1;
    }

    float d1 = system->receptors[DA_RECEPTOR_TYPE_D1].activation;
    float d2 = system->receptors[DA_RECEPTOR_TYPE_D2].activation;

    if (d2 > 0.001f) {
        *balance = d1 / d2;
    } else {
        *balance = d1 > 0.0f ? 10.0f : 1.0f;
    }

    return 0;
}

int nimcp_da_apply_drug(
    nimcp_da_release_system_t* system,
    nimcp_da_receptor_type_t receptor,
    float efficacy,
    float affinity
) {
    if (!system || !system->initialized || receptor >= DA_RECEPTOR_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_apply_drug: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Modify receptor Kd based on drug affinity */
    system->receptors[receptor].kd *= (1.0f - affinity);

    /* Modify activation based on efficacy */
    system->receptors[receptor].activation += efficacy * 0.5f;
    system->receptors[receptor].activation = nimcp_clampf(
        system->receptors[receptor].activation, 0.0f, 1.0f
    );

    return 0;
}

/*=============================================================================
 * Transporter API
 *===========================================================================*/

int nimcp_da_set_dat_inhibition(
    nimcp_da_release_system_t* system,
    float inhibition
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_set_dat_inhibition: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->transporter.inhibition = nimcp_clampf(inhibition, 0.0f, 1.0f);
    return 0;
}

int nimcp_da_get_uptake_rate(
    nimcp_da_release_system_t* system,
    float* rate
) {
    if (!system || !rate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_uptake_rate: required parameter is NULL (system, rate)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_uptake_rate: system->initialized is NULL");
        return -1;
    }

    float effective_vmax = system->transporter.vmax *
                           (1.0f - system->transporter.inhibition) *
                           system->transporter.expression;
    *rate = michaelis_menten(
        system->concentrations.synaptic,
        effective_vmax,
        system->transporter.km
    );

    return 0;
}

/*=============================================================================
 * Autoreceptor API
 *===========================================================================*/

int nimcp_da_get_autoreceptor_feedback(
    nimcp_da_release_system_t* system,
    float* feedback
) {
    if (!system || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_autoreceptor_feedback: required parameter is NULL (system, feedback)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_autoreceptor_feedback: system->initialized is NULL");
        return -1;
    }

    *feedback = system->autoreceptor_feedback;
    return 0;
}

/*=============================================================================
 * Vesicle API
 *===========================================================================*/

int nimcp_da_get_vesicle_pool(
    nimcp_da_release_system_t* system,
    nimcp_da_vesicle_pool_t* pool
) {
    if (!system || !pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_vesicle_pool: required parameter is NULL (system, pool)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_vesicle_pool: system->initialized is NULL");
        return -1;
    }

    *pool = system->vesicles;
    return 0;
}

int nimcp_da_get_release_efficacy(
    nimcp_da_release_system_t* system,
    float* efficacy
) {
    if (!system || !efficacy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_release_efficacy: required parameter is NULL (system, efficacy)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_da_get_release_efficacy: system->initialized is NULL");
        return -1;
    }

    *efficacy = system->release_efficacy;
    return 0;
}

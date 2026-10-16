/**
 * @file nimcp_norepinephrine_release.c
 * @brief Norepinephrine Release Dynamics Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "core/brain/regions/locus_coeruleus/nimcp_norepinephrine_release.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(norepinephrine_release, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float michaelis_menten(float substrate, float km, float vmax) {
    if (substrate <= 0.0f) return 0.0f;
    return (vmax * substrate) / (km + substrate);
}

static float hill_equation(float ligand, float ec50, float hill_coeff) {
    if (ligand <= 0.0f || ec50 <= 0.0f) return 0.0f;
    float ratio = powf(ligand / ec50, hill_coeff);
    return ratio / (1.0f + ratio);
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

nimcp_ne_release_config_t nimcp_ne_release_default_config(void) {
    nimcp_ne_release_config_t config;
    memset(&config, 0, sizeof(config));

    /* Vesicle parameters */
    config.initial_vesicles = NE_VESICLE_POOL_SIZE;
    config.ready_pool_fraction = 0.2f;
    config.refill_rate = 0.01f;

    /* Release parameters */
    config.release_probability = 0.3f;
    config.ne_per_vesicle = NE_PER_VESICLE;

    /* Clearance parameters */
    config.net_km = NE_NET_KM;
    config.net_vmax = NE_NET_VMAX;
    config.mao_rate = NE_MAO_RATE;
    config.comt_rate = 0.0005f;

    /* Receptor parameters */
    config.alpha1_density = 1.0f;
    config.alpha2_density = 1.0f;
    config.beta1_density = 0.5f;
    config.beta2_density = 0.3f;

    /* Autoreceptor parameters */
    config.autoreceptor_gain = 0.5f;
    config.autoreceptor_ec50 = NE_ALPHA2_EC50;

    /* Synthesis */
    config.basal_synthesis_rate = 0.001f;

    return config;
}

int nimcp_ne_release_init(
    nimcp_ne_release_system_t* system,
    const nimcp_ne_release_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(nimcp_ne_release_system_t));

    nimcp_ne_release_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = nimcp_ne_release_default_config();
    }

    /* Initialize concentrations */
    system->concentrations.vesicular = 1000.0f;  /* High in vesicles */
    system->concentrations.cytosolic = 10.0f;
    system->concentrations.synaptic = 0.5f;
    system->concentrations.extrasynaptic = 0.5f;

    /* Initialize vesicle pool */
    system->vesicles.total_vesicles = cfg.initial_vesicles;
    system->vesicles.ready_pool = (uint32_t)(cfg.initial_vesicles * cfg.ready_pool_fraction);
    system->vesicles.reserve_pool = cfg.initial_vesicles - system->vesicles.ready_pool;
    system->vesicles.depleted = 0;
    system->vesicles.refill_rate = cfg.refill_rate;
    system->vesicles.release_probability = cfg.release_probability;

    /* Initialize receptors */
    /* Alpha-1 */
    system->receptors[NE_RECEPTOR_ALPHA1].type = NE_RECEPTOR_ALPHA1;
    system->receptors[NE_RECEPTOR_ALPHA1].density = cfg.alpha1_density;
    system->receptors[NE_RECEPTOR_ALPHA1].affinity_nm = 100.0f;
    system->receptors[NE_RECEPTOR_ALPHA1].occupancy = 0.0f;
    system->receptors[NE_RECEPTOR_ALPHA1].response = 0.0f;
    system->receptors[NE_RECEPTOR_ALPHA1].desensitization = 0.0f;

    /* Alpha-2 */
    system->receptors[NE_RECEPTOR_ALPHA2].type = NE_RECEPTOR_ALPHA2;
    system->receptors[NE_RECEPTOR_ALPHA2].density = cfg.alpha2_density;
    system->receptors[NE_RECEPTOR_ALPHA2].affinity_nm = cfg.autoreceptor_ec50;
    system->receptors[NE_RECEPTOR_ALPHA2].occupancy = 0.0f;
    system->receptors[NE_RECEPTOR_ALPHA2].response = 0.0f;
    system->receptors[NE_RECEPTOR_ALPHA2].desensitization = 0.0f;

    /* Beta-1 */
    system->receptors[NE_RECEPTOR_BETA1].type = NE_RECEPTOR_BETA1;
    system->receptors[NE_RECEPTOR_BETA1].density = cfg.beta1_density;
    system->receptors[NE_RECEPTOR_BETA1].affinity_nm = 500.0f;
    system->receptors[NE_RECEPTOR_BETA1].occupancy = 0.0f;
    system->receptors[NE_RECEPTOR_BETA1].response = 0.0f;
    system->receptors[NE_RECEPTOR_BETA1].desensitization = 0.0f;

    /* Beta-2 */
    system->receptors[NE_RECEPTOR_BETA2].type = NE_RECEPTOR_BETA2;
    system->receptors[NE_RECEPTOR_BETA2].density = cfg.beta2_density;
    system->receptors[NE_RECEPTOR_BETA2].affinity_nm = 300.0f;
    system->receptors[NE_RECEPTOR_BETA2].occupancy = 0.0f;
    system->receptors[NE_RECEPTOR_BETA2].response = 0.0f;
    system->receptors[NE_RECEPTOR_BETA2].desensitization = 0.0f;

    /* Initialize transporter */
    system->transporter.km = cfg.net_km;
    system->transporter.vmax = cfg.net_vmax;
    system->transporter.current_rate = 0.0f;
    system->transporter.inhibition = 0.0f;
    system->transporter.enabled = true;

    /* Initialize degradation */
    system->mao_activity = cfg.mao_rate;
    system->comt_activity = cfg.comt_rate;

    /* Initialize autoreceptor */
    system->autoreceptor_activation = 0.0f;
    system->release_inhibition = 0.0f;

    /* Initialize metabolism */
    system->synthesis_rate = cfg.basal_synthesis_rate;
    system->degradation_rate = 0.0f;
    system->total_released = 0.0f;
    system->total_cleared = 0.0f;

    system->initialized = true;
    system->current_time = 0.0f;

    return 0;
}

int nimcp_ne_release_shutdown(nimcp_ne_release_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(nimcp_ne_release_system_t));
    return 0;
}

int nimcp_ne_release_reset(nimcp_ne_release_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_release_reset: system->initialized is NULL");
        return -1;
    }

    /* Reset to initial state */
    system->concentrations.vesicular = 1000.0f;
    system->concentrations.cytosolic = 10.0f;
    system->concentrations.synaptic = 0.5f;
    system->concentrations.extrasynaptic = 0.5f;

    system->vesicles.ready_pool = (uint32_t)(system->vesicles.total_vesicles * 0.2f);
    system->vesicles.reserve_pool = system->vesicles.total_vesicles - system->vesicles.ready_pool;
    system->vesicles.depleted = 0;

    for (int i = 0; i < NE_RECEPTOR_COUNT; i++) {
        system->receptors[i].occupancy = 0.0f;
        system->receptors[i].response = 0.0f;
        system->receptors[i].desensitization = 0.0f;
    }

    system->transporter.current_rate = 0.0f;
    system->transporter.inhibition = 0.0f;

    system->autoreceptor_activation = 0.0f;
    system->release_inhibition = 0.0f;

    system->total_released = 0.0f;
    system->total_cleared = 0.0f;
    system->current_time = 0.0f;

    return 0;
}

//=============================================================================
// Operations Implementation
//=============================================================================

int nimcp_ne_release_update(
    nimcp_ne_release_system_t* system,
    float dt,
    float firing_rate
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_release_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ne_release_update: validation failed");
        return -1;
    }

    /* Compute release based on firing rate */
    float spikes_per_update = firing_rate * dt / 1000.0f;
    uint32_t spikes = (uint32_t)spikes_per_update;
    if (spikes > 0) {
        nimcp_ne_release_trigger(system, spikes);
    }

    /* Diffusion from synaptic to extrasynaptic */
    float diffusion_rate = NE_DIFFUSION_COEFF * dt;
    float diff = (system->concentrations.synaptic - system->concentrations.extrasynaptic) *
                  diffusion_rate * 0.01f;
    system->concentrations.synaptic -= diff;
    system->concentrations.extrasynaptic += diff;

    /* Transporter-mediated uptake (synaptic -> cytosol) */
    if (system->transporter.enabled) {
        float effective_vmax = system->transporter.vmax * (1.0f - system->transporter.inhibition);
        float uptake_rate = michaelis_menten(system->concentrations.synaptic,
                                              system->transporter.km, effective_vmax);
        float uptake = uptake_rate * dt;
        uptake = nimcp_clampf(uptake, 0.0f, system->concentrations.synaptic);

        system->concentrations.synaptic -= uptake;
        system->concentrations.cytosolic += uptake;
        system->transporter.current_rate = uptake_rate;
        system->total_cleared += uptake;
    }

    /* MAO degradation (cytosolic) */
    float mao_degradation = system->concentrations.cytosolic * system->mao_activity * dt;
    system->concentrations.cytosolic -= mao_degradation;

    /* COMT degradation (extrasynaptic) */
    float comt_degradation = system->concentrations.extrasynaptic * system->comt_activity * dt;
    system->concentrations.extrasynaptic -= comt_degradation;

    system->degradation_rate = mao_degradation + comt_degradation;

    /* Vesicle refilling */
    if (system->vesicles.depleted > 0) {
        float refill = system->vesicles.refill_rate * dt * system->vesicles.depleted;
        uint32_t refilled = (uint32_t)refill;
        if (refilled > system->vesicles.depleted) {
            refilled = system->vesicles.depleted;
        }
        system->vesicles.depleted -= refilled;
        system->vesicles.reserve_pool += refilled;
    }

    /* Move vesicles from reserve to ready pool */
    float mobilization = 0.01f * dt * system->vesicles.reserve_pool;
    uint32_t mobilized = (uint32_t)mobilization;
    if (mobilized > system->vesicles.reserve_pool) {
        mobilized = system->vesicles.reserve_pool;
    }
    system->vesicles.reserve_pool -= mobilized;
    system->vesicles.ready_pool += mobilized;

    /* Synthesis (cytosol -> vesicular via VMAT) */
    float synthesis = system->synthesis_rate * dt;
    system->concentrations.cytosolic += synthesis;

    /* Update receptor states */
    float ne_conc = system->concentrations.synaptic + system->concentrations.extrasynaptic * 0.5f;

    for (int i = 0; i < NE_RECEPTOR_COUNT; i++) {
        nimcp_ne_receptor_state_t* rec = &system->receptors[i];

        /* Compute occupancy using Hill equation */
        rec->occupancy = hill_equation(ne_conc * rec->density, rec->affinity_nm, 1.0f);

        /* Compute response with desensitization */
        float effective_occupancy = rec->occupancy * (1.0f - rec->desensitization);
        float response_tau = 50.0f;  /* Response time constant */
        float alpha = 1.0f - expf(-dt / response_tau);
        rec->response += alpha * (effective_occupancy - rec->response);

        /* Update desensitization (increases with sustained activation) */
        float desens_rate = 0.0001f * rec->response;
        float desens_recovery = 0.001f * (1.0f - rec->response);
        rec->desensitization += dt * (desens_rate - desens_recovery);
        rec->desensitization = nimcp_clampf(rec->desensitization, 0.0f, 0.8f);
    }

    /* Update autoreceptor feedback (alpha-2) */
    system->autoreceptor_activation = system->receptors[NE_RECEPTOR_ALPHA2].response;
    system->release_inhibition = system->autoreceptor_activation * 0.5f;

    /* Clamp concentrations */
    system->concentrations.vesicular = nimcp_clampf(system->concentrations.vesicular, 0.0f, 10000.0f);
    system->concentrations.cytosolic = nimcp_clampf(system->concentrations.cytosolic, 0.0f, 1000.0f);
    system->concentrations.synaptic = nimcp_clampf(system->concentrations.synaptic, 0.0f, 1000.0f);
    system->concentrations.extrasynaptic = nimcp_clampf(system->concentrations.extrasynaptic, 0.0f, 500.0f);

    system->current_time += dt;
    return 0;
}

int nimcp_ne_release_trigger(nimcp_ne_release_system_t* system, uint32_t num_spikes) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_release_trigger: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (num_spikes == 0) {
        return 0;
    }

    /* Calculate release with autoreceptor inhibition */
    float effective_release_prob = system->vesicles.release_probability *
                                    (1.0f - system->release_inhibition);

    for (uint32_t i = 0; i < num_spikes && system->vesicles.ready_pool > 0; i++) {
        /* Probabilistic release */
        float rand_val = (float)nimcp_tl_rand() / RAND_MAX;
        if (rand_val < effective_release_prob) {
            /* Release one vesicle */
            system->vesicles.ready_pool--;
            system->vesicles.depleted++;

            /* Add NE to synaptic cleft */
            float released_ne = system->concentrations.vesicular * 0.001f;  /* Fraction of vesicular content */
            system->concentrations.synaptic += released_ne;
            system->total_released += released_ne;
        }
    }

    return 0;
}

int nimcp_ne_get_receptor_activation(
    const nimcp_ne_release_system_t* system,
    nimcp_ne_receptor_t receptor,
    float* activation
) {
    if (!system || !activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_get_receptor_activation: required parameter is NULL (system, activation)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_get_receptor_activation: system->initialized is NULL");
        return -1;
    }

    if (receptor >= NE_RECEPTOR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "nimcp_ne_get_receptor_activation: capacity exceeded");
        return -1;
    }

    *activation = system->receptors[receptor].response;
    return 0;
}

int nimcp_ne_get_concentration(
    const nimcp_ne_release_system_t* system,
    nimcp_ne_compartment_t compartment,
    float* concentration
) {
    if (!system || !concentration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_get_concentration: required parameter is NULL (system, concentration)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_get_concentration: system->initialized is NULL");
        return -1;
    }

    switch (compartment) {
        case NE_COMPARTMENT_VESICLE:
            *concentration = system->concentrations.vesicular;
            break;
        case NE_COMPARTMENT_CYTOSOL:
            *concentration = system->concentrations.cytosolic;
            break;
        case NE_COMPARTMENT_SYNAPTIC:
            *concentration = system->concentrations.synaptic;
            break;
        case NE_COMPARTMENT_EXTRASYNAPTIC:
            *concentration = system->concentrations.extrasynaptic;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ne_get_concentration: operation failed");
            return -1;
    }

    return 0;
}

int nimcp_ne_apply_net_inhibition(nimcp_ne_release_system_t* system, float inhibition) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_apply_net_inhibition: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->transporter.inhibition = nimcp_clampf(inhibition, 0.0f, 1.0f);
    return 0;
}

int nimcp_ne_get_autoreceptor_feedback(
    const nimcp_ne_release_system_t* system,
    float* feedback
) {
    if (!system || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_get_autoreceptor_feedback: required parameter is NULL (system, feedback)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ne_get_autoreceptor_feedback: system->initialized is NULL");
        return -1;
    }

    *feedback = system->release_inhibition;
    return 0;
}

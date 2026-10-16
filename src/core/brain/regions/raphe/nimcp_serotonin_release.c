/**
 * @file nimcp_serotonin_release.c
 * @brief 5-HT (Serotonin) release dynamics implementation
 * @date 2026-01-11
 */

#include "core/brain/regions/raphe/nimcp_serotonin_release.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(serotonin_release, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float michaelis_menten(float concentration, float vmax, float km) {
    return (vmax * concentration) / (km + concentration);
}

static float hill_equation(float concentration, float kd, float n) {
    float cn = powf(concentration, n);
    float kdn = powf(kd, n);
    return cn / (kdn + cn);
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_ht_release_config_t nimcp_ht_release_default_config(void) {
    nimcp_ht_release_config_t config = {
        .release_probability = HT_DEFAULT_RELEASE_PROB,
        .sert_vmax = HT_DEFAULT_SERT_VMAX,
        .sert_km = HT_DEFAULT_SERT_KM,
        .mao_rate = HT_DEFAULT_MAO_RATE,
        .autoreceptor_gain = 0.5f,
        .synthesis_rate = 0.5f,
        .enable_volume_transmission = true
    };
    return config;
}

int nimcp_ht_release_init(nimcp_ht_release_system_t* system,
                          const nimcp_ht_release_config_t* config) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    memset(system, 0, sizeof(nimcp_ht_release_system_t));

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_ht_release_default_config();
    }

    /* Initialize concentrations */
    system->concentrations.vesicular = 1000.0f;  /* Large vesicular store */
    system->concentrations.cytosolic = 100.0f;
    system->concentrations.synaptic = HT_DEFAULT_BASELINE;
    system->concentrations.extrasynaptic = HT_DEFAULT_BASELINE * 0.5f;

    /* Initialize transporter */
    system->transporter.vmax = system->config.sert_vmax;
    system->transporter.km = system->config.sert_km;
    system->transporter.inhibition = 0.0f;
    system->transporter.activity = 1.0f;

    /* Initialize autoreceptors */
    system->autoreceptor.activation_1a = 0.0f;
    system->autoreceptor.activation_1b = 0.0f;
    system->autoreceptor.feedback_strength = 0.0f;
    system->autoreceptor.desensitization = 0.0f;

    /* Initialize vesicle pools */
    system->vesicles.readily_releasable = 100;
    system->vesicles.reserve = 500;
    system->vesicles.recycling = 0;
    system->vesicles.mobilization_rate = 0.01f;

    /* Initialize receptor activations */
    for (int i = 0; i < HT_RECEPTOR_TYPE_COUNT; i++) {
        system->receptor_activation[i] = 0.0f;
    }

    /* Initialize dynamics */
    system->release_efficacy = 1.0f;
    system->synthesis_rate = system->config.synthesis_rate;

    system->initialized = true;
    return 0;
}

int nimcp_ht_release_shutdown(nimcp_ht_release_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    system->initialized = false;
    return 0;
}

int nimcp_ht_release_reset(nimcp_ht_release_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_ht_release_config_t config = system->config;
    return nimcp_ht_release_init(system, &config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_ht_release_update(nimcp_ht_release_system_t* system,
                            float firing_rate, float dt) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ht_release_reset: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    float dt_sec = dt / 1000.0f;

    /* 1. Compute autoreceptor feedback (inhibits release) */
    float extracellular = system->concentrations.synaptic +
                          system->concentrations.extrasynaptic;

    system->autoreceptor.activation_1a = hill_equation(
        extracellular, HT_AUTORECEPTOR_KD, 1.0f);
    system->autoreceptor.activation_1b = hill_equation(
        extracellular, HT_AUTORECEPTOR_KD * 2.0f, 1.0f);

    system->autoreceptor.feedback_strength =
        system->config.autoreceptor_gain *
        (system->autoreceptor.activation_1a + system->autoreceptor.activation_1b) / 2.0f;

    /* 2. Compute release efficacy (reduced by autoreceptor feedback) */
    system->release_efficacy = 1.0f - system->autoreceptor.feedback_strength;
    system->release_efficacy = nimcp_clampf(system->release_efficacy, 0.1f, 1.0f);

    /* 3. Compute release (slower than DA/NE) */
    float effective_release_prob = system->config.release_probability *
                                   system->release_efficacy;

    /* 5-HT neurons have lower baseline firing (~1-5 Hz) */
    float release_events = firing_rate * effective_release_prob * dt_sec;
    float ht_per_event = 2.0f;  /* nM per release event */

    float release_amount = release_events * ht_per_event;
    release_amount = nimcp_clampf(release_amount, 0.0f,
                            system->concentrations.vesicular * 0.01f);

    /* Transfer from vesicular to synaptic */
    system->concentrations.vesicular -= release_amount;
    system->concentrations.synaptic += release_amount;

    /* 4. Volume transmission (extrasynaptic diffusion) */
    if (system->config.enable_volume_transmission) {
        float diffusion = (system->concentrations.synaptic -
                          system->concentrations.extrasynaptic) * 0.1f * dt_sec;
        system->concentrations.synaptic -= diffusion;
        system->concentrations.extrasynaptic += diffusion;
    }

    /* 5. SERT reuptake (Michaelis-Menten kinetics) */
    float effective_vmax = system->transporter.vmax *
                          (1.0f - system->transporter.inhibition);

    float synaptic_uptake = michaelis_menten(
        system->concentrations.synaptic,
        effective_vmax,
        system->transporter.km
    ) * dt_sec;

    float extrasynaptic_uptake = michaelis_menten(
        system->concentrations.extrasynaptic,
        effective_vmax * 0.5f,  /* Slower extrasynaptic */
        system->transporter.km
    ) * dt_sec;

    system->concentrations.synaptic -= synaptic_uptake;
    system->concentrations.extrasynaptic -= extrasynaptic_uptake;
    system->concentrations.cytosolic += synaptic_uptake + extrasynaptic_uptake;

    system->transporter.activity = (synaptic_uptake + extrasynaptic_uptake) /
                                   (effective_vmax * dt_sec + 0.001f);

    /* 6. MAO degradation */
    float mao_degradation = system->concentrations.cytosolic *
                           system->config.mao_rate * dt_sec;
    system->concentrations.cytosolic -= mao_degradation;

    /* 7. Synthesis (tryptophan -> 5-HT -> vesicles via VMAT2) */
    float synthesis = system->synthesis_rate * dt_sec;
    system->concentrations.cytosolic += synthesis;

    /* VMAT2 packaging */
    float packaging = system->concentrations.cytosolic * 0.05f * dt_sec;
    system->concentrations.cytosolic -= packaging;
    system->concentrations.vesicular += packaging;

    /* 8. Update receptor activations */
    /* 5-HT1A: anxiolytic, inhibitory (somatodendritic autoreceptor) */
    system->receptor_activation[HT_RECEPTOR_TYPE_1A] = hill_equation(
        extracellular, 5.0f, 1.0f);

    /* 5-HT1B: terminal autoreceptor */
    system->receptor_activation[HT_RECEPTOR_TYPE_1B] = hill_equation(
        extracellular, 10.0f, 1.0f);

    /* 5-HT2A: mood, hallucinations at high activation */
    system->receptor_activation[HT_RECEPTOR_TYPE_2A] = hill_equation(
        extracellular, 20.0f, 1.5f);

    /* 5-HT2C: appetite suppression, anxiety */
    system->receptor_activation[HT_RECEPTOR_TYPE_2C] = hill_equation(
        extracellular, 15.0f, 1.0f);

    /* 9. Vesicle pool dynamics */
    /* Mobilization: reserve -> readily releasable */
    uint32_t mobilized = (uint32_t)(system->vesicles.reserve *
                                    system->vesicles.mobilization_rate * dt_sec);
    if (mobilized > system->vesicles.reserve) {
        mobilized = system->vesicles.reserve;
    }
    system->vesicles.reserve -= mobilized;
    system->vesicles.readily_releasable += mobilized;

    /* Recycling -> reserve */
    uint32_t recycled = (uint32_t)(system->vesicles.recycling * 0.1f * dt_sec);
    if (recycled > system->vesicles.recycling) {
        recycled = system->vesicles.recycling;
    }
    system->vesicles.recycling -= recycled;
    system->vesicles.reserve += recycled;

    /* Clamp concentrations */
    system->concentrations.synaptic = nimcp_clampf(
        system->concentrations.synaptic, 0.0f, HT_MAX_CONCENTRATION);
    system->concentrations.extrasynaptic = nimcp_clampf(
        system->concentrations.extrasynaptic, 0.0f, HT_MAX_CONCENTRATION);
    system->concentrations.cytosolic = nimcp_clampf(
        system->concentrations.cytosolic, 0.0f, 500.0f);
    system->concentrations.vesicular = nimcp_clampf(
        system->concentrations.vesicular, 0.0f, 5000.0f);

    return 0;
}

/*=============================================================================
 * Concentration API
 *===========================================================================*/

int nimcp_ht_get_concentration(nimcp_ht_release_system_t* system,
                               nimcp_ht_compartment_t compartment,
                               float* concentration) {
    if (!system || !system->initialized || !concentration) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ht_release_reset: required parameter is NULL (system, system->initialized, concentration)");
        return -1;
    }

    switch (compartment) {
        case HT_COMPARTMENT_VESICULAR:
            *concentration = system->concentrations.vesicular;
            break;
        case HT_COMPARTMENT_CYTOSOLIC:
            *concentration = system->concentrations.cytosolic;
            break;
        case HT_COMPARTMENT_SYNAPTIC:
            *concentration = system->concentrations.synaptic;
            break;
        case HT_COMPARTMENT_EXTRASYNAPTIC:
            *concentration = system->concentrations.extrasynaptic;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ht_release_reset: operation failed");
            return -1;
    }
    return 0;
}

int nimcp_ht_set_concentration(nimcp_ht_release_system_t* system,
                               nimcp_ht_compartment_t compartment,
                               float concentration) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ht_release_reset: required parameter is NULL (system, system->initialized)");
        return -1;
    }
    if (concentration < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ht_release_reset: validation failed");
        return -1;
    }

    switch (compartment) {
        case HT_COMPARTMENT_VESICULAR:
            system->concentrations.vesicular = concentration;
            break;
        case HT_COMPARTMENT_CYTOSOLIC:
            system->concentrations.cytosolic = concentration;
            break;
        case HT_COMPARTMENT_SYNAPTIC:
            system->concentrations.synaptic = concentration;
            break;
        case HT_COMPARTMENT_EXTRASYNAPTIC:
            system->concentrations.extrasynaptic = concentration;
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_ht_release_reset: operation failed");
            return -1;
    }
    return 0;
}

int nimcp_ht_get_total_extracellular(nimcp_ht_release_system_t* system,
                                     float* total) {
    if (!system || !system->initialized || !total) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, system->initialized, total)");
        return -1;
    }

    *total = system->concentrations.synaptic +
             system->concentrations.extrasynaptic;
    return 0;
}

/*=============================================================================
 * Receptor API
 *===========================================================================*/

int nimcp_ht_get_receptor_activation(nimcp_ht_release_system_t* system,
                                     nimcp_ht_receptor_type_t receptor,
                                     float* activation) {
    if (!system || !system->initialized || !activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, system->initialized, activation)");
        return -1;
    }
    if (receptor >= HT_RECEPTOR_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return -1;
    }

    *activation = system->receptor_activation[receptor];
    return 0;
}

int nimcp_ht_get_1a_2a_balance(nimcp_ht_release_system_t* system,
                               float* balance) {
    if (!system || !system->initialized || !balance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, system->initialized, balance)");
        return -1;
    }

    /* Balance between inhibitory 5-HT1A and excitatory 5-HT2A */
    /* Negative = 1A dominant (calming), Positive = 2A dominant (activating) */
    *balance = system->receptor_activation[HT_RECEPTOR_TYPE_2A] -
               system->receptor_activation[HT_RECEPTOR_TYPE_1A];
    return 0;
}

/*=============================================================================
 * Transporter API
 *===========================================================================*/

int nimcp_ht_set_sert_inhibition(nimcp_ht_release_system_t* system,
                                 float inhibition) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->transporter.inhibition = nimcp_clampf(inhibition, 0.0f, 1.0f);
    return 0;
}

int nimcp_ht_get_uptake_rate(nimcp_ht_release_system_t* system, float* rate) {
    if (!system || !system->initialized || !rate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ht_get_uptake_rate: required parameter is NULL (system, system->initialized, rate)");
        return -1;
    }

    float effective_vmax = system->transporter.vmax *
                          (1.0f - system->transporter.inhibition);
    *rate = michaelis_menten(system->concentrations.synaptic,
                            effective_vmax, system->transporter.km);
    return 0;
}

/*=============================================================================
 * Autoreceptor API
 *===========================================================================*/

int nimcp_ht_get_autoreceptor_feedback(nimcp_ht_release_system_t* system,
                                       float* feedback) {
    if (!system || !system->initialized || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ht_get_uptake_rate: required parameter is NULL (system, system->initialized, feedback)");
        return -1;
    }

    *feedback = system->autoreceptor.feedback_strength;
    return 0;
}

/*=============================================================================
 * Vesicle API
 *===========================================================================*/

int nimcp_ht_get_vesicle_pool(nimcp_ht_release_system_t* system,
                              nimcp_ht_vesicle_pool_t* pool) {
    if (!system || !system->initialized || !pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ht_get_uptake_rate: required parameter is NULL (system, system->initialized, pool)");
        return -1;
    }

    *pool = system->vesicles;
    return 0;
}

int nimcp_ht_get_release_efficacy(nimcp_ht_release_system_t* system,
                                  float* efficacy) {
    if (!system || !system->initialized || !efficacy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ht_get_uptake_rate: required parameter is NULL (system, system->initialized, efficacy)");
        return -1;
    }

    *efficacy = system->release_efficacy;
    return 0;
}

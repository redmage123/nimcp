/**
 * @file nimcp_proton_pumps.c
 * @brief Proton Pump Systems Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "chemistry/ph/nimcp_proton_pumps.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(proton_pumps)

//=============================================================================
// Internal Constants
//=============================================================================

/** Minimum activity threshold */
#define MIN_ACTIVITY_THRESHOLD 0.001f

/** ATP depletion threshold */
#define ATP_DEPLETION_THRESHOLD 0.1f

/** Default pump rate scaling */
#define PUMP_RATE_SCALE 0.001f

/**
 * @brief Michaelis-Menten kinetics
 */
static float michaelis_menten(float substrate, float km, float vmax) {
    if (substrate <= 0.0f) return 0.0f;
    return (vmax * substrate) / (km + substrate);
}

/**
 * @brief Initialize V-ATPase with defaults
 */
static void init_vatpase_defaults(nimcp_vatpase_t* vatpase) {
    memset(vatpase, 0, sizeof(nimcp_vatpase_t));

    vatpase->max_rate = PUMP_VATPASE_MAX_RATE;
    vatpase->current_rate = 0.0f;
    vatpase->km_atp = 0.5f;     /* mM */
    vatpase->km_h = 0.0001f;    /* M, ~pH 4 inhibition */

    vatpase->coupling_efficiency = 3.0f;  /* 3 H+ per ATP */
    vatpase->atp_hydrolysis_rate = 0.0f;

    vatpase->v0_v1_assembly = 1.0f;
    vatpase->glucose_sensitivity = 0.5f;
    vatpase->ph_sensitivity = 0.8f;

    vatpase->state = PUMP_STATE_BASAL;
    vatpase->activity_level = 1.0f;

    vatpase->total_h_transported = 0;
    vatpase->total_atp_consumed = 0.0f;
}

/**
 * @brief Initialize NHE with defaults
 */
static void init_nhe_defaults(nimcp_nhe_t* nhe) {
    memset(nhe, 0, sizeof(nimcp_nhe_t));

    nhe->isoform = NHE_ISOFORM_NHE1;

    nhe->max_rate = PUMP_NHE_MAX_RATE;
    nhe->current_rate = 0.0f;
    nhe->km_h_in = 0.0001f;    /* M, ~pH 4 for half-max */
    nhe->km_na_out = 10.0f;    /* mM */

    nhe->h_modifier_site = 0.5f;
    nhe->set_point = 7.2f;     /* Target intracellular pH */

    nhe->phosphorylation_state = 0.5f;
    nhe->calmodulin_binding = 0.5f;

    nhe->state = PUMP_STATE_BASAL;
    nhe->activity_level = 1.0f;

    nhe->total_exchanges = 0;
}

/**
 * @brief Initialize NBC with defaults
 */
static void init_nbc_defaults(nimcp_nbc_t* nbc) {
    memset(nbc, 0, sizeof(nimcp_nbc_t));

    nbc->max_rate = PUMP_NBC_MAX_RATE;
    nbc->current_rate = 0.0f;
    nbc->stoichiometry = 2.0f;  /* 1 Na+ : 2 HCO3- */

    nbc->na_gradient = 140.0f;  /* mM extracellular Na+ */
    nbc->hco3_gradient = 24.0f; /* mM HCO3- */

    nbc->camp_sensitivity = 0.5f;
    nbc->carbonic_anhydrase_coupling = 0.8f;

    nbc->state = PUMP_STATE_BASAL;
    nbc->activity_level = 1.0f;

    nbc->total_transported = 0;
}

/**
 * @brief Initialize default configuration
 */
static void init_default_config(nimcp_pump_config_t* config) {
    config->vatpase_density = 100.0f;
    config->vatpase_max_rate = PUMP_VATPASE_MAX_RATE;

    config->nhe_isoform = NHE_ISOFORM_NHE1;
    config->nhe_density = 500.0f;
    config->nhe_set_point = 7.2f;

    config->nbc_density = 200.0f;
    config->nbc_stoichiometry = 2.0f;

    config->atp_concentration = 5.0f;  /* mM */
    config->atp_regeneration_rate = 0.1f;

    config->na_gradient = 140.0f;
    config->cl_concentration = 100.0f;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

nimcp_pump_error_t nimcp_pump_init(
    nimcp_pump_system_t* system,
    const nimcp_pump_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Proton pump system is NULL");
        return PUMP_ERR_NULL_PTR;
    }

    memset(system, 0, sizeof(nimcp_pump_system_t));

    /* Initialize individual pumps */
    init_vatpase_defaults(&system->vatpase);
    init_nhe_defaults(&system->nhe);
    init_nbc_defaults(&system->nbc);

    /* Apply configuration */
    if (config) {
        memcpy(&system->config, config, sizeof(nimcp_pump_config_t));

        system->vatpase.max_rate = config->vatpase_max_rate;
        system->nhe.isoform = config->nhe_isoform;
        system->nhe.set_point = config->nhe_set_point;
        system->nbc.stoichiometry = config->nbc_stoichiometry;
        system->atp_available = config->atp_concentration;
    } else {
        init_default_config(&system->config);
        system->atp_available = system->config.atp_concentration;
    }

    system->initialized = true;
    system->update_count = 0;

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_pump_shutdown(nimcp_pump_system_t* system) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Proton pump system is NULL in shutdown");
        return PUMP_ERR_NULL_PTR;
    }

    memset(system, 0, sizeof(nimcp_pump_system_t));

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_pump_reset(nimcp_pump_system_t* system) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Proton pump system is NULL in reset");
        return PUMP_ERR_NULL_PTR;
    }

    /* Reset pump states */
    system->vatpase.current_rate = 0.0f;
    system->vatpase.atp_hydrolysis_rate = 0.0f;
    system->vatpase.state = PUMP_STATE_BASAL;
    system->vatpase.activity_level = 1.0f;
    system->vatpase.total_h_transported = 0;
    system->vatpase.total_atp_consumed = 0.0f;

    system->nhe.current_rate = 0.0f;
    system->nhe.state = PUMP_STATE_BASAL;
    system->nhe.activity_level = 1.0f;
    system->nhe.total_exchanges = 0;

    system->nbc.current_rate = 0.0f;
    system->nbc.state = PUMP_STATE_BASAL;
    system->nbc.activity_level = 1.0f;
    system->nbc.total_transported = 0;

    /* Reset resource tracking */
    system->atp_available = system->config.atp_concentration;
    system->atp_consumed = 0.0f;
    system->net_h_flux = 0.0f;
    system->net_hco3_flux = 0.0f;

    system->update_count = 0;

    return PUMP_OK;
}

//=============================================================================
// V-ATPase API Implementation
//=============================================================================

nimcp_pump_error_t nimcp_vatpase_set_activity(
    nimcp_vatpase_t* vatpase,
    float activity
) {
    if (!vatpase) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "V-ATPase is NULL in set_activity");
        return PUMP_ERR_NULL_PTR;
    }

    vatpase->activity_level = nimcp_clampf(activity, 0.0f, 1.0f);

    /* Update state based on activity */
    if (activity < MIN_ACTIVITY_THRESHOLD) {
        vatpase->state = PUMP_STATE_INACTIVE;
    } else if (activity > 0.9f) {
        vatpase->state = PUMP_STATE_SATURATED;
    } else if (activity > 0.5f) {
        vatpase->state = PUMP_STATE_ACTIVATED;
    } else {
        vatpase->state = PUMP_STATE_BASAL;
    }

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_vatpase_set_assembly(
    nimcp_vatpase_t* vatpase,
    float assembly
) {
    if (!vatpase) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "V-ATPase is NULL in set_assembly");
        return PUMP_ERR_NULL_PTR;
    }

    vatpase->v0_v1_assembly = nimcp_clampf(assembly, 0.0f, 1.0f);

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_vatpase_calculate_flux(
    const nimcp_vatpase_t* vatpase,
    float atp_available,
    float luminal_ph,
    float* h_flux,
    float* atp_cost
) {
    if (!vatpase || !h_flux || !atp_cost) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "V-ATPase calculate_flux: NULL argument");
        return PUMP_ERR_NULL_PTR;
    }

    /* Check for sufficient ATP */
    if (atp_available < ATP_DEPLETION_THRESHOLD) {
        *h_flux = 0.0f;
        *atp_cost = 0.0f;
        return PUMP_ERR_NO_ATP;
    }

    /* V-ATPase is inhibited by low luminal pH */
    float luminal_h = powf(10.0f, -luminal_ph);
    float ph_inhibition = 1.0f / (1.0f + (luminal_h / vatpase->km_h));

    /* ATP-dependent kinetics */
    float atp_factor = michaelis_menten(atp_available, vatpase->km_atp, 1.0f);

    /* Assembly state affects activity */
    float assembly_factor = vatpase->v0_v1_assembly;

    /* Calculate proton flux */
    float rate = vatpase->max_rate *
                 vatpase->activity_level *
                 atp_factor *
                 ph_inhibition *
                 assembly_factor;

    *h_flux = rate;
    *atp_cost = rate / vatpase->coupling_efficiency * PUMP_VATPASE_ATP_COST;

    return PUMP_OK;
}

//=============================================================================
// NHE API Implementation
//=============================================================================

nimcp_pump_error_t nimcp_nhe_set_activity(
    nimcp_nhe_t* nhe,
    float activity
) {
    if (!nhe) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NHE is NULL in set_activity");
        return PUMP_ERR_NULL_PTR;
    }

    nhe->activity_level = nimcp_clampf(activity, 0.0f, 1.0f);

    /* Update state */
    if (activity < MIN_ACTIVITY_THRESHOLD) {
        nhe->state = PUMP_STATE_INACTIVE;
    } else if (activity > 0.9f) {
        nhe->state = PUMP_STATE_SATURATED;
    } else if (activity > 0.5f) {
        nhe->state = PUMP_STATE_ACTIVATED;
    } else {
        nhe->state = PUMP_STATE_BASAL;
    }

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_nhe_set_setpoint(
    nimcp_nhe_t* nhe,
    float set_point
) {
    if (!nhe) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NHE is NULL in set_setpoint");
        return PUMP_ERR_NULL_PTR;
    }

    if (set_point < 6.5f || set_point > 7.5f) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "NHE set_point out of range: %f", set_point);
        return PUMP_ERR_INVALID_PARAM;
    }

    nhe->set_point = set_point;

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_nhe_calculate_exchange(
    const nimcp_nhe_t* nhe,
    float intracellular_ph,
    float extracellular_na,
    float* exchange_rate
) {
    if (!nhe || !exchange_rate) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NHE calculate_exchange: NULL argument");
        return PUMP_ERR_NULL_PTR;
    }

    /* NHE is activated by intracellular acidification */
    float h_in = powf(10.0f, -intracellular_ph);
    float h_modifier = powf(10.0f, -nhe->set_point);

    /* Allosteric activation by H+ */
    float h_activation = h_in / (h_in + h_modifier);

    /* Na+ gradient dependence */
    float na_factor = michaelis_menten(extracellular_na, nhe->km_na_out, 1.0f);

    /* Regulatory factors */
    float regulation = nhe->phosphorylation_state * 0.5f +
                      nhe->calmodulin_binding * 0.5f;

    /* Calculate exchange rate */
    float rate = nhe->max_rate *
                 nhe->activity_level *
                 h_activation *
                 na_factor *
                 regulation;

    *exchange_rate = rate;

    return PUMP_OK;
}

//=============================================================================
// NBC API Implementation
//=============================================================================

nimcp_pump_error_t nimcp_nbc_set_activity(
    nimcp_nbc_t* nbc,
    float activity
) {
    if (!nbc) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NBC is NULL in set_activity");
        return PUMP_ERR_NULL_PTR;
    }

    nbc->activity_level = nimcp_clampf(activity, 0.0f, 1.0f);

    /* Update state */
    if (activity < MIN_ACTIVITY_THRESHOLD) {
        nbc->state = PUMP_STATE_INACTIVE;
    } else if (activity > 0.9f) {
        nbc->state = PUMP_STATE_SATURATED;
    } else if (activity > 0.5f) {
        nbc->state = PUMP_STATE_ACTIVATED;
    } else {
        nbc->state = PUMP_STATE_BASAL;
    }

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_nbc_calculate_transport(
    const nimcp_nbc_t* nbc,
    float na_gradient,
    float hco3_gradient,
    float* transport_rate
) {
    if (!nbc || !transport_rate) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NBC calculate_transport: NULL argument");
        return PUMP_ERR_NULL_PTR;
    }

    /* Electrogenic: depends on both gradients */
    float na_factor = na_gradient / 140.0f;  /* Normalized to typical gradient */
    float hco3_factor = hco3_gradient / 24.0f;

    /* Stoichiometry affects driving force */
    float driving_force;
    if (nbc->stoichiometry >= 2.0f) {
        /* Electrogenic - more sensitive to voltage */
        driving_force = na_factor * powf(hco3_factor, nbc->stoichiometry - 1.0f);
    } else {
        /* Electroneutral */
        driving_force = na_factor * hco3_factor;
    }

    /* Calculate transport rate */
    float rate = nbc->max_rate *
                 nbc->activity_level *
                 driving_force *
                 nbc->carbonic_anhydrase_coupling;

    *transport_rate = rate;

    return PUMP_OK;
}

//=============================================================================
// System Update API Implementation
//=============================================================================

nimcp_pump_error_t nimcp_pump_update(
    nimcp_pump_system_t* system,
    float dt,
    float intracellular_ph,
    float extracellular_ph,
    float vesicular_ph
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Proton pump system is NULL in update");
        return PUMP_ERR_NULL_PTR;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "Proton pump system not initialized");
        return PUMP_ERR_NOT_INITIALIZED;
    }

    float dt_sec = dt / 1000.0f;
    float total_atp_cost = 0.0f;
    float net_h_flux = 0.0f;
    float net_hco3_flux = 0.0f;

    /* Update V-ATPase */
    {
        float h_flux, atp_cost;
        nimcp_pump_error_t err = nimcp_vatpase_calculate_flux(
            &system->vatpase,
            system->atp_available,
            vesicular_ph,
            &h_flux,
            &atp_cost
        );

        if (err == PUMP_OK) {
            system->vatpase.current_rate = h_flux;
            system->vatpase.atp_hydrolysis_rate = atp_cost;
            system->vatpase.total_h_transported += (uint64_t)(h_flux * dt_sec);
            system->vatpase.total_atp_consumed += atp_cost * dt_sec;
            total_atp_cost += atp_cost * dt_sec;
            net_h_flux += h_flux;  /* H+ into vesicle */
        }
    }

    /* Update NHE */
    {
        float exchange_rate;
        nimcp_pump_error_t err = nimcp_nhe_calculate_exchange(
            &system->nhe,
            intracellular_ph,
            system->config.na_gradient,
            &exchange_rate
        );

        if (err == PUMP_OK) {
            system->nhe.current_rate = exchange_rate;
            system->nhe.total_exchanges += (uint64_t)(exchange_rate * dt_sec);
            net_h_flux -= exchange_rate;  /* H+ exported from cell */
            /* NHE uses Na+ gradient, not ATP directly */
            total_atp_cost += exchange_rate * PUMP_NHE_ATP_EQUIVALENT * dt_sec;
        }
    }

    /* Update NBC */
    {
        float transport_rate;
        nimcp_pump_error_t err = nimcp_nbc_calculate_transport(
            &system->nbc,
            system->config.na_gradient,
            24.0f,  /* Typical HCO3- */
            &transport_rate
        );

        if (err == PUMP_OK) {
            system->nbc.current_rate = transport_rate;
            system->nbc.total_transported += (uint64_t)(transport_rate * dt_sec);
            net_hco3_flux += transport_rate;
        }
    }

    /* Update ATP */
    system->atp_available -= total_atp_cost;
    system->atp_available += system->config.atp_regeneration_rate * dt_sec;
    system->atp_available = nimcp_clampf(system->atp_available, 0.0f, 10.0f);
    system->atp_consumed += total_atp_cost;

    /* Update net fluxes */
    system->net_h_flux = net_h_flux;
    system->net_hco3_flux = net_hco3_flux;

    system->update_count++;

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_pump_get_net_h_flux(
    const nimcp_pump_system_t* system,
    float* h_flux
) {
    if (!system || !h_flux) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Pump get_net_h_flux: NULL argument");
        return PUMP_ERR_NULL_PTR;
    }

    *h_flux = system->net_h_flux;

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_pump_get_atp_consumption(
    const nimcp_pump_system_t* system,
    float* atp_rate
) {
    if (!system || !atp_rate) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Pump get_atp_consumption: NULL argument");
        return PUMP_ERR_NULL_PTR;
    }

    *atp_rate = system->vatpase.atp_hydrolysis_rate +
                system->nhe.current_rate * PUMP_NHE_ATP_EQUIVALENT;

    return PUMP_OK;
}

nimcp_pump_error_t nimcp_pump_supply_atp(
    nimcp_pump_system_t* system,
    float atp_amount
) {
    if (!system) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "Pump supply_atp: system is NULL");
        return PUMP_ERR_NULL_PTR;
    }

    if (atp_amount < 0.0f) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "Pump supply_atp: negative ATP amount: %f", atp_amount);
        return PUMP_ERR_INVALID_PARAM;
    }

    system->atp_available += atp_amount;
    system->atp_available = nimcp_clampf(system->atp_available, 0.0f, 10.0f);

    return PUMP_OK;
}

//=============================================================================
// Utility API Implementation
//=============================================================================

const char* nimcp_pump_state_string(nimcp_pump_state_t state) {
    switch (state) {
        case PUMP_STATE_INACTIVE:
            return "Inactive";
        case PUMP_STATE_BASAL:
            return "Basal";
        case PUMP_STATE_ACTIVATED:
            return "Activated";
        case PUMP_STATE_INHIBITED:
            return "Inhibited";
        case PUMP_STATE_SATURATED:
            return "Saturated";
        default:
            return "Unknown";
    }
}

const char* nimcp_pump_error_string(nimcp_pump_error_t error) {
    switch (error) {
        case PUMP_OK:
            return "OK";
        case PUMP_ERR_NULL_PTR:
            return "Null pointer";
        case PUMP_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case PUMP_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case PUMP_ERR_NO_ATP:
            return "Insufficient ATP";
        case PUMP_ERR_GRADIENT_DEPLETED:
            return "Gradient depleted";
        case PUMP_ERR_PUMP_INHIBITED:
            return "Pump inhibited";
        case PUMP_ERR_CAPACITY_EXCEEDED:
            return "Capacity exceeded";
        default:
            return "Unknown error";
    }
}

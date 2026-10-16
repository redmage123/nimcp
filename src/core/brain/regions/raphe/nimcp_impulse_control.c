/**
 * @file nimcp_impulse_control.c
 * @brief Impulse control and behavioral inhibition implementation
 * @date 2026-01-11
 */

#include "core/brain/regions/raphe/nimcp_impulse_control.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(impulse_control, MESH_ADAPTER_CATEGORY_COGNITIVE)

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

nimcp_impulse_config_t nimcp_impulse_default_config(void) {
    nimcp_impulse_config_t config = {
        .baseline_inhibition = IMPULSE_DEFAULT_INHIBITION,
        .baseline_patience = IMPULSE_DEFAULT_PATIENCE,
        .baseline_risk_aversion = IMPULSE_DEFAULT_RISK_AVERSION,
        .ht_inhibition_gain = IMPULSE_5HT_GAIN,
        .ht_patience_gain = 0.6f,
        .urgency_decay = 0.95f  /* Urgency decays 5% per second */
    };
    return config;
}

int nimcp_impulse_init(nimcp_impulse_system_t* system,
                       const nimcp_impulse_config_t* config) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    memset(system, 0, sizeof(nimcp_impulse_system_t));

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_impulse_default_config();
    }

    /* Initialize state */
    system->inhibition_strength = system->config.baseline_inhibition;
    system->patience = system->config.baseline_patience;
    system->risk_aversion = system->config.baseline_risk_aversion;
    system->impulsivity = 1.0f - system->inhibition_strength;

    /* Initialize urgency */
    system->accumulated_urgency = 0.0f;
    system->urgency_threshold = 0.8f;

    /* Initialize 5-HT state */
    system->current_5ht = 20.0f;  /* nM baseline */
    system->baseline_5ht = 20.0f;

    /* Initialize statistics */
    system->go_decisions = 0;
    system->nogo_decisions = 0;
    system->wait_decisions = 0;
    system->impulsive_actions = 0;

    system->initialized = true;
    return 0;
}

int nimcp_impulse_shutdown(nimcp_impulse_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }
    system->initialized = false;
    return 0;
}

int nimcp_impulse_reset(nimcp_impulse_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    nimcp_impulse_config_t config = system->config;
    return nimcp_impulse_init(system, &config);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int nimcp_impulse_update(nimcp_impulse_system_t* system, float ht_level, float dt) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    float dt_sec = dt / 1000.0f;
    system->current_5ht = ht_level;

    /* Compute 5-HT ratio */
    float ht_ratio = ht_level / system->baseline_5ht;

    /* 1. Update inhibition strength based on 5-HT */
    /* Higher 5-HT -> stronger inhibition (better impulse control) */
    float inhibition_target = system->config.baseline_inhibition +
                              (ht_ratio - 1.0f) * system->config.ht_inhibition_gain;
    inhibition_target = nimcp_clampf(inhibition_target, 0.1f, 0.95f);

    /* Smooth transition */
    float alpha = 1.0f - expf(-dt_sec * 0.5f);
    system->inhibition_strength = lerp(system->inhibition_strength,
                                        inhibition_target, alpha);

    /* 2. Update patience */
    /* Higher 5-HT -> more patience (can wait longer) */
    float patience_target = system->config.baseline_patience +
                           (ht_ratio - 1.0f) * system->config.ht_patience_gain;
    patience_target = nimcp_clampf(patience_target, 0.1f, 0.95f);
    system->patience = lerp(system->patience, patience_target, alpha);

    /* 3. Update risk aversion */
    /* Higher 5-HT -> more risk averse */
    float risk_target = system->config.baseline_risk_aversion +
                       (ht_ratio - 1.0f) * 0.4f;
    risk_target = nimcp_clampf(risk_target, 0.1f, 0.9f);
    system->risk_aversion = lerp(system->risk_aversion, risk_target, alpha);

    /* 4. Update impulsivity (inverse of inhibition) */
    system->impulsivity = 1.0f - system->inhibition_strength;

    /* 5. Decay accumulated urgency */
    system->accumulated_urgency *= powf(system->config.urgency_decay, dt_sec);

    return 0;
}

/*=============================================================================
 * Decision API
 *===========================================================================*/

int nimcp_impulse_evaluate(nimcp_impulse_system_t* system,
                           float action_urgency,
                           float action_reward,
                           float action_risk,
                           nimcp_impulse_result_t* result) {
    if (!system || !system->initialized || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_update: required parameter is NULL (system, system->initialized, result)");
        return -1;
    }

    /* Accumulate urgency */
    system->accumulated_urgency += action_urgency * 0.1f;
    system->accumulated_urgency = nimcp_clampf(system->accumulated_urgency, 0.0f, 1.0f);

    /* Compute action value (reward minus risk-adjusted penalty) */
    float risk_penalty = action_risk * system->risk_aversion;
    float net_value = action_reward - risk_penalty;

    /* Compute waiting cost (increases with urgency, decreases with patience) */
    float waiting_cost = system->accumulated_urgency * (1.0f - system->patience);

    /* Compute action cost (inhibition penalty for impulsive action) */
    float action_cost = (1.0f - net_value) * system->inhibition_strength;

    /* Decision logic */
    result->waiting_cost = waiting_cost;
    result->action_cost = action_cost;
    result->inhibition_strength = system->inhibition_strength;

    /* Check for impulsive breakthrough */
    bool impulse_breakthrough = system->accumulated_urgency > system->urgency_threshold;

    if (net_value > 0.7f && waiting_cost > 0.5f) {
        /* Strong positive value and high urgency -> GO */
        result->decision = IMPULSE_DECISION_GO;
        result->confidence = net_value * (1.0f - action_risk);
        system->go_decisions++;
    } else if (action_risk > 0.7f && system->inhibition_strength > 0.6f) {
        /* High risk and good inhibition -> NOGO */
        result->decision = IMPULSE_DECISION_NOGO;
        result->confidence = system->inhibition_strength;
        system->nogo_decisions++;
    } else if (impulse_breakthrough) {
        /* Urgency overwhelms inhibition -> GO (impulsive) */
        result->decision = IMPULSE_DECISION_GO;
        result->confidence = 0.3f;  /* Low confidence impulsive action */
        system->go_decisions++;
        system->impulsive_actions++;
    } else if (system->patience > 0.6f && waiting_cost < 0.4f) {
        /* Patient and low urgency -> WAIT */
        result->decision = IMPULSE_DECISION_WAIT;
        result->confidence = system->patience;
        system->wait_decisions++;
    } else if (net_value > action_cost) {
        /* Value exceeds action cost -> GO */
        result->decision = IMPULSE_DECISION_GO;
        result->confidence = net_value - action_cost;
        system->go_decisions++;
    } else {
        /* Default: NOGO */
        result->decision = IMPULSE_DECISION_NOGO;
        result->confidence = system->inhibition_strength;
        system->nogo_decisions++;
    }

    /* Reset urgency if action taken */
    if (result->decision == IMPULSE_DECISION_GO) {
        system->accumulated_urgency = 0.0f;
    }

    return 0;
}

int nimcp_impulse_compute_inhibition(nimcp_impulse_system_t* system,
                                     float impulse_strength,
                                     float* inhibition_output) {
    if (!system || !system->initialized || !inhibition_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_update: required parameter is NULL (system, system->initialized, inhibition_output)");
        return -1;
    }

    /* Inhibition must overcome impulse strength */
    float net_inhibition = system->inhibition_strength - impulse_strength;

    /* Positive = inhibition wins, Negative = impulse breaks through */
    *inhibition_output = nimcp_clampf(net_inhibition, -1.0f, 1.0f);

    return 0;
}

int nimcp_impulse_can_wait(nimcp_impulse_system_t* system,
                           float wait_duration,
                           bool* can_wait) {
    if (!system || !system->initialized || !can_wait) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_update: required parameter is NULL (system, system->initialized, can_wait)");
        return -1;
    }

    /* Patience determines how long we can wait */
    /* wait_duration in ms, patience maps to sustainable wait time */

    float max_wait = system->patience * 10000.0f;  /* Max 10 seconds at full patience */

    /* Urgency reduces wait capacity */
    max_wait *= (1.0f - system->accumulated_urgency);

    *can_wait = (wait_duration <= max_wait);
    return 0;
}

/*=============================================================================
 * Query API
 *===========================================================================*/

int nimcp_impulse_get_inhibition(nimcp_impulse_system_t* system, float* inhibition) {
    if (!system || !system->initialized || !inhibition) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_get_inhibition: required parameter is NULL (system, system->initialized, inhibition)");
        return -1;
    }

    *inhibition = system->inhibition_strength;
    return 0;
}

int nimcp_impulse_get_patience(nimcp_impulse_system_t* system, float* patience) {
    if (!system || !system->initialized || !patience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_get_patience: required parameter is NULL (system, system->initialized, patience)");
        return -1;
    }

    *patience = system->patience;
    return 0;
}

int nimcp_impulse_get_impulsivity(nimcp_impulse_system_t* system, float* impulsivity) {
    if (!system || !system->initialized || !impulsivity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_get_impulsivity: required parameter is NULL (system, system->initialized, impulsivity)");
        return -1;
    }

    *impulsivity = system->impulsivity;
    return 0;
}

int nimcp_impulse_get_risk_aversion(nimcp_impulse_system_t* system, float* risk_aversion) {
    if (!system || !system->initialized || !risk_aversion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_get_risk_aversion: required parameter is NULL (system, system->initialized, risk_aversion)");
        return -1;
    }

    *risk_aversion = system->risk_aversion;
    return 0;
}

/*=============================================================================
 * Urgency API
 *===========================================================================*/

int nimcp_impulse_reset_urgency(nimcp_impulse_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_reset_urgency: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->accumulated_urgency = 0.0f;
    return 0;
}

int nimcp_impulse_get_urgency(nimcp_impulse_system_t* system, float* urgency) {
    if (!system || !system->initialized || !urgency) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_impulse_get_urgency: required parameter is NULL (system, system->initialized, urgency)");
        return -1;
    }

    *urgency = system->accumulated_urgency;
    return 0;
}

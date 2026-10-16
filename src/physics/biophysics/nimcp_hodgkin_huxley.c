#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_hodgkin_huxley.c - Hodgkin-Huxley Neuron Model Implementation
//=============================================================================

#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
/* <complex.h> pulled in transitively via brain_internal->brain.h->quantum_walk.h
 * defines `I` as the imaginary-unit macro, which collides with the local
 * variable `I` used throughout this file for current. Undef here. */
#ifdef I
#undef I
#endif
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __linux__
#include <unistd.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hodgkin_huxley)

#endif

//=============================================================================
// Internal Constants
//=============================================================================

#define HH_EXP_LIMIT 80.0f  /* Prevent exp overflow */

//=============================================================================
// Helper Functions - Rate Constants
//=============================================================================

/**
 * Safe exponential that avoids overflow
 */
static inline float safe_exp(float x) {
    if (x > HH_EXP_LIMIT) return expf(HH_EXP_LIMIT);
    if (x < -HH_EXP_LIMIT) return expf(-HH_EXP_LIMIT);
    return expf(x);
}

/**
 * Alpha_m: Sodium activation opening rate
 * Classic HH formulation
 */
static float alpha_m(float V) {
    float dV = V + 40.0f;
    if (fabsf(dV) < 1e-6f) {
        return 1.0f;  /* L'Hopital limit */
    }
    return 0.1f * dV / (1.0f - safe_exp(-dV / 10.0f));
}

/**
 * Beta_m: Sodium activation closing rate
 */
static float beta_m(float V) {
    return 4.0f * safe_exp(-(V + 65.0f) / 18.0f);
}

/**
 * Alpha_h: Sodium inactivation opening rate
 */
static float alpha_h(float V) {
    return 0.07f * safe_exp(-(V + 65.0f) / 20.0f);
}

/**
 * Beta_h: Sodium inactivation closing rate
 */
static float beta_h(float V) {
    return 1.0f / (1.0f + safe_exp(-(V + 35.0f) / 10.0f));
}

/**
 * Alpha_n: Potassium activation opening rate
 */
static float alpha_n(float V) {
    float dV = V + 55.0f;
    if (fabsf(dV) < 1e-6f) {
        return 0.1f;  /* L'Hopital limit */
    }
    return 0.01f * dV / (1.0f - safe_exp(-dV / 10.0f));
}

/**
 * Beta_n: Potassium activation closing rate
 */
static float beta_n(float V) {
    return 0.125f * safe_exp(-(V + 65.0f) / 80.0f);
}

//=============================================================================
// Public Gating Functions
//=============================================================================

float nimcp_hh_m_inf(float V) {
    float a = alpha_m(V);
    float b = beta_m(V);
    float sum = a + b;
    if (sum < 1e-7f) sum = 1e-7f;
    return a / sum;
}

float nimcp_hh_m_tau(float V) {
    float a = alpha_m(V);
    float b = beta_m(V);
    float sum = a + b;
    if (sum < 1e-7f) sum = 1e-7f;
    return 1.0f / sum;
}

float nimcp_hh_h_inf(float V) {
    float a = alpha_h(V);
    float b = beta_h(V);
    float sum = a + b;
    if (sum < 1e-7f) sum = 1e-7f;
    return a / sum;
}

float nimcp_hh_h_tau(float V) {
    float a = alpha_h(V);
    float b = beta_h(V);
    float sum = a + b;
    if (sum < 1e-7f) sum = 1e-7f;
    return 1.0f / sum;
}

float nimcp_hh_n_inf(float V) {
    float a = alpha_n(V);
    float b = beta_n(V);
    float sum = a + b;
    if (sum < 1e-7f) sum = 1e-7f;
    return a / sum;
}

float nimcp_hh_n_tau(float V) {
    float a = alpha_n(V);
    float b = beta_n(V);
    float sum = a + b;
    if (sum < 1e-7f) sum = 1e-7f;
    return 1.0f / sum;
}

//=============================================================================
// Extended Channel Gating (Ca, K_Ca, K_A, H)
//=============================================================================

/* L-type calcium activation */
static float m_Ca_L_inf(float V) {
    return 1.0f / (1.0f + safe_exp(-(V + 10.0f) / 6.5f));
}

static float m_Ca_L_tau(float V) {
    (void)V;
    return 1.5f;  /* ms, relatively slow */
}

/* T-type calcium activation */
static float m_Ca_T_inf(float V) {
    return 1.0f / (1.0f + safe_exp(-(V + 57.0f) / 6.2f));
}

static float m_Ca_T_tau(float V) {
    return 1.0f + 10.0f / (1.0f + safe_exp((V + 68.0f) / 12.0f));
}

/* T-type calcium inactivation */
static float h_Ca_T_inf(float V) {
    return 1.0f / (1.0f + safe_exp((V + 81.0f) / 4.0f));
}

static float h_Ca_T_tau(float V) {
    return 28.3f + 60.0f / (1.0f + safe_exp((V + 55.0f) / 10.0f));
}

/* Calcium-activated potassium */
static float m_K_Ca_inf(float Ca_i) {
    float Ca_sq = Ca_i * Ca_i;
    return Ca_sq / (Ca_sq + 0.01f);  /* Half-activation at 0.1 uM */
}

/* A-type potassium activation */
static float m_K_A_inf(float V) {
    return 1.0f / (1.0f + safe_exp(-(V + 45.0f) / 14.0f));
}

static float m_K_A_tau(float V) {
    return 0.5f + 5.0f / (1.0f + safe_exp((V + 35.0f) / 10.0f));
}

/* A-type potassium inactivation */
static float h_K_A_inf(float V) {
    return 1.0f / (1.0f + safe_exp((V + 75.0f) / 6.0f));
}

static float h_K_A_tau(float V) {
    return 15.0f + 30.0f / (1.0f + safe_exp((V + 60.0f) / 10.0f));
}

/* H-current (Ih) activation */
static float m_H_inf(float V) {
    return 1.0f / (1.0f + safe_exp((V + 75.0f) / 5.5f));
}

static float m_H_tau(float V) {
    return 1.0f / (safe_exp(-14.59f - 0.086f * V) + safe_exp(-1.87f + 0.0701f * V));
}

//=============================================================================
// Ion Channel Initialization
//=============================================================================

static void init_channel_na(nimcp_ion_channel_t* ch, float g_max, float E_rev, float V) {
    ch->type = NIMCP_ION_CHANNEL_NA;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = nimcp_hh_m_inf(V);
    ch->activation.inf = ch->activation.value;
    ch->activation.tau = nimcp_hh_m_tau(V);
    ch->inactivation.value = nimcp_hh_h_inf(V);
    ch->inactivation.inf = ch->inactivation.value;
    ch->inactivation.tau = nimcp_hh_h_tau(V);
    ch->activation_power = 3;
    ch->inactivation_power = 1;
    ch->modulation_factor = 1.0f;
    ch->enabled = true;
}

static void init_channel_k(nimcp_ion_channel_t* ch, float g_max, float E_rev, float V) {
    ch->type = NIMCP_ION_CHANNEL_K;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = nimcp_hh_n_inf(V);
    ch->activation.inf = ch->activation.value;
    ch->activation.tau = nimcp_hh_n_tau(V);
    ch->inactivation.value = 1.0f;  /* No inactivation */
    ch->inactivation.inf = 1.0f;
    ch->inactivation.tau = 1.0f;
    ch->activation_power = 4;
    ch->inactivation_power = 0;
    ch->modulation_factor = 1.0f;
    ch->enabled = true;
}

static void init_channel_leak(nimcp_ion_channel_t* ch, float g_max, float E_rev) {
    ch->type = NIMCP_ION_CHANNEL_LEAK;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = 1.0f;
    ch->activation.inf = 1.0f;
    ch->activation.tau = 1.0f;
    ch->inactivation.value = 1.0f;
    ch->inactivation.inf = 1.0f;
    ch->inactivation.tau = 1.0f;
    ch->activation_power = 0;
    ch->inactivation_power = 0;
    ch->modulation_factor = 1.0f;
    ch->enabled = true;
}

static void init_channel_ca_l(nimcp_ion_channel_t* ch, float g_max, float E_rev, float V) {
    ch->type = NIMCP_ION_CHANNEL_CA_L;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = m_Ca_L_inf(V);
    ch->activation.inf = ch->activation.value;
    ch->activation.tau = m_Ca_L_tau(V);
    ch->inactivation.value = 1.0f;
    ch->inactivation.inf = 1.0f;
    ch->inactivation.tau = 1.0f;
    ch->activation_power = 2;
    ch->inactivation_power = 0;
    ch->modulation_factor = 1.0f;
    ch->enabled = false;  /* Disabled by default */
}

static void init_channel_ca_t(nimcp_ion_channel_t* ch, float g_max, float E_rev, float V) {
    ch->type = NIMCP_ION_CHANNEL_CA_T;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = m_Ca_T_inf(V);
    ch->activation.inf = ch->activation.value;
    ch->activation.tau = m_Ca_T_tau(V);
    ch->inactivation.value = h_Ca_T_inf(V);
    ch->inactivation.inf = ch->inactivation.value;
    ch->inactivation.tau = h_Ca_T_tau(V);
    ch->activation_power = 2;
    ch->inactivation_power = 1;
    ch->modulation_factor = 1.0f;
    ch->enabled = false;
}

static void init_channel_k_ca(nimcp_ion_channel_t* ch, float g_max, float E_rev, float Ca_i) {
    ch->type = NIMCP_ION_CHANNEL_K_CA;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = m_K_Ca_inf(Ca_i);
    ch->activation.inf = ch->activation.value;
    ch->activation.tau = 2.0f;  /* ms */
    ch->inactivation.value = 1.0f;
    ch->inactivation.inf = 1.0f;
    ch->inactivation.tau = 1.0f;
    ch->activation_power = 1;
    ch->inactivation_power = 0;
    ch->modulation_factor = 1.0f;
    ch->enabled = false;
}

static void init_channel_k_a(nimcp_ion_channel_t* ch, float g_max, float E_rev, float V) {
    ch->type = NIMCP_ION_CHANNEL_K_A;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = m_K_A_inf(V);
    ch->activation.inf = ch->activation.value;
    ch->activation.tau = m_K_A_tau(V);
    ch->inactivation.value = h_K_A_inf(V);
    ch->inactivation.inf = ch->inactivation.value;
    ch->inactivation.tau = h_K_A_tau(V);
    ch->activation_power = 4;
    ch->inactivation_power = 1;
    ch->modulation_factor = 1.0f;
    ch->enabled = false;
}

static void init_channel_h(nimcp_ion_channel_t* ch, float g_max, float E_rev, float V) {
    ch->type = NIMCP_ION_CHANNEL_H;
    ch->g_max = g_max;
    ch->E_rev = E_rev;
    ch->activation.value = m_H_inf(V);
    ch->activation.inf = ch->activation.value;
    ch->activation.tau = m_H_tau(V);
    ch->inactivation.value = 1.0f;
    ch->inactivation.inf = 1.0f;
    ch->inactivation.tau = 1.0f;
    ch->activation_power = 1;
    ch->inactivation_power = 0;
    ch->modulation_factor = 1.0f;
    ch->enabled = false;
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_error_t nimcp_hh_config_default(nimcp_hh_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Standard conductances */
    config->g_Na = NIMCP_HH_DEFAULT_G_NA;
    config->g_K = NIMCP_HH_DEFAULT_G_K;
    config->g_L = NIMCP_HH_DEFAULT_G_L;
    config->g_Ca_L = 1.0f;
    config->g_Ca_T = 0.5f;
    config->g_K_Ca = 5.0f;
    config->g_K_A = 10.0f;
    config->g_H = 0.02f;

    /* Reversal potentials */
    config->E_Na = NIMCP_HH_DEFAULT_E_NA;
    config->E_K = NIMCP_HH_DEFAULT_E_K;
    config->E_L = NIMCP_HH_DEFAULT_E_L;
    config->E_Ca = 120.0f;
    config->E_H = -40.0f;

    /* Membrane properties */
    config->C_m = NIMCP_HH_DEFAULT_C_M;
    config->V_rest = NIMCP_HH_DEFAULT_V_REST;

    /* Temperature */
    config->temperature = NIMCP_HH_TEMPERATURE_REF;

    /* Morphology defaults */
    config->surface_area = 1.0f;  /* cm^2 */
    config->length = 100.0f;      /* um */
    config->diameter = 10.0f;     /* um */

    /* Extended channels disabled by default */
    config->enable_calcium = false;
    config->enable_adaptation = false;
    config->enable_h_current = false;

    /* Integration */
    config->dt_max = NIMCP_HH_MAX_DT;
    config->adaptive_dt = false;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_config_for_type(nimcp_hh_config_t* config, const char* type) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    NIMCP_CHECK_THROW(type, NIMCP_ERROR_NULL_POINTER, "type is NULL");

    /* Start with defaults */
    nimcp_hh_config_default(config);

    if (strcmp(type, "pyramidal") == 0) {
        /* Cortical pyramidal neuron */
        config->g_Na = 100.0f;
        config->g_K = 30.0f;
        config->g_L = 0.1f;
        config->enable_calcium = true;
        config->enable_adaptation = true;
        config->g_Ca_L = 0.5f;
        config->g_K_Ca = 3.0f;
    } else if (strcmp(type, "interneuron") == 0) {
        /* Fast-spiking interneuron */
        config->g_Na = 150.0f;
        config->g_K = 50.0f;
        config->g_L = 0.2f;
        config->enable_calcium = false;
        config->enable_adaptation = false;
    } else if (strcmp(type, "purkinje") == 0) {
        /* Cerebellar Purkinje cell */
        config->g_Na = 125.0f;
        config->g_K = 40.0f;
        config->g_L = 0.25f;
        config->enable_calcium = true;
        config->g_Ca_L = 2.0f;
        config->g_Ca_T = 1.0f;
    } else if (strcmp(type, "thalamic") == 0) {
        /* Thalamocortical relay neuron */
        config->g_Na = 90.0f;
        config->g_K = 25.0f;
        config->g_L = 0.1f;
        config->enable_calcium = true;
        config->enable_h_current = true;
        config->g_Ca_T = 2.0f;
        config->g_H = 0.05f;
    } else if (strcmp(type, "dopaminergic") == 0) {
        /* Dopaminergic neuron (slow pacemaker) */
        config->g_Na = 50.0f;
        config->g_K = 20.0f;
        config->g_L = 0.05f;
        config->enable_calcium = true;
        config->enable_adaptation = true;
        config->g_Ca_L = 1.5f;
        config->g_K_Ca = 8.0f;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Neuron Lifecycle
//=============================================================================

nimcp_error_t nimcp_hh_neuron_init(nimcp_hh_neuron_t* neuron, const nimcp_hh_config_t* config) {
    NIMCP_CHECK_THROW(neuron, NIMCP_ERROR_NULL_POINTER, "neuron is NULL");

    memset(neuron, 0, sizeof(*neuron));

    /* Apply configuration */
    if (config) {
        neuron->config = *config;
    } else {
        nimcp_hh_config_default(&neuron->config);
    }

    /* Initial voltage */
    neuron->V = neuron->config.V_rest;
    neuron->V_prev = neuron->V;

    /* Temperature factor */
    neuron->temperature = neuron->config.temperature;
    neuron->phi = powf(NIMCP_HH_Q10_RATE,
                       (neuron->temperature - NIMCP_HH_TEMPERATURE_REF) / 10.0f);

    /* Initialize ion channels */
    init_channel_na(&neuron->channels[NIMCP_ION_CHANNEL_NA],
                    neuron->config.g_Na, neuron->config.E_Na, neuron->V);
    init_channel_k(&neuron->channels[NIMCP_ION_CHANNEL_K],
                   neuron->config.g_K, neuron->config.E_K, neuron->V);
    init_channel_leak(&neuron->channels[NIMCP_ION_CHANNEL_LEAK],
                      neuron->config.g_L, neuron->config.E_L);

    /* Extended channels */
    init_channel_ca_l(&neuron->channels[NIMCP_ION_CHANNEL_CA_L],
                      neuron->config.g_Ca_L, neuron->config.E_Ca, neuron->V);
    init_channel_ca_t(&neuron->channels[NIMCP_ION_CHANNEL_CA_T],
                      neuron->config.g_Ca_T, neuron->config.E_Ca, neuron->V);
    init_channel_k_ca(&neuron->channels[NIMCP_ION_CHANNEL_K_CA],
                      neuron->config.g_K_Ca, neuron->config.E_K, 0.0001f);
    init_channel_k_a(&neuron->channels[NIMCP_ION_CHANNEL_K_A],
                     neuron->config.g_K_A, neuron->config.E_K, neuron->V);
    init_channel_h(&neuron->channels[NIMCP_ION_CHANNEL_H],
                   neuron->config.g_H, neuron->config.E_H, neuron->V);

    /* Enable extended channels if configured */
    if (neuron->config.enable_calcium) {
        neuron->channels[NIMCP_ION_CHANNEL_CA_L].enabled = true;
        neuron->channels[NIMCP_ION_CHANNEL_CA_T].enabled = true;
    }
    if (neuron->config.enable_adaptation) {
        neuron->channels[NIMCP_ION_CHANNEL_K_CA].enabled = true;
    }
    if (neuron->config.enable_h_current) {
        neuron->channels[NIMCP_ION_CHANNEL_H].enabled = true;
    }

    /* Calcium */
    neuron->Ca_i = 0.0001f;  /* 0.1 uM baseline */
    neuron->Ca_o = 2.0f;     /* 2 mM extracellular */
    neuron->Ca_decay_tau = 80.0f;  /* ms */

    /* Morphology */
    neuron->surface_area = neuron->config.surface_area;

    /* Spike state */
    neuron->spiked = false;
    neuron->last_spike_time = -1000.0f;
    neuron->refractory_remaining = 0.0f;
    neuron->spike_count = 0;

    /* Time */
    neuron->time = 0.0f;
    neuron->dt = 0.025f;  /* 25 us default */

    /* Derived parameters */
    float g_total = neuron->config.g_Na * 0.01f + neuron->config.g_K * 0.01f + neuron->config.g_L;
    neuron->membrane_time_constant = neuron->config.C_m / g_total;
    neuron->input_resistance = 1.0f / (g_total * neuron->surface_area);

    neuron->initialized = true;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_neuron_create(nimcp_hh_neuron_t* neuron, brain_t brain,
                                      const nimcp_hh_config_t* config) {
    NIMCP_CHECK_THROW(neuron, NIMCP_ERROR_NULL_POINTER, "neuron is NULL");

    nimcp_error_t err = nimcp_hh_neuron_init(neuron, config);
    if (err != NIMCP_SUCCESS) return err;

    /* Brain integration would go here */
    (void)brain;

    return NIMCP_SUCCESS;
}

void nimcp_hh_neuron_destroy(nimcp_hh_neuron_t* neuron) {
    if (!neuron) return;
    memset(neuron, 0, sizeof(*neuron));
}

nimcp_error_t nimcp_hh_neuron_reset(nimcp_hh_neuron_t* neuron) {
    NIMCP_CHECK_THROW(neuron && neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");

    nimcp_hh_config_t config = neuron->config;
    return nimcp_hh_neuron_init(neuron, &config);
}

//=============================================================================
// Simulation - Core Update
//=============================================================================

/**
 * Compute total membrane current
 */
static float compute_total_current(nimcp_hh_neuron_t* neuron) {
    float I_total = 0.0f;

    for (int i = 0; i < NIMCP_ION_CHANNEL_COUNT; i++) {
        nimcp_ion_channel_t* ch = &neuron->channels[i];
        if (!ch->enabled) continue;

        /* Compute gating product */
        float m_pow = 1.0f;
        float h_pow = 1.0f;

        if (ch->activation_power > 0) {
            m_pow = powf(ch->activation.value, (float)ch->activation_power);
        }
        if (ch->inactivation_power > 0) {
            h_pow = powf(ch->inactivation.value, (float)ch->inactivation_power);
        }

        /* Current conductance */
        ch->g_current = ch->g_max * m_pow * h_pow * ch->modulation_factor;

        /* Current through channel */
        ch->I_current = ch->g_current * (neuron->V - ch->E_rev);

        I_total += ch->I_current;
    }

    return I_total;
}

/**
 * Update gating variables using exponential Euler
 */
static void update_gating(nimcp_hh_neuron_t* neuron, float dt) {
    float V = neuron->V;
    float phi = neuron->phi;

    /* Sodium activation (m) */
    nimcp_ion_channel_t* na = &neuron->channels[NIMCP_ION_CHANNEL_NA];
    if (na->enabled) {
        na->activation.inf = nimcp_hh_m_inf(V);
        na->activation.tau = nimcp_hh_m_tau(V) / phi;
        float factor = dt / na->activation.tau;
        if (factor > 1.0f) factor = 1.0f;
        na->activation.value += factor * (na->activation.inf - na->activation.value);

        na->inactivation.inf = nimcp_hh_h_inf(V);
        na->inactivation.tau = nimcp_hh_h_tau(V) / phi;
        factor = dt / na->inactivation.tau;
        if (factor > 1.0f) factor = 1.0f;
        na->inactivation.value += factor * (na->inactivation.inf - na->inactivation.value);
    }

    /* Potassium activation (n) */
    nimcp_ion_channel_t* k = &neuron->channels[NIMCP_ION_CHANNEL_K];
    if (k->enabled) {
        k->activation.inf = nimcp_hh_n_inf(V);
        k->activation.tau = nimcp_hh_n_tau(V) / phi;
        float factor = dt / k->activation.tau;
        if (factor > 1.0f) factor = 1.0f;
        k->activation.value += factor * (k->activation.inf - k->activation.value);
    }

    /* L-type calcium */
    nimcp_ion_channel_t* ca_l = &neuron->channels[NIMCP_ION_CHANNEL_CA_L];
    if (ca_l->enabled) {
        ca_l->activation.inf = m_Ca_L_inf(V);
        ca_l->activation.tau = m_Ca_L_tau(V) / phi;
        float factor = dt / ca_l->activation.tau;
        if (factor > 1.0f) factor = 1.0f;
        ca_l->activation.value += factor * (ca_l->activation.inf - ca_l->activation.value);
    }

    /* T-type calcium */
    nimcp_ion_channel_t* ca_t = &neuron->channels[NIMCP_ION_CHANNEL_CA_T];
    if (ca_t->enabled) {
        ca_t->activation.inf = m_Ca_T_inf(V);
        ca_t->activation.tau = m_Ca_T_tau(V) / phi;
        float factor = dt / ca_t->activation.tau;
        if (factor > 1.0f) factor = 1.0f;
        ca_t->activation.value += factor * (ca_t->activation.inf - ca_t->activation.value);

        ca_t->inactivation.inf = h_Ca_T_inf(V);
        ca_t->inactivation.tau = h_Ca_T_tau(V) / phi;
        factor = dt / ca_t->inactivation.tau;
        if (factor > 1.0f) factor = 1.0f;
        ca_t->inactivation.value += factor * (ca_t->inactivation.inf - ca_t->inactivation.value);
    }

    /* Calcium-activated K */
    nimcp_ion_channel_t* k_ca = &neuron->channels[NIMCP_ION_CHANNEL_K_CA];
    if (k_ca->enabled) {
        k_ca->activation.inf = m_K_Ca_inf(neuron->Ca_i);
        float factor = dt / k_ca->activation.tau;
        if (factor > 1.0f) factor = 1.0f;
        k_ca->activation.value += factor * (k_ca->activation.inf - k_ca->activation.value);
    }

    /* A-type K */
    nimcp_ion_channel_t* k_a = &neuron->channels[NIMCP_ION_CHANNEL_K_A];
    if (k_a->enabled) {
        k_a->activation.inf = m_K_A_inf(V);
        k_a->activation.tau = m_K_A_tau(V) / phi;
        float factor = dt / k_a->activation.tau;
        if (factor > 1.0f) factor = 1.0f;
        k_a->activation.value += factor * (k_a->activation.inf - k_a->activation.value);

        k_a->inactivation.inf = h_K_A_inf(V);
        k_a->inactivation.tau = h_K_A_tau(V) / phi;
        factor = dt / k_a->inactivation.tau;
        if (factor > 1.0f) factor = 1.0f;
        k_a->inactivation.value += factor * (k_a->inactivation.inf - k_a->inactivation.value);
    }

    /* H-current */
    nimcp_ion_channel_t* h = &neuron->channels[NIMCP_ION_CHANNEL_H];
    if (h->enabled) {
        h->activation.inf = m_H_inf(V);
        h->activation.tau = m_H_tau(V) / phi;
        float factor = dt / h->activation.tau;
        if (factor > 1.0f) factor = 1.0f;
        h->activation.value += factor * (h->activation.inf - h->activation.value);
    }
}

/**
 * Update calcium dynamics
 */
static void update_calcium(nimcp_hh_neuron_t* neuron, float dt) {
    /* Calcium influx from Ca channels */
    float I_Ca = 0.0f;
    if (neuron->channels[NIMCP_ION_CHANNEL_CA_L].enabled) {
        I_Ca += neuron->channels[NIMCP_ION_CHANNEL_CA_L].I_current;
    }
    if (neuron->channels[NIMCP_ION_CHANNEL_CA_T].enabled) {
        I_Ca += neuron->channels[NIMCP_ION_CHANNEL_CA_T].I_current;
    }

    /* Convert current to concentration change */
    /* dCa/dt = -k * I_Ca - (Ca - Ca_rest) / tau */
    float k_Ca = 0.0001f;  /* Conversion factor */
    float Ca_rest = 0.0001f;  /* 0.1 uM */

    float dCa = -k_Ca * I_Ca - (neuron->Ca_i - Ca_rest) / neuron->Ca_decay_tau;
    neuron->Ca_i += dCa * dt;

    /* Clamp to reasonable range */
    if (neuron->Ca_i < 0.00001f) neuron->Ca_i = 0.00001f;
    if (neuron->Ca_i > 0.01f) neuron->Ca_i = 0.01f;  /* 10 uM max */
}

nimcp_error_t nimcp_hh_neuron_update(nimcp_hh_neuron_t* neuron, float I_ext, float dt) {
    NIMCP_CHECK_THROW(neuron && neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");

    /* Use provided dt or default */
    if (dt <= 0.0f) dt = neuron->dt;
    if (dt > neuron->config.dt_max) dt = neuron->config.dt_max;
    if (dt < NIMCP_HH_MIN_DT) dt = NIMCP_HH_MIN_DT;

    neuron->dt = dt;
    neuron->V_prev = neuron->V;
    neuron->spiked = false;

    /* Store external current */
    neuron->I_ext = I_ext;

    /* Update refractory */
    if (neuron->refractory_remaining > 0.0f) {
        neuron->refractory_remaining -= dt;
    }

    /* Compute ionic currents */
    float I_ion = compute_total_current(neuron);

    /* Total current: I = I_ext + I_syn - I_ion */
    float I_total = I_ext + neuron->I_syn_exc - neuron->I_syn_inh - I_ion;

    /* Membrane equation: C_m * dV/dt = I */
    float dV = I_total / neuron->config.C_m;
    neuron->V += dV * dt;

    /* Update gating variables */
    update_gating(neuron, dt);

    /* Update calcium if enabled */
    if (neuron->config.enable_calcium) {
        update_calcium(neuron, dt);
    }

    /* Spike detection */
    if (neuron->refractory_remaining <= 0.0f &&
        neuron->V_prev < NIMCP_HH_SPIKE_THRESHOLD &&
        neuron->V >= NIMCP_HH_SPIKE_THRESHOLD) {
        neuron->spiked = true;
        neuron->last_spike_time = neuron->time;
        neuron->spike_count++;
        neuron->refractory_remaining = NIMCP_HH_SPIKE_REFRACTORY;
    }

    /* Update time */
    neuron->time += dt;

    /* Clear synaptic currents (they must be re-injected each step) */
    neuron->I_syn_exc = 0.0f;
    neuron->I_syn_inh = 0.0f;

    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

nimcp_error_t nimcp_hh_neuron_get_spike(const nimcp_hh_neuron_t* neuron, bool* spiked) {
    NIMCP_CHECK_THROW(neuron, NIMCP_ERROR_NULL_POINTER, "neuron is NULL");
    NIMCP_CHECK_THROW(spiked, NIMCP_ERROR_NULL_POINTER, "spiked is NULL");
    NIMCP_CHECK_THROW(neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");
    *spiked = neuron->spiked;
    return NIMCP_SUCCESS;
}

float nimcp_hh_neuron_get_voltage(const nimcp_hh_neuron_t* neuron) {
    if (!neuron || !neuron->initialized) return 0.0f;
    return neuron->V;
}

nimcp_error_t nimcp_hh_neuron_inject_synaptic(nimcp_hh_neuron_t* neuron,
                                               float I_exc, float I_inh) {
    NIMCP_CHECK_THROW(neuron && neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");
    neuron->I_syn_exc += I_exc;
    neuron->I_syn_inh += I_inh;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_get_channel(const nimcp_hh_neuron_t* neuron,
                                    nimcp_ion_channel_type_t type,
                                    nimcp_ion_channel_t* channel) {
    NIMCP_CHECK_THROW(neuron, NIMCP_ERROR_NULL_POINTER, "neuron is NULL");
    NIMCP_CHECK_THROW(channel, NIMCP_ERROR_NULL_POINTER, "channel is NULL");
    NIMCP_CHECK_THROW(neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");
    NIMCP_CHECK_THROW(type < NIMCP_ION_CHANNEL_COUNT, NIMCP_ERROR_INVALID_PARAMETER,
                      "type %d >= NIMCP_ION_CHANNEL_COUNT", type);

    *channel = neuron->channels[type];
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_modulate_channel(nimcp_hh_neuron_t* neuron,
                                         nimcp_ion_channel_type_t type,
                                         float factor) {
    NIMCP_CHECK_THROW(neuron && neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");
    NIMCP_CHECK_THROW(type < NIMCP_ION_CHANNEL_COUNT, NIMCP_ERROR_INVALID_PARAMETER,
                      "type %d >= NIMCP_ION_CHANNEL_COUNT", type);
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 2.0f) factor = 2.0f;

    neuron->channels[type].modulation_factor = factor;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_set_channel_enabled(nimcp_hh_neuron_t* neuron,
                                            nimcp_ion_channel_type_t type,
                                            bool enable) {
    NIMCP_CHECK_THROW(neuron && neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");
    NIMCP_CHECK_THROW(type < NIMCP_ION_CHANNEL_COUNT, NIMCP_ERROR_INVALID_PARAMETER,
                      "type %d >= NIMCP_ION_CHANNEL_COUNT", type);

    neuron->channels[type].enabled = enable;
    return NIMCP_SUCCESS;
}

//=============================================================================
// Temperature
//=============================================================================

nimcp_error_t nimcp_hh_set_temperature(nimcp_hh_neuron_t* neuron, float temperature) {
    NIMCP_CHECK_THROW(neuron && neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");

    neuron->temperature = temperature;
    neuron->phi = powf(NIMCP_HH_Q10_RATE,
                       (temperature - NIMCP_HH_TEMPERATURE_REF) / 10.0f);
    return NIMCP_SUCCESS;
}

float nimcp_hh_get_phi(const nimcp_hh_neuron_t* neuron) {
    if (!neuron || !neuron->initialized) return 1.0f;
    return neuron->phi;
}

//=============================================================================
// Population
//=============================================================================

nimcp_error_t nimcp_hh_population_create(nimcp_hh_population_t* population,
                                          uint32_t count,
                                          const nimcp_hh_config_t* config) {
    NIMCP_CHECK_THROW(population, NIMCP_ERROR_NULL_POINTER, "population is NULL");
    NIMCP_CHECK_THROW(count > 0, NIMCP_ERROR_INVALID_PARAMETER, "count must be > 0");

    memset(population, 0, sizeof(*population));

    population->neurons = (nimcp_hh_neuron_t*)nimcp_calloc(count, sizeof(nimcp_hh_neuron_t));
    if (!population->neurons) {
        LOG_ERROR("Failed to allocate HH population neurons array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate HH population");
        return NIMCP_ERROR_NO_MEMORY;
    }

    population->count = count;
    population->capacity = count;

    /* Store default config */
    if (config) {
        population->default_config = *config;
    } else {
        nimcp_hh_config_default(&population->default_config);
    }

    /* Initialize all neurons */
    for (uint32_t i = 0; i < count; i++) {
        nimcp_error_t err = nimcp_hh_neuron_init(&population->neurons[i],
                                                  &population->default_config);
        if (err != NIMCP_SUCCESS) {
            nimcp_hh_population_destroy(population);
            return err;
        }
    }

    population->initialized = true;
    return NIMCP_SUCCESS;
}

void nimcp_hh_population_destroy(nimcp_hh_population_t* population) {
    if (!population) return;

    /* Destroy thread pool if present */
    if (population->thread_pool) {
        nimcp_pool_destroy(population->thread_pool);
        population->thread_pool = NULL;
    }

    if (population->neurons) {
        for (uint32_t i = 0; i < population->count; i++) {
            nimcp_hh_neuron_destroy(&population->neurons[i]);
        }
        nimcp_free(population->neurons);
    }

    memset(population, 0, sizeof(*population));
}

nimcp_error_t nimcp_hh_population_update(nimcp_hh_population_t* population,
                                          const float* I_ext,
                                          float dt) {
    NIMCP_CHECK_THROW(population && population->initialized, NIMCP_ERROR_NOT_INITIALIZED, "population not initialized");

    uint32_t spike_count = 0;
    float voltage_sum = 0.0f;

    for (uint32_t i = 0; i < population->count; i++) {
        float current = I_ext ? I_ext[i] : 0.0f;
        nimcp_hh_neuron_update(&population->neurons[i], current, dt);

        if (population->neurons[i].spiked) {
            spike_count++;
        }
        voltage_sum += population->neurons[i].V;
    }

    if (population->count > 0) {
        population->mean_voltage = voltage_sum / (float)population->count;
    }
    population->mean_firing_rate = (float)spike_count * 1000.0f / dt;  /* Convert to Hz */

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_population_get_rate(const nimcp_hh_population_t* population,
                                            float* rate) {
    NIMCP_CHECK_THROW(population, NIMCP_ERROR_NULL_POINTER, "population is NULL");
    NIMCP_CHECK_THROW(rate, NIMCP_ERROR_NULL_POINTER, "rate is NULL");
    NIMCP_CHECK_THROW(population->initialized, NIMCP_ERROR_NOT_INITIALIZED, "population not initialized");
    *rate = population->mean_firing_rate;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_population_get_synchrony(const nimcp_hh_population_t* population,
                                                 float* synchrony) {
    NIMCP_CHECK_THROW(population, NIMCP_ERROR_NULL_POINTER, "population is NULL");
    NIMCP_CHECK_THROW(synchrony, NIMCP_ERROR_NULL_POINTER, "synchrony is NULL");
    NIMCP_CHECK_THROW(population->initialized, NIMCP_ERROR_NOT_INITIALIZED, "population not initialized");
    *synchrony = population->synchrony;
    return NIMCP_SUCCESS;
}

//=============================================================================
// Parallel Population Update (Thread Pool)
//=============================================================================

/**
 * @brief Task data for parallel neuron update
 */
typedef struct {
    nimcp_hh_neuron_t* neurons;   /**< Pointer to neuron array */
    const float* currents;         /**< Pointer to current array */
    uint32_t start;                /**< Start index (inclusive) */
    uint32_t end;                  /**< End index (exclusive) */
    float dt;                      /**< Timestep */
    uint32_t spike_count;          /**< Output: spikes in this chunk */
    float voltage_sum;             /**< Output: sum of voltages */
} hh_parallel_task_t;

/**
 * @brief Task function for parallel neuron update
 */
static void hh_parallel_update_task(void* arg) {
    hh_parallel_task_t* task = (hh_parallel_task_t*)arg;

    task->spike_count = 0;
    task->voltage_sum = 0.0f;

    for (uint32_t i = task->start; i < task->end; i++) {
        float current = task->currents ? task->currents[i] : 0.0f;
        nimcp_hh_neuron_update(&task->neurons[i], current, task->dt);

        if (task->neurons[i].spiked) {
            task->spike_count++;
        }
        task->voltage_sum += task->neurons[i].V;
    }
}

/**
 * @brief Get number of CPU cores (for auto-detect)
 */
static uint32_t get_num_cores(void) {
#ifdef __linux__
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (uint32_t)n : 4;
#else
    return 4;  /* Default fallback */
#endif
}

nimcp_error_t nimcp_hh_population_update_parallel(
    nimcp_hh_population_t* population,
    const float* I_ext,
    float dt,
    uint32_t num_threads
) {
    NIMCP_CHECK_THROW(population && population->initialized, NIMCP_ERROR_NOT_INITIALIZED, "population not initialized");

    /* Auto-detect thread count */
    if (num_threads == 0) {
        num_threads = get_num_cores();
    }

    /* Limit threads to population size */
    if (num_threads > population->count) {
        num_threads = population->count;
    }

    /* Fall back to sequential for small populations or single thread */
    if (num_threads <= 1 || population->count < 100) {
        return nimcp_hh_population_update(population, I_ext, dt);
    }

    /* Check if population has its own thread pool */
    nimcp_thread_pool_t* pool = population->thread_pool;
    bool own_pool = false;

    if (!pool) {
        /* Create temporary thread pool */
        pool = nimcp_pool_create(num_threads);
        if (!pool) {
            /* Fall back to sequential */
            return nimcp_hh_population_update(population, I_ext, dt);
        }
        own_pool = true;
    }

    /* Allocate task structures */
    hh_parallel_task_t* tasks = (hh_parallel_task_t*)nimcp_calloc(
        num_threads, sizeof(hh_parallel_task_t)
    );
    if (!tasks) {
        LOG_ERROR("Failed to allocate parallel update tasks");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate parallel tasks");
        if (own_pool) nimcp_pool_destroy(pool);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Compute chunk sizes */
    uint32_t chunk_size = population->count / num_threads;
    uint32_t remainder = population->count % num_threads;

    /* Submit tasks */
    uint32_t start = 0;
    for (uint32_t i = 0; i < num_threads; i++) {
        uint32_t this_chunk = chunk_size + (i < remainder ? 1 : 0);

        tasks[i].neurons = population->neurons;
        tasks[i].currents = I_ext;
        tasks[i].start = start;
        tasks[i].end = start + this_chunk;
        tasks[i].dt = dt;
        tasks[i].spike_count = 0;
        tasks[i].voltage_sum = 0.0f;

        nimcp_pool_submit(pool, hh_parallel_update_task, &tasks[i]);
        start += this_chunk;
    }

    /* Wait for all tasks to complete */
    nimcp_pool_wait(pool);

    /* Aggregate results */
    uint32_t total_spikes = 0;
    float total_voltage = 0.0f;

    for (uint32_t i = 0; i < num_threads; i++) {
        total_spikes += tasks[i].spike_count;
        total_voltage += tasks[i].voltage_sum;
    }

    /* Update population statistics */
    if (population->count > 0) {
        population->mean_voltage = total_voltage / (float)population->count;
    }
    population->mean_firing_rate = (float)total_spikes * 1000.0f / dt;

    /* Cleanup */
    nimcp_free(tasks);
    if (own_pool) {
        nimcp_pool_destroy(pool);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_population_create_parallel(
    nimcp_hh_population_t* population,
    uint32_t count,
    const nimcp_hh_config_t* config,
    uint32_t num_threads
) {
    /* First create the population normally */
    nimcp_error_t err = nimcp_hh_population_create(population, count, config);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    /* Auto-detect thread count */
    if (num_threads == 0) {
        num_threads = get_num_cores();
    }

    /* Only create pool for sufficiently large populations */
    if (count >= 100 && num_threads > 1) {
        population->thread_pool = nimcp_pool_create(num_threads);
        if (population->thread_pool) {
            population->pool_threads = num_threads;
        }
        /* Note: If pool creation fails, we silently continue without pool */
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Analysis
//=============================================================================

nimcp_error_t nimcp_hh_compute_fi_curve(nimcp_hh_neuron_t* neuron,
                                         float I_min, float I_max,
                                         uint32_t num_points,
                                         float* currents, float* rates) {
    NIMCP_CHECK_THROW(neuron, NIMCP_ERROR_NULL_POINTER, "neuron is NULL");
    NIMCP_CHECK_THROW(currents, NIMCP_ERROR_NULL_POINTER, "currents is NULL");
    NIMCP_CHECK_THROW(rates, NIMCP_ERROR_NULL_POINTER, "rates is NULL");
    NIMCP_CHECK_THROW(neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");
    NIMCP_CHECK_THROW(num_points > 1, NIMCP_ERROR_INVALID_PARAMETER, "num_points must be > 1");

    float step = (I_max - I_min) / (float)(num_points - 1);
    float sim_time = 500.0f;  /* ms per current level */
    float dt = 0.025f;

    for (uint32_t i = 0; i < num_points; i++) {
        float I = I_min + step * (float)i;
        currents[i] = I;

        /* Reset neuron */
        nimcp_hh_neuron_reset(neuron);

        /* Simulate */
        uint32_t spikes = 0;
        for (float t = 0.0f; t < sim_time; t += dt) {
            nimcp_hh_neuron_update(neuron, I, dt);
            if (neuron->spiked) spikes++;
        }

        rates[i] = (float)spikes * 1000.0f / sim_time;  /* Hz */
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_compute_rheobase(nimcp_hh_neuron_t* neuron, float* rheobase) {
    NIMCP_CHECK_THROW(neuron, NIMCP_ERROR_NULL_POINTER, "neuron is NULL");
    NIMCP_CHECK_THROW(rheobase, NIMCP_ERROR_NULL_POINTER, "rheobase is NULL");
    NIMCP_CHECK_THROW(neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");

    float I_low = 0.0f;
    float I_high = 50.0f;  /* uA/cm^2 */
    float tolerance = 0.1f;
    float dt = 0.025f;
    float sim_time = 200.0f;

    while (I_high - I_low > tolerance) {
        float I_mid = (I_low + I_high) / 2.0f;

        nimcp_hh_neuron_reset(neuron);
        bool spiked = false;

        for (float t = 0.0f; t < sim_time && !spiked; t += dt) {
            nimcp_hh_neuron_update(neuron, I_mid, dt);
            if (neuron->spiked) spiked = true;
        }

        if (spiked) {
            I_high = I_mid;
        } else {
            I_low = I_mid;
        }
    }

    *rheobase = (I_low + I_high) / 2.0f;
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_hh_get_stats(const nimcp_hh_neuron_t* neuron,
                                  float* firing_rate, float* tau_m, float* R_in) {
    NIMCP_CHECK_THROW(neuron, NIMCP_ERROR_NULL_POINTER, "neuron is NULL");
    NIMCP_CHECK_THROW(neuron->initialized, NIMCP_ERROR_NOT_INITIALIZED, "neuron not initialized");

    if (firing_rate) *firing_rate = neuron->avg_firing_rate;
    if (tau_m) *tau_m = neuron->membrane_time_constant;
    if (R_in) *R_in = neuron->input_resistance;

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * W15: HH aggregated KG emission + read path
 *
 * Rate-limiting: The caller is responsible for aggregating spikes before
 * calling the trigger. A trigger with spike_count==0 when kind is not a
 * mode-transition string is rejected. Never call from the per-neuron
 * update loop.
 *===========================================================================*/

#define HH_KG_ROOT_NAME "hodgkin_huxley_runtime"
#define HH_KG_MIN_SPIKE_BATCH 100u  /* Minimum spikes between rate events */

static brain_t s_hh_kg_brain = NULL;

static bool hh_kg_kind_is_mode_transition(const char* kind) {
    if (!kind) return false;
    return (strstr(kind, "mode_") != NULL) ||
           (strstr(kind, "rheobase") != NULL) ||
           (strstr(kind, "quiescent") != NULL) ||
           (strstr(kind, "tonic") != NULL) ||
           (strstr(kind, "bursting") != NULL);
}

void nimcp_hh_kg_register_brain(brain_t brain) {
    s_hh_kg_brain = brain;
}

void nimcp_hh_kg_trigger_mode_event(brain_t brain,
                                    const char* kind,
                                    float firing_rate,
                                    uint32_t spike_count,
                                    uint64_t ts_us) {
    if (!brain) brain = s_hh_kg_brain;
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    if (!kind) return;

    /* Rate limit: require either a mode-transition kind OR a batch >= threshold. */
    if (!hh_kg_kind_is_mode_transition(kind) &&
        spike_count < HH_KG_MIN_SPIKE_BATCH) {
        return;
    }

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    /* Ensure the HH runtime root node exists (idempotent). */
    brain_kg_node_id_t root = brain_kg_find_node(kg, HH_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) {
        root = brain_kg_add_node(kg, HH_KG_ROOT_NAME,
            BRAIN_KG_NODE_CORE,
            "Hodgkin-Huxley runtime aggregator - mode/rate events");
        /* Hook to the static physics_layer subgraph if present. */
        brain_kg_node_id_t phys = brain_kg_find_node(kg, "hodgkin_huxley");
        if (root != BRAIN_KG_INVALID_NODE && phys != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, phys, root,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "runtime aggregator for biophysics HH", 1.0f);
        }
    }

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "hodgkin_huxley_event_%s_%llu",
             kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "HH aggregated event: kind=%s rate=%.2fHz spike_count=%u",
             kind, firing_rate, spike_count);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
            "hh_runtime_event",
            firing_rate > 0.0f ? firing_rate : 1.0f);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.3f", firing_rate);
        brain_kg_add_metadata(kg, ev, "firing_rate_hz", buf);
        snprintf(buf, sizeof(buf), "%u", spike_count);
        brain_kg_add_metadata(kg, ev, "spike_count", buf);
        brain_kg_add_metadata(kg, ev, "kind", kind);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

uint32_t nimcp_hh_kg_count_events(struct brain_kg* kg_opaque,
                                  const char* kind_substr) {
    brain_kg_t* kg = (brain_kg_t*)kg_opaque;
    if (!kg) return 0;
    brain_kg_node_id_t root = brain_kg_find_node(kg, HH_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out = brain_kg_get_outgoing(kg, root);
    if (!out) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < out->count; ++i) {
        const brain_kg_edge_t* e = out->edges[i];
        if (!e) continue;
        const brain_kg_node_t* to = brain_kg_get_node(kg, e->to);
        if (!to) continue;
        if (!kind_substr || !*kind_substr) {
            count++;
        } else if ((to->name && strstr(to->name, kind_substr)) ||
                   (to->description && strstr(to->description, kind_substr))) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(out);
    return count;
}

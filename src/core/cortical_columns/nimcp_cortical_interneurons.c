/**
 * @file nimcp_cortical_interneurons.c
 * @brief Cortical Interneuron System - Core Implementation
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Implements the five major cortical GABAergic interneuron classes with
 *       biologically-calibrated dynamics, E/I balance tracking, gamma oscillation
 *       power computation, and VIP-mediated disinhibition.
 * WHY:  Cortical computation requires diverse inhibitory circuits for timing
 *       control, gain modulation, prediction error, and attentional gating.
 * HOW:  Each interneuron type has type-specific firing dynamics, membrane
 *       potential evolution with refractory periods, and contribution to
 *       system-level metrics (gamma, E/I, disinhibition, prediction error).
 */

#include "core/cortical_columns/nimcp_cortical_interneurons.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "CORTICAL_INTERNEURONS"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cortical_interneurons, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ============================================================================
 * Type-Specific Default Parameters
 * ============================================================================ */

/** Biological defaults per interneuron type */
typedef struct {
    float resting_potential;    /* mV */
    float threshold;            /* mV */
    float max_firing_rate;      /* Hz */
    float base_inhibition;      /* [0-1] */
    float refractory_period;    /* ms */
    uint32_t default_targets;   /* number of postsynaptic targets */
} cint_type_defaults_t;

static const cint_type_defaults_t TYPE_DEFAULTS[CINT_TYPE_COUNT] = {
    /* PV_BASKET: Fast-spiking, perisomatic, gamma generation */
    [CINT_PV_BASKET] = {
        .resting_potential = -70.0f,
        .threshold         = -40.0f,
        .max_firing_rate   = 300.0f,
        .base_inhibition   = 0.8f,
        .refractory_period = 1.0f,   /* Very short, enables high-frequency firing */
        .default_targets   = 200
    },
    /* PV_CHANDELIER: Axo-axonic, gates AP at AIS */
    [CINT_PV_CHANDELIER] = {
        .resting_potential = -68.0f,
        .threshold         = -42.0f,
        .max_firing_rate   = 150.0f,
        .base_inhibition   = 0.9f,   /* Strong veto capability */
        .refractory_period = 2.0f,
        .default_targets   = 50      /* Fewer targets but high impact */
    },
    /* SST_MARTINOTTI: Dendrite-targeting, L5->L1 feedback */
    [CINT_SST_MARTINOTTI] = {
        .resting_potential = -65.0f,
        .threshold         = -45.0f,
        .max_firing_rate   = 80.0f,
        .base_inhibition   = 0.5f,
        .refractory_period = 5.0f,
        .default_targets   = 100
    },
    /* VIP: Disinhibitory, attention gating */
    [CINT_VIP] = {
        .resting_potential = -66.0f,
        .threshold         = -44.0f,
        .max_firing_rate   = 60.0f,
        .base_inhibition   = 0.4f,   /* Inhibits other interneurons */
        .refractory_period = 4.0f,
        .default_targets   = 30      /* Targets are SST/PV interneurons */
    },
    /* NGF_L1: Volume transmission, slow tonic inhibition */
    [CINT_NGF_L1] = {
        .resting_potential = -64.0f,
        .threshold         = -46.0f,
        .max_firing_rate   = 40.0f,
        .base_inhibition   = 0.3f,
        .refractory_period = 8.0f,
        .default_targets   = 500     /* Broad via volume transmission */
    }
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Clamp a float value to [min, max]
 */
static float cint_clampf(float val, float lo, float hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/**
 * @brief Initialize a single interneuron with type-specific defaults
 */
static void init_interneuron(interneuron_state_t* neuron,
                             cortical_interneuron_type_t type)
{
    const cint_type_defaults_t* defaults = &TYPE_DEFAULTS[type];

    neuron->type                = type;
    neuron->firing_rate         = 0.0f;
    neuron->inhibition_strength = defaults->base_inhibition;
    neuron->membrane_potential  = defaults->resting_potential;
    neuron->threshold           = defaults->threshold;
    neuron->refractory_ms       = 0.0f;
    neuron->target_count        = defaults->default_targets;
    neuron->ei_ratio            = 0.0f;
}

/**
 * @brief Update a single interneuron's dynamics for one timestep
 *
 * WHAT: Advance membrane potential, check threshold, apply refractory period
 * WHY:  Each interneuron evolves according to leaky integrate-and-fire dynamics
 * HOW:  Leaky integration with type-specific time constants
 */
static void update_interneuron(interneuron_state_t* neuron, float dt_s)
{
    const cint_type_defaults_t* defaults = &TYPE_DEFAULTS[neuron->type];
    float dt_ms = dt_s * 1000.0f;

    /* Handle refractory period */
    if (neuron->refractory_ms > 0.0f) {
        neuron->refractory_ms -= dt_ms;
        if (neuron->refractory_ms < 0.0f) {
            neuron->refractory_ms = 0.0f;
        }
        /* During refractory: membrane returns toward resting */
        float tau_refract = 5.0f; /* ms */
        float decay = expf(-dt_ms / tau_refract);
        neuron->membrane_potential = defaults->resting_potential +
            (neuron->membrane_potential - defaults->resting_potential) * decay;
        return;
    }

    /* Leaky integration: dV/dt = -(V - V_rest) / tau + I_ext */
    float tau_membrane = 10.0f; /* ms, fast for interneurons */
    if (neuron->type == CINT_PV_BASKET || neuron->type == CINT_PV_CHANDELIER) {
        tau_membrane = 5.0f; /* PV cells have faster membrane dynamics */
    }

    float leak_decay = expf(-dt_ms / tau_membrane);
    /* Stochastic drive simulates synaptic input (simplified) */
    float drive = defaults->resting_potential + 30.0f * neuron->inhibition_strength;
    neuron->membrane_potential = drive +
        (neuron->membrane_potential - drive) * leak_decay;

    /* Threshold crossing -> spike */
    if (neuron->membrane_potential >= neuron->threshold) {
        /* Spike occurred */
        neuron->refractory_ms = defaults->refractory_period;
        neuron->membrane_potential = defaults->resting_potential - 10.0f; /* After-hyperpolarization */

        /* Update firing rate with EMA */
        float inst_rate = 1000.0f / (dt_ms + defaults->refractory_period);
        if (!isfinite(inst_rate)) {
            inst_rate = 0.0f;
        }
        inst_rate = cint_clampf(inst_rate, 0.0f, defaults->max_firing_rate);
        float alpha = cint_clampf(dt_s * 2.0f, 0.01f, 0.5f);
        neuron->firing_rate = (1.0f - alpha) * neuron->firing_rate + alpha * inst_rate;
    } else {
        /* No spike: decay firing rate */
        float decay_alpha = cint_clampf(dt_s * 0.5f, 0.01f, 0.3f);
        neuron->firing_rate *= (1.0f - decay_alpha);
    }

    /* isfinite guard on firing rate */
    if (!isfinite(neuron->firing_rate)) {
        neuron->firing_rate = 0.0f;
    }

    /* Update inhibition strength based on firing rate */
    neuron->inhibition_strength = defaults->base_inhibition *
        (neuron->firing_rate / (defaults->max_firing_rate + 1.0f));
    neuron->inhibition_strength = cint_clampf(neuron->inhibition_strength, 0.0f, 1.0f);

    /* E/I ratio contribution: how much this interneuron contributes to inhibition */
    neuron->ei_ratio = neuron->inhibition_strength * (float)neuron->target_count;
}

/**
 * @brief Compute gamma power from PV basket cell population
 *
 * WHAT: Estimate gamma oscillation power from PV basket cell synchrony
 * WHY:  PV basket cells are the primary generators of cortical gamma (30-80 Hz)
 * HOW:  Gamma power ~ mean PV basket firing rate normalized to gamma band
 */
static float compute_gamma_power(const cortical_interneuron_system_t* system)
{
    float sum_pv_rate = 0.0f;
    uint32_t pv_count = 0;

    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        if (system->interneurons[i].type == CINT_PV_BASKET) {
            sum_pv_rate += system->interneurons[i].firing_rate;
            pv_count++;
        }
    }

    if (pv_count == 0) return 0.0f;

    float avg_rate = sum_pv_rate / (float)pv_count;

    /* Gamma power is highest when PV cells fire within gamma band (30-80 Hz) */
    float gamma_center = (CINT_GAMMA_LOW_HZ + CINT_GAMMA_HIGH_HZ) / 2.0f;
    float gamma_width = (CINT_GAMMA_HIGH_HZ - CINT_GAMMA_LOW_HZ) / 2.0f;
    float distance = fabsf(avg_rate - gamma_center);
    float power = expf(-(distance * distance) / (2.0f * gamma_width * gamma_width));

    /* Scale by firing rate magnitude */
    float rate_factor = avg_rate / (gamma_center + 1.0f);
    rate_factor = cint_clampf(rate_factor, 0.0f, 1.5f);
    power *= rate_factor;

    return cint_clampf(power, 0.0f, 1.0f);
}

/**
 * @brief Compute E/I balance from all interneurons
 *
 * WHAT: Calculate overall excitation/inhibition ratio
 * WHY:  E/I balance is fundamental to cortical stability
 * HOW:  Sum total inhibitory output, compare to target excitatory level
 */
static float compute_ei_balance(const cortical_interneuron_system_t* system)
{
    float total_inhibition = 0.0f;

    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        total_inhibition += system->interneurons[i].inhibition_strength;
    }

    /* Normalize: target is 1/ei_ratio of total excitation */
    if (total_inhibition <= 0.0f) {
        return system->config.target_ei_ratio * 2.0f; /* No inhibition -> very high E/I */
    }

    /* E/I ratio = excitatory drive / inhibitory drive */
    /* Assume unit excitatory drive; inhibition is sum of strengths */
    float normalized_inhibition = total_inhibition / (float)system->num_interneurons;
    if (!isfinite(normalized_inhibition) || normalized_inhibition <= 0.0f) {
        return system->config.target_ei_ratio * 2.0f;
    }

    float ei = 1.0f / normalized_inhibition;
    return cint_clampf(ei, 0.1f, 20.0f);
}

/**
 * @brief Compute VIP-mediated disinhibition level
 *
 * WHAT: Measure how much VIP cells are releasing pyramidal cells from inhibition
 * WHY:  VIP-mediated disinhibition is the primary mechanism for attention gating
 * HOW:  Higher VIP firing = more inhibition of SST/PV = less pyramidal inhibition
 */
static float compute_disinhibition(const cortical_interneuron_system_t* system)
{
    float sum_vip_rate = 0.0f;
    uint32_t vip_count = 0;

    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        if (system->interneurons[i].type == CINT_VIP) {
            sum_vip_rate += system->interneurons[i].firing_rate;
            vip_count++;
        }
    }

    if (vip_count == 0) return 0.0f;

    float avg_vip_rate = sum_vip_rate / (float)vip_count;
    float max_vip_rate = TYPE_DEFAULTS[CINT_VIP].max_firing_rate;

    /* Disinhibition proportional to VIP firing relative to max */
    float disinhibition = avg_vip_rate / (max_vip_rate + 1.0f);
    return cint_clampf(disinhibition, 0.0f, 1.0f);
}

/**
 * @brief Compute prediction error from SST Martinotti cells
 *
 * WHAT: Derive prediction error signal from SST cell activity
 * WHY:  SST cells provide feedback inhibition encoding prediction error
 * HOW:  Elevated SST firing indicates mismatch between prediction and input
 */
static float compute_prediction_error(const cortical_interneuron_system_t* system)
{
    float sum_sst_rate = 0.0f;
    uint32_t sst_count = 0;

    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        if (system->interneurons[i].type == CINT_SST_MARTINOTTI) {
            sum_sst_rate += system->interneurons[i].firing_rate;
            sst_count++;
        }
    }

    if (sst_count == 0) return 0.0f;

    float avg_sst_rate = sum_sst_rate / (float)sst_count;
    float max_sst_rate = TYPE_DEFAULTS[CINT_SST_MARTINOTTI].max_firing_rate;

    /* Prediction error proportional to SST firing relative to max */
    float pe = avg_sst_rate / (max_sst_rate + 1.0f);
    return cint_clampf(pe, 0.0f, 1.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int cint_default_config(cint_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_default_config: config is NULL");
        return -1;
    }

    config->num_pv_basket    = CINT_DEFAULT_PV_BASKET;
    config->num_pv_chandelier = CINT_DEFAULT_PV_CHANDELIER;
    config->num_sst          = CINT_DEFAULT_SST;
    config->num_vip          = CINT_DEFAULT_VIP;
    config->num_ngf          = CINT_DEFAULT_NGF;
    config->target_ei_ratio  = CINT_TARGET_EI_RATIO;

    return 0;
}

cortical_interneuron_system_t* cint_create(const cint_config_t* config)
{
    /* Use defaults if no config provided */
    cint_config_t default_cfg;
    if (!config) {
        cint_default_config(&default_cfg);
        config = &default_cfg;
    }

    /* Validate configuration */
    if (config->target_ei_ratio <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "cint_create: target_ei_ratio must be positive");
        return NULL;
    }

    /* Allocate system */
    cortical_interneuron_system_t* system =
        (cortical_interneuron_system_t*)nimcp_calloc(1, sizeof(cortical_interneuron_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cint_create: failed to allocate system");
        return NULL;
    }

    system->magic = CINT_MAGIC;
    memcpy(&system->config, config, sizeof(cint_config_t));

    /* Compute total interneuron count */
    system->num_interneurons = config->num_pv_basket +
                               config->num_pv_chandelier +
                               config->num_sst +
                               config->num_vip +
                               config->num_ngf;

    if (system->num_interneurons == 0) {
        NIMCP_LOGGING_WARN("cint_create: zero interneurons requested");
        system->gamma_power = 0.0f;
        system->ei_balance = config->target_ei_ratio;
        system->disinhibition_level = 0.0f;
        system->prediction_error = 0.0f;
        system->last_update_us = get_current_time_us();
        system->lock = nimcp_mutex_create(NULL);
        return system;
    }

    /* Allocate interneuron array */
    system->interneurons = (interneuron_state_t*)nimcp_calloc(
        system->num_interneurons, sizeof(interneuron_state_t));
    if (!system->interneurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "cint_create: failed to allocate interneuron array");
        nimcp_free(system);
        return NULL;
    }

    /* Initialize interneurons by type */
    uint32_t idx = 0;

    for (uint32_t i = 0; i < config->num_pv_basket && idx < system->num_interneurons; i++, idx++) {
        init_interneuron(&system->interneurons[idx], CINT_PV_BASKET);
    }
    for (uint32_t i = 0; i < config->num_pv_chandelier && idx < system->num_interneurons; i++, idx++) {
        init_interneuron(&system->interneurons[idx], CINT_PV_CHANDELIER);
    }
    for (uint32_t i = 0; i < config->num_sst && idx < system->num_interneurons; i++, idx++) {
        init_interneuron(&system->interneurons[idx], CINT_SST_MARTINOTTI);
    }
    for (uint32_t i = 0; i < config->num_vip && idx < system->num_interneurons; i++, idx++) {
        init_interneuron(&system->interneurons[idx], CINT_VIP);
    }
    for (uint32_t i = 0; i < config->num_ngf && idx < system->num_interneurons; i++, idx++) {
        init_interneuron(&system->interneurons[idx], CINT_NGF_L1);
    }

    /* Initialize derived metrics */
    system->gamma_power = 0.0f;
    system->ei_balance = config->target_ei_ratio;
    system->disinhibition_level = 0.0f;
    system->prediction_error = 0.0f;
    system->last_update_us = get_current_time_us();

    /* Initialize statistics */
    memset(&system->stats, 0, sizeof(cint_stats_t));

    /* Create mutex */
    system->lock = nimcp_mutex_create(NULL);
    if (!system->lock) {
        NIMCP_LOGGING_WARN("cint_create: mutex creation failed, continuing without thread safety");
    }

    LOG_INFO(LOG_MODULE, "Cortical interneuron system created: %u total "
             "(PV_basket=%u, PV_chandelier=%u, SST=%u, VIP=%u, NGF=%u), target E/I=%.1f",
             system->num_interneurons,
             config->num_pv_basket, config->num_pv_chandelier,
             config->num_sst, config->num_vip, config->num_ngf,
             config->target_ei_ratio);

    return system;
}

void cint_destroy(cortical_interneuron_system_t* system)
{
    if (!system) return;

    /* Validate magic */
    if (system->magic != CINT_MAGIC) {
        NIMCP_LOGGING_WARN("cint_destroy: invalid magic (possible double-free or corruption)");
        return;
    }

    /* Invalidate magic to detect double-free */
    system->magic = 0;

    /* Free interneuron array */
    if (system->interneurons) {
        nimcp_free(system->interneurons);
        system->interneurons = NULL;
    }

    /* Destroy mutex */
    if (system->lock) {
        nimcp_mutex_destroy(system->lock);
        system->lock = NULL;
    }

    nimcp_free(system);

    LOG_INFO(LOG_MODULE, "Cortical interneuron system destroyed");
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int cint_update(cortical_interneuron_system_t* system, float dt_s)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_update: system is NULL");
        return -1;
    }

    if (system->magic != CINT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "cint_update: invalid magic");
        return -1;
    }

    if (!isfinite(dt_s) || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "cint_update: dt_s must be positive and finite");
        return -1;
    }

    /* Lock for thread safety */
    if (system->lock) {
        nimcp_mutex_lock(system->lock);
    }

    /* Update all interneurons */
    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        update_interneuron(&system->interneurons[i], dt_s);
    }

    /* Recompute derived metrics */
    system->gamma_power = compute_gamma_power(system);
    system->ei_balance = compute_ei_balance(system);
    system->disinhibition_level = compute_disinhibition(system);
    system->prediction_error = compute_prediction_error(system);
    system->last_update_us = get_current_time_us();

    /* isfinite guards on derived metrics */
    if (!isfinite(system->gamma_power)) system->gamma_power = 0.0f;
    if (!isfinite(system->ei_balance)) system->ei_balance = system->config.target_ei_ratio;
    if (!isfinite(system->disinhibition_level)) system->disinhibition_level = 0.0f;
    if (!isfinite(system->prediction_error)) system->prediction_error = 0.0f;

    /* Update statistics */
    system->stats.total_updates++;

    /* Compute per-type average firing rates for stats */
    float sum_pv = 0.0f, sum_sst = 0.0f, sum_vip = 0.0f;
    uint32_t cnt_pv = 0, cnt_sst = 0, cnt_vip = 0;

    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        switch (system->interneurons[i].type) {
            case CINT_PV_BASKET:
            case CINT_PV_CHANDELIER:
                sum_pv += system->interneurons[i].firing_rate;
                cnt_pv++;
                break;
            case CINT_SST_MARTINOTTI:
                sum_sst += system->interneurons[i].firing_rate;
                cnt_sst++;
                break;
            case CINT_VIP:
                sum_vip += system->interneurons[i].firing_rate;
                cnt_vip++;
                break;
            default:
                break;
        }
    }

    /* EMA for stats (alpha=0.1 for smoothing) */
    float alpha = 0.1f;
    if (cnt_pv > 0) {
        float avg = sum_pv / (float)cnt_pv;
        system->stats.avg_pv_firing_rate =
            (1.0f - alpha) * system->stats.avg_pv_firing_rate + alpha * avg;
    }
    if (cnt_sst > 0) {
        float avg = sum_sst / (float)cnt_sst;
        system->stats.avg_sst_firing_rate =
            (1.0f - alpha) * system->stats.avg_sst_firing_rate + alpha * avg;
    }
    if (cnt_vip > 0) {
        float avg = sum_vip / (float)cnt_vip;
        system->stats.avg_vip_firing_rate =
            (1.0f - alpha) * system->stats.avg_vip_firing_rate + alpha * avg;
    }

    if (system->gamma_power > system->stats.peak_gamma_power) {
        system->stats.peak_gamma_power = system->gamma_power;
    }
    if (system->stats.min_ei_balance == 0.0f || system->ei_balance < system->stats.min_ei_balance) {
        system->stats.min_ei_balance = system->ei_balance;
    }
    if (system->ei_balance > system->stats.max_ei_balance) {
        system->stats.max_ei_balance = system->ei_balance;
    }

    /* Unlock */
    if (system->lock) {
        nimcp_mutex_unlock(system->lock);
    }

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

float cint_get_gamma_power(const cortical_interneuron_system_t* system)
{
    if (!system || system->magic != CINT_MAGIC) return 0.0f;
    return system->gamma_power;
}

float cint_get_ei_balance(const cortical_interneuron_system_t* system)
{
    if (!system || system->magic != CINT_MAGIC) return -1.0f;
    return system->ei_balance;
}

float cint_get_disinhibition(const cortical_interneuron_system_t* system)
{
    if (!system || system->magic != CINT_MAGIC) return 0.0f;
    return system->disinhibition_level;
}

float cint_get_prediction_error(const cortical_interneuron_system_t* system)
{
    if (!system || system->magic != CINT_MAGIC) return 0.0f;
    return system->prediction_error;
}

/* ============================================================================
 * Modulation Implementation
 * ============================================================================ */

int cint_modulate_attention(cortical_interneuron_system_t* system,
                            float attention_level)
{
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_modulate_attention: system is NULL");
        return -1;
    }

    if (system->magic != CINT_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "cint_modulate_attention: invalid magic");
        return -1;
    }

    if (!isfinite(attention_level)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "cint_modulate_attention: attention_level not finite");
        return -1;
    }

    attention_level = cint_clampf(attention_level, 0.0f, 1.0f);

    /* Lock for thread safety */
    if (system->lock) {
        nimcp_mutex_lock(system->lock);
    }

    /* Attention activates VIP cells -> disinhibition of pyramidal cells
     * Also enhances PV basket cells -> stronger gamma oscillations */
    for (uint32_t i = 0; i < system->num_interneurons; i++) {
        interneuron_state_t* neuron = &system->interneurons[i];

        switch (neuron->type) {
            case CINT_VIP:
                /* VIP cells driven by top-down attention */
                /* Depolarize membrane proportional to attention */
                neuron->membrane_potential += attention_level * 15.0f;
                neuron->membrane_potential = cint_clampf(
                    neuron->membrane_potential, -80.0f, 0.0f);
                break;

            case CINT_PV_BASKET:
                /* PV basket cells enhanced by attention -> stronger gamma */
                neuron->membrane_potential += attention_level * 10.0f;
                neuron->membrane_potential = cint_clampf(
                    neuron->membrane_potential, -80.0f, 0.0f);
                break;

            case CINT_SST_MARTINOTTI:
                /* SST cells suppressed by VIP disinhibition during attention */
                neuron->membrane_potential -= attention_level * 8.0f;
                neuron->membrane_potential = cint_clampf(
                    neuron->membrane_potential, -80.0f, 0.0f);
                break;

            case CINT_PV_CHANDELIER:
                /* Chandelier cells moderately enhanced */
                neuron->membrane_potential += attention_level * 5.0f;
                neuron->membrane_potential = cint_clampf(
                    neuron->membrane_potential, -80.0f, 0.0f);
                break;

            case CINT_NGF_L1:
                /* NGF cells relatively unaffected by attention */
                break;

            default:
                break;
        }
    }

    /* Unlock */
    if (system->lock) {
        nimcp_mutex_unlock(system->lock);
    }

    return 0;
}

int cint_get_stats(const cortical_interneuron_system_t* system,
                   cint_stats_t* stats)
{
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cint_get_stats: system or stats is NULL");
        return -1;
    }

    memcpy(stats, &system->stats, sizeof(cint_stats_t));
    return 0;
}

void cint_reset_stats(cortical_interneuron_system_t* system)
{
    if (!system) return;
    memset(&system->stats, 0, sizeof(cint_stats_t));
}

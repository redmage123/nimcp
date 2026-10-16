/**
 * @file nimcp_brain_immune_plasticity.c
 * @brief Implementation of brain immune-plasticity integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Connects immune system state to plasticity modulation
 * WHY:  Immune activation affects learning and synaptic plasticity
 * HOW:  Compute modulation factors from cytokines/inflammation, apply to plasticity
 *
 * IMPLEMENTATION PRINCIPLES:
 * - Functions < 50 lines
 * - Guard clauses for early returns
 * - WHAT-WHY-HOW comments
 * - Single responsibility per function
 */

#include "cognitive/immune/nimcp_brain_immune_plasticity.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "immune_plasticity"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(brain_immune_plasticity, MESH_ADAPTER_CATEGORY_SECURITY)



/* ============================================================================
 * Global Statistics
 * ============================================================================ */

static immune_plasticity_stats_t g_stats = {0};
static pthread_mutex_t g_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int immune_plasticity_default_config(immune_plasticity_config_t* config) {
    /* WHAT: Provide research-based default parameters
     * WHY:  Based on neuroimmune literature values
     * HOW:  Initialize struct with biological parameters
     */
    if (!config) {
        LOG_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_default_config: config is NULL");
        return -1;
    }

    /* Cytokine sensitivity (from neuroimmune research) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_de", 0.0f);


    config->il1_threshold_sensitivity = 1.5f;    /* IL-1β elevates threshold 1.5x at max */
    config->il6_timing_sensitivity = 0.5f;       /* IL-6 narrows windows to 50% at max */
    config->tnf_attention_sensitivity = 0.7f;    /* TNF-α reduces attention to 30% at max */
    config->il10_recovery_rate = 0.8f;           /* IL-10 restores 80% function */

    /* Inflammation thresholds */
    config->local_inflammation_threshold = 0.3f;     /* 30% for local effects */
    config->regional_inflammation_threshold = 0.6f;  /* 60% for regional */
    config->systemic_inflammation_threshold = 0.8f;  /* 80% for systemic */

    /* Modulation bounds */
    config->min_plasticity_factor = 0.1f;        /* Severe inflammation: 10% plasticity */
    config->max_threshold_elevation = 2.0f;      /* Max 2x BCM threshold */
    config->max_timing_narrowing = 0.3f;         /* Min 30% STDP window width */
    config->max_attention_impairment = 0.2f;     /* Min 20% attention strength */

    return 0;
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Clamp float value to range [min, max]
 * WHY:  Prevent modulation factors from going out of bounds
 * HOW:  Use fminf/fmaxf for branchless clamping
 */
static inline float clamp_f(float val, float min_val, float max_val) {
    return fminf(fmaxf(val, min_val), max_val);
}

/**
 * WHAT: Compute inflammation severity as normalized factor
 * WHY:  Convert enum to continuous value for calculations
 * HOW:  Map inflammation levels to 0-1 scale
 */
static float inflammation_to_factor(brain_inflammation_level_t level) {
    switch (level) {
        case INFLAMMATION_NONE:     return 0.0f;
        case INFLAMMATION_LOCAL:    return 0.25f;
        case INFLAMMATION_REGIONAL: return 0.5f;
        case INFLAMMATION_SYSTEMIC: return 0.75f;
        case INFLAMMATION_STORM:    return 1.0f;
        default:                    return 0.0f;
    }
}

/* ============================================================================
 * Modulation Computation API
 * ============================================================================ */

float immune_plasticity_get_cytokine_concentration(
    const brain_immune_system_t* immune_system,
    brain_cytokine_type_t cytokine_type)
{
    /* WHAT: Sum concentrations of specific cytokine type
     * WHY:  Multiple cytokine releases accumulate
     * HOW:  Iterate cytokines, sum matching types
     */
    if (!immune_system) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_ge", 0.0f);


    float total = 0.0f;
    for (size_t i = 0; i < immune_system->cytokine_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune_system->cytokine_count > 256) {
            brain_immune_plasticity_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)immune_system->cytokine_count);
        }

        const brain_cytokine_t* cyt = &immune_system->cytokines[i];
        if (cyt->type == cytokine_type) {
            total += cyt->concentration;
        }
    }

    /* Clamp to [0, 1] */
    return fminf(total, 1.0f);
}

brain_inflammation_level_t immune_plasticity_get_max_inflammation(
    const brain_immune_system_t* immune_system)
{
    /* WHAT: Find highest inflammation level in system
     * WHY:  Most severe inflammation determines effects
     * HOW:  Scan sites, track maximum
     */
    if (!immune_system || immune_system->inflammation_count == 0) {
        return INFLAMMATION_NONE;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_ge", 0.0f);


    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune_system->inflammation_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && immune_system->inflammation_count > 256) {
            brain_immune_plasticity_heartbeat("brain_immune_loop",
                             (float)(i + 1) / (float)immune_system->inflammation_count);
        }

        const brain_inflammation_site_t* site = &immune_system->inflammation_sites[i];
        if (site->level > max_level) {
            max_level = site->level;
        }
    }

    return max_level;
}

int immune_plasticity_compute_modulation(
    const brain_immune_system_t* immune_system,
    const immune_plasticity_config_t* config,
    immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Compute all plasticity modulation factors from immune state
     * WHY:  Central conversion of immune state to plasticity effects
     * HOW:  Read cytokines and inflammation, compute each factor
     */

    /* Guard: Validate inputs */
    if (!immune_system || !config || !modulation) {
        LOG_ERROR("NULL pointer in compute_modulation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_compute_modulation: required parameter is NULL (immune_system, config, modulation)");
        return -1;
    }

    /* Initialize to baseline (no modulation) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_co", 0.0f);


    memset(modulation, 0, sizeof(immune_plasticity_modulation_t));

    /* BCM */
    modulation->bcm_threshold_scale = 1.0f;
    modulation->bcm_learning_rate_scale = 1.0f;

    /* STDP */
    modulation->stdp_tau_plus_scale = 1.0f;
    modulation->stdp_tau_minus_scale = 1.0f;
    modulation->stdp_learning_rate_scale = 1.0f;

    /* STP */
    modulation->stp_u_scale = 1.0f;
    modulation->stp_tau_d_scale = 1.0f;
    modulation->stp_tau_f_scale = 1.0f;

    /* Homeostatic */
    modulation->homeostatic_scaling_rate = 1.0f;
    modulation->homeostatic_target_shift = 0.0f;
    modulation->metaplasticity_theta_shift = 0.0f;

    /* Dendritic */
    modulation->nmda_conductance_scale = 1.0f;
    modulation->dendritic_spike_threshold_shift = 0.0f;
    modulation->ca_influx_scale = 1.0f;

    /* Adaptive */
    modulation->adaptive_threshold_shift = 0.0f;
    modulation->adaptive_sparsity_target = 0.0f;

    /* Eligibility */
    modulation->eligibility_decay_scale = 1.0f;
    modulation->eligibility_learning_rate_scale = 1.0f;

    /* Predictive coding */
    modulation->pc_prediction_precision_scale = 1.0f;
    modulation->pc_error_weight_scale = 1.0f;
    modulation->pc_learning_rate_scale = 1.0f;

    /* Attention */
    modulation->attention_gate_scale = 1.0f;
    modulation->attention_temperature = 0.0f;

    /* Global */
    modulation->global_plasticity_scale = 1.0f;

    /* Get cytokine concentrations */
    modulation->il1_concentration =
        immune_plasticity_get_cytokine_concentration(immune_system, CYTOKINE_IL1B);
    modulation->il6_concentration =
        immune_plasticity_get_cytokine_concentration(immune_system, CYTOKINE_IL6);
    modulation->tnf_alpha_concentration =
        immune_plasticity_get_cytokine_concentration(immune_system, CYTOKINE_TNFA);
    modulation->il10_concentration =
        immune_plasticity_get_cytokine_concentration(immune_system, CYTOKINE_IL10);

    /* Get inflammation level */
    modulation->inflammation_level = immune_plasticity_get_max_inflammation(immune_system);
    float inflammation_factor = inflammation_to_factor(modulation->inflammation_level);

    /* BCM modulation: IL-1β elevates threshold */
    float il1_effect = modulation->il1_concentration * config->il1_threshold_sensitivity;
    modulation->bcm_threshold_scale = 1.0f + il1_effect;
    modulation->bcm_threshold_scale = clamp_f(
        modulation->bcm_threshold_scale, 1.0f, config->max_threshold_elevation);

    /* BCM learning rate: reduced by inflammation */
    modulation->bcm_learning_rate_scale = 1.0f - (inflammation_factor * 0.5f);
    modulation->bcm_learning_rate_scale = clamp_f(
        modulation->bcm_learning_rate_scale, config->min_plasticity_factor, 1.0f);

    /* STDP timing windows: IL-6 narrows windows */
    float il6_effect = modulation->il6_concentration * config->il6_timing_sensitivity;
    modulation->stdp_tau_plus_scale = 1.0f - il6_effect;
    modulation->stdp_tau_minus_scale = 1.0f - il6_effect;
    modulation->stdp_tau_plus_scale = clamp_f(
        modulation->stdp_tau_plus_scale, config->max_timing_narrowing, 1.0f);
    modulation->stdp_tau_minus_scale = clamp_f(
        modulation->stdp_tau_minus_scale, config->max_timing_narrowing, 1.0f);

    /* STDP learning rate: reduced by TNF-α and inflammation */
    float tnf_effect = modulation->tnf_alpha_concentration * 0.6f;
    modulation->stdp_learning_rate_scale = 1.0f - fmaxf(tnf_effect, inflammation_factor * 0.5f);
    modulation->stdp_learning_rate_scale = clamp_f(
        modulation->stdp_learning_rate_scale, config->min_plasticity_factor, 1.0f);

    /* Attention: TNF-α impairs gate, inflammation increases temperature */
    modulation->attention_gate_scale = 1.0f -
        (modulation->tnf_alpha_concentration * config->tnf_attention_sensitivity);
    modulation->attention_gate_scale = clamp_f(
        modulation->attention_gate_scale, config->max_attention_impairment, 1.0f);

    modulation->attention_temperature = inflammation_factor * 0.3f; /* Max +0.3 temperature */

    /* IL-10 recovery: partially restores plasticity */
    float il10_recovery = modulation->il10_concentration * config->il10_recovery_rate;
    modulation->bcm_learning_rate_scale += il10_recovery * 0.2f;
    modulation->stdp_learning_rate_scale += il10_recovery * 0.2f;
    modulation->attention_gate_scale += il10_recovery * 0.3f;

    /* Clamp after IL-10 recovery */
    modulation->bcm_learning_rate_scale = clamp_f(modulation->bcm_learning_rate_scale, 0.0f, 1.0f);
    modulation->stdp_learning_rate_scale = clamp_f(modulation->stdp_learning_rate_scale, 0.0f, 1.0f);
    modulation->attention_gate_scale = clamp_f(modulation->attention_gate_scale, 0.0f, 1.0f);

    /* STP modulation: IL-1β reduces release probability, inflammation slows recovery */
    modulation->stp_u_scale = 1.0f - (modulation->il1_concentration * 0.3f);  /* Reduce U by up to 30% */
    modulation->stp_tau_d_scale = 1.0f + (inflammation_factor * 0.5f);  /* Slow depression recovery */
    modulation->stp_tau_f_scale = 1.0f + (modulation->tnf_alpha_concentration * 0.4f);  /* Slow facilitation */

    /* Homeostatic modulation: Chronic inflammation shifts setpoints */
    modulation->homeostatic_scaling_rate = 1.0f - (inflammation_factor * 0.4f);  /* Slower compensation */
    modulation->homeostatic_target_shift = inflammation_factor * 2.0f;  /* Shift target up to 2 Hz */
    modulation->metaplasticity_theta_shift = modulation->il1_concentration * 0.3f;  /* Elevate BCM theta */

    /* Dendritic modulation: Inflammation impairs NMDA and calcium signaling */
    modulation->nmda_conductance_scale = 1.0f - (modulation->il1_concentration * 0.4f);  /* Reduce NMDA conductance */
    modulation->dendritic_spike_threshold_shift = inflammation_factor * 5.0f;  /* Raise threshold up to 5 mV */
    modulation->ca_influx_scale = 1.0f - (modulation->tnf_alpha_concentration * 0.5f);  /* Reduce Ca influx */

    /* Adaptive plasticity modulation: Inflammation increases threshold, sparsity */
    modulation->adaptive_threshold_shift = inflammation_factor * 0.2f;  /* Raise threshold */
    modulation->adaptive_sparsity_target = inflammation_factor * 0.15f;  /* Increase sparsity (more sparse) */

    /* Eligibility trace modulation: Inflammation shortens credit assignment window */
    modulation->eligibility_decay_scale = 1.0f + (inflammation_factor * 0.3f);  /* Faster decay */
    modulation->eligibility_learning_rate_scale = 1.0f - (modulation->tnf_alpha_concentration * 0.5f);

    /* Predictive coding modulation: Inflammation reduces precision, impairs error correction */
    modulation->pc_prediction_precision_scale = 1.0f - (inflammation_factor * 0.4f);  /* Lower precision */
    modulation->pc_error_weight_scale = 1.0f - (modulation->il6_concentration * 0.3f);  /* Reduce error weight */
    modulation->pc_learning_rate_scale = 1.0f - (inflammation_factor * 0.5f);

    /* IL-10 recovery: Restore all mechanisms */
    if (il10_recovery > 0.0f) {
        /* STP recovery */
        modulation->stp_u_scale += il10_recovery * 0.2f;
        modulation->stp_tau_d_scale -= il10_recovery * 0.3f;
        modulation->stp_tau_f_scale -= il10_recovery * 0.3f;

        /* Homeostatic recovery */
        modulation->homeostatic_scaling_rate += il10_recovery * 0.3f;
        modulation->homeostatic_target_shift -= il10_recovery * 1.5f;

        /* Dendritic recovery */
        modulation->nmda_conductance_scale += il10_recovery * 0.3f;
        modulation->ca_influx_scale += il10_recovery * 0.4f;

        /* Eligibility and PC recovery */
        modulation->eligibility_learning_rate_scale += il10_recovery * 0.3f;
        modulation->pc_prediction_precision_scale += il10_recovery * 0.3f;
    }

    /* Clamp all factors to valid ranges */
    modulation->stp_u_scale = clamp_f(modulation->stp_u_scale, 0.1f, 1.0f);
    modulation->stp_tau_d_scale = clamp_f(modulation->stp_tau_d_scale, 0.5f, 2.0f);
    modulation->stp_tau_f_scale = clamp_f(modulation->stp_tau_f_scale, 0.5f, 2.0f);
    modulation->homeostatic_scaling_rate = clamp_f(modulation->homeostatic_scaling_rate, 0.2f, 1.0f);
    modulation->homeostatic_target_shift = clamp_f(modulation->homeostatic_target_shift, 0.0f, 3.0f);
    modulation->nmda_conductance_scale = clamp_f(modulation->nmda_conductance_scale, 0.3f, 1.0f);
    modulation->ca_influx_scale = clamp_f(modulation->ca_influx_scale, 0.2f, 1.0f);
    modulation->eligibility_decay_scale = clamp_f(modulation->eligibility_decay_scale, 0.7f, 1.5f);
    modulation->eligibility_learning_rate_scale = clamp_f(modulation->eligibility_learning_rate_scale, 0.2f, 1.0f);
    modulation->pc_prediction_precision_scale = clamp_f(modulation->pc_prediction_precision_scale, 0.3f, 1.0f);
    modulation->pc_error_weight_scale = clamp_f(modulation->pc_error_weight_scale, 0.3f, 1.0f);
    modulation->pc_learning_rate_scale = clamp_f(modulation->pc_learning_rate_scale, 0.2f, 1.0f);

    /* Global plasticity: weighted average across all mechanisms */
    modulation->global_plasticity_scale = (
        modulation->bcm_learning_rate_scale * 0.125f +
        modulation->stdp_learning_rate_scale * 0.125f +
        modulation->stp_u_scale * 0.125f +
        modulation->homeostatic_scaling_rate * 0.125f +
        modulation->nmda_conductance_scale * 0.125f +
        modulation->eligibility_learning_rate_scale * 0.125f +
        modulation->pc_learning_rate_scale * 0.125f +
        modulation->attention_gate_scale * 0.125f
    );

    nimcp_platform_mutex_lock(&g_stats_mutex);
    g_stats.cytokine_updates++;
    nimcp_platform_mutex_unlock(&g_stats_mutex);

    return 0;
}

/* ============================================================================
 * BCM Integration API
 * ============================================================================ */

int immune_plasticity_modulate_bcm(
    bcm_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Apply immune modulation to BCM parameters
     * WHY:  IL-1β elevates threshold, inflammation reduces learning
     * HOW:  Scale threshold and learning rate by factors
     */
    if (!params || !modulation) {
        LOG_ERROR("NULL pointer in modulate_bcm");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_bcm: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Elevate BCM threshold (harder to induce LTP) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    params->min_threshold *= modulation->bcm_threshold_scale;
    params->max_threshold *= modulation->bcm_threshold_scale;

    /* Reduce learning rate during inflammation */
    params->learning_rate *= modulation->bcm_learning_rate_scale;

    nimcp_platform_mutex_lock(&g_stats_mutex);
    g_stats.bcm_modulation_events++;
    g_stats.avg_bcm_threshold_elevation =
        (g_stats.avg_bcm_threshold_elevation * 0.9f) +
        (modulation->bcm_threshold_scale * 0.1f);
    nimcp_platform_mutex_unlock(&g_stats_mutex);

    LOG_DEBUG("BCM modulated: threshold_scale=%.3f, lr_scale=%.3f",
              modulation->bcm_threshold_scale, modulation->bcm_learning_rate_scale);

    return 0;
}

float immune_plasticity_modulate_bcm_threshold(
    bcm_synapse_t* synapse,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Adjust BCM synapse threshold in real-time
     * WHY:  Dynamic threshold changes during learning
     * HOW:  Multiply by immune scale factor
     */
    if (!synapse || !modulation) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    float new_threshold = synapse->threshold * modulation->bcm_threshold_scale;
    synapse->threshold = new_threshold;

    return new_threshold;
}

/* ============================================================================
 * STDP Integration API
 * ============================================================================ */

int immune_plasticity_modulate_stdp(
    stdp_config_t* config,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Apply immune modulation to STDP configuration
     * WHY:  IL-6 narrows windows, inflammation reduces learning
     * HOW:  Scale timing constants and learning rate
     */
    if (!config || !modulation) {
        LOG_ERROR("NULL pointer in modulate_stdp");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_stdp: required parameter is NULL (config, modulation)");
        return -1;
    }

    /* Narrow STDP timing windows (IL-6 effect) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    config->tau_plus *= modulation->stdp_tau_plus_scale;
    config->tau_minus *= modulation->stdp_tau_minus_scale;

    /* Reduce learning rate (TNF-α and inflammation) */
    config->learning_rate *= modulation->stdp_learning_rate_scale;

    nimcp_platform_mutex_lock(&g_stats_mutex);
    g_stats.stdp_modulation_events++;
    g_stats.avg_stdp_window_reduction =
        (g_stats.avg_stdp_window_reduction * 0.9f) +
        ((1.0f - modulation->stdp_tau_plus_scale) * 0.1f);
    nimcp_platform_mutex_unlock(&g_stats_mutex);

    LOG_DEBUG("STDP modulated: tau_scale=%.3f, lr_scale=%.3f",
              modulation->stdp_tau_plus_scale, modulation->stdp_learning_rate_scale);

    return 0;
}

int immune_plasticity_modulate_stdp_timing(
    stdp_synapse_t* synapse,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Adjust STDP timing windows in real-time
     * WHY:  Dynamic window changes during inflammation
     * HOW:  Scale tau values by immune factors
     */
    if (!synapse || !modulation) {
        LOG_ERROR("NULL pointer in modulate_stdp_timing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_stdp_timing: required parameter is NULL (synapse, modulation)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    synapse->tau_plus *= modulation->stdp_tau_plus_scale;
    synapse->tau_minus *= modulation->stdp_tau_minus_scale;
    synapse->learning_rate *= modulation->stdp_learning_rate_scale;

    return 0;
}

/* ============================================================================
 * Attention Integration API
 * ============================================================================ */

int immune_plasticity_modulate_attention_config(
    multihead_attention_config_t* config,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Apply immune modulation to attention configuration
     * WHY:  TNF-α impairs attention, inflammation diffuses focus
     * HOW:  Reduce gate bias, increase temperature
     */
    if (!config || !modulation) {
        LOG_ERROR("NULL pointer in modulate_attention_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_attention_config: required parameter is NULL (config, modulation)");
        return -1;
    }

    /* Reduce thalamic gate opening (TNF-α effect) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    config->gate_bias *= modulation->attention_gate_scale;

    /* Note: Can't modify temperature per head from config,
     * but this would be applied during forward pass if we add
     * temperature to multihead_attention_forward */

    nimcp_platform_mutex_lock(&g_stats_mutex);
    g_stats.attention_modulation_events++;
    g_stats.avg_attention_impairment =
        (g_stats.avg_attention_impairment * 0.9f) +
        ((1.0f - modulation->attention_gate_scale) * 0.1f);
    nimcp_platform_mutex_unlock(&g_stats_mutex);

    LOG_DEBUG("Attention modulated: gate_scale=%.3f, temp_increase=%.3f",
              modulation->attention_gate_scale, modulation->attention_temperature);

    return 0;
}

int immune_plasticity_modulate_attention_gate(
    multihead_attention_t mha,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Adjust active attention system gate
     * WHY:  Real-time attention impairment
     * HOW:  Set gate signal based on immune state
     */
    if (!mha || !modulation) {
        LOG_ERROR("NULL pointer in modulate_attention_gate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_attention_gate: required parameter is NULL (mha, modulation)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    float gate_signal = modulation->attention_gate_scale;
    bool success = multihead_attention_set_gate(mha, gate_signal);

    return success ? 0 : -1;
}

/* ============================================================================
 * STP Integration API
 * ============================================================================ */

int immune_plasticity_modulate_stp(
    stp_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Apply immune modulation to STP parameters
     * WHY:  Inflammation affects neurotransmitter release
     * HOW:  Scale U, tau_D, tau_F by immune factors
     */
    if (!params || !modulation) {
        LOG_ERROR("NULL pointer in modulate_stp");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_stp: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Reduce release probability (U) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    params->U *= modulation->stp_u_scale;

    /* Increase depression time constant (slower recovery) */
    params->tau_D *= modulation->stp_tau_d_scale;

    /* Increase facilitation time constant (slower facilitation) */
    params->tau_F *= modulation->stp_tau_f_scale;

    LOG_DEBUG("STP modulated: U_scale=%.3f, tau_D_scale=%.3f, tau_F_scale=%.3f",
              modulation->stp_u_scale, modulation->stp_tau_d_scale, modulation->stp_tau_f_scale);

    return 0;
}

int immune_plasticity_modulate_stp_state(
    stp_state_t* state,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Apply immune modulation to STP state
     * WHY:  Real-time adjustment
     * HOW:  Update embedded params
     */
    if (!state || !modulation) {
        LOG_ERROR("NULL pointer in modulate_stp_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_stp_state: required parameter is NULL (state, modulation)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    return immune_plasticity_modulate_stp(&state->params, modulation);
}

/* ============================================================================
 * Homeostatic Plasticity Integration API
 * ============================================================================ */

int immune_plasticity_modulate_homeostatic_config(
    homeostatic_config_t* config,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Apply immune modulation to homeostatic config
     * WHY:  Inflammation disrupts homeostasis
     * HOW:  Adjust scaling rate and target shifts
     */
    if (!config || !modulation) {
        LOG_ERROR("NULL pointer in modulate_homeostatic_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_homeostatic_config: required parameter is NULL (config, modulation)");
        return -1;
    }

    /* Modulate synaptic scaling */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    immune_plasticity_modulate_synaptic_scaling(&config->scaling_params, modulation);

    /* Modulate metaplasticity */
    immune_plasticity_modulate_metaplasticity(&config->meta_params, modulation);

    LOG_DEBUG("Homeostatic config modulated: scaling_rate=%.3f, target_shift=%.3f",
              modulation->homeostatic_scaling_rate, modulation->homeostatic_target_shift);

    return 0;
}

int immune_plasticity_modulate_synaptic_scaling(
    synaptic_scaling_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Adjust synaptic scaling parameters
     * WHY:  Inflammation affects global scaling
     * HOW:  Shift target rate and slow scaling
     */
    if (!params || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_synaptic_scaling: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Shift target firing rate upward during inflammation */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    params->target_rate += modulation->homeostatic_target_shift;

    /* Slow down scaling rate */
    params->scaling_time_constant *= (2.0f - modulation->homeostatic_scaling_rate);

    return 0;
}

int immune_plasticity_modulate_metaplasticity(
    metaplasticity_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Adjust metaplasticity (BCM threshold) parameters
     * WHY:  Inflammation elevates modification threshold
     * HOW:  Shift min/max theta
     */
    if (!params || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_metaplasticity: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Elevate BCM threshold range */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    float theta_shift = modulation->metaplasticity_theta_shift;
    params->min_theta *= (1.0f + theta_shift);
    params->max_theta *= (1.0f + theta_shift);

    return 0;
}

/* ============================================================================
 * Dendritic Nonlinearity Integration API
 * ============================================================================ */

int immune_plasticity_modulate_nmda(
    nmda_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Modify NMDA receptor dynamics
     * WHY:  Inflammation reduces NMDA conductance and Ca influx
     * HOW:  Scale g_max and Ca permeability
     */
    if (!params || !modulation) {
        LOG_ERROR("NULL pointer in modulate_nmda");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_nmda: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Reduce NMDA conductance */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    params->g_max *= modulation->nmda_conductance_scale;

    /* Reduce calcium influx */
    params->ca_permeability *= modulation->ca_influx_scale;

    LOG_DEBUG("NMDA modulated: g_max_scale=%.3f, ca_scale=%.3f",
              modulation->nmda_conductance_scale, modulation->ca_influx_scale);

    return 0;
}

int immune_plasticity_modulate_dendritic_compartment(
    compartment_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Adjust dendritic compartment excitability
     * WHY:  Inflammation raises spike thresholds
     * HOW:  Shift spike threshold upward
     */
    if (!params || !modulation) {
        LOG_ERROR("NULL pointer in modulate_dendritic_compartment");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_dendritic_compartment: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Raise dendritic spike threshold */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    params->spike_threshold += modulation->dendritic_spike_threshold_shift;

    /* Reduce supralinear summation */
    params->supralinearity_factor *= (1.0f - modulation->inflammation_level * 0.1f);

    return 0;
}

/* ============================================================================
 * Adaptive Plasticity Integration API
 * ============================================================================ */

int immune_plasticity_modulate_adaptive_params(
    adaptive_spike_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Modify adaptive spiking parameters
     * WHY:  Inflammation increases threshold and sparsity
     * HOW:  Shift threshold and sparsity target
     */
    if (!params || !modulation) {
        LOG_ERROR("NULL pointer in modulate_adaptive_params");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_adaptive_params: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Shift sparsity target (more sparse during inflammation) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    params->sparsity_target += modulation->adaptive_sparsity_target;

    /* Clamp to valid range [0, 1] */
    if (params->sparsity_target > 1.0f) params->sparsity_target = 1.0f;

    LOG_DEBUG("Adaptive params modulated: threshold_shift=%.3f, sparsity=%.3f",
              modulation->adaptive_threshold_shift, params->sparsity_target);

    return 0;
}

/* ============================================================================
 * Eligibility Trace Integration API
 * ============================================================================ */

int immune_plasticity_modulate_eligibility_config(
    eligibility_config_t* config,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Modify eligibility trace parameters
     * WHY:  Inflammation shortens credit assignment window
     * HOW:  Adjust decay lambda and learning rate
     */
    if (!config || !modulation) {
        LOG_ERROR("NULL pointer in modulate_eligibility_config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_eligibility_config: required parameter is NULL (config, modulation)");
        return -1;
    }

    /* Faster decay (shorter credit window) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    config->decay_lambda *= modulation->eligibility_decay_scale;

    /* Clamp to valid range */
    if (config->decay_lambda > 0.99f) config->decay_lambda = 0.99f;
    if (config->decay_lambda < 0.7f) config->decay_lambda = 0.7f;

    /* Reduce learning rate */
    config->learning_rate *= modulation->eligibility_learning_rate_scale;

    LOG_DEBUG("Eligibility modulated: decay_scale=%.3f, lr_scale=%.3f",
              modulation->eligibility_decay_scale, modulation->eligibility_learning_rate_scale);

    return 0;
}

/* ============================================================================
 * Predictive Coding Integration API
 * ============================================================================ */

int immune_plasticity_modulate_predictive_coding_layer(
    pc_layer_params_t* params,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Modify PC layer parameters
     * WHY:  Inflammation reduces prediction precision
     * HOW:  Adjust learning rates and precision bounds
     */
    if (!params || !modulation) {
        LOG_ERROR("NULL pointer in modulate_predictive_coding_layer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_predictive_coding_layer: required parameter is NULL (params, modulation)");
        return -1;
    }

    /* Reduce precision learning rate (slower precision updates) */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    params->learning_rate_precision *= modulation->pc_prediction_precision_scale;

    /* Reduce representation learning rate */
    params->learning_rate_mu *= modulation->pc_learning_rate_scale;

    /* Reduce weight learning rate */
    params->learning_rate_weights *= modulation->pc_learning_rate_scale;

    /* Lower minimum precision (increase uncertainty) */
    params->min_precision *= modulation->pc_prediction_precision_scale;

    LOG_DEBUG("PC layer modulated: precision_scale=%.3f, lr_scale=%.3f",
              modulation->pc_prediction_precision_scale, modulation->pc_learning_rate_scale);

    return 0;
}

int immune_plasticity_modulate_predictive_coding_hierarchy(
    pc_hierarchy_config_t* config,
    const immune_plasticity_modulation_t* modulation)
{
    /* WHAT: Modify PC hierarchy config
     * WHY:  Global effects on predictive processing
     * HOW:  Adjust global learning rates
     */
    if (!config || !modulation) {
        LOG_ERROR("NULL pointer in modulate_predictive_coding_hierarchy");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_plasticity_modulate_predictive_coding_hierarchy: required parameter is NULL (config, modulation)");
        return -1;
    }

    /* Reduce global learning rate */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    config->learning_rate *= modulation->pc_learning_rate_scale;

    /* Reduce precision learning rate */
    config->precision_learning_rate *= modulation->pc_prediction_precision_scale;

    return 0;
}

/* ============================================================================
 * Monitoring and Statistics API
 * ============================================================================ */

int immune_plasticity_get_stats(immune_plasticity_stats_t* stats) {
    /* WHAT: Copy statistics to output
     * WHY:  Allow monitoring of immune-plasticity interactions
     * HOW:  Memcpy internal stats
     */
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_ge", 0.0f);


    nimcp_platform_mutex_lock(&g_stats_mutex);
    memcpy(stats, &g_stats, sizeof(immune_plasticity_stats_t));
    nimcp_platform_mutex_unlock(&g_stats_mutex);
    return 0;
}

void immune_plasticity_reset_stats(void) {
    /* WHAT: Clear all statistics
     * WHY:  Start fresh measurement period
     * HOW:  Zero the struct
     */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_re", 0.0f);

    nimcp_platform_mutex_lock(&g_stats_mutex);
    memset(&g_stats, 0, sizeof(immune_plasticity_stats_t));
    nimcp_platform_mutex_unlock(&g_stats_mutex);
}

bool immune_plasticity_is_impaired(
    const immune_plasticity_modulation_t* modulation,
    float threshold)
{
    /* WHAT: Check if plasticity significantly impaired
     * WHY:  Quick detection of severe immune effects
     * HOW:  Compare global scale to threshold
     */
    if (!modulation) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_is", 0.0f);


    return modulation->global_plasticity_scale < threshold;
}

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

int immune_plasticity_modulation_to_string(
    const immune_plasticity_modulation_t* modulation,
    char* buffer,
    size_t buffer_size)
{
    /* WHAT: Format modulation state as string
     * WHY:  Debugging and logging
     * HOW:  snprintf key values
     */
    if (!modulation || !buffer || buffer_size == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_immune_plasticity_mo", 0.0f);


    return snprintf(buffer, buffer_size,
        "ImmuneModulation{inflammation=%d, IL1=%.2f, IL6=%.2f, TNFa=%.2f, IL10=%.2f, "
        "bcm_thresh=%.2f, bcm_lr=%.2f, stdp_tau=%.2f, stdp_lr=%.2f, "
        "attn_gate=%.2f, global=%.2f}",
        modulation->inflammation_level,
        modulation->il1_concentration,
        modulation->il6_concentration,
        modulation->tnf_alpha_concentration,
        modulation->il10_concentration,
        modulation->bcm_threshold_scale,
        modulation->bcm_learning_rate_scale,
        modulation->stdp_tau_plus_scale,
        modulation->stdp_learning_rate_scale,
        modulation->attention_gate_scale,
        modulation->global_plasticity_scale
    );
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about brain immune plasticity
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int brain_immune_plasticity_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    brain_immune_plasticity_heartbeat("brain_immune_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Brain_Immune_Plasticity");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                brain_immune_plasticity_heartbeat("brain_immune_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Brain immune plasticity self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Brain_Immune_Plasticity");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Brain_Immune_Plasticity");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void brain_immune_plasticity_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_brain_immune_plasticity_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int brain_immune_plasticity_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_plasticity_training_begin: NULL argument");
        return -1;
    }
    brain_immune_plasticity_heartbeat_instance(NULL, "brain_immune_plasticity_training_begin", 0.0f);
    return 0;
}

int brain_immune_plasticity_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_plasticity_training_end: NULL argument");
        return -1;
    }
    brain_immune_plasticity_heartbeat_instance(NULL, "brain_immune_plasticity_training_end", 1.0f);
    return 0;
}

int brain_immune_plasticity_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_plasticity_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    brain_immune_plasticity_heartbeat_instance(NULL, "brain_immune_plasticity_training_step", progress);
    return 0;
}

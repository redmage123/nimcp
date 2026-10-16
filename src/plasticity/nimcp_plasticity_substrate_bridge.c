/**
 * @file nimcp_plasticity_substrate_bridge.c
 * @brief Plasticity-Neural Substrate Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "plasticity/nimcp_plasticity_substrate_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include <math.h>
#include <string.h>
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(plasticity_substrate_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(plasticity_substrate_bridge)


/* ============================================================================
 * Helper Functions - Temperature Scaling
 * ============================================================================ */

/**
 * @brief Compute Q10-based temperature scaling factor
 *
 * WHAT: Calculate multiplicative factor for rate constants
 * WHY:  Biological processes have exponential temperature dependence
 * HOW:  factor = Q10^((T - T_ref) / 10)
 *
 * BIOLOGICAL: Q10 rule from Arrhenius equation
 * - Q10 = 2: rate doubles every 10°C
 * - Q10 = 3: rate triples every 10°C
 *
 * @param current_temp Current temperature (°C)
 * @param reference_temp Reference temperature (°C), typically 37°C
 * @param q10 Q10 coefficient
 * @return Temperature scaling factor
 */
static float compute_q10_factor(float current_temp, float reference_temp, float q10)
{
    if (q10 <= 0.0f) {
        return 1.0f;
    }

    float temp_diff = current_temp - reference_temp;
    float exponent = temp_diff / 10.0f;
    return powf(q10, exponent);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int plasticity_substrate_default_config(plasticity_substrate_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Config is NULL in default_config");
        return -1;
    }

    /* Enable all mechanisms by default */
    config->enable_stdp_modulation = true;
    config->enable_bcm_modulation = true;
    config->enable_homeostatic_modulation = true;
    config->enable_eligibility_modulation = true;
    config->enable_dendritic_modulation = true;
    config->enable_bio_async = false;

    /* Moderate sensitivity (1.0 = biological measurements) */
    config->atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->temperature_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->membrane_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Biological realism */
    config->enforce_atp_blocking = true;    /* Block LTP at low ATP */
    config->use_q10_temperature = true;     /* Use Q10 scaling */
    config->compensate_homeostatic = true;  /* Homeostatic compensation */

    return 0;
}

plasticity_substrate_bridge_t* plasticity_substrate_bridge_create(
    const plasticity_substrate_config_t* config,
    neural_substrate_t* substrate)
{
    /* Guard: Validate substrate */
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Cannot create plasticity substrate bridge: NULL substrate");
        NIMCP_LOGGING_ERROR("Cannot create plasticity substrate bridge: NULL substrate");
        return NULL;
    }

    /* Allocate bridge structure */
    plasticity_substrate_bridge_t* bridge =
        (plasticity_substrate_bridge_t*)nimcp_malloc(sizeof(plasticity_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate plasticity substrate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate plasticity substrate bridge");
        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(plasticity_substrate_bridge_t));

    /* Store substrate reference */
    bridge->substrate = substrate;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        plasticity_substrate_default_config(&bridge->config);
    }

    /* Initialize mutex */
    bridge->base.mutex = (nimcp_platform_mutex_t*)nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate mutex");
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    if (nimcp_platform_mutex_init(bridge->base.mutex, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to initialize mutex");
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize effects to neutral (no modulation) */
    bridge->effects.global_learning_rate = 1.0f;
    bridge->effects.plasticity_capacity = 1.0f;
    bridge->effects.stdp.learning_rate_mod = 1.0f;
    bridge->effects.stdp.tau_plus_mod = 1.0f;
    bridge->effects.stdp.tau_minus_mod = 1.0f;
    bridge->effects.stdp.temperature_factor = 1.0f;
    bridge->effects.stdp.atp_gating = 1.0f;
    bridge->effects.bcm.threshold_shift = 1.0f;
    bridge->effects.bcm.learning_rate_mod = 1.0f;
    bridge->effects.bcm.metabolic_bias = 0.0f;
    bridge->effects.bcm.stability_factor = 1.0f;
    bridge->effects.homeostatic.target_rate_adjustment = 1.0f;
    bridge->effects.homeostatic.scaling_rate_mod = 1.0f;
    bridge->effects.homeostatic.ip_threshold_shift = 0.0f;
    bridge->effects.homeostatic.recovery_boost = 0.0f;
    bridge->effects.eligibility.decay_lambda_mod = 1.0f;
    bridge->effects.eligibility.consolidation_gate = 1.0f;
    bridge->effects.eligibility.atp_maintenance = 1.0f;
    bridge->effects.eligibility.protein_synthesis_rate = 1.0f;
    bridge->effects.dendritic.nmda_conductance_mod = 1.0f;
    bridge->effects.dendritic.spike_threshold_shift = 0.0f;
    bridge->effects.dendritic.calcium_influx_mod = 1.0f;
    bridge->effects.dendritic.membrane_factor = 1.0f;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(plasticity_substrate_stats_t));
    bridge->stats.min_learning_rate_factor = 1.0f;
    bridge->stats.max_learning_rate_factor = 1.0f;
    bridge->stats.avg_plasticity_capacity = 1.0f;

    NIMCP_LOGGING_INFO("Created plasticity substrate bridge");

    return bridge;
}

void plasticity_substrate_bridge_destroy(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        plasticity_substrate_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed plasticity substrate bridge");
}

int plasticity_substrate_connect_contexts(
    plasticity_substrate_bridge_t* bridge,
    void* stdp_ctx,
    void* bcm_ctx,
    homeostatic_controller_t homeostatic_ctrl,
    void* eligibility_ctx,
    dendritic_tree_t dendritic_tree)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Bridge is NULL in connect_contexts");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->stdp_context = stdp_ctx;
    bridge->bcm_context = bcm_ctx;
    bridge->homeostatic_controller = homeostatic_ctrl;
    bridge->eligibility_context = eligibility_ctx;
    bridge->dendritic_tree = dendritic_tree;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Connected plasticity contexts to bridge");

    return 0;
}

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

int plasticity_substrate_connect_bio_async(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Bridge is NULL in connect_bio_async");
        return -1;
    }

    if (bridge->base.bio_async_enabled) {
        return 0;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_PLASTICITY_SUBSTRATE,
        .module_name = "plasticity_substrate_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected plasticity substrate bridge to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_substrate_connect_bio_async: validation failed");
    return -1;
}

int plasticity_substrate_disconnect_bio_async(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->base.bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid bridge state in disconnect_bio_async");
        return -1;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected plasticity substrate bridge from bio-async");

    return 0;
}

bool plasticity_substrate_is_bio_async_connected(const plasticity_substrate_bridge_t* bridge)
{
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Update API - STDP Modulation
 * ============================================================================ */

int plasticity_substrate_update_stdp(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_stdp_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid bridge in update_stdp");
        return -1;
    }

    substrate_metabolic_state_t metabolic;
    substrate_physical_state_t physical;

    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_substrate_update_stdp: validation failed");
        return -1;
    }
    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_substrate_update_stdp: validation failed");
        return -1;
    }

    /* ========================================================================
     * BIOLOGICAL: Temperature effects on STDP window (Q10 scaling)
     * ======================================================================== */
    if (bridge->config.use_q10_temperature) {
        /* Q10 ≈ 2.2 for STDP (Bi & Poo 1998) */
        float temp_factor = compute_q10_factor(
            physical.temperature,
            PLASTICITY_REFERENCE_TEMP,
            PLASTICITY_Q10_STDP
        );

        /* Scale by sensitivity */
        temp_factor = 1.0f + (temp_factor - 1.0f) * bridge->config.temperature_sensitivity;

        /* Clamp to valid range */
        bridge->effects.stdp.temperature_factor = nimcp_clampf(
            temp_factor,
            PLASTICITY_STDP_WINDOW_MIN,
            PLASTICITY_STDP_WINDOW_MAX
        );

        /* Both LTP and LTD windows affected similarly */
        bridge->effects.stdp.tau_plus_mod = bridge->effects.stdp.temperature_factor;
        bridge->effects.stdp.tau_minus_mod = bridge->effects.stdp.temperature_factor;
    } else {
        bridge->effects.stdp.temperature_factor = 1.0f;
        bridge->effects.stdp.tau_plus_mod = 1.0f;
        bridge->effects.stdp.tau_minus_mod = 1.0f;
    }

    /* ========================================================================
     * BIOLOGICAL: ATP gating of LTP (energy-dependent plasticity)
     * ======================================================================== */
    float atp_factor;
    if (metabolic.atp_level >= PLASTICITY_ATP_FULL) {
        /* Full plasticity */
        atp_factor = 1.0f;
    } else if (metabolic.atp_level >= PLASTICITY_ATP_REDUCED) {
        /* Reduced plasticity (linear scaling) */
        float range = PLASTICITY_ATP_FULL - PLASTICITY_ATP_REDUCED;
        float position = metabolic.atp_level - PLASTICITY_ATP_REDUCED;
        atp_factor = 0.5f + 0.5f * (position / range);
    } else if (metabolic.atp_level >= PLASTICITY_ATP_BLOCKED) {
        /* Severely reduced */
        float range = PLASTICITY_ATP_REDUCED - PLASTICITY_ATP_BLOCKED;
        float position = metabolic.atp_level - PLASTICITY_ATP_BLOCKED;
        atp_factor = 0.1f + 0.4f * (position / range);
    } else {
        /* LTP blocked (LTD may still occur) */
        if (bridge->config.enforce_atp_blocking) {
            atp_factor = 0.0f;
        } else {
            atp_factor = 0.1f;
        }
    }

    /* Scale by sensitivity */
    atp_factor = 1.0f - (1.0f - atp_factor) * bridge->config.atp_sensitivity;
    bridge->effects.stdp.atp_gating = nimcp_clampf(atp_factor, 0.0f, 1.0f);

    /* ========================================================================
     * Combined learning rate modulation
     * ======================================================================== */
    bridge->effects.stdp.learning_rate_mod = bridge->effects.stdp.atp_gating;

    /* Track statistics */
    if (bridge->effects.stdp.atp_gating < 0.8f) {
        bridge->stats.atp_limited_events++;
    }
    if (fabsf(bridge->effects.stdp.temperature_factor - 1.0f) > 0.1f) {
        bridge->stats.temperature_modulations++;
    }

    return 0;
}

/* ============================================================================
 * Update API - BCM Modulation
 * ============================================================================ */

int plasticity_substrate_update_bcm(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_bcm_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid bridge in update_bcm");
        return -1;
    }

    substrate_metabolic_state_t metabolic;

    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_substrate_update_bcm: validation failed");
        return -1;
    }

    /* ========================================================================
     * BIOLOGICAL: Metabolic stress shifts BCM threshold toward LTD
     * - Low energy → higher threshold → more depression (protective)
     * - Hypoxia → impaired calcium signaling → biased LTD
     * ======================================================================== */

    float threshold_shift = PLASTICITY_BCM_OPTIMAL_SHIFT;

    /* ATP effect on threshold */
    if (metabolic.atp_level < PLASTICITY_ATP_REDUCED) {
        /* Low ATP → higher threshold */
        float deficit = (PLASTICITY_ATP_REDUCED - metabolic.atp_level) / PLASTICITY_ATP_REDUCED;
        threshold_shift += deficit * (PLASTICITY_BCM_STRESS_SHIFT - 1.0f);
    }

    /* Oxygen effect on threshold */
    if (metabolic.oxygen_saturation < 0.7f) {
        /* Hypoxia → higher threshold */
        float hypoxia = (0.7f - metabolic.oxygen_saturation) / 0.7f;
        threshold_shift += hypoxia * (PLASTICITY_BCM_HYPOXIA_SHIFT - 1.0f);
    }

    /* Clamp threshold shift */
    bridge->effects.bcm.threshold_shift = nimcp_clampf(threshold_shift, 1.0f, 1.5f);

    /* ========================================================================
     * Learning rate modulation (same as STDP for consistency)
     * ======================================================================== */
    float lr_mod = metabolic.atp_level / PLASTICITY_ATP_FULL;
    bridge->effects.bcm.learning_rate_mod = nimcp_clampf(lr_mod, 0.1f, 1.0f);

    /* ========================================================================
     * Metabolic bias: negative bias → favor LTD
     * ======================================================================== */
    if (metabolic.metabolic_capacity < 0.7f) {
        /* Low metabolic capacity → bias toward LTD */
        bridge->effects.bcm.metabolic_bias = -(1.0f - metabolic.metabolic_capacity);
    } else {
        bridge->effects.bcm.metabolic_bias = 0.0f;
    }

    /* Threshold stability (high metabolic state → stable threshold) */
    bridge->effects.bcm.stability_factor = metabolic.metabolic_capacity;

    return 0;
}

/* ============================================================================
 * Update API - Homeostatic Modulation
 * ============================================================================ */

int plasticity_substrate_update_homeostatic(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_homeostatic_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid bridge in update_homeostatic");
        return -1;
    }

    substrate_health_level_t health = substrate_get_health_level(bridge->substrate);
    float capacity = substrate_get_capacity(bridge->substrate);

    /* ========================================================================
     * BIOLOGICAL: Substrate degradation → lower safe target firing rate
     * - Compromised substrate can't sustain high firing rates
     * - Homeostatic compensation protects degraded neurons
     * ======================================================================== */

    switch (health) {
        case SUBSTRATE_HEALTH_OPTIMAL:
            bridge->effects.homeostatic.target_rate_adjustment = 1.0f;
            bridge->effects.homeostatic.recovery_boost = 0.0f;
            break;

        case SUBSTRATE_HEALTH_STRESSED:
            bridge->effects.homeostatic.target_rate_adjustment = 0.95f;
            bridge->effects.homeostatic.recovery_boost = 0.1f;
            break;

        case SUBSTRATE_HEALTH_COMPROMISED:
            bridge->effects.homeostatic.target_rate_adjustment = 0.85f;
            bridge->effects.homeostatic.recovery_boost = 0.2f;
            break;

        case SUBSTRATE_HEALTH_CRITICAL:
            bridge->effects.homeostatic.target_rate_adjustment = PLASTICITY_HOMEOSTATIC_DEGRADED;
            bridge->effects.homeostatic.recovery_boost = 0.3f;
            break;

        case SUBSTRATE_HEALTH_FAILING:
            bridge->effects.homeostatic.target_rate_adjustment = 0.5f;
            bridge->effects.homeostatic.recovery_boost = 0.5f;
            break;

        default:
            bridge->effects.homeostatic.target_rate_adjustment = 1.0f;
            bridge->effects.homeostatic.recovery_boost = 0.0f;
            break;
    }

    /* Scaling rate modulation (faster scaling for degraded substrate) */
    if (bridge->config.compensate_homeostatic) {
        /* Enhanced scaling for poor health */
        bridge->effects.homeostatic.scaling_rate_mod = 1.0f + (1.0f - capacity) * 0.5f;
    } else {
        bridge->effects.homeostatic.scaling_rate_mod = 1.0f;
    }

    /* Intrinsic plasticity threshold shift (raise threshold for low health) */
    bridge->effects.homeostatic.ip_threshold_shift = (1.0f - capacity) * 5.0f; /* mV shift */

    return 0;
}

/* ============================================================================
 * Update API - Eligibility Trace Modulation
 * ============================================================================ */

int plasticity_substrate_update_eligibility(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_eligibility_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid bridge in update_eligibility");
        return -1;
    }

    substrate_metabolic_state_t metabolic;
    substrate_physical_state_t physical;

    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_substrate_update_eligibility: validation failed");
        return -1;
    }
    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_substrate_update_eligibility: validation failed");
        return -1;
    }

    /* ========================================================================
     * BIOLOGICAL: Trace persistence requires active maintenance (ATP cost)
     * - Low ATP → faster decay (can't maintain traces)
     * - Temperature affects biochemical reaction rates
     * ======================================================================== */

    /* ATP effect on trace decay */
    float atp_effect = 1.0f;
    if (metabolic.atp_level < 0.8f) {
        /* Low ATP → faster decay (higher decay rate, lower lambda) */
        float deficit = (0.8f - metabolic.atp_level) / 0.8f;
        atp_effect = 1.0f - deficit * PLASTICITY_ELIGIBILITY_ATP_FACTOR;
    }

    /* Temperature effect (Q10 ≈ 1.8 for trace decay) */
    float temp_effect = 1.0f;
    if (bridge->config.use_q10_temperature) {
        temp_effect = compute_q10_factor(
            physical.temperature,
            PLASTICITY_REFERENCE_TEMP,
            PLASTICITY_Q10_ELIGIBILITY
        );
        /* Scale by sensitivity */
        temp_effect = 1.0f + (temp_effect - 1.0f) * PLASTICITY_ELIGIBILITY_TEMP_FACTOR;
    }

    /* Combined decay modulation */
    bridge->effects.eligibility.decay_lambda_mod = atp_effect * temp_effect;
    bridge->effects.eligibility.decay_lambda_mod = nimcp_clampf(
        bridge->effects.eligibility.decay_lambda_mod,
        0.8f,
        1.2f
    );

    /* ========================================================================
     * BIOLOGICAL: Consolidation (trace→weight) requires protein synthesis
     * - ATP-dependent process
     * - Blocked at low energy
     * ======================================================================== */
    if (metabolic.atp_level >= PLASTICITY_ATP_FULL) {
        bridge->effects.eligibility.consolidation_gate = 1.0f;
        bridge->effects.eligibility.protein_synthesis_rate = 1.0f;
    } else if (metabolic.atp_level >= PLASTICITY_ATP_REDUCED) {
        float range = PLASTICITY_ATP_FULL - PLASTICITY_ATP_REDUCED;
        float position = metabolic.atp_level - PLASTICITY_ATP_REDUCED;
        float factor = position / range;
        bridge->effects.eligibility.consolidation_gate = factor;
        bridge->effects.eligibility.protein_synthesis_rate = factor;
    } else {
        /* Severely reduced or blocked */
        bridge->effects.eligibility.consolidation_gate = 0.2f;
        bridge->effects.eligibility.protein_synthesis_rate = 0.1f;
    }

    /* ATP maintenance factor */
    bridge->effects.eligibility.atp_maintenance = metabolic.atp_level;

    return 0;
}

/* ============================================================================
 * Update API - Dendritic Modulation
 * ============================================================================ */

int plasticity_substrate_update_dendritic(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge || !bridge->config.enable_dendritic_modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Invalid bridge in update_dendritic");
        return -1;
    }

    substrate_physical_state_t physical;

    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_substrate_update_dendritic: validation failed");
        return -1;
    }

    /* ========================================================================
     * BIOLOGICAL: NMDA function depends on membrane integrity
     * - Mg²⁺ unblock requires healthy membrane potential
     * - Damaged membranes → leaky conductance → impaired NMDA
     * ======================================================================== */

    if (physical.membrane_integrity >= 0.9f) {
        /* Healthy membrane → full NMDA function */
        bridge->effects.dendritic.nmda_conductance_mod = 1.0f;
        bridge->effects.dendritic.membrane_factor = 1.0f;
    } else if (physical.membrane_integrity >= PLASTICITY_DENDRITIC_MEMBRANE_MIN) {
        /* Degraded membrane → reduced NMDA */
        float range = 0.9f - PLASTICITY_DENDRITIC_MEMBRANE_MIN;
        float position = physical.membrane_integrity - PLASTICITY_DENDRITIC_MEMBRANE_MIN;
        float factor = position / range;
        bridge->effects.dendritic.nmda_conductance_mod = 0.5f + 0.5f * factor;
        bridge->effects.dendritic.membrane_factor = factor;
    } else {
        /* Severely damaged → minimal NMDA */
        bridge->effects.dendritic.nmda_conductance_mod = 0.2f;
        bridge->effects.dendritic.membrane_factor = 0.2f;
        bridge->stats.membrane_blocks++;
    }

    /* Scale by sensitivity */
    bridge->effects.dendritic.nmda_conductance_mod = 1.0f -
        (1.0f - bridge->effects.dendritic.nmda_conductance_mod) *
        bridge->config.membrane_sensitivity;

    /* ========================================================================
     * BIOLOGICAL: Ion imbalance affects dendritic spike generation
     * - Na+/K+ gradients required for dendritic spikes
     * - Ionic imbalance → higher spike threshold
     * ======================================================================== */

    if (physical.ion_balance >= 0.9f) {
        bridge->effects.dendritic.spike_threshold_shift = 0.0f;
    } else if (physical.ion_balance >= PLASTICITY_DENDRITIC_ION_MIN) {
        /* Ion imbalance → raised threshold (mV shift) */
        float imbalance = (0.9f - physical.ion_balance) / 0.9f;
        bridge->effects.dendritic.spike_threshold_shift = imbalance * 10.0f; /* Up to +10mV */
    } else {
        /* Severe imbalance */
        bridge->effects.dendritic.spike_threshold_shift = 15.0f;
    }

    /* ========================================================================
     * Calcium influx modulation (depends on membrane and ion balance)
     * ======================================================================== */
    bridge->effects.dendritic.calcium_influx_mod =
        bridge->effects.dendritic.membrane_factor *
        (physical.ion_balance / 0.95f);
    bridge->effects.dendritic.calcium_influx_mod = nimcp_clampf(
        bridge->effects.dendritic.calcium_influx_mod,
        0.2f,
        1.0f
    );

    return 0;
}

/* ============================================================================
 * Update API - Combined Update
 * ============================================================================ */

int plasticity_substrate_update_all(plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Bridge is NULL in update_all");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update all mechanisms */
    if (bridge->config.enable_stdp_modulation) {
        plasticity_substrate_update_stdp(bridge);
    }
    if (bridge->config.enable_bcm_modulation) {
        plasticity_substrate_update_bcm(bridge);
    }
    if (bridge->config.enable_homeostatic_modulation) {
        plasticity_substrate_update_homeostatic(bridge);
    }
    if (bridge->config.enable_eligibility_modulation) {
        plasticity_substrate_update_eligibility(bridge);
    }
    if (bridge->config.enable_dendritic_modulation) {
        plasticity_substrate_update_dendritic(bridge);
    }

    /* ========================================================================
     * Compute global learning rate modulation
     * Combine STDP and BCM factors (they should be similar)
     * ======================================================================== */
    float lr_factors[2];
    int num_factors = 0;

    if (bridge->config.enable_stdp_modulation) {
        lr_factors[num_factors++] = bridge->effects.stdp.learning_rate_mod;
    }
    if (bridge->config.enable_bcm_modulation) {
        lr_factors[num_factors++] = bridge->effects.bcm.learning_rate_mod;
    }

    if (num_factors > 0) {
        float sum = 0.0f;
        for (int i = 0; i < num_factors; i++) {
            sum += lr_factors[i];
        }
        bridge->effects.global_learning_rate = sum / num_factors;
    } else {
        bridge->effects.global_learning_rate = 1.0f;
    }

    /* Clamp to valid range */
    bridge->effects.global_learning_rate = nimcp_clampf(
        bridge->effects.global_learning_rate,
        PLASTICITY_LR_MIN_FACTOR,
        PLASTICITY_LR_MAX_FACTOR
    );

    /* ========================================================================
     * Compute overall plasticity capacity (from substrate)
     * ======================================================================== */
    bridge->effects.plasticity_capacity = substrate_get_capacity(bridge->substrate);

    /* ========================================================================
     * Update statistics
     * ======================================================================== */
    bridge->stats.total_updates++;

    /* Track min/max learning rate */
    if (bridge->effects.global_learning_rate < bridge->stats.min_learning_rate_factor) {
        bridge->stats.min_learning_rate_factor = bridge->effects.global_learning_rate;
    }
    if (bridge->effects.global_learning_rate > bridge->stats.max_learning_rate_factor) {
        bridge->stats.max_learning_rate_factor = bridge->effects.global_learning_rate;
    }

    /* Running average of plasticity capacity */
    float alpha = 0.01f;
    bridge->stats.avg_plasticity_capacity =
        (1.0f - alpha) * bridge->stats.avg_plasticity_capacity +
        alpha * bridge->effects.plasticity_capacity;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float plasticity_substrate_get_learning_rate_mod(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }
    return bridge->effects.global_learning_rate;
}

float plasticity_substrate_get_stdp_window_mod(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }
    /* Average of LTP and LTD window modulation */
    return (bridge->effects.stdp.tau_plus_mod + bridge->effects.stdp.tau_minus_mod) / 2.0f;
}

float plasticity_substrate_get_bcm_threshold_shift(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }
    return bridge->effects.bcm.threshold_shift;
}

float plasticity_substrate_get_homeostatic_adjustment(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }
    return bridge->effects.homeostatic.target_rate_adjustment;
}

float plasticity_substrate_get_eligibility_decay_mod(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }
    return bridge->effects.eligibility.decay_lambda_mod;
}

float plasticity_substrate_get_nmda_conductance_mod(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }
    return bridge->effects.dendritic.nmda_conductance_mod;
}

int plasticity_substrate_get_effects(
    const plasticity_substrate_bridge_t* bridge,
    plasticity_substrate_effects_t* effects)
{
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in get_effects");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    *effects = bridge->effects;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return 0;
}

bool plasticity_substrate_is_limited(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    /* Check if any modulation factor is significantly below 1.0 */
    if (bridge->effects.global_learning_rate < 0.8f) {
        return true;
    }
    if (bridge->effects.stdp.atp_gating < 0.8f) {
        return true;
    }
    if (bridge->effects.dendritic.nmda_conductance_mod < 0.8f) {
        return true;
    }

    return false;
}

float plasticity_substrate_get_capacity(const plasticity_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }
    return bridge->effects.plasticity_capacity;
}

int plasticity_substrate_get_stats(
    const plasticity_substrate_bridge_t* bridge,
    plasticity_substrate_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL pointer in get_stats");
        return -1;
    }

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)bridge->base.mutex);

    return 0;
}

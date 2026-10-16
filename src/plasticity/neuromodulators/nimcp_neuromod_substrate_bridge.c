/**
 * @file nimcp_neuromod_substrate_bridge.c
 * @brief Neural Substrate-Neuromodulator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "plasticity/neuromodulators/nimcp_neuromod_substrate_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuromod_substrate_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(neuromod_substrate_bridge)


/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Compute Q10 temperature scaling factor
 *
 * WHAT: Calculate rate scaling based on temperature deviation
 * WHY:  Biochemical processes scale with temperature (Arrhenius equation)
 * HOW:  factor = Q10^((T - T_ref) / 10)
 *
 * @param temperature Current temperature (°C)
 * @param q10 Q10 coefficient
 * @return Scaling factor [0.5-2.0 typically]
 */
static float compute_q10_factor(float temperature, float q10)
{
    float delta_t = temperature - REFERENCE_TEMPERATURE;
    return powf(q10, delta_t / 10.0f);
}

/**
 * @brief Compute ATP-dependent synthesis modulation
 *
 * WHAT: Sigmoid curve for ATP → synthesis rate
 * WHY:  Enzymes require ATP; synthesis drops sharply below threshold
 * HOW:  Piecewise: normal above threshold, impaired below
 *
 * @param atp_level Current ATP [0-1]
 * @return Synthesis rate multiplier [0-1.5]
 */
static float compute_atp_synthesis_factor(float atp_level)
{
    if (atp_level >= ATP_SYNTHESIS_THRESHOLD) {
        /* Above threshold: can boost synthesis */
        float boost = (atp_level - ATP_SYNTHESIS_THRESHOLD) /
                     (1.0f - ATP_SYNTHESIS_THRESHOLD);
        return 1.0f + boost * (ATP_SYNTHESIS_MAX_BOOST - 1.0f);
    } else if (atp_level >= ATP_SYNTHESIS_CRITICAL) {
        /* Between critical and threshold: linear reduction */
        return (atp_level - ATP_SYNTHESIS_CRITICAL) /
               (ATP_SYNTHESIS_THRESHOLD - ATP_SYNTHESIS_CRITICAL);
    } else {
        /* Below critical: minimal synthesis */
        return 0.1f * (atp_level / ATP_SYNTHESIS_CRITICAL);
    }
}

/**
 * @brief Compute calcium-dependent release modulation
 *
 * WHAT: Linear scaling for Ca2+ → release probability
 * WHY:  Vesicle fusion is Ca2+-triggered
 * HOW:  Linear ramp from critical to threshold
 *
 * @param ca_homeostasis Calcium homeostasis [0-1]
 * @return Release probability multiplier [0-1.5]
 */
static float compute_calcium_release_factor(float ca_homeostasis)
{
    if (ca_homeostasis >= CA_RELEASE_THRESHOLD) {
        /* Above threshold: can boost release */
        float boost = (ca_homeostasis - CA_RELEASE_THRESHOLD) /
                     (1.0f - CA_RELEASE_THRESHOLD);
        return 1.0f + boost * (CA_RELEASE_MAX_BOOST - 1.0f);
    } else if (ca_homeostasis >= CA_RELEASE_CRITICAL) {
        /* Between critical and threshold: linear reduction */
        return (ca_homeostasis - CA_RELEASE_CRITICAL) /
               (CA_RELEASE_THRESHOLD - CA_RELEASE_CRITICAL);
    } else {
        /* Below critical: minimal release */
        return 0.1f * (ca_homeostasis / CA_RELEASE_CRITICAL);
    }
}

/**
 * @brief Compute ion gradient-dependent reuptake efficiency
 *
 * WHAT: Quadratic scaling for ion balance → reuptake
 * WHY:  Transporters require Na+ gradient; dysfunction is nonlinear
 * HOW:  efficiency = ion_balance^2
 *
 * @param ion_balance Ion homeostasis [0-1]
 * @return Reuptake efficiency [0-1]
 */
static float compute_ion_reuptake_factor(float ion_balance)
{
    if (ion_balance >= ION_REUPTAKE_THRESHOLD) {
        return 1.0f;
    } else if (ion_balance >= ION_REUPTAKE_CRITICAL) {
        /* Quadratic degradation */
        return ion_balance * ion_balance;
    } else {
        /* Severe impairment */
        return 0.1f * (ion_balance / ION_REUPTAKE_CRITICAL);
    }
}

/**
 * @brief Get substrate effects pointer for neuromodulator type
 *
 * @param bridge Bridge
 * @param neuromod_type Neuromodulator type
 * @return Pointer to effects structure or NULL
 */
static substrate_neuromod_effects_t* get_effects_for_type(
    neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    switch (neuromod_type) {
        case NEUROMOD_BRIDGE_DOPAMINE:
            return &bridge->effects.dopamine;
        case NEUROMOD_BRIDGE_SEROTONIN:
            return &bridge->effects.serotonin;
        case NEUROMOD_BRIDGE_ACETYLCHOLINE:
            return &bridge->effects.acetylcholine;
        case NEUROMOD_BRIDGE_NOREPINEPHRINE:
            return &bridge->effects.norepinephrine;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_effects_for_type: operation failed");
            return NULL;
    }
}

/**
 * @brief Compute composite modulation for a single neuromodulator
 *
 * @param effects Effects structure to update
 */
static void compute_composite_modulation(substrate_neuromod_effects_t* effects)
{
    if (!effects) {
        return;
    }

    /* Synthesis: ATP × temperature */
    effects->overall_synthesis_mod =
        effects->atp_synthesis_factor * effects->temp_synthesis_factor;

    /* Release: calcium × temperature */
    effects->overall_release_mod =
        effects->calcium_release_factor * effects->temp_reuptake_factor;

    /* Reuptake: ion gradient × temperature */
    effects->overall_reuptake_mod =
        effects->ion_reuptake_factor * effects->temp_reuptake_factor;

    /* Overall capacity: minimum of all factors */
    float min_capacity = effects->overall_synthesis_mod;
    if (effects->overall_release_mod < min_capacity) {
        min_capacity = effects->overall_release_mod;
    }
    if (effects->overall_reuptake_mod < min_capacity) {
        min_capacity = effects->overall_reuptake_mod;
    }
    effects->overall_capacity = min_capacity;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int neuromod_substrate_default_config(neuromod_substrate_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Enable all features by default */
    config->enable_atp_synthesis_modulation = true;
    config->enable_calcium_release_modulation = true;
    config->enable_temperature_modulation = true;
    config->enable_ion_reuptake_modulation = true;
    config->enable_metabolic_feedback = true;
    config->enable_bio_async = true;

    /* Default sensitivity (1.0 = normal) */
    config->atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->calcium_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->temperature_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->ion_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Biological Q10 coefficients */
    config->q10_synthesis = Q10_SYNTHESIS;
    config->q10_degradation = Q10_DEGRADATION;
    config->q10_reuptake = Q10_REUPTAKE;
    config->q10_receptor = Q10_RECEPTOR_BINDING;

    return NIMCP_SUCCESS;
}

neuromod_substrate_bridge_t* neuromod_substrate_bridge_create(
    const neuromod_substrate_config_t* config,
    neural_substrate_t* substrate,
    neuromodulator_system_t neuromod_system
)
{
    /* Guard: validate inputs */
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_substrate_bridge_create: substrate is NULL");
        NIMCP_LOGGING_ERROR("NULL substrate pointer");
        return NULL;
    }
    if (!neuromod_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neuromod_substrate_bridge_create: neuromod_system is NULL");
        NIMCP_LOGGING_ERROR("NULL neuromodulator system pointer");
        return NULL;
    }

    /* Allocate bridge structure */
    neuromod_substrate_bridge_t* bridge =
        (neuromod_substrate_bridge_t*)nimcp_malloc(sizeof(neuromod_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_substrate_bridge_create: failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(neuromod_substrate_bridge_t));

    /* Store system handles */
    bridge->substrate = substrate;
    bridge->neuromod_system = neuromod_system;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        neuromod_substrate_default_config(&bridge->config);
    }

    /* Create mutex */
    bridge->base.mutex = (nimcp_platform_mutex_t*)nimcp_malloc(sizeof(nimcp_platform_mutex_t));
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_substrate_bridge_create: failed to allocate mutex");
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(bridge);
        return NULL;
    }

    if (nimcp_platform_mutex_init(bridge->base.mutex, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "neuromod_substrate_bridge_create: failed to initialize mutex");
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize all effects to normal (1.0) */
    for (int i = 0; i < NEUROMOD_BRIDGE_COUNT; i++) {
        substrate_neuromod_effects_t* effects = get_effects_for_type(bridge, i);
        if (effects) {
            effects->atp_synthesis_factor = 1.0f;
            effects->calcium_release_factor = 1.0f;
            effects->temp_synthesis_factor = 1.0f;
            effects->temp_degradation_factor = 1.0f;
            effects->temp_reuptake_factor = 1.0f;
            effects->temp_receptor_factor = 1.0f;
            effects->ion_reuptake_factor = 1.0f;
            effects->overall_synthesis_mod = 1.0f;
            effects->overall_release_mod = 1.0f;
            effects->overall_reuptake_mod = 1.0f;
            effects->overall_capacity = 1.0f;
        }
    }

    /* Connect bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        neuromod_substrate_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Neuromodulator-substrate bridge created");
    return bridge;
}

void neuromod_substrate_bridge_destroy(neuromod_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        neuromod_substrate_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_platform_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Neuromodulator-substrate bridge destroyed");
}

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

int neuromod_substrate_connect_bio_async(neuromod_substrate_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_NEUROMOD_SUBSTRATE,
        .module_name = "neuromod_substrate_bridge",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return NIMCP_SUCCESS;
}

int neuromod_substrate_disconnect_bio_async(neuromod_substrate_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return NIMCP_SUCCESS;
}

bool neuromod_substrate_is_bio_async_connected(const neuromod_substrate_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Substrate → Neuromodulation Effects
 * ============================================================================ */

int neuromod_substrate_compute_atp_effects(neuromod_substrate_bridge_t* bridge)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->substrate, NIMCP_ERROR_INVALID_STATE, "bridge->substrate is NULL");
    if (!bridge->config.enable_atp_synthesis_modulation) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get substrate metabolic state */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute ATP-dependent synthesis factor */
    float atp_factor = compute_atp_synthesis_factor(metabolic.atp_level);
    atp_factor *= bridge->config.atp_sensitivity;

    /* Apply to all neuromodulators */
    for (int i = 0; i < NEUROMOD_BRIDGE_COUNT; i++) {
        substrate_neuromod_effects_t* effects = get_effects_for_type(bridge, i);
        if (effects) {
            effects->atp_synthesis_factor = atp_factor;
        }
    }

    /* Track statistics */
    if (metabolic.atp_level < ATP_SYNTHESIS_CRITICAL) {
        bridge->stats.atp_depletion_events++;
    }
    if (atp_factor < 1.0f) {
        bridge->stats.synthesis_limited_cycles++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int neuromod_substrate_compute_calcium_effects(neuromod_substrate_bridge_t* bridge)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->substrate, NIMCP_ERROR_INVALID_STATE, "bridge->substrate is NULL");
    if (!bridge->config.enable_calcium_release_modulation) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get substrate physical state */
    substrate_physical_state_t physical;
    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute calcium-dependent release factor */
    float ca_factor = compute_calcium_release_factor(physical.ca_homeostasis);
    ca_factor *= bridge->config.calcium_sensitivity;

    /* Apply to all neuromodulators */
    for (int i = 0; i < NEUROMOD_BRIDGE_COUNT; i++) {
        substrate_neuromod_effects_t* effects = get_effects_for_type(bridge, i);
        if (effects) {
            effects->calcium_release_factor = ca_factor;
        }
    }

    /* Track statistics */
    if (physical.ca_homeostasis < CA_RELEASE_CRITICAL) {
        bridge->stats.calcium_depletion_events++;
    }
    if (ca_factor < 1.0f) {
        bridge->stats.release_limited_cycles++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int neuromod_substrate_compute_temperature_effects(neuromod_substrate_bridge_t* bridge)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->substrate, NIMCP_ERROR_INVALID_STATE, "bridge->substrate is NULL");
    if (!bridge->config.enable_temperature_modulation) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get substrate physical state */
    substrate_physical_state_t physical;
    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute Q10-scaled factors */
    float temp_synthesis = compute_q10_factor(physical.temperature, bridge->config.q10_synthesis);
    float temp_degradation = compute_q10_factor(physical.temperature, bridge->config.q10_degradation);
    float temp_reuptake = compute_q10_factor(physical.temperature, bridge->config.q10_reuptake);
    float temp_receptor = compute_q10_factor(physical.temperature, bridge->config.q10_receptor);

    /* Apply sensitivity */
    temp_synthesis = 1.0f + (temp_synthesis - 1.0f) * bridge->config.temperature_sensitivity;
    temp_degradation = 1.0f + (temp_degradation - 1.0f) * bridge->config.temperature_sensitivity;
    temp_reuptake = 1.0f + (temp_reuptake - 1.0f) * bridge->config.temperature_sensitivity;
    temp_receptor = 1.0f + (temp_receptor - 1.0f) * bridge->config.temperature_sensitivity;

    /* Apply to all neuromodulators */
    for (int i = 0; i < NEUROMOD_BRIDGE_COUNT; i++) {
        substrate_neuromod_effects_t* effects = get_effects_for_type(bridge, i);
        if (effects) {
            effects->temp_synthesis_factor = temp_synthesis;
            effects->temp_degradation_factor = temp_degradation;
            effects->temp_reuptake_factor = temp_reuptake;
            effects->temp_receptor_factor = temp_receptor;
        }
    }

    /* Track statistics */
    if (physical.temperature > SUBSTRATE_HYPERTHERMIA_THRESHOLD) {
        bridge->stats.hyperthermia_cycles++;
    }
    if (physical.temperature < SUBSTRATE_HYPOTHERMIA_THRESHOLD) {
        bridge->stats.hypothermia_cycles++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int neuromod_substrate_compute_ion_effects(neuromod_substrate_bridge_t* bridge)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->substrate, NIMCP_ERROR_INVALID_STATE, "bridge->substrate is NULL");
    if (!bridge->config.enable_ion_reuptake_modulation) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get substrate physical state */
    substrate_physical_state_t physical;
    if (substrate_get_physical_state(bridge->substrate, &physical) != 0) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Compute ion-dependent reuptake factor */
    float ion_factor = compute_ion_reuptake_factor(physical.ion_balance);
    ion_factor *= bridge->config.ion_sensitivity;

    /* Apply to all neuromodulators */
    for (int i = 0; i < NEUROMOD_BRIDGE_COUNT; i++) {
        substrate_neuromod_effects_t* effects = get_effects_for_type(bridge, i);
        if (effects) {
            effects->ion_reuptake_factor = ion_factor;
        }
    }

    /* Track statistics */
    if (physical.ion_balance < ION_REUPTAKE_CRITICAL) {
        bridge->stats.ion_imbalance_cycles++;
    }
    if (ion_factor < 1.0f) {
        bridge->stats.reuptake_limited_cycles++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int neuromod_substrate_update_effects(neuromod_substrate_bridge_t* bridge)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Compute all individual effects */
    int result = neuromod_substrate_compute_atp_effects(bridge);
    NIMCP_CHECK_THROW(result == 0, NIMCP_ERROR_OPERATION_FAILED, "ATP effects computation failed");
    result = neuromod_substrate_compute_calcium_effects(bridge);
    NIMCP_CHECK_THROW(result == 0, NIMCP_ERROR_OPERATION_FAILED, "Calcium effects computation failed");
    result = neuromod_substrate_compute_temperature_effects(bridge);
    NIMCP_CHECK_THROW(result == 0, NIMCP_ERROR_OPERATION_FAILED, "Temperature effects computation failed");
    result = neuromod_substrate_compute_ion_effects(bridge);
    NIMCP_CHECK_THROW(result == 0, NIMCP_ERROR_OPERATION_FAILED, "Ion effects computation failed");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute composite modulation for each neuromodulator */
    float total_synthesis = 0.0f;
    float total_release = 0.0f;
    float total_reuptake = 0.0f;
    float min_capacity = 1.0f;

    for (int i = 0; i < NEUROMOD_BRIDGE_COUNT; i++) {
        substrate_neuromod_effects_t* effects = get_effects_for_type(bridge, i);
        if (effects) {
            compute_composite_modulation(effects);
            total_synthesis += effects->overall_synthesis_mod;
            total_release += effects->overall_release_mod;
            total_reuptake += effects->overall_reuptake_mod;
            if (effects->overall_capacity < min_capacity) {
                min_capacity = effects->overall_capacity;
            }
        }
    }

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->stats.avg_synthesis_capacity = total_synthesis / NEUROMOD_BRIDGE_COUNT;
    bridge->stats.avg_release_capacity = total_release / NEUROMOD_BRIDGE_COUNT;
    bridge->stats.avg_reuptake_capacity = total_reuptake / NEUROMOD_BRIDGE_COUNT;

    if (min_capacity < bridge->stats.min_overall_capacity || bridge->stats.min_overall_capacity == 0.0f) {
        bridge->stats.min_overall_capacity = min_capacity;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Neuromodulation → Substrate Feedback
 * ============================================================================ */

int neuromod_substrate_record_synthesis(
    neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->substrate, NIMCP_ERROR_INVALID_STATE, "bridge->substrate is NULL");
    if (!bridge->config.enable_metabolic_feedback) {
        return NIMCP_SUCCESS;
    }
    NIMCP_CHECK_THROW(neuromod_type < NEUROMOD_BRIDGE_COUNT,
                      NIMCP_ERROR_INVALID_PARAM, "neuromod_type %d >= NEUROMOD_BRIDGE_COUNT", neuromod_type);

    /* Get current ATP */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Deplete ATP */
    float new_atp = metabolic.atp_level - COST_PER_SYNTHESIS;
    if (new_atp < 0.0f) {
        new_atp = 0.0f;
    }

    return substrate_set_atp(bridge->substrate, new_atp);
}

int neuromod_substrate_record_release(
    neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type,
    uint32_t vesicle_count
)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->substrate, NIMCP_ERROR_INVALID_STATE, "bridge->substrate is NULL");
    if (!bridge->config.enable_metabolic_feedback) {
        return NIMCP_SUCCESS;
    }
    NIMCP_CHECK_THROW(neuromod_type < NEUROMOD_BRIDGE_COUNT,
                      NIMCP_ERROR_INVALID_PARAM, "neuromod_type %d >= NEUROMOD_BRIDGE_COUNT", neuromod_type);

    /* Get current ATP */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Deplete ATP proportional to vesicle count */
    float cost = COST_PER_RELEASE * vesicle_count;
    float new_atp = metabolic.atp_level - cost;
    if (new_atp < 0.0f) {
        new_atp = 0.0f;
    }

    return substrate_set_atp(bridge->substrate, new_atp);
}

int neuromod_substrate_record_reuptake(
    neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->substrate, NIMCP_ERROR_INVALID_STATE, "bridge->substrate is NULL");
    if (!bridge->config.enable_metabolic_feedback) {
        return NIMCP_SUCCESS;
    }
    NIMCP_CHECK_THROW(neuromod_type < NEUROMOD_BRIDGE_COUNT,
                      NIMCP_ERROR_INVALID_PARAM, "neuromod_type %d >= NEUROMOD_BRIDGE_COUNT", neuromod_type);

    /* Get current ATP */
    substrate_metabolic_state_t metabolic;
    if (substrate_get_metabolic_state(bridge->substrate, &metabolic) != 0) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Deplete ATP (reuptake uses Na+ gradient, which requires ATP pumps) */
    float new_atp = metabolic.atp_level - COST_PER_REUPTAKE;
    if (new_atp < 0.0f) {
        new_atp = 0.0f;
    }

    return substrate_set_atp(bridge->substrate, new_atp);
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float neuromod_substrate_get_synthesis_mod(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    if (!bridge || neuromod_type >= NEUROMOD_BRIDGE_COUNT) {
        return 1.0f;
    }

    const substrate_neuromod_effects_t* effects = get_effects_for_type(
        (neuromod_substrate_bridge_t*)bridge, neuromod_type);

    return effects ? effects->overall_synthesis_mod : 1.0f;
}

float neuromod_substrate_get_release_mod(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    if (!bridge || neuromod_type >= NEUROMOD_BRIDGE_COUNT) {
        return 1.0f;
    }

    const substrate_neuromod_effects_t* effects = get_effects_for_type(
        (neuromod_substrate_bridge_t*)bridge, neuromod_type);

    return effects ? effects->overall_release_mod : 1.0f;
}

float neuromod_substrate_get_reuptake_mod(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    if (!bridge || neuromod_type >= NEUROMOD_BRIDGE_COUNT) {
        return 1.0f;
    }

    const substrate_neuromod_effects_t* effects = get_effects_for_type(
        (neuromod_substrate_bridge_t*)bridge, neuromod_type);

    return effects ? effects->overall_reuptake_mod : 1.0f;
}

float neuromod_substrate_get_capacity(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    if (!bridge || neuromod_type >= NEUROMOD_BRIDGE_COUNT) {
        return 1.0f;
    }

    const substrate_neuromod_effects_t* effects = get_effects_for_type(
        (neuromod_substrate_bridge_t*)bridge, neuromod_type);

    return effects ? effects->overall_capacity : 1.0f;
}

int neuromod_substrate_get_effects(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type,
    substrate_neuromod_effects_t* effects
)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");
    NIMCP_CHECK_THROW(neuromod_type < NEUROMOD_BRIDGE_COUNT,
                      NIMCP_ERROR_INVALID_PARAM, "neuromod_type %d >= NEUROMOD_BRIDGE_COUNT", neuromod_type);

    const substrate_neuromod_effects_t* src = get_effects_for_type(
        (neuromod_substrate_bridge_t*)bridge, neuromod_type);

    NIMCP_CHECK_THROW(src, NIMCP_ERROR_OPERATION_FAILED, "Failed to get effects for neuromod_type %d", neuromod_type);

    *effects = *src;
    return NIMCP_SUCCESS;
}

int neuromod_substrate_get_stats(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_substrate_stats_t* stats
)
{
    /* Guard: validate inputs */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool neuromod_substrate_is_limited(
    const neuromod_substrate_bridge_t* bridge,
    neuromod_bridge_type_t neuromod_type
)
{
    if (!bridge || neuromod_type >= NEUROMOD_BRIDGE_COUNT) {
        return false;
    }

    const substrate_neuromod_effects_t* effects = get_effects_for_type(
        (neuromod_substrate_bridge_t*)bridge, neuromod_type);

    if (!effects) {
        return false;
    }

    /* Limited if any factor is below 0.8 */
    return (effects->overall_synthesis_mod < 0.8f ||
            effects->overall_release_mod < 0.8f ||
            effects->overall_reuptake_mod < 0.8f);
}

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* neuromod_bridge_type_to_string(neuromod_bridge_type_t neuromod_type)
{
    switch (neuromod_type) {
        case NEUROMOD_BRIDGE_DOPAMINE:
            return "Dopamine";
        case NEUROMOD_BRIDGE_SEROTONIN:
            return "Serotonin";
        case NEUROMOD_BRIDGE_ACETYLCHOLINE:
            return "Acetylcholine";
        case NEUROMOD_BRIDGE_NOREPINEPHRINE:
            return "Norepinephrine";
        default:
            return "Unknown";
    }
}

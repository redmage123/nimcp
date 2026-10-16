/**
 * @file nimcp_substrate_immune_bridge.c
 * @brief Neural Substrate-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional substrate-immune coupling
 * WHY:  Inflammation affects metabolic/physical substrate; substrate stress triggers immune
 * HOW:  Apply cytokine effects to substrate; trigger immune from substrate alerts
 */

#include "core/neural_substrate/nimcp_substrate_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(substrate_immune_bridge)

/**
 * @brief Get cytokine concentration from immune system
 */
static float get_cytokine_concentration(
    const brain_immune_system_t* immune,
    brain_cytokine_type_t type
) {
    if (!immune) return 0.0f;

    float total = 0.0f;
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        if (immune->cytokines[i].type == type && immune->cytokines[i].delivered) {
            total += immune->cytokines[i].concentration;
        }
    }

    return nimcp_clampf(total, 0.0f, 1.0f);
}

/**
 * @brief Get max inflammation level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;

    brain_inflammation_level_t max_level = INFLAMMATION_NONE;
    for (size_t i = 0; i < immune->inflammation_count; i++) {
        if (immune->inflammation_sites[i].level > max_level) {
            max_level = immune->inflammation_sites[i].level;
        }
    }

    return max_level;
}

/**
 * @brief Create DAMP epitope from substrate state
 */
static size_t create_damp_epitope(
    const neural_substrate_t* substrate,
    uint8_t* epitope_out,
    size_t max_len
) {
    if (!substrate || !epitope_out || max_len < 16) return 0;

    uint32_t* data = (uint32_t*)epitope_out;
    size_t idx = 0;

    /* Encode substrate state */
    data[idx++] = (uint32_t)(substrate->metabolic.atp_level * 1000.0f);
    data[idx++] = (uint32_t)(substrate->physical.membrane_integrity * 1000.0f);
    data[idx++] = (uint32_t)(substrate->physical.ion_balance * 1000.0f);
    data[idx++] = (uint32_t)substrate->health_level;

    return idx * sizeof(uint32_t);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int substrate_immune_default_config(substrate_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled */
    config->enable_fever_response = true;
    config->enable_metabolic_effects = true;
    config->enable_damage_effects = true;
    config->enable_substrate_immune_trigger = true;
    config->enable_il10_recovery = true;
    config->enable_bio_async = true;

    /* Sensitivity multipliers */
    config->temperature_sensitivity = 1.0f;
    config->metabolic_sensitivity = 1.0f;
    config->damage_sensitivity = 1.0f;

    /* Thresholds */
    config->alert_persistence_threshold = SUBSTRATE_ALERT_IMMUNE_THRESHOLD;
    config->max_fever_temperature = 41.0f;  /* Limit fever to 41°C */

    return 0;
}

substrate_immune_bridge_t* substrate_immune_bridge_create(
    const substrate_immune_config_t* config,
    neural_substrate_t* substrate,
    brain_immune_system_t* immune_system
) {
    /* Guard: require substrate */
    if (!substrate) {
        NIMCP_LOGGING_ERROR("Cannot create bridge without substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

        return NULL;
    }

    substrate_immune_bridge_t* bridge = (substrate_immune_bridge_t*)
        nimcp_calloc(1, sizeof(substrate_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Bridge allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substrate_immune_bridge_create: bridge allocation failed");
        return NULL;
    }

    /* Apply configuration */
    substrate_immune_config_t default_cfg;
    if (!config) {
        substrate_immune_default_config(&default_cfg);
        config = &default_cfg;
    }
    bridge->config = *config;

    /* Link systems */
    bridge->substrate = substrate;
    bridge->immune_system = immune_system;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "substrate_immune") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "substrate_immune_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Mutex allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substrate_immune_bridge_create: mutex allocation failed");
        substrate_immune_bridge_destroy(bridge);
        return NULL;
    }

    /* Try bio-async connection */
    bridge->base.bio_async_enabled = false;
    if (config->enable_bio_async) {
        substrate_immune_connect_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Substrate immune bridge created");
    return bridge;
}

void substrate_immune_bridge_destroy(substrate_immune_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        substrate_immune_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Substrate immune bridge destroyed");
}

/* ============================================================================
 * Bio-async Implementation
 * ============================================================================ */

int substrate_immune_connect_bio_async(substrate_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_SUBSTRATE,
        .module_name = "substrate_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "substrate_immune_connect_bio_async: validation failed");
    return -1;
}

int substrate_immune_disconnect_bio_async(substrate_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->base.bio_async_enabled) return 0;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;

    return 0;
}

bool substrate_immune_is_bio_async_connected(const substrate_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Immune → Substrate Implementation
 * ============================================================================ */

int substrate_immune_apply_fever(substrate_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_apply_fever: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_fever_response) return -1;
    if (!bridge->immune_system) return 0;  /* No immune = no fever */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get cytokine levels */
    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);

    /* Calculate temperature effects */
    float sens = bridge->config.temperature_sensitivity;
    bridge->cytokine_effects.il1_temp_effect = il1 * CYTOKINE_IL1_TEMP_INCREASE * sens;
    bridge->cytokine_effects.il6_temp_effect = il6 * CYTOKINE_IL6_TEMP_INCREASE * sens;

    /* Get inflammation level contribution */
    float inflammation_temp = 0.0f;
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    switch (level) {
        case INFLAMMATION_LOCAL:    inflammation_temp = INFLAMMATION_LOCAL_TEMP; break;
        case INFLAMMATION_REGIONAL: inflammation_temp = INFLAMMATION_REGIONAL_TEMP; break;
        case INFLAMMATION_SYSTEMIC: inflammation_temp = INFLAMMATION_SYSTEMIC_TEMP; break;
        case INFLAMMATION_STORM:    inflammation_temp = INFLAMMATION_STORM_TEMP; break;
        default: break;
    }

    /* Total temperature increase */
    bridge->cytokine_effects.total_temp_increase =
        bridge->cytokine_effects.il1_temp_effect +
        bridge->cytokine_effects.il6_temp_effect +
        inflammation_temp;

    /* Calculate fever intensity */
    bridge->cytokine_effects.fever_intensity =
        nimcp_clampf(bridge->cytokine_effects.total_temp_increase / 3.0f, 0.0f, 1.0f);

    /* Apply to substrate (relative to normal) */
    float new_temp = SUBSTRATE_NORMAL_TEMPERATURE + bridge->cytokine_effects.total_temp_increase;
    new_temp = nimcp_clampf(new_temp, 35.0f, bridge->config.max_fever_temperature);
    substrate_set_temperature(bridge->substrate, new_temp);

    /* Track stats */
    if (bridge->cytokine_effects.fever_intensity > 0.1f) {
        bridge->stats.fever_cycles++;
    }
    if (new_temp > bridge->stats.max_temperature) {
        bridge->stats.max_temperature = new_temp;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int substrate_immune_apply_metabolic_effects(substrate_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_apply_metabolic_effects: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_metabolic_effects) return -1;
    if (!bridge->immune_system) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get cytokine levels */
    float il1 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL6);
    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float ifn = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IFN_GAMMA);

    float sens = bridge->config.metabolic_sensitivity;

    /* Calculate metabolic effects */
    bridge->cytokine_effects.il1_metabolic_effect = il1 * CYTOKINE_IL1_METABOLIC_INCREASE * sens;
    bridge->cytokine_effects.il6_metabolic_effect = il6 * CYTOKINE_IL6_METABOLIC_INCREASE * sens;
    bridge->cytokine_effects.tnf_atp_effect = tnf * CYTOKINE_TNF_ATP_DEPLETION * sens;
    bridge->cytokine_effects.ifn_o2_effect = ifn * CYTOKINE_IFN_O2_CONSUMPTION * sens;

    /* Metabolic burden */
    bridge->cytokine_effects.metabolic_burden = nimcp_clampf(
        bridge->cytokine_effects.il1_metabolic_effect +
        bridge->cytokine_effects.il6_metabolic_effect +
        bridge->cytokine_effects.tnf_atp_effect +
        bridge->cytokine_effects.ifn_o2_effect,
        0.0f, 1.0f
    );

    /* Apply ATP depletion from TNF */
    if (bridge->cytokine_effects.tnf_atp_effect > 0.01f) {
        substrate_metabolic_state_t metabolic;
        substrate_get_metabolic_state(bridge->substrate, &metabolic);

        float new_atp = metabolic.atp_level * (1.0f - bridge->cytokine_effects.tnf_atp_effect);
        substrate_set_atp(bridge->substrate, new_atp);

        if (new_atp < bridge->stats.min_atp_level || bridge->stats.min_atp_level == 0.0f) {
            bridge->stats.min_atp_level = new_atp;
        }
        if (new_atp < 0.5f) {
            bridge->stats.atp_depletions++;
        }
    }

    /* Apply O2 consumption from IFN */
    if (bridge->cytokine_effects.ifn_o2_effect > 0.01f) {
        substrate_metabolic_state_t metabolic;
        substrate_get_metabolic_state(bridge->substrate, &metabolic);

        float new_o2 = metabolic.oxygen_saturation * (1.0f - bridge->cytokine_effects.ifn_o2_effect);
        substrate_set_oxygen(bridge->substrate, new_o2);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int substrate_immune_apply_damage(substrate_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_apply_damage: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_damage_effects) return -1;
    if (!bridge->immune_system) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float tnf = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_TNF);
    float sens = bridge->config.damage_sensitivity;

    /* Membrane damage from TNF/cytotoxicity */
    bridge->cytokine_effects.tnf_membrane_effect = tnf * CYTOKINE_TNF_MEMBRANE_DAMAGE * sens;

    /* Ion imbalance from inflammation */
    brain_inflammation_level_t level = get_max_inflammation_level(bridge->immune_system);
    float ion_effect = 0.0f;
    switch (level) {
        case INFLAMMATION_LOCAL:    ion_effect = 0.02f; break;
        case INFLAMMATION_REGIONAL: ion_effect = 0.05f; break;
        case INFLAMMATION_SYSTEMIC: ion_effect = 0.10f; break;
        case INFLAMMATION_STORM:    ion_effect = 0.20f; break;
        default: break;
    }
    bridge->cytokine_effects.ion_imbalance_effect = ion_effect * sens;

    /* Damage severity */
    bridge->cytokine_effects.damage_severity = nimcp_clampf(
        bridge->cytokine_effects.tnf_membrane_effect +
        bridge->cytokine_effects.ion_imbalance_effect,
        0.0f, 1.0f
    );

    /* Apply membrane damage */
    if (bridge->cytokine_effects.tnf_membrane_effect > 0.01f) {
        substrate_physical_state_t physical;
        substrate_get_physical_state(bridge->substrate, &physical);

        float new_membrane = physical.membrane_integrity *
            (1.0f - bridge->cytokine_effects.tnf_membrane_effect);
        substrate_set_membrane_integrity(bridge->substrate, new_membrane);

        if (new_membrane < 0.8f) {
            bridge->stats.membrane_damages++;
        }
    }

    /* Apply ion imbalance */
    if (bridge->cytokine_effects.ion_imbalance_effect > 0.01f) {
        substrate_physical_state_t physical;
        substrate_get_physical_state(bridge->substrate, &physical);

        float new_ion = physical.ion_balance *
            (1.0f - bridge->cytokine_effects.ion_imbalance_effect);
        substrate_set_ion_balance(bridge->substrate, new_ion);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int substrate_immune_apply_il10_recovery(
    substrate_immune_bridge_t* bridge,
    float il10_concentration
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_apply_il10_recovery: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_il10_recovery) return -1;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    float recovery = nimcp_clampf(il10_concentration, 0.0f, 1.0f);

    /* Temperature reduction */
    substrate_physical_state_t physical;
    substrate_get_physical_state(bridge->substrate, &physical);

    float target_temp = SUBSTRATE_NORMAL_TEMPERATURE;
    float current_temp = physical.temperature;
    float temp_reduction = (current_temp - target_temp) * IL10_TEMPERATURE_REDUCTION * recovery;
    float new_temp = current_temp - temp_reduction;
    substrate_set_temperature(bridge->substrate, new_temp);

    /* ATP recovery boost */
    substrate_metabolic_state_t metabolic;
    substrate_get_metabolic_state(bridge->substrate, &metabolic);

    float atp_boost = IL10_ATP_RECOVERY_BOOST * recovery;
    float new_atp = nimcp_clampf(metabolic.atp_level + atp_boost, 0.0f, 1.0f);
    substrate_set_atp(bridge->substrate, new_atp);

    /* Membrane repair boost */
    float membrane_boost = IL10_MEMBRANE_REPAIR_BOOST * recovery;
    float new_membrane = nimcp_clampf(physical.membrane_integrity + membrane_boost, 0.0f, 1.0f);
    substrate_set_membrane_integrity(bridge->substrate, new_membrane);

    if (recovery > 0.1f) {
        bridge->stats.il10_recoveries++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Substrate → Immune Implementation
 * ============================================================================ */

bool substrate_immune_check_stress(substrate_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    if (!bridge->config.enable_substrate_immune_trigger) return false;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get alerts from substrate */
    substrate_alert_type_t alerts[8];
    uint32_t alert_count;
    substrate_get_alerts(bridge->substrate, alerts, &alert_count);

    /* Reset alert flags */
    bridge->trigger_state.atp_alert = false;
    bridge->trigger_state.membrane_alert = false;
    bridge->trigger_state.ion_alert = false;
    bridge->trigger_state.hyperthermia_alert = false;

    /* Check each alert */
    for (uint32_t i = 0; i < alert_count; i++) {
        switch (alerts[i]) {
            case SUBSTRATE_ALERT_LOW_ATP:
                bridge->trigger_state.atp_alert = true;
                break;
            case SUBSTRATE_ALERT_MEMBRANE_DAMAGE:
                bridge->trigger_state.membrane_alert = true;
                break;
            case SUBSTRATE_ALERT_ION_IMBALANCE:
                bridge->trigger_state.ion_alert = true;
                break;
            case SUBSTRATE_ALERT_HYPERTHERMIA:
                bridge->trigger_state.hyperthermia_alert = true;
                break;
            default:
                break;
        }
    }

    /* Update consecutive alerts */
    if (alert_count >= bridge->config.alert_persistence_threshold) {
        bridge->trigger_state.consecutive_alerts++;
    } else {
        bridge->trigger_state.consecutive_alerts = 0;
    }

    bool should_trigger = (bridge->trigger_state.consecutive_alerts >=
                          bridge->config.alert_persistence_threshold) &&
                         !bridge->trigger_state.immune_triggered;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return should_trigger;
}

int substrate_immune_trigger_response(substrate_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_trigger_response: bridge is NULL");
        return -1;
    }
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_trigger_response: immune_system is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Create DAMP epitope */
    uint8_t epitope[32];
    size_t epitope_len = create_damp_epitope(bridge->substrate, epitope, sizeof(epitope));
    if (epitope_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "substrate_immune_trigger_response: create_damp_epitope failed");
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return -1;
    }

    /* Compute severity */
    bridge->trigger_state.computed_severity = substrate_immune_compute_severity(bridge);

    /* Present DAMP as antigen */
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        epitope_len,
        (uint8_t)bridge->trigger_state.computed_severity,
        0,  /* No specific node */
        &bridge->trigger_state.antigen_id
    );

    if (result == 0) {
        bridge->trigger_state.immune_triggered = true;
        bridge->stats.immune_triggers++;
        NIMCP_LOGGING_INFO("Substrate stress triggered immune response, severity %u",
                          bridge->trigger_state.computed_severity);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return result;
}

uint32_t substrate_immune_compute_severity(const substrate_immune_bridge_t* bridge) {
    if (!bridge) return 1;

    uint32_t severity = 1;

    /* ATP depletion is high severity */
    if (bridge->trigger_state.atp_alert) {
        severity += SUBSTRATE_DAMP_SEVERITY_LOW_ATP;
    }

    /* Membrane damage is high severity */
    if (bridge->trigger_state.membrane_alert) {
        severity += SUBSTRATE_DAMP_SEVERITY_MEMBRANE;
    }

    /* Ion imbalance */
    if (bridge->trigger_state.ion_alert) {
        severity += SUBSTRATE_DAMP_SEVERITY_ION;
    }

    /* Normalize to 1-10 range */
    severity = severity / 3;  /* Average */
    if (severity < 1) severity = 1;
    if (severity > 10) severity = 10;

    return severity;
}

/* ============================================================================
 * Bidirectional Update Implementation
 * ============================================================================ */

int substrate_immune_bridge_update(
    substrate_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    (void)delta_ms;  /* Used for timing if needed */

    /* Apply immune effects to substrate */
    substrate_immune_apply_fever(bridge);
    substrate_immune_apply_metabolic_effects(bridge);
    substrate_immune_apply_damage(bridge);

    /* Check for IL-10 recovery */
    if (bridge->config.enable_il10_recovery && bridge->immune_system) {
        float il10 = get_cytokine_concentration(bridge->immune_system, BRAIN_CYTOKINE_IL10);
        if (il10 > 0.1f) {
            substrate_immune_apply_il10_recovery(bridge, il10);
        }
    }

    /* Check substrate stress */
    if (substrate_immune_check_stress(bridge)) {
        substrate_immune_trigger_response(bridge);
    }

    bridge->stats.total_updates++;
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int substrate_immune_get_cytokine_effects(
    const substrate_immune_bridge_t* bridge,
    cytokine_substrate_effects_t* effects
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_get_cytokine_effects: bridge is NULL");
        return -1;
    }
    if (!effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_get_cytokine_effects: effects is NULL");
        return -1;
    }
    *effects = bridge->cytokine_effects;
    return 0;
}

int substrate_immune_get_trigger_state(
    const substrate_immune_bridge_t* bridge,
    substrate_immune_trigger_t* trigger
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_get_trigger_state: bridge is NULL");
        return -1;
    }
    if (!trigger) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_get_trigger_state: trigger is NULL");
        return -1;
    }
    *trigger = bridge->trigger_state;
    return 0;
}

bool substrate_immune_is_modulated(const substrate_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->cytokine_effects.fever_intensity > 0.05f ||
           bridge->cytokine_effects.metabolic_burden > 0.05f ||
           bridge->cytokine_effects.damage_severity > 0.05f;
}

float substrate_immune_get_fever_intensity(const substrate_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->cytokine_effects.fever_intensity;
}

int substrate_immune_get_stats(
    const substrate_immune_bridge_t* bridge,
    substrate_immune_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_immune_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

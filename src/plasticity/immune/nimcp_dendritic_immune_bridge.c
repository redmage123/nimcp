/**
 * @file nimcp_dendritic_immune_bridge.c
 * @brief Dendritic Plasticity-Immune System Integration Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional coupling between brain immune and dendritic plasticity systems
 * WHY:  Biological realism - cytokines affect spine density, structural damage triggers immunity
 * HOW:  Monitor cytokine levels to modulate dendritic structure, monitor spine loss to trigger immune responses
 */

#include "plasticity/immune/nimcp_dendritic_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dendritic_immune_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(dendritic_immune_bridge)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute dendritic atrophy level from inflammation
 *
 * WHAT: Calculate severity of dendritic atrophy
 * WHY:  Chronic inflammation causes progressive dendritic damage
 * HOW:  Weighted combination of inflammation level and duration
 */
static float compute_atrophy_severity(
    brain_inflammation_level_t level,
    float duration_sec,
    bool is_chronic
) {
    if (level == INFLAMMATION_NONE) return 0.0f;

    /* Inflammation intensity factor */
    float intensity = (float)level / (float)INFLAMMATION_STORM;

    /* Duration factor (saturates at 2x chronic threshold) */
    float duration_factor = 0.0f;
    if (is_chronic) {
        duration_factor = nimcp_clampf(
            duration_sec / (CHRONIC_INFLAMMATION_DENDRITIC_THRESHOLD * 2.0f),
            0.0f, 1.0f
        );
    }

    /* Atrophy severity */
    float atrophy = (intensity * 0.6f) + (duration_factor * 0.4f);
    return nimcp_clampf(atrophy, 0.0f, 1.0f);
}

/**
 * @brief Get inflammation duration
 *
 * WHAT: Calculate how long inflammation has persisted
 * WHY:  Chronic inflammation (>3 days) has different dendritic effects
 * HOW:  Query immune system for inflammation sites
 */
static float get_inflammation_duration_sec(const brain_immune_system_t* immune) {
    if (!immune) return 0.0f;
    /* Would query immune system for inflammation sites */
    /* For now, return 0 - actual implementation would check inflammation_sites array */
    return 0.0f;
}

/**
 * @brief Get current inflammation level
 *
 * WHAT: Get highest inflammation level in system
 * WHY:  Max inflammation determines dendritic impact
 * HOW:  Query immune system for max inflammation site level
 */
static brain_inflammation_level_t get_max_inflammation_level(
    const brain_immune_system_t* immune
) {
    if (!immune) return INFLAMMATION_NONE;
    /* Would query immune system inflammation_sites */
    return INFLAMMATION_NONE;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int dendritic_immune_default_config(dendritic_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* All features enabled by default */
    config->enable_cytokine_dendritic_modulation = true;
    config->enable_inflammation_atrophy = true;
    config->enable_damage_immune_trigger = true;
    config->enable_recovery_immune_support = true;
    config->enable_spine_density_tracking = true;
    config->enable_complexity_tracking = true;

    /* Biologically-based default sensitivities */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->damage_trigger_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Evidence-based thresholds */
    config->spine_loss_trigger_threshold = SPINE_LOSS_IMMUNE_THRESHOLD;
    config->inflammation_atrophy_threshold = INFLAMMATION_ATROPHY_THRESHOLD;

    /* Baseline parameters */
    config->baseline_spine_density = SPINE_DENSITY_HEALTHY;

    return 0;
}

dendritic_immune_bridge_t* dendritic_immune_bridge_create(
    const dendritic_immune_config_t* config,
    brain_immune_system_t* immune_system,
    dendritic_tree_t dendritic_tree
) {
    /* Guard: require immune and dendritic systems */
    if (!immune_system || !dendritic_tree) {
        LOG_MODULE_ERROR("dendritic_immune_bridge",
                  "Cannot create bridge without immune and dendritic systems");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_bridge_create: required parameter is NULL (immune_system, dendritic_tree)");
        return NULL;
    }

    /* Allocate bridge */
    dendritic_immune_bridge_t* bridge = (dendritic_immune_bridge_t*)
        nimcp_malloc(sizeof(dendritic_immune_bridge_t));
    if (!bridge) {
        LOG_MODULE_ERROR("dendritic_immune_bridge", "Allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dendritic_immune_bridge_create: bridge allocation failed");

        return NULL;
    }

    /* Initialize to zero */
    memset(bridge, 0, sizeof(dendritic_immune_bridge_t));

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->dendritic_tree = dendritic_tree;

    /* Apply configuration */
    if (config) {
        bridge->enable_cytokine_dendritic_modulation = config->enable_cytokine_dendritic_modulation;
        bridge->enable_inflammation_atrophy = config->enable_inflammation_atrophy;
        bridge->enable_damage_immune_trigger = config->enable_damage_immune_trigger;
        bridge->enable_recovery_immune_support = config->enable_recovery_immune_support;
        bridge->enable_spine_density_tracking = config->enable_spine_density_tracking;
        bridge->enable_complexity_tracking = config->enable_complexity_tracking;

        /* Initialize baseline spine density */
        bridge->inflammation_state.spine_density = config->baseline_spine_density;
        bridge->inflammation_state.spine_density_baseline = config->baseline_spine_density;
    } else {
        /* Use defaults */
        dendritic_immune_config_t default_cfg;
        dendritic_immune_default_config(&default_cfg);
        bridge->enable_cytokine_dendritic_modulation = default_cfg.enable_cytokine_dendritic_modulation;
        bridge->enable_inflammation_atrophy = default_cfg.enable_inflammation_atrophy;
        bridge->enable_damage_immune_trigger = default_cfg.enable_damage_immune_trigger;
        bridge->enable_recovery_immune_support = default_cfg.enable_recovery_immune_support;
        bridge->enable_spine_density_tracking = default_cfg.enable_spine_density_tracking;
        bridge->enable_complexity_tracking = default_cfg.enable_complexity_tracking;

        bridge->inflammation_state.spine_density = default_cfg.baseline_spine_density;
        bridge->inflammation_state.spine_density_baseline = default_cfg.baseline_spine_density;
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "dendritic_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);    return NULL;
    }

    LOG_MODULE_INFO("dendritic_immune_bridge", "Bridge created successfully");
    return bridge;
}

void dendritic_immune_bridge_destroy(dendritic_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Destroy mutex */
    if (bridge->base.mutex) {
        nimcp_mutex_destroy((nimcp_mutex_t*)bridge->base.mutex);
    }

    /* Free bridge (don't destroy linked systems - we don't own them) */
    nimcp_free(bridge);
    LOG_MODULE_INFO("dendritic_immune_bridge", "Bridge destroyed");
}

/* ============================================================================
 * Immune → Dendritic Implementation
 * ============================================================================ */

int dendritic_immune_apply_cytokine_effects(dendritic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_cytokine_dendritic_modulation) return 0;
    if (!bridge->immune_system || !bridge->dendritic_tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_apply_cytokine_effects: required parameter is NULL (bridge->immune_system, bridge->dendritic_tree)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    /* Compute cytokine effects */
    cytokine_dendritic_effects_t* effects = &bridge->cytokine_effects;

    /* Pro-inflammatory cytokines → spine loss */
    /* Note: Would query actual cytokine levels from immune system */
    effects->il1_spine_loss = 0.0f;  /* IL-1β level * IL1_SPINE_IMPACT */
    effects->il6_spine_loss = 0.0f;  /* IL-6 level * IL6_SPINE_IMPACT */
    effects->tnf_spine_loss = 0.0f;  /* TNF-α level * TNF_SPINE_IMPACT */
    effects->ifn_gamma_spine_loss = 0.0f;

    /* Anti-inflammatory cytokines → spine growth */
    effects->il10_growth_promotion = 0.0f;  /* IL-10 level * IL10_SPINE_IMPACT */

    /* Aggregate effects */
    effects->total_spine_density_change =
        effects->il1_spine_loss +
        effects->il6_spine_loss +
        effects->tnf_spine_loss +
        effects->ifn_gamma_spine_loss +
        effects->il10_growth_promotion;

    /* Complexity reduction from pro-inflammatory cytokines */
    float proinflam_total = fabs(effects->il1_spine_loss) +
                           fabs(effects->il6_spine_loss) +
                           fabs(effects->tnf_spine_loss);
    effects->complexity_reduction = nimcp_clampf(proinflam_total * 0.5f, 0.0f, 1.0f);

    /* Integration impairment (IL-1β specifically affects NMDA) */
    effects->integration_impairment = nimcp_clampf(fabs(effects->il1_spine_loss) * 1.2f, 0.0f, 1.0f);

    /* Growth suppression */
    effects->growth_suppression = nimcp_clampf(proinflam_total * 0.7f, 0.0f, 1.0f);

    /* Update spine density in inflammation state */
    float new_density = bridge->inflammation_state.spine_density + effects->total_spine_density_change;
    bridge->inflammation_state.spine_density = nimcp_clampf(
        new_density,
        SPINE_DENSITY_MIN,
        SPINE_DENSITY_MAX
    );

    /* Track total changes */
    if (effects->total_spine_density_change < 0.0f) {
        bridge->total_spine_loss += fabs(effects->total_spine_density_change);
    } else {
        bridge->total_spine_growth += effects->total_spine_density_change;
    }

    bridge->cytokine_modulations++;
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int dendritic_immune_apply_inflammation_effects(dendritic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_inflammation_atrophy) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_apply_inflammation_effects: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    inflammation_dendritic_state_t* state = &bridge->inflammation_state;

    /* Get inflammation state */
    state->current_level = get_max_inflammation_level(bridge->immune_system);
    state->inflammation_duration_sec = get_inflammation_duration_sec(bridge->immune_system);
    state->is_chronic = (state->inflammation_duration_sec >= CHRONIC_INFLAMMATION_DENDRITIC_THRESHOLD);

    /* Compute atrophy severity */
    state->atrophy_severity = compute_atrophy_severity(
        state->current_level,
        state->inflammation_duration_sec,
        state->is_chronic
    );

    /* Complexity loss based on inflammation level */
    float inflammation_intensity = (float)state->current_level / (float)INFLAMMATION_STORM;
    state->complexity_loss = nimcp_clampf(inflammation_intensity * 0.6f, 0.0f, 1.0f);

    /* NMDA trafficking impairment (IL-1β effect) */
    state->nmda_trafficking_impairment = nimcp_clampf(inflammation_intensity * 0.5f, 0.0f, 1.0f);

    /* Dendritic spike generation impairment */
    state->spike_generation_impairment = nimcp_clampf(inflammation_intensity * 0.4f, 0.0f, 1.0f);

    /* Calcium dysregulation */
    state->calcium_dysregulation = nimcp_clampf(inflammation_intensity * 0.7f, 0.0f, 1.0f);

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

float dendritic_immune_compute_spine_loss(const dendritic_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Spine loss from baseline */
    float baseline = bridge->inflammation_state.spine_density_baseline;
    float current = bridge->inflammation_state.spine_density;
    float loss = baseline - current;

    return nimcp_clampf(loss, 0.0f, 1.0f);
}

float dendritic_immune_compute_complexity_loss(const dendritic_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->inflammation_state.complexity_loss;
}

/* ============================================================================
 * Dendritic → Immune Implementation
 * ============================================================================ */

int dendritic_immune_trigger_from_spine_loss(dendritic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_damage_immune_trigger) return 0;
    if (!bridge->immune_system || !bridge->dendritic_tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_trigger_from_spine_loss: required parameter is NULL (bridge->immune_system, bridge->dendritic_tree)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    dendritic_immune_trigger_t* trigger = &bridge->damage_trigger;

    /* Compute spine loss rate */
    float spine_loss = dendritic_immune_compute_spine_loss(bridge);
    trigger->spine_loss_rate = spine_loss;  /* Would normalize by time */

    /* Compute pruning rate */
    trigger->pruning_rate = spine_loss * 1.2f;  /* Pruning slightly higher than loss */
    trigger->pruning_rate = nimcp_clampf(trigger->pruning_rate, 0.0f, 1.0f);

    /* Complexity reduction rate */
    trigger->complexity_reduction_rate = bridge->inflammation_state.complexity_loss;

    /* Check if spine loss exceeds threshold */
    if (trigger->spine_loss_rate >= SPINE_LOSS_IMMUNE_THRESHOLD) {
        trigger->microglial_surveillance_active = true;
        trigger->synaptic_debris_detected = true;

        /* Compute danger signal strength */
        trigger->danger_signal_strength = nimcp_clampf(
            (trigger->spine_loss_rate - SPINE_LOSS_IMMUNE_THRESHOLD) * 2.0f,
            0.0f,
            1.0f
        );

        /* Trigger cytokine release in immune system */
        /* Note: Would call brain_immune_release_cytokine() for IL-1, IL-6 */

        bridge->damage_triggered_responses++;
    } else {
        trigger->microglial_surveillance_active = false;
        trigger->synaptic_debris_detected = false;
        trigger->danger_signal_strength = 0.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int dendritic_immune_trigger_from_damage(dendritic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_damage_immune_trigger) return 0;
    if (!bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_trigger_from_damage: bridge->immune_system is NULL");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    dendritic_immune_trigger_t* trigger = &bridge->damage_trigger;

    /* Check calcium dysregulation */
    float calcium_problem = bridge->inflammation_state.calcium_dysregulation;

    /* Check complexity loss */
    float complexity_problem = bridge->inflammation_state.complexity_loss;

    /* Homeostatic scaling dysfunction */
    if (calcium_problem > 0.5f || complexity_problem > 0.6f) {
        trigger->scaling_dysfunction = true;
        trigger->compensation_failure = nimcp_clampf(
            (calcium_problem * 0.6f) + (complexity_problem * 0.4f),
            0.0f,
            1.0f
        );

        /* Would trigger immune response */
        bridge->damage_triggered_responses++;
    } else {
        trigger->scaling_dysfunction = false;
        trigger->compensation_failure = 0.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

int dendritic_immune_support_from_health(dendritic_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->enable_recovery_immune_support) return 0;
    if (!bridge->immune_system || !bridge->dendritic_tree) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_support_from_health: required parameter is NULL (bridge->immune_system, bridge->dendritic_tree)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);

    dendritic_immune_support_t* support = &bridge->recovery_support;

    /* Compute structural health */
    support->spine_density_health = bridge->inflammation_state.spine_density;
    support->complexity_health = 1.0f - bridge->inflammation_state.complexity_loss;

    /* Growth rate (positive if spine density > baseline) */
    float density_diff = bridge->inflammation_state.spine_density -
                        bridge->inflammation_state.spine_density_baseline;
    support->growth_rate = (density_diff > 0.0f) ? density_diff : 0.0f;

    /* Immune support from healthy dendrites */
    float health_score = (support->spine_density_health * 0.6f) +
                        (support->complexity_health * 0.4f);
    support->immune_support = nimcp_clampf(health_score, 0.0f, 1.0f);

    /* Healthy dendrites promote IL-10 release */
    if (support->immune_support > 0.7f) {
        support->il10_release_trigger = (support->immune_support - 0.7f) / 0.3f;
        support->inflammation_clearance = support->il10_release_trigger * 0.5f;

        /* Would call brain_immune_release_cytokine(CYTOKINE_IL10) */

        bridge->recovery_supports++;
    } else {
        support->il10_release_trigger = 0.0f;
        support->inflammation_clearance = 0.0f;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int dendritic_immune_bridge_update(
    dendritic_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Apply all bidirectional effects */

    /* Immune → Dendritic */
    dendritic_immune_apply_cytokine_effects(bridge);
    dendritic_immune_apply_inflammation_effects(bridge);

    /* Dendritic → Immune */
    dendritic_immune_trigger_from_spine_loss(bridge);
    dendritic_immune_trigger_from_damage(bridge);
    dendritic_immune_support_from_health(bridge);

    bridge->total_updates++;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int dendritic_immune_get_cytokine_effects(
    const dendritic_immune_bridge_t* bridge,
    cytokine_dendritic_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cytokine_dendritic_effects_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

int dendritic_immune_get_inflammation_state(
    const dendritic_immune_bridge_t* bridge,
    inflammation_dendritic_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dendritic_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(inflammation_dendritic_state_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)bridge->base.mutex);

    return 0;
}

bool dendritic_immune_is_atrophy(const dendritic_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Atrophy threshold */
    return bridge->inflammation_state.atrophy_severity >= INFLAMMATION_ATROPHY_THRESHOLD;
}

float dendritic_immune_get_spine_density(const dendritic_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->inflammation_state.spine_density;
}

float dendritic_immune_get_complexity_loss(const dendritic_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->inflammation_state.complexity_loss;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

#define DENDRITIC_IMMUNE_MODULE_NAME "dendritic_immune_bridge"

/**
 * @brief Connect bridge to bio-async router
 */
int dendritic_immune_connect_bio_async(dendritic_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_DENDRITIC,
        .module_name = DENDRITIC_IMMUNE_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("dendritic_immune_bridge connected to bio-async router");
    } else {
        NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");
    }

    return 0;
}

/**
 * @brief Disconnect from bio-async router
 */
int dendritic_immune_disconnect_bio_async(dendritic_immune_bridge_t* bridge) {
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

    NIMCP_LOGGING_DEBUG("dendritic_immune_bridge disconnected from bio-async router");
    return 0;
}

/**
 * @brief Check if bio-async is connected
 */
bool dendritic_immune_is_bio_async_connected(const dendritic_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

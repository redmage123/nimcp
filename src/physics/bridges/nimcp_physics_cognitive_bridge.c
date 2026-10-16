#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_physics_cognitive_bridge.c - Physics Layer to Cognitive Layer Bridge
//=============================================================================
/**
 * @file nimcp_physics_cognitive_bridge.c
 * @brief Implementation of physics-cognitive bidirectional bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_physics_cognitive_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(physics_cognitive_bridge)

#define LOG_MODULE "PHYSICS_COGNITIVE_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct physics_cog_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Configuration */
    physics_cog_config_t config;

    /** Connected physics modules */
    nimcp_thermodynamic_state_t* thermo;
    nimcp_hh_population_t* hh_pop;
    nimcp_ephaptic_system_t* ephaptic;

    /** Current physics state */
    physics_cog_state_t current_state;

    /** Current capacity */
    physics_cog_capacity_t current_capacity;

    /** Current feedback */
    physics_cog_feedback_t current_feedback;

    /** Statistics */
    physics_cog_stats_t stats;

    /** Update timer */
    float update_timer;

    /** Running averages */
    float capacity_sum;
    float attention_sum;
    uint64_t sample_count;

    /** Initialized flag */
    bool initialized;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute speed factor from firing rate and temperature
 */
static float compute_speed_factor(float firing_rate, float temp_deviation) {
    /* Higher firing rate = faster processing (up to a point) */
    float rate_factor = 0.5f + 0.5f * fminf(firing_rate / 50.0f, 1.0f);

    /* Temperature deviation impairs speed */
    float temp_factor = 1.0f - 0.1f * fabsf(temp_deviation);
    if (temp_factor < 0.5f) temp_factor = 0.5f;

    return rate_factor * temp_factor;
}

/**
 * @brief Compute capacity factor from ATP level
 */
static float compute_capacity_factor(float atp_level, float impair_thresh) {
    if (atp_level >= impair_thresh) {
        /* Above threshold: normal to enhanced */
        float excess = (atp_level - impair_thresh) / (1.0f - impair_thresh);
        return 1.0f + 0.3f * excess;  /* Up to 1.3x */
    } else {
        /* Below threshold: impaired */
        return 0.3f + 0.7f * (atp_level / impair_thresh);  /* Down to 0.3x */
    }
}

/**
 * @brief Compute binding factor from coherence
 */
static float compute_binding_factor(float coherence, float thresh) {
    if (coherence >= thresh) {
        /* High coherence: enhanced binding */
        float enhancement = (coherence - thresh) / (1.0f - thresh);
        return 1.0f + 0.5f * enhancement;  /* Up to 1.5x */
    } else {
        /* Low coherence: reduced binding */
        return 0.5f + 0.5f * (coherence / thresh);  /* Down to 0.5x */
    }
}

/**
 * @brief Compute control factor from synchrony
 */
static float compute_control_factor(float synchrony) {
    /* Moderate synchrony is optimal (not too low, not too high) */
    float optimal_sync = 0.5f;
    float deviation = fabsf(synchrony - optimal_sync);
    float factor = 1.0f - deviation;
    if (factor < 0.5f) factor = 0.5f;
    return factor;
}

/**
 * @brief Compute geometric mean of factors
 */
static float compute_geometric_mean(float* factors, uint32_t count) {
    float product = 1.0f;
    for (uint32_t i = 0; i < count; i++) {
        product *= factors[i];
    }
    return powf(product, 1.0f / (float)count);
}

//=============================================================================
// Configuration API
//=============================================================================

int physics_cog_default_config(physics_cog_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    config->enable_physics_to_cog = true;
    config->enable_cog_to_physics = true;
    config->enable_atp_scaling = true;
    config->enable_coherence_binding = true;
    config->enable_temperature_effects = true;
    config->attention_gain = PHYSICS_COG_ATTENTION_GAIN;
    config->arousal_gain = PHYSICS_COG_AROUSAL_GAIN;
    config->atp_impairment_threshold = PHYSICS_COG_ATP_IMPAIR;
    config->coherence_threshold = PHYSICS_COG_COHERENCE_THRESH;
    config->update_interval_ms = 50.0f;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

physics_cog_bridge_t* physics_cog_bridge_create(
    const physics_cog_config_t* config
) {
    physics_cog_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate physics-cognitive bridge");
    if (bridge_base_init(&bridge->base, 0, "physics_cognitive") != 0) { nimcp_free(bridge); return NULL; }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        physics_cog_default_config(&bridge->config);
    }

    /* Initialize state to defaults */
    bridge->current_state.atp_level = 1.0f;
    bridge->current_state.excitability = 0.5f;
    bridge->current_state.phase_coherence = 0.5f;

    /* Initialize capacity to baseline */
    bridge->current_capacity.speed_factor = 1.0f;
    bridge->current_capacity.capacity_factor = 1.0f;
    bridge->current_capacity.attention_factor = 1.0f;
    bridge->current_capacity.binding_factor = 1.0f;
    bridge->current_capacity.control_factor = 1.0f;
    bridge->current_capacity.overall_efficiency = 1.0f;

    /* Initialize feedback to baseline */
    bridge->current_feedback.attention_level = 0.5f;
    bridge->current_feedback.arousal_level = 0.5f;
    bridge->current_feedback.wm_load = 0.0f;
    bridge->current_feedback.engagement = 0.5f;
    bridge->current_feedback.stress_level = 0.0f;
    bridge->current_feedback.excitability_mod = 1.0f;
    bridge->current_feedback.metabolic_demand = 0.5f;

    bridge->initialized = true;

    NIMCP_LOG_INFO(PHYSICS_COG_MODULE_NAME,
        "Physics-cognitive bridge created: phys_to_cog=%d, cog_to_phys=%d",
        bridge->config.enable_physics_to_cog,
        bridge->config.enable_cog_to_physics);

    return bridge;
}

void physics_cog_bridge_destroy(physics_cog_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "physics_cognitive");

    NIMCP_LOG_INFO(PHYSICS_COG_MODULE_NAME,
        "Bridge destroyed - physics_to_cog: %lu, cog_to_physics: %lu, impairments: %lu",
        (unsigned long)bridge->stats.physics_to_cog_count,
        (unsigned long)bridge->stats.cog_to_physics_count,
        (unsigned long)bridge->stats.impairment_events);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int physics_cog_connect_physics(
    physics_cog_bridge_t* bridge,
    nimcp_thermodynamic_state_t* thermo,
    nimcp_hh_population_t* hh_pop,
    nimcp_ephaptic_system_t* ephaptic
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->thermo = thermo;
    bridge->hh_pop = hh_pop;
    bridge->ephaptic = ephaptic;

    NIMCP_LOG_DEBUG(PHYSICS_COG_MODULE_NAME,
        "Connected physics: thermo=%p, hh=%p, ephaptic=%p",
        (void*)thermo, (void*)hh_pop, (void*)ephaptic);

    return 0;
}

int physics_cog_reset(physics_cog_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Reset state */
    memset(&bridge->current_state, 0, sizeof(bridge->current_state));
    bridge->current_state.atp_level = 1.0f;
    bridge->current_state.excitability = 0.5f;
    bridge->current_state.phase_coherence = 0.5f;

    /* Reset capacity to baseline */
    bridge->current_capacity.speed_factor = 1.0f;
    bridge->current_capacity.capacity_factor = 1.0f;
    bridge->current_capacity.attention_factor = 1.0f;
    bridge->current_capacity.binding_factor = 1.0f;
    bridge->current_capacity.control_factor = 1.0f;
    bridge->current_capacity.overall_efficiency = 1.0f;
    bridge->current_capacity.impaired = false;
    bridge->current_capacity.impairment_reason = 0;

    /* Reset feedback to baseline */
    bridge->current_feedback.attention_level = 0.5f;
    bridge->current_feedback.arousal_level = 0.5f;
    bridge->current_feedback.wm_load = 0.0f;
    bridge->current_feedback.engagement = 0.5f;
    bridge->current_feedback.excitability_mod = 1.0f;

    /* Reset stats */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->update_timer = 0.0f;
    bridge->capacity_sum = 0.0f;
    bridge->attention_sum = 0.0f;
    bridge->sample_count = 0;

    NIMCP_LOG_DEBUG(PHYSICS_COG_MODULE_NAME, "Bridge reset");

    return 0;
}

//=============================================================================
// Physics -> Cognitive API
//=============================================================================

int physics_cog_report_state(
    physics_cog_bridge_t* bridge,
    physics_cog_state_t* state
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Sample HH population */
    if (bridge->hh_pop) {
        float rate;
        if (nimcp_hh_population_get_rate(bridge->hh_pop, &rate) == NIMCP_SUCCESS) {
            bridge->current_state.firing_rate = rate;
        }
    }

    /* Sample thermodynamics */
    if (bridge->thermo) {
        double atp = nimcp_thermo_get_atp_ratio(bridge->thermo);
        bridge->current_state.atp_level = (float)atp;
    }

    /* Sample ephaptic */
    if (bridge->ephaptic) {
        float coherence;
        if (nimcp_ephaptic_get_phase_coherence(bridge->ephaptic, &coherence) == NIMCP_SUCCESS) {
            bridge->current_state.phase_coherence = coherence;
        }
    }

    /* Compute derived values */
    /* Excitability is influenced by attention and arousal */
    bridge->current_state.excitability =
        0.5f + 0.25f * (bridge->current_feedback.attention_level - 0.5f) +
        0.25f * (bridge->current_feedback.arousal_level - 0.5f);

    bridge->stats.physics_to_cog_count++;

    if (state) {
        *state = bridge->current_state;
    }

    return 0;
}

int physics_cog_compute_capacity(
    physics_cog_bridge_t* bridge,
    physics_cog_capacity_t* capacity
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Report state first to get current values */
    physics_cog_report_state(bridge, NULL);

    physics_cog_capacity_t cap;
    memset(&cap, 0, sizeof(cap));

    /* Compute individual factors */
    cap.speed_factor = compute_speed_factor(
        bridge->current_state.firing_rate,
        bridge->current_state.temp_deviation
    );

    if (bridge->config.enable_atp_scaling) {
        cap.capacity_factor = compute_capacity_factor(
            bridge->current_state.atp_level,
            bridge->config.atp_impairment_threshold
        );
    } else {
        cap.capacity_factor = 1.0f;
    }

    /* Attention factor based on current attention level */
    cap.attention_factor = 0.5f + bridge->current_feedback.attention_level;

    if (bridge->config.enable_coherence_binding) {
        cap.binding_factor = compute_binding_factor(
            bridge->current_state.phase_coherence,
            bridge->config.coherence_threshold
        );
    } else {
        cap.binding_factor = 1.0f;
    }

    cap.control_factor = compute_control_factor(bridge->current_state.synchrony);

    /* Clamp all factors */
    cap.speed_factor = fmaxf(PHYSICS_COG_MIN_CAPACITY, fminf(PHYSICS_COG_MAX_CAPACITY, cap.speed_factor));
    cap.capacity_factor = fmaxf(PHYSICS_COG_MIN_CAPACITY, fminf(PHYSICS_COG_MAX_CAPACITY, cap.capacity_factor));
    cap.attention_factor = fmaxf(PHYSICS_COG_MIN_CAPACITY, fminf(PHYSICS_COG_MAX_CAPACITY, cap.attention_factor));
    cap.binding_factor = fmaxf(PHYSICS_COG_MIN_CAPACITY, fminf(PHYSICS_COG_MAX_CAPACITY, cap.binding_factor));
    cap.control_factor = fmaxf(PHYSICS_COG_MIN_CAPACITY, fminf(PHYSICS_COG_MAX_CAPACITY, cap.control_factor));

    /* Compute overall efficiency as geometric mean */
    float factors[5] = {
        cap.speed_factor, cap.capacity_factor, cap.attention_factor,
        cap.binding_factor, cap.control_factor
    };
    cap.overall_efficiency = compute_geometric_mean(factors, 5);

    /* Check for impairment */
    cap.impaired = false;
    cap.impairment_reason = PHYSICS_COG_IMPAIR_NONE;

    if (bridge->current_state.atp_level < bridge->config.atp_impairment_threshold) {
        cap.impaired = true;
        cap.impairment_reason |= PHYSICS_COG_IMPAIR_LOW_ATP;
    }

    if (bridge->current_state.temp_deviation > 2.0f) {
        cap.impaired = true;
        cap.impairment_reason |= PHYSICS_COG_IMPAIR_TEMP_HIGH;
    } else if (bridge->current_state.temp_deviation < -2.0f) {
        cap.impaired = true;
        cap.impairment_reason |= PHYSICS_COG_IMPAIR_TEMP_LOW;
    }

    if (bridge->current_state.phase_coherence < 0.2f) {
        cap.impaired = true;
        cap.impairment_reason |= PHYSICS_COG_IMPAIR_LOW_SYNC;
    }

    /* Track impairment events */
    if (cap.impaired && !bridge->current_capacity.impaired) {
        bridge->stats.impairment_events++;
    }

    /* Track binding enhancement events */
    if (cap.binding_factor > 1.2f &&
        bridge->current_capacity.binding_factor <= 1.2f) {
        bridge->stats.binding_enhancements++;
    }

    /* Update running averages */
    bridge->capacity_sum += cap.overall_efficiency;
    bridge->sample_count++;
    if (bridge->sample_count > 0) {
        bridge->stats.avg_capacity_factor =
            bridge->capacity_sum / (float)bridge->sample_count;
    }

    /* Store result */
    bridge->current_capacity = cap;

    if (capacity) {
        *capacity = cap;
    }

    return 0;
}

float physics_cog_get_factor(
    const physics_cog_bridge_t* bridge,
    physics_cog_factor_t factor
) {
    if (!bridge) return 1.0f;

    switch (factor) {
        case PHYSICS_COG_FACTOR_SPEED:
            return bridge->current_capacity.speed_factor;
        case PHYSICS_COG_FACTOR_CAPACITY:
            return bridge->current_capacity.capacity_factor;
        case PHYSICS_COG_FACTOR_ATTENTION:
            return bridge->current_capacity.attention_factor;
        case PHYSICS_COG_FACTOR_BINDING:
            return bridge->current_capacity.binding_factor;
        case PHYSICS_COG_FACTOR_CONTROL:
            return bridge->current_capacity.control_factor;
        default:
            return 1.0f;
    }
}

//=============================================================================
// Cognitive -> Physics API
//=============================================================================

int physics_cog_receive_feedback(
    physics_cog_bridge_t* bridge,
    const physics_cog_feedback_t* feedback
) {
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_cog_receive_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }

    bridge->current_feedback = *feedback;
    bridge->stats.cog_to_physics_count++;

    /* Update running averages */
    bridge->attention_sum += feedback->attention_level;
    if (bridge->sample_count > 0) {
        bridge->stats.avg_attention_level =
            bridge->attention_sum / (float)bridge->sample_count;
    }

    return 0;
}

int physics_cog_apply_modulation(physics_cog_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.enable_cog_to_physics) return 0;

    /* Compute excitability modulation from attention and arousal */
    float attention_mod = 1.0f +
        bridge->config.attention_gain *
        (bridge->current_feedback.attention_level - 0.5f);

    float arousal_mod = 1.0f +
        bridge->config.arousal_gain *
        (bridge->current_feedback.arousal_level - 0.5f);

    bridge->current_feedback.excitability_mod = attention_mod * arousal_mod;

    /* Compute metabolic demand from engagement and WM load */
    bridge->current_feedback.metabolic_demand =
        0.3f + 0.4f * bridge->current_feedback.engagement +
        0.3f * bridge->current_feedback.wm_load;

    /* In full implementation, would apply to HH population */
    if (bridge->hh_pop) {
        NIMCP_LOG_DEBUG(PHYSICS_COG_MODULE_NAME,
            "Applied modulation: excitability=%.2f, metabolic=%.2f",
            bridge->current_feedback.excitability_mod,
            bridge->current_feedback.metabolic_demand);
    }

    return 0;
}

int physics_cog_set_attention(
    physics_cog_bridge_t* bridge,
    float attention
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->current_feedback.attention_level =
        fmaxf(0.0f, fminf(1.0f, attention));

    return 0;
}

int physics_cog_set_arousal(
    physics_cog_bridge_t* bridge,
    float arousal
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->current_feedback.arousal_level =
        fmaxf(0.0f, fminf(1.0f, arousal));

    return 0;
}

//=============================================================================
// Update API
//=============================================================================

int physics_cog_update(
    physics_cog_bridge_t* bridge,
    float dt
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->update_timer += dt;

    if (bridge->update_timer >= bridge->config.update_interval_ms) {
        bridge->update_timer = 0.0f;

        /* Physics -> Cognitive */
        if (bridge->config.enable_physics_to_cog) {
            physics_cog_report_state(bridge, NULL);
            physics_cog_compute_capacity(bridge, NULL);
        }

        /* Cognitive -> Physics */
        if (bridge->config.enable_cog_to_physics) {
            physics_cog_apply_modulation(bridge);
        }
    }

    bridge->stats.last_update_ms += dt;

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int physics_cog_get_state(
    const physics_cog_bridge_t* bridge,
    physics_cog_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_cog_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->current_state;
    return 0;
}

int physics_cog_get_capacity(
    const physics_cog_bridge_t* bridge,
    physics_cog_capacity_t* capacity
) {
    if (!bridge || !capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_cog_get_capacity: required parameter is NULL (bridge, capacity)");
        return -1;
    }
    *capacity = bridge->current_capacity;
    return 0;
}

int physics_cog_get_feedback(
    const physics_cog_bridge_t* bridge,
    physics_cog_feedback_t* feedback
) {
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_cog_get_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }
    *feedback = bridge->current_feedback;
    return 0;
}

int physics_cog_get_stats(
    const physics_cog_bridge_t* bridge,
    physics_cog_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "physics_cog_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

bool physics_cog_is_impaired(const physics_cog_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->current_capacity.impaired;
}

uint32_t physics_cog_get_impairment(const physics_cog_bridge_t* bridge) {
    if (!bridge) return 0;
    return bridge->current_capacity.impairment_reason;
}

/**
 * @file nimcp_stdp_pr_bridge.c
 * @brief Implementation of STDP ↔ Prime Resonant Memory Bridge
 * @version 1.0.0
 * @date 2026-01-12
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/stdp/nimcp_stdp_pr_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _POSIX_C_SOURCE
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_pr_bridge)

/* Security integration */
#define LOG_MODULE "STDP_PR_BRIDGE"

#endif

//=============================================================================
// Internal Structure
//=============================================================================

struct stdp_pr_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    stdp_pr_bridge_config_t config;
    stdp_pr_bridge_state_t state;
    stdp_pr_bridge_stats_t stats;
    nimcp_platform_mutex_t mutex;
    bool mutex_initialized;
    bool initialized;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(stdp_pr_bridge, struct stdp_pr_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_ms(void) {
#ifdef _POSIX_C_SOURCE
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
    return 0;
#endif
}

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

//=============================================================================
// Configuration Functions
//=============================================================================

stdp_pr_bridge_config_t stdp_pr_bridge_default_config(void) {
    stdp_pr_bridge_config_t config = {
        .resonance_lr_min = STDP_PR_RESONANCE_LR_MIN,
        .resonance_lr_max = STDP_PR_RESONANCE_LR_MAX,
        .resonance_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .consolidation_gate_high = STDP_PR_CONSOLIDATION_GATE_HIGH,
        .consolidation_gate_low = STDP_PR_CONSOLIDATION_GATE_LOW,
        .consolidation_reduction = STDP_PR_CONSOLIDATION_REDUCTION,
        .enable_consolidation_gate = true,

        .ltp_entangle_gain = STDP_PR_LTP_ENTANGLE_GAIN,
        .ltd_entangle_decay = STDP_PR_LTD_ENTANGLE_DECAY,
        .enable_entangle_updates = true,

        .tier_rates = {
            STDP_PR_TIER_Z0_RATE,
            STDP_PR_TIER_Z1_RATE,
            STDP_PR_TIER_Z2_RATE,
            STDP_PR_TIER_Z3_RATE
        },
        .enable_tier_modulation = true,

        .burst_consolidation_gain = STDP_PR_BURST_CONSOLIDATION_GAIN,
        .enable_burst_consolidation = true,

        .enable_bio_async = false
    };
    return config;
}

bool stdp_pr_bridge_validate_config(const stdp_pr_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_validate_config: config is NULL");
        return false;
    }

    if (config->resonance_lr_min < 0.0f || config->resonance_lr_max < config->resonance_lr_min) {
        return false;
    }
    if (config->consolidation_gate_low > config->consolidation_gate_high) {
        return false;
    }
    if (config->ltp_entangle_gain < 0.0f || config->ltd_entangle_decay < 0.0f) {
        return false;
    }
    for (int i = 0; i < STDP_PR_TIER_COUNT; i++) {
        if (config->tier_rates[i] < 0.0f || config->tier_rates[i] > 2.0f) {
            return false;
        }
    }
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

stdp_pr_bridge_t stdp_pr_bridge_create(const stdp_pr_bridge_config_t* config) {
    stdp_pr_bridge_t bridge = nimcp_calloc(1, sizeof(struct stdp_pr_bridge_struct));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_pr_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        if (!stdp_pr_bridge_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_pr_bridge_create: invalid config");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = stdp_pr_bridge_default_config();
    }

    bridge->base.mutex = nimcp_mutex_create(NULL);
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_pr_bridge_create: failed to init mutex");
        nimcp_free(bridge);
        return NULL;
    }
    bridge->mutex_initialized = true;

    /* Initialize state */
    bridge->state.current_resonance = 0.5f;
    bridge->state.current_consolidation = 0.0f;
    bridge->state.current_tier = STDP_PR_TIER_Z0;
    bridge->state.cumulative_entangle_delta = 0.0f;
    bridge->state.last_ltp_time_ms = 0;
    bridge->state.last_ltd_time_ms = 0;
    bridge->state.bridge_coherence = 1.0f;

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created %s bridge", "stdp_pr");
    return bridge;
}

void stdp_pr_bridge_destroy(stdp_pr_bridge_t bridge) {
    if (!bridge) {
        /* Silent return for destroy - idempotent operation */
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "stdp_pr");
        return;
    }

    if (bridge->mutex_initialized) {
        nimcp_mutex_free(bridge->base.mutex);
    }
    nimcp_free(bridge);
}

bool stdp_pr_bridge_is_connected(stdp_pr_bridge_t bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->initialized;
}

//=============================================================================
// Forward Direction: STDP → Prime Resonant
//=============================================================================

int stdp_pr_notify_ltp(stdp_pr_bridge_t bridge,
                       uint64_t source_id, uint64_t target_id,
                       float weight_change,
                       stdp_pr_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_notify_ltp: bridge is NULL");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pr_notify_ltp: bridge not initialized");
        return -1;
    }
    if (weight_change <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_pr_notify_ltp: weight_change must be positive");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute entanglement increase */
    float entangle_delta = weight_change * bridge->config.ltp_entangle_gain;
    entangle_delta = nimcp_clampf(entangle_delta, 0.0f,
                                 STDP_PR_ENTANGLE_MAX - STDP_PR_ENTANGLE_MIN);

    /* Update state */
    bridge->state.cumulative_entangle_delta += entangle_delta;
    bridge->state.last_ltp_time_ms = get_timestamp_ms();

    /* Update stats */
    bridge->stats.ltp_events++;
    bridge->stats.entangle_increases++;
    bridge->stats.total_entangle_delta += entangle_delta;
    bridge->stats.forward_calls++;

    /* Fill effect if provided */
    if (effect) {
        effect->source_node_id = source_id;
        effect->target_node_id = target_id;
        effect->event_type = STDP_PR_EVENT_LTP;
        effect->weight_change = weight_change;
        effect->entangle_delta = entangle_delta;
        effect->consolidation_boost = 0.0f;
        effect->timestamp_ms = bridge->state.last_ltp_time_ms;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_pr_notify_ltd(stdp_pr_bridge_t bridge,
                       uint64_t source_id, uint64_t target_id,
                       float weight_change,
                       stdp_pr_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_notify_ltd: bridge is NULL");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pr_notify_ltd: bridge not initialized");
        return -1;
    }
    if (weight_change >= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_pr_notify_ltd: weight_change must be negative");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute entanglement decrease */
    float entangle_delta = weight_change * bridge->config.ltd_entangle_decay;
    /* entangle_delta will be negative */

    /* Update state */
    bridge->state.cumulative_entangle_delta += entangle_delta;
    bridge->state.last_ltd_time_ms = get_timestamp_ms();

    /* Update stats */
    bridge->stats.ltd_events++;
    bridge->stats.entangle_decreases++;
    bridge->stats.total_entangle_delta += entangle_delta;
    bridge->stats.forward_calls++;

    /* Fill effect if provided */
    if (effect) {
        effect->source_node_id = source_id;
        effect->target_node_id = target_id;
        effect->event_type = STDP_PR_EVENT_LTD;
        effect->weight_change = weight_change;
        effect->entangle_delta = entangle_delta;
        effect->consolidation_boost = 0.0f;
        effect->timestamp_ms = bridge->state.last_ltd_time_ms;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_pr_notify_burst(stdp_pr_bridge_t bridge,
                         uint64_t source_id, uint64_t target_id,
                         float weight_change, bool is_ltp,
                         stdp_pr_forward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_notify_burst: bridge is NULL");
        return -1;
    }
    if (!bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_pr_notify_burst: bridge not initialized");
        return -1;
    }
    if (fabsf(weight_change) < STDP_PR_BURST_MIN_WEIGHT_CHANGE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_pr_notify_burst: weight_change below minimum");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute entanglement delta */
    float entangle_delta;
    if (is_ltp) {
        entangle_delta = weight_change * bridge->config.ltp_entangle_gain * 2.0f;
    } else {
        entangle_delta = weight_change * bridge->config.ltd_entangle_decay * 2.0f;
    }

    /* Compute consolidation boost */
    float consolidation_boost = 0.0f;
    if (bridge->config.enable_burst_consolidation) {
        consolidation_boost = fabsf(weight_change) * bridge->config.burst_consolidation_gain;
        consolidation_boost = nimcp_clampf(consolidation_boost, 0.0f, 0.1f);
    }

    /* Update state */
    bridge->state.cumulative_entangle_delta += entangle_delta;
    uint64_t now = get_timestamp_ms();
    if (is_ltp) {
        bridge->state.last_ltp_time_ms = now;
    } else {
        bridge->state.last_ltd_time_ms = now;
    }

    /* Update stats */
    bridge->stats.burst_events++;
    if (is_ltp) {
        bridge->stats.ltp_events++;
        bridge->stats.entangle_increases++;
    } else {
        bridge->stats.ltd_events++;
        bridge->stats.entangle_decreases++;
    }
    bridge->stats.total_entangle_delta += entangle_delta;
    bridge->stats.forward_calls++;

    /* Fill effect if provided */
    if (effect) {
        effect->source_node_id = source_id;
        effect->target_node_id = target_id;
        effect->event_type = is_ltp ? STDP_PR_EVENT_BURST_LTP : STDP_PR_EVENT_BURST_LTD;
        effect->weight_change = weight_change;
        effect->entangle_delta = entangle_delta;
        effect->consolidation_boost = consolidation_boost;
        effect->timestamp_ms = now;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_pr_notify_batch(stdp_pr_bridge_t bridge,
                         const stdp_pr_forward_effect_t* events,
                         size_t count) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_notify_batch: bridge is NULL");
        return -1;
    }
    if (!events || count == 0) return 0;

    int applied = 0;
    for (size_t i = 0; i < count; i++) {
        const stdp_pr_forward_effect_t* e = &events[i];
        int ret = 0;

        switch (e->event_type) {
            case STDP_PR_EVENT_LTP:
                ret = stdp_pr_notify_ltp(bridge, e->source_node_id, e->target_node_id,
                                         e->weight_change, NULL);
                break;
            case STDP_PR_EVENT_LTD:
                ret = stdp_pr_notify_ltd(bridge, e->source_node_id, e->target_node_id,
                                         e->weight_change, NULL);
                break;
            case STDP_PR_EVENT_BURST_LTP:
                ret = stdp_pr_notify_burst(bridge, e->source_node_id, e->target_node_id,
                                           e->weight_change, true, NULL);
                break;
            case STDP_PR_EVENT_BURST_LTD:
                ret = stdp_pr_notify_burst(bridge, e->source_node_id, e->target_node_id,
                                           e->weight_change, false, NULL);
                break;
            default:
                ret = -1;
                break;
        }

        if (ret == 0) applied++;
    }
    return applied;
}

//=============================================================================
// Backward Direction: Prime Resonant → STDP
//=============================================================================

int stdp_pr_get_modulation(stdp_pr_bridge_t bridge,
                           uint64_t node_id,
                           stdp_pr_backward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_get_modulation: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_get_modulation: effect is NULL");
        return -1;
    }
    (void)node_id;  /* Would query actual PR memory in full implementation */

    nimcp_mutex_lock(bridge->base.mutex);

    effect->resonance_score = bridge->state.current_resonance;
    effect->consolidation_level = bridge->state.current_consolidation;
    effect->memory_tier = bridge->state.current_tier;

    /* Compute combined modulation */
    float res_mod = lerp(bridge->config.resonance_lr_min,
                         bridge->config.resonance_lr_max,
                         effect->resonance_score);

    float consol_mod = 1.0f;
    if (bridge->config.enable_consolidation_gate) {
        if (effect->consolidation_level > bridge->config.consolidation_gate_high) {
            consol_mod = bridge->config.consolidation_reduction;
        } else if (effect->consolidation_level > bridge->config.consolidation_gate_low) {
            float t = (effect->consolidation_level - bridge->config.consolidation_gate_low) /
                      (bridge->config.consolidation_gate_high - bridge->config.consolidation_gate_low);
            consol_mod = lerp(1.0f, bridge->config.consolidation_reduction, t);
        }
    }

    float tier_mod = 1.0f;
    if (bridge->config.enable_tier_modulation && effect->memory_tier < STDP_PR_TIER_COUNT) {
        tier_mod = bridge->config.tier_rates[effect->memory_tier];
    }

    effect->lr_modulation = res_mod * consol_mod * tier_mod;
    effect->effective_a_plus = effect->lr_modulation;
    effect->effective_a_minus = effect->lr_modulation;
    effect->plasticity_allowed = (consol_mod > 0.1f);

    bridge->stats.backward_calls++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int stdp_pr_apply_resonance_modulation(stdp_pr_bridge_t bridge,
                                       float resonance, float base_lr,
                                       float* modulated_lr) {
    BRIDGE_BBB_VALIDATE(bridge, modulated_lr, sizeof(*modulated_lr));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_apply_resonance_modulation: bridge is NULL");
        return -1;
    }
    if (!modulated_lr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_apply_resonance_modulation: modulated_lr is NULL");
        return -1;
    }

    resonance = nimcp_clampf(resonance, 0.0f, 1.0f);

    float factor = lerp(bridge->config.resonance_lr_min,
                        bridge->config.resonance_lr_max,
                        powf(resonance, bridge->config.resonance_sensitivity));

    *modulated_lr = base_lr * factor;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_resonance = resonance;
    bridge->stats.avg_resonance_modulation =
        0.9f * bridge->stats.avg_resonance_modulation + 0.1f * factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stdp_pr_apply_consolidation_gate(stdp_pr_bridge_t bridge,
                                     float consolidation, float base_lr,
                                     float* gated_lr) {
    BRIDGE_BBB_VALIDATE(bridge, gated_lr, sizeof(*gated_lr));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_apply_consolidation_gate: bridge is NULL");
        return -1;
    }
    if (!gated_lr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_apply_consolidation_gate: gated_lr is NULL");
        return -1;
    }

    consolidation = nimcp_clampf(consolidation, 0.0f, 1.0f);

    float gate = 1.0f;
    if (bridge->config.enable_consolidation_gate) {
        if (consolidation > bridge->config.consolidation_gate_high) {
            gate = bridge->config.consolidation_reduction;
        } else if (consolidation > bridge->config.consolidation_gate_low) {
            float t = (consolidation - bridge->config.consolidation_gate_low) /
                      (bridge->config.consolidation_gate_high - bridge->config.consolidation_gate_low);
            gate = lerp(1.0f, bridge->config.consolidation_reduction, t);
        }
    }

    *gated_lr = base_lr * gate;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_consolidation = consolidation;
    bridge->stats.avg_consolidation_gate =
        0.9f * bridge->stats.avg_consolidation_gate + 0.1f * gate;
    if (gate < 0.3f) {
        bridge->stats.blocked_by_consolidation++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stdp_pr_get_tier_rate(stdp_pr_bridge_t bridge,
                          stdp_pr_memory_tier_t tier,
                          float* rate_multiplier) {
    BRIDGE_BBB_VALIDATE(bridge, rate_multiplier, sizeof(*rate_multiplier));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_get_tier_rate: bridge is NULL");
        return -1;
    }
    if (!rate_multiplier) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_get_tier_rate: rate_multiplier is NULL");
        return -1;
    }
    if (tier >= STDP_PR_TIER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_pr_get_tier_rate: invalid tier");
        return -1;
    }

    *rate_multiplier = bridge->config.tier_rates[tier];

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_tier = tier;
    bridge->stats.avg_tier_modulation =
        0.9f * bridge->stats.avg_tier_modulation + 0.1f * (*rate_multiplier);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stdp_pr_compute_modulation(stdp_pr_bridge_t bridge,
                               float resonance, float consolidation,
                               stdp_pr_memory_tier_t tier,
                               float base_a_plus, float base_a_minus,
                               stdp_pr_backward_effect_t* effect) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_compute_modulation: bridge is NULL");
        return -1;
    }
    if (!effect) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_compute_modulation: effect is NULL");
        return -1;
    }

    resonance = nimcp_clampf(resonance, 0.0f, 1.0f);
    consolidation = nimcp_clampf(consolidation, 0.0f, 1.0f);
    if (tier >= STDP_PR_TIER_COUNT) tier = STDP_PR_TIER_Z0;

    effect->resonance_score = resonance;
    effect->consolidation_level = consolidation;
    effect->memory_tier = tier;

    /* Compute each modulation factor */
    float res_mod = lerp(bridge->config.resonance_lr_min,
                         bridge->config.resonance_lr_max,
                         powf(resonance, bridge->config.resonance_sensitivity));

    float consol_mod = 1.0f;
    if (bridge->config.enable_consolidation_gate) {
        if (consolidation > bridge->config.consolidation_gate_high) {
            consol_mod = bridge->config.consolidation_reduction;
        } else if (consolidation > bridge->config.consolidation_gate_low) {
            float t = (consolidation - bridge->config.consolidation_gate_low) /
                      (bridge->config.consolidation_gate_high - bridge->config.consolidation_gate_low);
            consol_mod = lerp(1.0f, bridge->config.consolidation_reduction, t);
        }
    }

    float tier_mod = bridge->config.enable_tier_modulation ?
                     bridge->config.tier_rates[tier] : 1.0f;

    effect->lr_modulation = res_mod * consol_mod * tier_mod;
    effect->effective_a_plus = base_a_plus * effect->lr_modulation;
    effect->effective_a_minus = base_a_minus * effect->lr_modulation;
    effect->plasticity_allowed = (consol_mod > 0.1f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_resonance = resonance;
    bridge->state.current_consolidation = consolidation;
    bridge->state.current_tier = tier;
    bridge->stats.backward_calls++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// State and Statistics
//=============================================================================

int stdp_pr_bridge_get_state(stdp_pr_bridge_t bridge,
                             stdp_pr_bridge_state_t* state) {
    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_get_state: state is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stdp_pr_bridge_get_stats(stdp_pr_bridge_t bridge,
                             stdp_pr_bridge_stats_t* stats) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stdp_pr_bridge_reset_stats(stdp_pr_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int stdp_pr_bridge_update(stdp_pr_bridge_t bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_update: bridge is NULL");
        return -1;
    }
    (void)dt_ms;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute bridge coherence based on recent activity */
    uint64_t now = get_timestamp_ms();
    uint64_t ltp_age = (bridge->state.last_ltp_time_ms > 0) ?
                       now - bridge->state.last_ltp_time_ms : UINT64_MAX;
    uint64_t ltd_age = (bridge->state.last_ltd_time_ms > 0) ?
                       now - bridge->state.last_ltd_time_ms : UINT64_MAX;

    /* Coherence based on recent bidirectional activity */
    float ltp_factor = (ltp_age < 1000) ? 1.0f : expf(-(float)(ltp_age - 1000) / 5000.0f);
    float ltd_factor = (ltd_age < 1000) ? 1.0f : expf(-(float)(ltd_age - 1000) / 5000.0f);
    float activity_factor = 0.5f * (ltp_factor + ltd_factor);

    /* Coherence also depends on resonance */
    float res_factor = bridge->state.current_resonance;

    bridge->state.bridge_coherence = 0.5f * activity_factor + 0.5f * res_factor;
    bridge->state.bridge_coherence = nimcp_clampf(bridge->state.bridge_coherence, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float stdp_pr_bridge_get_coherence(stdp_pr_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_pr_bridge_get_coherence: bridge is NULL");
        return -1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    float coherence = bridge->state.bridge_coherence;
    nimcp_mutex_unlock(bridge->base.mutex);

    return coherence;
}

void stdp_pr_bridge_print_summary(stdp_pr_bridge_t bridge) {
    if (!bridge) {
        printf("STDP-PR Bridge: NULL\n");
        return;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    printf("=== STDP-PR Bridge Summary ===\n");
    printf("State:\n");
    printf("  Resonance: %.3f\n", bridge->state.current_resonance);
    printf("  Consolidation: %.3f\n", bridge->state.current_consolidation);
    printf("  Tier: Z%d\n", bridge->state.current_tier);
    printf("  Coherence: %.3f\n", bridge->state.bridge_coherence);
    printf("Statistics:\n");
    printf("  LTP events: %lu\n", (unsigned long)bridge->stats.ltp_events);
    printf("  LTD events: %lu\n", (unsigned long)bridge->stats.ltd_events);
    printf("  Burst events: %lu\n", (unsigned long)bridge->stats.burst_events);
    printf("  Entangle delta: %.4f\n", bridge->stats.total_entangle_delta);
    printf("  Blocked by consolidation: %lu\n",
           (unsigned long)bridge->stats.blocked_by_consolidation);

    nimcp_mutex_unlock(bridge->base.mutex);
}

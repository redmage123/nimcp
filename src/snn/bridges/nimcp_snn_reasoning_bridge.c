/**
 * @file nimcp_snn_reasoning_bridge.c
 * @brief SNN-Reasoning integration bridge implementation
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_reasoning_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_reasoning_bridge)

#define BIO_MODULE_SNN_REASONING_BRIDGE 0x0612

void snn_reasoning_config_default(snn_reasoning_config_t* config) {
    if (!config) return;
    config->evidence_rate_min = 10.0f;
    config->evidence_rate_max = 100.0f;
    config->decision_threshold = 0.7f;
    config->integration_time_window_ms = 200.0f;
    config->enable_competition = true;
    config->lateral_inhibition = 0.3f;
    config->reasoning_population_id = 0;
    config->update_interval_ms = 50.0f;
    config->enable_bio_async = false;
}

snn_reasoning_bridge_t* snn_reasoning_bridge_create(
    const snn_reasoning_config_t* config,
    snn_network_t* snn,
    reasoning_system_t reasoning
) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_bridge_create: config is NULL");
        return NULL;
    }
    if (!snn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_bridge_create: snn is NULL");
        return NULL;
    }

    snn_reasoning_bridge_t* bridge = nimcp_malloc(sizeof(snn_reasoning_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_reasoning_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(snn_reasoning_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_reasoning") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->reasoning = reasoning;
    bridge->config = *config;

    if (config->reasoning_population_id > 0) {
        bridge->reasoning_pop = snn_network_get_population(snn, config->reasoning_population_id);
    }

    NIMCP_LOGGING_INFO("Created SNN-reasoning bridge");
    return bridge;
}

void snn_reasoning_bridge_destroy(snn_reasoning_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->base.bio_async_enabled) {
        snn_reasoning_bridge_disconnect_bio_async(bridge);
    }
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-reasoning bridge");
}

int snn_reasoning_bridge_connect_bio_async(snn_reasoning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_REASONING_BRIDGE,
        .module_name = "snn_reasoning_bridge",
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
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_reasoning_bridge_connect_bio_async: validation failed");
    return -1;
}

int snn_reasoning_bridge_disconnect_bio_async(snn_reasoning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_reasoning_bridge_disconnect_bio_async: bridge is NULL");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool snn_reasoning_bridge_is_bio_async_connected(const snn_reasoning_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "snn_reasoning_bridge_is_bio_async_connected: bridge is NULL");
        return false;
    }
    return bridge->base.bio_async_enabled;
}

int snn_reasoning_bridge_process(
    snn_reasoning_bridge_t* bridge,
    const float* input,
    float* output
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_bridge_process: bridge is NULL");
        return -1;
    }

    int ret = snn_reasoning_bridge_update(bridge, bridge->config.update_interval_ms);
    if (ret != 0) return ret;

    if (output) {
        output[0] = bridge->state.evidence_accumulation;
        output[1] = bridge->state.decision_confidence;
    }

    return 0;
}

int snn_reasoning_bridge_update(snn_reasoning_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_bridge_update: bridge is NULL");
        return -1;
    }

    if (bridge->last_update_time > 0 && (dt < bridge->config.update_interval_ms)) {
        return 0;
    }

    if (bridge->reasoning_pop) {
        float spike_rate = snn_network_get_population_rate(
            bridge->snn,
            bridge->config.reasoning_population_id,
            bridge->config.integration_time_window_ms
        );

        bridge->state.evidence_accumulation = snn_reasoning_accumulate_evidence(bridge, spike_rate);
        bridge->state.avg_evidence_rate =
            (bridge->state.avg_evidence_rate * bridge->state.integration_steps + spike_rate) /
            (bridge->state.integration_steps + 1);

        bridge->state.integration_steps++;

        if (snn_reasoning_check_decision_threshold(bridge)) {
            bridge->state.decision_committed = true;
            bridge->state.decision_confidence = bridge->state.evidence_accumulation;
        }
    }

    bridge->last_update_time += dt;
    return 0;
}

float snn_reasoning_accumulate_evidence(
    snn_reasoning_bridge_t* bridge,
    float spike_rate
) {
    if (!bridge) return 0.0f;

    float min_rate = bridge->config.evidence_rate_min;
    float max_rate = bridge->config.evidence_rate_max;
    float normalized = (spike_rate - min_rate) / (max_rate - min_rate);

    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    /* Temporal integration (leaky accumulation) */
    float alpha = 0.3f;
    float accumulated = bridge->state.evidence_accumulation * (1.0f - alpha) + normalized * alpha;
    if (!isfinite(accumulated)) accumulated = normalized;

    return accumulated;
}

int snn_reasoning_compete_populations(
    snn_reasoning_bridge_t* bridge,
    uint32_t* population_ids,
    uint32_t num_populations,
    uint32_t* winner
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_compete_populations: bridge is NULL");
        return -1;
    }
    if (!population_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_compete_populations: population_ids is NULL");
        return -1;
    }
    if (num_populations == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_reasoning_compete_populations: num_populations is 0");
        return -1;
    }

    float max_rate = 0.0f;
    uint32_t max_idx = 0;

    for (uint32_t i = 0; i < num_populations; i++) {
        float rate = snn_network_get_population_rate(
            bridge->snn,
            population_ids[i],
            bridge->config.integration_time_window_ms
        );

        if (rate > max_rate) {
            max_rate = rate;
            max_idx = i;
        }
    }

    if (winner) *winner = population_ids[max_idx];
    bridge->state.winning_population = population_ids[max_idx];

    return 0;
}

bool snn_reasoning_check_decision_threshold(const snn_reasoning_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.evidence_accumulation >= bridge->config.decision_threshold;
}

int snn_reasoning_bridge_get_state(
    const snn_reasoning_bridge_t* bridge,
    snn_reasoning_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_bridge_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_bridge_get_state: state is NULL");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

float snn_reasoning_get_evidence(const snn_reasoning_bridge_t* bridge) {
    return bridge ? bridge->state.evidence_accumulation : 0.0f;
}

float snn_reasoning_get_confidence(const snn_reasoning_bridge_t* bridge) {
    return bridge ? bridge->state.decision_confidence : 0.0f;
}

bool snn_reasoning_is_decision_made(const snn_reasoning_bridge_t* bridge) {
    return bridge ? bridge->state.decision_committed : false;
}

int snn_reasoning_get_stats(
    const snn_reasoning_bridge_t* bridge,
    uint32_t* integration_steps,
    uint32_t* decisions_made,
    float* avg_confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_reasoning_get_stats: bridge is NULL");
        return -1;
    }
    if (integration_steps) *integration_steps = bridge->state.integration_steps;
    if (decisions_made) *decisions_made = bridge->state.decision_committed ? 1 : 0;
    if (avg_confidence) *avg_confidence = bridge->state.decision_confidence;
    return 0;
}

void snn_reasoning_reset_stats(snn_reasoning_bridge_t* bridge) {
    if (!bridge) return;
    bridge->state.integration_steps = 0;
    bridge->state.evidence_accumulation = 0.0f;
    bridge->state.decision_committed = false;
    bridge->state.avg_evidence_rate = 0.0f;
}

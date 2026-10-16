/**
 * @file nimcp_stdp_quantum_bridge.h
 * @brief Quantum-inspired STDP learning rate optimization
 *
 * WHAT: Integrates quantum annealing optimizer with STDP plasticity
 * WHY:  Escape local minima in weight space, find better solutions
 * HOW:  Simulated quantum tunneling for learning rate adaptation
 *
 * BIOLOGICAL INSPIRATION:
 * - Metaplasticity: learning to learn
 * - Synaptic scaling homeostasis
 * - Sleep-dependent memory consolidation
 */

#ifndef NIMCP_STDP_QUANTUM_BRIDGE_H
#define NIMCP_STDP_QUANTUM_BRIDGE_H

#include "plasticity/stdp/nimcp_quantum_stdp_optimizer.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct stdp_quantum_bridge stdp_quantum_bridge_t;

typedef struct {
    bool enabled;
    qstdp_objective_t objective;
    qstdp_schedule_t schedule;
    float initial_temp;
    float final_temp;
    float tunnel_probability;
} stdp_quantum_config_t;

/**
 * WHAT: Parameters from quantum optimizer
 */
typedef struct {
    float learning_rate;
    float a_plus;
    float a_minus;
    float tau_plus;
    float tau_minus;
} stdp_quantum_params_t;

typedef struct {
    uint64_t optimization_steps;
    uint64_t tunneling_events;
    float avg_learning_rate;
    float best_objective;
    float current_temperature;
} stdp_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

stdp_quantum_config_t stdp_quantum_default_config(void);

stdp_quantum_bridge_t* stdp_quantum_bridge_create(
    const stdp_quantum_config_t* config
);

void stdp_quantum_bridge_destroy(stdp_quantum_bridge_t* bridge);

bool stdp_quantum_bridge_is_enabled(const stdp_quantum_bridge_t* bridge);

void stdp_quantum_bridge_set_enabled(stdp_quantum_bridge_t* bridge, bool enabled);

/**
 * WHAT: Get quantum-optimized learning rate for current step
 */
float stdp_quantum_get_lr(stdp_quantum_bridge_t* bridge);

/**
 * WHAT: Perform optimization step with activity stats
 */
float stdp_quantum_step(
    stdp_quantum_bridge_t* bridge,
    const qstdp_activity_stats_t* stats
);

/**
 * WHAT: Get current optimization parameters
 */
int stdp_quantum_get_params(
    stdp_quantum_bridge_t* bridge,
    stdp_quantum_params_t* params_out
);

/**
 * WHAT: Reset optimizer state
 */
void stdp_quantum_reset(stdp_quantum_bridge_t* bridge);

int stdp_quantum_get_stats(
    const stdp_quantum_bridge_t* bridge,
    stdp_quantum_stats_t* stats
);

void stdp_quantum_reset_stats(stdp_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_STDP_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

struct stdp_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    stdp_quantum_config_t config;
    qstdp_optimizer_t optimizer;  /* Direct handle, not pointer-to-pointer */
    stdp_quantum_stats_t stats;
    float last_lr;
};

stdp_quantum_config_t stdp_quantum_default_config(void) {
    return (stdp_quantum_config_t){
        .enabled = true,
        .objective = QSTDP_OBJ_STABILITY,
        .schedule = QSTDP_SCHEDULE_EXPONENTIAL,
        .initial_temp = 1.0f,
        .final_temp = 0.01f,
        .tunnel_probability = 0.3f
    };
}

stdp_quantum_bridge_t* stdp_quantum_bridge_create(
    const stdp_quantum_config_t* config
) {
    stdp_quantum_bridge_t* bridge = (stdp_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_quantum_bridge_create: failed to allocate bridge");
        return NULL;
    }

    bridge->config = config ? *config : stdp_quantum_default_config();

    qstdp_optimizer_config_t qconfig = qstdp_optimizer_default_config();
    qconfig.objective = bridge->config.objective;
    qconfig.schedule = bridge->config.schedule;
    qconfig.initial_temperature = bridge->config.initial_temp;
    qconfig.final_temperature = bridge->config.final_temp;
    qconfig.tunneling_rate = bridge->config.tunnel_probability;

    bridge->optimizer = qstdp_optimizer_create(&qconfig);
    if (!bridge->optimizer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_quantum_bridge_create: failed to create quantum optimizer");
        nimcp_free(bridge);
        return NULL;
    }

    bridge->last_lr = 0.01f;  /* Default learning rate */
    return bridge;
}

void stdp_quantum_bridge_destroy(stdp_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_bridge_destroy: bridge is NULL");
        return;
    }
    if (bridge->optimizer) qstdp_optimizer_destroy(bridge->optimizer);
    nimcp_free(bridge);
}

bool stdp_quantum_bridge_is_enabled(const stdp_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_bridge_is_enabled: bridge is NULL");
        return false;
    }
    return bridge->config.enabled;
}

void stdp_quantum_bridge_set_enabled(stdp_quantum_bridge_t* bridge, bool enabled) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_bridge_set_enabled: bridge is NULL");
        return;
    }
    bridge->config.enabled = enabled;
}

float stdp_quantum_get_lr(stdp_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_get_lr: bridge is NULL");
        return 0.01f;
    }
    if (!bridge->optimizer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_quantum_get_lr: optimizer is NULL");
        return 0.01f;
    }

    float lr = 0.01f;
    qstdp_optimizer_get_params(bridge->optimizer, &lr, NULL, NULL, NULL, NULL);
    return lr;
}

float stdp_quantum_step(
    stdp_quantum_bridge_t* bridge,
    const qstdp_activity_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_step: bridge is NULL");
        return 0.01f;
    }
    if (!bridge->optimizer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_quantum_step: optimizer is NULL");
        return 0.01f;
    }

    float new_lr = qstdp_optimizer_step(bridge->optimizer, stats);

    bridge->stats.optimization_steps++;
    bridge->stats.avg_learning_rate =
        (bridge->stats.avg_learning_rate * (bridge->stats.optimization_steps - 1)
         + new_lr) / bridge->stats.optimization_steps;

    bridge->last_lr = new_lr;
    return new_lr;
}

int stdp_quantum_get_params(
    stdp_quantum_bridge_t* bridge,
    stdp_quantum_params_t* params_out
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_get_params: bridge is NULL");
        return -1;
    }
    if (!bridge->optimizer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "stdp_quantum_get_params: optimizer is NULL");
        return -1;
    }
    if (!params_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_get_params: params_out is NULL");
        return -1;
    }

    return qstdp_optimizer_get_params(bridge->optimizer,
                                       &params_out->learning_rate,
                                       &params_out->a_plus,
                                       &params_out->a_minus,
                                       &params_out->tau_plus,
                                       &params_out->tau_minus);
}

void stdp_quantum_reset(stdp_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_reset: bridge is NULL");
        return;
    }
    if (bridge->optimizer) {
        qstdp_optimizer_reset(bridge->optimizer);
    }
}

int stdp_quantum_get_stats(
    const stdp_quantum_bridge_t* bridge,
    stdp_quantum_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_get_stats: stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void stdp_quantum_reset_stats(stdp_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_quantum_reset_stats: bridge is NULL");
        return;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

#endif // NIMCP_STDP_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_STDP_QUANTUM_BRIDGE_H

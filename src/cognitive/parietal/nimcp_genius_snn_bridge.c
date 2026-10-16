/**
 * @file nimcp_genius_snn_bridge.c
 * @brief Mathematical Genius - SNN Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-24
 */

#include "cognitive/parietal/nimcp_genius_snn_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_kg_hierarchy.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_neural_constants.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(genius_snn_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from genius_snn_bridge module (instance-level) */
static inline void genius_snn_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_genius_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_genius_snn_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_genius_snn_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "GENIUS_SNN_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct genius_snn_bridge {
    bridge_base_t base;
    genius_snn_config_t config;
    struct snn_network* snn;
    struct mathematical_genius* genius;

    /* State */
    genius_snn_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Heartbeat tracking (Phase 8) */
    uint64_t last_heartbeat_us;

    /* Dimension state */
    genius_dim_state_t dim_states[GENIUS_SNN_MAX_DIMENSIONS];

    /* Buffers */
    float* encoding_buffer;
    float* output_buffer;
    float* mode_buffer;

    /* Insight output state */
    genius_insight_output_t last_insight;
    genius_mode_t current_mode;
    float insight_accumulator;

    /* Previous state for change detection */
    float* prev_state;

    /* Callbacks */
    genius_snn_insight_callback_t insight_callback;
    void* insight_callback_data;
    genius_snn_breakthrough_callback_t breakthrough_callback;
    void* breakthrough_callback_data;
    genius_snn_mode_callback_t mode_callback;
    void* mode_callback_data;

    /* Statistics */
    genius_snn_stats_t stats;

    /* KG Wiring */
    struct kg_module_wiring* kg_wiring;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(genius_snn_bridge)

//=============================================================================
// Helper Functions
//=============================================================================

static void softmax(float* values, uint32_t n) {
    if (n == 0) return;

    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(i + 1) / (float)n);
        }

        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 0.0f) {
        for (uint32_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                genius_snn_bridge_heartbeat("genius_snn_b_loop",
                                 (float)(i + 1) / (float)n);
            }

            values[i] /= sum;
        }
    }
}

static genius_mode_t find_dominant_mode(const genius_insight_output_t* insight) {
    float max_activity = insight->gauss_activity;
    genius_mode_t mode = GENIUS_MODE_GAUSS;

    if (insight->newton_activity > max_activity) {
        max_activity = insight->newton_activity;
        mode = GENIUS_MODE_NEWTON;
    }
    if (insight->erdos_activity > max_activity) {
        mode = GENIUS_MODE_ERDOS;
    }
    return mode;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

genius_snn_config_t genius_snn_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_config_de", 0.0f);


    genius_snn_config_t config = {
        .num_dimensions = GENIUS_DIM_COUNT,
        .neurons_per_dim = GENIUS_SNN_NEURONS_PER_CONCEPT,
        .hidden_dim = NIMCP_DEFAULT_HIDDEN_SIZE,

        .dt_ms = 1.0f,
        .encoding_window_ms = GENIUS_SNN_ENCODING_WINDOW,
        .integration_tau_ms = 100.0f,
        .insight_tau_ms = 200.0f,

        .encoding = GENIUS_SNN_ENCODE_POPULATION,
        .encoding_gain = 1.0f,
        .baseline_rate_hz = 5.0f,
        .max_rate_hz = 100.0f,

        .decoding = GENIUS_SNN_DECODE_INTEGRATION,
        .insight_threshold = GENIUS_SNN_INSIGHT_THRESH,
        .elegance_threshold = 0.6f,
        .mode_switch_threshold = 0.3f,

        .enable_competition = true,
        .inhibition_strength = 0.2f,
        .enable_insight_detection = true,
        .insight_sensitivity = NIMCP_SENSITIVITY_DEFAULT,

        .enable_gauss_circuits = true,
        .enable_newton_circuits = true,
        .enable_erdos_circuits = true,
        .mode_coupling_strength = NIMCP_OSCILLATION_COUPLING_DEFAULT,

        .enable_bio_async = false,
        .enable_plasticity_integration = true
    };
    return config;
}

genius_snn_bridge_t* genius_snn_create(const genius_snn_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_create", 0.0f);


    genius_snn_bridge_t* bridge = nimcp_calloc(1, sizeof(genius_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge allocation failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = genius_snn_config_default();
    }

    /* Validate config */
    if (bridge->config.num_dimensions == 0 ||
        bridge->config.num_dimensions > GENIUS_SNN_MAX_DIMENSIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "invalid num_dimensions: %u (max: %u)",
            bridge->config.num_dimensions, GENIUS_SNN_MAX_DIMENSIONS);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "genius_snn") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "bridge_base_init failed for genius_snn");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate buffers */
    uint32_t input_dim = bridge->config.num_dimensions * bridge->config.neurons_per_dim;
    uint32_t output_dim = 8; /* insight, elegance, pattern, conjecture, gauss, newton, erdos, creativity */

    bridge->encoding_buffer = nimcp_calloc(input_dim, sizeof(float));
    bridge->output_buffer = nimcp_calloc(output_dim, sizeof(float));
    bridge->mode_buffer = nimcp_calloc(GENIUS_MODE_COUNT, sizeof(float));
    bridge->prev_state = nimcp_calloc(bridge->config.num_dimensions, sizeof(float));

    if (!bridge->encoding_buffer || !bridge->output_buffer ||
        !bridge->mode_buffer || !bridge->prev_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "buffer allocation failed for genius_snn_bridge");
        genius_snn_destroy(bridge);
        return NULL;
    }

    /* Initialize dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].last_spike_time_us = 0;
        bridge->dim_states[i].insight_contribution = 0.0f;
    }

    /* Initialize state */
    bridge->state = GENIUS_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->current_mode = GENIUS_MODE_ADAPTIVE;
    bridge->insight_accumulator = 0.0f;

    memset(&bridge->last_insight, 0, sizeof(genius_insight_output_t));
    memset(&bridge->stats, 0, sizeof(genius_snn_stats_t));

    /* Initialize KG wiring */
    bridge->kg_wiring = genius_snn_create_kg_wiring();

    NIMCP_LOGGING_INFO("Created %s bridge", "genius_snn");
    return bridge;
}

void genius_snn_destroy(genius_snn_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "genius_snn");

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_destroy", 0.0f);


    if (bridge->encoding_buffer) nimcp_free(bridge->encoding_buffer);
    if (bridge->output_buffer) nimcp_free(bridge->output_buffer);
    if (bridge->mode_buffer) nimcp_free(bridge->mode_buffer);
    if (bridge->prev_state) nimcp_free(bridge->prev_state);

    /* Destroy KG wiring */
    if (bridge->kg_wiring) {
        kg_module_wiring_destroy(bridge->kg_wiring);
        bridge->kg_wiring = NULL;
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int genius_snn_reset(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset dimension states */
    for (uint32_t i = 0; i < bridge->config.num_dimensions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(i + 1) / (float)bridge->config.num_dimensions);
        }

        bridge->dim_states[i].activation = 0.0f;
        bridge->dim_states[i].accumulated_evidence = 0.0f;
        bridge->dim_states[i].spike_count = 0;
        bridge->dim_states[i].mean_rate_hz = bridge->config.baseline_rate_hz;
        bridge->dim_states[i].insight_contribution = 0.0f;
    }

    bridge->state = GENIUS_SNN_STATE_IDLE;
    bridge->current_time_us = 0;
    bridge->insight_accumulator = 0.0f;

    memset(&bridge->last_insight, 0, sizeof(genius_insight_output_t));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_link_genius(genius_snn_bridge_t* bridge, struct mathematical_genius* genius) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_link_genius: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_link_geni", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->genius = genius;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_link_snn(genius_snn_bridge_t* bridge, struct snn_network* snn) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_link_snn: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_link_snn", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->snn = snn;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

int genius_snn_encode_state(genius_snn_bridge_t* bridge, const float* dimensions, uint32_t num_dims) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_snn_encode_state: bridge is NULL");
        return -1;
    }
    if (!dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_snn_encode_state: dimensions is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_encode_st", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, dimensions, sizeof(*dimensions));

    if (num_dims > bridge->config.num_dimensions) {
        num_dims = bridge->config.num_dimensions;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_SNN_STATE_ENCODING;

    int total_spikes = 0;
    uint32_t neurons_per_dim = bridge->config.neurons_per_dim;

    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        float value = nimcp_myelin_clamp(dimensions[d], 0.0f, 1.0f);
        bridge->dim_states[d].activation = value;

        /* Population coding */
        float rate = bridge->config.baseline_rate_hz +
                    value * (bridge->config.max_rate_hz - bridge->config.baseline_rate_hz);

        /* Encode into buffer */
        for (uint32_t n = 0; n < neurons_per_dim; n++) {
            /* Phase 8: Loop progress heartbeat */
            if ((n & 0xFF) == 0 && neurons_per_dim > 256) {
                genius_snn_bridge_heartbeat("genius_snn_b_loop",
                                 (float)(n + 1) / (float)neurons_per_dim);
            }

            float neuron_pref = (neurons_per_dim > 1) ? (float)n / (float)(neurons_per_dim - 1) : 0.0f;
            float tuning = expf(-4.0f * (value - neuron_pref) * (value - neuron_pref));
            bridge->encoding_buffer[d * neurons_per_dim + n] = tuning * rate * bridge->config.encoding_gain;

            if (tuning > 0.5f) total_spikes++;
        }

        bridge->dim_states[d].spike_count += (uint32_t)(rate * bridge->config.dt_ms / 1000.0f);
        bridge->dim_states[d].mean_rate_hz = 0.9f * bridge->dim_states[d].mean_rate_hz + 0.1f * rate;
    }

    bridge->stats.total_spikes += total_spikes;
    bridge->stats.total_evaluations++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return total_spikes;
}

int genius_snn_encode_pattern(genius_snn_bridge_t* bridge, float pattern_strength, uint32_t pattern_type) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_encode_pattern: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_encode_pa", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->dim_states[GENIUS_DIM_PATTERN_RECOGNITION].activation = nimcp_myelin_clamp(pattern_strength, 0.0f, 1.0f);
    bridge->dim_states[GENIUS_DIM_PATTERN_RECOGNITION].accumulated_evidence += pattern_strength * 0.1f;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_encode_proof_state(genius_snn_bridge_t* bridge, float progress, float elegance, uint32_t depth) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_encode_proof_state: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_encode_pr", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->dim_states[GENIUS_DIM_PROOF_SEARCH].activation = nimcp_myelin_clamp(progress, 0.0f, 1.0f);
    bridge->dim_states[GENIUS_DIM_ELEGANCE_SIGNAL].activation = nimcp_myelin_clamp(elegance, 0.0f, 1.0f);
    bridge->dim_states[GENIUS_DIM_RIGOR_LEVEL].activation = nimcp_myelin_clamp(1.0f - (float)depth / 100.0f, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_encode_mode(genius_snn_bridge_t* bridge, genius_mode_t mode, float activation) {
    if (!bridge || mode >= GENIUS_MODE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "genius_snn_encode_mode: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_encode_mo", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    activation = nimcp_myelin_clamp(activation, 0.0f, 1.0f);

    switch (mode) {
        case GENIUS_MODE_GAUSS:
            bridge->dim_states[GENIUS_DIM_GAUSS_ACTIVITY].activation = activation;
            break;
        case GENIUS_MODE_NEWTON:
            bridge->dim_states[GENIUS_DIM_NEWTON_ACTIVITY].activation = activation;
            break;
        case GENIUS_MODE_ERDOS:
            bridge->dim_states[GENIUS_DIM_ERDOS_ACTIVITY].activation = activation;
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Simulation Functions
//=============================================================================

int genius_snn_simulate(genius_snn_bridge_t* bridge, float duration_ms) {
    if (!bridge || duration_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "genius_snn_simulate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_simulate", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_SNN_STATE_PROCESSING;

    float dt = bridge->config.dt_ms;
    int steps = (int)(duration_ms / (fabsf(dt) > 1e-7f ? dt : 1e-7f));

    for (int s = 0; s < steps; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && steps > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(s + 1) / (float)steps);
        }

        /* Update insight accumulator */
        float insight_input = 0.0f;
        for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
                genius_snn_bridge_heartbeat("genius_snn_b_loop",
                                 (float)(d + 1) / (float)bridge->config.num_dimensions);
            }

            insight_input += bridge->dim_states[d].activation * bridge->dim_states[d].insight_contribution;
        }

        float tau = bridge->config.insight_tau_ms;
        bridge->insight_accumulator += dt * (insight_input - bridge->insight_accumulator) / tau;

        /* Update time */
        bridge->current_time_us += (uint64_t)(dt * 1000.0f);
        bridge->stats.total_simulations++;
    }

    /* Check for insight emergence */
    if (bridge->config.enable_insight_detection &&
        bridge->insight_accumulator > bridge->config.insight_threshold) {
        bridge->state = GENIUS_SNN_STATE_INSIGHT_EMERGING;
        bridge->stats.insights_detected++;

        if (bridge->insight_callback) {
            bridge->insight_callback(bridge, bridge->insight_accumulator,
                                    bridge->current_mode, bridge->insight_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_step(genius_snn_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_step", 0.0f);


    return genius_snn_simulate(bridge, bridge->config.dt_ms);
}

int genius_snn_forward(genius_snn_bridge_t* bridge, const float* inputs, uint32_t input_count) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_forward: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_forward", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "genius_snn_step");
    BRIDGE_LGSS_GATE(bridge, "genius_snn_step");
    BRIDGE_BBB_VALIDATE(bridge, inputs, sizeof(*inputs));

    return genius_snn_encode_state(bridge, inputs, input_count);
}

//=============================================================================
// Decoding Functions
//=============================================================================

int genius_snn_get_insight_output(genius_snn_bridge_t* bridge, genius_insight_output_t* insight) {
    if (!bridge || !insight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_get_insight_output: required parameter is NULL (bridge, insight)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_get_insig", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, insight, sizeof(*insight));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_SNN_STATE_DECODING;

    insight->insight_strength = bridge->insight_accumulator;
    insight->elegance_signal = bridge->dim_states[GENIUS_DIM_ELEGANCE_SIGNAL].activation;
    insight->pattern_confidence = bridge->dim_states[GENIUS_DIM_PATTERN_RECOGNITION].activation;
    insight->conjecture_strength = bridge->dim_states[GENIUS_DIM_CONJECTURE_CONFIDENCE].activation;

    insight->gauss_activity = bridge->dim_states[GENIUS_DIM_GAUSS_ACTIVITY].activation;
    insight->newton_activity = bridge->dim_states[GENIUS_DIM_NEWTON_ACTIVITY].activation;
    insight->erdos_activity = bridge->dim_states[GENIUS_DIM_ERDOS_ACTIVITY].activation;

    insight->active_mode = find_dominant_mode(insight);
    insight->insight_detected = bridge->insight_accumulator > bridge->config.insight_threshold;
    insight->breakthrough_imminent = bridge->insight_accumulator > 0.9f;

    insight->creativity_level = bridge->dim_states[GENIUS_DIM_CREATIVITY_LEVEL].activation;
    insight->rigor_level = bridge->dim_states[GENIUS_DIM_RIGOR_LEVEL].activation;

    bridge->last_insight = *insight;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_get_activations(genius_snn_bridge_t* bridge, float* activations, uint32_t num_dims) {
    if (!bridge || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_get_activations: required parameter is NULL (bridge, activations)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_get_activ", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, activations, sizeof(*activations));

    nimcp_mutex_lock(bridge->base.mutex);

    if (num_dims > bridge->config.num_dimensions) {
        num_dims = bridge->config.num_dimensions;
    }

    for (uint32_t d = 0; d < num_dims; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && num_dims > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(d + 1) / (float)num_dims);
        }

        activations[d] = bridge->dim_states[d].activation;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool genius_snn_check_insight(genius_snn_bridge_t* bridge, float* insight_level) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_check_ins", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, insight_level, sizeof(*insight_level));

    nimcp_mutex_lock(bridge->base.mutex);

    bool detected = bridge->insight_accumulator > bridge->config.insight_threshold;
    if (insight_level) {
        *insight_level = bridge->insight_accumulator;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return detected;
}

genius_mode_t genius_snn_recommend_mode(genius_snn_bridge_t* bridge, float* confidence) {
    if (!bridge) return GENIUS_MODE_ADAPTIVE;

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_recommend", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float gauss = bridge->dim_states[GENIUS_DIM_GAUSS_ACTIVITY].activation;
    float newton = bridge->dim_states[GENIUS_DIM_NEWTON_ACTIVITY].activation;
    float erdos = bridge->dim_states[GENIUS_DIM_ERDOS_ACTIVITY].activation;

    genius_mode_t mode = GENIUS_MODE_GAUSS;
    float max_act = gauss;

    if (newton > max_act) {
        max_act = newton;
        mode = GENIUS_MODE_NEWTON;
    }
    if (erdos > max_act) {
        max_act = erdos;
        mode = GENIUS_MODE_ERDOS;
    }

    if (confidence) {
        float total = gauss + newton + erdos;
        *confidence = (total > 0.0f) ? max_act / total : 0.33f;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return mode;
}

//=============================================================================
// State Query Functions
//=============================================================================

int genius_snn_get_dim_state(genius_snn_bridge_t* bridge, uint32_t dim, genius_dim_state_t* state) {
    if (!bridge || !state || dim >= bridge->config.num_dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_get_dim_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_get_dim_s", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->dim_states[dim];
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_get_state(genius_snn_bridge_t* bridge, genius_snn_bridge_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_get_state", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_dimensions = 0;
    state->total_activity = 0.0f;
    state->mean_insight = bridge->insight_accumulator;

    for (uint32_t d = 0; d < bridge->config.num_dimensions; d++) {
        /* Phase 8: Loop progress heartbeat */
        if ((d & 0xFF) == 0 && bridge->config.num_dimensions > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(d + 1) / (float)bridge->config.num_dimensions);
        }

        if (bridge->dim_states[d].activation > 0.1f) {
            state->active_dimensions++;
        }
        state->total_activity += bridge->dim_states[d].activation;
    }

    float gauss = bridge->dim_states[GENIUS_DIM_GAUSS_ACTIVITY].activation;
    float newton = bridge->dim_states[GENIUS_DIM_NEWTON_ACTIVITY].activation;
    float erdos = bridge->dim_states[GENIUS_DIM_ERDOS_ACTIVITY].activation;
    float total = gauss + newton + erdos;
    state->mode_coherence = (total > 0.0f) ? fmaxf(gauss, fmaxf(newton, erdos)) / total : 0.0f;

    genius_insight_output_t insight = {0};
    insight.gauss_activity = gauss;
    insight.newton_activity = newton;
    insight.erdos_activity = erdos;
    state->dominant_mode = find_dominant_mode(&insight);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_get_stats(genius_snn_bridge_t* bridge, genius_snn_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_get_stats", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_reset_stats(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_reset_sta", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(genius_snn_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int genius_snn_register_insight_callback(genius_snn_bridge_t* bridge,
                                         genius_snn_insight_callback_t callback,
                                         void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_register_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->insight_callback = callback;
    bridge->insight_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_register_breakthrough_callback(genius_snn_bridge_t* bridge,
                                              genius_snn_breakthrough_callback_t callback,
                                              void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_register_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->breakthrough_callback = callback;
    bridge->breakthrough_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_register_mode_callback(genius_snn_bridge_t* bridge,
                                      genius_snn_mode_callback_t callback,
                                      void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_register_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->mode_callback = callback;
    bridge->mode_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int genius_snn_bio_async_connect(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_bio_async_connect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_bio_async", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_bio_async_disconnect(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_bio_async", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool genius_snn_is_bio_async_connected(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_is_bio_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);
    return connected;
}

//=============================================================================
// Heartbeat and State Serialization (Phase 8)
//=============================================================================

int genius_snn_send_heartbeat(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_snn_send_heartbeat: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->last_heartbeat_us = nimcp_time_get_us();
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

uint64_t genius_snn_get_last_heartbeat(const genius_snn_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Cast away const for the mutex lock — reading last_heartbeat_us is
     * logically a read-only operation, but the mutex lock/unlock requires
     * non-const access. This is a standard C idiom for "logically const"
     * getters that use mutable internal synchronization. */
    genius_snn_bridge_t* mut = (genius_snn_bridge_t*)bridge;
    nimcp_mutex_lock(mut->base.mutex);
    uint64_t last_hb = mut->last_heartbeat_us;
    nimcp_mutex_unlock(mut->base.mutex);
    return last_hb;
}

bool genius_snn_is_heartbeat_stale(const genius_snn_bridge_t* bridge,
                                    uint32_t timeout_ms) {
    if (!bridge) return true;

    uint64_t last_hb = genius_snn_get_last_heartbeat(bridge);
    if (last_hb == 0) return true;

    uint64_t now_us = nimcp_time_get_us();
    uint64_t elapsed_us = now_us - last_hb;
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

    return elapsed_us > timeout_us;
}

int genius_snn_serialize_state(genius_snn_bridge_t* bridge,
                                genius_snn_serialized_t* serialized) {
    if (!bridge || !serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_snn_serialize_state: bridge or serialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_serialize", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, serialized, sizeof(*serialized));

    nimcp_mutex_lock(bridge->base.mutex);

    memset(serialized, 0, sizeof(*serialized));
    serialized->version = 1;
    serialized->num_dimensions = bridge->config.num_dimensions;
    serialized->timestamp_us = nimcp_time_get_us();

    /* Capture bridge state */
    genius_snn_get_state(bridge, &serialized->state);

    /* Copy statistics */
    memcpy(&serialized->stats, &bridge->stats, sizeof(genius_snn_stats_t));

    /* Compute checksum */
    serialized->checksum = genius_snn_compute_checksum(serialized);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_snn_deserialize_state(genius_snn_bridge_t* bridge,
                                  const genius_snn_serialized_t* serialized) {
    if (!bridge || !serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_snn_deserialize_state: bridge or serialized is NULL");
        return -1;
    }

    if (!genius_snn_verify_checksum(serialized)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_snn_deserialize_state: checksum verification failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_deseriali", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, serialized, sizeof(*serialized));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Restore state */
    bridge->state = serialized->state.state;

    /* Restore statistics */
    memcpy(&bridge->stats, &serialized->stats, sizeof(genius_snn_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

uint32_t genius_snn_compute_checksum(const genius_snn_serialized_t* serialized) {
    if (!serialized) return 0;

    /* FNV-1a hash over relevant fields */
    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_compute_c", 0.0f);


    uint32_t hash = 2166136261u;
    const uint8_t* data = (const uint8_t*)serialized;
    size_t len = offsetof(genius_snn_serialized_t, checksum);

    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            genius_snn_bridge_heartbeat("genius_snn_b_loop",
                             (float)(i + 1) / (float)len);
        }

        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

bool genius_snn_verify_checksum(const genius_snn_serialized_t* serialized) {
    if (!serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_verify_checksum: serialized is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_verify_ch", 0.0f);


    uint32_t computed = genius_snn_compute_checksum(serialized);
    return computed == serialized->checksum;
}

//=============================================================================
// KG Wiring Integration
//=============================================================================

kg_module_wiring_t* genius_snn_create_kg_wiring(void) {
    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_create_kg", 0.0f);


    kg_module_wiring_t* wiring = kg_module_wiring_create(
        KG_GENIUS_SNN_MODULE_NAME,
        KG_GENIUS_SNN_MODULE_TYPE
    );
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wiring is NULL");
        return NULL;
    }

    /* Set hierarchical placement - parietal cortex layer IV */
    wiring->target_layer = KG_LAYER_IV;
    wiring->hemisphere_affinity = KG_HEMISPHERE_BILATERAL;

    /* Set metadata */
    kg_module_wiring_set_metadata(wiring,
        "NIMCP Team",
        "MATHEMATICAL_REASONING",
        "Mathematical genius SNN encoding and decoding bridge"
    );
    kg_module_wiring_set_version(wiring, 1, 0, 0);

    /* Register inputs from mathematical genius module */
    kg_module_wiring_add_input(wiring, "mathematical_genius", KG_MSG_GENIUS_MATH_STATE, true);
    kg_module_wiring_add_input(wiring, "mathematical_genius", KG_MSG_GENIUS_MODE_ACTIVATION, false);
    kg_module_wiring_add_input(wiring, "pattern_detector", KG_MSG_GENIUS_PATTERN_INPUT, false);
    kg_module_wiring_add_input(wiring, "proof_system", KG_MSG_GENIUS_PROOF_STEP, false);

    /* Register outputs to SNN and observation systems */
    kg_module_wiring_add_output(wiring, KG_MSG_GENIUS_INSIGHT_DETECTED, "Mathematical insight detected from spike patterns");
    kg_module_wiring_add_output(wiring, KG_MSG_GENIUS_MODE_RECOMMEND, "Recommended genius mode based on SNN activity");
    kg_module_wiring_add_output(wiring, KG_MSG_GENIUS_SPIKE_PATTERN, "Encoded spike pattern representation");
    kg_module_wiring_add_output(wiring, KG_MSG_GENIUS_BREAKTHROUGH, "Mathematical breakthrough event");

    /* Register message handlers */
    kg_module_wiring_add_handler(wiring, KG_MSG_GENIUS_ENCODE_REQUEST, 100);
    kg_module_wiring_add_handler(wiring, KG_MSG_GENIUS_SIMULATE_REQUEST, 150);
    kg_module_wiring_add_handler(wiring, KG_MSG_GENIUS_DECODE_REQUEST, 100);

    /* Set network type to SNN */
    wiring->network_type = KG_WEIGHT_SNN;

    /* Add custom metadata */
    kg_module_wiring_add_metadata_entry(wiring, "brain_region", "parietal_intraparietal_sulcus");
    kg_module_wiring_add_metadata_entry(wiring, "encoding_type", "population_coding");
    kg_module_wiring_add_metadata_entry(wiring, "genius_modes", "gauss,newton,erdos");

    return wiring;
}

kg_module_wiring_t* genius_snn_get_kg_wiring(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_snn_get_kg_wiring: bridge is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_snn_bridge_heartbeat("genius_snn_b_genius_snn_get_kg_wi", 0.0f);


    return bridge->kg_wiring;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void genius_snn_bridge_set_instance_health_agent(
    genius_snn_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int genius_snn_bridge_training_begin(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_snn_bridge_training_begin: NULL argument");
        return -1;
    }
    genius_snn_bridge_heartbeat_instance(bridge->health_agent, "genius_snn_bridge_training_begin", 0.0f);
    return 0;
}

int genius_snn_bridge_training_end(genius_snn_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_snn_bridge_training_end: NULL argument");
        return -1;
    }
    genius_snn_bridge_heartbeat_instance(bridge->health_agent, "genius_snn_bridge_training_end", 1.0f);
    return 0;
}

int genius_snn_bridge_training_step(genius_snn_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_snn_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "genius_snn_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "genius_snn_bridge_training_step");
    genius_snn_bridge_heartbeat_instance(bridge->health_agent, "genius_snn_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

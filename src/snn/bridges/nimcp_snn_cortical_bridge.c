/**
 * @file nimcp_snn_cortical_bridge.c
 * @brief Implementation of SNN-cortical column bridge
 */

#include "snn/bridges/nimcp_snn_cortical_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "async/nimcp_bio_async.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_cortical_bridge)

/* Bio-async module ID for cortical bridge */
#define BIO_MODULE_SNN_CORTICAL 0x0607

//=============================================================================
// Default Configuration
//=============================================================================

void snn_cortical_config_default(snn_cortical_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_config_default: null config pointer");
        return;
    }

    /* Layer-specific rates from cortical neuroscience */
    config->layer_2_3_base_rate = 15.0f;    /* L2/3: moderate rate */
    config->layer_4_base_rate = 25.0f;      /* L4: high input rate */
    config->layer_5_base_rate = 20.0f;      /* L5: strong output */
    config->layer_6_base_rate = 10.0f;      /* L6: slow feedback */

    /* Burst parameters */
    config->burst_threshold = 0.7f;
    config->burst_spike_count = 3;
    config->burst_isi = 5.0f;               /* 5ms ISI */

    /* Population coding */
    config->neurons_per_minicolumn = 80;    /* Typical minicolumn size */
    config->tuning_curve_width = 0.2f;
    config->population_code_threshold = 0.3f;

    /* Lateral inhibition */
    config->inhibitory_spike_weight = -0.5f;
    config->inhibitory_delay = 2.0f;
    config->mexican_hat_radius = 2.0f;

    /* Competition */
    config->competition_mode = CC_COMPETITION_SOFTMAX;
    config->k_winners = 3;
    config->competition_timescale = 10.0f;

    /* Bio-async */
    config->enable_bio_async = true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create minicolumn spike pattern
 *
 * WHAT: Allocate and initialize spike pattern for minicolumn
 * WHY:  Track spike representation of minicolumn state
 * HOW:  Allocate neuron rate array
 */
static minicolumn_spike_pattern_t* create_minicolumn_pattern(
    uint32_t minicolumn_id,
    uint32_t n_neurons
) {
    minicolumn_spike_pattern_t* pattern = nimcp_malloc(sizeof(minicolumn_spike_pattern_t));
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_minicolumn_pattern: pattern is NULL");
        return NULL;
    }

    pattern->minicolumn_id = minicolumn_id;
    pattern->activation_level = 0.0f;
    pattern->n_neurons = n_neurons;
    pattern->is_winner = false;
    pattern->lateral_inhibition = 0.0f;

    pattern->neuron_rates = nimcp_malloc(n_neurons * sizeof(float));
    if (!pattern->neuron_rates) {
        nimcp_free(pattern);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_minicolumn_pattern: pattern->neuron_rates is NULL");
        return NULL;
    }

    memset(pattern->neuron_rates, 0, n_neurons * sizeof(float));
    return pattern;
}

/**
 * @brief Destroy minicolumn spike pattern
 */
static void destroy_minicolumn_pattern(minicolumn_spike_pattern_t* pattern) {
    if (!pattern) return;
    if (pattern->neuron_rates) {
        nimcp_free(pattern->neuron_rates);
    }
    nimcp_free(pattern);
}

snn_cortical_bridge_t* snn_cortical_bridge_create(
    const snn_cortical_config_t* config,
    snn_network_t* network,
    cortical_column_pool_t* pool,
    hypercolumn_t* hypercolumn
) {
    /* Guard clauses */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_create: config is NULL");
        return NULL;
    }
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_create: network is NULL");
        return NULL;
    }
    if (!pool) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_create: pool is NULL");
        return NULL;
    }
    if (!hypercolumn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_create: hypercolumn is NULL");
        return NULL;
    }

    /* Allocate bridge */
    snn_cortical_bridge_t* bridge = nimcp_malloc(sizeof(snn_cortical_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate cortical bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_cortical_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Initialize fields */
    memset(bridge, 0, sizeof(snn_cortical_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_cortical") != 0) { nimcp_free(bridge); return NULL; }
    bridge->network = network;
    bridge->pool = pool;
    bridge->hypercolumn = hypercolumn;
    bridge->config = *config;
    bridge->connected = true;
    bridge->last_update_time = 0.0f;
    bridge->update_count = 0;

    /* Get number of minicolumns from hypercolumn */
    cc_hypercolumn_stats_t hcol_stats;
    hypercolumn_get_stats(hypercolumn, &hcol_stats);
    bridge->n_minicolumns = hcol_stats.num_minicolumns;

    /* Create minicolumn spike patterns */
    bridge->minicolumn_patterns = nimcp_malloc(
        bridge->n_minicolumns * sizeof(minicolumn_spike_pattern_t*)
    );
    if (!bridge->minicolumn_patterns) {
        NIMCP_LOGGING_ERROR("Failed to allocate minicolumn patterns");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_cortical_bridge_create: bridge->minicolumn_patterns is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        bridge->minicolumn_patterns[i] = create_minicolumn_pattern(
            i, config->neurons_per_minicolumn
        );
        if (!bridge->minicolumn_patterns[i]) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j < i; j++) {
                destroy_minicolumn_pattern(bridge->minicolumn_patterns[j]);
            }
            nimcp_free(bridge->minicolumn_patterns);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_create: bridge->minicolumn_patterns is NULL");
            return NULL;
        }
    }

    /* Initialize layer activity */
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        bridge->layer_activity[layer].spike_rate = 0.0f;
        bridge->layer_activity[layer].burst_ratio = 0.0f;
        bridge->layer_activity[layer].mean_isi = 0.0f;
        bridge->layer_activity[layer].synchrony_index = 0.0f;
        bridge->layer_activity[layer].active_neurons = 0;
        bridge->layer_activity[layer].total_spikes = 0;
    }

    /* NOTE: Populations would be created here in full implementation */
    /* For now, we set them to NULL - they would reference network populations */
    bridge->layer_2_3_pop = NULL;
    bridge->layer_4_pop = NULL;
    bridge->layer_5_pop = NULL;
    bridge->layer_6_pop = NULL;

    NIMCP_LOGGING_INFO("Created SNN-cortical bridge with %u minicolumns",
                       bridge->n_minicolumns);
    return bridge;
}

void snn_cortical_bridge_destroy(snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_destroy: null bridge pointer");
        return;
    }

    /* Destroy minicolumn patterns */
    if (bridge->minicolumn_patterns) {
        for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
            destroy_minicolumn_pattern(bridge->minicolumn_patterns[i]);
        }
        nimcp_free(bridge->minicolumn_patterns);
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        snn_cortical_bridge_disconnect_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

//=============================================================================
// Bio-async Integration
//=============================================================================

int snn_cortical_bridge_connect_bio_async(snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_CORTICAL,
        .module_name = "snn_cortical_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }
    return 0;
}

int snn_cortical_bridge_disconnect_bio_async(snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_disconnect_bio_async: null bridge pointer");
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

bool snn_cortical_bridge_is_bio_async_connected(const snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Compute Gaussian tuning curve
 *
 * WHAT: Calculate neuron response based on distance from preferred stimulus
 * WHY:  Implement population coding with Gaussian tuning curves
 * HOW:  exp(-distance^2 / (2 * sigma^2))
 */
static float compute_gaussian_tuning(float stimulus, float preference, float sigma) {
    float distance = stimulus - preference;
    return expf(-(distance * distance) / (2.0f * sigma * sigma));
}

int snn_cortical_bridge_process(
    snn_cortical_bridge_t* bridge,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_process: null bridge pointer");
        return -1;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_process: null input pointer");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_process: null output pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_cortical_bridge_process: bridge not connected");
        return -1;
    }

    /* Process input through hypercolumn */
    hypercolumn_compute(bridge->hypercolumn, input, input_size);

    /* Get activation distribution */
    float* activations = nimcp_malloc(bridge->n_minicolumns * sizeof(float));
    if (!activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_cortical_bridge_process: activations is NULL");
        return -1;
    }

    hypercolumn_get_distribution(bridge->hypercolumn, activations, bridge->n_minicolumns);

    /* Update minicolumn spike patterns */
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        minicolumn_spike_pattern_t* pattern = bridge->minicolumn_patterns[i];
        pattern->activation_level = activations[i];

        /* Compute population code - Gaussian tuning curves */
        for (uint32_t n = 0; n < pattern->n_neurons; n++) {
            float neuron_preference = (float)n / (float)pattern->n_neurons;
            float tuning = compute_gaussian_tuning(
                activations[i], neuron_preference, bridge->config.tuning_curve_width
            );
            pattern->neuron_rates[n] = tuning * bridge->config.layer_4_base_rate;
        }
    }

    /* Copy activations to output */
    uint32_t copy_size = (output_size < bridge->n_minicolumns) ?
                         output_size : bridge->n_minicolumns;
    memcpy(output, activations, copy_size * sizeof(float));

    nimcp_free(activations);
    return 0;
}

int snn_cortical_bridge_update(snn_cortical_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_update: null bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_cortical_bridge_update: bridge not connected");
        return -1;
    }

    /* Update time */
    bridge->last_update_time += dt;
    bridge->update_count++;

    /* Apply lateral inhibition */
    snn_cortical_apply_lateral_inhibition(bridge);

    /* Run competition */
    snn_cortical_run_competition(bridge);

    /* Update layer activity statistics */
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        /* Statistics would be computed from actual spike trains */
        /* For now, we set placeholder values */
        bridge->layer_activity[layer].spike_rate = 0.0f;
        bridge->layer_activity[layer].burst_ratio = 0.0f;
        bridge->layer_activity[layer].mean_isi = 0.0f;
    }

    return 0;
}

uint32_t snn_cortical_generate_bursts(
    snn_cortical_bridge_t* bridge,
    cortical_layer_t layer
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_generate_bursts: null bridge pointer");
        return 0;
    }

    uint32_t burst_count = 0;

    /* Check which minicolumns exceed burst threshold */
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        minicolumn_spike_pattern_t* pattern = bridge->minicolumn_patterns[i];

        if (pattern->activation_level >= bridge->config.burst_threshold) {
            /* Would generate burst_spike_count spikes here */
            /* Spikes would be injected into corresponding SNN population */
            burst_count++;
        }
    }

    return burst_count;
}

int snn_cortical_apply_lateral_inhibition(snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_apply_lateral_inhibition: null bridge pointer");
        return -1;
    }

    /* Compute Mexican hat lateral inhibition */
    float radius = bridge->config.mexican_hat_radius;

    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        minicolumn_spike_pattern_t* pattern_i = bridge->minicolumn_patterns[i];
        float total_inhibition = 0.0f;

        for (uint32_t j = 0; j < bridge->n_minicolumns; j++) {
            if (i == j) continue;

            minicolumn_spike_pattern_t* pattern_j = bridge->minicolumn_patterns[j];

            /* Compute distance between minicolumns */
            float distance = fabsf((float)i - (float)j);

            /* Mexican hat: nearby excitation, far inhibition */
            float weight;
            if (distance < radius) {
                weight = expf(-(distance * distance) / (2.0f * radius * radius));
            } else {
                weight = -bridge->config.inhibitory_spike_weight *
                         expf(-(distance * distance) / (2.0f * radius * radius * 4.0f));
            }

            total_inhibition += weight * pattern_j->activation_level;
        }

        pattern_i->lateral_inhibition = total_inhibition;
    }

    return 0;
}

int snn_cortical_run_competition(snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_run_competition: null bridge pointer");
        return -1;
    }

    /* Reset winner flags */
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        bridge->minicolumn_patterns[i]->is_winner = false;
    }

    /* Competition based on mode */
    if (bridge->config.competition_mode == CC_COMPETITION_WINNER_TAKE_ALL) {
        /* Find single winner */
        uint32_t winner_idx = 0;
        float max_activation = 0.0f;

        for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
            float effective_activation = bridge->minicolumn_patterns[i]->activation_level -
                                        bridge->minicolumn_patterns[i]->lateral_inhibition;
            if (effective_activation > max_activation) {
                max_activation = effective_activation;
                winner_idx = i;
            }
        }

        bridge->minicolumn_patterns[winner_idx]->is_winner = true;

    } else if (bridge->config.competition_mode == CC_COMPETITION_K_WINNERS) {
        /* Find top K winners */
        /* Simple bubble-sort for small K */
        for (uint32_t k = 0; k < bridge->config.k_winners && k < bridge->n_minicolumns; k++) {
            uint32_t winner_idx = 0;
            float max_activation = -1.0f;

            for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
                if (bridge->minicolumn_patterns[i]->is_winner) continue;

                float effective_activation = bridge->minicolumn_patterns[i]->activation_level -
                                            bridge->minicolumn_patterns[i]->lateral_inhibition;
                if (effective_activation > max_activation) {
                    max_activation = effective_activation;
                    winner_idx = i;
                }
            }

            bridge->minicolumn_patterns[winner_idx]->is_winner = true;
        }
    }

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int snn_cortical_get_layer_activity(
    const snn_cortical_bridge_t* bridge,
    cortical_layer_t layer,
    cortical_layer_activity_t* activity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_get_layer_activity: null bridge pointer");
        return -1;
    }
    if (!activity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_get_layer_activity: null activity pointer");
        return -1;
    }
    if (layer >= LAYER_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_cortical_get_layer_activity: invalid layer");
        return -1;
    }

    *activity = bridge->layer_activity[layer];
    return 0;
}

int snn_cortical_get_minicolumn_pattern(
    const snn_cortical_bridge_t* bridge,
    uint32_t minicolumn_idx,
    minicolumn_spike_pattern_t* pattern
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_get_minicolumn_pattern: null bridge pointer");
        return -1;
    }
    if (!pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_get_minicolumn_pattern: null pattern pointer");
        return -1;
    }
    if (minicolumn_idx >= bridge->n_minicolumns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_cortical_get_minicolumn_pattern: invalid minicolumn index");
        return -1;
    }

    minicolumn_spike_pattern_t* src = bridge->minicolumn_patterns[minicolumn_idx];

    pattern->minicolumn_id = src->minicolumn_id;
    pattern->activation_level = src->activation_level;
    pattern->n_neurons = src->n_neurons;
    pattern->is_winner = src->is_winner;
    pattern->lateral_inhibition = src->lateral_inhibition;

    /* Note: neuron_rates array not copied - caller must allocate */
    pattern->neuron_rates = NULL;

    return 0;
}

uint32_t snn_cortical_get_winner(const snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_get_winner: null bridge pointer");
        return UINT32_MAX;
    }

    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        if (bridge->minicolumn_patterns[i]->is_winner) {
            return i;
        }
    }

    return UINT32_MAX;
}

float snn_cortical_bridge_get_activity(const snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_bridge_get_activity: null bridge pointer");
        return -1.0f;
    }

    /* Average activation across all minicolumns */
    float total = 0.0f;
    for (uint32_t i = 0; i < bridge->n_minicolumns; i++) {
        total += bridge->minicolumn_patterns[i]->activation_level;
    }

    return total / (float)bridge->n_minicolumns;
}

//=============================================================================
// Statistics
//=============================================================================

int snn_cortical_get_stats(
    const snn_cortical_bridge_t* bridge,
    uint64_t* total_spikes,
    float* mean_rate,
    uint32_t* updates
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_get_stats: null bridge pointer");
        return -1;
    }

    if (total_spikes) {
        *total_spikes = 0;
        for (int layer = 0; layer < LAYER_COUNT; layer++) {
            *total_spikes += bridge->layer_activity[layer].total_spikes;
        }
    }

    if (mean_rate) {
        float total_rate = 0.0f;
        for (int layer = 0; layer < LAYER_COUNT; layer++) {
            total_rate += bridge->layer_activity[layer].spike_rate;
        }
        *mean_rate = total_rate / (float)LAYER_COUNT;
    }

    if (updates) {
        *updates = bridge->update_count;
    }

    return 0;
}

void snn_cortical_reset_stats(snn_cortical_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_cortical_reset_stats: null bridge pointer");
        return;
    }

    bridge->update_count = 0;
    bridge->last_update_time = 0.0f;

    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        bridge->layer_activity[layer].total_spikes = 0;
        bridge->layer_activity[layer].spike_rate = 0.0f;
        bridge->layer_activity[layer].burst_ratio = 0.0f;
        bridge->layer_activity[layer].mean_isi = 0.0f;
        bridge->layer_activity[layer].synchrony_index = 0.0f;
        bridge->layer_activity[layer].active_neurons = 0;
    }
}

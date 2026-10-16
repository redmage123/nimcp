/**
 * @file nimcp_weights.c
 * @brief Immutable network weights separated from mutable statistics implementation
 * @version 2.7.0
 * @date 2025-11-04
 */

#include "plasticity/adaptive/nimcp_weights.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include <string.h>

//=============================================================================
// Network Weights Implementation
//=============================================================================

network_weights_t* network_weights_create(neural_network_t base_network,
                                          const adaptive_spike_params_t* spike_params) {
    if (!base_network || !spike_params) {
        return NULL;
    }

    network_weights_t* weights = (network_weights_t*)nimcp_calloc(1, sizeof(network_weights_t));
    if (!weights) {
        return NULL;
    }

    // Initialize reference counting
    weights->refcount = (uint32_t*)nimcp_malloc(sizeof(uint32_t));
    if (!weights->refcount) {
        nimcp_free(weights);
        return NULL;
    }
    *weights->refcount = 1;

    weights->refcount_mutex = (pthread_mutex_t*)nimcp_malloc(sizeof(pthread_mutex_t));
    if (!weights->refcount_mutex) {
        nimcp_free(weights->refcount);
        nimcp_free(weights);
        return NULL;
    }

    if (pthread_mutex_init(weights->refcount_mutex, NULL) != 0) {
        nimcp_free(weights->refcount_mutex);
        nimcp_free(weights->refcount);
        nimcp_free(weights);
        return NULL;
    }

    // Store network and parameters
    weights->base_network = base_network;
    weights->spike_params = *spike_params;

    // Calculate metadata
    // TODO: Add accessor functions to get network size
    weights->num_neurons = 0;  // Placeholder - would need accessor functions
    weights->num_synapses = 0;  // Would need to count actual synapses in network

    // Estimate memory usage
    weights->memory_bytes = sizeof(network_weights_t);

    weights->pruning_threshold = 0.01f;  // Default threshold

    return weights;
}

uint32_t network_weights_retain(network_weights_t* weights) {
    if (!weights || !weights->refcount || !weights->refcount_mutex) {
        return 0;
    }

    pthread_mutex_lock(weights->refcount_mutex);
    (*weights->refcount)++;
    uint32_t new_count = *weights->refcount;
    pthread_mutex_unlock(weights->refcount_mutex);

    return new_count;
}

uint32_t network_weights_release(network_weights_t* weights) {
    if (!weights || !weights->refcount || !weights->refcount_mutex) {
        return 0;
    }

    pthread_mutex_lock(weights->refcount_mutex);
    (*weights->refcount)--;
    uint32_t remaining_count = *weights->refcount;
    pthread_mutex_unlock(weights->refcount_mutex);

    if (remaining_count == 0) {
        // Last reference - destroy everything
        if (weights->base_network) {
            neural_network_destroy(weights->base_network);
        }

        pthread_mutex_destroy(weights->refcount_mutex);
        nimcp_free(weights->refcount_mutex);
        nimcp_free(weights->refcount);
        nimcp_free(weights);

        return 0;
    }

    return remaining_count;
}

uint32_t network_weights_get_refcount(const network_weights_t* weights) {
    if (!weights || !weights->refcount || !weights->refcount_mutex) {
        return 0;
    }

    pthread_mutex_lock(weights->refcount_mutex);
    uint32_t count = *weights->refcount;
    pthread_mutex_unlock(weights->refcount_mutex);

    return count;
}

network_weights_t* network_weights_clone(const network_weights_t* weights) {
    if (!weights || !weights->base_network) {
        return NULL;
    }

    // TODO: Implement network cloning
    // For now, return NULL as neural_network_clone doesn't exist yet
    // This would need to be implemented when proper cloning support is added
    return NULL;
}

//=============================================================================
// Network Statistics Implementation
//=============================================================================

nimcp_network_stats_t* network_stats_create(void) {
    nimcp_network_stats_t* stats = (nimcp_network_stats_t*)nimcp_calloc(1, sizeof(nimcp_network_stats_t));
    if (!stats) {
        return NULL;
    }

    // Initialize with default values
    stats->running_sparsity = 0.0f;
    stats->total_inferences = 0;
    stats->avg_inference_time_us = 0.0f;
    stats->active_neuron_count = 0;

    stats->total_learning_steps = 0;
    stats->avg_learning_time_us = 0.0f;
    stats->current_loss = 0.0f;
    stats->accuracy = 0.0f;

    stats->avg_threshold = 0.0f;
    stats->min_threshold = 1.0f;
    stats->max_threshold = 0.0f;

    stats->memory_bytes = sizeof(nimcp_network_stats_t);

    return stats;
}

void network_stats_destroy(nimcp_network_stats_t* stats) {
    if (!stats) {
        return;
    }

    nimcp_free(stats);
}

void network_stats_reset(nimcp_network_stats_t* stats) {
    if (!stats) {
        return;
    }

    stats->running_sparsity = 0.0f;
    stats->total_inferences = 0;
    stats->avg_inference_time_us = 0.0f;
    stats->active_neuron_count = 0;

    stats->total_learning_steps = 0;
    stats->avg_learning_time_us = 0.0f;
    stats->current_loss = 0.0f;
    stats->accuracy = 0.0f;

    stats->avg_threshold = 0.0f;
    stats->min_threshold = 1.0f;
    stats->max_threshold = 0.0f;
}

void network_stats_update_sparsity(nimcp_network_stats_t* stats,
                                    uint32_t active_count,
                                    uint32_t total_count) {
    if (!stats || total_count == 0) {
        return;
    }

    float current_sparsity = 1.0f - ((float)active_count / total_count);

    // Exponential moving average
    const float alpha = 0.1f;
    if (stats->total_inferences == 0) {
        stats->running_sparsity = current_sparsity;
    } else {
        stats->running_sparsity = alpha * current_sparsity + (1.0f - alpha) * stats->running_sparsity;
    }

    stats->active_neuron_count = active_count;
}

void network_stats_record_inference(nimcp_network_stats_t* stats, float time_us) {
    if (!stats) {
        return;
    }

    // Update exponential moving average
    const float alpha = 0.1f;
    if (stats->total_inferences == 0) {
        stats->avg_inference_time_us = time_us;
    } else {
        stats->avg_inference_time_us = alpha * time_us + (1.0f - alpha) * stats->avg_inference_time_us;
    }

    stats->total_inferences++;
}

void network_stats_record_learning(nimcp_network_stats_t* stats, float time_us, float loss) {
    if (!stats) {
        return;
    }

    // Update exponential moving average
    const float alpha = 0.1f;
    if (stats->total_learning_steps == 0) {
        stats->avg_learning_time_us = time_us;
    } else {
        stats->avg_learning_time_us = alpha * time_us + (1.0f - alpha) * stats->avg_learning_time_us;
    }

    stats->current_loss = loss;
    stats->total_learning_steps++;
}

//=============================================================================
// Integrated Adaptive Network (Weights + Stats)
//=============================================================================

// These functions would integrate with the existing adaptive_network_t structure
// For now, they're placeholders that would need to be implemented alongside
// updates to the adaptive_network_struct definition

bool adaptive_network_owns_weights(const adaptive_network_t network) {
    if (!network) {
        return false;
    }

    // This would check if the network has a private copy of weights
    // or is sharing weights with other networks
    // Implementation depends on adaptive_network_struct changes
    return true;  // Placeholder
}

const network_weights_t* adaptive_network_get_weights(const adaptive_network_t network) {
    if (!network) {
        return NULL;
    }

    // This would return the weights handle from the adaptive network
    // Implementation depends on adaptive_network_struct changes
    return NULL;  // Placeholder
}

nimcp_network_stats_t* adaptive_network_get_stats(const adaptive_network_t network) {
    if (!network) {
        return NULL;
    }

    // This would return the statistics handle from the adaptive network
    // Implementation depends on adaptive_network_struct changes
    return NULL;  // Placeholder
}

bool adaptive_network_trigger_weight_cow(adaptive_network_t network) {
    if (!network) {
        return false;
    }

    // This would:
    // 1. Check if weights are currently shared (refcount > 1)
    // 2. If shared, clone the weights to get a private copy
    // 3. Update the network to point to the new private weights
    // 4. Decrement refcount on old shared weights
    // Implementation depends on adaptive_network_struct changes

    return false;  // Placeholder
}

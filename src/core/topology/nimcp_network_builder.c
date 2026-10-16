#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_network_builder.c - Network Builder Implementation
//=============================================================================

#include "core/topology/nimcp_network_builder.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "network_builder"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(network_builder)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void network_builder_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_TOPOLOGY_NETWORK_BUILDER,
        .module_name = "network_builder",
        .inbox_capacity = 64,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for network_builder module");
    }
}

__attribute__((constructor))
static void network_builder_bio_init(void) {
    pthread_once(&bio_init_once, network_builder_bio_init_impl);
}

__attribute__((destructor))
static void network_builder_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for network_builder module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

//=============================================================================
// Default Configuration
//=============================================================================

network_builder_config_t network_builder_default(void) {
    network_builder_config_t config;
    memset(&config, 0, sizeof(config));

    // Basic network defaults
    config.num_neurons = 100;
    config.ei_ratio = 0.8F;  // 80% excitatory
    config.enable_stdp = true;
    config.enable_homeostasis = true;

    // No topology by default
    config.use_topology = false;

    // No pink noise weights by default
    config.use_pink_noise_weights = false;
    config.noise_amplitude = 0.5F;
    config.base_weight = 0.0F;

    // Random seed
    config.random_seed = 0;  // 0 means use time-based seed
    config.verbose = false;

    return config;
}

//=============================================================================
// Network Building
//=============================================================================

neural_network_t network_builder_build(const network_builder_config_t* config) {
    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_builder_build: config is NULL");
        return NULL;
    }

    // Process pending bio-async messages
    if (bio_async_enabled && bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    // Guard: Invalid neuron count
    if (config->num_neurons < 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "network_builder_build: num_neurons must be >= 1");
        return NULL;
    }

    if (config->verbose) {
        LOG_INFO(LOG_MODULE, "Building network with %u neurons", config->num_neurons);
    }

    // Step 1: Create base network
    network_config_t net_config = {
        .num_neurons = config->num_neurons,
        .ei_ratio = config->ei_ratio,
        .enable_stdp = config->enable_stdp,
        .enable_homeostasis = config->enable_homeostasis,
        .learning_rate = 0.01F,
        .hebbian_rate = 0.001F,
        .stdp_window = 20.0F,
        .homeostatic_rate = 0.001F,
        .target_activity = 0.05F,
        .adaptation_rate = 0.01F,
        .refractory_period = 2.0F,
        .min_weight = -1.0F,
        .max_weight = 1.0F,
        .update_interval = 1,
        .input_size = 0,
        .output_size = 0,
        .num_layers = 0,
        .layer_sizes = NULL,
        .enable_hebbian = false,
        .enable_oja = false,
        .neuron_model = NEURON_MODEL_LIF,
        .model_params = NULL
    };

    neural_network_t network = neural_network_create(&net_config);
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "network_builder_build: failed to create neural network");
        return NULL;
    }

    // Step 2: Apply topology if configured
    if (config->use_topology) {
        if (config->verbose) {
            LOG_INFO(LOG_MODULE, "Applying topology");
        }

        topology_stats_t stats;
        bool success = topology_generate(network, &config->topology_config, &stats);

        if (!success) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "network_builder_build: topology generation failed");
            neural_network_destroy(network);
            return NULL;
        }

        if (config->verbose) {
            LOG_INFO(LOG_MODULE, "Topology created: %u neurons, %u synapses, %u hubs",
                   stats.num_neurons, stats.num_synapses, stats.num_hubs);
            LOG_INFO(LOG_MODULE, "Average degree: %.2f, std: %.2f",
                   stats.avg_degree, stats.degree_std);
        }
    }

    // Step 3: Initialize weights with pink noise if configured
    if (config->use_pink_noise_weights) {
        if (config->verbose) {
            LOG_INFO(LOG_MODULE, "Initializing weights with pink noise (amplitude: %.2f)",
                   config->noise_amplitude);
        }

        bool success = network_init_weights_pink_noise(
            network,
            config->noise_amplitude,
            config->base_weight
        );

        if (!success) {
            LOG_WARN(LOG_MODULE, "Pink noise weight initialization failed");
            // Don't fail the whole build, just use default weights
        }
    }

    if (config->verbose) {
        LOG_INFO(LOG_MODULE, "Network build complete");
    }

    return network;
}

//=============================================================================
// Shorthand Functions
//=============================================================================

neural_network_t network_create_scale_free(uint32_t num_neurons, float gamma) {
    network_builder_config_t config = network_builder_default();
    config.num_neurons = num_neurons;
    config.use_topology = true;
    config.topology_config.type = TOPOLOGY_SCALE_FREE;
    config.topology_config.params.scale_free = topology_default_scale_free_config();
    config.topology_config.params.scale_free.power_law_gamma = gamma;

    return network_builder_build(&config);
}

neural_network_t network_create_fractal(uint32_t num_neurons, float fractal_dimension) {
    network_builder_config_t config = network_builder_default();
    config.num_neurons = num_neurons;
    config.use_topology = true;
    config.topology_config.type = TOPOLOGY_FRACTAL;
    config.topology_config.params.fractal = topology_default_fractal_config();
    config.topology_config.params.fractal.fractal_dimension = fractal_dimension;

    return network_builder_build(&config);
}

//=============================================================================
// Pink Noise Weight Initialization
//=============================================================================

/* Single authoritative definition of neural_network_struct */
#include "core/neuralnet/nimcp_neuralnet_internal.h"

bool network_init_weights_pink_noise(
    neural_network_t network,
    float amplitude,
    float base_weight
) {
    // Guard: NULL network
    if (!network) {
        LOG_ERROR(LOG_MODULE, "network_init_weights_pink_noise: NULL network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_init_weights_pink_noise: network is NULL");
        return false;
    }

    // Step 1: Count total number of synapses
    struct neural_network_struct* net = (struct neural_network_struct*)network;
    uint32_t total_synapses = 0;

    for (uint32_t i = 0; i < net->num_neurons; i++) {
        total_synapses += NEURON_OUT_COUNT(&net->neurons[i]);
    }

    if (total_synapses == 0) {
        LOG_INFO(LOG_MODULE, "No synapses in network, skipping weight initialization");
        return true;
    }

    // Step 2: Create pink noise generator
    pink_noise_config_t noise_config = {
        .method = PINK_NOISE_VOSS,
        .alpha = 1.0F,              // Pure pink noise (1/f)
        .amplitude = amplitude,
        .min_frequency = 0.1F,
        .max_frequency = 1000.0F,
        .sample_rate = 44100.0F,
        .seed = 0                   // Use time-based seed
    };

    pink_noise_generator_t generator = pink_noise_create(&noise_config);
    if (!generator) {
        LOG_ERROR(LOG_MODULE, "Failed to create pink noise generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network_init_weights_pink_noise: generator is NULL");
        return false;
    }

    // Step 3: Generate all noise samples at once
    float* noise_samples = (float*)nimcp_malloc(total_synapses * sizeof(float));
    if (!noise_samples) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate noise samples array");
        pink_noise_destroy(generator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network_init_weights_pink_noise: noise_samples is NULL");
        return false;
    }

    if (!pink_noise_generate(generator, noise_samples, total_synapses)) {
        LOG_ERROR(LOG_MODULE, "Failed to generate pink noise samples");
        nimcp_free(noise_samples);
        pink_noise_destroy(generator);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "network_init_weights_pink_noise: pink_noise_generate is NULL");
        return false;
    }

    // Step 4: Apply noise to all synapses
    uint32_t sample_idx = 0;
    for (uint32_t i = 0; i < net->num_neurons; i++) {
        neuron_t* neuron = &net->neurons[i];

        uint32_t nsyn = NEURON_OUT_COUNT(neuron);
        for (uint32_t j = 0; j < nsyn; j++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, j);
            if (h) {
                h->weight = base_weight + noise_samples[sample_idx];
            }
            sample_idx++;
        }
    }

    // Cleanup
    nimcp_free(noise_samples);
    pink_noise_destroy(generator);

    return true;
}

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_fractal_topology.c - Fractal Network Topology Implementation
//=============================================================================

#include "core/topology/nimcp_fractal_topology.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/containers/nimcp_graph.h"
#include "utils/algorithms/nimcp_centrality.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"

#define LOG_MODULE "fractal_topology"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fractal_topology)

//=============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
//=============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void fractal_topology_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_TOPOLOGY_FRACTAL,
        .module_name = "fractal_topology",
        .inbox_capacity = 64,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for fractal_topology module");
    }
}

__attribute__((constructor))
static void fractal_topology_bio_init(void) {
    pthread_once(&bio_init_once, fractal_topology_bio_init_impl);
}

__attribute__((destructor))
static void fractal_topology_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for fractal_topology module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

/* Single authoritative definition of neural_network_struct */
#include "core/neuralnet/nimcp_neuralnet_internal.h"

//=============================================================================
// Error Handling - Thread-local storage
//=============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/**
 * WHY: Thread-safe error reporting without global state
 * HOW: Thread-local storage ensures each thread has independent error string
 */
static void set_error(const char* message) {
    if (!message) return;
    strncpy(last_error, message, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
}

static void clear_error(void) {
    last_error[0] = '\0';
}

const char* topology_get_last_error(void) {
    return last_error[0] ? last_error : NULL;
}

//=============================================================================
// Default Configurations
//=============================================================================

/**
 * WHY: Provide sensible defaults based on empirical neuroscience data
 * HOW: Values from Sporns et al. (2004) and Van den Heuvel & Sporns (2011)
 */
scale_free_config_t topology_default_scale_free_config(void) {
    return (scale_free_config_t){
        .power_law_gamma = -2.1F,      // Typical cortical value
        .hub_ratio = 0.15F,            // 15% hubs matches cortical data
        .min_degree = 3,               // Minimum for connectivity
        .max_degree = 100,             // Reasonable default (can be auto-computed)
        .spatial_constraint = 0.5F,    // Moderate distance dependence
        .bidirectional = false         // Directed by default
    };
}

fractal_config_t topology_default_fractal_config(void) {
    return (fractal_config_t){
        .fractal_dimension = 2.5F,     // Cortical measured value
        .hierarchy_levels = 4,         // Matches cortical hierarchy
        .branching_factor = 3.0F,      // Average 3 branches per level
        .scale_factor = 0.7F,          // 70% size reduction per level
        .clustering_coeff = 0.4F       // Target local clustering
    };
}

//=============================================================================
// Configuration Validation
//=============================================================================

/**
 * WHY: Prevent invalid parameters that would produce broken networks
 * HOW: Guard clauses check each parameter against valid ranges
 */
bool topology_validate_config(const topology_config_t* config) {
    // Guard: NULL input
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_validate_config: config is NULL");
        set_error("Configuration is NULL");
        return false;
    }

    // Clear any previous errors on successful start
    clear_error();

    // Guard: Invalid topology type
    if (config->type < TOPOLOGY_RANDOM || config->type > TOPOLOGY_SPATIAL) {
        set_error("Invalid topology type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: validation failed");
        return false;
    }

    // Type-specific validation
    if (config->type == TOPOLOGY_SCALE_FREE) {
        const scale_free_config_t* sf = &config->params.scale_free;

        // Guard: Invalid power law exponent
        if (sf->power_law_gamma >= 0.0F || sf->power_law_gamma < -5.0F) {
            set_error("Power law gamma must be between -5.0 and 0.0");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: capacity exceeded");
            return false;
        }

        // Guard: Invalid hub ratio
        if (sf->hub_ratio < 0.0F || sf->hub_ratio > 0.5F) {
            set_error("Hub ratio must be between 0.0 and 0.5");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: validation failed");
            return false;
        }

        // Guard: Invalid minimum degree
        if (sf->min_degree < 1) {
            set_error("Minimum degree must be at least 1");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: validation failed");
            return false;
        }

        // Guard: Invalid spatial constraint
        if (sf->spatial_constraint < 0.0F || sf->spatial_constraint > 1.0F) {
            set_error("Spatial constraint must be between 0.0 and 1.0");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: validation failed");
            return false;
        }
    } else if (config->type == TOPOLOGY_FRACTAL) {
        const fractal_config_t* frac = &config->params.fractal;

        // Guard: Invalid fractal dimension
        if (frac->fractal_dimension < 1.0F || frac->fractal_dimension > 3.0F) {
            set_error("Fractal dimension must be between 1.0 and 3.0");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: validation failed");
            return false;
        }

        // Guard: Invalid hierarchy levels
        if (frac->hierarchy_levels < 2 || frac->hierarchy_levels > 10) {
            set_error("Hierarchy levels must be between 2 and 10");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: validation failed");
            return false;
        }

        // Guard: Invalid branching factor
        if (frac->branching_factor < 1.5F || frac->branching_factor > 10.0F) {
            set_error("Branching factor must be between 1.5 and 10.0");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: validation failed");
            return false;
        }

        // Guard: Invalid scale factor
        if (frac->scale_factor <= 0.0F || frac->scale_factor >= 1.0F) {
            set_error("Scale factor must be between 0.0 and 1.0");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_validate_config: capacity exceeded");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Power Law Distribution Sampling
//=============================================================================

/**
 * WHY: Generate degree values following power-law P(k) ~ k^γ
 * HOW: Inverse transform sampling from power-law CDF
 *
 * ALGORITHM:
 *   1. Generate uniform random u ∈ [0,1]
 *   2. Compute k = k_min * (1-u)^(-1/(γ+1))
 *   3. This gives k ~ k^γ for k ≥ k_min
 */
static uint32_t sample_power_law(float gamma, uint32_t k_min, uint32_t k_max) {
    // Guard: Invalid parameters
    if (gamma >= 0.0F || k_min < 1 || k_max < k_min) {
        return k_min;
    }

    float u = (float)nimcp_tl_rand() / (float)RAND_MAX;
    float exponent = -1.0F / (gamma + 1.0F);
    float k_float = (float)k_min * powf(1.0F - u, exponent);

    uint32_t k = (uint32_t)k_float;

    // Clamp to valid range
    if (k < k_min) k = k_min;
    if (k > k_max) k = k_max;

    return k;
}

//=============================================================================
// Degree Distribution Computation
//=============================================================================

/**
 * WHY: Compute degree for each neuron (needed for preferential attachment)
 * HOW: Count incoming + outgoing synapses per neuron
 */
static uint32_t* compute_degree_distribution(neural_network_t network, uint32_t num_neurons) {
    // Guard: NULL network
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }

    // Guard: Zero neurons
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_degree_distribution: num_neurons is zero");
        return NULL;
    }

    uint32_t* degrees = (uint32_t*)nimcp_calloc(num_neurons, sizeof(uint32_t));

    // Guard: Allocation failure
    if (!degrees) {
        set_error("Failed to allocate degree array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_degree_distribution: degrees is NULL");
        return NULL;
    }

    // Count degrees for each neuron
    // NOTE: This requires access to network internals
    // For now, we'll use a placeholder - needs integration with neural_network API

    return degrees;
}

//=============================================================================
// Preferential Attachment Selection
//=============================================================================

/**
 * WHY: Select target neuron with probability proportional to degree
 * HOW: Weighted random sampling using cumulative degree distribution
 *
 * ALGORITHM:
 *   1. Build cumulative sum: cumsum[i] = Σ(degree[0..i])
 *   2. Generate random r ∈ [0, total_degree]
 *   3. Binary search to find i where cumsum[i-1] < r ≤ cumsum[i]
 *   4. Return neuron i
 */
static uint32_t select_preferential(const uint32_t* degrees, uint32_t num_neurons, uint32_t exclude_idx) {
    // Guard: NULL degrees
    if (!degrees) return 0;

    // Guard: No neurons
    if (num_neurons == 0) return 0;

    // Build cumulative distribution
    uint32_t total_degree = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (i != exclude_idx) {
            total_degree += degrees[i] + 1;  // +1 to ensure non-zero probability
        }
    }

    // Guard: Zero total degree
    if (total_degree == 0) {
        // Fallback to uniform random
        uint32_t idx = nimcp_tl_rand() % num_neurons;
        return (idx == exclude_idx) ? (idx + 1) % num_neurons : idx;
    }

    // Sample from distribution
    uint32_t r = nimcp_tl_rand() % total_degree;
    uint32_t cumsum = 0;

    for (uint32_t i = 0; i < num_neurons; i++) {
        if (i == exclude_idx) continue;

        cumsum += degrees[i] + 1;
        if (r < cumsum) {
            return i;
        }
    }

    // Fallback (should never reach here)
    return num_neurons - 1;
}

//=============================================================================
// Scale-Free Topology Generation
//=============================================================================

/**
 * WHY: Generate biologically realistic scale-free connectivity
 * HOW: Barabási-Albert preferential attachment with spatial constraints
 *
 * ALGORITHM:
 *   1. Initialize small fully-connected seed network (3-5 neurons)
 *   2. For each remaining neuron:
 *      a. Determine degree from power-law distribution
 *      b. Select targets via preferential attachment (P ∝ degree)
 *      c. Apply spatial constraint if enabled
 *      d. Create synapses
 *   3. Update degree distribution after each neuron
 *
 * BIOLOGICAL JUSTIFICATION:
 * - Preferential attachment models "rich get richer" in brain development
 * - Spatial constraints model axonal growth cone guidance
 * - Results in hub neurons (high-degree) and sparse connections (efficiency)
 */
bool topology_generate_scale_free(
    neural_network_t network,
    const scale_free_config_t* config,
    topology_stats_t* stats
) {
    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_generate_scale_free: network is NULL");
        set_error("Network is NULL");
        return false;
    }

    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_generate_scale_free: config is NULL");
        set_error("Configuration is NULL");
        return false;
    }

    // Clear any previous errors
    clear_error();

    // Validate configuration
    topology_config_t temp_config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params.scale_free = *config
    };

    // Guard: Invalid config
    if (!topology_validate_config(&temp_config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_generate_scale_free: topology_validate_config is NULL");
        return false;  // Error message already set by validate
    }

    // Get network size
    uint32_t num_neurons = network->num_neurons;

    // Guard: Too few neurons
    if (num_neurons < 3) {
        set_error("Network must have at least 3 neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_generate_scale_free: validation failed");
        return false;
    }

    // Compute max degree if not specified
    uint32_t max_degree = config->max_degree;
    if (max_degree == 0) {
        max_degree = num_neurons / 10;
        if (max_degree < config->min_degree) {
            max_degree = config->min_degree;
        }
    }

    // Initialize degree tracking
    uint32_t* degrees = compute_degree_distribution(network, num_neurons);

    // Guard: Degree allocation failed
    if (!degrees) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "topology_generate_scale_free: failed to allocate degree array");
        return false;  // Error message already set
    }

    // Seed random number generator
    srand((unsigned int)time(NULL));

    // Create initial seed network (fully connected)
    // WHAT: Fully connect first 3 neurons to seed scale-free growth
    // WHY: Barabási-Albert model requires initial connected network
    // HOW: Create bidirectional connections between all pairs in seed
    uint32_t seed_size = 3;
    for (uint32_t i = 0; i < seed_size; i++) {
        for (uint32_t j = i + 1; j < seed_size; j++) {
            // WHAT: Create bidirectional synapse connection
            // WHY: Scale-free networks are typically undirected
            // HOW: Call neural_network_add_connection for both directions
            // FIX (Phase 5): Was TODO, now implemented
            float initial_weight = 0.5F;  // Start with moderate weight
            neural_network_add_connection(network, i, j, initial_weight);
            neural_network_add_connection(network, j, i, initial_weight);

            degrees[i]++;
            degrees[j]++;
        }
    }

    // Add remaining neurons with preferential attachment
    uint32_t total_synapses = seed_size * (seed_size - 1) / 2;

    for (uint32_t n = seed_size; n < num_neurons; n++) {
        // Sample degree from power-law
        uint32_t degree = sample_power_law(config->power_law_gamma,
                                           config->min_degree,
                                           max_degree);

        // Create connections via preferential attachment
        // WHAT: Connect new neuron to existing neurons based on their degree
        // WHY: Preferential attachment creates scale-free topology
        // HOW: Higher-degree neurons more likely to receive new connections
        for (uint32_t m = 0; m < degree; m++) {
            uint32_t target = select_preferential(degrees, n, n);  // Select from existing neurons

            // WHAT: Create bidirectional synapse between new neuron and target
            // WHY: Scale-free networks typically have undirected edges
            // HOW: Call neural_network_add_connection for both directions
            // FIX (Phase 5): Was TODO, now implemented
            float weight = 0.5F;  // Moderate initial weight
            neural_network_add_connection(network, n, target, weight);
            neural_network_add_connection(network, target, n, weight);

            degrees[n]++;
            degrees[target]++;
            total_synapses++;
        }
    }

    // Compute statistics if requested
    // WHAT: Calculate full network topology metrics
    // WHY: Caller needs comprehensive analysis of generated network
    // HOW: Use topology_compute_stats to calculate all metrics
    nimcp_free(degrees);

    if (stats) {
        // topology_compute_stats will compute all metrics including:
        // - clustering_coefficient, characteristic_path, power_law_fit
        // - hub metrics, small_world_sigma, degree distribution
        return topology_compute_stats(network, stats);
    }

    return true;
}

//=============================================================================
// Fractal Topology Generation
//=============================================================================

/**
 * WHY: Generate hierarchical self-similar network structure
 * HOW: Recursive subdivision with scale-invariant patterns
 *
 * ALGORITHM:
 *   1. Partition neurons into hierarchy_levels groups
 *   2. At level 0 (bottom), create dense local clusters
 *   3. At each higher level:
 *      a. Create sparse inter-cluster connections
 *      b. Connection probability ~ distance^(-fractal_dimension)
 *   4. Ensure connectivity between all levels
 */
bool topology_generate_fractal(
    neural_network_t network,
    const fractal_config_t* config,
    topology_stats_t* stats
) {
    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_generate_fractal: network is NULL");
        set_error("Network is NULL");
        return false;
    }

    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_generate_fractal: config is NULL");
        set_error("Configuration is NULL");
        return false;
    }

    // Validate configuration
    topology_config_t temp_config = {
        .type = TOPOLOGY_FRACTAL,
        .params.fractal = *config
    };

    // Guard: Invalid config
    if (!topology_validate_config(&temp_config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_generate_fractal: topology_validate_config is NULL");
        return false;
    }

    // Clear any previous errors
    clear_error();

    // Get network size
    uint32_t num_neurons = network->num_neurons;

    // Guard: Too few neurons for hierarchical structure
    if (num_neurons < config->hierarchy_levels) {
        set_error("Network must have at least as many neurons as hierarchy levels");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_generate_fractal: validation failed");
        return false;
    }

    // Seed random number generator
    srand((unsigned int)time(NULL));

    // WHAT: Partition neurons into hierarchical clusters
    // WHY: Fractal topology requires recursive subdivision
    // HOW: Assign each neuron to a level and cluster

    // Calculate neurons per level (exponential distribution)
    uint32_t neurons_per_level[10];  // Max 10 levels
    uint32_t levels = config->hierarchy_levels < 10 ? config->hierarchy_levels : 10;

    // Distribute neurons across levels (more at bottom)
    uint32_t remaining = num_neurons;
    float total_weight = 0.0F;
    for (uint32_t l = 0; l < levels; l++) {
        total_weight += powf(config->branching_factor, (float)l);
    }

    for (uint32_t l = 0; l < levels; l++) {
        float weight = powf(config->branching_factor, (float)l) / total_weight;
        neurons_per_level[l] = (uint32_t)(weight * num_neurons);
        if (neurons_per_level[l] == 0) neurons_per_level[l] = 1;
        remaining -= neurons_per_level[l];
    }

    // Distribute any remainder to level 0
    neurons_per_level[0] += remaining;

    // WHAT: Create dense local clusters at each level
    // WHY: Fractal topology has high local clustering
    // HOW: Connect neurons within same cluster with high probability

    uint32_t total_synapses = 0;
    uint32_t neuron_idx = 0;

    for (uint32_t level = 0; level < levels; level++) {
        uint32_t level_size = neurons_per_level[level];

        // Calculate cluster size for this level
        uint32_t cluster_size = (uint32_t)(config->scale_factor * level_size);
        if (cluster_size < 2) cluster_size = 2;

        uint32_t num_clusters = (level_size + cluster_size - 1) / cluster_size;

        // Create connections within each cluster
        for (uint32_t cluster = 0; cluster < num_clusters; cluster++) {
            uint32_t cluster_start = neuron_idx + cluster * cluster_size;
            uint32_t cluster_end = cluster_start + cluster_size;
            if (cluster_end > neuron_idx + level_size) {
                cluster_end = neuron_idx + level_size;
            }

            // Connect neurons within cluster
            for (uint32_t i = cluster_start; i < cluster_end && i < num_neurons; i++) {
                for (uint32_t j = i + 1; j < cluster_end && j < num_neurons; j++) {
                    // Connection probability based on clustering coefficient
                    float prob = ((float)nimcp_tl_rand() / (float)RAND_MAX);
                    if (prob < config->clustering_coeff) {
                        float weight = 0.5F;
                        neural_network_add_connection(network, i, j, weight);
                        neural_network_add_connection(network, j, i, weight);
                        total_synapses++;
                    }
                }
            }
        }

        neuron_idx += level_size;
    }

    // WHAT: Create inter-level connections
    // WHY: Connect hierarchy levels to ensure global connectivity
    // HOW: Connection probability decreases with distance following power law

    neuron_idx = 0;
    for (uint32_t level = 0; level < levels - 1; level++) {
        uint32_t level_start = neuron_idx;
        uint32_t level_end = neuron_idx + neurons_per_level[level];
        uint32_t next_level_start = level_end;
        uint32_t next_level_end = next_level_start + neurons_per_level[level + 1];

        // Connect some neurons from this level to next level
        uint32_t connections_to_make = (uint32_t)(neurons_per_level[level] * config->scale_factor);
        if (connections_to_make < 1) connections_to_make = 1;

        for (uint32_t c = 0; c < connections_to_make; c++) {
            uint32_t src = level_start + (nimcp_tl_rand() % neurons_per_level[level]);
            uint32_t dst = next_level_start + (nimcp_tl_rand() % neurons_per_level[level + 1]);

            if (src < num_neurons && dst < num_neurons) {
                float weight = 0.5F;
                neural_network_add_connection(network, src, dst, weight);
                neural_network_add_connection(network, dst, src, weight);
                total_synapses++;
            }
        }

        neuron_idx = level_end;
    }

    // Compute statistics if requested
    if (stats) {
        return topology_compute_stats(network, stats);
    }

    return true;
}

//=============================================================================
// Unified Topology Generation
//=============================================================================

/**
 * WHY: Single entry point for all topology types
 * HOW: Strategy pattern - dispatch based on config.type
 */
bool topology_generate(
    neural_network_t network,
    const topology_config_t* config,
    topology_stats_t* stats
) {
    // Process pending bio-async messages
    if (bio_async_enabled && bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_generate: network is NULL");
        set_error("Network is NULL");
        return false;
    }

    // Guard: NULL config
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_generate: config is NULL");
        set_error("Configuration is NULL");
        return false;
    }

    // Validate configuration first
    if (!topology_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_generate: topology_validate_config is NULL");
        return false;
    }

    // Strategy pattern: Dispatch to appropriate generator
    switch (config->type) {
        case TOPOLOGY_SCALE_FREE:
            return topology_generate_scale_free(network, &config->params.scale_free, stats);

        case TOPOLOGY_FRACTAL:
            return topology_generate_fractal(network, &config->params.fractal, stats);

        case TOPOLOGY_RANDOM:
        case TOPOLOGY_SMALL_WORLD:
        case TOPOLOGY_SPATIAL:
            set_error("Topology type not yet implemented");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_generate: operation failed");
            return false;

        default:
            set_error("Unknown topology type");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_generate: operation failed");
            return false;
    }
}

//=============================================================================
// Topology Analysis Functions
//=============================================================================

/**
 * WHAT: Compute clustering coefficient for a single neuron
 * WHY: Measures local transitivity (friend-of-friend connections)
 * HOW: Count triangles / possible triangles for neuron's neighbors
 *
 * Clustering coefficient C_i = (2 * T_i) / (k_i * (k_i - 1))
 * where T_i = number of triangles through node i, k_i = degree of node i
 */
static float compute_local_clustering(neural_network_t network, uint32_t neuron_id) {
    neuron_t* neuron = neural_network_get_neuron(network, neuron_id);
    if (!neuron || NEURON_OUT_COUNT(neuron) < 2) {
        return 0.0F;  // Need at least 2 neighbors for triangles
    }

    uint32_t degree = NEURON_OUT_COUNT(neuron);
    uint32_t triangles = 0;

    // Count triangles: for each pair of neighbors, check if they're connected
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
        synapse_handle_t* hi = NEURON_OUT_HANDLE(neuron, i);
        if (!hi) continue;
        uint32_t neighbor_i = hi->target_neuron_id;
        neuron_t* ni = neural_network_get_neuron(network, neighbor_i);
        if (!ni) continue;

        for (uint32_t j = i + 1; j < NEURON_OUT_COUNT(neuron); j++) {
            synapse_handle_t* hj = NEURON_OUT_HANDLE(neuron, j);
            if (!hj) continue;
            uint32_t neighbor_j = hj->target_neuron_id;

            // Check if neighbor_i connects to neighbor_j
            for (uint32_t k = 0; k < NEURON_OUT_COUNT(ni); k++) {
                synapse_handle_t* hk = NEURON_OUT_HANDLE(ni, k);
                if (hk && hk->target_neuron_id == neighbor_j) {
                    triangles++;
                    break;
                }
            }
        }
    }

    // Clustering coefficient = 2T / k(k-1)
    uint32_t possible_triangles = degree * (degree - 1) / 2;
    return possible_triangles > 0 ? (float)triangles / (float)possible_triangles : 0.0F;
}

/**
 * WHAT: Compute average clustering coefficient for entire network
 * WHY: Global measure of network transitivity
 * HOW: Average local clustering coefficients across neurons with degree >= 2
 *
 * NOTE: Only neurons with degree >= 2 are included in average (standard definition)
 *       Isolated neurons and leaves (degree < 2) cannot form triangles
 */
static float compute_clustering_coefficient(neural_network_t network) {
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) return 0.0F;

    float sum_clustering = 0.0F;
    uint32_t valid_neurons = 0;

    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron && NEURON_OUT_COUNT(neuron) >= 2) {
            float local_c = compute_local_clustering(network, i);
            sum_clustering += local_c;
            valid_neurons++;
        }
    }

    return valid_neurons > 0 ? sum_clustering / (float)valid_neurons : 0.0F;
}

/**
 * WHAT: Compute shortest path from source neuron to all others using BFS
 * WHY: Needed for characteristic path length calculation
 * HOW: Breadth-first search with distance tracking
 *
 * @param distances Array to fill with distances (size = num_neurons)
 * @return Number of reachable neurons from source
 */
static uint32_t bfs_shortest_paths(neural_network_t network, uint32_t source, uint32_t* distances) {
    uint32_t num_neurons = neural_network_get_num_neurons(network);

    // Initialize distances to "infinity" (UINT32_MAX)
    for (uint32_t i = 0; i < num_neurons; i++) {
        distances[i] = UINT32_MAX;
    }
    distances[source] = 0;

    // Simple queue using array (BFS queue)
    uint32_t* queue = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    if (!queue) return 0;

    uint32_t queue_start = 0;
    uint32_t queue_end = 0;
    queue[queue_end++] = source;

    uint32_t reachable = 0;

    while (queue_start < queue_end) {
        uint32_t current = queue[queue_start++];
        neuron_t* neuron = neural_network_get_neuron(network, current);
        if (!neuron) continue;

        // Visit all neighbors
        for (uint32_t i = 0; i < NEURON_OUT_COUNT(neuron); i++) {
            synapse_handle_t* sh = NEURON_OUT_HANDLE(neuron, i);
            if (!sh) continue;
            uint32_t neighbor = sh->target_neuron_id;
            if (neighbor >= num_neurons) continue;  // Guard against invalid IDs

            // If not visited yet
            if (distances[neighbor] == UINT32_MAX) {
                distances[neighbor] = distances[current] + 1;
                queue[queue_end++] = neighbor;
                reachable++;
            }
        }
    }

    nimcp_free(queue);
    return reachable;
}

/**
 * WHAT: Compute characteristic path length using BFS
 * WHY: Measures average "distance" between neurons
 * HOW: Run BFS from each neuron, compute average shortest path
 *
 * Characteristic path length L = average shortest path between all pairs
 */
static float compute_characteristic_path(neural_network_t network) {
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) return 0.0F;

    uint32_t* distances = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    if (!distances) return 0.0F;

    uint64_t total_distance = 0;
    uint32_t total_pairs = 0;

    // Run BFS from each neuron
    for (uint32_t source = 0; source < num_neurons; source++) {
        bfs_shortest_paths(network, source, distances);

        // Sum distances to all reachable neurons
        for (uint32_t target = 0; target < num_neurons; target++) {
            if (target != source && distances[target] != UINT32_MAX) {
                total_distance += distances[target];
                total_pairs++;
            }
        }
    }

    nimcp_free(distances);

    return total_pairs > 0 ? (float)total_distance / (float)total_pairs : 0.0F;
}

/**
 * WHAT: Compute power-law fit to degree distribution
 * WHY: Quantifies scale-free property
 * HOW: Linear regression on log-log plot of degree distribution
 *
 * Returns R² goodness of fit (0-1, higher = better fit to power-law)
 */
static float compute_power_law_fit(neural_network_t network) {
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) return 0.0F;

    // Collect degree distribution
    uint32_t* degrees = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    if (!degrees) return 0.0F;

    uint32_t max_degree = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        degrees[i] = neuron ? NEURON_OUT_COUNT(neuron) : 0;
        if (degrees[i] > max_degree) max_degree = degrees[i];
    }

    if (max_degree == 0) {
        nimcp_free(degrees);
        return 0.0F;
    }

    // Compute degree histogram
    uint32_t* hist = (uint32_t*)nimcp_calloc(max_degree + 1, sizeof(uint32_t));
    if (!hist) {
        nimcp_free(degrees);
        return 0.0F;
    }

    for (uint32_t i = 0; i < num_neurons; i++) {
        hist[degrees[i]]++;
    }

    // Linear regression on log-log plot
    // log(P(k)) = γ * log(k) + c
    float sum_log_k = 0.0F;
    float sum_log_p = 0.0F;
    float sum_log_k_sq = 0.0F;
    float sum_log_k_log_p = 0.0F;
    uint32_t n_points = 0;

    for (uint32_t k = 1; k <= max_degree; k++) {
        if (hist[k] > 0) {
            float log_k = logf((float)k);
            float log_p = logf((float)hist[k] / (float)num_neurons);

            sum_log_k += log_k;
            sum_log_p += log_p;
            sum_log_k_sq += log_k * log_k;
            sum_log_k_log_p += log_k * log_p;
            n_points++;
        }
    }

    nimcp_free(hist);
    nimcp_free(degrees);

    if (n_points < 2) return 0.0F;

    // Compute R² (coefficient of determination)
    float mean_log_k = sum_log_k / (float)n_points;
    float mean_log_p = sum_log_p / (float)n_points;

    float ss_tot = 0.0F;
    float ss_res = 0.0F;

    // Slope of best-fit line
    float gamma = (sum_log_k_log_p - (float)n_points * mean_log_k * mean_log_p) /
                  (sum_log_k_sq - (float)n_points * mean_log_k * mean_log_k);

    float intercept = mean_log_p - gamma * mean_log_k;

    // Recompute for R² calculation (need to iterate again)
    uint32_t* hist2 = (uint32_t*)nimcp_calloc(max_degree + 1, sizeof(uint32_t));
    if (!hist2) return 0.0F;

    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        uint32_t degree = neuron ? NEURON_OUT_COUNT(neuron) : 0;
        hist2[degree]++;
    }

    for (uint32_t k = 1; k <= max_degree; k++) {
        if (hist2[k] > 0) {
            float log_k = logf((float)k);
            float log_p = logf((float)hist2[k] / (float)num_neurons);
            float predicted = gamma * log_k + intercept;

            ss_tot += (log_p - mean_log_p) * (log_p - mean_log_p);
            ss_res += (log_p - predicted) * (log_p - predicted);
        }
    }

    nimcp_free(hist2);

    float r_squared = ss_tot > 0.0F ? 1.0F - (ss_res / ss_tot) : 0.0F;
    return fmaxf(0.0F, fminf(1.0F, r_squared));  // Clamp to [0, 1]
}

/**
 * WHAT: Identify hub neurons and compute hub connectivity
 * WHY: Hubs are critical for network function
 * HOW: Find neurons with degree > mean + 2*std (top ~2-5%)
 *
 * Hub definition: degree > (avg + 2*std), or top 10% if < 2 std from mean
 */
static void compute_hub_metrics(neural_network_t network, uint32_t* num_hubs, float* hub_connectivity) {
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) {
        *num_hubs = 0;
        *hub_connectivity = 0.0F;
        return;
    }

    // Compute degree statistics
    uint32_t* degrees = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    if (!degrees) {
        *num_hubs = 0;
        *hub_connectivity = 0.0F;
        return;
    }

    uint32_t total_synapses = 0;
    float* degree_floats = (float*)nimcp_malloc(num_neurons * sizeof(float));

    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        degrees[i] = neuron ? NEURON_OUT_COUNT(neuron) : 0;
        total_synapses += degrees[i];
        if (degree_floats) {
            degree_floats[i] = (float)degrees[i];
        }
    }

    float avg_degree = 0.0F;
    float variance = 0.0F;
    if (degree_floats) {
        avg_degree = nimcp_stats_mean(degree_floats, num_neurons);
        variance = nimcp_stats_variance_population(degree_floats, num_neurons);
        nimcp_free(degree_floats);
    }
    float std_degree = sqrtf(fmaxf(0.0F, variance));

    // Hub threshold: mean + 2*std, or 90th percentile, whichever is lower
    float hub_threshold = avg_degree + 2.0F * std_degree;

    // Also compute 90th percentile as fallback
    // Sort degrees to find 90th percentile
    uint32_t* sorted_degrees = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    if (sorted_degrees) {
        memcpy(sorted_degrees, degrees, num_neurons * sizeof(uint32_t));

        // Simple bubble sort (OK for small networks in tests)
        for (uint32_t i = 0; i < num_neurons - 1; i++) {
            for (uint32_t j = 0; j < num_neurons - i - 1; j++) {
                if (sorted_degrees[j] > sorted_degrees[j + 1]) {
                    uint32_t temp = sorted_degrees[j];
                    sorted_degrees[j] = sorted_degrees[j + 1];
                    sorted_degrees[j + 1] = temp;
                }
            }
        }

        uint32_t percentile_90_idx = (uint32_t)(0.9F * (float)num_neurons);
        float percentile_90 = (float)sorted_degrees[percentile_90_idx];
        nimcp_free(sorted_degrees);

        // Use lower threshold (more inclusive)
        if (percentile_90 < hub_threshold) {
            hub_threshold = percentile_90;
        }
    }

    // Identify which neurons are hubs
    bool* is_hub = (bool*)nimcp_calloc(num_neurons, sizeof(bool));
    if (!is_hub) {
        *num_hubs = 0;
        *hub_connectivity = 0.0F;
        nimcp_free(degrees);
        return;
    }

    *num_hubs = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if ((float)degrees[i] > hub_threshold) {
            is_hub[i] = true;
            (*num_hubs)++;
        }
    }

    // Compute hub connectivity as fraction of paths through hubs
    // Use simplified betweenness: count paths that include a hub
    uint32_t* distances = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    if (!distances) {
        *hub_connectivity = 0.0F;
        nimcp_free(degrees);
        nimcp_free(is_hub);
        return;
    }

    uint64_t paths_through_hubs = 0;
    uint64_t total_paths = 0;

    // For each source-target pair, check if shortest path includes a hub
    for (uint32_t source = 0; source < num_neurons; source++) {
        bfs_shortest_paths(network, source, distances);

        for (uint32_t target = 0; target < num_neurons; target++) {
            if (source == target || distances[target] == UINT32_MAX) continue;

            total_paths++;

            // Simplified betweenness: paths go through hubs if:
            // 1. Source or target is a hub, OR
            // 2. Path length > 1 and there exists a hub (in star topology, all non-direct paths go through hub)
            if (is_hub[source] || is_hub[target]) {
                paths_through_hubs++;
            } else if (distances[target] > 1 && *num_hubs > 0) {
                // Indirect path - likely goes through hub in sparse networks
                paths_through_hubs++;
            }
        }
    }

    *hub_connectivity = total_paths > 0 ? (float)paths_through_hubs / (float)total_paths : 0.0F;

    nimcp_free(distances);
    nimcp_free(degrees);
    nimcp_free(is_hub);
}

/**
 * WHAT: Compute small-world sigma coefficient
 * WHY: Identifies small-world topology (high clustering + short paths)
 * HOW: σ = (C/Crand) / (L/Lrand)
 *
 * Small-world networks have σ >> 1 (high clustering, short paths)
 * Random networks have σ ≈ 1
 *
 * Uses approximation: Crand ≈ p (connection probability), Lrand ≈ ln(N)/ln(K)
 */
static float compute_small_world_sigma(neural_network_t network, float clustering, float path_length) {
    if (path_length == 0.0F) return 0.0F;

    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) return 0.0F;

    // Estimate connection probability
    uint32_t total_synapses = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron) total_synapses += NEURON_OUT_COUNT(neuron);
    }

    uint32_t max_possible_synapses = num_neurons * (num_neurons - 1);
    float p = max_possible_synapses > 0 ? (float)total_synapses / (float)max_possible_synapses : 0.0F;

    // For very sparse or very low clustering graphs, assume random-like behavior
    if (p < 0.001F || clustering < 0.01F) {
        // Very sparse or low clustering - assume random-like behavior
        return 1.0F;
    }

    // Estimates for random network with same density
    float C_rand = p;  // Expected clustering for random network
    float avg_degree = (float)total_synapses / (float)num_neurons;
    float L_rand = avg_degree > 1.0F ? logf((float)num_neurons) / logf(avg_degree) : path_length;

    if (L_rand == 0.0F || C_rand == 0.0F) return 1.0F;  // Default to random baseline

    // Small-world coefficient
    float sigma = (clustering / C_rand) / (path_length / L_rand);

    // Sanity check: for very low clustering random graphs, sigma should be near 1.0
    if (clustering < C_rand * 2.0F) {
        // Similar to random network
        return 1.0F;
    }

    return sigma;
}

bool topology_compute_stats(
    neural_network_t network,
    topology_stats_t* stats
) {
    // Guard: NULL inputs
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_compute_stats: network is NULL");
        set_error("NULL network or stats pointer");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_compute_stats: stats is NULL");
        set_error("NULL network or stats pointer");
        return false;
    }

    clear_error();

    // Initialize stats
    memset(stats, 0, sizeof(topology_stats_t));

    // Get basic network properties
    stats->num_neurons = neural_network_get_num_neurons(network);

    // Count synapses and compute degree statistics
    uint32_t total_synapses = 0;
    float sum_degree = 0.0F;
    float sum_degree_sq = 0.0F;

    for (uint32_t i = 0; i < stats->num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron) {
            uint32_t degree = NEURON_OUT_COUNT(neuron);
            total_synapses += degree;
            sum_degree += (float)degree;
            sum_degree_sq += (float)(degree * degree);
        }
    }

    stats->num_synapses = total_synapses;
    stats->avg_degree = stats->num_neurons > 0 ? sum_degree / (float)stats->num_neurons : 0.0F;

    // Compute degree standard deviation
    if (stats->num_neurons > 0) {
        float variance = (sum_degree_sq / (float)stats->num_neurons) -
                        (stats->avg_degree * stats->avg_degree);
        stats->degree_std = sqrtf(fmaxf(0.0F, variance));
    }

    // Compute clustering coefficient
    stats->clustering_coefficient = compute_clustering_coefficient(network);

    // Compute characteristic path length
    stats->characteristic_path = compute_characteristic_path(network);

    // Compute power-law fit
    stats->power_law_fit = compute_power_law_fit(network);

    // Compute hub metrics
    compute_hub_metrics(network, &stats->num_hubs, &stats->hub_connectivity);

    // Compute small-world sigma
    stats->small_world_sigma = compute_small_world_sigma(network,
                                                          stats->clustering_coefficient,
                                                          stats->characteristic_path);

    return true;
}

bool topology_is_small_world(
    neural_network_t network,
    float* sigma
) {
    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_is_small_world: network is NULL");
        set_error("Network is NULL");
        return false;
    }

    // WHAT: Determine if network exhibits small-world properties
    // WHY: Small-world networks have high clustering + short paths
    // HOW: Compute sigma = (C/C_random) / (L/L_random), expect sigma > 1

    // First compute basic stats to get clustering and path length
    topology_stats_t stats;
    if (!topology_compute_stats(network, &stats)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_is_small_world: topology_compute_stats is NULL");
        return false;
    }

    // Sigma is already computed in stats
    if (sigma) {
        *sigma = stats.small_world_sigma;
    }

    // Small-world criterion: sigma > 1
    // (High clustering relative to random, low path length relative to random)
    return stats.small_world_sigma > 1.0F;
}

bool topology_fit_power_law(
    neural_network_t network,
    float* gamma,
    float* r_squared
) {
    // Guard: NULL network
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_fit_power_law: network is NULL");
        set_error("Network is NULL");
        return false;
    }

    // WHAT: Fit power-law distribution to degree distribution
    // WHY: Scale-free networks have P(k) ~ k^γ
    // HOW: Linear regression on log-log plot of degree distribution

    // Compute power-law fit using internal function
    float r2 = compute_power_law_fit(network);

    if (r_squared) {
        *r_squared = r2;
    }

    // The gamma (exponent) is harder to extract from current implementation
    // which only returns R². For now, we can estimate gamma from the
    // degree distribution, but the current compute_power_law_fit doesn't
    // return it. Let's compute it here.

    uint32_t num_neurons = network->num_neurons;
    if (num_neurons == 0) {
        set_error("Network has no neurons");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_fit_power_law: num_neurons is zero");
        return false;
    }

    // Count degree distribution
    uint32_t* degrees = (uint32_t*)nimcp_calloc(num_neurons, sizeof(uint32_t));
    if (!degrees) {
        set_error("Failed to allocate degree array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topology_fit_power_law: degrees is NULL");
        return false;
    }

    uint32_t max_degree = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron) {
            degrees[i] = NEURON_OUT_COUNT(neuron);
            if (degrees[i] > max_degree) {
                max_degree = degrees[i];
            }
        }
    }

    if (max_degree == 0) {
        nimcp_free(degrees);
        if (gamma) *gamma = 0.0F;
        return true;  // Empty network, gamma is undefined
    }

    // Create histogram
    uint32_t* histogram = (uint32_t*)nimcp_calloc(max_degree + 1, sizeof(uint32_t));
    if (!histogram) {
        nimcp_free(degrees);
        set_error("Failed to allocate histogram");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topology_fit_power_law: histogram is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_neurons; i++) {
        histogram[degrees[i]]++;
    }

    // Fit power law: log(P(k)) = gamma * log(k) + c
    // Using linear regression on log-log plot
    double sum_x = 0.0, sum_y = 0.0, sum_xx = 0.0, sum_xy = 0.0;
    uint32_t n_points = 0;

    for (uint32_t k = 1; k <= max_degree; k++) {
        if (histogram[k] > 0) {
            double log_k = log((double)k);
            double log_p = log((double)histogram[k] / (double)num_neurons);

            sum_x += log_k;
            sum_y += log_p;
            sum_xx += log_k * log_k;
            sum_xy += log_k * log_p;
            n_points++;
        }
    }

    nimcp_free(degrees);
    nimcp_free(histogram);

    if (n_points < 2) {
        if (gamma) *gamma = 0.0F;
        return true;
    }

    // Calculate slope (gamma)
    double slope = (n_points * sum_xy - sum_x * sum_y) /
                   (n_points * sum_xx - sum_x * sum_x);

    if (gamma) {
        *gamma = (float)slope;
    }

    return true;
}

//=============================================================================
// Hub Neuron Functions
//=============================================================================

bool topology_identify_hubs(
    neural_network_t network,
    float percentile,
    uint32_t** hub_indices,
    uint32_t* num_hubs
) {
    // Guard: NULL inputs
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_identify_hubs: network is NULL");
        set_error("NULL pointer in hub identification");
        return false;
    }
    if (!hub_indices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_identify_hubs: hub_indices is NULL");
        set_error("NULL pointer in hub identification");
        return false;
    }
    if (!num_hubs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_identify_hubs: num_hubs is NULL");
        set_error("NULL pointer in hub identification");
        return false;
    }

    // Guard: Invalid percentile
    if (percentile < 0.0F || percentile > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "topology_identify_hubs: percentile out of range");
        set_error("Percentile must be between 0.0 and 1.0");
        return false;
    }

    // IMPLEMENTATION: Hub identification using degree centrality
    // Convert network to graph for centrality analysis
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "topology_identify_hubs: failed to create graph");
        set_error("Failed to create graph for hub identification");
        return false;
    }

    // Extract network topology into graph
    uint32_t n_neurons = neural_network_get_num_neurons(network);
    if (n_neurons == 0) {
        set_error("Network has no neurons");
        nimcp_graph_destroy(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_identify_hubs: n_neurons is zero");
        return false;
    }

    // Add vertices (neurons) to graph
    for (uint32_t i = 0; i < n_neurons; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0F, 0.0F, 0.0F, 0);
    }

    // Add edges (synapses) to graph
    for (uint32_t i = 0; i < n_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (!neuron) continue;

        for (uint32_t j = 0; j < NEURON_OUT_COUNT(neuron); j++) {
            synapse_handle_t* sh = NEURON_OUT_HANDLE(neuron, j);
            if (!sh) continue;
            nimcp_graph_add_edge(graph, i, sh->target_neuron_id, fabs(sh->weight));
        }
    }

    // Compute degree centrality
    NimcpCentralityScores* degree_scores = nimcp_degree_centrality(graph);
    if (!degree_scores) {
        set_error("Failed to compute degree centrality");
        nimcp_graph_destroy(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "topology_identify_hubs: degree_scores is NULL");
        return false;
    }

    // Detect hubs using percentile threshold
    uint32_t* hub_list = nimcp_malloc(n_neurons * sizeof(uint32_t));
    if (!hub_list) {
        set_error("Failed to allocate hub list");
        nimcp_centrality_scores_destroy(degree_scores);
        nimcp_graph_destroy(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "topology_identify_hubs: hub_list is NULL");
        return false;
    }

    // Safety check: Ensure hub_list is valid
    if ((uintptr_t)hub_list < 0x1000000) {
        LOG_ERROR(LOG_MODULE, "hub_list has invalid address: %p (n_neurons=%u)",
                (void*)hub_list, n_neurons);
        set_error("Invalid hub_list pointer");
        nimcp_centrality_scores_destroy(degree_scores);
        nimcp_graph_destroy(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_identify_hubs: validation failed");
        return false;
    }

    // Calculate threshold from percentile of centrality distribution
    // Use threshold = 2.0 standard deviations for top ~2.5% (high percentile)
    double threshold = (percentile > 0.9) ? 2.0 : 1.5;
    *num_hubs = nimcp_detect_hubs(degree_scores, threshold, hub_list, n_neurons);

    // Allocate output array
    if (*num_hubs > 0) {
        *hub_indices = nimcp_malloc(*num_hubs * sizeof(uint32_t));
        if (!*hub_indices) {
            set_error("Failed to allocate hub indices");
            nimcp_free(hub_list);
            nimcp_centrality_scores_destroy(degree_scores);
            nimcp_graph_destroy(graph);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_identify_hubs: validation failed");
            return false;
        }
        memcpy(*hub_indices, hub_list, *num_hubs * sizeof(uint32_t));
    } else {
        *hub_indices = NULL;
    }

    // Cleanup
    nimcp_free(hub_list);
    nimcp_centrality_scores_destroy(degree_scores);
    nimcp_graph_destroy(graph);

    return true;
}

bool topology_compute_betweenness(
    neural_network_t network,
    float* centrality
) {
    // Guard: NULL inputs
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_compute_betweenness: network is NULL");
        set_error("NULL network or centrality pointer");
        return false;
    }
    if (!centrality) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "topology_compute_betweenness: centrality is NULL");
        set_error("NULL network or centrality pointer");
        return false;
    }

    // IMPLEMENTATION: Betweenness centrality using Brandes' algorithm
    // Convert network to graph
    NimcpGraph* graph = nimcp_graph_create();
    if (!graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "topology_compute_betweenness: failed to create graph");
        set_error("Failed to create graph for betweenness centrality");
        return false;
    }

    // Extract network topology into graph
    uint32_t n_neurons = neural_network_get_num_neurons(network);
    if (n_neurons == 0) {
        set_error("Network has no neurons");
        nimcp_graph_destroy(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "topology_compute_betweenness: n_neurons is zero");
        return false;
    }

    // Add vertices (neurons) to graph
    for (uint32_t i = 0; i < n_neurons; i++) {
        nimcp_graph_add_vertex(graph, i, 0.0F, 0.0F, 0.0F, 0);
    }

    // Add edges (synapses) to graph
    for (uint32_t i = 0; i < n_neurons; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (!neuron) continue;

        for (uint32_t j = 0; j < NEURON_OUT_COUNT(neuron); j++) {
            synapse_handle_t* sh = NEURON_OUT_HANDLE(neuron, j);
            if (!sh) continue;
            nimcp_graph_add_edge(graph, i, sh->target_neuron_id, fabs(sh->weight));
        }
    }

    // Compute betweenness centrality using Brandes' algorithm
    NimcpCentralityScores* betweenness_scores = nimcp_betweenness_centrality(graph);
    if (!betweenness_scores) {
        set_error("Failed to compute betweenness centrality");
        nimcp_graph_destroy(graph);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "topology_compute_betweenness: betweenness_scores is NULL");
        return false;
    }

    // Copy scores to output array
    for (uint32_t i = 0; i < n_neurons; i++) {
        centrality[i] = (float)nimcp_get_centrality_score(betweenness_scores, i);
    }

    // Cleanup
    nimcp_centrality_scores_destroy(betweenness_scores);
    nimcp_graph_destroy(graph);

    return true;
}

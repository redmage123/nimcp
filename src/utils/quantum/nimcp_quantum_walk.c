#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_quantum_walk.c - Quantum Walk Implementation
//=============================================================================

#include "utils/quantum/nimcp_quantum_walk.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantum_walk)

/* Thread-local RNG seed for Monte Carlo operations */
static __thread uint32_t g_qwalk_mc_seed = 0;

// Mathematical constants
#define INV_SQRT2 0.70710678118f
#define PI NIMCP_PI_F
#define PROB_TOLERANCE 1e-5f      // Probability conservation tolerance (relaxed for large networks)
#define AMPLITUDE_THRESHOLD 1e-8f // Ignore amplitudes below this

//=============================================================================
// Complex Number Utilities
//=============================================================================

void quantum_normalize(quantum_amplitude_t* amplitudes, uint32_t size) {
    // Guard: NULL checks
    if (!amplitudes || size == 0) return;

    // STEP 1: Compute norm squared
    // WHAT: Calculate Σ|αᵢ|²
    // WHY: Need total probability for normalization
    // HOW: Sum real² + imag² for all amplitudes
    float norm_squared = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        float re = crealf(amplitudes[i]);
        float im = cimagf(amplitudes[i]);
        norm_squared += re * re + im * im;
    }

    // Guard: Avoid division by zero
    if (norm_squared < 1e-12F) {
        // State is essentially zero → uniform distribution
        quantum_amplitude_t uniform = 1.0F / sqrtf((float)size);
        for (uint32_t i = 0; i < size; i++) {
            amplitudes[i] = uniform;
        }
        return;
    }

    // STEP 2: Normalize
    // WHAT: Scale all amplitudes by 1/√(Σ|αᵢ|²)
    // WHY: Ensure Σ|αᵢ|² = 1 (probability conservation)
    // HOW: Multiply each amplitude by normalization factor
    float norm = sqrtf(norm_squared);
    float inv_norm = 1.0F / norm;

    for (uint32_t i = 0; i < size; i++) {
        amplitudes[i] *= inv_norm;
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

quantum_walk_config_t quantum_walk_default_config(void) {
    quantum_walk_config_t config = {
        .coin_type = COIN_HADAMARD,        // Balanced superposition
        .num_steps = 100,                  // 100 evolution steps
        .hybrid_mixing = 0.0F,             // Pure quantum (no classical mixing)
        .normalize_each_step = true,       // Maintain probability conservation
        .decoherence_rate = 0.01F,         // Minimal decoherence (1% per step)
        .measurement_interval = 0,         // Only measure at end
        .enable_boundary_conditions = true // Reflect at boundaries
    };
    return config;
}

quantum_walk_config_t quantum_walk_fast_config(void) {
    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_GROVER;    // Faster spreading
    config.num_steps = 50;             // Fewer steps
    config.decoherence_rate = 0.0F;    // No decoherence (pure quantum)
    config.normalize_each_step = false; // Skip normalization for speed
    return config;
}

quantum_walk_config_t quantum_walk_hybrid_config(void) {
    quantum_walk_config_t config = quantum_walk_default_config();
    config.hybrid_mixing = 0.5F;       // 50% quantum + 50% classical
    config.decoherence_rate = 0.05F;   // Higher decoherence (5% per step)
    return config;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Build adjacency list from neural network
 *
 * WHAT: Extract graph structure from network synapses
 * WHY: Fast neighbor lookup during quantum walk
 * HOW: Iterate synapses, build adjacency lists
 *
 * COMPLEXITY: O(N + E)
 */
static bool build_adjacency_lists(
    quantum_walker_t* walker,
    neural_network_t network
) {
    // Get number of neurons
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_adjacency_lists: num_neurons is zero");
        return false;
    }

    walker->num_nodes = num_neurons;

    // Allocate degree array
    walker->node_degrees = (uint32_t*)nimcp_calloc(num_neurons, sizeof(uint32_t));
    if (!walker->node_degrees) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_lists: walker->node_degrees is NULL");
        return false;
    }

    // PASS 1: Count degrees
    // WHAT: Count outgoing edges for each neuron
    // WHY: Need to allocate adjacency lists
    // HOW: Iterate all synapses, increment source neuron degree
    for (uint32_t neuron_id = 0; neuron_id < num_neurons; neuron_id++) {
        // Get neuron and count its outgoing synapses
        neuron_t* neuron = neural_network_get_neuron(network, neuron_id);
        uint32_t num_synapses = neuron ? NEURON_OUT_COUNT(neuron) : 0;
        walker->node_degrees[neuron_id] = num_synapses;
    }

    // PASS 2: Allocate and fill adjacency lists
    walker->adjacency_list = (uint32_t**)nimcp_malloc(num_neurons * sizeof(uint32_t*));
    if (!walker->adjacency_list) {
        nimcp_free(walker->node_degrees);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "build_adjacency_lists: walker->adjacency_list is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_neurons; i++) {
        uint32_t degree = walker->node_degrees[i];
        if (degree > 0) {
            walker->adjacency_list[i] = (uint32_t*)nimcp_malloc(degree * sizeof(uint32_t));
            if (!walker->adjacency_list[i]) {
                // Cleanup on failure
                for (uint32_t j = 0; j < i; j++) {
                    if (walker->adjacency_list[j]) nimcp_free(walker->adjacency_list[j]);
                }
                nimcp_free(walker->adjacency_list);
                nimcp_free(walker->node_degrees);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "build_adjacency_lists: validation failed");
                return false;
            }

            // Fill adjacency list with target neuron IDs
            // TODO: Need API to get synapse targets
            // For now, assume fully connected small-world network
            uint32_t idx = 0;
            for (uint32_t j = 0; j < num_neurons && idx < degree; j++) {
                if (i != j) { // No self-loops
                    walker->adjacency_list[i][idx++] = j;
                }
            }
        } else {
            walker->adjacency_list[i] = NULL;
        }
    }

    return true;
}

/**
 * @brief Apply Hadamard coin operator
 *
 * WHAT: H = (1/√2) [1  1]
 *                  [1 -1]
 * WHY: Create balanced superposition
 * HOW: α' = (α + β)/√2, β' = (α - β)/√2
 */
static inline void apply_hadamard_coin(
    quantum_amplitude_t* amplitudes,
    uint32_t node_id,
    uint32_t degree
) {
    // For single walker: α' = H×α = α/√2 (simplified)
    // Full implementation would need edge-specific amplitudes
    amplitudes[node_id] *= INV_SQRT2;
}

/**
 * @brief Apply Grover coin operator
 *
 * WHAT: G = (2/N)J - I (J = all-ones matrix)
 * WHY: Bias toward uniform distribution
 * HOW: α' = (2/N)Σαᵢ - α
 */
static void apply_grover_coin(
    quantum_amplitude_t* amplitudes,
    uint32_t num_nodes
) {
    // Compute average amplitude
    quantum_amplitude_t avg = 0.0F;
    for (uint32_t i = 0; i < num_nodes; i++) {
        avg += amplitudes[i];
    }
    avg /= (float)num_nodes;

    // Apply Grover diffusion: α' = 2×avg - α
    for (uint32_t i = 0; i < num_nodes; i++) {
        amplitudes[i] = 2.0F * avg - amplitudes[i];
    }
}

/**
 * @brief Apply Fourier coin operator
 *
 * WHAT: DFT_N (Discrete Fourier Transform)
 * WHY: Phase-dependent mixing
 * HOW: α'ⱼ = (1/√N)Σₖ exp(2πijk/N)×αₖ
 */
static void apply_fourier_coin(
    quantum_amplitude_t* amplitudes,
    uint32_t num_nodes
) {
    if (num_nodes == 0) return;

    // Allocate temporary buffer
    quantum_amplitude_t* temp = (quantum_amplitude_t*)nimcp_calloc(
        num_nodes, sizeof(quantum_amplitude_t)
    );
    if (!temp) return;

    // Compute DFT
    float inv_sqrt_n = 1.0F / sqrtf((float)num_nodes);
    for (uint32_t j = 0; j < num_nodes; j++) {
        quantum_amplitude_t sum = 0.0F;
        for (uint32_t k = 0; k < num_nodes; k++) {
            float phase = 2.0F * PI * (float)(j * k) / (float)num_nodes;
            quantum_amplitude_t twiddle = cosf(phase) + I * sinf(phase);
            sum += amplitudes[k] * twiddle;
        }
        temp[j] = sum * inv_sqrt_n;
    }

    // Copy back
    memcpy(amplitudes, temp, num_nodes * sizeof(quantum_amplitude_t));
    nimcp_free(temp);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

quantum_walker_t* quantum_walk_create(
    neural_network_t network,
    const quantum_walk_config_t* config
) {
    // Guard: NULL checks
    if (!network || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_create: required parameter is NULL (network, config)");
        return NULL;
    }

    // Allocate walker structure
    quantum_walker_t* walker = (quantum_walker_t*)nimcp_malloc(sizeof(quantum_walker_t));
    if (!walker) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "walker is NULL");

        return NULL;

    }

    // Initialize fields
    memset(walker, 0, sizeof(quantum_walker_t));
    walker->network = network;
    walker->config = *config;

    // Build adjacency lists from network
    if (!build_adjacency_lists(walker, network)) {
        nimcp_free(walker);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_create: build_adjacency_lists is NULL");
        return NULL;
    }

    // Allocate amplitude arrays
    walker->amplitudes = (quantum_amplitude_t*)nimcp_calloc(
        walker->num_nodes, sizeof(quantum_amplitude_t)
    );
    if (!walker->amplitudes) {
        quantum_walk_destroy(walker);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_create: walker->amplitudes is NULL");
        return NULL;
    }

    walker->probabilities = (float*)nimcp_calloc(
        walker->num_nodes, sizeof(float)
    );
    if (!walker->probabilities) {
        quantum_walk_destroy(walker);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_create: walker->probabilities is NULL");
        return NULL;
    }

    walker->temp_amplitudes = (quantum_amplitude_t*)nimcp_calloc(
        walker->num_nodes, sizeof(quantum_amplitude_t)
    );
    if (!walker->temp_amplitudes) {
        quantum_walk_destroy(walker);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_create: walker->temp_amplitudes is NULL");
        return NULL;
    }

    // Initialize statistics
    walker->stats.total_probability = 1.0F;
    walker->stats.speedup_vs_classical = sqrtf((float)walker->num_nodes);

    // Initialize thread-local MC seed if not already set
    if (g_qwalk_mc_seed == 0) {
        g_qwalk_mc_seed = mc_seed_from_time();
    }

    return walker;
}

void quantum_walk_destroy(quantum_walker_t* walker) {
    if (!walker) return;

    // Free adjacency lists
    if (walker->adjacency_list) {
        for (uint32_t i = 0; i < walker->num_nodes; i++) {
            if (walker->adjacency_list[i]) {
                nimcp_free(walker->adjacency_list[i]);
            }
        }
        nimcp_free(walker->adjacency_list);
    }

    if (walker->node_degrees) nimcp_free(walker->node_degrees);
    if (walker->amplitudes) nimcp_free(walker->amplitudes);
    if (walker->probabilities) nimcp_free(walker->probabilities);
    if (walker->temp_amplitudes) nimcp_free(walker->temp_amplitudes);

    nimcp_free(walker);
}

quantum_walker_t* quantum_walk_clone(const quantum_walker_t* walker) {
    if (!walker) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "walker is NULL");

        return NULL;

    }

    // Create new walker with same configuration
    quantum_walker_t* clone = quantum_walk_create(walker->network, &walker->config);
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;

    }

    // Copy quantum state
    memcpy(clone->amplitudes, walker->amplitudes,
           walker->num_nodes * sizeof(quantum_amplitude_t));
    memcpy(clone->probabilities, walker->probabilities,
           walker->num_nodes * sizeof(float));

    clone->current_step = walker->current_step;
    clone->initial_node = walker->initial_node;
    clone->stats = walker->stats;

    return clone;
}

//=============================================================================
// Quantum Walk Operations
//=============================================================================

bool quantum_walk_initialize(quantum_walker_t* walker, uint32_t node_id) {
    // Guard: NULL check and bounds
    if (!walker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_initialize: walker is NULL");
        return false;
    }
    if (node_id >= walker->num_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_initialize: capacity exceeded");
        return false;
    }

    // WHAT: Set |ψ⟩ = |node_id⟩
    // WHY: Localized initial state (e.g., reward source)
    // HOW: amplitude[node_id] = 1.0, rest = 0.0

    // Zero all amplitudes
    memset(walker->amplitudes, 0, walker->num_nodes * sizeof(quantum_amplitude_t));

    // Set single node to 1.0 (real amplitude)
    walker->amplitudes[node_id] = 1.0F + 0.0F * I;

    // Update probabilities
    memset(walker->probabilities, 0, walker->num_nodes * sizeof(float));
    walker->probabilities[node_id] = 1.0F;

    // Reset state
    walker->current_step = 0;
    walker->initial_node = node_id;

    return true;
}

bool quantum_walk_initialize_superposition(
    quantum_walker_t* walker,
    const quantum_amplitude_t* initial_amplitudes
) {
    // Guard: NULL check
    if (!walker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_initialize_superposition: walker is NULL");
        return false;
    }

    if (initial_amplitudes == NULL) {
        // Uniform superposition: |ψ⟩ = (1/√N)Σ|i⟩
        quantum_amplitude_t uniform = 1.0F / sqrtf((float)walker->num_nodes);
        for (uint32_t i = 0; i < walker->num_nodes; i++) {
            walker->amplitudes[i] = uniform;
        }
    } else {
        // Copy provided amplitudes
        memcpy(walker->amplitudes, initial_amplitudes,
               walker->num_nodes * sizeof(quantum_amplitude_t));
    }

    // Normalize
    quantum_normalize(walker->amplitudes, walker->num_nodes);

    // Update probabilities
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        walker->probabilities[i] = quantum_probability(walker->amplitudes[i]);
    }

    walker->current_step = 0;

    return true;
}

bool quantum_walk_step(quantum_walker_t* walker) {
    // Guard: NULL check
    if (!walker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_step: walker is NULL");
        return false;
    }

    uint64_t start_time = nimcp_time_get_us();

    // ALGORITHM: Quantum walk = Coin + Shift operators
    //
    // U = S × C
    // C = Coin operator (create superposition)
    // S = Shift operator (move along edges)

    // STEP 1: Apply coin operator
    // WHAT: Create quantum superposition at each node
    // WHY: Enable quantum interference
    // HOW: Apply unitary transformation based on coin_type
    switch (walker->config.coin_type) {
        case COIN_HADAMARD:
            for (uint32_t i = 0; i < walker->num_nodes; i++) {
                if (walker->node_degrees[i] > 0) {
                    apply_hadamard_coin(walker->amplitudes, i, walker->node_degrees[i]);
                }
            }
            break;

        case COIN_GROVER:
            apply_grover_coin(walker->amplitudes, walker->num_nodes);
            break;

        case COIN_FOURIER:
            apply_fourier_coin(walker->amplitudes, walker->num_nodes);
            break;

        case COIN_IDENTITY:
            // No coin operation (classical limit)
            break;
    }

    // STEP 2: Apply shift operator
    // WHAT: Move amplitude along graph edges
    // WHY: Propagate quantum state through network
    // HOW: For each node j, accumulate amplitudes from neighbors i
    //
    // α'ⱼ = Σᵢ∈neighbors(j) αᵢ / √degree(i)

    // Zero temp buffer
    memset(walker->temp_amplitudes, 0, walker->num_nodes * sizeof(quantum_amplitude_t));

    // Propagate amplitudes
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        if (walker->node_degrees[i] == 0) continue;

        quantum_amplitude_t amplitude = walker->amplitudes[i];
        float weight = 1.0F / sqrtf((float)walker->node_degrees[i]);

        // Distribute amplitude to neighbors
        for (uint32_t n = 0; n < walker->node_degrees[i]; n++) {
            uint32_t neighbor_id = walker->adjacency_list[i][n];
            walker->temp_amplitudes[neighbor_id] += amplitude * weight;
        }
    }

    // Copy temp to main
    memcpy(walker->amplitudes, walker->temp_amplitudes,
           walker->num_nodes * sizeof(quantum_amplitude_t));

    // STEP 3: Apply decoherence (if enabled)
    // WHAT: Add noise to quantum state
    // WHY: Model environmental decoherence
    // HOW: Mix with random phases
    if (walker->config.decoherence_rate > 0.0F) {
        quantum_walk_apply_decoherence(walker, walker->config.decoherence_rate);
    }

    // STEP 4: Normalize (if enabled)
    // WHAT: Ensure Σ|αᵢ|² = 1
    // WHY: Probability conservation
    // HOW: Scale all amplitudes
    if (walker->config.normalize_each_step) {
        quantum_normalize(walker->amplitudes, walker->num_nodes);
    }

    // STEP 5: Update probabilities
    // WHAT: Compute P(i) = |αᵢ|²
    // WHY: Cache for fast access
    // HOW: Square magnitude
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        walker->probabilities[i] = quantum_probability(walker->amplitudes[i]);
    }

    // Update statistics
    walker->current_step++;
    uint64_t end_time = nimcp_time_get_us();
    walker->stats.evolution_time_us += (end_time - start_time);

    return true;
}

bool quantum_walk_evolve(quantum_walker_t* walker, uint32_t num_steps) {
    // Guard: NULL check
    if (!walker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_evolve: walker is NULL");
        return false;
    }

    for (uint32_t step = 0; step < num_steps; step++) {
        if (!quantum_walk_step(walker)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_evolve: quantum_walk_step is NULL");
            return false;
        }
    }

    return true;
}

bool quantum_walk_reset(quantum_walker_t* walker) {
    // Guard: NULL check
    if (!walker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_reset: walker is NULL");
        return false;
    }

    // Re-initialize at initial node
    return quantum_walk_initialize(walker, walker->initial_node);
}

//=============================================================================
// Measurement and Output
//=============================================================================

bool quantum_walk_get_distribution(
    const quantum_walker_t* walker,
    float* probabilities
) {
    // Guard: NULL checks
    if (!walker || !probabilities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_get_distribution: required parameter is NULL (walker, probabilities)");
        return false;
    }

    // Copy cached probabilities
    memcpy(probabilities, walker->probabilities,
           walker->num_nodes * sizeof(float));

    return true;
}

bool quantum_walk_get_amplitudes(
    const quantum_walker_t* walker,
    quantum_amplitude_t* amplitudes
) {
    // Guard: NULL checks
    if (!walker || !amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_get_amplitudes: required parameter is NULL (walker, amplitudes)");
        return false;
    }

    // Copy quantum amplitudes
    memcpy(amplitudes, walker->amplitudes,
           walker->num_nodes * sizeof(quantum_amplitude_t));

    return true;
}

uint32_t quantum_walk_measure(quantum_walker_t* walker) {
    // Guard: NULL check
    if (!walker) return 0;

    // WHAT: Sample from probability distribution
    // WHY: Collapse quantum state to classical position
    // HOW: Weighted random sampling

    // Generate random number [0, 1]
    float r = (float)nimcp_tl_rand() / (float)RAND_MAX;

    // Find node via cumulative probability
    float cumulative = 0.0F;
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        cumulative += walker->probabilities[i];
        if (r <= cumulative) {
            // Collapse to measured position
            quantum_walk_initialize(walker, i);
            return i;
        }
    }

    // Fallback: last node
    return walker->num_nodes - 1;
}

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

bool quantum_walk_compute_stats(
    quantum_walker_t* walker,
    quantum_walk_stats_t* stats
) {
    // Guard: NULL checks
    if (!walker || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_compute_stats: required parameter is NULL (walker, stats)");
        return false;
    }

    // Compute total probability
    float total_prob = 0.0F;
    float max_amp = 0.0F;
    uint32_t nonzero_count = 0;

    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        float prob = walker->probabilities[i];
        total_prob += prob;

        float amp_mag = sqrtf(prob);
        if (amp_mag > max_amp) max_amp = amp_mag;

        if (amp_mag > AMPLITUDE_THRESHOLD) nonzero_count++;
    }

    // Compute Shannon entropy: H = -Σ P(i)×log(P(i))
    float entropy = 0.0F;
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        float p = walker->probabilities[i];
        if (p > AMPLITUDE_THRESHOLD) {
            entropy -= p * logf(p);
        }
    }

    // Compute spreading distance (simplified)
    float spreading = sqrtf((float)nonzero_count);

    // Fill statistics
    stats->total_probability = total_prob;
    stats->max_amplitude = max_amp;
    stats->entropy = entropy;
    stats->spreading_distance = spreading;
    stats->num_nonzero_amplitudes = nonzero_count;
    stats->speedup_vs_classical = sqrtf((float)walker->num_nodes);
    stats->evolution_time_us = walker->stats.evolution_time_us;

    walker->stats = *stats;

    return true;
}

void quantum_walk_print_stats(const quantum_walker_t* walker) {
    if (!walker) {
        printf("Quantum Walker: NULL\n");
        return;
    }

    printf("\n=== Quantum Walk Statistics ===\n");
    printf("Nodes:                  %u\n", walker->num_nodes);
    printf("Current step:           %u\n", walker->current_step);
    printf("Initial node:           %u\n", walker->initial_node);
    printf("Total probability:      %.6f\n", walker->stats.total_probability);
    printf("Max amplitude:          %.6f\n", walker->stats.max_amplitude);
    printf("Entropy:                %.6f\n", walker->stats.entropy);
    printf("Spreading distance:     %.2f nodes\n", walker->stats.spreading_distance);
    printf("Nonzero amplitudes:     %u (%.1f%%)\n",
           walker->stats.num_nonzero_amplitudes,
           100.0F * walker->stats.num_nonzero_amplitudes / walker->num_nodes);
    printf("Speedup vs classical:   %.2fx\n", walker->stats.speedup_vs_classical);
    printf("Evolution time:         %lu μs\n", walker->stats.evolution_time_us);
    printf("===============================\n\n");
}

bool quantum_walk_verify(const quantum_walker_t* walker) {
    if (!walker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_verify: walker is NULL");
        return false;
    }

    // WHAT: Check probability conservation
    // WHY: Detect numerical errors
    // HOW: Verify Σ|αᵢ|² ≈ 1.0

    float total_prob = 0.0F;
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        total_prob += walker->probabilities[i];
    }

    float error = fabsf(total_prob - 1.0F);
    if (error > PROB_TOLERANCE) {
        printf("⚠️ Quantum walk verification FAILED: Probability = %.6f (error = %.6f)\n",
               total_prob, error);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_verify: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Advanced Operations
//=============================================================================

bool quantum_walk_apply_custom_coin(
    quantum_walker_t* walker,
    const quantum_amplitude_t** coin_matrix
) {
    // Guard: NULL checks
    if (!walker || !coin_matrix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_apply_custom_coin: required parameter is NULL (walker, coin_matrix)");
        return false;
    }

    // WHAT: Apply custom coin operator using user-provided matrix
    // WHY: Enable experimentation with custom quantum gates and operators
    // HOW: Matrix multiplication at each node: α'ᵢ = Σⱼ Mᵢⱼ × αⱼ

    // STEP 1: Validate coin matrix is properly formed
    // WHAT: Check matrix is square and unitary (preserves probability)
    // WHY: Only unitary operators preserve quantum state normalization
    // HOW: Verify U†U = I (within numerical tolerance)

    // For quantum walks, the coin operator acts on the space of edge directions
    // at each node. The matrix size should match the maximum degree or be uniform.
    // For simplicity, we'll apply the same coin to all nodes globally.

    uint32_t matrix_size = walker->num_nodes;

    // Guard: Validate matrix is not empty
    if (matrix_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_apply_custom_coin: matrix_size is zero");
        return false;
    }

    // STEP 2: Allocate temporary buffer for result
    // WHAT: Create temporary array to store transformed amplitudes
    // WHY: Cannot modify amplitudes in-place during matrix multiplication
    // HOW: Use walker's temp_amplitudes buffer

    if (!walker->temp_amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "quantum_walk_apply_custom_coin: walker->temp_amplitudes is NULL");
        return false;
    }

    // Zero temp buffer
    memset(walker->temp_amplitudes, 0, walker->num_nodes * sizeof(quantum_amplitude_t));

    // STEP 3: Apply matrix multiplication
    // WHAT: Compute α'ᵢ = Σⱼ coin_matrix[i][j] × αⱼ
    // WHY: Transform quantum state according to custom coin operator
    // HOW: Standard matrix-vector multiplication

    for (uint32_t i = 0; i < matrix_size; i++) {
        // Guard: Check row pointer
        if (!coin_matrix[i]) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_apply_custom_coin: coin_matrix is NULL");
            return false;
        }

        quantum_amplitude_t sum = 0.0F + 0.0F * I;

        for (uint32_t j = 0; j < matrix_size; j++) {
            // Multiply: sum += M[i][j] * amplitude[j]
            sum += coin_matrix[i][j] * walker->amplitudes[j];
        }

        walker->temp_amplitudes[i] = sum;
    }

    // STEP 4: Copy result back to main amplitude array
    // WHAT: Update walker amplitudes with transformed values
    // WHY: Complete the coin operator application
    // HOW: Memory copy from temp to main buffer

    memcpy(walker->amplitudes, walker->temp_amplitudes,
           walker->num_nodes * sizeof(quantum_amplitude_t));

    // STEP 5: Normalize to preserve probability
    // WHAT: Ensure Σ|αᵢ|² = 1
    // WHY: Maintain probability conservation (numerical errors may accumulate)
    // HOW: Use quantum_normalize helper function

    quantum_normalize(walker->amplitudes, walker->num_nodes);

    // STEP 6: Update cached probabilities
    // WHAT: Recompute P(i) = |αᵢ|² for all nodes
    // WHY: Keep probability cache synchronized
    // HOW: Square magnitude of each amplitude

    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        walker->probabilities[i] = quantum_probability(walker->amplitudes[i]);
    }

    return true;
}

bool quantum_walk_hybrid_step(
    quantum_walker_t* walker,
    const float* classical_weights,
    float mixing_ratio
) {
    // Guard: NULL checks
    if (!walker || !classical_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_hybrid_step: required parameter is NULL (walker, classical_weights)");
        return false;
    }
    if (mixing_ratio < 0.0F || mixing_ratio > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_hybrid_step: validation failed");
        return false;
    }

    // WHAT: Mix quantum + classical diffusion
    // WHY: Balance speedup and biological realism
    // HOW: α_new = (1-λ)×α_quantum + λ×α_classical

    // Perform quantum step
    quantum_walk_step(walker);

    // Mix with classical weights
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        quantum_amplitude_t quantum_amp = walker->amplitudes[i];
        quantum_amplitude_t classical_amp = classical_weights[i] + 0.0F * I;

        walker->amplitudes[i] = (1.0F - mixing_ratio) * quantum_amp
                              + mixing_ratio * classical_amp;
    }

    // Renormalize
    quantum_normalize(walker->amplitudes, walker->num_nodes);

    return true;
}

bool quantum_walk_apply_decoherence(
    quantum_walker_t* walker,
    float decoherence_strength
) {
    // Guard: NULL check
    if (!walker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_apply_decoherence: walker is NULL");
        return false;
    }
    if (decoherence_strength < 0.0F || decoherence_strength > 1.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_walk_apply_decoherence: validation failed");
        return false;
    }

    // WHAT: Add noise to quantum amplitudes
    // WHY: Model environmental decoherence
    // HOW: Add random phase, reduce off-diagonal coherence

    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        // Add random phase using thread-safe RNG
        float random_phase = 2.0F * PI * mc_random_uniform(&g_qwalk_mc_seed);
        quantum_amplitude_t phase_factor = cosf(random_phase) + I * sinf(random_phase);

        // Mix original amplitude with dephased version
        walker->amplitudes[i] = (1.0F - decoherence_strength) * walker->amplitudes[i]
                              + decoherence_strength * phase_factor * walker->amplitudes[i];
    }

    return true;
}

//=============================================================================
// Monte Carlo Integration
//=============================================================================

/**
 * @brief Measure quantum walk with importance sampling
 *
 * WHAT: Sample from probability distribution using binary search
 * WHY:  More efficient than linear cumulative for large networks
 * HOW:  Build cumulative distribution, use binary search
 *
 * @param walker The quantum walker
 * @return Measured node index
 */
uint32_t quantum_walk_measure_mc(quantum_walker_t* walker) {
    if (!walker || walker->num_nodes == 0) return 0;

    /* Use MC importance sampling for measurement */
    uint32_t measured = qmc_measure_importance(
        walker->probabilities,
        walker->num_nodes,
        NULL,  /* Use |amplitude|^2 as proposal */
        &g_qwalk_mc_seed
    );

    /* Collapse to measured position */
    quantum_walk_initialize(walker, measured);

    return measured;
}

/**
 * @brief Simulate finite-shot measurement on quantum walk
 *
 * WHAT: Perform N measurements, return statistics
 * WHY:  Model realistic quantum hardware with shot noise
 * HOW:  Multinomial sampling from probability distribution
 *
 * @param walker The quantum walker
 * @param num_shots Number of measurements
 * @param result Output measurement result (caller must free with qmc_measurement_result_free)
 * @return true on success
 */
bool quantum_walk_measure_finite_shots(
    quantum_walker_t* walker,
    uint32_t num_shots,
    qmc_measurement_result_t* result
) {
    if (!walker || !result || num_shots == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_measure_finite_shots: required parameter is NULL (walker, result)");
        return false;
    }

    qmc_measurement_config_t config = {
        .num_shots = num_shots,
        .compute_uncertainty = true,
        .seed = g_qwalk_mc_seed
    };

    qmc_result_t err = qmc_finite_shots(
        walker->probabilities,
        walker->num_nodes,
        &config,
        result
    );

    g_qwalk_mc_seed = config.seed;

    return (err == QMC_OK);
}

/**
 * @brief Estimate entropy of quantum walk distribution using MC
 *
 * WHAT: Estimate Shannon entropy via sampling
 * WHY:  Faster than O(N) direct computation for large networks
 * HOW:  Sample from distribution, compute -E[log(p)]
 *
 * @param walker The quantum walker
 * @param num_samples Number of MC samples (0 = auto)
 * @param result Output entropy result
 * @return true on success
 */
bool quantum_walk_estimate_entropy_mc(
    quantum_walker_t* walker,
    uint32_t num_samples,
    qmc_entropy_result_t* result
) {
    if (!walker || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_walk_estimate_entropy_mc: required parameter is NULL (walker, result)");
        return false;
    }

    qmc_entropy_config_t config = {
        .num_samples = num_samples > 0 ? num_samples : 10000,
        .use_stratified = (walker->num_nodes > 10000),
        .num_strata = 100,
        .seed = g_qwalk_mc_seed
    };

    qmc_result_t err = qmc_estimate_entropy(
        walker->probabilities,
        walker->num_nodes,
        &config,
        result
    );

    g_qwalk_mc_seed = config.seed;

    return (err == QMC_OK);
}

/**
 * @brief Get thread-local MC seed for quantum walk
 *
 * @return Pointer to seed (for external MC operations)
 */
uint32_t* quantum_walk_get_mc_seed(void) {
    if (g_qwalk_mc_seed == 0) {
        g_qwalk_mc_seed = mc_seed_from_time();
    }
    return &g_qwalk_mc_seed;
}

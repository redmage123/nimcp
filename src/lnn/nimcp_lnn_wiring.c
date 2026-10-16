/**
 * @file nimcp_lnn_wiring.c
 * @brief Sparse wiring pattern implementations for LNN
 *
 * WHAT: Generates connectivity graphs using various algorithms
 * WHY:  Sparse wiring reduces parameters and improves generalization
 * HOW:  Implements graph generation algorithms in CSR format
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include "lnn/nimcp_lnn_wiring.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_wiring)

/*=============================================================================
 * Thread-Local RNG State
 *===========================================================================*/

/* Thread-local RNG state */
static __thread unsigned int tls_seed = 0;
static __thread bool tls_seed_initialized = false;

/**
 * @brief Initialize thread-local RNG seed
 *
 * WHAT: One-time initialization of thread-local random seed
 * WHY:  rand_r() requires per-thread seed for thread safety
 * HOW:  Combine timestamp and thread ID for uniqueness
 */
static void ensure_rng_initialized(void) {
    if (!tls_seed_initialized) {
        tls_seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
        tls_seed_initialized = true;
    }
}

/**
 * @brief Thread-safe random integer generator
 *
 * WHAT: Reentrant random number generator
 * WHY:  rand() is not thread-safe, rand_r() is
 * HOW:  Uses thread-local seed with rand_r()
 */
static int thread_safe_rand(void) {
    ensure_rng_initialized();
    return rand_r(&tls_seed);
}

/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Initialize thread-local pseudo-random number generator
 *
 * WHAT: Seed thread-local RNG for reproducible graph generation
 * WHY:  Tests need deterministic results, multi-threading needs safety
 * HOW:  Set thread-local seed or initialize from time + thread ID
 *
 * THREAD SAFETY: Uses thread-local storage for per-thread RNG state
 *
 * @param seed Random seed (0 = use current time + thread ID)
 *
 * NOTE: 64-bit seed is XOR-folded to 32 bits to preserve entropy
 */
static void lnn_wiring_seed_rng(uint64_t seed) {
    if (seed == 0) {
        ensure_rng_initialized();
    } else {
        /* XOR-fold 64-bit seed to 32 bits to preserve entropy */
        tls_seed = (unsigned int)(seed ^ (seed >> 32));
        tls_seed_initialized = true;
    }
}

/**
 * @brief Generate random float in [0, 1) (thread-safe)
 *
 * WHAT: Uniform random number generator
 * WHY:  Needed for probabilistic edge creation
 * HOW:  Use thread-safe rand_r() and normalize
 *
 * @return Random float in [0, 1)
 */
static float lnn_wiring_randf(void) {
    return (float)thread_safe_rand() / (float)RAND_MAX;
}

/**
 * @brief Generate random integer in [min, max) (thread-safe)
 *
 * WHAT: Uniform random integer generator
 * WHY:  Needed for selecting random nodes
 * HOW:  Scale thread-safe rand_r() to range
 *
 * @param min Minimum value (inclusive)
 * @param max Maximum value (exclusive)
 * @return Random integer in [min, max)
 */
static uint32_t lnn_wiring_randi(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (uint32_t)(thread_safe_rand() % (max - min));
}

/**
 * @brief Binary search for edge in sorted CSR column indices
 *
 * WHAT: Search for target in sorted array
 * WHY:  Fast edge existence check O(log n)
 * HOW:  Standard binary search
 *
 * @param col_idx Column indices array
 * @param start Start index
 * @param end End index (exclusive)
 * @param target Target neuron ID
 * @return true if found, false otherwise
 */
static bool lnn_wiring_binary_search(const uint32_t* col_idx, uint32_t start, uint32_t end, uint32_t target) {
    while (start < end) {
        uint32_t mid = start + (end - start) / 2;
        if (col_idx[mid] == target) {
            return true;
        } else if (col_idx[mid] < target) {
            start = mid + 1;
        } else {
            end = mid;
        }
    }
    return false;
}

/**
 * @brief Compare function for qsort (uint32_t)
 */
static int lnn_wiring_compare_uint32(const void* a, const void* b) {
    uint32_t ua = *(const uint32_t*)a;
    uint32_t ub = *(const uint32_t*)b;
    if (ua < ub) {
        return -1;
    }
    if (ua > ub) return 1;
    return 0;
}

/*=============================================================================
 * Wiring Creation - Full
 *===========================================================================*/

lnn_wiring_t* lnn_wiring_create_full(uint32_t n_neurons) {
    // Guard clause: validate inputs
    if (n_neurons == 0) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_full: n_neurons must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_full: n_neurons is zero");
        return NULL;
    }

    // Allocate wiring structure
    lnn_wiring_t* wiring = (lnn_wiring_t*)nimcp_calloc(1, sizeof(lnn_wiring_t));
    if (!wiring) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_full: Failed to allocate wiring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_full: failed to allocate wiring");
        return NULL;
    }

    // Set metadata
    wiring->type = LNN_WIRING_FULL;
    wiring->n_neurons = n_neurons;
    wiring->n_edges = n_neurons * n_neurons;
    wiring->sparsity = 0.0f;

    // Allocate CSR arrays
    wiring->row_ptr = (uint32_t*)nimcp_calloc(n_neurons + 1, sizeof(uint32_t));
    wiring->col_idx = (uint32_t*)nimcp_calloc(wiring->n_edges, sizeof(uint32_t));
    wiring->edge_weights = NULL;  // Optional, not used for full connectivity

    if (!wiring->row_ptr || !wiring->col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_full: Failed to allocate CSR arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_full: failed to allocate CSR arrays");
        lnn_wiring_destroy(wiring);
        return NULL;
    }

    // Fill CSR structure
    // Each neuron connects to all neurons (including self)
    for (uint32_t i = 0; i < n_neurons; i++) {
        wiring->row_ptr[i] = i * n_neurons;
        for (uint32_t j = 0; j < n_neurons; j++) {
            wiring->col_idx[i * n_neurons + j] = j;
        }
    }
    wiring->row_ptr[n_neurons] = wiring->n_edges;

    NIMCP_LOGGING_INFO("Created full wiring: %u neurons, %u edges", n_neurons, wiring->n_edges);
    return wiring;
}

/*=============================================================================
 * Wiring Creation - Random (Erdos-Renyi)
 *===========================================================================*/

lnn_wiring_t* lnn_wiring_create_random(uint32_t n_neurons, float sparsity, uint64_t seed) {
    // Guard clauses
    if (n_neurons == 0) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_random: n_neurons must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "lnn_wiring_create_random: n_neurons=0");
        return NULL;
    }
    if (sparsity < 0.0f || sparsity >= 1.0f) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_random: sparsity must be in [0, 1)");
        return NULL;
    }

    // Seed RNG
    lnn_wiring_seed_rng(seed);

    // Allocate wiring structure
    lnn_wiring_t* wiring = (lnn_wiring_t*)nimcp_calloc(1, sizeof(lnn_wiring_t));
    if (!wiring) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_random: Failed to allocate wiring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_random: failed to allocate wiring");
        return NULL;
    }

    // Set metadata
    wiring->type = LNN_WIRING_RANDOM;
    wiring->n_neurons = n_neurons;
    wiring->sparsity = sparsity;

    // Temporary storage for edges (worst case: all edges)
    uint32_t max_edges = n_neurons * n_neurons;
    uint32_t* temp_edges = (uint32_t*)nimcp_malloc(max_edges * sizeof(uint32_t));
    if (!temp_edges) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_random: Failed to allocate temp edges");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_random: failed to allocate temp edges");
        nimcp_free(wiring);
        return NULL;
    }

    // Allocate row_ptr
    wiring->row_ptr = (uint32_t*)nimcp_calloc(n_neurons + 1, sizeof(uint32_t));
    if (!wiring->row_ptr) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_random: Failed to allocate row_ptr");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_random: failed to allocate row_ptr");
        nimcp_free(temp_edges);
        nimcp_free(wiring);
        return NULL;
    }

    // Generate edges with probability p = 1 - sparsity
    float edge_prob = 1.0f - sparsity;
    uint32_t edge_count = 0;

    for (uint32_t i = 0; i < n_neurons; i++) {
        wiring->row_ptr[i] = edge_count;
        for (uint32_t j = 0; j < n_neurons; j++) {
            if (lnn_wiring_randf() < edge_prob) {
                temp_edges[edge_count++] = j;
            }
        }
    }
    wiring->row_ptr[n_neurons] = edge_count;
    wiring->n_edges = edge_count;

    // Allocate exact size for col_idx
    wiring->col_idx = (uint32_t*)nimcp_malloc(edge_count * sizeof(uint32_t));
    if (!wiring->col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_random: Failed to allocate col_idx");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_random: failed to allocate col_idx");
        nimcp_free(temp_edges);
        lnn_wiring_destroy(wiring);
        return NULL;
    }

    // Copy edges
    memcpy(wiring->col_idx, temp_edges, edge_count * sizeof(uint32_t));
    nimcp_free(temp_edges);

    NIMCP_LOGGING_INFO("Created random wiring: %u neurons, %u edges, %.2f%% sparse",
                       n_neurons, edge_count, sparsity * 100.0f);
    return wiring;
}

/*=============================================================================
 * Wiring Creation - Small-World (Watts-Strogatz)
 *===========================================================================*/

lnn_wiring_t* lnn_wiring_create_small_world(uint32_t n_neurons, uint32_t k, float p, uint64_t seed) {
    // Guard clauses
    if (n_neurons == 0) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_small_world: n_neurons must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_small_world: n_neurons is zero");
        return NULL;
    }
    if (k == 0 || k >= n_neurons) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_small_world: k must be in (0, n_neurons)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_small_world: k is zero");
        return NULL;
    }
    if (k % 2 != 0) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_small_world: k must be even");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_small_world: validation failed");
        return NULL;
    }
    if (p < 0.0f || p > 1.0f) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_small_world: p must be in [0, 1]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_small_world: validation failed");
        return NULL;
    }

    // Seed RNG
    lnn_wiring_seed_rng(seed);

    // Allocate wiring structure
    lnn_wiring_t* wiring = (lnn_wiring_t*)nimcp_calloc(1, sizeof(lnn_wiring_t));
    if (!wiring) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_small_world: Failed to allocate wiring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_small_world: failed to allocate wiring");
        return NULL;
    }

    // Set metadata
    wiring->type = LNN_WIRING_SMALL_WORLD;
    wiring->n_neurons = n_neurons;
    wiring->k_neighbors = k;
    wiring->rewire_prob = p;
    wiring->n_edges = n_neurons * k;  // Initial ring lattice edges

    // Allocate CSR arrays
    wiring->row_ptr = (uint32_t*)nimcp_calloc(n_neurons + 1, sizeof(uint32_t));
    wiring->col_idx = (uint32_t*)nimcp_malloc(wiring->n_edges * sizeof(uint32_t));

    if (!wiring->row_ptr || !wiring->col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_small_world: Failed to allocate CSR arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_small_world: failed to allocate CSR arrays");
        lnn_wiring_destroy(wiring);
        return NULL;
    }

    // Step 1: Create ring lattice with k/2 neighbors on each side
    uint32_t edge_count = 0;
    for (uint32_t i = 0; i < n_neurons; i++) {
        wiring->row_ptr[i] = edge_count;
        for (uint32_t offset = 1; offset <= k / 2; offset++) {
            // Right neighbor
            wiring->col_idx[edge_count++] = (i + offset) % n_neurons;
            // Left neighbor
            wiring->col_idx[edge_count++] = (i + n_neurons - offset) % n_neurons;
        }
    }
    wiring->row_ptr[n_neurons] = edge_count;

    // Step 2: Rewire edges with probability p
    for (uint32_t i = 0; i < n_neurons; i++) {
        uint32_t start = wiring->row_ptr[i];
        uint32_t end = wiring->row_ptr[i + 1];
        for (uint32_t e = start; e < end; e++) {
            if (lnn_wiring_randf() < p) {
                // Rewire to random target (avoid self-loops and duplicates)
                uint32_t new_target;
                bool duplicate;
                int max_attempts = 100;
                do {
                    new_target = lnn_wiring_randi(0, n_neurons);
                    duplicate = (new_target == i);  // No self-loops
                    if (!duplicate) {
                        // Check for duplicates in existing edges
                        for (uint32_t e2 = start; e2 < end; e2++) {
                            if (e2 != e && wiring->col_idx[e2] == new_target) {
                                duplicate = true;
                                break;
                            }
                        }
                    }
                } while (duplicate && --max_attempts > 0);

                if (max_attempts > 0) {
                    wiring->col_idx[e] = new_target;
                }
            }
        }
    }

    // Step 3: Sort col_idx within each row for binary search compatibility
    for (uint32_t i = 0; i < n_neurons; i++) {
        uint32_t start = wiring->row_ptr[i];
        uint32_t end = wiring->row_ptr[i + 1];
        if (end > start) {
            qsort(&wiring->col_idx[start], end - start, sizeof(uint32_t), lnn_wiring_compare_uint32);
        }
    }

    // Compute sparsity
    wiring->sparsity = lnn_wiring_compute_sparsity(wiring);

    NIMCP_LOGGING_INFO("Created small-world wiring: %u neurons, k=%u, p=%.2f, %u edges",
                       n_neurons, k, p, edge_count);
    return wiring;
}

/*=============================================================================
 * Wiring Creation - Scale-Free (Barabasi-Albert)
 *===========================================================================*/

lnn_wiring_t* lnn_wiring_create_scale_free(uint32_t n_neurons, uint32_t m, uint64_t seed) {
    // Guard clauses
    if (n_neurons == 0) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_scale_free: n_neurons must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_scale_free: n_neurons is zero");
        return NULL;
    }
    if (m == 0 || m >= n_neurons) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_scale_free: m must be in (0, n_neurons)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_scale_free: m is zero");
        return NULL;
    }

    // Seed RNG
    lnn_wiring_seed_rng(seed);

    // Allocate wiring structure
    lnn_wiring_t* wiring = (lnn_wiring_t*)nimcp_calloc(1, sizeof(lnn_wiring_t));
    if (!wiring) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_scale_free: Failed to allocate wiring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_scale_free: failed to allocate wiring");
        return NULL;
    }

    // Set metadata
    wiring->type = LNN_WIRING_SCALE_FREE;
    wiring->n_neurons = n_neurons;
    wiring->m_edges = m;

    // Allocate temporary edge storage
    uint32_t max_edges = n_neurons * m;  // Upper bound
    uint32_t* temp_row_ptr = (uint32_t*)nimcp_calloc(n_neurons + 1, sizeof(uint32_t));
    uint32_t* temp_col_idx = (uint32_t*)nimcp_malloc(max_edges * sizeof(uint32_t));
    uint32_t* degrees = (uint32_t*)nimcp_calloc(n_neurons, sizeof(uint32_t));

    if (!temp_row_ptr || !temp_col_idx || !degrees) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_scale_free: Failed to allocate temp arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_scale_free: failed to allocate temp arrays");
        nimcp_free(temp_row_ptr);
        nimcp_free(temp_col_idx);
        nimcp_free(degrees);
        nimcp_free(wiring);
        return NULL;
    }

    // Step 1: Start with m0 = m fully connected nodes
    uint32_t edge_count = 0;
    for (uint32_t i = 0; i < m; i++) {
        temp_row_ptr[i] = edge_count;
        for (uint32_t j = 0; j < m; j++) {
            if (i != j) {
                temp_col_idx[edge_count++] = j;
                degrees[j]++;
            }
        }
    }

    // Step 2: Add remaining nodes with preferential attachment
    for (uint32_t i = m; i < n_neurons; i++) {
        temp_row_ptr[i] = edge_count;

        // Calculate total degree for normalization
        uint32_t total_degree = 0;
        for (uint32_t j = 0; j < i; j++) {
            total_degree += degrees[j];
        }

        // Attach to m existing nodes with probability proportional to degree
        uint32_t attached = 0;
        bool* selected = (bool*)nimcp_calloc(i, sizeof(bool));
        if (!selected) {
            NIMCP_LOGGING_ERROR("lnn_wiring_create_scale_free: Failed to allocate selection array");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_scale_free: failed to allocate selection array");
            nimcp_free(temp_row_ptr);
            nimcp_free(temp_col_idx);
            nimcp_free(degrees);
            nimcp_free(wiring);
            return NULL;
        }

        while (attached < m && attached < i) {
            // Select target with probability proportional to degree
            float r = lnn_wiring_randf() * (float)total_degree;
            float cumsum = 0.0f;
            for (uint32_t j = 0; j < i; j++) {
                if (!selected[j]) {
                    cumsum += (float)degrees[j];
                    if (cumsum >= r) {
                        temp_col_idx[edge_count++] = j;
                        degrees[j]++;
                        selected[j] = true;
                        attached++;
                        break;
                    }
                }
            }
        }
        nimcp_free(selected);
    }
    temp_row_ptr[n_neurons] = edge_count;
    wiring->n_edges = edge_count;

    // Allocate final CSR arrays
    wiring->row_ptr = (uint32_t*)nimcp_malloc((n_neurons + 1) * sizeof(uint32_t));
    wiring->col_idx = (uint32_t*)nimcp_malloc(edge_count * sizeof(uint32_t));

    if (!wiring->row_ptr || !wiring->col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_scale_free: Failed to allocate final CSR arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_scale_free: failed to allocate final CSR arrays");
        nimcp_free(temp_row_ptr);
        nimcp_free(temp_col_idx);
        nimcp_free(degrees);
        lnn_wiring_destroy(wiring);
        return NULL;
    }

    // Copy to final arrays
    memcpy(wiring->row_ptr, temp_row_ptr, (n_neurons + 1) * sizeof(uint32_t));
    memcpy(wiring->col_idx, temp_col_idx, edge_count * sizeof(uint32_t));

    // Cleanup
    nimcp_free(temp_row_ptr);
    nimcp_free(temp_col_idx);
    nimcp_free(degrees);

    // Compute sparsity
    wiring->sparsity = lnn_wiring_compute_sparsity(wiring);

    NIMCP_LOGGING_INFO("Created scale-free wiring: %u neurons, m=%u, %u edges",
                       n_neurons, m, edge_count);
    return wiring;
}

/*=============================================================================
 * Wiring Creation - NCP
 *===========================================================================*/

lnn_wiring_t* lnn_wiring_create_ncp(uint32_t n_sensory, uint32_t n_inter,
                                     uint32_t n_command, uint32_t n_motor) {
    // Guard clauses
    if (n_sensory == 0 || n_inter == 0 || n_command == 0 || n_motor == 0) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_ncp: All neuron counts must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_scale_free: n_sensory is zero");
        return NULL;
    }

    uint32_t n_total = n_sensory + n_inter + n_command + n_motor;

    // Allocate wiring structure
    lnn_wiring_t* wiring = (lnn_wiring_t*)nimcp_calloc(1, sizeof(lnn_wiring_t));
    if (!wiring) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_ncp: Failed to allocate wiring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_ncp: failed to allocate wiring");
        return NULL;
    }

    // Set metadata
    wiring->type = LNN_WIRING_NCP;
    wiring->n_neurons = n_total;
    wiring->n_sensory = n_sensory;
    wiring->n_inter = n_inter;
    wiring->n_command = n_command;
    wiring->n_motor = n_motor;

    // Neuron ID ranges:
    // Sensory: [0, n_sensory)
    // Inter: [n_sensory, n_sensory + n_inter)
    // Command: [n_sensory + n_inter, n_sensory + n_inter + n_command)
    // Motor: [n_sensory + n_inter + n_command, n_total)

    uint32_t inter_start = n_sensory;
    uint32_t command_start = n_sensory + n_inter;
    uint32_t motor_start = n_sensory + n_inter + n_command;

    // Temporary edge storage
    uint32_t max_edges = n_total * n_total;  // Worst case
    uint32_t* temp_row_ptr = (uint32_t*)nimcp_calloc(n_total + 1, sizeof(uint32_t));
    uint32_t* temp_col_idx = (uint32_t*)nimcp_malloc(max_edges * sizeof(uint32_t));

    if (!temp_row_ptr || !temp_col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_ncp: Failed to allocate temp arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_ncp: failed to allocate temp arrays");
        nimcp_free(temp_row_ptr);
        nimcp_free(temp_col_idx);
        nimcp_free(wiring);
        return NULL;
    }

    uint32_t edge_count = 0;

    // Connectivity rules (sparse, probabilistic)
    for (uint32_t i = 0; i < n_total; i++) {
        temp_row_ptr[i] = edge_count;

        // Sensory neurons (no incoming, only outgoing)
        if (i < inter_start) {
            // Sensory → Inter (50% connectivity)
            for (uint32_t j = inter_start; j < command_start; j++) {
                if (lnn_wiring_randf() < 0.5f) {
                    temp_col_idx[edge_count++] = j;
                }
            }
        }
        // Interneurons
        else if (i < command_start) {
            // Inter → Inter (30% recurrent)
            for (uint32_t j = inter_start; j < command_start; j++) {
                if (i != j && lnn_wiring_randf() < 0.3f) {
                    temp_col_idx[edge_count++] = j;
                }
            }
            // Inter → Command (50% connectivity)
            for (uint32_t j = command_start; j < motor_start; j++) {
                if (lnn_wiring_randf() < 0.5f) {
                    temp_col_idx[edge_count++] = j;
                }
            }
        }
        // Command neurons
        else if (i < motor_start) {
            // Command → Motor (80% dense for precise control)
            for (uint32_t j = motor_start; j < n_total; j++) {
                if (lnn_wiring_randf() < 0.8f) {
                    temp_col_idx[edge_count++] = j;
                }
            }
        }
        // Motor neurons (no outgoing connections in typical NCP)
    }
    temp_row_ptr[n_total] = edge_count;
    wiring->n_edges = edge_count;

    // Allocate final CSR arrays
    wiring->row_ptr = (uint32_t*)nimcp_malloc((n_total + 1) * sizeof(uint32_t));
    wiring->col_idx = (uint32_t*)nimcp_malloc(edge_count * sizeof(uint32_t));

    if (!wiring->row_ptr || !wiring->col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_ncp: Failed to allocate final CSR arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_ncp: failed to allocate final CSR arrays");
        nimcp_free(temp_row_ptr);
        nimcp_free(temp_col_idx);
        lnn_wiring_destroy(wiring);
        return NULL;
    }

    // Copy to final arrays
    memcpy(wiring->row_ptr, temp_row_ptr, (n_total + 1) * sizeof(uint32_t));
    memcpy(wiring->col_idx, temp_col_idx, edge_count * sizeof(uint32_t));

    // Cleanup
    nimcp_free(temp_row_ptr);
    nimcp_free(temp_col_idx);

    // Compute sparsity
    wiring->sparsity = lnn_wiring_compute_sparsity(wiring);

    NIMCP_LOGGING_INFO("Created NCP wiring: S=%u I=%u C=%u M=%u, %u edges, %.2f%% sparse",
                       n_sensory, n_inter, n_command, n_motor, edge_count, wiring->sparsity * 100.0f);
    return wiring;
}

/*=============================================================================
 * Wiring Creation - From Adjacency Matrix
 *===========================================================================*/

lnn_wiring_t* lnn_wiring_create_from_adjacency(const uint8_t* adj, uint32_t n_neurons) {
    // Guard clauses
    if (!adj) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_from_adjacency: adj is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_from_adjacency: adj is NULL");
        return NULL;
    }
    if (n_neurons == 0) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_from_adjacency: n_neurons must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_from_adjacency: n_neurons is zero");
        return NULL;
    }

    // Allocate wiring structure
    lnn_wiring_t* wiring = (lnn_wiring_t*)nimcp_calloc(1, sizeof(lnn_wiring_t));
    if (!wiring) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_from_adjacency: Failed to allocate wiring");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_from_adjacency: failed to allocate wiring");
        return NULL;
    }

    // Set metadata
    wiring->type = LNN_WIRING_CUSTOM;
    wiring->n_neurons = n_neurons;

    // First pass: count edges
    uint32_t edge_count = 0;
    for (uint32_t i = 0; i < n_neurons; i++) {
        for (uint32_t j = 0; j < n_neurons; j++) {
            if (adj[i * n_neurons + j] != 0) {
                edge_count++;
            }
        }
    }
    wiring->n_edges = edge_count;

    // Allocate CSR arrays
    wiring->row_ptr = (uint32_t*)nimcp_calloc(n_neurons + 1, sizeof(uint32_t));
    wiring->col_idx = (uint32_t*)nimcp_malloc(edge_count * sizeof(uint32_t));

    if (!wiring->row_ptr || !wiring->col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_create_from_adjacency: Failed to allocate CSR arrays");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create_from_adjacency: failed to allocate CSR arrays");
        lnn_wiring_destroy(wiring);
        return NULL;
    }

    // Second pass: fill CSR structure
    uint32_t edge_idx = 0;
    for (uint32_t i = 0; i < n_neurons; i++) {
        wiring->row_ptr[i] = edge_idx;
        for (uint32_t j = 0; j < n_neurons; j++) {
            if (adj[i * n_neurons + j] != 0) {
                wiring->col_idx[edge_idx++] = j;
            }
        }
    }
    wiring->row_ptr[n_neurons] = edge_idx;

    // Compute sparsity
    wiring->sparsity = lnn_wiring_compute_sparsity(wiring);

    NIMCP_LOGGING_INFO("Created custom wiring from adjacency: %u neurons, %u edges, %.2f%% sparse",
                       n_neurons, edge_count, wiring->sparsity * 100.0f);
    return wiring;
}

/*=============================================================================
 * Wiring Destruction
 *===========================================================================*/

void lnn_wiring_destroy(lnn_wiring_t* wiring) {
    // Guard clause: NULL is safe
    if (!wiring) {
        return;
    }

    // Free CSR arrays
    if (wiring->row_ptr) {
        nimcp_free(wiring->row_ptr);
    }
    if (wiring->col_idx) {
        nimcp_free(wiring->col_idx);
    }
    if (wiring->edge_weights) {
        nimcp_free(wiring->edge_weights);
    }

    // Free wiring structure
    nimcp_free(wiring);
}

/*=============================================================================
 * Wiring Queries
 *===========================================================================*/

bool lnn_wiring_has_edge(const lnn_wiring_t* wiring, uint32_t from, uint32_t to) {
    // Guard clauses
    if (!wiring || !wiring->row_ptr || !wiring->col_idx) {
        return false;
    }
    if (from >= wiring->n_neurons || to >= wiring->n_neurons) {
        return false;
    }

    // Get range for source neuron
    uint32_t start = wiring->row_ptr[from];
    uint32_t end = wiring->row_ptr[from + 1];

    // Binary search in col_idx
    return lnn_wiring_binary_search(wiring->col_idx, start, end, to);
}

uint32_t lnn_wiring_out_degree(const lnn_wiring_t* wiring, uint32_t neuron) {
    // Guard clauses
    if (!wiring || !wiring->row_ptr) {
        return 0;
    }
    if (neuron >= wiring->n_neurons) {
        return 0;
    }

    // Return difference in row pointers
    return wiring->row_ptr[neuron + 1] - wiring->row_ptr[neuron];
}

uint32_t lnn_wiring_in_degree(const lnn_wiring_t* wiring, uint32_t neuron) {
    // Guard clauses
    if (!wiring || !wiring->col_idx) {
        return 0;
    }
    if (neuron >= wiring->n_neurons) {
        return 0;
    }

    // Count occurrences in col_idx (expensive O(E) operation)
    uint32_t count = 0;
    for (uint32_t i = 0; i < wiring->n_edges; i++) {
        if (wiring->col_idx[i] == neuron) {
            count++;
        }
    }
    return count;
}

const uint32_t* lnn_wiring_get_neighbors(const lnn_wiring_t* wiring, uint32_t neuron, uint32_t* count) {
    // Guard clauses
    if (!wiring || !wiring->row_ptr || !wiring->col_idx || !count) {
        if (count) *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_wiring_get_neighbors: validation failed");
        return NULL;
    }
    if (neuron >= wiring->n_neurons) {
        *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_wiring_get_neighbors: capacity exceeded");
        return NULL;
    }

    // Get range
    uint32_t start = wiring->row_ptr[neuron];
    *count = wiring->row_ptr[neuron + 1] - start;

    // Return pointer to start of neighbors
    return &wiring->col_idx[start];
}

/*=============================================================================
 * Wiring Utilities
 *===========================================================================*/

float lnn_wiring_compute_sparsity(const lnn_wiring_t* wiring) {
    // Guard clause
    if (!wiring || wiring->n_neurons == 0) {
        return 0.0f;
    }

    // Sparsity = 1 - (n_edges / n_neurons^2)
    uint32_t max_edges = wiring->n_neurons * wiring->n_neurons;
    if (max_edges == 0) {
        return 0.0f;
    }

    return 1.0f - ((float)wiring->n_edges / (float)max_edges);
}

int lnn_wiring_to_dense(const lnn_wiring_t* wiring, float* dense, uint32_t rows, uint32_t cols) {
    // Guard clauses
    if (!wiring || !dense) {
        NIMCP_LOGGING_ERROR("lnn_wiring_to_dense: NULL input");
        return LNN_ERROR_NULL_POINTER;
    }
    if (rows < wiring->n_neurons || cols < wiring->n_neurons) {
        NIMCP_LOGGING_ERROR("lnn_wiring_to_dense: Output matrix too small");
        return LNN_ERROR_INVALID_DIMENSION;
    }
    if (!wiring->row_ptr || !wiring->col_idx) {
        NIMCP_LOGGING_ERROR("lnn_wiring_to_dense: Invalid wiring structure");
        return LNN_ERROR_INVALID_STATE;
    }

    // Initialize dense matrix to zeros
    memset(dense, 0, rows * cols * sizeof(float));

    // Fill in edges from CSR
    for (uint32_t i = 0; i < wiring->n_neurons; i++) {
        uint32_t start = wiring->row_ptr[i];
        uint32_t end = wiring->row_ptr[i + 1];
        for (uint32_t e = start; e < end; e++) {
            uint32_t j = wiring->col_idx[e];
            dense[i * cols + j] = 1.0f;
        }
    }

    return LNN_SUCCESS;
}

lnn_wiring_t* lnn_wiring_create(lnn_wiring_type_t type, uint32_t n_neurons, float sparsity) {
    /* Guard: validate inputs */
    if (n_neurons == 0) {
        NIMCP_LOGGING_ERROR("n_neurons must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_wiring_create: n_neurons is zero");
        return NULL;
    }

    /* Dispatch to appropriate creator based on type */
    switch (type) {
        case LNN_WIRING_FULL:
            return lnn_wiring_create_full(n_neurons);

        case LNN_WIRING_RANDOM:
            return lnn_wiring_create_random(n_neurons, sparsity, 0);

        case LNN_WIRING_SMALL_WORLD:
            /* Default: 4 neighbors, 5% rewiring */
            return lnn_wiring_create_small_world(n_neurons, 4, 0.05f, 0);

        case LNN_WIRING_SCALE_FREE:
            /* Default: 2 edges per new node */
            return lnn_wiring_create_scale_free(n_neurons, 2, 0);

        case LNN_WIRING_FEEDFORWARD:
        case LNN_WIRING_RECURRENT:
            /* For now, treat as full connectivity */
            return lnn_wiring_create_full(n_neurons);

        case LNN_WIRING_NCP:
            /* For NCP, need specific neuron counts - use default split.
             * NCP requires at least 8 neurons (minimum 1 per group × 4 groups + slack).
             * For small networks, fall back to full connectivity.
             */
            {
                if (n_neurons < 8) {
                    NIMCP_LOGGING_WARN("NCP wiring requires >= 8 neurons, using full connectivity");
                    return lnn_wiring_create_full(n_neurons);
                }
                /* Ensure each group has at least 1 neuron */
                uint32_t n_sensory = (n_neurons / 4) > 0 ? (n_neurons / 4) : 1;
                uint32_t n_inter = (n_neurons / 2) > 0 ? (n_neurons / 2) : 1;
                uint32_t n_command = (n_neurons / 8) > 0 ? (n_neurons / 8) : 1;
                uint32_t n_motor = n_neurons - n_sensory - n_inter - n_command;
                if (n_motor == 0) n_motor = 1;
                return lnn_wiring_create_ncp(n_sensory, n_inter, n_command, n_motor);
            }

        case LNN_WIRING_MODULAR:
        case LNN_WIRING_CUSTOM:
        default:
            /* Fall back to random sparse */
            NIMCP_LOGGING_WARN("Wiring type %d not fully supported, using random", type);
            return lnn_wiring_create_random(n_neurons, sparsity > 0 ? sparsity : 0.5f, 0);
    }
}

const char* lnn_wiring_type_to_string(lnn_wiring_type_t type) {
    switch (type) {
        case LNN_WIRING_FULL:        return "Full";
        case LNN_WIRING_RANDOM:      return "Random";
        case LNN_WIRING_SMALL_WORLD: return "Small-World";
        case LNN_WIRING_SCALE_FREE:  return "Scale-Free";
        case LNN_WIRING_MODULAR:     return "Modular";
        case LNN_WIRING_FEEDFORWARD: return "Feedforward";
        case LNN_WIRING_RECURRENT:   return "Recurrent";
        case LNN_WIRING_NCP:         return "NCP";
        case LNN_WIRING_CUSTOM:      return "Custom";
        default:                     return "Unknown";
    }
}

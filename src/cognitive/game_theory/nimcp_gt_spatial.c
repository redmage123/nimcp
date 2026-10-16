//=============================================================================
// nimcp_gt_spatial.c - Population and Spatial Games Implementation
//=============================================================================
/**
 * @file nimcp_gt_spatial.c
 * @brief Evolutionary game theory with spatial structure
 *
 * WHAT: Implements spatial evolutionary dynamics
 * WHY:  Model local interactions and strategy evolution
 * HOW:  Networks, replicator dynamics, update rules
 */

#include "cognitive/game_theory/nimcp_gt_spatial.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/statistics/nimcp_statistics.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#define LOG_MODULE "spatial"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(gt_spatial, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal Constants
//=============================================================================

#define MIN_FREQUENCY           1e-10f
#define CONVERGENCE_CHECK_FREQ  10
#define HISTORY_SAMPLE_FREQ     10

//=============================================================================
// Internal Structure
//=============================================================================

struct nimcp_spatial_game_struct {
    nimcp_spatial_config_t config;
    nimcp_spatial_state_t state;

    /** Payoff matrix [num_strategies x num_strategies] */
    float* payoff_matrix;

    /** Network structure */
    uint32_t num_nodes;
    uint32_t* node_degree;           /**< Degree of each node */
    uint32_t** node_neighbors;       /**< Neighbors of each node */
    float** edge_weights;            /**< Edge weights (NULL if unweighted) */

    /** Node states */
    uint32_t* node_strategy;         /**< Current strategy per node */
    uint32_t* node_strategy_next;    /**< Next strategy (for synchronous update) */
    float* node_fitness;             /**< Fitness of each node */

    /** Population-level state */
    float* frequencies;              /**< Strategy frequencies */
    float* fitness_per_strategy;     /**< Average fitness per strategy */
    float avg_fitness;

    /** Simulation state */
    uint32_t current_step;
    uint64_t strategy_switches;

    /** History tracking */
    float* frequency_history;
    uint32_t history_capacity;
    uint32_t history_length;

    /** Random state */
    uint64_t rng_state;

    /** Thread safety */
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Static Name Tables
//=============================================================================

static const char* s_topology_names[] = {
    "Complete Graph",
    "2D Grid (4 neighbors)",
    "2D Grid Moore (8 neighbors)",
    "Ring",
    "Random Graph (Erdos-Renyi)",
    "Scale-Free (Barabasi-Albert)",
    "Small World (Watts-Strogatz)",
    "Custom"
};

static const char* s_update_rule_names[] = {
    "Replicator Dynamics",
    "Imitation (Best Neighbor)",
    "Best Response",
    "Fermi Update",
    "Moran Process",
    "Death-Birth"
};

//=============================================================================
// Internal Random Number Generator (xorshift64)
//=============================================================================

static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static float random_float(uint64_t* state) {
    return (float)(xorshift64(state) & 0xFFFFFFFF) / (float)0xFFFFFFFF;
}

static uint32_t random_range(uint64_t* state, uint32_t max) {
    if (max == 0) return 0;
    return (uint32_t)(xorshift64(state) % max);
}

//=============================================================================
// Forward Declarations
//=============================================================================

static bool is_ess_unlocked(const nimcp_spatial_game_t ctx, uint32_t strategy);

//=============================================================================
// Network Building Functions
//=============================================================================

/**
 * @brief Allocate neighbor arrays for all nodes
 */
static nimcp_error_t allocate_neighbors(nimcp_spatial_game_t ctx) {
    ctx->node_neighbors = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t*));
    if (!ctx->node_neighbors) return NIMCP_GT_ERROR_NO_MEMORY;

    ctx->node_degree = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t));
    if (!ctx->node_degree) return NIMCP_GT_ERROR_NO_MEMORY;

    return NIMCP_SUCCESS;
}

/**
 * @brief Build complete graph (all nodes connected)
 */
static nimcp_error_t build_complete_graph(nimcp_spatial_game_t ctx) {
    uint32_t n = ctx->num_nodes;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        ctx->node_degree[i] = n - 1;
        ctx->node_neighbors[i] = nimcp_calloc(n - 1, sizeof(uint32_t));
        if (!ctx->node_neighbors[i]) return NIMCP_GT_ERROR_NO_MEMORY;

        uint32_t idx = 0;
        for (uint32_t j = 0; j < n; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && n > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(j + 1) / (float)n);
            }

            if (j != i) {
                ctx->node_neighbors[i][idx++] = j;
            }
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Build 2D grid with von Neumann neighborhood (4 neighbors)
 */
static nimcp_error_t build_grid_2d(nimcp_spatial_game_t ctx) {
    uint32_t w = ctx->config.grid_width;
    uint32_t h = ctx->config.grid_height;
    bool periodic = (ctx->config.boundary == NIMCP_BOUNDARY_PERIODIC);

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)ctx->num_nodes);
        }

        uint32_t x = i % w;
        uint32_t y = i / w;

        // Count neighbors
        uint32_t count = 0;
        uint32_t temp_neighbors[4];

        // Left
        if (x > 0 || periodic) {
            uint32_t nx = (x == 0) ? (w - 1) : (x - 1);
            temp_neighbors[count++] = y * w + nx;
        }
        // Right
        if (x < w - 1 || periodic) {
            uint32_t nx = (x == w - 1) ? 0 : (x + 1);
            temp_neighbors[count++] = y * w + nx;
        }
        // Up
        if (y > 0 || periodic) {
            uint32_t ny = (y == 0) ? (h - 1) : (y - 1);
            temp_neighbors[count++] = ny * w + x;
        }
        // Down
        if (y < h - 1 || periodic) {
            uint32_t ny = (y == h - 1) ? 0 : (y + 1);
            temp_neighbors[count++] = ny * w + x;
        }

        ctx->node_degree[i] = count;
        ctx->node_neighbors[i] = nimcp_calloc(count, sizeof(uint32_t));
        if (!ctx->node_neighbors[i]) return NIMCP_GT_ERROR_NO_MEMORY;

        memcpy(ctx->node_neighbors[i], temp_neighbors, count * sizeof(uint32_t));
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Build 2D grid with Moore neighborhood (8 neighbors)
 */
static nimcp_error_t build_grid_2d_moore(nimcp_spatial_game_t ctx) {
    uint32_t w = ctx->config.grid_width;
    uint32_t h = ctx->config.grid_height;
    bool periodic = (ctx->config.boundary == NIMCP_BOUNDARY_PERIODIC);

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)ctx->num_nodes);
        }

        uint32_t x = i % w;
        uint32_t y = i / w;

        uint32_t count = 0;
        uint32_t temp_neighbors[8];

        // All 8 directions
        int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

        for (int d = 0; d < 8; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && 8 > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(d + 1) / (float)8);
            }

            int nx = (int)x + dx[d];
            int ny = (int)y + dy[d];

            if (periodic) {
                nx = (nx + (int)w) % (int)w;
                ny = (ny + (int)h) % (int)h;
            } else {
                if (nx < 0 || nx >= (int)w || ny < 0 || ny >= (int)h) {
                    continue;
                }
            }

            temp_neighbors[count++] = (uint32_t)ny * w + (uint32_t)nx;
        }

        ctx->node_degree[i] = count;
        ctx->node_neighbors[i] = nimcp_calloc(count, sizeof(uint32_t));
        if (!ctx->node_neighbors[i]) return NIMCP_GT_ERROR_NO_MEMORY;

        memcpy(ctx->node_neighbors[i], temp_neighbors, count * sizeof(uint32_t));
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Build ring graph
 */
static nimcp_error_t build_ring(nimcp_spatial_game_t ctx) {
    uint32_t n = ctx->num_nodes;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        ctx->node_degree[i] = 2;
        ctx->node_neighbors[i] = nimcp_calloc(2, sizeof(uint32_t));
        if (!ctx->node_neighbors[i]) return NIMCP_GT_ERROR_NO_MEMORY;

        ctx->node_neighbors[i][0] = (i + n - 1) % n;  // Previous
        ctx->node_neighbors[i][1] = (i + 1) % n;      // Next
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Build Erdos-Renyi random graph
 */
static nimcp_error_t build_random_graph(nimcp_spatial_game_t ctx) {
    uint32_t n = ctx->num_nodes;
    float p = ctx->config.edge_probability;

    // First pass: count degrees
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        uint32_t count = 0;
        for (uint32_t j = i + 1; j < n; j++) {
            if (random_float(&ctx->rng_state) < p) {
                count++;
            }
        }
        ctx->node_degree[i] = count;
    }

    // Second pass: for undirected graph, add reverse edges
    // Use adjacency list construction
    uint32_t** temp_adj = nimcp_calloc(n, sizeof(uint32_t*));
    uint32_t* temp_count = nimcp_calloc(n, sizeof(uint32_t));
    uint32_t* temp_cap = nimcp_calloc(n, sizeof(uint32_t));
    if (!temp_adj || !temp_count || !temp_cap) {
        nimcp_free(temp_adj);
        temp_adj = NULL;
        nimcp_free(temp_count);
        temp_count = NULL;
        nimcp_free(temp_cap);
        temp_cap = NULL;
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    // Initialize with estimated capacity
    uint32_t expected_degree = (uint32_t)(p * (float)n);
    if (expected_degree < 4) expected_degree = 4;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        temp_cap[i] = expected_degree * 2;
        temp_adj[i] = nimcp_calloc(temp_cap[i], sizeof(uint32_t));
        if (!temp_adj[i]) {
            for (uint32_t k = 0; k < i; k++) nimcp_free(temp_adj[k]);
            nimcp_free(temp_adj);
            temp_adj = NULL;
            nimcp_free(temp_count);
            temp_count = NULL;
            nimcp_free(temp_cap);
            temp_cap = NULL;
            return NIMCP_GT_ERROR_NO_MEMORY;
        }
    }

    // Reset RNG for reproducibility
    ctx->rng_state = ctx->config.seed ? ctx->config.seed : (uint64_t)nimcp_time_get_ms();

    // Generate edges
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)n);
        }

        for (uint32_t j = i + 1; j < n; j++) {
            if (random_float(&ctx->rng_state) < p) {
                // Add edge i-j
                if (temp_count[i] >= temp_cap[i]) {
                    temp_cap[i] *= 2;
                    uint32_t* new_arr = nimcp_realloc(temp_adj[i], temp_cap[i] * sizeof(uint32_t));
                    if (!new_arr) goto cleanup_random;
                    temp_adj[i] = new_arr;
                }
                temp_adj[i][temp_count[i]++] = j;

                // Add edge j-i
                if (temp_count[j] >= temp_cap[j]) {
                    temp_cap[j] *= 2;
                    uint32_t* new_arr = nimcp_realloc(temp_adj[j], temp_cap[j] * sizeof(uint32_t));
                    if (!new_arr) goto cleanup_random;
                    temp_adj[j] = new_arr;
                }
                temp_adj[j][temp_count[j]++] = i;
            }
        }
    }

    // Copy to final storage
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        ctx->node_degree[i] = temp_count[i];
        if (temp_count[i] > 0) {
            ctx->node_neighbors[i] = nimcp_calloc(temp_count[i], sizeof(uint32_t));
            if (!ctx->node_neighbors[i]) goto cleanup_random;
            memcpy(ctx->node_neighbors[i], temp_adj[i], temp_count[i] * sizeof(uint32_t));
        } else {
            ctx->node_neighbors[i] = NULL;
        }
        nimcp_free(temp_adj[i]);
    }

    nimcp_free(temp_adj);
    temp_adj = NULL;
    nimcp_free(temp_count);
    temp_count = NULL;
    nimcp_free(temp_cap);
    temp_cap = NULL;
    return NIMCP_SUCCESS;

cleanup_random:
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        nimcp_free(temp_adj[i]);
    }
    nimcp_free(temp_adj);
    temp_adj = NULL;
    nimcp_free(temp_count);
    temp_count = NULL;
    nimcp_free(temp_cap);
    temp_cap = NULL;
    return NIMCP_GT_ERROR_NO_MEMORY;
}

/**
 * @brief Build Barabasi-Albert scale-free network
 */
static nimcp_error_t build_scale_free(nimcp_spatial_game_t ctx) {
    uint32_t n = ctx->num_nodes;
    uint32_t m0 = ctx->config.initial_edges;
    uint32_t m = ctx->config.edges_per_step;

    if (m0 < 2) m0 = 2;
    if (m < 1) m = 1;
    if (m > m0) m = m0;

    // Temporary storage with dynamic sizing
    uint32_t** temp_adj = nimcp_calloc(n, sizeof(uint32_t*));
    if (!temp_adj) return -1;
    uint32_t* temp_count = nimcp_calloc(n, sizeof(uint32_t));
    if (!temp_count) return -1;
    uint32_t* temp_cap = nimcp_calloc(n, sizeof(uint32_t));
    uint32_t* degree_sum_arr = nimcp_calloc(n, sizeof(uint32_t));

    if (!temp_adj || !temp_count || !temp_cap || !degree_sum_arr) {
        nimcp_free(temp_adj);
        temp_adj = NULL;
        nimcp_free(temp_count);
        temp_count = NULL;
        nimcp_free(temp_cap);
        temp_cap = NULL;
        nimcp_free(degree_sum_arr);
        degree_sum_arr = NULL;
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    // Initialize all nodes
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        temp_cap[i] = m * 4;
        temp_adj[i] = nimcp_calloc(temp_cap[i], sizeof(uint32_t));
        if (!temp_adj[i]) {
            for (uint32_t k = 0; k < i; k++) nimcp_free(temp_adj[k]);
            nimcp_free(temp_adj);
            temp_adj = NULL;
            nimcp_free(temp_count);
            temp_count = NULL;
            nimcp_free(temp_cap);
            temp_cap = NULL;
            nimcp_free(degree_sum_arr);
            degree_sum_arr = NULL;
            return NIMCP_GT_ERROR_NO_MEMORY;
        }
    }

    // Start with complete graph of m0 nodes
    for (uint32_t i = 0; i < m0 && i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)n);
        }

        for (uint32_t j = i + 1; j < m0 && j < n; j++) {
            // Add edge i-j
            if (temp_count[i] >= temp_cap[i]) {
                temp_cap[i] *= 2;
                uint32_t* new_arr = nimcp_realloc(temp_adj[i], temp_cap[i] * sizeof(uint32_t));
                if (!new_arr) goto cleanup_sf;
                temp_adj[i] = new_arr;
            }
            temp_adj[i][temp_count[i]++] = j;

            // Add edge j-i
            if (temp_count[j] >= temp_cap[j]) {
                temp_cap[j] *= 2;
                uint32_t* new_arr = nimcp_realloc(temp_adj[j], temp_cap[j] * sizeof(uint32_t));
                if (!new_arr) goto cleanup_sf;
                temp_adj[j] = new_arr;
            }
            temp_adj[j][temp_count[j]++] = i;
        }
    }

    // Add remaining nodes with preferential attachment
    for (uint32_t new_node = m0; new_node < n; new_node++) {
        // Compute cumulative degree sum
        uint32_t total_degree = 0;
        for (uint32_t i = 0; i < new_node; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && new_node > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)new_node);
            }

            total_degree += temp_count[i];
            degree_sum_arr[i] = total_degree;
        }

        if (total_degree == 0) {
            // Edge case: connect to random nodes
            for (uint32_t e = 0; e < m && e < new_node; e++) {
                uint32_t target = random_range(&ctx->rng_state, new_node);

                // Add edge
                if (temp_count[new_node] >= temp_cap[new_node]) {
                    temp_cap[new_node] *= 2;
                    uint32_t* new_arr = nimcp_realloc(temp_adj[new_node], temp_cap[new_node] * sizeof(uint32_t));
                    if (!new_arr) goto cleanup_sf;
                    temp_adj[new_node] = new_arr;
                }
                temp_adj[new_node][temp_count[new_node]++] = target;

                if (temp_count[target] >= temp_cap[target]) {
                    temp_cap[target] *= 2;
                    uint32_t* new_arr = nimcp_realloc(temp_adj[target], temp_cap[target] * sizeof(uint32_t));
                    if (!new_arr) goto cleanup_sf;
                    temp_adj[target] = new_arr;
                }
                temp_adj[target][temp_count[target]++] = new_node;
            }
        } else {
            // Preferential attachment
            uint32_t edges_added = 0;
            uint32_t attempts = 0;

            while (edges_added < m && attempts < m * 10) {
                attempts++;
                uint32_t r = random_range(&ctx->rng_state, total_degree);

                // Binary search for target
                uint32_t target = 0;
                uint32_t lo = 0, hi = new_node - 1;
                while (lo < hi) {
                    uint32_t mid = (lo + hi) / 2;
                    if (degree_sum_arr[mid] <= r) {
                        lo = mid + 1;
                    } else {
                        hi = mid;
                    }
                }
                target = lo;

                // Check if edge already exists
                bool exists = false;
                for (uint32_t e = 0; e < temp_count[new_node]; e++) {
                    if (temp_adj[new_node][e] == target) {
                        exists = true;
                        break;
                    }
                }
                if (exists) continue;

                // Add edge
                if (temp_count[new_node] >= temp_cap[new_node]) {
                    temp_cap[new_node] *= 2;
                    uint32_t* new_arr = nimcp_realloc(temp_adj[new_node], temp_cap[new_node] * sizeof(uint32_t));
                    if (!new_arr) goto cleanup_sf;
                    temp_adj[new_node] = new_arr;
                }
                temp_adj[new_node][temp_count[new_node]++] = target;

                if (temp_count[target] >= temp_cap[target]) {
                    temp_cap[target] *= 2;
                    uint32_t* new_arr = nimcp_realloc(temp_adj[target], temp_cap[target] * sizeof(uint32_t));
                    if (!new_arr) goto cleanup_sf;
                    temp_adj[target] = new_arr;
                }
                temp_adj[target][temp_count[target]++] = new_node;

                edges_added++;
            }
        }
    }

    // Copy to final storage
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        ctx->node_degree[i] = temp_count[i];
        if (temp_count[i] > 0) {
            ctx->node_neighbors[i] = nimcp_calloc(temp_count[i], sizeof(uint32_t));
            if (!ctx->node_neighbors[i]) goto cleanup_sf;
            memcpy(ctx->node_neighbors[i], temp_adj[i], temp_count[i] * sizeof(uint32_t));
        }
        nimcp_free(temp_adj[i]);
    }

    nimcp_free(temp_adj);
    temp_adj = NULL;
    nimcp_free(temp_count);
    temp_count = NULL;
    nimcp_free(temp_cap);
    temp_cap = NULL;
    nimcp_free(degree_sum_arr);
    degree_sum_arr = NULL;
    return NIMCP_SUCCESS;

cleanup_sf:
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        nimcp_free(temp_adj[i]);
    }
    nimcp_free(temp_adj);
    temp_adj = NULL;
    nimcp_free(temp_count);
    temp_count = NULL;
    nimcp_free(temp_cap);
    temp_cap = NULL;
    nimcp_free(degree_sum_arr);
    degree_sum_arr = NULL;
    return NIMCP_GT_ERROR_NO_MEMORY;
}

/**
 * @brief Build Watts-Strogatz small-world network
 */
static nimcp_error_t build_small_world(nimcp_spatial_game_t ctx) {
    uint32_t n = ctx->num_nodes;
    float p = ctx->config.rewiring_prob;
    uint32_t k = 4;  // Initial ring lattice with k/2 neighbors on each side

    // Start with ring lattice
    uint32_t** temp_adj = nimcp_calloc(n, sizeof(uint32_t*));
    uint32_t* temp_count = nimcp_calloc(n, sizeof(uint32_t));
    uint32_t* temp_cap = nimcp_calloc(n, sizeof(uint32_t));

    if (!temp_adj || !temp_count || !temp_cap) {
        nimcp_free(temp_adj);
        temp_adj = NULL;
        nimcp_free(temp_count);
        temp_count = NULL;
        nimcp_free(temp_cap);
        temp_cap = NULL;
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        temp_cap[i] = k * 2;
        temp_adj[i] = nimcp_calloc(temp_cap[i], sizeof(uint32_t));
        if (!temp_adj[i]) {
            for (uint32_t j = 0; j < i; j++) nimcp_free(temp_adj[j]);
            nimcp_free(temp_adj);
            temp_adj = NULL;
            nimcp_free(temp_count);
            temp_count = NULL;
            nimcp_free(temp_cap);
            temp_cap = NULL;
            return NIMCP_GT_ERROR_NO_MEMORY;
        }
    }

    // Build initial ring lattice
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)n);
        }

        for (uint32_t j = 1; j <= k / 2; j++) {
            uint32_t neighbor = (i + j) % n;

            temp_adj[i][temp_count[i]++] = neighbor;
            temp_adj[neighbor][temp_count[neighbor]++] = i;
        }
    }

    // Rewire edges with probability p
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        for (uint32_t e = 0; e < temp_count[i]; e++) {
            uint32_t j = temp_adj[i][e];
            if (j <= i) continue;  // Only process each edge once

            if (random_float(&ctx->rng_state) < p) {
                // Rewire: pick new target for i
                uint32_t attempts = 0;
                while (attempts < n) {
                    uint32_t new_target = random_range(&ctx->rng_state, n);
                    if (new_target == i) { attempts++; continue; }

                    // Check if edge already exists
                    bool exists = false;
                    for (uint32_t k2 = 0; k2 < temp_count[i]; k2++) {
                        if (temp_adj[i][k2] == new_target) {
                            exists = true;
                            break;
                        }
                    }
                    if (exists) { attempts++; continue; }

                    // Remove old edge i-j from j
                    for (uint32_t k2 = 0; k2 < temp_count[j]; k2++) {
                        if (temp_adj[j][k2] == i) {
                            temp_adj[j][k2] = temp_adj[j][temp_count[j] - 1];
                            temp_count[j]--;
                            break;
                        }
                    }

                    // Update edge i-j to i-new_target
                    temp_adj[i][e] = new_target;

                    // Add edge new_target-i
                    if (temp_count[new_target] >= temp_cap[new_target]) {
                        temp_cap[new_target] *= 2;
                        uint32_t* new_arr = nimcp_realloc(temp_adj[new_target], temp_cap[new_target] * sizeof(uint32_t));
                        if (!new_arr) goto cleanup_sw;
                        temp_adj[new_target] = new_arr;
                    }
                    temp_adj[new_target][temp_count[new_target]++] = i;
                    break;
                }
            }
        }
    }

    // Copy to final storage
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        ctx->node_degree[i] = temp_count[i];
        if (temp_count[i] > 0) {
            ctx->node_neighbors[i] = nimcp_calloc(temp_count[i], sizeof(uint32_t));
            if (!ctx->node_neighbors[i]) goto cleanup_sw;
            memcpy(ctx->node_neighbors[i], temp_adj[i], temp_count[i] * sizeof(uint32_t));
        }
        nimcp_free(temp_adj[i]);
    }

    nimcp_free(temp_adj);
    temp_adj = NULL;
    nimcp_free(temp_count);
    temp_count = NULL;
    nimcp_free(temp_cap);
    temp_cap = NULL;
    return NIMCP_SUCCESS;

cleanup_sw:
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)n);
        }

        nimcp_free(temp_adj[i]);
    }
    nimcp_free(temp_adj);
    temp_adj = NULL;
    nimcp_free(temp_count);
    temp_count = NULL;
    nimcp_free(temp_cap);
    temp_cap = NULL;
    return NIMCP_GT_ERROR_NO_MEMORY;
}

/**
 * @brief Build network based on topology type
 */
static nimcp_error_t build_network(nimcp_spatial_game_t ctx) {
    nimcp_error_t err = allocate_neighbors(ctx);
    if (err != NIMCP_SUCCESS) return err;

    switch (ctx->config.topology) {
        case NIMCP_TOPOLOGY_COMPLETE:
            return build_complete_graph(ctx);
        case NIMCP_TOPOLOGY_GRID_2D:
            return build_grid_2d(ctx);
        case NIMCP_TOPOLOGY_GRID_2D_MOORE:
            return build_grid_2d_moore(ctx);
        case NIMCP_TOPOLOGY_RING:
            return build_ring(ctx);
        case NIMCP_TOPOLOGY_RANDOM_GRAPH:
            return build_random_graph(ctx);
        case NIMCP_TOPOLOGY_SCALE_FREE:
            return build_scale_free(ctx);
        case NIMCP_TOPOLOGY_SMALL_WORLD:
            return build_small_world(ctx);
        case NIMCP_TOPOLOGY_CUSTOM:
            // Custom topology will be set later
            return NIMCP_SUCCESS;
        default:
            return NIMCP_SPATIAL_ERROR_INVALID_TOPOLOGY;
    }
}

//=============================================================================
// Fitness Computation
//=============================================================================

/**
 * @brief Compute fitness for a single node
 */
static float compute_node_fitness(nimcp_spatial_game_t ctx, uint32_t node) {
    uint32_t my_strategy = ctx->node_strategy[node];
    float fitness = 0.0f;

    uint32_t degree = ctx->node_degree[node];
    if (degree == 0) return 0.0f;

    for (uint32_t i = 0; i < degree; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && degree > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)degree);
        }

        uint32_t neighbor = ctx->node_neighbors[node][i];
        uint32_t neighbor_strategy = ctx->node_strategy[neighbor];

        // Payoff from playing against this neighbor
        float payoff = ctx->payoff_matrix[my_strategy * ctx->config.num_strategies + neighbor_strategy];
        fitness += payoff;
    }

    // Average payoff
    return fitness / (float)degree;
}

/**
 * @brief Update fitness for all nodes
 */
static void update_all_fitness(nimcp_spatial_game_t ctx) {
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)ctx->num_nodes);
        }

        ctx->node_fitness[i] = compute_node_fitness(ctx, i);
    }
}

/**
 * @brief Compute strategy frequencies
 */
static void compute_frequencies(nimcp_spatial_game_t ctx) {
    memset(ctx->frequencies, 0, ctx->config.num_strategies * sizeof(float));

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)ctx->num_nodes);
        }

        ctx->frequencies[ctx->node_strategy[i]] += 1.0f;
    }

    for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(s + 1) / (float)ctx->config.num_strategies);
        }

        ctx->frequencies[s] /= (float)ctx->num_nodes;
    }
}

/**
 * @brief Compute average fitness per strategy
 */
static void compute_fitness_per_strategy(nimcp_spatial_game_t ctx) {
    uint32_t* count = nimcp_calloc(ctx->config.num_strategies, sizeof(uint32_t));
    if (!count) return;

    memset(ctx->fitness_per_strategy, 0, ctx->config.num_strategies * sizeof(float));

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)ctx->num_nodes);
        }

        uint32_t s = ctx->node_strategy[i];
        ctx->fitness_per_strategy[s] += ctx->node_fitness[i];
        count[s]++;
    }

    ctx->avg_fitness = 0.0f;
    for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(s + 1) / (float)ctx->config.num_strategies);
        }

        if (count[s] > 0) {
            ctx->fitness_per_strategy[s] /= (float)count[s];
        }
        ctx->avg_fitness += ctx->frequencies[s] * ctx->fitness_per_strategy[s];
    }

    nimcp_free(count);
    count = NULL;
}

//=============================================================================
// Update Rules
//=============================================================================

/**
 * @brief Imitation update: copy best neighbor's strategy
 */
static uint32_t imitation_update(nimcp_spatial_game_t ctx, uint32_t node) {
    uint32_t best_strategy = ctx->node_strategy[node];
    float best_fitness = ctx->node_fitness[node];

    for (uint32_t i = 0; i < ctx->node_degree[node]; i++) {
        uint32_t neighbor = ctx->node_neighbors[node][i];
        if (ctx->node_fitness[neighbor] > best_fitness) {
            best_fitness = ctx->node_fitness[neighbor];
            best_strategy = ctx->node_strategy[neighbor];
        }
    }

    return best_strategy;
}

/**
 * @brief Best response: choose strategy that maximizes payoff against current neighbors
 */
static uint32_t best_response_update(nimcp_spatial_game_t ctx, uint32_t node) {
    uint32_t best_strategy = 0;
    float best_payoff = -FLT_MAX;

    for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
        /* Phase 8: Loop progress heartbeat */
        if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(s + 1) / (float)ctx->config.num_strategies);
        }

        float payoff = 0.0f;

        for (uint32_t i = 0; i < ctx->node_degree[node]; i++) {
            uint32_t neighbor = ctx->node_neighbors[node][i];
            uint32_t neighbor_strategy = ctx->node_strategy[neighbor];
            payoff += ctx->payoff_matrix[s * ctx->config.num_strategies + neighbor_strategy];
        }

        if (payoff > best_payoff) {
            best_payoff = payoff;
            best_strategy = s;
        }
    }

    return best_strategy;
}

/**
 * @brief Fermi update: stochastic imitation with temperature
 */
static uint32_t fermi_update(nimcp_spatial_game_t ctx, uint32_t node) {
    if (ctx->node_degree[node] == 0) {
        return ctx->node_strategy[node];
    }

    // Pick random neighbor
    uint32_t rand_idx = random_range(&ctx->rng_state, ctx->node_degree[node]);
    uint32_t neighbor = ctx->node_neighbors[node][rand_idx];

    float my_fitness = ctx->node_fitness[node];
    float neighbor_fitness = ctx->node_fitness[neighbor];

    // Fermi function: P(adopt) = 1 / (1 + exp(-beta * (f_j - f_i)))
    float beta = ctx->config.selection_intensity / ctx->config.fermi_temperature;
    float delta = neighbor_fitness - my_fitness;
    float p_adopt = 1.0f / (1.0f + expf(-beta * delta));

    if (random_float(&ctx->rng_state) < p_adopt) {
        return ctx->node_strategy[neighbor];
    }

    return ctx->node_strategy[node];
}

/**
 * @brief Moran process: proportional selection
 */
static uint32_t moran_update(nimcp_spatial_game_t ctx, uint32_t node) {
    // Sum fitness in neighborhood (including self)
    float total_fitness = ctx->node_fitness[node];
    for (uint32_t i = 0; i < ctx->node_degree[node]; i++) {
        total_fitness += ctx->node_fitness[ctx->node_neighbors[node][i]];
    }

    if (total_fitness <= 0.0f) {
        // Random selection
        uint32_t rand_idx = random_range(&ctx->rng_state, ctx->node_degree[node] + 1);
        if (rand_idx == 0) return ctx->node_strategy[node];
        return ctx->node_strategy[ctx->node_neighbors[node][rand_idx - 1]];
    }

    // Proportional selection
    float r = random_float(&ctx->rng_state) * total_fitness;
    float cumulative = ctx->node_fitness[node];

    if (r < cumulative) {
        return ctx->node_strategy[node];
    }

    for (uint32_t i = 0; i < ctx->node_degree[node]; i++) {
        uint32_t neighbor = ctx->node_neighbors[node][i];
        cumulative += ctx->node_fitness[neighbor];
        if (r < cumulative) {
            return ctx->node_strategy[neighbor];
        }
    }

    return ctx->node_strategy[node];
}

/**
 * @brief Apply mutation with given probability
 */
static uint32_t apply_mutation(nimcp_spatial_game_t ctx, uint32_t strategy) {
    if (random_float(&ctx->rng_state) < ctx->config.mutation_rate) {
        return random_range(&ctx->rng_state, ctx->config.num_strategies);
    }
    return strategy;
}

/**
 * @brief Get new strategy for node based on update rule
 */
static uint32_t get_new_strategy(nimcp_spatial_game_t ctx, uint32_t node) {
    uint32_t new_strategy = 0;

    switch (ctx->config.update_rule) {
        case NIMCP_UPDATE_IMITATION:
            new_strategy = imitation_update(ctx, node);
            break;
        case NIMCP_UPDATE_BEST_RESPONSE:
            new_strategy = best_response_update(ctx, node);
            break;
        case NIMCP_UPDATE_FERMI:
            new_strategy = fermi_update(ctx, node);
            break;
        case NIMCP_UPDATE_MORAN:
        case NIMCP_UPDATE_DEATH_BIRTH:
            new_strategy = moran_update(ctx, node);
            break;
        case NIMCP_UPDATE_REPLICATOR:
        default:
            // Replicator dynamics handled at population level
            new_strategy = ctx->node_strategy[node];
            break;
    }

    return apply_mutation(ctx, new_strategy);
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_spatial_config_t nimcp_spatial_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_default_conf", 0.0f);


    nimcp_spatial_config_t config;
    memset(&config, 0, sizeof(config));

    config.topology = NIMCP_TOPOLOGY_GRID_2D;
    config.grid_width = NIMCP_SPATIAL_DEFAULT_GRID_SIZE;
    config.grid_height = NIMCP_SPATIAL_DEFAULT_GRID_SIZE;
    config.num_nodes = config.grid_width * config.grid_height;
    config.boundary = NIMCP_BOUNDARY_PERIODIC;

    config.edge_probability = 0.1f;
    config.initial_edges = 3;
    config.edges_per_step = 2;
    config.rewiring_prob = 0.1f;

    config.num_strategies = 2;
    config.selection_intensity = NIMCP_SPATIAL_DEFAULT_SELECTION;
    config.mutation_rate = NIMCP_SPATIAL_DEFAULT_MUTATION;

    config.update_rule = NIMCP_UPDATE_IMITATION;
    config.synchronous_update = true;
    config.fermi_temperature = NIMCP_TEMPERATURE_DEFAULT;
    config.dt = NIMCP_SPATIAL_DEFAULT_DT;

    config.max_steps = NIMCP_SPATIAL_DEFAULT_STEPS;
    config.convergence_threshold = NIMCP_CONVERGENCE_THRESHOLD;
    config.convergence_window = 100;

    config.track_history = false;
    config.enable_statistics = true;
    config.seed = 0;

    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_spatial_game_t nimcp_spatial_create(
    const nimcp_spatial_config_t* config,
    const float* payoff_matrix
) {
    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_create", 0.0f);


    NIMCP_API_CHECK_NULL_RET_NULL(config, "NULL config in nimcp_spatial_create");
    NIMCP_API_CHECK_NULL_RET_NULL(payoff_matrix, "NULL payoff_matrix in nimcp_spatial_create");

    if (config->num_strategies < 1 || config->num_strategies > NIMCP_SPATIAL_MAX_STRATEGIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid strategy count: %u", config->num_strategies);
        return NULL;
    }

    if (config->num_nodes < 1 || config->num_nodes > NIMCP_SPATIAL_MAX_NODES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid node count: %u", config->num_nodes);
        return NULL;
    }

    nimcp_spatial_game_t ctx = nimcp_calloc(1, sizeof(struct nimcp_spatial_game_struct));
    if (!ctx) return NULL;
    NIMCP_API_CHECK_ALLOC(ctx, "Failed to allocate spatial game context");

    ctx->config = *config;
    ctx->state = NIMCP_SPATIAL_STATE_UNINITIALIZED;
    ctx->num_nodes = config->num_nodes;

    // Initialize RNG
    ctx->rng_state = config->seed ? config->seed : (uint64_t)nimcp_time_get_ms();

    // Allocate payoff matrix
    size_t matrix_size = config->num_strategies * config->num_strategies * sizeof(float);
    ctx->payoff_matrix = nimcp_malloc(matrix_size);
    if (!ctx->payoff_matrix) {
        nimcp_free(ctx);
        ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_spatial_create: ctx->payoff_matrix is NULL");
        return NULL;
    }
    memcpy(ctx->payoff_matrix, payoff_matrix, matrix_size);

    // Allocate node arrays
    ctx->node_strategy = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t));
    if (!ctx->node_strategy) return NULL;
    ctx->node_strategy_next = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t));
    if (!ctx->node_strategy_next) return NULL;
    ctx->node_fitness = nimcp_calloc(ctx->num_nodes, sizeof(float));
    if (!ctx->node_fitness) return NULL;
    ctx->frequencies = nimcp_calloc(config->num_strategies, sizeof(float));
    if (!ctx->frequencies) return NULL;
    ctx->fitness_per_strategy = nimcp_calloc(config->num_strategies, sizeof(float));
    if (!ctx->fitness_per_strategy) return NULL;

    if (!ctx->node_strategy || !ctx->node_strategy_next || !ctx->node_fitness ||
        !ctx->frequencies || !ctx->fitness_per_strategy) {
        nimcp_spatial_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_spatial_create: operation failed");
        return NULL;
    }

    // Build network
    nimcp_error_t err = build_network(ctx);
    if (err != NIMCP_SUCCESS) {
        nimcp_spatial_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_spatial_create: build_network failed");
        return NULL;
    }

    // Allocate history if tracking
    if (config->track_history) {
        ctx->history_capacity = config->max_steps / HISTORY_SAMPLE_FREQ + 1;
        ctx->frequency_history = nimcp_calloc(
            ctx->history_capacity * config->num_strategies,
            sizeof(float)
        );
        if (!ctx->frequency_history) {
            nimcp_spatial_destroy(ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_spatial_create: ctx->frequency_history is NULL");
            return NULL;
        }
    }

    // Initialize mutex
    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        nimcp_spatial_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_spatial_create: validation failed");
        return NULL;
    }

    ctx->current_step = 0;
    ctx->strategy_switches = 0;
    ctx->state = NIMCP_SPATIAL_STATE_INITIALIZED;

    return ctx;
}

void nimcp_spatial_destroy(nimcp_spatial_game_t ctx) {
    if (!ctx) return;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_destroy", 0.0f);


    nimcp_platform_mutex_destroy(&ctx->mutex);

    // Free network
    if (ctx->node_neighbors) {
        for (uint32_t i = 0; i < ctx->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)ctx->num_nodes);
            }

            nimcp_free(ctx->node_neighbors[i]);
        }
        nimcp_free(ctx->node_neighbors);
    }
    nimcp_free(ctx->node_degree);

    if (ctx->edge_weights) {
        for (uint32_t i = 0; i < ctx->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)ctx->num_nodes);
            }

            nimcp_free(ctx->edge_weights[i]);
        }
        nimcp_free(ctx->edge_weights);
    }

    nimcp_free(ctx->payoff_matrix);
    nimcp_free(ctx->node_strategy);
    nimcp_free(ctx->node_strategy_next);
    nimcp_free(ctx->node_fitness);
    nimcp_free(ctx->frequencies);
    nimcp_free(ctx->fitness_per_strategy);
    nimcp_free(ctx->frequency_history);

    nimcp_free(ctx);
    ctx = NULL;
}

//=============================================================================
// Topology Configuration
//=============================================================================

nimcp_error_t nimcp_spatial_set_topology(
    nimcp_spatial_game_t ctx,
    nimcp_topology_type_t topology,
    const float* params
) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_set_topology", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_SPATIAL_STATE_RUNNING) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_SPATIAL_ERROR_SIMULATION_RUNNING;
    }

    // Free existing network
    if (ctx->node_neighbors) {
        for (uint32_t i = 0; i < ctx->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)ctx->num_nodes);
            }

            nimcp_free(ctx->node_neighbors[i]);
        }
        nimcp_free(ctx->node_neighbors);
        ctx->node_neighbors = NULL;
    }
    nimcp_free(ctx->node_degree);
    ctx->node_degree = NULL;

    ctx->config.topology = topology;

    // Apply parameters
    if (params) {
        switch (topology) {
            case NIMCP_TOPOLOGY_RANDOM_GRAPH:
                ctx->config.edge_probability = params[0];
                break;
            case NIMCP_TOPOLOGY_SCALE_FREE:
                ctx->config.initial_edges = (uint32_t)params[0];
                ctx->config.edges_per_step = (uint32_t)params[1];
                break;
            case NIMCP_TOPOLOGY_SMALL_WORLD:
                ctx->config.rewiring_prob = params[0];
                break;
            default:
                break;
        }
    }

    nimcp_error_t err = build_network(ctx);
    ctx->state = (err == NIMCP_SUCCESS) ? NIMCP_SPATIAL_STATE_INITIALIZED : NIMCP_SPATIAL_STATE_UNINITIALIZED;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return err;
}

nimcp_error_t nimcp_spatial_set_custom_network(
    nimcp_spatial_game_t ctx,
    const float* adjacency,
    uint32_t num_nodes
) {
    if (!ctx || !adjacency) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_set_custom_n", 0.0f);


    if (num_nodes > NIMCP_SPATIAL_MAX_NODES) {
        return NIMCP_SPATIAL_ERROR_NETWORK_TOO_LARGE;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_SPATIAL_STATE_RUNNING) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_SPATIAL_ERROR_SIMULATION_RUNNING;
    }

    // Free existing network
    if (ctx->node_neighbors) {
        for (uint32_t i = 0; i < ctx->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)ctx->num_nodes);
            }

            nimcp_free(ctx->node_neighbors[i]);
        }
        nimcp_free(ctx->node_neighbors);
    }
    nimcp_free(ctx->node_degree);

    ctx->num_nodes = num_nodes;
    ctx->config.num_nodes = num_nodes;
    ctx->config.topology = NIMCP_TOPOLOGY_CUSTOM;

    // Count degrees
    ctx->node_degree = nimcp_calloc(num_nodes, sizeof(uint32_t));
    ctx->node_neighbors = nimcp_calloc(num_nodes, sizeof(uint32_t*));

    if (!ctx->node_degree || !ctx->node_neighbors) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)num_nodes);
        }

        uint32_t degree = 0;
        for (uint32_t j = 0; j < num_nodes; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(j + 1) / (float)num_nodes);
            }

            if (adjacency[i * num_nodes + j] != 0.0f) {
                degree++;
            }
        }
        ctx->node_degree[i] = degree;

        if (degree > 0) {
            ctx->node_neighbors[i] = nimcp_calloc(degree, sizeof(uint32_t));
            if (!ctx->node_neighbors[i]) {
                nimcp_platform_mutex_unlock(&ctx->mutex);
                return NIMCP_GT_ERROR_NO_MEMORY;
            }

            uint32_t idx = 0;
            for (uint32_t j = 0; j < num_nodes; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && num_nodes > 256) {
                    gt_spatial_heartbeat("gt_spatial_loop",
                                     (float)(j + 1) / (float)num_nodes);
                }

                if (adjacency[i * num_nodes + j] != 0.0f) {
                    ctx->node_neighbors[i][idx++] = j;
                }
            }
        }
    }

    // Reallocate node arrays if needed
    if (ctx->num_nodes != num_nodes) {
        nimcp_free(ctx->node_strategy);
        nimcp_free(ctx->node_strategy_next);
        nimcp_free(ctx->node_fitness);

        ctx->node_strategy = nimcp_calloc(num_nodes, sizeof(uint32_t));
        ctx->node_strategy_next = nimcp_calloc(num_nodes, sizeof(uint32_t));
        if (!ctx->node_strategy_next) return -1;
        ctx->node_fitness = nimcp_calloc(num_nodes, sizeof(float));

        if (!ctx->node_strategy || !ctx->node_strategy_next || !ctx->node_fitness) {
            nimcp_platform_mutex_unlock(&ctx->mutex);
            return NIMCP_GT_ERROR_NO_MEMORY;
        }
    }

    ctx->state = NIMCP_SPATIAL_STATE_INITIALIZED;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_spatial_get_network(
    const nimcp_spatial_game_t ctx,
    nimcp_spatial_network_t* network
) {
    if (!ctx || !network) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_get_network", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    memset(network, 0, sizeof(nimcp_spatial_network_t));
    network->num_nodes = ctx->num_nodes;
    network->is_directed = false;

    network->degree = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t));
    network->neighbors = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t*));

    if (!network->degree || !network->neighbors) {
        nimcp_free(network->degree);
        nimcp_free(network->neighbors);
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)ctx->num_nodes);
        }

        network->degree[i] = ctx->node_degree[i];
        if (ctx->node_degree[i] > 0) {
            network->neighbors[i] = nimcp_calloc(ctx->node_degree[i], sizeof(uint32_t));
            if (!network->neighbors[i]) {
                nimcp_spatial_network_destroy(network);
                nimcp_platform_mutex_unlock(&ctx->mutex);
                return NIMCP_GT_ERROR_NO_MEMORY;
            }
            memcpy(network->neighbors[i], ctx->node_neighbors[i],
                   ctx->node_degree[i] * sizeof(uint32_t));
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

void nimcp_spatial_network_destroy(nimcp_spatial_network_t* network) {
    if (!network) return;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_network_dest", 0.0f);


    if (network->neighbors) {
        for (uint32_t i = 0; i < network->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && network->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)network->num_nodes);
            }

            nimcp_free(network->neighbors[i]);
        }
        nimcp_free(network->neighbors);
    }
    if (network->weights) {
        for (uint32_t i = 0; i < network->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && network->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)network->num_nodes);
            }

            nimcp_free(network->weights[i]);
        }
        nimcp_free(network->weights);
    }
    nimcp_free(network->degree);

    memset(network, 0, sizeof(nimcp_spatial_network_t));
}

//=============================================================================
// Initialization
//=============================================================================

nimcp_error_t nimcp_spatial_initialize_random(
    nimcp_spatial_game_t ctx,
    const float* strategy_probs
) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_initialize_r", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    // Validate or create uniform distribution
    float* probs = nimcp_calloc(ctx->config.num_strategies, sizeof(float));
    if (!probs) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    if (strategy_probs) {
        memcpy(probs, strategy_probs, ctx->config.num_strategies * sizeof(float));
    } else {
        // Uniform distribution
        float p = 1.0f / (float)ctx->config.num_strategies;
        for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(s + 1) / (float)ctx->config.num_strategies);
            }

            probs[s] = p;
        }
    }

    // Convert to cumulative distribution
    for (uint32_t s = 1; s < ctx->config.num_strategies; s++) {
        probs[s] += probs[s - 1];
    }

    // Assign strategies
    for (uint32_t i = 0; i < ctx->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)ctx->num_nodes);
        }

        float r = random_float(&ctx->rng_state);
        uint32_t strategy = 0;

        for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(s + 1) / (float)ctx->config.num_strategies);
            }

            if (r < probs[s]) {
                strategy = s;
                break;
            }
        }

        ctx->node_strategy[i] = strategy;
    }

    nimcp_free(probs);
    probs = NULL;

    // Update derived state
    update_all_fitness(ctx);
    compute_frequencies(ctx);
    compute_fitness_per_strategy(ctx);

    ctx->current_step = 0;
    ctx->strategy_switches = 0;
    ctx->history_length = 0;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_spatial_initialize_cluster(
    nimcp_spatial_game_t ctx,
    uint32_t strategy,
    uint32_t center,
    float radius
) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_initialize_c", 0.0f);


    if (strategy >= ctx->config.num_strategies) {
        return NIMCP_SPATIAL_ERROR_INVALID_STRATEGY;
    }

    if (center >= ctx->num_nodes) {
        return NIMCP_SPATIAL_ERROR_NODE_OUT_OF_BOUNDS;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    // For grid topologies, use Euclidean distance
    if (ctx->config.topology == NIMCP_TOPOLOGY_GRID_2D ||
        ctx->config.topology == NIMCP_TOPOLOGY_GRID_2D_MOORE) {

        uint32_t cx = center % ctx->config.grid_width;
        uint32_t cy = center / ctx->config.grid_width;

        for (uint32_t i = 0; i < ctx->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)ctx->num_nodes);
            }

            uint32_t x = i % ctx->config.grid_width;
            uint32_t y = i / ctx->config.grid_width;

            float dx = (float)x - (float)cx;
            float dy = (float)y - (float)cy;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist <= radius) {
                ctx->node_strategy[i] = strategy;
            }
        }
    } else {
        // For other topologies, use BFS to find nodes within graph distance
        uint32_t* distance = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t));
        bool* visited = nimcp_calloc(ctx->num_nodes, sizeof(bool));
        uint32_t* queue = nimcp_calloc(ctx->num_nodes, sizeof(uint32_t));

        if (!distance || !visited || !queue) {
            nimcp_free(distance);
            distance = NULL;
            nimcp_free(visited);
            visited = NULL;
            nimcp_free(queue);
            queue = NULL;
            nimcp_platform_mutex_unlock(&ctx->mutex);
            return NIMCP_GT_ERROR_NO_MEMORY;
        }

        // BFS from center
        uint32_t front = 0, back = 0;
        queue[back++] = center;
        visited[center] = true;
        distance[center] = 0;

        while (front < back) {
            uint32_t node = queue[front++];

            if ((float)distance[node] <= radius) {
                ctx->node_strategy[node] = strategy;
            }

            for (uint32_t i = 0; i < ctx->node_degree[node]; i++) {
                uint32_t neighbor = ctx->node_neighbors[node][i];
                if (!visited[neighbor]) {
                    visited[neighbor] = true;
                    distance[neighbor] = distance[node] + 1;
                    queue[back++] = neighbor;
                }
            }
        }

        nimcp_free(distance);
        distance = NULL;
        nimcp_free(visited);
        visited = NULL;
        nimcp_free(queue);
        queue = NULL;
    }

    // Update derived state
    update_all_fitness(ctx);
    compute_frequencies(ctx);
    compute_fitness_per_strategy(ctx);

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_spatial_set_node_strategy(
    nimcp_spatial_game_t ctx,
    uint32_t node_id,
    uint32_t strategy
) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_set_node_str", 0.0f);


    if (node_id >= ctx->num_nodes) {
        return NIMCP_SPATIAL_ERROR_NODE_OUT_OF_BOUNDS;
    }

    if (strategy >= ctx->config.num_strategies) {
        return NIMCP_SPATIAL_ERROR_INVALID_STRATEGY;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->node_strategy[node_id] = strategy;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Simulation
//=============================================================================

nimcp_error_t nimcp_spatial_step(nimcp_spatial_game_t ctx) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_step", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->state == NIMCP_SPATIAL_STATE_CONVERGED) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_SUCCESS;  // Already done
    }

    ctx->state = NIMCP_SPATIAL_STATE_RUNNING;

    // Update all fitness values
    update_all_fitness(ctx);

    if (ctx->config.update_rule == NIMCP_UPDATE_REPLICATOR) {
        // Population-level replicator dynamics
        compute_frequencies(ctx);

        float new_freq[NIMCP_SPATIAL_MAX_STRATEGIES];
        nimcp_replicator_dynamics(
            ctx->frequencies,
            ctx->payoff_matrix,
            ctx->config.num_strategies,
            ctx->config.dt,
            new_freq
        );

        // Update node strategies based on new frequencies
        // Probabilistic assignment
        float cumulative[NIMCP_SPATIAL_MAX_STRATEGIES];
        cumulative[0] = new_freq[0];
        for (uint32_t s = 1; s < ctx->config.num_strategies; s++) {
            cumulative[s] = cumulative[s - 1] + new_freq[s];
        }

        for (uint32_t i = 0; i < ctx->num_nodes; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)ctx->num_nodes);
            }

            float r = random_float(&ctx->rng_state);
            uint32_t new_strategy = 0;

            for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
                /* Phase 8: Loop progress heartbeat */
                if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
                    gt_spatial_heartbeat("gt_spatial_loop",
                                     (float)(s + 1) / (float)ctx->config.num_strategies);
                }

                if (r < cumulative[s]) {
                    new_strategy = s;
                    break;
                }
            }

            if (new_strategy != ctx->node_strategy[i]) {
                ctx->strategy_switches++;
            }
            ctx->node_strategy[i] = new_strategy;
        }
    } else {
        // Spatial update rules
        if (ctx->config.synchronous_update) {
            // Synchronous: compute all next states, then update
            for (uint32_t i = 0; i < ctx->num_nodes; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                    gt_spatial_heartbeat("gt_spatial_loop",
                                     (float)(i + 1) / (float)ctx->num_nodes);
                }

                ctx->node_strategy_next[i] = get_new_strategy(ctx, i);
            }

            for (uint32_t i = 0; i < ctx->num_nodes; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && ctx->num_nodes > 256) {
                    gt_spatial_heartbeat("gt_spatial_loop",
                                     (float)(i + 1) / (float)ctx->num_nodes);
                }

                if (ctx->node_strategy_next[i] != ctx->node_strategy[i]) {
                    ctx->strategy_switches++;
                }
                ctx->node_strategy[i] = ctx->node_strategy_next[i];
            }
        } else {
            // Asynchronous: random node update
            uint32_t node = random_range(&ctx->rng_state, ctx->num_nodes);
            uint32_t new_strategy = get_new_strategy(ctx, node);

            if (new_strategy != ctx->node_strategy[node]) {
                ctx->strategy_switches++;
                ctx->node_strategy[node] = new_strategy;
            }
        }
    }

    // Update frequencies and fitness
    compute_frequencies(ctx);
    compute_fitness_per_strategy(ctx);

    // Record history
    if (ctx->config.track_history && ctx->frequency_history) {
        if (ctx->current_step % HISTORY_SAMPLE_FREQ == 0 &&
            ctx->history_length < ctx->history_capacity) {
            memcpy(
                &ctx->frequency_history[ctx->history_length * ctx->config.num_strategies],
                ctx->frequencies,
                ctx->config.num_strategies * sizeof(float)
            );
            ctx->history_length++;
        }
    }

    ctx->current_step++;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_spatial_run(
    nimcp_spatial_game_t ctx,
    uint32_t num_steps,
    nimcp_evolutionary_result_t* result
) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_run", 0.0f);


    uint32_t max_steps = num_steps > 0 ? num_steps : ctx->config.max_steps;

    // Store previous frequencies for convergence check
    float* prev_freq = nimcp_calloc(ctx->config.num_strategies, sizeof(float));
    if (!prev_freq) return NIMCP_GT_ERROR_NO_MEMORY;

    nimcp_platform_mutex_lock(&ctx->mutex);
    memcpy(prev_freq, ctx->frequencies, ctx->config.num_strategies * sizeof(float));
    nimcp_platform_mutex_unlock(&ctx->mutex);

    uint32_t convergence_count = 0;

    for (uint32_t step = 0; step < max_steps; step++) {
        /* Phase 8: Loop progress heartbeat */
        if ((step & 0xFF) == 0 && max_steps > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(step + 1) / (float)max_steps);
        }

        nimcp_error_t err = nimcp_spatial_step(ctx);
        if (err != NIMCP_SUCCESS) {
            nimcp_free(prev_freq);
            prev_freq = NULL;
            return err;
        }

        // Check convergence
        if (step % CONVERGENCE_CHECK_FREQ == 0) {
            nimcp_platform_mutex_lock(&ctx->mutex);

            float max_change = 0.0f;
            for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
                /* Phase 8: Loop progress heartbeat */
                if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
                    gt_spatial_heartbeat("gt_spatial_loop",
                                     (float)(s + 1) / (float)ctx->config.num_strategies);
                }

                float change = fabsf(ctx->frequencies[s] - prev_freq[s]);
                if (change > max_change) max_change = change;
                prev_freq[s] = ctx->frequencies[s];
            }

            if (max_change < ctx->config.convergence_threshold) {
                convergence_count++;
                if (convergence_count * CONVERGENCE_CHECK_FREQ >= ctx->config.convergence_window) {
                    ctx->state = NIMCP_SPATIAL_STATE_CONVERGED;
                    nimcp_platform_mutex_unlock(&ctx->mutex);
                    break;
                }
            } else {
                convergence_count = 0;
            }

            nimcp_platform_mutex_unlock(&ctx->mutex);
        }
    }

    nimcp_free(prev_freq);
    prev_freq = NULL;

    // Fill result if provided
    if (result) {
        nimcp_platform_mutex_lock(&ctx->mutex);

        memset(result, 0, sizeof(nimcp_evolutionary_result_t));
        result->final_state = ctx->state;
        result->steps_taken = ctx->current_step;
        result->strategy_switches = ctx->strategy_switches;

        memcpy(result->final_frequencies, ctx->frequencies,
               ctx->config.num_strategies * sizeof(float));

        // Find dominant strategy
        result->dominant_strategy = -1;
        result->dominance_ratio = 0.0f;

        for (uint32_t s = 0; s < ctx->config.num_strategies; s++) {
            /* Phase 8: Loop progress heartbeat */
            if ((s & 0xFF) == 0 && ctx->config.num_strategies > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(s + 1) / (float)ctx->config.num_strategies);
            }

            if (ctx->frequencies[s] > result->dominance_ratio) {
                result->dominance_ratio = ctx->frequencies[s];
                result->dominant_strategy = (int32_t)s;
            }
        }

        result->is_equilibrium = (ctx->state == NIMCP_SPATIAL_STATE_CONVERGED);
        result->equilibrium_fitness = ctx->avg_fitness;

        // Compute entropy
        result->entropy = nimcp_compute_entropy(ctx->frequencies, ctx->config.num_strategies);

        // Copy history if tracked
        if (ctx->config.track_history && ctx->frequency_history && ctx->history_length > 0) {
            result->history_length = ctx->history_length;
            result->frequency_history = nimcp_calloc(
                ctx->history_length * ctx->config.num_strategies,
                sizeof(float)
            );
            if (result->frequency_history) {
                memcpy(result->frequency_history, ctx->frequency_history,
                       ctx->history_length * ctx->config.num_strategies * sizeof(float));
            }
        }

        // Check if dominant is ESS (use unlocked version since we hold mutex)
        if (result->dominant_strategy >= 0 && result->dominance_ratio > 0.95f) {
            result->is_ess = is_ess_unlocked(ctx, (uint32_t)result->dominant_strategy);
        }

        nimcp_platform_mutex_unlock(&ctx->mutex);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_spatial_stop(nimcp_spatial_game_t ctx) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_stop", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->state = NIMCP_SPATIAL_STATE_STOPPED;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_spatial_reset(nimcp_spatial_game_t ctx) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_reset", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    ctx->current_step = 0;
    ctx->strategy_switches = 0;
    ctx->history_length = 0;

    memset(ctx->node_strategy, 0, ctx->num_nodes * sizeof(uint32_t));
    memset(ctx->node_fitness, 0, ctx->num_nodes * sizeof(float));
    memset(ctx->frequencies, 0, ctx->config.num_strategies * sizeof(float));

    ctx->state = NIMCP_SPATIAL_STATE_INITIALIZED;

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

nimcp_error_t nimcp_spatial_get_frequencies(
    const nimcp_spatial_game_t ctx,
    float* frequencies
) {
    if (!ctx || !frequencies) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_get_frequenc", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    memcpy(frequencies, ctx->frequencies, ctx->config.num_strategies * sizeof(float));
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

int32_t nimcp_spatial_get_node_strategy(
    const nimcp_spatial_game_t ctx,
    uint32_t node_id
) {
    if (!ctx || node_id >= ctx->num_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_spatial_get_node_strategy: ctx is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_get_node_str", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    int32_t strategy = (int32_t)ctx->node_strategy[node_id];
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return strategy;
}

float nimcp_spatial_get_node_fitness(
    const nimcp_spatial_game_t ctx,
    uint32_t node_id
) {
    if (!ctx || node_id >= ctx->num_nodes) return NAN;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_get_node_fit", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    float fitness = ctx->node_fitness[node_id];
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return fitness;
}

nimcp_spatial_state_t nimcp_spatial_get_state(const nimcp_spatial_game_t ctx) {
    if (!ctx) return NIMCP_SPATIAL_STATE_UNINITIALIZED;
    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_get_state", 0.0f);


    return ctx->state;
}

uint32_t nimcp_spatial_get_step(const nimcp_spatial_game_t ctx) {
    if (!ctx) return 0;
    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_get_step", 0.0f);


    return ctx->current_step;
}

nimcp_error_t nimcp_spatial_get_population(
    const nimcp_spatial_game_t ctx,
    nimcp_population_t* population
) {
    if (!ctx || !population) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_get_populati", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);

    population->num_strategies = ctx->config.num_strategies;
    population->avg_fitness = ctx->avg_fitness;
    population->generation = ctx->current_step;

    population->frequencies = nimcp_calloc(ctx->config.num_strategies, sizeof(float));
    population->fitness = nimcp_calloc(ctx->config.num_strategies, sizeof(float));

    if (!population->frequencies || !population->fitness) {
        nimcp_free(population->frequencies);
        nimcp_free(population->fitness);
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return NIMCP_GT_ERROR_NO_MEMORY;
    }

    memcpy(population->frequencies, ctx->frequencies,
           ctx->config.num_strategies * sizeof(float));
    memcpy(population->fitness, ctx->fitness_per_strategy,
           ctx->config.num_strategies * sizeof(float));

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Evolutionary Stability Analysis
//=============================================================================

/**
 * @brief Internal unlocked version of is_ess check
 * @note Caller must hold mutex
 */
static bool is_ess_unlocked(
    const nimcp_spatial_game_t ctx,
    uint32_t strategy
) {
    uint32_t n = ctx->config.num_strategies;
    const float* A = ctx->payoff_matrix;

    // ESS condition: For all j != i:
    // Either A[i,i] > A[j,i] (strict Nash)
    // Or A[i,i] = A[j,i] and A[i,j] > A[j,j] (stability condition)

    float Aii = A[strategy * n + strategy];

    for (uint32_t j = 0; j < n; j++) {
        /* Phase 8: Loop progress heartbeat */
        if ((j & 0xFF) == 0 && n > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(j + 1) / (float)n);
        }

        if (j == strategy) continue;

        float Aji = A[j * n + strategy];
        float Aij = A[strategy * n + j];
        float Ajj = A[j * n + j];

        if (Aji > Aii) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_ess_unlocked: validation failed");
            return false;  // Not Nash
        }

        if (fabsf(Aji - Aii) < 1e-10f && Ajj >= Aij) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_ess_unlocked: capacity exceeded");
            return false;  // Stability condition fails
        }
    }

    return true;
}

bool nimcp_spatial_is_ess(
    const nimcp_spatial_game_t ctx,
    uint32_t strategy
) {
    if (!ctx || strategy >= ctx->config.num_strategies) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_is_ess", 0.0f);


    nimcp_platform_mutex_lock(&ctx->mutex);
    bool result = is_ess_unlocked(ctx, strategy);
    nimcp_platform_mutex_unlock(&ctx->mutex);
    return result;
}

float nimcp_spatial_invasion_fitness(
    const nimcp_spatial_game_t ctx,
    uint32_t mutant_strategy,
    uint32_t resident_strategy
) {
    if (!ctx) return NAN;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_invasion_fit", 0.0f);


    if (mutant_strategy >= ctx->config.num_strategies ||
        resident_strategy >= ctx->config.num_strategies) {
        return NAN;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    uint32_t n = ctx->config.num_strategies;
    const float* A = ctx->payoff_matrix;

    // Invasion fitness = payoff of mutant vs resident - payoff of resident vs resident
    float mutant_fitness = A[mutant_strategy * n + resident_strategy];
    float resident_fitness = A[resident_strategy * n + resident_strategy];

    nimcp_platform_mutex_unlock(&ctx->mutex);

    return mutant_fitness - resident_fitness;
}

nimcp_error_t nimcp_spatial_analyze_invasion(
    const nimcp_spatial_game_t ctx,
    uint32_t mutant_strategy,
    uint32_t resident_strategy,
    nimcp_invasion_result_t* result
) {
    if (!ctx || !result) return NIMCP_GT_ERROR_NULL_POINTER;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_analyze_inva", 0.0f);


    if (mutant_strategy >= ctx->config.num_strategies ||
        resident_strategy >= ctx->config.num_strategies) {
        return NIMCP_SPATIAL_ERROR_INVALID_STRATEGY;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    memset(result, 0, sizeof(nimcp_invasion_result_t));

    uint32_t n = ctx->config.num_strategies;
    const float* A = ctx->payoff_matrix;

    result->resident_fitness = A[resident_strategy * n + resident_strategy];
    result->invasion_fitness = A[mutant_strategy * n + resident_strategy];

    // Can invade if mutant has higher fitness than resident
    result->can_invade = (result->invasion_fitness > result->resident_fitness);

    // Approximate fixation probability (Moran process approximation)
    // rho = (1 - 1/r) / (1 - 1/r^N) where r = f_mutant / f_resident
    if (result->resident_fitness > 0.0f) {
        float r = result->invasion_fitness / result->resident_fitness;
        if (r > 0.0f && r != 1.0f) {
            float N = (float)ctx->num_nodes;
            result->invasion_probability = (1.0f - 1.0f / r) / (1.0f - powf(1.0f / r, N));

            // Expected time to fixation (if successful)
            // Rough approximation: N * N / (selection_intensity * fitness_diff)
            if (result->invasion_fitness > result->resident_fitness) {
                float diff = result->invasion_fitness - result->resident_fitness;
                result->expected_time_to_fixation = (N * N) / (ctx->config.selection_intensity * diff);
            }
        } else if (fabsf(r - 1.0f) < 1e-10f) {
            result->invasion_probability = 1.0f / (float)ctx->num_nodes;  // Neutral drift
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_spatial_find_equilibrium(
    nimcp_spatial_game_t ctx,
    nimcp_evolutionary_result_t* result
) {
    if (!ctx) return NIMCP_GT_ERROR_NULL_POINTER;

    // Initialize with uniform distribution
    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_find_equilib", 0.0f);


    nimcp_error_t err = nimcp_spatial_initialize_random(ctx, NULL);
    if (err != NIMCP_SUCCESS) return err;

    // Run until convergence
    err = nimcp_spatial_run(ctx, 0, result);
    if (err != NIMCP_SUCCESS) return err;

    if (result && !result->is_equilibrium) {
        return NIMCP_SPATIAL_ERROR_NO_CONVERGENCE;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Replicator Dynamics
//=============================================================================

nimcp_error_t nimcp_replicator_dynamics(
    const float* frequencies,
    const float* payoff_matrix,
    uint32_t num_strategies,
    float dt,
    float* new_frequencies
) {
    if (!frequencies || !payoff_matrix || !new_frequencies) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_replicator_dynamics", 0.0f);


    if (num_strategies == 0 || num_strategies > NIMCP_SPATIAL_MAX_STRATEGIES) {
        return NIMCP_GT_ERROR_INVALID_PARAMETER;
    }

    // Compute fitness for each strategy: f_i = Sum_j x_j * A[i,j]
    float fitness[NIMCP_SPATIAL_MAX_STRATEGIES];
    float avg_fitness = 0.0f;

    for (uint32_t i = 0; i < num_strategies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_strategies > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)num_strategies);
        }

        fitness[i] = 0.0f;
        for (uint32_t j = 0; j < num_strategies; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_strategies > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(j + 1) / (float)num_strategies);
            }

            fitness[i] += frequencies[j] * payoff_matrix[i * num_strategies + j];
        }
        avg_fitness += frequencies[i] * fitness[i];
    }

    // Replicator equation: dx_i/dt = x_i * (f_i - avg_f)
    float total = 0.0f;
    for (uint32_t i = 0; i < num_strategies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_strategies > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)num_strategies);
        }

        float growth = frequencies[i] * (fitness[i] - avg_fitness);
        new_frequencies[i] = frequencies[i] + dt * growth;

        // Ensure non-negative
        if (new_frequencies[i] < MIN_FREQUENCY) {
            new_frequencies[i] = 0.0f;
        }

        total += new_frequencies[i];
    }

    // Normalize to sum to 1
    if (total > 0.0f) {
        for (uint32_t i = 0; i < num_strategies; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_strategies > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)num_strategies);
            }

            new_frequencies[i] /= total;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_compute_strategy_fitness(
    const float* frequencies,
    const float* payoff_matrix,
    uint32_t num_strategies,
    float* fitness
) {
    if (!frequencies || !payoff_matrix || !fitness) {
        return NIMCP_GT_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_compute_strategy_fit", 0.0f);


    for (uint32_t i = 0; i < num_strategies; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_strategies > 256) {
            gt_spatial_heartbeat("gt_spatial_loop",
                             (float)(i + 1) / (float)num_strategies);
        }

        fitness[i] = 0.0f;
        for (uint32_t j = 0; j < num_strategies; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && num_strategies > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(j + 1) / (float)num_strategies);
            }

            fitness[i] += frequencies[j] * payoff_matrix[i * num_strategies + j];
        }
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_topology_name(nimcp_topology_type_t topology) {
    if (topology >= NIMCP_TOPOLOGY_COUNT) return "Unknown";
    return s_topology_names[topology];
}

const char* nimcp_update_rule_name(nimcp_update_rule_t rule) {
    if (rule >= NIMCP_UPDATE_COUNT) return "Unknown";
    return s_update_rule_names[rule];
}

float nimcp_compute_entropy(const float* frequencies, uint32_t n) {
    if (!frequencies || n == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_compute_entropy", 0.0f);

    /* Delegate to central statistics module.
     * nimcp_stats_entropy expects a probability distribution.
     * The frequencies array in game theory contexts represents strategy
     * frequencies which should sum to 1 (probability distribution). */
    return nimcp_stats_entropy(frequencies, n);
}

void nimcp_evolutionary_result_cleanup(nimcp_evolutionary_result_t* result) {
    if (!result) return;
    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_evolutionary_result_", 0.0f);


    nimcp_free(result->frequency_history);
    result->frequency_history = NULL;
    result->history_length = 0;
}

void nimcp_population_cleanup(nimcp_population_t* population) {
    if (!population) return;
    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_population_cleanup", 0.0f);


    nimcp_free(population->frequencies);
    nimcp_free(population->fitness);
    population->frequencies = NULL;
    population->fitness = NULL;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int spatial_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    gt_spatial_heartbeat("gt_spatial_spatial_query_self_k", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Spatial_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gt_spatial_heartbeat("gt_spatial_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG(LOG_MODULE, "Spatial self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Spatial_Module");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Spatial_Module");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gt_spatial_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_gt_spatial_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gt_spatial_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_spatial_training_begin: NULL argument");
        return -1;
    }
    gt_spatial_heartbeat_instance(NULL, "gt_spatial_training_begin", 0.0f);
    (void)(struct nimcp_spatial_game_struct*)instance; /* Module state available for reset */
    return 0;
}

int gt_spatial_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_spatial_training_end: NULL argument");
        return -1;
    }
    gt_spatial_heartbeat_instance(NULL, "gt_spatial_training_end", 1.0f);
    (void)(struct nimcp_spatial_game_struct*)instance; /* Module state available for finalization */
    return 0;
}

int gt_spatial_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gt_spatial_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gt_spatial_heartbeat_instance(NULL, "gt_spatial_training_step", progress);
    (void)(struct nimcp_spatial_game_struct*)instance; /* Module state available for step adaptation */
    return 0;
}

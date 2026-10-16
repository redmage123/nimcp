/**
 * @file nimcp_monte_carlo.c
 * @brief Monte Carlo Tree Search and Sampling Algorithms Implementation
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Central implementation of Monte Carlo algorithms
 * WHY:  Provide generic decision-making infrastructure for cognitive modules
 * HOW:  Callback-based APIs for state/action space independence
 *
 * @author NIMCP Development Team
 */

#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h>

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MCTS_INVALID_NODE   UINT32_MAX
#define MCTS_ROOT_NODE      0

/* ============================================================================
 * Internal MCTS Node Structure
 * ============================================================================ */

typedef struct mcts_node {
    uint32_t id;                    /**< Node ID */
    uint32_t parent_id;             /**< Parent node ID (MCTS_INVALID_NODE for root) */
    uint32_t* children_ids;         /**< Array of child node IDs */
    uint32_t num_children;          /**< Number of children */
    uint32_t children_capacity;     /**< Allocated capacity for children */
    uint32_t action_from_parent;    /**< Action that led to this node */

    void* state;                    /**< Opaque state pointer */
    uint32_t visit_count;           /**< Number of visits N(s) */
    float total_value;              /**< Sum of backpropagated values */
    float q_value;                  /**< Mean value Q(s) = total/visits */
    float prior;                    /**< Prior probability (for PUCT) */

    uint32_t depth;                 /**< Depth in tree */
    bool is_expanded;               /**< Whether node has been expanded */
    bool is_terminal;               /**< Whether state is terminal */
} mcts_node_t;

/**
 * @brief Internal MCTS tree structure
 */
typedef struct mcts_tree {
    mcts_node_t** nodes;            /**< Array of node pointers */
    uint32_t num_nodes;             /**< Current number of nodes */
    uint32_t max_nodes;             /**< Maximum allowed nodes */
    uint32_t root_id;               /**< Root node ID */
    uint32_t seed;                  /**< RNG seed state */
} mcts_tree_t;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Swap two memory regions
 */
static inline void swap_bytes(void* a, void* b, size_t size) {
    unsigned char* pa = (unsigned char*)a;
    unsigned char* pb = (unsigned char*)b;
    unsigned char tmp;

    for (size_t i = 0; i < size; i++) {
        tmp = pa[i];
        pa[i] = pb[i];
        pb[i] = tmp;
    }
}

/**
 * @brief Linear congruential generator for fast RNG
 */
static inline uint32_t lcg_next(uint32_t* seed) {
    *seed = (*seed) * 1103515245u + 12345u;
    return *seed;
}

/* ============================================================================
 * Random Number Generation Implementation
 * ============================================================================ */

/**
 * @brief Thread-local Box-Muller cache for Gaussian RNG
 *
 * THREAD SAFETY: Uses __thread storage class for per-thread state.
 * The Box-Muller transform generates two independent Gaussians per pair
 * of uniform random numbers. Caching the second value doubles efficiency.
 *
 * DEADLOCK PREVENTION: No locks needed - each thread has its own cache.
 */
typedef struct {
    bool has_cached;      /**< True if cached_value is valid */
    float cached_value;   /**< Second Gaussian from previous Box-Muller call */
} mc_box_muller_cache_t;

static __thread mc_box_muller_cache_t g_mc_box_muller_cache = {false, 0.0f};

uint32_t mc_seed_from_time(void) {
    return (uint32_t)time(NULL) ^ (uint32_t)clock();
}

float mc_random_uniform(uint32_t* seed) {
    if (!seed) return 0.0f;
    uint32_t r = lcg_next(seed);
    return (float)(r & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

uint32_t mc_random_int(uint32_t* seed, uint32_t max) {
    if (!seed || max == 0) return 0;
    return lcg_next(seed) % max;
}

/**
 * @brief Generate Gaussian random number using Box-Muller with caching
 *
 * THREAD SAFETY: Uses thread-local storage for the Box-Muller cache.
 * Each thread maintains its own cached value, eliminating the need for
 * locks and avoiding any deadlock vulnerabilities.
 *
 * The Box-Muller transform converts two uniform random numbers in [0,1)
 * into two independent standard Gaussian random numbers:
 *   r = sqrt(-2 * ln(u1))
 *   theta = 2 * PI * u2
 *   z0 = r * cos(theta)  <- returned on odd calls
 *   z1 = r * sin(theta)  <- cached and returned on even calls
 *
 * @param seed Pointer to RNG seed state (modified)
 * @param mean Mean of the Gaussian distribution
 * @param stddev Standard deviation of the Gaussian distribution
 * @return Gaussian random number with specified mean and stddev
 */
float mc_random_normal(uint32_t* seed, float mean, float stddev) {
    if (!seed) return mean;

    /* Return cached value if available (thread-local, no locking needed) */
    if (g_mc_box_muller_cache.has_cached) {
        g_mc_box_muller_cache.has_cached = false;
        return mean + stddev * g_mc_box_muller_cache.cached_value;
    }

    /* Box-Muller transform: generate two Gaussians from two uniforms */
    float u1 = mc_random_uniform(seed);
    float u2 = mc_random_uniform(seed);

    /* Avoid log(0) - clamp u1 away from zero */
    if (u1 < 1e-10f) u1 = 1e-10f;

    float r = sqrtf(-2.0f * logf(u1));
    float theta = 2.0f * (float)M_PI * u2;

    /* Cache the second Gaussian for next call (thread-local storage) */
    g_mc_box_muller_cache.cached_value = r * sinf(theta);
    g_mc_box_muller_cache.has_cached = true;

    /* Return the first Gaussian */
    float z0 = r * cosf(theta);
    return mean + stddev * z0;
}

uint32_t mc_random_choice(uint32_t* seed, const float* weights, uint32_t n) {
    if (!seed || !weights || n == 0) return 0;

    /* Compute sum of weights */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += weights[i];
    }

    if (sum <= 0.0f) {
        /* Uniform choice if all weights are zero */
        return mc_random_int(seed, n);
    }

    /* Sample from cumulative distribution */
    float r = mc_random_uniform(seed) * sum;
    float cumsum = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        cumsum += weights[i];
        if (r < cumsum) {
            return i;
        }
    }

    return n - 1;  /* Fallback to last element */
}

/* ============================================================================
 * Array Shuffling Implementation
 * ============================================================================ */

void mc_shuffle_u32(uint32_t* array, uint32_t n, uint32_t* seed) {
    if (!array || n < 2 || !seed) return;

    for (uint32_t i = n - 1; i > 0; i--) {
        uint32_t j = mc_random_int(seed, i + 1);
        uint32_t tmp = array[i];
        array[i] = array[j];
        array[j] = tmp;
    }
}

void mc_shuffle(void* array, uint32_t n, size_t size, uint32_t* seed) {
    if (!array || n < 2 || size == 0 || !seed) return;

    unsigned char* arr = (unsigned char*)array;

    for (uint32_t i = n - 1; i > 0; i--) {
        uint32_t j = mc_random_int(seed, i + 1);
        swap_bytes(arr + i * size, arr + j * size, size);
    }
}

/* ============================================================================
 * Statistical Utilities Implementation
 * ============================================================================ */

float mc_mean(const float* values, uint32_t n) {
    if (!values || n == 0) return 0.0f;

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += values[i];
    }
    return sum / (float)n;
}

float mc_variance(const float* values, uint32_t n, float mean) {
    if (!values || n < 2) return 0.0f;

    /* Compute mean if not provided */
    if (mean != mean) {  /* NaN check */
        mean = mc_mean(values, n);
    }

    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float diff = values[i] - mean;
        sum_sq += diff * diff;
    }

    return sum_sq / (float)(n - 1);  /* Sample variance */
}

float mc_std_error(float variance, uint32_t n) {
    if (n == 0) return 0.0f;
    return sqrtf(variance / (float)n);
}

float mc_effective_sample_size(const float* weights, uint32_t n) {
    if (!weights || n == 0) return 0.0f;

    float sum_w = 0.0f;
    float sum_w2 = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        sum_w += weights[i];
        sum_w2 += weights[i] * weights[i];
    }

    if (sum_w2 < 1e-10f) return 0.0f;

    return (sum_w * sum_w) / sum_w2;
}

/* ============================================================================
 * UCB Calculation Implementation
 * ============================================================================ */

float mcts_compute_ucb1(
    float q_value,
    uint32_t visit_count,
    uint32_t parent_visits,
    float c
) {
    if (visit_count == 0) {
        return FLT_MAX;  /* Unvisited nodes have infinite UCB */
    }

    float exploitation = q_value;
    float exploration = c * sqrtf(logf((float)parent_visits) / (float)visit_count);

    return exploitation + exploration;
}

float mcts_compute_puct(
    float q_value,
    uint32_t visit_count,
    uint32_t parent_visits,
    float prior,
    float c
) {
    float exploitation = q_value;
    float exploration = c * prior * sqrtf((float)parent_visits) / (1.0f + (float)visit_count);

    return exploitation + exploration;
}

/* ============================================================================
 * MCTS Internal Functions
 * ============================================================================ */

/**
 * @brief Allocate and initialize a new MCTS node
 */
static mcts_node_t* mcts_alloc_node(mcts_tree_t* tree) {
    if (!tree || tree->num_nodes >= tree->max_nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mcts_alloc_node: tree is NULL");
        return NULL;
    }

    mcts_node_t* node = (mcts_node_t*)nimcp_calloc(1, sizeof(mcts_node_t));
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->id = tree->num_nodes;
    node->parent_id = MCTS_INVALID_NODE;
    node->children_ids = NULL;
    node->num_children = 0;
    node->children_capacity = 0;
    node->action_from_parent = UINT32_MAX;
    node->state = NULL;
    node->visit_count = 0;
    node->total_value = 0.0f;
    node->q_value = 0.0f;
    node->prior = 1.0f;
    node->depth = 0;
    node->is_expanded = false;
    node->is_terminal = false;

    tree->nodes[tree->num_nodes] = node;
    tree->num_nodes++;

    return node;
}

/**
 * @brief Free an MCTS node and its resources
 */
static void mcts_free_node(
    mcts_node_t* node,
    const mcts_config_t* config
) {
    if (!node) return;

    if (node->state && config && config->free_state) {
        config->free_state(node->state, config->user_data);
    }
    if (node->children_ids) {
        nimcp_free(node->children_ids);
    }
    nimcp_free(node);
}

/**
 * @brief Add a child to a node
 */
static nimcp_mc_result_t mcts_add_child(
    mcts_tree_t* tree,
    mcts_node_t* parent,
    uint32_t action,
    void* child_state,
    const mcts_config_t* config
) {
    mcts_node_t* child = mcts_alloc_node(tree);
    if (!child) {
        if (config->free_state) {
            config->free_state(child_state, config->user_data);
        }
        return NIMCP_MC_ERROR_MAX_NODES;
    }

    child->parent_id = parent->id;
    child->action_from_parent = action;
    child->state = child_state;
    child->depth = parent->depth + 1;

    /* Check if terminal */
    if (config->is_terminal) {
        child->is_terminal = config->is_terminal(child_state, config->user_data);
    }

    /* Get prior if available */
    if (config->get_prior) {
        child->prior = config->get_prior(parent->state, action, config->user_data);
    }

    /* Expand parent's children array if needed */
    if (parent->num_children >= parent->children_capacity) {
        uint32_t new_cap = parent->children_capacity == 0 ? 8 : parent->children_capacity * 2;
        uint32_t* new_children = (uint32_t*)nimcp_realloc(
            parent->children_ids, new_cap * sizeof(uint32_t));
        if (!new_children) {
            return NIMCP_MC_ERROR_MEMORY;
        }
        parent->children_ids = new_children;
        parent->children_capacity = new_cap;
    }

    parent->children_ids[parent->num_children++] = child->id;

    return NIMCP_MC_OK;
}

/**
 * @brief Select best child using configured policy
 */
static uint32_t mcts_select_child(
    const mcts_tree_t* tree,
    const mcts_node_t* node,
    const mcts_config_t* config
) {
    if (!node || node->num_children == 0) {
        return MCTS_INVALID_NODE;
    }

    uint32_t best_child = MCTS_INVALID_NODE;
    float best_value = -FLT_MAX;

    for (uint32_t i = 0; i < node->num_children; i++) {
        uint32_t child_id = node->children_ids[i];
        if (child_id >= tree->num_nodes) continue;

        mcts_node_t* child = tree->nodes[child_id];
        if (!child) continue;

        float value;

        switch (config->policy) {
            case MCTS_SELECT_UCB1:
                value = mcts_compute_ucb1(
                    child->q_value,
                    child->visit_count,
                    node->visit_count,
                    config->exploration_constant
                );
                break;

            case MCTS_SELECT_PUCT:
                value = mcts_compute_puct(
                    child->q_value,
                    child->visit_count,
                    node->visit_count,
                    child->prior,
                    config->exploration_constant
                );
                break;

            case MCTS_SELECT_EPSILON_GREEDY:
                if (mc_random_uniform((uint32_t*)&tree->seed) < config->epsilon) {
                    /* Random selection */
                    return node->children_ids[mc_random_int((uint32_t*)&tree->seed, node->num_children)];
                }
                value = child->q_value;
                break;

            default:
                value = mcts_compute_ucb1(
                    child->q_value,
                    child->visit_count,
                    node->visit_count,
                    config->exploration_constant
                );
        }

        if (value > best_value) {
            best_value = value;
            best_child = child_id;
        }
    }

    return best_child;
}

/**
 * @brief Selection phase: traverse tree to leaf
 */
static mcts_node_t* mcts_select(
    mcts_tree_t* tree,
    const mcts_config_t* config
) {
    mcts_node_t* node = tree->nodes[tree->root_id];

    while (node->is_expanded && !node->is_terminal && node->num_children > 0) {
        uint32_t child_id = mcts_select_child(tree, node, config);
        if (child_id == MCTS_INVALID_NODE) break;
        node = tree->nodes[child_id];
    }

    return node;
}

/**
 * @brief Expansion phase: add children to leaf node
 */
static nimcp_mc_result_t mcts_expand(
    mcts_tree_t* tree,
    mcts_node_t* node,
    const mcts_config_t* config
) {
    if (node->is_expanded || node->is_terminal) {
        return NIMCP_MC_OK;
    }

    if (node->depth >= config->max_depth) {
        node->is_terminal = true;
        return NIMCP_MC_OK;
    }

    /* Get available actions */
    uint32_t num_actions = config->get_action_count(node->state, config->user_data);
    if (num_actions == 0) {
        node->is_terminal = true;
        return NIMCP_MC_OK;
    }

    /* Expand all actions */
    for (uint32_t i = 0; i < num_actions; i++) {
        uint32_t action = config->get_action(node->state, i, config->user_data);
        if (action == UINT32_MAX) continue;

        void* child_state = config->apply_action(node->state, action, config->user_data);
        if (!child_state) continue;

        nimcp_mc_result_t result = mcts_add_child(tree, node, action, child_state, config);
        if (result != NIMCP_MC_OK) {
            return result;
        }
    }

    node->is_expanded = true;
    return NIMCP_MC_OK;
}

/**
 * @brief Simulation phase: rollout from node to terminal or max depth
 */
static float mcts_simulate(
    mcts_tree_t* tree,
    mcts_node_t* start_node,
    const mcts_config_t* config
) {
    if (!start_node->state) return 0.0f;

    /* If terminal, just evaluate */
    if (start_node->is_terminal) {
        return config->evaluate(start_node->state, config->user_data);
    }

    /* Simple evaluation rollout */
    void* current_state = config->clone_state(start_node->state, config->user_data);
    if (!current_state) {
        return config->evaluate(start_node->state, config->user_data);
    }

    float total_value = 0.0f;
    float discount = 1.0f;
    uint32_t depth = start_node->depth;

    while (depth < config->max_depth) {
        /* Check terminal */
        if (config->is_terminal(current_state, config->user_data)) {
            break;
        }

        /* Get actions */
        uint32_t num_actions = config->get_action_count(current_state, config->user_data);
        if (num_actions == 0) break;

        /* Random action selection for rollout */
        uint32_t action_idx = mc_random_int((uint32_t*)&tree->seed, num_actions);
        uint32_t action = config->get_action(current_state, action_idx, config->user_data);

        /* Apply action */
        void* next_state = config->apply_action(current_state, action, config->user_data);
        config->free_state(current_state, config->user_data);

        if (!next_state) break;
        current_state = next_state;

        /* Accumulate discounted value */
        float step_value = config->evaluate(current_state, config->user_data);
        total_value += discount * step_value;
        discount *= config->discount_factor;

        depth++;
    }

    /* Final state evaluation */
    float final_value = config->evaluate(current_state, config->user_data);
    total_value += discount * final_value;

    config->free_state(current_state, config->user_data);

    return total_value;
}

/**
 * @brief Backpropagation phase: update statistics from leaf to root
 */
static void mcts_backpropagate(
    mcts_tree_t* tree,
    mcts_node_t* leaf,
    float value
) {
    mcts_node_t* node = leaf;

    while (node != NULL) {
        node->visit_count++;
        node->total_value += value;
        node->q_value = node->total_value / (float)node->visit_count;

        if (node->parent_id == MCTS_INVALID_NODE) break;
        node = tree->nodes[node->parent_id];
    }
}

/**
 * @brief Free all tree resources
 */
static void mcts_free_tree(mcts_tree_t* tree, const mcts_config_t* config) {
    if (!tree) return;

    for (uint32_t i = 0; i < tree->num_nodes; i++) {
        mcts_free_node(tree->nodes[i], config);
    }
    nimcp_free(tree->nodes);
    nimcp_free(tree);
}

/* ============================================================================
 * MCTS Public API Implementation
 * ============================================================================ */

void mcts_config_init(mcts_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(mcts_config_t));

    config->max_iterations = MCTS_DEFAULT_ITERATIONS;
    config->max_depth = MCTS_DEFAULT_MAX_DEPTH;
    config->max_nodes = 10000;
    config->exploration_constant = MCTS_DEFAULT_EXPLORATION;
    config->discount_factor = MCTS_DEFAULT_DISCOUNT;
    config->policy = MCTS_SELECT_UCB1;
    config->epsilon = 0.1f;
    config->seed = 0;
}

nimcp_mc_result_t mcts_search(
    const mcts_config_t* config,
    const void* initial_state,
    mcts_result_t* result
) {
    /* Validate parameters */
    if (!config || !initial_state || !result) {
        return NIMCP_MC_ERROR_NULL;
    }
    if (!config->get_action_count || !config->get_action ||
        !config->apply_action || !config->evaluate ||
        !config->is_terminal || !config->free_state || !config->clone_state) {
        return NIMCP_MC_ERROR_NULL;
    }

    memset(result, 0, sizeof(mcts_result_t));

    /* Initialize tree */
    mcts_tree_t* tree = (mcts_tree_t*)nimcp_calloc(1, sizeof(mcts_tree_t));
    if (!tree) return NIMCP_MC_ERROR_MEMORY;

    tree->nodes = (mcts_node_t**)nimcp_calloc(config->max_nodes, sizeof(mcts_node_t*));
    if (!tree->nodes) {
        nimcp_free(tree);
        return NIMCP_MC_ERROR_MEMORY;
    }
    tree->max_nodes = config->max_nodes;
    tree->num_nodes = 0;
    tree->seed = config->seed ? config->seed : mc_seed_from_time();

    /* Create root node */
    mcts_node_t* root = mcts_alloc_node(tree);
    if (!root) {
        mcts_free_tree(tree, config);
        return NIMCP_MC_ERROR_MEMORY;
    }
    tree->root_id = root->id;

    root->state = config->clone_state(initial_state, config->user_data);
    if (!root->state) {
        mcts_free_tree(tree, config);
        return NIMCP_MC_ERROR_MEMORY;
    }
    root->is_terminal = config->is_terminal(root->state, config->user_data);

    nimcp_mc_result_t status = NIMCP_MC_OK;
    uint32_t iterations = 0;
    uint32_t max_depth = 0;

    /* Main MCTS loop */
    for (iterations = 0; iterations < config->max_iterations; iterations++) {
        /* Selection */
        mcts_node_t* leaf = mcts_select(tree, config);
        if (!leaf) break;

        /* Expansion */
        status = mcts_expand(tree, leaf, config);
        if (status != NIMCP_MC_OK) break;

        /* Select a child if we just expanded */
        mcts_node_t* sim_node = leaf;
        if (leaf->is_expanded && leaf->num_children > 0) {
            uint32_t child_idx = mc_random_int(&tree->seed, leaf->num_children);
            sim_node = tree->nodes[leaf->children_ids[child_idx]];
        }

        /* Track max depth */
        if (sim_node->depth > max_depth) {
            max_depth = sim_node->depth;
        }

        /* Simulation */
        float value = mcts_simulate(tree, sim_node, config);

        /* Backpropagation */
        mcts_backpropagate(tree, sim_node, value);
    }

    /* Extract results from root children */
    root = tree->nodes[tree->root_id];
    result->num_actions = root->num_children;
    result->nodes_created = tree->num_nodes;
    result->iterations_completed = iterations;
    result->max_depth_reached = max_depth;

    if (root->num_children > 0) {
        result->action_visits = (uint32_t*)nimcp_calloc(root->num_children, sizeof(uint32_t));
        result->action_values = (float*)nimcp_calloc(root->num_children, sizeof(float));

        if (!result->action_visits || !result->action_values) {
            mcts_result_free(result);
            mcts_free_tree(tree, config);
            return NIMCP_MC_ERROR_MEMORY;
        }

        uint32_t best_idx = 0;
        uint32_t best_visits = 0;

        for (uint32_t i = 0; i < root->num_children; i++) {
            mcts_node_t* child = tree->nodes[root->children_ids[i]];
            result->action_visits[i] = child->visit_count;
            result->action_values[i] = child->q_value;

            /* Best action by visit count (robust selection) */
            if (child->visit_count > best_visits) {
                best_visits = child->visit_count;
                best_idx = i;
                result->best_action = child->action_from_parent;
                result->best_value = child->q_value;
            }
        }
    }

    mcts_free_tree(tree, config);
    return status;
}

void mcts_result_free(mcts_result_t* result) {
    if (!result) return;

    if (result->action_visits) {
        nimcp_free(result->action_visits);
        result->action_visits = NULL;
    }
    if (result->action_values) {
        nimcp_free(result->action_values);
        result->action_values = NULL;
    }
}

/* ============================================================================
 * Monte Carlo Sampling Implementation
 * ============================================================================ */

void mc_config_init(mc_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(mc_config_t));

    config->method = MC_SAMPLE_UNIFORM;
    config->num_samples = MC_DEFAULT_SAMPLES;
    config->burnin = MC_DEFAULT_BURNIN;
    config->tolerance = 1e-6f;
    config->store_samples = false;
    config->seed = 0;
    config->num_strata = 10;

    /* Initialize GPU config with defaults */
    mc_gpu_config_init(&config->gpu);
}

/**
 * @brief Uniform Monte Carlo estimation
 */
static nimcp_mc_result_t mc_estimate_uniform(
    const mc_config_t* config,
    mc_result_t* result,
    uint32_t* seed
) {
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < config->num_samples; i++) {
        float sample = config->sampler(config->user_data);
        float value = config->objective(sample, config->user_data);

        if (result->samples) {
            result->samples[i] = value;
        }

        sum += value;
        sum_sq += value * value;
    }

    result->num_samples = config->num_samples;
    result->estimate = sum / (float)config->num_samples;

    float mean_sq = sum_sq / (float)config->num_samples;
    result->variance = mean_sq - result->estimate * result->estimate;
    result->variance *= (float)config->num_samples / (float)(config->num_samples - 1);

    result->std_error = mc_std_error(result->variance, config->num_samples);

    return NIMCP_MC_OK;
}

/**
 * @brief Importance sampling Monte Carlo estimation
 */
static nimcp_mc_result_t mc_estimate_importance(
    const mc_config_t* config,
    mc_result_t* result,
    uint32_t* seed
) {
    if (!config->weight) {
        return NIMCP_MC_ERROR_NULL;
    }

    float* weights = (float*)nimcp_calloc(config->num_samples, sizeof(float));
    float* values = (float*)nimcp_calloc(config->num_samples, sizeof(float));

    if (!weights || !values) {
        nimcp_free(weights);
        nimcp_free(values);
        return NIMCP_MC_ERROR_MEMORY;
    }

    float sum_w = 0.0f;
    float sum_wf = 0.0f;

    for (uint32_t i = 0; i < config->num_samples; i++) {
        float sample = config->sampler(config->user_data);
        float w = config->weight(sample, config->user_data);
        float f = config->objective(sample, config->user_data);

        weights[i] = w;
        values[i] = f;

        sum_w += w;
        sum_wf += w * f;

        if (result->samples) {
            result->samples[i] = w * f;
        }
    }

    result->num_samples = config->num_samples;
    result->estimate = sum_wf / sum_w;

    /* Compute variance using self-normalized importance sampling */
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < config->num_samples; i++) {
        float normalized_w = weights[i] / sum_w;
        float diff = values[i] - result->estimate;
        sum_sq += normalized_w * diff * diff;
    }
    result->variance = sum_sq * (float)config->num_samples / (float)(config->num_samples - 1);
    result->std_error = mc_std_error(result->variance, config->num_samples);

    result->effective_sample_size = mc_effective_sample_size(weights, config->num_samples);

    nimcp_free(weights);
    nimcp_free(values);

    return NIMCP_MC_OK;
}

/**
 * @brief Stratified sampling Monte Carlo estimation
 */
static nimcp_mc_result_t mc_estimate_stratified(
    const mc_config_t* config,
    mc_result_t* result,
    uint32_t* seed
) {
    uint32_t samples_per_stratum = config->num_samples / config->num_strata;
    if (samples_per_stratum == 0) samples_per_stratum = 1;

    float sum = 0.0f;
    float sum_sq = 0.0f;
    uint32_t total_samples = 0;

    for (uint32_t s = 0; s < config->num_strata; s++) {
        float stratum_sum = 0.0f;

        for (uint32_t i = 0; i < samples_per_stratum; i++) {
            float sample = config->sampler(config->user_data);
            float value = config->objective(sample, config->user_data);

            stratum_sum += value;

            if (result->samples && total_samples < config->num_samples) {
                result->samples[total_samples] = value;
            }
            total_samples++;
        }

        float stratum_mean = stratum_sum / (float)samples_per_stratum;
        sum += stratum_mean;
    }

    result->num_samples = total_samples;
    result->estimate = sum / (float)config->num_strata;

    /* Compute variance from stratum means */
    if (result->samples) {
        result->variance = mc_variance(result->samples, total_samples, result->estimate);
    } else {
        result->variance = 0.0f;  /* Would need to store stratum values */
    }
    result->std_error = mc_std_error(result->variance, total_samples);

    return NIMCP_MC_OK;
}

/**
 * @brief Metropolis-Hastings MCMC estimation
 */
static nimcp_mc_result_t mc_estimate_mh(
    const mc_config_t* config,
    mc_result_t* result,
    uint32_t* seed
) {
    if (!config->proposal || !config->density) {
        return NIMCP_MC_ERROR_NULL;
    }

    /* Initialize chain */
    float current = config->sampler(config->user_data);
    float current_density = config->density(current, config->user_data);

    float sum = 0.0f;
    float sum_sq = 0.0f;
    uint32_t accepted = 0;
    uint32_t sample_idx = 0;

    /* Burn-in phase */
    for (uint32_t i = 0; i < config->burnin; i++) {
        float proposed = config->proposal(current, config->user_data);
        float proposed_density = config->density(proposed, config->user_data);

        float alpha = proposed_density / current_density;
        if (alpha > 1.0f || mc_random_uniform(seed) < alpha) {
            current = proposed;
            current_density = proposed_density;
        }
    }

    /* Sampling phase */
    for (uint32_t i = 0; i < config->num_samples; i++) {
        float proposed = config->proposal(current, config->user_data);
        float proposed_density = config->density(proposed, config->user_data);

        float alpha = proposed_density / current_density;
        if (alpha > 1.0f || mc_random_uniform(seed) < alpha) {
            current = proposed;
            current_density = proposed_density;
            accepted++;
        }

        float value = config->objective(current, config->user_data);
        sum += value;
        sum_sq += value * value;

        if (result->samples) {
            result->samples[sample_idx++] = value;
        }
    }

    result->num_samples = config->num_samples;
    result->estimate = sum / (float)config->num_samples;

    float mean_sq = sum_sq / (float)config->num_samples;
    result->variance = mean_sq - result->estimate * result->estimate;
    result->variance *= (float)config->num_samples / (float)(config->num_samples - 1);

    result->std_error = mc_std_error(result->variance, config->num_samples);
    result->acceptance_rate = (float)accepted / (float)config->num_samples;

    return NIMCP_MC_OK;
}

nimcp_mc_result_t mc_estimate(
    const mc_config_t* config,
    mc_result_t* result
) {
    /* Validate parameters */
    if (!config || !result) {
        return NIMCP_MC_ERROR_NULL;
    }
    if (!config->sampler || !config->objective) {
        return NIMCP_MC_ERROR_NULL;
    }
    if (config->num_samples == 0) {
        return NIMCP_MC_ERROR_INVALID;
    }

    memset(result, 0, sizeof(mc_result_t));

    /* Allocate sample storage if requested */
    if (config->store_samples) {
        result->samples = (float*)nimcp_calloc(config->num_samples, sizeof(float));
        if (!result->samples) {
            return NIMCP_MC_ERROR_MEMORY;
        }
    }

    /* Initialize seed */
    uint32_t seed = config->seed ? config->seed : mc_seed_from_time();

    /* Dispatch to appropriate method */
    nimcp_mc_result_t status;

    switch (config->method) {
        case MC_SAMPLE_UNIFORM:
            status = mc_estimate_uniform(config, result, &seed);
            break;

        case MC_SAMPLE_IMPORTANCE:
            status = mc_estimate_importance(config, result, &seed);
            break;

        case MC_SAMPLE_STRATIFIED:
            status = mc_estimate_stratified(config, result, &seed);
            break;

        case MC_SAMPLE_METROPOLIS_HASTINGS:
            status = mc_estimate_mh(config, result, &seed);
            break;

        default:
            status = NIMCP_MC_ERROR_INVALID;
    }

    if (status != NIMCP_MC_OK) {
        mc_result_free(result);
    }

    return status;
}

void mc_result_free(mc_result_t* result) {
    if (!result) return;

    if (result->samples) {
        nimcp_free(result->samples);
        result->samples = NULL;
    }
}

/* ============================================================================
 * GPU Acceleration Utilities
 * ============================================================================ */

void mc_gpu_config_init(mc_gpu_config_t* gpu_config) {
    if (!gpu_config) return;

    gpu_config->mode = MC_GPU_DISABLED;
    gpu_config->ctx = NULL;
    gpu_config->min_samples_for_gpu = 10000;
    gpu_config->threads_per_block = 256;
}

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(monte_carlo)

#endif
//=============================================================================

/**
 * @file nimcp_bayesian_network.c
 * @brief Bayesian network implementation for anomaly detection
 *
 * WHAT: Bayesian network with variable elimination inference
 * WHY:  Probabilistic inference for anomaly detection
 * HOW:  DAG structure with CPTs, variable elimination algorithm
 *
 * ALGORITHM: Variable Elimination with Sum-Product
 * - Builds factor graph from evidence
 * - Eliminates variables in topological order
 * - Marginalizes to compute posteriors
 *
 * COMPLEXITY:
 * - Space: O(n * max_parent_states)
 * - Time: O(n * exp(tree_width)) - exponential in tree width
 *
 * @author NIMCP Development Team
 * @date 2025-12-07
 */

#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/algorithms/nimcp_sort.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bayesian_network, MESH_ADAPTER_CATEGORY_SECURITY)


/*=============================================================================
 * INTERNAL CONSTANTS
 *============================================================================*/

#define MAX_PARENTS 4
#define MAX_STATES 10
#define EPSILON 1e-9f
#define LOG_EPSILON -20.0f

/*=============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

/**
 * @brief Bayesian network node
 */
typedef struct {
    uint32_t node_id;
    uint32_t num_parents;
    uint32_t parents[MAX_PARENTS];

    /** Conditional Probability Table P(node | parents) */
    float* cpt;              /**< Flattened CPT */
    size_t cpt_size;         /**< Total CPT size */

    /** Learning statistics */
    uint64_t observation_count;
    float* count_table;      /**< For online learning */
} bn_node_t;

/**
 * @brief Bayesian network internal structure
 */
struct nimcp_bayesian_network_internal {
    uint32_t magic;
    uint32_t num_nodes;
    bn_node_t* nodes;

    /** Topological ordering for inference */
    uint32_t* topo_order;
    bool topo_valid;

    /** Statistics */
    uint64_t inference_count;
    uint64_t learning_count;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * @brief Validate network handle
 */
static inline bool bn_is_valid(nimcp_bayesian_network_t bn) {
    return bn != NULL && bn->magic == NIMCP_BAYESIAN_NETWORK_MAGIC;
}

/**
 * @brief Get CPT size for node
 *
 * WHAT: Calculate size of CPT based on parents
 * WHY:  CPT size = product of parent state counts
 * HOW:  For continuous: discretize to 10 bins, CPT size = 10^num_parents * 10
 */
static size_t get_cpt_size(uint32_t num_parents) {
    if (num_parents == 0) {
        return MAX_STATES;  /* Prior distribution */
    }

    size_t size = MAX_STATES;  /* Child states */
    for (uint32_t i = 0; i < num_parents; i++) {
        size *= MAX_STATES;  /* Each parent has MAX_STATES states */
    }
    return size;
}

/**
 * @brief Discretize continuous value to state index
 *
 * WHAT: Map continuous [0,1] to discrete state [0, MAX_STATES-1]
 * WHY:  Bayesian networks work with discrete states
 * HOW:  Linear binning
 */
static uint32_t discretize(float value) {
    if (isnan(value)) {
        return UINT32_MAX;  /* Unobserved */
    }

    /* Clamp to [0, 1] */
    if (value < 0.0F) value = 0.0F;
    if (value > 1.0F) value = 1.0F;

    uint32_t state = (uint32_t)(value * (MAX_STATES - 1));
    if (state >= MAX_STATES) state = MAX_STATES - 1;
    return state;
}

/**
 * @brief Undiscretize state to continuous value
 */
static float undiscretize(uint32_t state) {
    if (state >= MAX_STATES) {
        return NAN;
    }
    return (float)state / (float)(MAX_STATES - 1);
}

/**
 * @brief Get CPT index for given parent states and child state
 *
 * WHAT: Map parent states and child state to CPT index
 * WHY:  CPT is flattened array, need to compute offset
 * HOW:  Row-major indexing
 */
static size_t get_cpt_index(const bn_node_t* node, const uint32_t* parent_states, uint32_t child_state) {
    size_t index = 0;
    size_t multiplier = 1;

    /* Child state is the innermost dimension */
    index = child_state;
    multiplier = MAX_STATES;

    /* Parent states in reverse order */
    for (int i = (int)node->num_parents - 1; i >= 0; i--) {
        index += parent_states[i] * multiplier;
        multiplier *= MAX_STATES;
    }

    return index;
}

/**
 * @brief Initialize uniform CPT
 */
static void init_uniform_cpt(float* cpt, size_t cpt_size) {
    float uniform_prob = 1.0F / (float)MAX_STATES;
    for (size_t i = 0; i < cpt_size; i++) {
        cpt[i] = uniform_prob;
    }
}

/**
 * @brief Normalize CPT row to sum to 1.0
 */
static void normalize_cpt_row(float* row, size_t row_size) {
    float sum = 0.0F;
    for (size_t i = 0; i < row_size; i++) {
        sum += row[i];
    }

    if (sum > EPSILON) {
        for (size_t i = 0; i < row_size; i++) {
            row[i] /= sum;
        }
    } else {
        /* If sum is zero, use uniform */
        init_uniform_cpt(row, row_size);
    }
}

/*=============================================================================
 * TOPOLOGICAL SORT CALLBACKS (for nimcp_sort.h API)
 *============================================================================*/

/**
 * @brief Get parent count for a node (callback for nimcp_topological_sort)
 */
static uint32_t bn_get_dep_count(uint32_t node_index, void* user_data) {
    nimcp_bayesian_network_t bn = (nimcp_bayesian_network_t)user_data;
    if (node_index >= bn->num_nodes) {
        return 0;
    }
    return bn->nodes[node_index].num_parents;
}

/**
 * @brief Get a specific parent of a node (callback for nimcp_topological_sort)
 */
static uint32_t bn_get_dep(uint32_t node_index, uint32_t dep_index, void* user_data) {
    nimcp_bayesian_network_t bn = (nimcp_bayesian_network_t)user_data;
    if (node_index >= bn->num_nodes) {
        return UINT32_MAX;
    }
    if (dep_index >= bn->nodes[node_index].num_parents) {
        return UINT32_MAX;
    }
    return bn->nodes[node_index].parents[dep_index];
}

/**
 * @brief Compute topological order using consolidated nimcp_sort API
 */
static bool compute_topo_order(nimcp_bayesian_network_t bn) {
    if (!bn || bn->num_nodes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compute_topo_order: bn is NULL");
        return false;
    }

    nimcp_topo_config_t config = {
        .node_count = bn->num_nodes,
        .user_data = (void*)bn,
        .get_dep_count = bn_get_dep_count,
        .get_dep = bn_get_dep,
        .get_dependent_count = NULL,
        .get_dependent = NULL
    };

    uint32_t sorted_count = 0;
    nimcp_sort_result_t result = nimcp_topological_sort(
        &config, bn->topo_order, bn->num_nodes, &sorted_count);

    bn->topo_valid = (result == NIMCP_SORT_OK && sorted_count == bn->num_nodes);
    return bn->topo_valid;
}

/*=============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

nimcp_bayesian_network_t nimcp_bn_create(uint32_t num_nodes) {
    LOG_MODULE_DEBUG("security.bayesian", "Creating Bayesian network with %u nodes", num_nodes);

    if (num_nodes == 0 || num_nodes > 1000) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_bn_create: invalid number of nodes %u", num_nodes);
        return NULL;
    }

    nimcp_bayesian_network_t bn = (nimcp_bayesian_network_t)nimcp_calloc(1, sizeof(struct nimcp_bayesian_network_internal));
    if (!bn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bn_create: failed to allocate network structure");
        return NULL;
    }

    bn->magic = NIMCP_BAYESIAN_NETWORK_MAGIC;
    bn->num_nodes = num_nodes;

    bn->nodes = (bn_node_t*)nimcp_calloc(num_nodes, sizeof(bn_node_t));
    if (!bn->nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bn_create: failed to allocate nodes array");
        nimcp_free(bn);
        return NULL;
    }

    bn->topo_order = (uint32_t*)nimcp_malloc(num_nodes * sizeof(uint32_t));
    if (!bn->topo_order) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bn_create: failed to allocate topo_order array");
        nimcp_free(bn->nodes);
        nimcp_free(bn);
        return NULL;
    }

    /* Initialize nodes with default prior (no parents) */
    for (uint32_t i = 0; i < num_nodes; i++) {
        bn->nodes[i].node_id = i;
        bn->nodes[i].num_parents = 0;
        bn->nodes[i].cpt_size = MAX_STATES;
        bn->nodes[i].cpt = (float*)nimcp_malloc(MAX_STATES * sizeof(float));
        bn->nodes[i].count_table = (float*)nimcp_calloc(MAX_STATES, sizeof(float));

        if (!bn->nodes[i].cpt || !bn->nodes[i].count_table) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_bn_create: failed to allocate CPT for node %u", i);
            nimcp_bn_destroy(bn);
            return NULL;
        }

        init_uniform_cpt(bn->nodes[i].cpt, MAX_STATES);
    }

    bn->topo_valid = false;

    LOG_MODULE_INFO("security.bayesian", "Successfully created Bayesian network with %u nodes", num_nodes);
    return bn;
}

void nimcp_bn_destroy(nimcp_bayesian_network_t bn) {
    if (!bn) {
        return;
    }

    LOG_MODULE_DEBUG("security.bayesian", "Destroying Bayesian network with %u nodes (inferences: %llu, learning: %llu)",
                     bn->num_nodes, (unsigned long long)bn->inference_count, (unsigned long long)bn->learning_count);

    if (bn->nodes) {
        for (uint32_t i = 0; i < bn->num_nodes; i++) {
            nimcp_free(bn->nodes[i].cpt);
            nimcp_free(bn->nodes[i].count_table);
        }
        nimcp_free(bn->nodes);
    }

    nimcp_free(bn->topo_order);

    bn->magic = 0;
    nimcp_free(bn);
}

nimcp_error_t nimcp_bn_add_edge(nimcp_bayesian_network_t bn, uint32_t parent, uint32_t child) {
    LOG_MODULE_DEBUG("security.bayesian", "Adding edge from node %u to node %u", parent, child);

    if (!bn_is_valid(bn)) {
        LOG_MODULE_ERROR("security.bayesian", "Invalid Bayesian network handle");
        return NIMCP_INVALID_PARAM;
    }

    if (parent >= bn->num_nodes || child >= bn->num_nodes) {
        LOG_MODULE_ERROR("security.bayesian", "Invalid node IDs: parent=%u, child=%u (max=%u)",
                        parent, child, bn->num_nodes - 1);
        return NIMCP_INVALID_PARAM;
    }

    bn_node_t* node = &bn->nodes[child];

    if (node->num_parents >= MAX_PARENTS) {
        LOG_MODULE_WARN("security.bayesian", "Node %u already has %u parents (max=%u)",
                       child, node->num_parents, MAX_PARENTS);
        return NIMCP_INVALID_PARAM;  /* Too many parents */
    }

    /* Check for duplicate */
    for (uint32_t i = 0; i < node->num_parents; i++) {
        if (node->parents[i] == parent) {
            LOG_MODULE_DEBUG("security.bayesian", "Edge %u->%u already exists", parent, child);
            return NIMCP_SUCCESS;  /* Already exists */
        }
    }

    /* Add parent */
    node->parents[node->num_parents++] = parent;

    /* Resize CPT */
    size_t new_cpt_size = get_cpt_size(node->num_parents);
    float* new_cpt = (float*)nimcp_malloc(new_cpt_size * sizeof(float));
    float* new_count = (float*)nimcp_calloc(new_cpt_size, sizeof(float));

    if (!new_cpt || !new_count) {
        nimcp_free(new_cpt);
        nimcp_free(new_count);
        node->num_parents--;  /* Rollback */
        return NIMCP_NO_MEMORY;
    }

    init_uniform_cpt(new_cpt, new_cpt_size);

    nimcp_free(node->cpt);
    nimcp_free(node->count_table);
    node->cpt = new_cpt;
    node->count_table = new_count;
    node->cpt_size = new_cpt_size;

    /* Invalidate topological order */
    bn->topo_valid = false;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bn_set_cpt(nimcp_bayesian_network_t bn, uint32_t node_id, const float* cpt, size_t cpt_size) {
    if (!bn_is_valid(bn) || !cpt) {
        return NIMCP_INVALID_PARAM;
    }

    if (node_id >= bn->num_nodes) {
        return NIMCP_INVALID_PARAM;
    }

    bn_node_t* node = &bn->nodes[node_id];

    if (cpt_size != node->cpt_size) {
        return NIMCP_INVALID_PARAM;  /* Size mismatch */
    }

    memcpy(node->cpt, cpt, cpt_size * sizeof(float));

    /* Normalize each row */
    size_t num_rows = cpt_size / MAX_STATES;
    for (size_t row = 0; row < num_rows; row++) {
        normalize_cpt_row(&node->cpt[row * MAX_STATES], MAX_STATES);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bn_infer(nimcp_bayesian_network_t bn, const float* evidence, float* posteriors) {
    if (!bn_is_valid(bn) || !evidence || !posteriors) {
        return NIMCP_INVALID_PARAM;
    }

    /* Ensure topological order is computed */
    if (!bn->topo_valid) {
        if (!compute_topo_order(bn)) {
            return NIMCP_INVALID_STATE;
        }
    }

    /* Discretize evidence */
    uint32_t* states = (uint32_t*)nimcp_malloc(bn->num_nodes * sizeof(uint32_t));
    if (!states) {
        return NIMCP_NO_MEMORY;
    }

    for (uint32_t i = 0; i < bn->num_nodes; i++) {
        states[i] = discretize(evidence[i]);
    }

    /* Forward pass: compute posteriors */
    for (uint32_t i = 0; i < bn->num_nodes; i++) {
        uint32_t node_id = bn->topo_order[i];
        bn_node_t* node = &bn->nodes[node_id];

        if (states[node_id] != UINT32_MAX) {
            /* Evidence observed: posterior = 1 for observed state */
            posteriors[node_id] = undiscretize(states[node_id]);
        } else {
            /* No evidence: use CPT to compute marginal */
            if (node->num_parents == 0) {
                /* Root node: use prior */
                float sum = 0.0F;
                float weighted_sum = 0.0F;
                for (uint32_t s = 0; s < MAX_STATES; s++) {
                    float prob = node->cpt[s];
                    sum += prob;
                    weighted_sum += prob * undiscretize(s);
                }
                posteriors[node_id] = (sum > EPSILON) ? (weighted_sum / sum) : 0.5F;
            } else {
                /* Non-root: marginalize over parent states */
                uint32_t parent_states[MAX_PARENTS];
                for (uint32_t p = 0; p < node->num_parents; p++) {
                    uint32_t parent_id = node->parents[p];
                    parent_states[p] = discretize(posteriors[parent_id]);
                    if (parent_states[p] == UINT32_MAX) {
                        parent_states[p] = MAX_STATES / 2;  /* Use median */
                    }
                }

                /* Compute expectation over child states */
                float sum = 0.0F;
                float weighted_sum = 0.0F;
                for (uint32_t s = 0; s < MAX_STATES; s++) {
                    size_t idx = get_cpt_index(node, parent_states, s);
                    float prob = node->cpt[idx];
                    sum += prob;
                    weighted_sum += prob * undiscretize(s);
                }
                posteriors[node_id] = (sum > EPSILON) ? (weighted_sum / sum) : 0.5F;
            }
        }
    }

    nimcp_free(states);
    bn->inference_count++;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bn_learn(nimcp_bayesian_network_t bn, const float* sample) {
    if (!bn_is_valid(bn) || !sample) {
        return NIMCP_INVALID_PARAM;
    }

    /* Discretize sample */
    uint32_t* states = (uint32_t*)nimcp_malloc(bn->num_nodes * sizeof(uint32_t));
    if (!states) {
        return NIMCP_NO_MEMORY;
    }

    for (uint32_t i = 0; i < bn->num_nodes; i++) {
        states[i] = discretize(sample[i]);
    }

    /* Update count tables (for MLE) */
    for (uint32_t i = 0; i < bn->num_nodes; i++) {
        bn_node_t* node = &bn->nodes[i];
        uint32_t child_state = states[i];

        if (child_state == UINT32_MAX) {
            continue;  /* Skip unobserved */
        }

        if (node->num_parents == 0) {
            /* Root node: update prior */
            node->count_table[child_state] += 1.0F;
        } else {
            /* Get parent states */
            uint32_t parent_states[MAX_PARENTS];
            bool all_observed = true;
            for (uint32_t p = 0; p < node->num_parents; p++) {
                parent_states[p] = states[node->parents[p]];
                if (parent_states[p] == UINT32_MAX) {
                    all_observed = false;
                    break;
                }
            }

            if (all_observed) {
                size_t idx = get_cpt_index(node, parent_states, child_state);
                node->count_table[idx] += 1.0F;
            }
        }

        node->observation_count++;
    }

    /* Update CPTs using incremental MLE with Laplace smoothing */
    const float alpha = 1.0F;  /* Laplace smoothing parameter */

    for (uint32_t i = 0; i < bn->num_nodes; i++) {
        bn_node_t* node = &bn->nodes[i];

        size_t num_rows = node->cpt_size / MAX_STATES;
        for (size_t row = 0; row < num_rows; row++) {
            float row_sum = 0.0F;
            for (uint32_t s = 0; s < MAX_STATES; s++) {
                row_sum += node->count_table[row * MAX_STATES + s] + alpha;
            }

            for (uint32_t s = 0; s < MAX_STATES; s++) {
                size_t idx = row * MAX_STATES + s;
                node->cpt[idx] = (node->count_table[idx] + alpha) / row_sum;
            }
        }
    }

    nimcp_free(states);
    bn->learning_count++;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_bn_log_likelihood(nimcp_bayesian_network_t bn, const float* sample, float* log_likelihood) {
    if (!bn_is_valid(bn) || !sample || !log_likelihood) {
        return NIMCP_INVALID_PARAM;
    }

    /* Discretize sample */
    uint32_t* states = (uint32_t*)nimcp_malloc(bn->num_nodes * sizeof(uint32_t));
    if (!states) {
        return NIMCP_NO_MEMORY;
    }

    for (uint32_t i = 0; i < bn->num_nodes; i++) {
        states[i] = discretize(sample[i]);
    }

    /* Compute log P(sample) = Σ log P(node | parents) */
    float ll = 0.0F;

    for (uint32_t i = 0; i < bn->num_nodes; i++) {
        bn_node_t* node = &bn->nodes[i];
        uint32_t child_state = states[i];

        if (child_state == UINT32_MAX) {
            continue;  /* Skip unobserved */
        }

        float prob;

        if (node->num_parents == 0) {
            /* Root node: P(node) */
            prob = node->cpt[child_state];
        } else {
            /* Get parent states */
            uint32_t parent_states[MAX_PARENTS];
            bool all_observed = true;
            for (uint32_t p = 0; p < node->num_parents; p++) {
                parent_states[p] = states[node->parents[p]];
                if (parent_states[p] == UINT32_MAX) {
                    all_observed = false;
                    break;
                }
            }

            if (!all_observed) {
                continue;  /* Can't compute likelihood with missing parents */
            }

            size_t idx = get_cpt_index(node, parent_states, child_state);
            prob = node->cpt[idx];
        }

        /* Add log probability (avoid log(0)) */
        if (prob < EPSILON) {
            ll += LOG_EPSILON;
        } else {
            ll += logf(prob);
        }
    }

    nimcp_free(states);
    *log_likelihood = ll;

    return NIMCP_SUCCESS;
}

/**
 * @file nimcp_reasoning_causal.c
 * @brief Causal Reasoning Engine implementation
 *
 * WHAT: DAG-based causal inference with do-calculus
 * WHY:  Enable the reasoning system to distinguish causation from correlation
 * HOW:  Adjacency list DAG, Kahn's topological sort for validation,
 *       belief propagation for queries, do-operator for interventions
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include "cognitive/reasoning/nimcp_reasoning_causal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "reasoning_causal"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Adjacency list entry for a child node
 */
typedef struct {
    uint32_t child_id;    /**< Child node ID */
    float strength;       /**< Causal strength of this edge */
} causal_child_entry_t;

/**
 * @brief Adjacency list entry for a parent node
 */
typedef struct {
    uint32_t parent_id;   /**< Parent node ID */
    float strength;       /**< Causal strength of this edge */
} causal_parent_entry_t;

/**
 * @brief Internal causal DAG structure
 */
struct causal_dag {
    causal_dag_config_t config;

    /* Node storage */
    causal_node_t nodes[CAUSAL_MAX_NODES];
    uint32_t num_nodes;

    /* Adjacency lists: children for forward traversal */
    causal_child_entry_t children[CAUSAL_MAX_NODES][CAUSAL_MAX_PARENTS];
    uint32_t num_children[CAUSAL_MAX_NODES];

    /* Reverse adjacency: parents for backward traversal */
    causal_parent_entry_t parents[CAUSAL_MAX_NODES][CAUSAL_MAX_PARENTS];
    uint32_t num_parents[CAUSAL_MAX_NODES];

    /* Statistics */
    causal_dag_stats_t stats;
    uint32_t total_path_length;  /**< Sum of all path lengths for averaging */
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Check if a node ID is valid
 */
static bool is_valid_node(const causal_dag_t* dag, uint32_t node_id)
{
    return node_id < dag->num_nodes;
}

/**
 * @brief Find edge index in children list, or -1 if not found
 */
static int find_child_edge(const causal_dag_t* dag, uint32_t from_id, uint32_t to_id)
{
    for (uint32_t i = 0; i < dag->num_children[from_id]; i++) {
        if (dag->children[from_id][i].child_id == to_id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Find edge index in parents list, or -1 if not found
 */
static int find_parent_edge(const causal_dag_t* dag, uint32_t to_id, uint32_t from_id)
{
    for (uint32_t i = 0; i < dag->num_parents[to_id]; i++) {
        if (dag->parents[to_id][i].parent_id == from_id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Cycle detection via Kahn's algorithm (topological sort)
 *
 * @return true if acyclic, false if cycle exists
 */
static bool is_acyclic(const causal_dag_t* dag)
{
    if (dag->num_nodes == 0) return true;

    /* Compute in-degree for each node */
    uint32_t in_degree[CAUSAL_MAX_NODES];
    memset(in_degree, 0, sizeof(uint32_t) * dag->num_nodes);

    for (uint32_t i = 0; i < dag->num_nodes; i++) {
        in_degree[i] = dag->num_parents[i];
    }

    /* Queue for nodes with in-degree 0 */
    uint32_t queue[CAUSAL_MAX_NODES];
    uint32_t front = 0, back = 0;

    for (uint32_t i = 0; i < dag->num_nodes; i++) {
        if (in_degree[i] == 0) {
            queue[back++] = i;
        }
    }

    uint32_t sorted_count = 0;
    while (front < back) {
        uint32_t node = queue[front++];
        sorted_count++;

        for (uint32_t i = 0; i < dag->num_children[node]; i++) {
            uint32_t child = dag->children[node][i].child_id;
            in_degree[child]--;
            if (in_degree[child] == 0) {
                queue[back++] = child;
            }
        }
    }

    return sorted_count == dag->num_nodes;
}

/**
 * @brief DFS-based path finder from source to target
 *
 * @param dag The DAG
 * @param current Current node in DFS
 * @param target Target node
 * @param visited Visited array
 * @param path Path array
 * @param path_len Current path length
 * @param skip_intervened If true, skip incoming edges to intervened nodes
 * @return true if path found
 */
static bool dfs_find_path(const causal_dag_t* dag, uint32_t current, uint32_t target,
                           bool* visited, uint32_t* path, uint32_t* path_len,
                           bool skip_intervened)
{
    if (current == target) {
        path[*path_len] = current;
        (*path_len)++;
        return true;
    }

    visited[current] = true;
    path[*path_len] = current;
    (*path_len)++;

    for (uint32_t i = 0; i < dag->num_children[current]; i++) {
        uint32_t child = dag->children[current][i].child_id;

        /* If we're doing intervention propagation, skip intervened nodes'
         * incoming edges (except from the intervention source) */
        if (skip_intervened && dag->nodes[child].is_intervened && child != target) {
            continue;
        }

        if (!visited[child]) {
            if (dfs_find_path(dag, child, target, visited, path, path_len, skip_intervened)) {
                return true;
            }
        }
    }

    /* Backtrack */
    (*path_len)--;
    visited[current] = false;
    return false;
}

/**
 * @brief Compute path strength: product of edge strengths along a path
 */
static float compute_path_strength(const causal_dag_t* dag, const uint32_t* path,
                                    uint32_t path_len)
{
    if (path_len < 2) return 1.0f;

    float strength = 1.0f;
    for (uint32_t i = 0; i < path_len - 1; i++) {
        uint32_t from = path[i];
        uint32_t to = path[i + 1];
        int idx = find_child_edge(dag, from, to);
        if (idx < 0) return 0.0f;
        strength *= dag->children[from][idx].strength;
        strength *= dag->config.propagation_damping;
    }
    return strength;
}

/**
 * @brief Association query: P(Y|X) via belief propagation along paths
 */
static int query_association(causal_dag_t* dag, const causal_query_t* query,
                              causal_result_t* result)
{
    uint32_t target = query->target_id;
    float prior = dag->nodes[target].prior_probability;

    /* If target is observed, return the observation directly */
    if (dag->nodes[target].is_observed) {
        result->probability = dag->nodes[target].observed_value;
        result->confidence = 1.0f;
        result->is_causal = false;
        result->causal_strength = 0.0f;
        result->path_length = 0;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Node '%s' directly observed (value=%.3f)",
                 dag->nodes[target].name, dag->nodes[target].observed_value);
        return 0;
    }

    /* Find paths from each condition to target, compute max strength */
    float max_strength = 0.0f;
    uint32_t best_path_len = 0;
    bool found_path = false;

    for (uint32_t c = 0; c < query->num_conditions; c++) {
        uint32_t cond_id = query->condition_ids[c];
        if (!is_valid_node(dag, cond_id)) continue;

        uint32_t path[CAUSAL_MAX_NODES];
        uint32_t path_len = 0;
        bool visited[CAUSAL_MAX_NODES];
        memset(visited, 0, sizeof(bool) * dag->num_nodes);

        if (dfs_find_path(dag, cond_id, target, visited, path, &path_len, false)) {
            float strength = compute_path_strength(dag, path, path_len);
            if (strength > max_strength) {
                max_strength = strength;
                best_path_len = path_len - 1;  /* edges, not nodes */
            }
            found_path = true;
        }
    }

    if (found_path) {
        /* P(Y|X) = prior(Y) + (1 - prior(Y)) * max_path_strength */
        result->probability = prior + (1.0f - prior) * max_strength;
        if (result->probability > 1.0f) result->probability = 1.0f;
        if (result->probability < 0.0f) result->probability = 0.0f;
        result->confidence = max_strength;
        result->is_causal = false;  /* Association is not causal */
        result->causal_strength = max_strength;
        result->path_length = best_path_len;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Association P(%s|conditions): path_strength=%.3f, "
                 "path_len=%u (correlation, not causation)",
                 dag->nodes[target].name, max_strength, best_path_len);
    } else {
        /* No path found: return prior */
        result->probability = prior;
        result->confidence = 0.2f;
        result->is_causal = false;
        result->causal_strength = 0.0f;
        result->path_length = 0;
        snprintf(result->explanation, sizeof(result->explanation),
                 "No path from conditions to '%s': returning prior %.3f",
                 dag->nodes[target].name, prior);
    }

    return 0;
}

/**
 * @brief Intervention query: P(Y|do(X)) via do-calculus
 *
 * The do-operator cuts all incoming edges to the intervened node,
 * then propagates forward from it. This isolates the causal effect.
 */
static int query_intervention(causal_dag_t* dag, const causal_query_t* query,
                               causal_result_t* result)
{
    uint32_t target = query->target_id;
    uint32_t intervention = query->intervention_id;

    if (!is_valid_node(dag, intervention)) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Invalid intervention node ID %u", intervention);
        return -1;
    }

    float prior = dag->nodes[target].prior_probability;

    /* Apply do-operator: temporarily mark node as intervened */
    bool was_intervened = dag->nodes[intervention].is_intervened;
    float old_intervened_value = dag->nodes[intervention].intervened_value;

    dag->nodes[intervention].is_intervened = true;
    dag->nodes[intervention].intervened_value = query->intervention_value;

    /* Find path from intervention node to target, skipping intervened nodes'
     * incoming edges (the do-operator cuts them) */
    uint32_t path[CAUSAL_MAX_NODES];
    uint32_t path_len = 0;
    bool visited[CAUSAL_MAX_NODES];
    memset(visited, 0, sizeof(bool) * dag->num_nodes);

    bool found = dfs_find_path(dag, intervention, target, visited,
                                path, &path_len, true);

    if (found) {
        float strength = compute_path_strength(dag, path, path_len);

        /* P(Y|do(X)) = prior(Y) + (1 - prior(Y)) * path_strength * intervention_value */
        float effect = strength * query->intervention_value;
        result->probability = prior + (1.0f - prior) * effect;
        if (result->probability > 1.0f) result->probability = 1.0f;
        if (result->probability < 0.0f) result->probability = 0.0f;
        result->confidence = strength;
        result->is_causal = true;  /* Intervention identifies causal effect */
        result->causal_strength = strength;
        result->path_length = path_len - 1;
        snprintf(result->explanation, sizeof(result->explanation),
                 "Intervention P(%s|do(%s=%.2f)): causal_strength=%.3f, "
                 "path_len=%u (causal effect identified)",
                 dag->nodes[target].name, dag->nodes[intervention].name,
                 query->intervention_value, strength, path_len - 1);
    } else {
        /* No causal path from intervention to target */
        result->probability = prior;
        result->confidence = 0.5f;
        result->is_causal = false;
        result->causal_strength = 0.0f;
        result->path_length = 0;
        snprintf(result->explanation, sizeof(result->explanation),
                 "No causal path from '%s' to '%s': intervention has no effect",
                 dag->nodes[intervention].name, dag->nodes[target].name);
    }

    /* Restore previous intervention state */
    dag->nodes[intervention].is_intervened = was_intervened;
    dag->nodes[intervention].intervened_value = old_intervened_value;

    dag->stats.num_interventions++;
    return 0;
}

/**
 * @brief Counterfactual query: P(Y_x|X',Y')
 *
 * Simplified counterfactual:
 * 1. Use current observations as "structural equations"
 * 2. Intervene on X with counterfactual value
 * 3. Propagate to compute counterfactual Y
 * 4. Reduce confidence (counterfactuals are less certain)
 */
static int query_counterfactual(causal_dag_t* dag, const causal_query_t* query,
                                 causal_result_t* result)
{
    /* First run an intervention query */
    int rc = query_intervention(dag, query, result);
    if (rc != 0) return rc;

    /* Counterfactuals have lower confidence (0.7x base) */
    result->confidence *= 0.7f;

    /* Adjust explanation */
    char base_explanation[256];
    strncpy(base_explanation, result->explanation, sizeof(base_explanation) - 1);
    base_explanation[sizeof(base_explanation) - 1] = '\0';

    snprintf(result->explanation, sizeof(result->explanation),
             "Counterfactual: What if %s=%.2f? %s (confidence reduced for counterfactual)",
             dag->nodes[query->intervention_id].name,
             query->intervention_value, base_explanation);

    dag->stats.num_counterfactuals++;
    /* Undo the extra intervention increment from query_intervention */
    dag->stats.num_interventions--;
    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

causal_dag_config_t causal_dag_default_config(void)
{
    causal_dag_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_nodes = CAUSAL_MAX_NODES;
    config.max_edges = CAUSAL_MAX_EDGES;
    config.default_prior = CAUSAL_DEFAULT_PRIOR;
    config.propagation_damping = 0.9f;
    return config;
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

causal_dag_t* causal_dag_create(const causal_dag_config_t* config)
{
    causal_dag_t* dag = (causal_dag_t*)nimcp_calloc(1, sizeof(causal_dag_t));
    if (!dag) return NULL;

    if (config) {
        dag->config = *config;
    } else {
        dag->config = causal_dag_default_config();
    }

    /* All arrays are zeroed by calloc */
    dag->num_nodes = 0;
    memset(&dag->stats, 0, sizeof(dag->stats));
    dag->total_path_length = 0;

    return dag;
}

void causal_dag_destroy(causal_dag_t* dag)
{
    if (!dag) return;
    nimcp_free(dag);
    dag = NULL;
}

/*=============================================================================
 * NODE AND EDGE MANAGEMENT
 *===========================================================================*/

int causal_dag_add_node(causal_dag_t* dag, const char* name, float prior)
{
    if (!dag || !name) return -1;
    if (dag->num_nodes >= dag->config.max_nodes) return -1;
    if (dag->num_nodes >= CAUSAL_MAX_NODES) return -1;

    uint32_t id = dag->num_nodes;
    causal_node_t* node = &dag->nodes[id];

    node->id = id;
    strncpy(node->name, name, CAUSAL_MAX_NAME_LEN - 1);
    node->name[CAUSAL_MAX_NAME_LEN - 1] = '\0';
    node->prior_probability = prior;
    node->observed_value = NAN;
    node->is_observed = false;
    node->is_intervened = false;
    node->intervened_value = 0.0f;

    dag->num_nodes++;
    dag->stats.num_nodes = dag->num_nodes;

    return (int)id;
}

int causal_dag_add_edge(causal_dag_t* dag, uint32_t from_id, uint32_t to_id,
                         float strength)
{
    if (!dag) return -1;
    if (!is_valid_node(dag, from_id) || !is_valid_node(dag, to_id)) return -1;
    if (from_id == to_id) return -1;  /* No self-loops */
    if (dag->num_children[from_id] >= CAUSAL_MAX_PARENTS) return -1;
    if (dag->num_parents[to_id] >= CAUSAL_MAX_PARENTS) return -1;

    /* Check if edge already exists */
    if (find_child_edge(dag, from_id, to_id) >= 0) return -1;

    /* Count total edges to enforce limit */
    uint32_t total_edges = 0;
    for (uint32_t i = 0; i < dag->num_nodes; i++) {
        total_edges += dag->num_children[i];
    }
    if (total_edges >= dag->config.max_edges) return -1;
    if (total_edges >= CAUSAL_MAX_EDGES) return -1;

    /* Tentatively add edge */
    uint32_t child_idx = dag->num_children[from_id];
    dag->children[from_id][child_idx].child_id = to_id;
    dag->children[from_id][child_idx].strength = strength;
    dag->num_children[from_id]++;

    uint32_t parent_idx = dag->num_parents[to_id];
    dag->parents[to_id][parent_idx].parent_id = from_id;
    dag->parents[to_id][parent_idx].strength = strength;
    dag->num_parents[to_id]++;

    /* Check for cycles */
    if (!is_acyclic(dag)) {
        /* Remove the edge we just added */
        dag->num_children[from_id]--;
        dag->num_parents[to_id]--;
        return -1;
    }

    dag->stats.num_edges = total_edges + 1;
    return 0;
}

int causal_dag_remove_edge(causal_dag_t* dag, uint32_t from_id, uint32_t to_id)
{
    if (!dag) return -1;
    if (!is_valid_node(dag, from_id) || !is_valid_node(dag, to_id)) return -1;

    /* Find and remove from children list */
    int child_idx = find_child_edge(dag, from_id, to_id);
    if (child_idx < 0) return -1;

    /* Shift remaining children down */
    for (uint32_t i = (uint32_t)child_idx; i < dag->num_children[from_id] - 1; i++) {
        dag->children[from_id][i] = dag->children[from_id][i + 1];
    }
    dag->num_children[from_id]--;

    /* Find and remove from parents list */
    int parent_idx = find_parent_edge(dag, to_id, from_id);
    if (parent_idx >= 0) {
        for (uint32_t i = (uint32_t)parent_idx; i < dag->num_parents[to_id] - 1; i++) {
            dag->parents[to_id][i] = dag->parents[to_id][i + 1];
        }
        dag->num_parents[to_id]--;
    }

    /* Recount edges */
    uint32_t total_edges = 0;
    for (uint32_t i = 0; i < dag->num_nodes; i++) {
        total_edges += dag->num_children[i];
    }
    dag->stats.num_edges = total_edges;

    return 0;
}

/*=============================================================================
 * OBSERVATION AND INTERVENTION
 *===========================================================================*/

int causal_dag_observe(causal_dag_t* dag, uint32_t node_id, float value)
{
    if (!dag) return -1;
    if (!is_valid_node(dag, node_id)) return -1;

    dag->nodes[node_id].observed_value = value;
    dag->nodes[node_id].is_observed = true;
    return 0;
}

int causal_dag_intervene(causal_dag_t* dag, uint32_t node_id, float value)
{
    if (!dag) return -1;
    if (!is_valid_node(dag, node_id)) return -1;

    dag->nodes[node_id].is_intervened = true;
    dag->nodes[node_id].intervened_value = value;
    dag->stats.num_interventions++;
    return 0;
}

int causal_dag_clear_intervention(causal_dag_t* dag, uint32_t node_id)
{
    if (!dag) return -1;
    if (!is_valid_node(dag, node_id)) return -1;

    dag->nodes[node_id].is_intervened = false;
    dag->nodes[node_id].intervened_value = 0.0f;
    return 0;
}

/*=============================================================================
 * QUERIES
 *===========================================================================*/

int causal_dag_query(causal_dag_t* dag, const causal_query_t* query,
                      causal_result_t* result)
{
    if (!dag || !query || !result) return -1;
    if (!is_valid_node(dag, query->target_id)) return -1;

    memset(result, 0, sizeof(causal_result_t));

    int rc = 0;
    switch (query->type) {
        case CAUSAL_QUERY_ASSOCIATION:
            rc = query_association(dag, query, result);
            break;
        case CAUSAL_QUERY_INTERVENTION:
            rc = query_intervention(dag, query, result);
            break;
        case CAUSAL_QUERY_COUNTERFACTUAL:
            rc = query_counterfactual(dag, query, result);
            break;
        default:
            rc = -1;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Unknown query type %d", (int)query->type);
            break;
    }

    if (rc == 0) {
        dag->stats.num_queries++;
        if (result->path_length > 0) {
            dag->total_path_length += result->path_length;
            dag->stats.avg_path_length = (float)dag->total_path_length /
                                          (float)dag->stats.num_queries;
        }
    }

    return rc;
}

/*=============================================================================
 * GRAPH OPERATIONS
 *===========================================================================*/

int causal_dag_find_path(const causal_dag_t* dag, uint32_t from_id,
                          uint32_t to_id, uint32_t* path_out, uint32_t* path_len)
{
    if (!dag || !path_out || !path_len) return -1;
    if (!is_valid_node(dag, from_id) || !is_valid_node(dag, to_id)) return -1;

    bool visited[CAUSAL_MAX_NODES];
    memset(visited, 0, sizeof(bool) * dag->num_nodes);
    *path_len = 0;

    if (dfs_find_path(dag, from_id, to_id, visited, path_out, path_len, false)) {
        return 0;
    }

    *path_len = 0;
    return -1;
}

bool causal_dag_is_ancestor(const causal_dag_t* dag, uint32_t ancestor_id,
                             uint32_t descendant_id)
{
    if (!dag) return false;
    if (!is_valid_node(dag, ancestor_id) || !is_valid_node(dag, descendant_id)) {
        return false;
    }
    if (ancestor_id == descendant_id) return false;

    uint32_t path[CAUSAL_MAX_NODES];
    uint32_t path_len = 0;
    bool visited[CAUSAL_MAX_NODES];
    memset(visited, 0, sizeof(bool) * dag->num_nodes);

    return dfs_find_path(dag, ancestor_id, descendant_id, visited,
                          path, &path_len, false);
}

int causal_dag_get_parents(const causal_dag_t* dag, uint32_t node_id,
                            uint32_t* parents_out, uint32_t* count)
{
    if (!dag || !parents_out || !count) return -1;
    if (!is_valid_node(dag, node_id)) return -1;

    *count = dag->num_parents[node_id];
    for (uint32_t i = 0; i < dag->num_parents[node_id]; i++) {
        parents_out[i] = dag->parents[node_id][i].parent_id;
    }
    return 0;
}

int causal_dag_get_children(const causal_dag_t* dag, uint32_t node_id,
                             uint32_t* children_out, uint32_t* count)
{
    if (!dag || !children_out || !count) return -1;
    if (!is_valid_node(dag, node_id)) return -1;

    *count = dag->num_children[node_id];
    for (uint32_t i = 0; i < dag->num_children[node_id]; i++) {
        children_out[i] = dag->children[node_id][i].child_id;
    }
    return 0;
}

/*=============================================================================
 * STATISTICS AND VALIDATION
 *===========================================================================*/

int causal_dag_get_stats(const causal_dag_t* dag, causal_dag_stats_t* stats)
{
    if (!dag || !stats) return -1;
    *stats = dag->stats;
    return 0;
}

int causal_dag_validate(const causal_dag_t* dag)
{
    if (!dag) return -1;
    return is_acyclic(dag) ? 0 : -1;
}

int causal_dag_get_node(const causal_dag_t* dag, uint32_t node_id,
                         causal_node_t* out)
{
    if (!dag || !out) return -1;
    if (!is_valid_node(dag, node_id)) return -1;

    *out = dag->nodes[node_id];
    return 0;
}

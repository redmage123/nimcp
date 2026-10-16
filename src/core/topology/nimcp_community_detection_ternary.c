/**
 * @file nimcp_community_detection_ternary.c
 * @brief Ternary Community Detection Implementation
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Community detection on ternary adjacency graphs
 * WHY:  Detect functional modules in networks with signed connections
 * HOW:  Extended Louvain algorithm for ternary adjacency matrices
 *
 * @author NIMCP Development Team
 */

#include "core/topology/nimcp_community_detection_ternary.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(community_detection_ternary)

/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Compute node degree (positive and negative separately)
 */
static void compute_degrees(
    const trit_matrix_t* adj,
    uint32_t node,
    int32_t* degree_pos,
    int32_t* degree_neg
) {
    int32_t pos = 0, neg = 0;
    uint32_t n = adj->rows;

    for (uint32_t j = 0; j < n; j++) {
        trit_t val = trit_matrix_get(adj, node, j);
        if (val == TRIT_POSITIVE) pos++;
        else if (val == TRIT_NEGATIVE) neg++;
    }

    if (degree_pos) *degree_pos = pos;
    if (degree_neg) *degree_neg = neg;
}

/**
 * @brief Compute sum of edge weights within community
 */
static void compute_community_edges(
    const trit_matrix_t* adj,
    const uint32_t* community_ids,
    uint32_t num_nodes,
    uint32_t community,
    int32_t* sum_pos,
    int32_t* sum_neg
) {
    int32_t pos = 0, neg = 0;

    for (uint32_t i = 0; i < num_nodes; i++) {
        if (community_ids[i] != community) continue;

        for (uint32_t j = i + 1; j < num_nodes; j++) {
            if (community_ids[j] != community) continue;

            trit_t val = trit_matrix_get(adj, i, j);
            if (val == TRIT_POSITIVE) pos++;
            else if (val == TRIT_NEGATIVE) neg++;
        }
    }

    if (sum_pos) *sum_pos = pos;
    if (sum_neg) *sum_neg = neg;
}

/**
 * @brief Compute total edges in graph
 */
static void compute_total_edges(
    const trit_matrix_t* adj,
    int32_t* total_pos,
    int32_t* total_neg
) {
    int32_t pos = 0, neg = 0;
    uint32_t n = adj->rows;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            trit_t val = trit_matrix_get(adj, i, j);
            if (val == TRIT_POSITIVE) pos++;
            else if (val == TRIT_NEGATIVE) neg++;
        }
    }

    if (total_pos) *total_pos = pos;
    if (total_neg) *total_neg = neg;
}

/**
 * @brief Compute edges between node and community
 */
static void compute_node_community_edges(
    const trit_matrix_t* adj,
    const uint32_t* community_ids,
    uint32_t num_nodes,
    uint32_t node,
    uint32_t community,
    int32_t* edges_pos,
    int32_t* edges_neg
) {
    int32_t pos = 0, neg = 0;

    for (uint32_t j = 0; j < num_nodes; j++) {
        if (j == node || community_ids[j] != community) continue;

        trit_t val = trit_matrix_get(adj, node, j);
        if (val == TRIT_POSITIVE) pos++;
        else if (val == TRIT_NEGATIVE) neg++;
    }

    if (edges_pos) *edges_pos = pos;
    if (edges_neg) *edges_neg = neg;
}

/*=============================================================================
 * Ternary Adjacency Creation
 *===========================================================================*/

trit_matrix_t* community_ternary_quantize_adjacency(
    const float* float_adj,
    uint32_t num_nodes,
    float threshold,
    ternary_pack_mode_t pack_mode
) {
    if (!float_adj || num_nodes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "community_ternary_quantize_adjacency: float_adj is NULL");
        return NULL;
    }

    trit_matrix_t* adj = trit_matrix_create(num_nodes, num_nodes, pack_mode);
    if (!adj) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adj is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < num_nodes; i++) {
        for (uint32_t j = 0; j < num_nodes; j++) {
            float w = float_adj[i * num_nodes + j];
            trit_t t;

            if (fabsf(w) < threshold) {
                t = TRIT_UNKNOWN;
            } else if (w > 0) {
                t = TRIT_POSITIVE;
            } else {
                t = TRIT_NEGATIVE;
            }

            trit_matrix_set(adj, i, j, t);
        }
    }

    return adj;
}

/*=============================================================================
 * Community Detection
 *===========================================================================*/

community_ternary_result_t* community_ternary_detect(
    const trit_matrix_t* adjacency,
    const community_ternary_config_t* config
) {
    /* Guard: validate inputs */
    if (!adjacency || adjacency->magic != TERNARY_MAGIC) {
        NIMCP_LOGGING_ERROR("Invalid adjacency matrix");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_ternary_detect: adjacency is NULL");
        return NULL;
    }

    if (adjacency->rows != adjacency->cols) {
        NIMCP_LOGGING_ERROR("Adjacency must be square");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_ternary_detect: validation failed");
        return NULL;
    }

    uint32_t n = adjacency->rows;

    /* Use defaults if no config */
    community_ternary_config_t default_config;
    if (!config) {
        community_ternary_config_default(&default_config);
        config = &default_config;
    }

    /* Allocate result */
    community_ternary_result_t* result = nimcp_malloc(sizeof(community_ternary_result_t));
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "result is NULL");

        return NULL;

    }
    memset(result, 0, sizeof(community_ternary_result_t));

    result->magic = COMMUNITY_TERNARY_MAGIC;
    result->num_nodes = n;

    /* Allocate community IDs */
    result->community_ids = nimcp_malloc(n * sizeof(uint32_t));
    if (!result->community_ids) {
        nimcp_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_ternary_detect: result->community_ids is NULL");
        return NULL;
    }

    /* Initialize each node in own community */
    for (uint32_t i = 0; i < n; i++) {
        result->community_ids[i] = i;
    }

    /* Compute total edges */
    int32_t m_pos, m_neg;
    compute_total_edges(adjacency, &m_pos, &m_neg);
    result->total_positive = (uint32_t)m_pos;
    result->total_negative = (uint32_t)m_neg;
    result->total_absent = n * n - m_pos - m_neg;

    if (m_neg > 0) {
        result->network_ei_ratio = (float)m_pos / (float)m_neg;
    } else {
        result->network_ei_ratio = (m_pos > 0) ? INFINITY : 1.0f;
    }

    float resolution = config->resolution;

    /* Phase 1: Local optimization (Louvain) */
    bool improved = true;
    uint32_t iteration = 0;

    while (improved && iteration < config->max_iterations) {
        improved = false;
        iteration++;

        for (uint32_t i = 0; i < n; i++) {
            uint32_t current_comm = result->community_ids[i];
            float best_gain = 0.0f;
            uint32_t best_comm = current_comm;

            /* Try moving to each neighbor's community */
            for (uint32_t j = 0; j < n; j++) {
                if (j == i) continue;

                trit_t edge = trit_matrix_get(adjacency, i, j);
                if (edge == TRIT_UNKNOWN) continue;  /* No edge */

                uint32_t target_comm = result->community_ids[j];
                if (target_comm == current_comm) continue;

                /* Compute modularity gain */
                float gain = community_ternary_modularity_gain(
                    adjacency, result->community_ids, i, target_comm, resolution
                );

                if (gain > best_gain + config->min_modularity_gain) {
                    best_gain = gain;
                    best_comm = target_comm;
                }
            }

            /* Make the move if improvement found */
            if (best_comm != current_comm) {
                result->community_ids[i] = best_comm;
                improved = true;
            }
        }
    }

    /* Renumber communities consecutively */
    uint32_t* community_map = nimcp_malloc(n * sizeof(uint32_t));
    bool* seen = nimcp_malloc(n * sizeof(bool));
    if (!community_map || !seen) {
        if (community_map) nimcp_free(community_map);
        if (seen) nimcp_free(seen);
        community_ternary_result_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_ternary_detect: validation failed");
        return NULL;
    }

    memset(seen, 0, n * sizeof(bool));
    uint32_t num_communities = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t old_comm = result->community_ids[i];
        if (!seen[old_comm]) {
            community_map[old_comm] = num_communities++;
            seen[old_comm] = true;
        }
        result->community_ids[i] = community_map[old_comm];
    }

    nimcp_free(community_map);
    nimcp_free(seen);

    result->num_communities = num_communities;

    /* Allocate per-community arrays */
    result->community_sizes = nimcp_malloc(num_communities * sizeof(uint32_t));
    result->ei_ratio = nimcp_malloc(num_communities * sizeof(float));
    result->internal_positive = nimcp_malloc(num_communities * sizeof(uint32_t));
    result->internal_negative = nimcp_malloc(num_communities * sizeof(uint32_t));
    result->external_positive = nimcp_malloc(num_communities * sizeof(uint32_t));
    result->external_negative = nimcp_malloc(num_communities * sizeof(uint32_t));

    if (!result->community_sizes || !result->ei_ratio ||
        !result->internal_positive || !result->internal_negative ||
        !result->external_positive || !result->external_negative) {
        community_ternary_result_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "community_ternary_detect: operation failed");
        return NULL;
    }

    memset(result->community_sizes, 0, num_communities * sizeof(uint32_t));

    /* Count community sizes */
    for (uint32_t i = 0; i < n; i++) {
        result->community_sizes[result->community_ids[i]]++;
    }

    /* Compute per-community statistics */
    for (uint32_t c = 0; c < num_communities; c++) {
        community_ternary_edge_counts(
            adjacency, result->community_ids, c,
            &result->internal_positive[c],
            &result->internal_negative[c],
            &result->external_positive[c],
            &result->external_negative[c]
        );

        if (result->internal_negative[c] > 0) {
            result->ei_ratio[c] = (float)result->internal_positive[c] /
                                  (float)result->internal_negative[c];
        } else {
            result->ei_ratio[c] = (result->internal_positive[c] > 0) ? INFINITY : 1.0f;
        }
    }

    /* Compute final modularity scores */
    result->modularity_signed = community_ternary_modularity_signed(
        adjacency, result->community_ids, num_communities, resolution
    );

    result->modularity = community_ternary_modularity_unsigned(
        adjacency, result->community_ids, num_communities
    );

    return result;
}

void community_ternary_result_free(community_ternary_result_t* result) {
    if (!result) return;
    if (result->magic != COMMUNITY_TERNARY_MAGIC) return;

    if (result->community_ids) nimcp_free(result->community_ids);
    if (result->community_sizes) nimcp_free(result->community_sizes);
    if (result->ei_ratio) nimcp_free(result->ei_ratio);
    if (result->internal_positive) nimcp_free(result->internal_positive);
    if (result->internal_negative) nimcp_free(result->internal_negative);
    if (result->external_positive) nimcp_free(result->external_positive);
    if (result->external_negative) nimcp_free(result->external_negative);

    result->magic = 0;
    nimcp_free(result);
}

/*=============================================================================
 * Modularity Computation
 *===========================================================================*/

float community_ternary_modularity_signed(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t num_communities,
    float resolution
) {
    if (!adjacency || !community_ids || adjacency->magic != TERNARY_MAGIC) {
        return 0.0f;
    }

    uint32_t n = adjacency->rows;

    /* Compute total edges */
    int32_t m_pos = 0, m_neg = 0;
    compute_total_edges(adjacency, &m_pos, &m_neg);

    if (m_pos + m_neg == 0) return 0.0f;

    float m = (float)(m_pos + m_neg);

    /* Compute modularity */
    float Q = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        int32_t ki_pos, ki_neg;
        compute_degrees(adjacency, i, &ki_pos, &ki_neg);

        for (uint32_t j = i + 1; j < n; j++) {
            if (community_ids[i] != community_ids[j]) continue;

            int32_t kj_pos, kj_neg;
            compute_degrees(adjacency, j, &kj_pos, &kj_neg);

            trit_t a_ij = trit_matrix_get(adjacency, i, j);
            float expected = resolution * (
                (float)(ki_pos * kj_pos - ki_neg * kj_neg) / (2.0f * m)
            );

            Q += 2.0f * ((float)a_ij - expected);
        }
    }

    return Q / (2.0f * m);
}

float community_ternary_modularity_unsigned(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t num_communities
) {
    if (!adjacency || !community_ids || adjacency->magic != TERNARY_MAGIC) {
        return 0.0f;
    }

    uint32_t n = adjacency->rows;

    /* Compute total edges (treating all as positive) */
    int32_t m = 0;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            trit_t val = trit_matrix_get(adjacency, i, j);
            if (val != TRIT_UNKNOWN) m++;
        }
    }

    if (m == 0) return 0.0f;

    /* Compute degrees (unsigned) */
    uint32_t* degrees = nimcp_malloc(n * sizeof(uint32_t));
    if (!degrees) return 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        degrees[i] = 0;
        for (uint32_t j = 0; j < n; j++) {
            trit_t val = trit_matrix_get(adjacency, i, j);
            if (val != TRIT_UNKNOWN) degrees[i]++;
        }
    }

    /* Compute modularity */
    float Q = 0.0f;
    float two_m = 2.0f * (float)m;

    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (community_ids[i] != community_ids[j]) continue;

            trit_t a_ij = trit_matrix_get(adjacency, i, j);
            float connected = (a_ij != TRIT_UNKNOWN) ? 1.0f : 0.0f;
            float expected = (float)(degrees[i] * degrees[j]) / two_m;

            Q += 2.0f * (connected - expected);
        }
    }

    nimcp_free(degrees);
    return Q / two_m;
}

float community_ternary_modularity_gain(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t node,
    uint32_t target_community,
    float resolution
) {
    if (!adjacency || !community_ids || adjacency->magic != TERNARY_MAGIC) {
        return 0.0f;
    }

    uint32_t n = adjacency->rows;
    uint32_t current_comm = community_ids[node];

    if (current_comm == target_community) return 0.0f;

    /* Edges between node and current community */
    int32_t ki_in_pos, ki_in_neg;
    compute_node_community_edges(
        adjacency, community_ids, n, node, current_comm,
        &ki_in_pos, &ki_in_neg
    );

    /* Edges between node and target community */
    int32_t ki_out_pos, ki_out_neg;
    compute_node_community_edges(
        adjacency, community_ids, n, node, target_community,
        &ki_out_pos, &ki_out_neg
    );

    /* Node degrees */
    int32_t ki_pos, ki_neg;
    compute_degrees(adjacency, node, &ki_pos, &ki_neg);

    /* Total edges */
    int32_t m_pos, m_neg;
    compute_total_edges(adjacency, &m_pos, &m_neg);
    float m = (float)(m_pos + m_neg);

    if (m == 0) return 0.0f;

    /* Compute sum of degrees in current and target communities */
    int32_t sum_in_pos = 0, sum_in_neg = 0;
    int32_t sum_out_pos = 0, sum_out_neg = 0;

    for (uint32_t j = 0; j < n; j++) {
        if (j == node) continue;

        int32_t kj_pos, kj_neg;
        compute_degrees(adjacency, j, &kj_pos, &kj_neg);

        if (community_ids[j] == current_comm) {
            sum_in_pos += kj_pos;
            sum_in_neg += kj_neg;
        } else if (community_ids[j] == target_community) {
            sum_out_pos += kj_pos;
            sum_out_neg += kj_neg;
        }
    }

    /* Modularity gain formula for signed networks */
    float gain = 0.0f;

    /* Gain from moving into target community */
    gain += (float)(ki_out_pos - ki_out_neg);
    gain -= resolution * (float)(ki_pos * sum_out_pos - ki_neg * sum_out_neg) / (2.0f * m);

    /* Loss from leaving current community */
    gain -= (float)(ki_in_pos - ki_in_neg);
    gain += resolution * (float)(ki_pos * sum_in_pos - ki_neg * sum_in_neg) / (2.0f * m);

    return gain / m;
}

/*=============================================================================
 * Statistics
 *===========================================================================*/

float community_ternary_ei_ratio(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t community
) {
    uint32_t pos = 0, neg = 0;

    community_ternary_edge_counts(
        adjacency, community_ids, community,
        &pos, &neg, NULL, NULL
    );

    if (neg > 0) {
        return (float)pos / (float)neg;
    } else {
        return (pos > 0) ? INFINITY : 1.0f;
    }
}

void community_ternary_edge_counts(
    const trit_matrix_t* adjacency,
    const uint32_t* community_ids,
    uint32_t community,
    uint32_t* internal_pos,
    uint32_t* internal_neg,
    uint32_t* external_pos,
    uint32_t* external_neg
) {
    uint32_t int_pos = 0, int_neg = 0;
    uint32_t ext_pos = 0, ext_neg = 0;

    if (adjacency && community_ids && adjacency->magic == TERNARY_MAGIC) {
        uint32_t n = adjacency->rows;

        for (uint32_t i = 0; i < n; i++) {
            if (community_ids[i] != community) continue;

            for (uint32_t j = i + 1; j < n; j++) {
                trit_t val = trit_matrix_get(adjacency, i, j);
                if (val == TRIT_UNKNOWN) continue;

                bool j_in_comm = (community_ids[j] == community);

                if (j_in_comm) {
                    if (val == TRIT_POSITIVE) int_pos++;
                    else int_neg++;
                } else {
                    if (val == TRIT_POSITIVE) ext_pos++;
                    else ext_neg++;
                }
            }
        }
    }

    if (internal_pos) *internal_pos = int_pos;
    if (internal_neg) *internal_neg = int_neg;
    if (external_pos) *external_pos = ext_pos;
    if (external_neg) *external_neg = ext_neg;
}

/*=============================================================================
 * Configuration Helpers
 *===========================================================================*/

void community_ternary_config_default(community_ternary_config_t* config) {
    if (!config) return;

    config->max_iterations = 100;
    config->min_modularity_gain = 1e-5f;
    config->max_communities = 0;  /* No limit */
    config->resolution = COMMUNITY_TERNARY_DEFAULT_RESOLUTION;
    config->random_seed = 0;

    config->use_signed_modularity = true;
    config->negative_weight = 1.0f;
    config->separate_pos_neg = false;
}

int community_ternary_config_validate(const community_ternary_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "community_ternary_config_validate: config is NULL");
        return -1;
    }

    if (config->max_iterations == 0) {
        NIMCP_LOGGING_ERROR("max_iterations must be > 0");
        return -2;
    }

    if (config->resolution <= 0.0f) {
        NIMCP_LOGGING_ERROR("resolution must be > 0");
        return -3;
    }

    if (config->negative_weight < 0.0f) {
        NIMCP_LOGGING_ERROR("negative_weight must be >= 0");
        return -4;
    }

    return 0;
}

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

void community_ternary_print_stats(const community_ternary_result_t* result) {
    if (!result || result->magic != COMMUNITY_TERNARY_MAGIC) {
        printf("Invalid result\n");
        return;
    }

    printf("=== Ternary Community Detection Results ===\n");
    printf("Nodes: %u\n", result->num_nodes);
    printf("Communities: %u\n", result->num_communities);
    printf("Signed Modularity Q: %.4f\n", result->modularity_signed);
    printf("Unsigned Modularity Q: %.4f\n", result->modularity);
    printf("Network E/I Ratio: %.2f\n", result->network_ei_ratio);
    printf("Total Edges: +%u / -%u / absent %u\n",
           result->total_positive, result->total_negative, result->total_absent);

    printf("\nPer-Community Statistics:\n");
    for (uint32_t c = 0; c < result->num_communities && c < 10; c++) {
        printf("  Community %u: size=%u, E/I=%.2f, int(+%u,-%u), ext(+%u,-%u)\n",
               c, result->community_sizes[c], result->ei_ratio[c],
               result->internal_positive[c], result->internal_negative[c],
               result->external_positive[c], result->external_negative[c]);
    }

    if (result->num_communities > 10) {
        printf("  ... and %u more communities\n", result->num_communities - 10);
    }
}

uint32_t community_ternary_best_balanced(
    const community_ternary_result_t* result,
    float target_ratio
) {
    if (!result || result->magic != COMMUNITY_TERNARY_MAGIC) {
        return UINT32_MAX;
    }

    uint32_t best = 0;
    float best_diff = INFINITY;

    for (uint32_t c = 0; c < result->num_communities; c++) {
        float diff = fabsf(result->ei_ratio[c] - target_ratio);
        if (diff < best_diff) {
            best_diff = diff;
            best = c;
        }
    }

    return best;
}

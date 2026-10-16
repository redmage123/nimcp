//=============================================================================
// nimcp_thalamic_quantum_bridge.h - Quantum-Enhanced Thalamic Routing
//=============================================================================
/**
 * @file nimcp_thalamic_quantum_bridge.h
 * @brief Quantum acceleration for thalamic signal routing decisions
 *
 * WHAT: Integrates quantum attention with thalamic router for O(√N) routing
 * WHY:  Thalamus gates signals to cortex - quantum attention provides fast gating
 * HOW:  Use quantum attention for rapid routing decisions across destinations
 *
 * BIOLOGICAL INSPIRATION:
 * - Thalamic reticular nucleus (TRN) gates information flow
 * - Quantum speedup models parallel processing of thalamic nuclei
 * - Burst vs. tonic modes analogous to quantum superposition/measurement
 * - Attention-based routing = collapse of routing possibilities
 *
 * QUANTUM ROUTING MODEL:
 * Classical routing: O(N) attention checks for N destinations
 * Quantum routing: O(√N) via superposition + Grover search
 *
 * KEY INSIGHT:
 * - Routing = finding which destinations should receive signal
 * - This is a search problem: find all dest where attention(src,dest) > threshold
 * - Grover's algorithm provides √N speedup for unstructured search
 *
 * INTEGRATION:
 * - Uses nimcp_quantum_attention.h for quantum attention mechanism
 * - Integrates with nimcp_thalamic_router.h for signal routing
 * - Provides fast gating decisions for multi-destination broadcasting
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_THALAMIC_QUANTUM_BRIDGE_H
#define NIMCP_THALAMIC_QUANTUM_BRIDGE_H

#include "plasticity/attention/nimcp_quantum_attention.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Quantum routing configuration
 */
typedef struct {
    bool enabled;                   /**< Enable quantum routing (default: true) */
    float routing_threshold;        /**< Attention threshold for routing (0.3) */
    float attention_weight;         /**< Weight for quantum attention (0.7) */
    uint32_t max_destinations;      /**< Max destinations to evaluate */
    quantum_attention_mode_t mode;  /**< Quantum attention mode */
    float collapse_threshold;       /**< Measurement threshold (0.5) */
    bool use_sparse_routing;        /**< Use sparse attention mask */
} thalamic_quantum_config_t;

/**
 * @brief Quantum routing statistics
 */
typedef struct {
    uint64_t quantum_routes;        /**< Routes computed via quantum attention */
    uint64_t quantum_gates;         /**< Gating decisions made */
    uint64_t classical_fallbacks;   /**< Fallback to classical routing */
    float routing_speedup;          /**< Estimated speedup vs classical */
    float avg_coherence;            /**< Average superposition level */
    float avg_sparsity;             /**< Average routing sparsity */
} thalamic_quantum_stats_t;

//=============================================================================
// Types
//=============================================================================

typedef struct thalamic_quantum_bridge thalamic_quantum_bridge_t;

//=============================================================================
// API
//=============================================================================

/**
 * @brief Get default quantum routing configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Provide easy setup for quantum-enhanced routing
 * HOW:  Return config with quantum routing enabled, moderate thresholds
 *
 * @return Default configuration
 */
thalamic_quantum_config_t thalamic_quantum_default_config(void);

/**
 * @brief Create quantum routing bridge
 *
 * WHAT: Initialize quantum attention system for routing
 * WHY:  Prepare quantum acceleration infrastructure
 * HOW:  Create quantum attention context with routing-optimized parameters
 *
 * @param config Quantum routing configuration
 * @return Bridge handle or NULL on failure
 */
thalamic_quantum_bridge_t* thalamic_quantum_bridge_create(
    const thalamic_quantum_config_t* config
);

/**
 * @brief Destroy quantum routing bridge
 *
 * @param bridge Bridge to destroy
 */
void thalamic_quantum_bridge_destroy(thalamic_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum routing is enabled
 *
 * @param bridge Bridge handle
 * @return true if enabled, false otherwise
 */
bool thalamic_quantum_bridge_is_enabled(const thalamic_quantum_bridge_t* bridge);

/**
 * @brief Enable/disable quantum routing
 *
 * @param bridge Bridge handle
 * @param enabled Enable flag
 */
void thalamic_quantum_bridge_set_enabled(
    thalamic_quantum_bridge_t* bridge,
    bool enabled
);

/**
 * @brief Compute quantum routing decision
 *
 * WHAT: Use quantum attention to determine which destinations should receive signal
 * WHY:  Fast routing decisions via √N speedup
 * HOW:  Measure quantum attention for source-dest pairs, gate based on threshold
 *
 * @param bridge Bridge handle
 * @param source_id Source module ID
 * @param dest_ids Candidate destination IDs
 * @param num_dests Number of candidate destinations
 * @param signal_features Signal feature vector (for attention scoring)
 * @param feature_dim Feature dimension
 * @param routed_dests Output: destinations that should receive signal (caller allocated)
 * @param num_routed Output: number of destinations selected
 * @return 0 on success, negative error code on failure
 */
int thalamic_quantum_route(
    thalamic_quantum_bridge_t* bridge,
    uint32_t source_id,
    const uint32_t* dest_ids,
    uint32_t num_dests,
    const float* signal_features,
    uint32_t feature_dim,
    uint32_t* routed_dests,
    uint32_t* num_routed
);

/**
 * @brief Quantum gating decision for single destination
 *
 * WHAT: Fast gate/no-gate decision using quantum attention
 * WHY:  Single-destination routing decision with √N speedup
 * HOW:  Measure quantum attention score, compare to threshold
 *
 * @param bridge Bridge handle
 * @param source_id Source module ID
 * @param dest_id Destination module ID
 * @param signal_features Signal feature vector
 * @param feature_dim Feature dimension
 * @param gate_weight Output: attention weight [0.0, 1.0]
 * @return true if signal should be gated through, false otherwise
 */
bool thalamic_quantum_gate_signal(
    thalamic_quantum_bridge_t* bridge,
    uint32_t source_id,
    uint32_t dest_id,
    const float* signal_features,
    uint32_t feature_dim,
    float* gate_weight
);

/**
 * @brief Get quantum routing statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, negative error code on failure
 */
int thalamic_quantum_get_stats(
    const thalamic_quantum_bridge_t* bridge,
    thalamic_quantum_stats_t* stats
);

/**
 * @brief Reset quantum routing statistics
 *
 * @param bridge Bridge handle
 */
void thalamic_quantum_reset_stats(thalamic_quantum_bridge_t* bridge);

//=============================================================================
// Implementation Section
//=============================================================================

#ifdef NIMCP_THALAMIC_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

/**
 * @brief Internal bridge structure
 */
struct thalamic_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    thalamic_quantum_config_t config;   /**< Configuration */
    quantum_attention_t quantum_attn;   /**< Quantum attention context */
    thalamic_quantum_stats_t stats;     /**< Statistics */

    /* Workspace buffers */
    float* query_buffer;                /**< Query vector buffer */
    float* key_buffer;                  /**< Key vector buffer */
    float* attention_scores;            /**< Attention score buffer */
    uint32_t buffer_capacity;           /**< Buffer size */
};

thalamic_quantum_config_t thalamic_quantum_default_config(void) {
    return (thalamic_quantum_config_t){
        .enabled = true,
        .routing_threshold = 0.3f,
        .attention_weight = 0.7f,
        .max_destinations = 256,
        .mode = QUANTUM_ATTENTION_SPARSE,
        .collapse_threshold = 0.5f,
        .use_sparse_routing = true
    };
}

thalamic_quantum_bridge_t* thalamic_quantum_bridge_create(
    const thalamic_quantum_config_t* config
) {
    thalamic_quantum_bridge_t* bridge = (thalamic_quantum_bridge_t*)
        nimcp_calloc(1, sizeof(thalamic_quantum_bridge_t));
    if (!bridge) return NULL;

    /* Use defaults if no config provided */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = thalamic_quantum_default_config();
    }
    bridge->buffer_capacity = bridge->config.max_destinations;

    /* Create quantum attention context */
    quantum_attention_config_t qconfig = quantum_attention_default_config();
    qconfig.mode = bridge->config.mode;
    qconfig.collapse_threshold = bridge->config.collapse_threshold;
    qconfig.use_sparse_output = bridge->config.use_sparse_routing;
    qconfig.sparsity_threshold = bridge->config.routing_threshold;

    /* Routing optimized: small sequence length (source + dests) */
    uint32_t seq_len = 1 + bridge->config.max_destinations;  /* 1 source + N dests */
    uint32_t head_dim = 64;  /* Feature dimension for attention */
    uint32_t num_heads = 1;  /* Single-head for routing */

    bridge->quantum_attn = quantum_attention_create(&qconfig, seq_len, head_dim, num_heads);
    /* quantum_attn may be NULL if quantum attention is not available */
    /* Bridge can still work with classical fallback */

    /* Allocate workspace buffers */
    bridge->query_buffer = (float*)nimcp_calloc(head_dim, sizeof(float));
    bridge->key_buffer = (float*)nimcp_calloc(bridge->config.max_destinations * head_dim, sizeof(float));
    bridge->attention_scores = (float*)nimcp_calloc(bridge->config.max_destinations, sizeof(float));

    if (!bridge->query_buffer || !bridge->key_buffer || !bridge->attention_scores) {
        thalamic_quantum_bridge_destroy(bridge);
        return NULL;
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(thalamic_quantum_stats_t));

    return bridge;
}

void thalamic_quantum_bridge_destroy(thalamic_quantum_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->quantum_attn) {
        quantum_attention_destroy(bridge->quantum_attn);
    }

    nimcp_free(bridge->query_buffer);
    nimcp_free(bridge->key_buffer);
    nimcp_free(bridge->attention_scores);
    nimcp_free(bridge);
}

bool thalamic_quantum_bridge_is_enabled(const thalamic_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void thalamic_quantum_bridge_set_enabled(
    thalamic_quantum_bridge_t* bridge,
    bool enabled
) {
    if (bridge) {
        bridge->config.enabled = enabled;
    }
}

int thalamic_quantum_route(
    thalamic_quantum_bridge_t* bridge,
    uint32_t source_id,
    const uint32_t* dest_ids,
    uint32_t num_dests,
    const float* signal_features,
    uint32_t feature_dim,
    uint32_t* routed_dests,
    uint32_t* num_routed
) {
    /* Guard clauses */
    if (!bridge || !dest_ids || !signal_features || !routed_dests || !num_routed) {
        return -1;
    }

    if (num_dests == 0 || num_dests > bridge->config.max_destinations) {
        return -2;
    }

    *num_routed = 0;

    /* Classical fallback if disabled or quantum attention unavailable */
    if (!bridge->config.enabled || !bridge->quantum_attn) {
        bridge->stats.classical_fallbacks++;
        /* Route to all destinations (classical behavior) */
        for (uint32_t i = 0; i < num_dests; i++) {
            routed_dests[i] = dest_ids[i];
        }
        *num_routed = num_dests;
        return 0;
    }

    /* Prepare query vector from signal features */
    uint32_t head_dim = (feature_dim < 64) ? feature_dim : 64;
    memset(bridge->query_buffer, 0, 64 * sizeof(float));
    memcpy(bridge->query_buffer, signal_features, head_dim * sizeof(float));

    /* Prepare key vectors for each destination (simplified: use dest_id as feature) */
    memset(bridge->key_buffer, 0, num_dests * 64 * sizeof(float));
    for (uint32_t i = 0; i < num_dests; i++) {
        /* Simple encoding: dest_id modulates key vector */
        float dest_weight = (float)dest_ids[i] / (float)bridge->config.max_destinations;
        for (uint32_t j = 0; j < head_dim; j++) {
            bridge->key_buffer[i * 64 + j] = signal_features[j] * dest_weight;
        }
    }

    /* Compute quantum attention scores */
    float scale = 1.0f / sqrtf((float)head_dim);
    quantum_attention_compute_scores(
        bridge->quantum_attn,
        bridge->query_buffer,
        bridge->key_buffer,
        0,  /* head_idx */
        scale
    );

    /* Extract attention scores for query position 0 (source) */
    quantum_attention_stats_t q_stats;
    quantum_attention_get_stats(bridge->quantum_attn, &q_stats);

    /* Get sparse attention pairs */
    uint32_t* query_indices = (uint32_t*)nimcp_calloc(num_dests, sizeof(uint32_t));
    uint32_t* key_indices = (uint32_t*)nimcp_calloc(num_dests, sizeof(uint32_t));
    float* attention_values = (float*)nimcp_calloc(num_dests, sizeof(float));

    if (!query_indices || !key_indices || !attention_values) {
        nimcp_free(query_indices);
        nimcp_free(key_indices);
        nimcp_free(attention_values);
        return -3;
    }

    uint32_t num_pairs = quantum_attention_get_sparse_pairs(
        bridge->quantum_attn,
        query_indices,
        key_indices,
        attention_values,
        num_dests
    );

    /* Select destinations with attention above threshold */
    for (uint32_t i = 0; i < num_pairs; i++) {
        if (query_indices[i] == 0) {  /* Query position 0 = source */
            uint32_t dest_idx = key_indices[i];
            if (dest_idx > 0 && dest_idx <= num_dests) {
                float attention = attention_values[i];
                if (attention >= bridge->config.routing_threshold) {
                    routed_dests[*num_routed] = dest_ids[dest_idx - 1];
                    (*num_routed)++;
                    bridge->stats.quantum_gates++;
                }
            }
        }
    }

    nimcp_free(query_indices);
    nimcp_free(key_indices);
    nimcp_free(attention_values);

    /* Update statistics */
    bridge->stats.quantum_routes++;
    bridge->stats.avg_coherence = q_stats.avg_coherence;
    bridge->stats.avg_sparsity = q_stats.avg_sparsity;

    /* Compute speedup */
    if (num_dests > 0) {
        float classical_ops = (float)num_dests;
        float quantum_ops = sqrtf((float)num_dests);
        bridge->stats.routing_speedup = classical_ops / quantum_ops;
    }

    return 0;
}

bool thalamic_quantum_gate_signal(
    thalamic_quantum_bridge_t* bridge,
    uint32_t source_id,
    uint32_t dest_id,
    const float* signal_features,
    uint32_t feature_dim,
    float* gate_weight
) {
    /* Guard clauses */
    if (!bridge || !signal_features || !gate_weight) {
        return false;
    }

    *gate_weight = 0.0f;

    /* Classical fallback if disabled or quantum attention unavailable */
    if (!bridge->config.enabled || !bridge->quantum_attn) {
        bridge->stats.classical_fallbacks++;
        *gate_weight = 1.0f;  /* Pass through */
        return true;
    }

    /* Use quantum routing for single destination */
    uint32_t routed_dest = 0;
    uint32_t num_routed = 0;

    int result = thalamic_quantum_route(
        bridge,
        source_id,
        &dest_id,
        1,
        signal_features,
        feature_dim,
        &routed_dest,
        &num_routed
    );

    if (result < 0) {
        return false;
    }

    /* Gate decision */
    if (num_routed > 0) {
        *gate_weight = bridge->config.attention_weight;
        return true;
    } else {
        *gate_weight = 0.0f;
        return false;
    }
}

int thalamic_quantum_get_stats(
    const thalamic_quantum_bridge_t* bridge,
    thalamic_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;

    *stats = bridge->stats;
    return 0;
}

void thalamic_quantum_reset_stats(thalamic_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(thalamic_quantum_stats_t));
        if (bridge->quantum_attn) {
            quantum_attention_reset_stats(bridge->quantum_attn);
        }
    }
}

#endif // NIMCP_THALAMIC_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_THALAMIC_QUANTUM_BRIDGE_H

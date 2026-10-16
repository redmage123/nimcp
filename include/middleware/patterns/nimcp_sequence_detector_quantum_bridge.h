/**
 * @file nimcp_sequence_detector_quantum_bridge.h
 * @brief Quantum-accelerated sequence pattern matching
 *
 * WHAT: Integrates quantum sequence matcher with pattern detector
 * WHY:  O(√N) speedup for finding patterns in spike sequences
 * HOW:  Grover search with amplitude-encoded templates
 *
 * BIOLOGICAL INSPIRATION:
 * - Hippocampal sequence replay
 * - Motor sequence learning in basal ganglia
 * - Speech pattern recognition in auditory cortex
 */

#ifndef NIMCP_SEQUENCE_DETECTOR_QUANTUM_BRIDGE_H
#define NIMCP_SEQUENCE_DETECTOR_QUANTUM_BRIDGE_H

#include "middleware/patterns/nimcp_sequence_detector.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/patterns/nimcp_quantum_sequence_matcher.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct sequence_quantum_bridge sequence_quantum_bridge_t;

typedef struct {
    bool enabled;
    uint32_t grover_iterations;
    float match_threshold;
    uint32_t max_templates;
    bool use_distance_matrix;
} sequence_quantum_config_t;

typedef struct {
    uint64_t quantum_matches;
    uint64_t classical_fallbacks;
    uint64_t templates_added;
    float avg_match_score;
    float avg_speedup;
} sequence_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

sequence_quantum_config_t sequence_quantum_default_config(void);

sequence_quantum_bridge_t* sequence_quantum_bridge_create(
    const sequence_quantum_config_t* config
);

void sequence_quantum_bridge_destroy(sequence_quantum_bridge_t* bridge);

int sequence_quantum_bridge_connect(
    sequence_quantum_bridge_t* bridge,
    sequence_detector_t* detector
);

int sequence_quantum_bridge_disconnect(sequence_quantum_bridge_t* bridge);

bool sequence_quantum_bridge_is_enabled(const sequence_quantum_bridge_t* bridge);

void sequence_quantum_bridge_set_enabled(sequence_quantum_bridge_t* bridge, bool enabled);

/**
 * WHAT: Add template pattern for quantum matching
 */
int sequence_quantum_add_template(
    sequence_quantum_bridge_t* bridge,
    const char* name,
    const uint32_t* symbols,
    uint32_t length
);

/**
 * WHAT: Match input sequence against templates using Grover search
 */
int sequence_quantum_match(
    sequence_quantum_bridge_t* bridge,
    const uint32_t* input_symbols,
    uint32_t input_length,
    qseq_match_result_t* best_match
);

/**
 * WHAT: Find all matching templates above threshold
 */
int sequence_quantum_find_all(
    sequence_quantum_bridge_t* bridge,
    const uint32_t* input_symbols,
    uint32_t input_length,
    qseq_match_result_t* matches,
    uint32_t max_matches,
    uint32_t* n_matches
);

/**
 * WHAT: Compute distance matrix between templates
 */
int sequence_quantum_distance_matrix(
    sequence_quantum_bridge_t* bridge,
    float* distances,
    uint32_t* n_templates
);

int sequence_quantum_get_stats(
    const sequence_quantum_bridge_t* bridge,
    sequence_quantum_stats_t* stats
);

void sequence_quantum_reset_stats(sequence_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_SEQUENCE_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

struct sequence_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    sequence_quantum_config_t config;
    sequence_detector_t* detector;
    qseq_matcher_t matcher;  /* Direct handle, not pointer-to-pointer */
    sequence_quantum_stats_t stats;
    bool connected;
};

sequence_quantum_config_t sequence_quantum_default_config(void) {
    return (sequence_quantum_config_t){
        .enabled = true,
        .grover_iterations = 0,  // Auto
        .match_threshold = 0.7f,
        .max_templates = 100,
        .use_distance_matrix = true
    };
}

sequence_quantum_bridge_t* sequence_quantum_bridge_create(
    const sequence_quantum_config_t* config
) {
    sequence_quantum_bridge_t* bridge = (sequence_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : sequence_quantum_default_config();

    qseq_matcher_config_t qconfig = qseq_matcher_default_config();
    qconfig.max_templates = bridge->config.max_templates;
    qconfig.grover_iterations = bridge->config.grover_iterations;
    qconfig.min_similarity = bridge->config.match_threshold;

    bridge->matcher = qseq_matcher_create(&qconfig);
    if (!bridge->matcher) {
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void sequence_quantum_bridge_destroy(sequence_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->matcher) qseq_matcher_destroy(bridge->matcher);
    nimcp_free(bridge);
}

int sequence_quantum_bridge_connect(
    sequence_quantum_bridge_t* bridge,
    sequence_detector_t* detector
) {
    if (!bridge || !detector) return -1;
    bridge->detector = detector;
    bridge->connected = true;
    return 0;
}

int sequence_quantum_bridge_disconnect(sequence_quantum_bridge_t* bridge) {
    if (!bridge) return -1;
    bridge->detector = NULL;
    bridge->connected = false;
    return 0;
}

bool sequence_quantum_bridge_is_enabled(const sequence_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled && bridge->connected;
}

void sequence_quantum_bridge_set_enabled(sequence_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int sequence_quantum_add_template(
    sequence_quantum_bridge_t* bridge,
    const char* name,
    const uint32_t* symbols,
    uint32_t length
) {
    if (!bridge || !name || !symbols) return -1;
    (void)name;  /* Name not used in qseq API */

    /* Create pattern from symbols (timestamps auto-generated) */
    qseq_pattern_t pattern = qseq_create_pattern(symbols, NULL, length);

    uint32_t pattern_id;
    int status = qseq_matcher_add_template(bridge->matcher, &pattern, &pattern_id);
    if (status == 0) {
        bridge->stats.templates_added++;
    }
    return status;
}

int sequence_quantum_match(
    sequence_quantum_bridge_t* bridge,
    const uint32_t* input_symbols,
    uint32_t input_length,
    qseq_match_result_t* best_match
) {
    if (!bridge || !input_symbols || !best_match) return -1;

    if (!bridge->config.enabled) {
        bridge->stats.classical_fallbacks++;
        return -1;  /* Would use classical matching */
    }

    /* Create pattern from input (timestamps auto-generated) */
    qseq_pattern_t pattern = qseq_create_pattern(input_symbols, NULL, input_length);

    int status = qseq_matcher_match(bridge->matcher, &pattern, best_match);

    if (status == 0) {
        bridge->stats.quantum_matches++;
        bridge->stats.avg_match_score =
            (bridge->stats.avg_match_score * (bridge->stats.quantum_matches - 1)
             + best_match->similarity) / bridge->stats.quantum_matches;

        /* Estimate speedup: O(sqrt(N)) vs O(N) */
        float speedup = sqrtf((float)bridge->stats.templates_added);
        bridge->stats.avg_speedup =
            (bridge->stats.avg_speedup * (bridge->stats.quantum_matches - 1) + speedup)
            / bridge->stats.quantum_matches;
    }

    return status;
}

int sequence_quantum_find_all(
    sequence_quantum_bridge_t* bridge,
    const uint32_t* input_symbols,
    uint32_t input_length,
    qseq_match_result_t* matches,
    uint32_t max_matches,
    uint32_t* n_matches
) {
    if (!bridge || !input_symbols || !matches || !n_matches) return -1;

    /* Create pattern from input (timestamps auto-generated) */
    qseq_pattern_t pattern = qseq_create_pattern(input_symbols, NULL, input_length);

    int status = qseq_matcher_find_all(bridge->matcher, &pattern,
                                        bridge->config.match_threshold,
                                        matches, max_matches, n_matches);

    if (status >= 0) {
        bridge->stats.quantum_matches++;
    }

    return status;
}

int sequence_quantum_distance_matrix(
    sequence_quantum_bridge_t* bridge,
    float* distances,
    uint32_t* n_templates
) {
    if (!bridge || !distances || !n_templates) return -1;
    return qseq_matcher_distance_matrix(bridge->matcher, distances, n_templates);
}

int sequence_quantum_get_stats(
    const sequence_quantum_bridge_t* bridge,
    sequence_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void sequence_quantum_reset_stats(sequence_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

#endif // NIMCP_SEQUENCE_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SEQUENCE_DETECTOR_QUANTUM_BRIDGE_H

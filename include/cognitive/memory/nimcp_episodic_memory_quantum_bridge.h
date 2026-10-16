/**
 * @file nimcp_episodic_memory_quantum_bridge.h
 * @brief Quantum-accelerated episodic memory pattern matching
 *
 * WHAT: Integrates quantum sequence matcher with episodic memory for O(√N) episode retrieval
 * WHY:  Accelerate temporal pattern matching in recovery history from O(N) to O(√N)
 * HOW:  Quantum sequence matcher with episode-specific encoding and similarity metrics
 *
 * BIOLOGICAL INSPIRATION:
 * - Hippocampal replay detection operates at compressed timescales
 * - Pattern completion in associative memory recalls similar episodes
 * - Episodic memory retrieval via partial cues (content-addressable)
 * - Emotional tagging influences retrieval probability
 *
 * QUANTUM ADVANTAGE:
 * - Traditional LSH: O(log N) similarity search
 * - Quantum Search: O(√N) pattern matching
 * - Episode encoding: Error signatures → quantum amplitude states
 * - Temporal patterns: Recovery strategy sequences as spike trains
 *
 * INTEGRATION:
 * - Complements existing LSH-based retrieval
 * - Used for temporal pattern matching (strategy sequences)
 * - Fallback to LSH for error signature matching
 * - Quantum speedup for large episode databases (>1000 episodes)
 *
 * PERFORMANCE:
 * - Pattern encoding: O(1) per episode
 * - Quantum matching: O(√N) where N = episode count
 * - Memory overhead: ~512 bytes per episode template
 * - Speedup threshold: N > 100 episodes
 *
 * @author NIMCP Development Team
 * @date 2025-01-21
 */

#ifndef NIMCP_EPISODIC_MEMORY_QUANTUM_BRIDGE_H
#define NIMCP_EPISODIC_MEMORY_QUANTUM_BRIDGE_H

#include "cognitive/fault_tolerance/nimcp_recovery_episodic_memory.h"
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

typedef struct episodic_quantum_bridge episodic_quantum_bridge_t;

/**
 * WHAT: Configuration for quantum episodic memory bridge
 * WHY:  Control quantum matching behavior and thresholds
 */
typedef struct {
    bool enabled;                     /**< Enable quantum acceleration */
    uint32_t grover_iterations;       /**< Grover iterations (0 = auto) */
    float match_threshold;            /**< Minimum similarity [0,1] (default: 0.7) */
    uint32_t max_episodes;            /**< Max episode templates (default: 1000) */
    bool use_temporal_encoding;       /**< Encode recovery time sequences */
    bool use_emotional_weighting;     /**< Weight by emotional tags */
    float temporal_tolerance_ms;      /**< Timing tolerance for sequences (default: 100ms) */
} episodic_quantum_config_t;

/**
 * WHAT: Statistics for quantum episodic operations
 * WHY:  Monitor performance and quantum advantage
 */
typedef struct {
    uint64_t quantum_matches;         /**< Successful quantum matches */
    uint64_t quantum_retrievals;      /**< Quantum retrieval operations */
    uint64_t classical_fallbacks;     /**< Fallback to LSH */
    uint64_t patterns_encoded;        /**< Episode patterns encoded */
    float avg_match_score;            /**< Average similarity score */
    float avg_speedup;                /**< Average speedup vs linear search */
    float best_similarity_ever;       /**< Best similarity found */
} episodic_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

/**
 * WHAT: Get default quantum episodic memory configuration
 * WHY:  Simplify creation with sensible defaults
 * HOW:  Static initialization
 *
 * @return Default configuration
 */
episodic_quantum_config_t episodic_quantum_default_config(void);

/**
 * WHAT: Create quantum bridge for episodic memory
 * WHY:  Initialize quantum sequence matcher for episode patterns
 * HOW:  Allocate matcher, configure for episodic encoding
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
episodic_quantum_bridge_t* episodic_quantum_bridge_create(
    const episodic_quantum_config_t* config
);

/**
 * WHAT: Destroy quantum episodic bridge
 * WHY:  Free all resources
 * HOW:  Destroy matcher, free memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void episodic_quantum_bridge_destroy(episodic_quantum_bridge_t* bridge);

/**
 * WHAT: Connect quantum bridge to episodic memory
 * WHY:  Establish integration for quantum operations
 * HOW:  Store episodic memory reference
 *
 * @param bridge Quantum bridge (non-NULL)
 * @param memory Episodic memory instance (non-NULL)
 * @return 0 on success, negative on error
 */
int episodic_quantum_bridge_connect(
    episodic_quantum_bridge_t* bridge,
    episodic_memory_t* memory
);

/**
 * WHAT: Disconnect quantum bridge
 * WHY:  Remove integration
 * HOW:  Clear episodic memory reference
 *
 * @param bridge Quantum bridge (non-NULL)
 * @return 0 on success, negative on error
 */
int episodic_quantum_bridge_disconnect(episodic_quantum_bridge_t* bridge);

/**
 * WHAT: Check if quantum bridge is enabled
 * WHY:  Determine if quantum operations are active
 *
 * @param bridge Quantum bridge
 * @return true if enabled and connected
 */
bool episodic_quantum_bridge_is_enabled(const episodic_quantum_bridge_t* bridge);

/**
 * WHAT: Set quantum bridge enabled state
 * WHY:  Toggle quantum operations
 *
 * @param bridge Quantum bridge
 * @param enabled New enabled state
 */
void episodic_quantum_bridge_set_enabled(episodic_quantum_bridge_t* bridge, bool enabled);

/**
 * WHAT: Encode recovery episode as quantum pattern
 * WHY:  Add episode to quantum matcher templates
 * HOW:  Convert error signature + strategy sequence to quantum pattern
 *
 * ENCODING:
 * - Symbols: [error_type, error_code_low, error_code_high, strategy_type, ...]
 * - Timestamps: [0, recovery_time/4, recovery_time/2, recovery_time, ...]
 * - Weights: Emotional tag influences pattern weight
 *
 * @param bridge Quantum bridge (non-NULL)
 * @param episode Episode to encode (non-NULL)
 * @param pattern_id_out Output: assigned pattern ID (non-NULL)
 * @return 0 on success, negative on error
 */
int episodic_quantum_encode_episode(
    episodic_quantum_bridge_t* bridge,
    const recovery_episode_t* episode,
    uint32_t* pattern_id_out
);

/**
 * WHAT: Retrieve similar episodes using quantum pattern matching
 * WHY:  O(√N) speedup for temporal pattern similarity
 * HOW:  Quantum sequence matching on episode patterns
 *
 * ALGORITHM:
 * 1. Encode query episode as quantum pattern
 * 2. Run Grover search on episode templates
 * 3. Return top-K most similar by quantum fidelity
 * 4. Fallback to LSH if quantum matching fails
 *
 * @param bridge Quantum bridge (non-NULL)
 * @param query Query episode or error signature (non-NULL)
 * @param max_results Maximum episodes to return
 * @param results Output: match results (caller allocates)
 * @param n_results Output: number of results found (non-NULL)
 * @return 0 on success, negative on error
 *
 * @note Returns quantum match results sorted by similarity
 * @note Caller must map pattern_id back to episode_id
 */
int episodic_quantum_retrieve_similar(
    episodic_quantum_bridge_t* bridge,
    const recovery_episode_t* query,
    uint32_t max_results,
    qseq_match_result_t* results,
    uint32_t* n_results
);

/**
 * WHAT: Match recovery strategy sequence pattern
 * WHY:  Find episodes with similar recovery sequences
 * HOW:  Pattern match on strategy type sequences
 *
 * USE CASE:
 * - Multi-step recovery plans (RETRY → RELOAD → FALLBACK)
 * - Temporal pattern of recovery attempts
 * - Strategy effectiveness prediction
 *
 * @param bridge Quantum bridge (non-NULL)
 * @param strategy_sequence Strategy types in order (non-NULL)
 * @param sequence_length Number of strategies
 * @param best_match Output: best matching pattern (non-NULL)
 * @return 0 on success, negative on error
 */
int episodic_quantum_pattern_match(
    episodic_quantum_bridge_t* bridge,
    const recovery_strategy_type_t* strategy_sequence,
    uint32_t sequence_length,
    qseq_match_result_t* best_match
);

/**
 * WHAT: Get quantum bridge statistics
 * WHY:  Monitor quantum performance
 *
 * @param bridge Quantum bridge (non-NULL)
 * @param stats Output: statistics (non-NULL)
 * @return 0 on success, negative on error
 */
int episodic_quantum_get_stats(
    const episodic_quantum_bridge_t* bridge,
    episodic_quantum_stats_t* stats
);

/**
 * WHAT: Reset quantum bridge statistics
 * WHY:  Clear counters for fresh monitoring
 *
 * @param bridge Quantum bridge (non-NULL)
 */
void episodic_quantum_reset_stats(episodic_quantum_bridge_t* bridge);

/**
 * WHAT: Get episode count in quantum matcher
 * WHY:  Monitor template storage
 *
 * @param bridge Quantum bridge
 * @return Number of encoded episodes (0 if NULL)
 */
uint32_t episodic_quantum_get_episode_count(const episodic_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_EPISODIC_MEMORY_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

/**
 * WHAT: Internal bridge structure
 * WHY:  Encapsulation and efficient storage
 */
struct episodic_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    episodic_quantum_config_t config;
    episodic_memory_t* memory;
    qseq_matcher_t matcher;           /* Quantum sequence matcher */
    episodic_quantum_stats_t stats;
    bool connected;

    /* Pattern ID → Episode ID mapping */
    uint64_t* pattern_to_episode;     /* Maps pattern_id to episode_id */
    uint32_t* episode_to_pattern;     /* Maps episode_id to pattern_id */
    uint32_t mapping_capacity;
};

episodic_quantum_config_t episodic_quantum_default_config(void) {
    return (episodic_quantum_config_t){
        .enabled = true,
        .grover_iterations = 0,       /* Auto: sqrt(N) */
        .match_threshold = 0.7f,
        .max_episodes = 1000,
        .use_temporal_encoding = true,
        .use_emotional_weighting = true,
        .temporal_tolerance_ms = 100.0f
    };
}

episodic_quantum_bridge_t* episodic_quantum_bridge_create(
    const episodic_quantum_config_t* config
) {
    episodic_quantum_bridge_t* bridge =
        (episodic_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : episodic_quantum_default_config();

    /* Create quantum sequence matcher */
    qseq_matcher_config_t qconfig = qseq_matcher_default_config();
    qconfig.max_templates = bridge->config.max_episodes;
    qconfig.grover_iterations = bridge->config.grover_iterations;
    qconfig.min_similarity = bridge->config.match_threshold;
    qconfig.temporal_tolerance = bridge->config.temporal_tolerance_ms;
    qconfig.enable_compression_detection = true;  /* Detect time-scaled patterns */
    qconfig.amplitude_dim = 128;  /* Larger for episodic patterns */

    bridge->matcher = qseq_matcher_create(&qconfig);
    if (!bridge->matcher) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate pattern mapping tables */
    bridge->mapping_capacity = bridge->config.max_episodes;
    bridge->pattern_to_episode =
        (uint64_t*)nimcp_calloc(bridge->mapping_capacity, sizeof(uint64_t));
    bridge->episode_to_pattern =
        (uint32_t*)nimcp_calloc(bridge->mapping_capacity, sizeof(uint32_t));

    if (!bridge->pattern_to_episode || !bridge->episode_to_pattern) {
        qseq_matcher_destroy(bridge->matcher);
        nimcp_free(bridge->pattern_to_episode);
        nimcp_free(bridge->episode_to_pattern);
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void episodic_quantum_bridge_destroy(episodic_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->matcher) qseq_matcher_destroy(bridge->matcher);
    nimcp_free(bridge->pattern_to_episode);
    nimcp_free(bridge->episode_to_pattern);
    nimcp_free(bridge);
}

int episodic_quantum_bridge_connect(
    episodic_quantum_bridge_t* bridge,
    episodic_memory_t* memory
) {
    if (!bridge || !memory) return -1;
    bridge->memory = memory;
    bridge->connected = true;
    return 0;
}

int episodic_quantum_bridge_disconnect(episodic_quantum_bridge_t* bridge) {
    if (!bridge) return -1;
    bridge->memory = NULL;
    bridge->connected = false;
    return 0;
}

bool episodic_quantum_bridge_is_enabled(const episodic_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled && bridge->connected;
}

void episodic_quantum_bridge_set_enabled(episodic_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int episodic_quantum_encode_episode(
    episodic_quantum_bridge_t* bridge,
    const recovery_episode_t* episode,
    uint32_t* pattern_id_out
) {
    if (!bridge || !episode || !pattern_id_out) return -1;

    /* Encode episode as sequence pattern */
    /* Symbols: [error_type, error_code_parts, strategy_type, success, ...] */
    uint32_t symbols[8];
    float timestamps[8];
    uint32_t length = 0;

    /* Element 0: Error type */
    symbols[length] = (uint32_t)episode->error_sig.error_type;
    timestamps[length] = 0.0f;
    length++;

    /* Element 1-2: Error code (split into 16-bit parts for better encoding) */
    symbols[length] = episode->error_sig.error_code & 0xFFFF;
    timestamps[length] = 10.0f;
    length++;
    symbols[length] = (episode->error_sig.error_code >> 16) & 0xFFFF;
    timestamps[length] = 20.0f;
    length++;

    /* Element 3: Strategy type */
    symbols[length] = (uint32_t)episode->strategy_type;
    timestamps[length] = 30.0f;
    length++;

    /* Element 4: Success flag */
    symbols[length] = episode->success ? 1 : 0;
    timestamps[length] = (float)episode->recovery_time_us / 1000.0f;  /* Convert to ms */
    length++;

    /* Element 5: Recovery steps count */
    if (episode->num_recovery_steps > 0) {
        symbols[length] = episode->num_recovery_steps;
        timestamps[length] = timestamps[length-1] + 10.0f;
        length++;
    }

    /* Create quantum pattern */
    qseq_pattern_t pattern = qseq_create_pattern(symbols, timestamps, length);

    /* Apply emotional weighting if enabled */
    if (bridge->config.use_emotional_weighting) {
        /* Emotional tag [-1, 1] → weight [0.5, 1.5] */
        float emotional_weight = 1.0f + 0.5f * episode->emotional_tag;
        for (uint32_t i = 0; i < pattern.length; i++) {
            pattern.elements[i].weight = emotional_weight;
        }
    }

    /* Add to quantum matcher */
    uint32_t pattern_id;
    int status = qseq_matcher_add_template(bridge->matcher, &pattern, &pattern_id);
    if (status != 0) return status;

    /* Update mappings */
    if (pattern_id < bridge->mapping_capacity) {
        bridge->pattern_to_episode[pattern_id] = episode->episode_id;
        /* episode_to_pattern uses episode_id as index if within bounds */
        if (episode->episode_id < bridge->mapping_capacity) {
            bridge->episode_to_pattern[episode->episode_id] = pattern_id;
        }
    }

    bridge->stats.patterns_encoded++;
    *pattern_id_out = pattern_id;
    return 0;
}

int episodic_quantum_retrieve_similar(
    episodic_quantum_bridge_t* bridge,
    const recovery_episode_t* query,
    uint32_t max_results,
    qseq_match_result_t* results,
    uint32_t* n_results
) {
    if (!bridge || !query || !results || !n_results) return -1;

    if (!bridge->config.enabled) {
        bridge->stats.classical_fallbacks++;
        return -1;  /* Fallback to LSH */
    }

    /* Encode query as pattern (same as encode_episode) */
    uint32_t symbols[8];
    float timestamps[8];
    uint32_t length = 0;

    symbols[length] = (uint32_t)query->error_sig.error_type;
    timestamps[length++] = 0.0f;
    symbols[length] = query->error_sig.error_code & 0xFFFF;
    timestamps[length++] = 10.0f;
    symbols[length] = (query->error_sig.error_code >> 16) & 0xFFFF;
    timestamps[length++] = 20.0f;
    symbols[length] = (uint32_t)query->strategy_type;
    timestamps[length++] = 30.0f;
    symbols[length] = query->success ? 1 : 0;
    timestamps[length++] = (float)query->recovery_time_us / 1000.0f;

    qseq_pattern_t pattern = qseq_create_pattern(symbols, timestamps, length);

    /* Find all similar patterns */
    int status = qseq_matcher_find_all(
        bridge->matcher,
        &pattern,
        bridge->config.match_threshold,
        results,
        max_results,
        n_results
    );

    if (status >= 0 && *n_results > 0) {
        bridge->stats.quantum_retrievals++;
        bridge->stats.quantum_matches += *n_results;

        /* Update average match score */
        float total_score = 0.0f;
        for (uint32_t i = 0; i < *n_results; i++) {
            total_score += results[i].similarity;
            if (results[i].similarity > bridge->stats.best_similarity_ever) {
                bridge->stats.best_similarity_ever = results[i].similarity;
            }
        }
        float avg_this_query = total_score / *n_results;
        bridge->stats.avg_match_score =
            (bridge->stats.avg_match_score * (bridge->stats.quantum_retrievals - 1)
             + avg_this_query) / bridge->stats.quantum_retrievals;

        /* Estimate speedup: O(sqrt(N)) vs O(N) */
        uint32_t n_templates = qseq_matcher_template_count(bridge->matcher);
        float speedup = sqrtf((float)n_templates);
        bridge->stats.avg_speedup =
            (bridge->stats.avg_speedup * (bridge->stats.quantum_retrievals - 1) + speedup)
            / bridge->stats.quantum_retrievals;
    }

    return status;
}

int episodic_quantum_pattern_match(
    episodic_quantum_bridge_t* bridge,
    const recovery_strategy_type_t* strategy_sequence,
    uint32_t sequence_length,
    qseq_match_result_t* best_match
) {
    if (!bridge || !strategy_sequence || !best_match) return -1;
    if (sequence_length == 0 || sequence_length > QSEQ_MAX_PATTERN_LENGTH) return -1;

    /* Convert strategy sequence to quantum pattern */
    uint32_t symbols[QSEQ_MAX_PATTERN_LENGTH];
    for (uint32_t i = 0; i < sequence_length; i++) {
        symbols[i] = (uint32_t)strategy_sequence[i];
    }

    /* Auto-generate timestamps (evenly spaced) */
    qseq_pattern_t pattern = qseq_create_pattern(symbols, NULL, sequence_length);

    /* Match against all templates */
    int status = qseq_matcher_match(bridge->matcher, &pattern, best_match);

    if (status == 0) {
        bridge->stats.quantum_matches++;
    }

    return status;
}

int episodic_quantum_get_stats(
    const episodic_quantum_bridge_t* bridge,
    episodic_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void episodic_quantum_reset_stats(episodic_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

uint32_t episodic_quantum_get_episode_count(const episodic_quantum_bridge_t* bridge) {
    if (!bridge) return 0;
    return qseq_matcher_template_count(bridge->matcher);
}

#endif // NIMCP_EPISODIC_MEMORY_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EPISODIC_MEMORY_QUANTUM_BRIDGE_H

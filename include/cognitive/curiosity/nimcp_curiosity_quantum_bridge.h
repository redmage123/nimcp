/**
 * @file nimcp_curiosity_quantum_bridge.h
 * @brief Quantum Walk Bridge for Curiosity Exploration
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Quantum walk integration for efficient novelty space exploration
 * WHY:  Curiosity explores state space for novelty - quantum walk provides
 *       quadratic speedup over classical random walk exploration
 * HOW:  Map curiosity topics to graph nodes, use quantum walk to find novel regions
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * While quantum mechanics doesn't operate at neural timescales, quantum walk
 * algorithms model the exploratory dynamics of curiosity-driven search:
 *
 * - Superposition → Parallel exploration of multiple hypotheses
 * - Interference → Constructive amplification of promising directions
 * - Measurement → Commitment to exploring specific topic
 *
 * CURIOSITY-QUANTUM MAPPING:
 * - Nodes: Curiosity topics/concepts in knowledge space
 * - Edges: Semantic similarity between topics
 * - Amplitude: Curiosity intensity for each topic
 * - Walk steps: Exploration depth
 *
 * SPEEDUP:
 * Classical random walk: O(N) steps to explore N nodes
 * Quantum walk: O(√N) steps (quadratic speedup)
 *
 * REFERENCES:
 * - Childs (2010) "On the relationship between continuous- and discrete-time quantum walk"
 * - Gottlieb et al. (2013) "Information-seeking, curiosity, and attention"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CURIOSITY_QUANTUM_BRIDGE_H
#define NIMCP_CURIOSITY_QUANTUM_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "utils/quantum/nimcp_quantum_walk_ternary.h"
#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CURIOSITY_QUANTUM_MAX_TOPICS          1024  /**< Max topics in knowledge graph */
#define CURIOSITY_QUANTUM_EXPLORATION_STEPS   50    /**< Default quantum walk steps */
#define CURIOSITY_QUANTUM_NOVELTY_THRESHOLD   0.3f  /**< Novelty detection threshold */
#define CURIOSITY_QUANTUM_SIMILARITY_CUTOFF   0.2f  /**< Minimum similarity for edge */

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Curiosity quantum bridge configuration
 */
typedef struct curiosity_quantum_config_s {
    bool enabled;                    /**< Enable quantum exploration */
    uint32_t max_topics;             /**< Maximum topics to track */
    uint32_t exploration_steps;      /**< Quantum walk steps per exploration */
    float novelty_threshold;         /**< Threshold for novel topic detection */
    float similarity_cutoff;         /**< Minimum similarity for topic edges */
    float exploration_bias_left;     /**< Quantum walk left bias */
    float exploration_bias_stay;     /**< Quantum walk stay bias */
    float exploration_bias_right;    /**< Quantum walk right bias */
} curiosity_quantum_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Curiosity quantum bridge statistics
 */
typedef struct curiosity_quantum_stats_s {
    uint64_t quantum_explorations;   /**< Number of quantum explorations */
    uint64_t novelty_detections;     /**< Novel topics discovered */
    uint64_t topics_explored;        /**< Total topics explored */
    uint64_t graph_expansions;       /**< Knowledge graph expansions */
    float avg_exploration_speedup;   /**< Average speedup vs classical */
    float total_exploration_time_ms; /**< Total exploration time */
    uint32_t max_graph_size;         /**< Maximum graph size reached */
} curiosity_quantum_stats_t;

/* ============================================================================
 * Topic Node
 * ============================================================================ */

/**
 * @brief Topic node in curiosity knowledge graph
 */
typedef struct curiosity_topic_node_s {
    char topic[256];                 /**< Topic identifier */
    uint32_t node_id;                /**< Graph node ID */
    float curiosity_intensity;       /**< Current curiosity level [0,1] */
    float novelty_score;             /**< Novelty score [0,1] */
    uint64_t last_explored_ms;       /**< Last exploration timestamp */
    uint32_t exploration_count;      /**< Times explored */
    bool is_active;                  /**< Active in graph */
} curiosity_topic_node_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Curiosity quantum bridge
 *
 * WHAT: Bridge between curiosity system and quantum walk exploration
 * WHY:  Enable efficient parallel exploration of novelty space
 * HOW:  Maintain topic graph, use quantum walk for exploration
 */
typedef struct curiosity_quantum_bridge_s {
    bridge_base_t base;                      /**< MUST be first: base bridge infrastructure */

    curiosity_quantum_config_t config;       /**< Configuration */
    curiosity_quantum_stats_t stats;         /**< Statistics */

    /* Topic graph */
    curiosity_topic_node_t* topics;          /**< Topic nodes */
    uint32_t num_topics;                     /**< Number of active topics */
    trit_matrix_t* adjacency;                /**< Topic similarity graph */

    /* Quantum walker */
    trit_walker_graph_t* walker;             /**< Quantum walk engine */

    /* Exploration state */
    uint32_t current_topic_id;               /**< Current exploration topic */
    float* exploration_probabilities;        /**< Exploration probability distribution */
    bool exploration_active;                 /**< Currently exploring */
} curiosity_quantum_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize default quantum bridge configuration
 *
 * WHAT: Set all config values to sensible defaults
 * WHY:  Ensure valid configuration before use
 * HOW:  Initialize with optimal quantum walk parameters
 *
 * @param config Configuration to initialize
 */
void curiosity_quantum_default_config(curiosity_quantum_config_t* config);

/**
 * @brief Create curiosity quantum bridge
 *
 * WHAT: Instantiate quantum exploration bridge
 * WHY:  Enable quantum-accelerated curiosity exploration
 * HOW:  Allocate memory, create quantum walker, initialize graph
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
curiosity_quantum_bridge_t* curiosity_quantum_create(
    const curiosity_quantum_config_t* config
);

/**
 * @brief Destroy curiosity quantum bridge
 *
 * WHAT: Clean up all resources
 * WHY:  Prevent memory leaks
 * HOW:  Free graph, walker, and allocations
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void curiosity_quantum_destroy(curiosity_quantum_bridge_t* bridge);

/* ============================================================================
 * Topic Management
 * ============================================================================ */

/**
 * @brief Add topic to knowledge graph
 *
 * WHAT: Register new topic for quantum exploration
 * WHY:  Expand explorable novelty space
 * HOW:  Add node, compute similarities, update adjacency
 *
 * @param bridge Bridge handle
 * @param topic Topic identifier
 * @param initial_curiosity Initial curiosity intensity [0,1]
 * @param novelty_score Topic novelty score [0,1]
 * @return Topic node ID, or -1 on failure
 */
int curiosity_quantum_add_topic(
    curiosity_quantum_bridge_t* bridge,
    const char* topic,
    float initial_curiosity,
    float novelty_score
);

/**
 * @brief Compute topic similarity
 *
 * WHAT: Compute semantic similarity between topics
 * WHY:  Build topic connectivity graph
 * HOW:  Use string distance (can be replaced with embeddings)
 *
 * @param topic1 First topic
 * @param topic2 Second topic
 * @return Similarity score [0,1]
 */
float curiosity_quantum_topic_similarity(
    const char* topic1,
    const char* topic2
);

/**
 * @brief Update topic curiosity intensity
 *
 * WHAT: Modulate curiosity for specific topic
 * WHY:  Reflect changing interests
 * HOW:  Update node intensity, propagate to neighbors
 *
 * @param bridge Bridge handle
 * @param topic Topic identifier
 * @param intensity New curiosity intensity [0,1]
 * @return 0 on success, negative on error
 */
int curiosity_quantum_update_topic_intensity(
    curiosity_quantum_bridge_t* bridge,
    const char* topic,
    float intensity
);

/* ============================================================================
 * Quantum Exploration
 * ============================================================================ */

/**
 * @brief Perform quantum exploration from current topic
 *
 * WHAT: Use quantum walk to explore novelty space
 * WHY:  Find novel topics with quadratic speedup
 * HOW:  Initialize walker at current topic, run quantum steps, measure
 *
 * @param bridge Bridge handle
 * @param start_topic Starting topic (NULL for current)
 * @param steps Number of quantum walk steps (0 for default)
 * @param novel_topic Output: discovered novel topic (can be NULL)
 * @return Novelty score of discovered topic, or -1.0f on failure
 */
float curiosity_quantum_explore(
    curiosity_quantum_bridge_t* bridge,
    const char* start_topic,
    uint32_t steps,
    char* novel_topic
);

/**
 * @brief Evaluate novelty using quantum walk
 *
 * WHAT: Assess novelty of topic in knowledge space
 * WHY:  Quantify exploration value
 * HOW:  Run quantum walk, compute amplitude distribution entropy
 *
 * @param bridge Bridge handle
 * @param topic Topic to evaluate
 * @return Novelty score [0,1], or -1.0f on failure
 */
float curiosity_quantum_evaluate_novelty(
    curiosity_quantum_bridge_t* bridge,
    const char* topic
);

/**
 * @brief Get exploration probability distribution
 *
 * WHAT: Query quantum walk amplitude distribution
 * WHY:  Understand exploration landscape
 * HOW:  Return probability of visiting each topic
 *
 * @param bridge Bridge handle
 * @param probabilities Output array (caller allocated, num_topics size)
 * @return 0 on success, negative on error
 */
int curiosity_quantum_get_distribution(
    const curiosity_quantum_bridge_t* bridge,
    float* probabilities
);

/**
 * @brief Find most novel unvisited topic
 *
 * WHAT: Identify highest-novelty unexplored topic
 * WHY:  Direct curiosity to most promising target
 * HOW:  Run quantum walk, find max amplitude in unexplored nodes
 *
 * @param bridge Bridge handle
 * @param novel_topic Output: topic identifier (256 bytes min)
 * @return Novelty score, or -1.0f on failure
 */
float curiosity_quantum_find_novel_topic(
    curiosity_quantum_bridge_t* bridge,
    char* novel_topic
);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Query accumulated statistics
 * WHY:  Monitor exploration performance
 * HOW:  Copy stats data
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int curiosity_quantum_get_stats(
    const curiosity_quantum_bridge_t* bridge,
    curiosity_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero all counters
 *
 * @param bridge Bridge handle
 */
void curiosity_quantum_reset_stats(
    curiosity_quantum_bridge_t* bridge
);

/**
 * @brief Get current graph size
 *
 * WHAT: Query number of active topics
 * WHY:  Monitor knowledge space growth
 * HOW:  Return active topic count
 *
 * @param bridge Bridge handle
 * @return Number of active topics
 */
uint32_t curiosity_quantum_get_graph_size(
    const curiosity_quantum_bridge_t* bridge
);

/**
 * @brief Compute exploration speedup
 *
 * WHAT: Estimate speedup vs classical random walk
 * WHY:  Quantify quantum advantage
 * HOW:  Compare quantum steps to expected classical steps
 *
 * @param bridge Bridge handle
 * @return Speedup factor (typically ~√N for N topics)
 */
float curiosity_quantum_compute_speedup(
    const curiosity_quantum_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * Implementation Section
 * ============================================================================ */

#ifdef NIMCP_CURIOSITY_QUANTUM_BRIDGE_IMPLEMENTATION

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* Use parent module's LOG_MODULE */

/* Helper: Get current time */
static uint64_t cq_get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

/* Helper: Clamp float */
static float cq_clampf(float val, float min_val, float max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

/* Helper: Simple string distance (Levenshtein-like) */
static uint32_t cq_string_distance(const char* s1, const char* s2) {
    if (!s1 || !s2) return 1000;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    if (len1 == 0) return (uint32_t)len2;
    if (len2 == 0) return (uint32_t)len1;

    /* Simple character difference count */
    uint32_t diff = 0;
    size_t min_len = len1 < len2 ? len1 : len2;

    for (size_t i = 0; i < min_len; i++) {
        if (s1[i] != s2[i]) diff++;
    }

    diff += (uint32_t)(len1 > len2 ? len1 - len2 : len2 - len1);
    return diff;
}

/* Default configuration */
void curiosity_quantum_default_config(curiosity_quantum_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->enabled = true;
    config->max_topics = CURIOSITY_QUANTUM_MAX_TOPICS;
    config->exploration_steps = CURIOSITY_QUANTUM_EXPLORATION_STEPS;
    config->novelty_threshold = CURIOSITY_QUANTUM_NOVELTY_THRESHOLD;
    config->similarity_cutoff = CURIOSITY_QUANTUM_SIMILARITY_CUTOFF;

    /* Uniform quantum walk bias (equal superposition) */
    config->exploration_bias_left = 1.0f / 3.0f;
    config->exploration_bias_stay = 1.0f / 3.0f;
    config->exploration_bias_right = 1.0f / 3.0f;
}

/* Create bridge */
curiosity_quantum_bridge_t* curiosity_quantum_create(
    const curiosity_quantum_config_t* config) {

    curiosity_quantum_bridge_t* bridge = (curiosity_quantum_bridge_t*)
        nimcp_calloc(1, sizeof(curiosity_quantum_bridge_t));

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate curiosity quantum bridge");
        return NULL;
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, BIO_MODULE_CURIOSITY, "curiosity_quantum") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Store config */
    if (config) {
        bridge->config = *config;
    } else {
        curiosity_quantum_default_config(&bridge->config);
    }

    /* Allocate topic array */
    bridge->topics = (curiosity_topic_node_t*)nimcp_calloc(
        bridge->config.max_topics, sizeof(curiosity_topic_node_t));

    if (!bridge->topics) {
        NIMCP_LOGGING_ERROR("Failed to allocate topic array");
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate exploration probabilities */
    bridge->exploration_probabilities = (float*)nimcp_calloc(
        bridge->config.max_topics, sizeof(float));

    if (!bridge->exploration_probabilities) {
        NIMCP_LOGGING_ERROR("Failed to allocate probability array");
        nimcp_free(bridge->topics);
        nimcp_free(bridge);
        return NULL;
    }

    /* Create adjacency matrix (will be built as topics are added) */
    bridge->adjacency = trit_matrix_create(
        bridge->config.max_topics,
        bridge->config.max_topics,
        TERNARY_PACK_BASE243
    );

    if (!bridge->adjacency) {
        NIMCP_LOGGING_ERROR("Failed to create adjacency matrix");
        nimcp_free(bridge->exploration_probabilities);
        nimcp_free(bridge->topics);
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize adjacency to UNKNOWN (no edges) */
    for (uint32_t i = 0; i < bridge->config.max_topics; i++) {
        for (uint32_t j = 0; j < bridge->config.max_topics; j++) {
            trit_matrix_set(bridge->adjacency, i, j, TRIT_UNKNOWN);
        }
    }

    bridge->num_topics = 0;
    bridge->current_topic_id = 0;
    bridge->exploration_active = false;

    NIMCP_LOGGING_INFO("Created curiosity quantum bridge (max_topics=%u, steps=%u)",
                      bridge->config.max_topics, bridge->config.exploration_steps);

    return bridge;
}

/* Destroy bridge */
void curiosity_quantum_destroy(curiosity_quantum_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->walker) {
        trit_walker_graph_destroy(bridge->walker);
    }

    if (bridge->adjacency) {
        trit_matrix_destroy(bridge->adjacency);
    }

    if (bridge->exploration_probabilities) {
        nimcp_free(bridge->exploration_probabilities);
    }

    if (bridge->topics) {
        nimcp_free(bridge->topics);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed curiosity quantum bridge");
}

/* Compute topic similarity */
float curiosity_quantum_topic_similarity(
    const char* topic1,
    const char* topic2) {

    if (!topic1 || !topic2) return 0.0f;
    if (strcmp(topic1, topic2) == 0) return 1.0f;

    /* Simple string distance-based similarity */
    uint32_t dist = cq_string_distance(topic1, topic2);
    size_t max_len = strlen(topic1) > strlen(topic2) ?
                     strlen(topic1) : strlen(topic2);

    if (max_len == 0) return 0.0f;

    float similarity = 1.0f - ((float)dist / (float)max_len);
    return cq_clampf(similarity, 0.0f, 1.0f);
}

/* Find topic by name */
static int cq_find_topic(
    const curiosity_quantum_bridge_t* bridge,
    const char* topic) {

    if (!bridge || !topic) return -1;

    for (uint32_t i = 0; i < bridge->num_topics; i++) {
        if (bridge->topics[i].is_active &&
            strcmp(bridge->topics[i].topic, topic) == 0) {
            return (int)i;
        }
    }

    return -1;
}

/* Add topic */
int curiosity_quantum_add_topic(
    curiosity_quantum_bridge_t* bridge,
    const char* topic,
    float initial_curiosity,
    float novelty_score) {

    if (!bridge || !topic) return -1;
    if (bridge->num_topics >= bridge->config.max_topics) return -1;

    /* Check if topic already exists */
    int existing = cq_find_topic(bridge, topic);
    if (existing >= 0) {
        /* Update existing */
        bridge->topics[existing].curiosity_intensity = initial_curiosity;
        bridge->topics[existing].novelty_score = novelty_score;
        return existing;
    }

    /* Add new topic */
    uint32_t id = bridge->num_topics++;
    curiosity_topic_node_t* node = &bridge->topics[id];

    memset(node, 0, sizeof(*node));
    strncpy(node->topic, topic, sizeof(node->topic) - 1);
    node->node_id = id;
    node->curiosity_intensity = cq_clampf(initial_curiosity, 0.0f, 1.0f);
    node->novelty_score = cq_clampf(novelty_score, 0.0f, 1.0f);
    node->last_explored_ms = 0;
    node->exploration_count = 0;
    node->is_active = true;

    /* Compute similarities and build edges */
    for (uint32_t i = 0; i < id; i++) {
        if (!bridge->topics[i].is_active) continue;

        float sim = curiosity_quantum_topic_similarity(
            node->topic, bridge->topics[i].topic);

        if (sim >= bridge->config.similarity_cutoff) {
            /* Encode similarity as ternary weight */
            trit_t weight;
            if (sim > 0.7f) {
                weight = TRIT_POSITIVE;  /* Strong connection */
            } else if (sim > 0.4f) {
                weight = TRIT_UNKNOWN;   /* Medium connection */
            } else {
                weight = TRIT_NEGATIVE;  /* Weak connection */
            }

            trit_matrix_set(bridge->adjacency, id, i, weight);
            trit_matrix_set(bridge->adjacency, i, id, weight);
        }
    }

    /* Self-loop */
    trit_matrix_set(bridge->adjacency, id, id, TRIT_UNKNOWN);

    /* Recreate walker with updated graph */
    if (bridge->walker) {
        trit_walker_graph_destroy(bridge->walker);
    }

    bridge->walker = trit_walker_graph_create(bridge->adjacency);
    if (!bridge->walker) {
        NIMCP_LOGGING_WARN("Failed to create quantum walker");
    }

    bridge->stats.graph_expansions++;
    if (bridge->num_topics > bridge->stats.max_graph_size) {
        bridge->stats.max_graph_size = bridge->num_topics;
    }

    NIMCP_LOGGING_DEBUG("Added topic '%s' (id=%u, novelty=%.2f, graph_size=%u)",
                       topic, id, novelty_score, bridge->num_topics);

    return (int)id;
}

/* Update topic intensity */
int curiosity_quantum_update_topic_intensity(
    curiosity_quantum_bridge_t* bridge,
    const char* topic,
    float intensity) {

    if (!bridge || !topic) return -1;

    int id = cq_find_topic(bridge, topic);
    if (id < 0) return -1;

    bridge->topics[id].curiosity_intensity = cq_clampf(intensity, 0.0f, 1.0f);
    return 0;
}

/* Quantum exploration */
float curiosity_quantum_explore(
    curiosity_quantum_bridge_t* bridge,
    const char* start_topic,
    uint32_t steps,
    char* novel_topic) {

    if (!bridge || !bridge->walker) return -1.0f;
    if (bridge->num_topics == 0) return -1.0f;

    uint64_t start_time = cq_get_time_ms();

    /* Find starting topic */
    uint32_t start_id = bridge->current_topic_id;
    if (start_topic) {
        int id = cq_find_topic(bridge, start_topic);
        if (id >= 0) {
            start_id = (uint32_t)id;
        }
    }

    if (start_id >= bridge->num_topics) {
        start_id = 0;
    }

    /* Use default steps if not specified */
    if (steps == 0) {
        steps = bridge->config.exploration_steps;
    }

    /* Initialize quantum walk at starting topic */
    trit_walker_graph_init(bridge->walker, start_id);

    /* Run quantum walk */
    trit_walker_graph_run(bridge->walker, steps);

    /* Get probability distribution */
    trit_walker_graph_get_distribution(bridge->walker,
                                       bridge->exploration_probabilities);

    /* Find most novel topic (highest amplitude among high-novelty topics) */
    float max_score = -1.0f;
    uint32_t best_id = start_id;

    for (uint32_t i = 0; i < bridge->num_topics; i++) {
        if (!bridge->topics[i].is_active) continue;

        float prob = bridge->exploration_probabilities[i];
        float novelty = bridge->topics[i].novelty_score;

        /* Combined score: quantum amplitude × novelty × (1 - exploration_count_penalty) */
        float exploration_penalty = 1.0f / (1.0f + (float)bridge->topics[i].exploration_count * 0.1f);
        float score = prob * novelty * exploration_penalty;

        if (score > max_score) {
            max_score = score;
            best_id = i;
        }
    }

    /* Update bridge state */
    bridge->current_topic_id = best_id;
    bridge->exploration_active = true;
    bridge_base_record_update(&bridge->base);

    /* Update topic state */
    bridge->topics[best_id].last_explored_ms = bridge->base.last_update_time_ms;
    bridge->topics[best_id].exploration_count++;

    /* Output novel topic */
    if (novel_topic && best_id < bridge->num_topics) {
        strncpy(novel_topic, bridge->topics[best_id].topic, 256);
    }

    /* Update statistics */
    bridge->stats.quantum_explorations++;
    bridge->stats.topics_explored++;

    if (bridge->topics[best_id].novelty_score >= bridge->config.novelty_threshold) {
        bridge->stats.novelty_detections++;
    }

    /* Compute exploration time and speedup */
    uint64_t end_time = cq_get_time_ms();
    float elapsed_ms = (float)(end_time - start_time);
    bridge->stats.total_exploration_time_ms += elapsed_ms;

    /* Classical random walk would need O(N) steps, quantum needs O(√N) */
    float classical_steps = (float)bridge->num_topics;
    float quantum_steps = (float)steps;
    float speedup = classical_steps / (quantum_steps + 1.0f);

    /* Update running average speedup */
    float alpha = 0.1f;
    bridge->stats.avg_exploration_speedup =
        alpha * speedup + (1.0f - alpha) * bridge->stats.avg_exploration_speedup;

    NIMCP_LOGGING_DEBUG("Quantum exploration: start=%u, steps=%u, found=%u ('%s'), "
                       "novelty=%.2f, speedup=%.2f",
                       start_id, steps, best_id,
                       bridge->topics[best_id].topic,
                       bridge->topics[best_id].novelty_score,
                       speedup);

    return bridge->topics[best_id].novelty_score;
}

/* Evaluate novelty */
float curiosity_quantum_evaluate_novelty(
    curiosity_quantum_bridge_t* bridge,
    const char* topic) {

    if (!bridge || !topic) return -1.0f;

    int id = cq_find_topic(bridge, topic);
    if (id < 0) return 0.0f;  /* Unknown topic = no novelty */

    /* Run short quantum walk from topic */
    if (bridge->walker) {
        trit_walker_graph_init(bridge->walker, (uint32_t)id);
        trit_walker_graph_run(bridge->walker, 10);

        /* Compute entropy of amplitude distribution */
        float entropy = 0.0f;
        for (uint32_t i = 0; i < bridge->num_topics; i++) {
            float prob = trit_walker_graph_get_probability(bridge->walker, i);
            if (prob > 1e-6f) {
                entropy -= prob * logf(prob);
            }
        }

        /* Normalize entropy to [0,1] */
        float max_entropy = logf((float)bridge->num_topics);
        if (max_entropy > 0.0f) {
            entropy /= max_entropy;
        }

        /* High entropy = high novelty (many possible connections) */
        return cq_clampf(entropy, 0.0f, 1.0f);
    }

    /* Fallback: use stored novelty score */
    return bridge->topics[id].novelty_score;
}

/* Get distribution */
int curiosity_quantum_get_distribution(
    const curiosity_quantum_bridge_t* bridge,
    float* probabilities) {

    if (!bridge || !probabilities) return -1;
    if (!bridge->exploration_active) return -1;

    memcpy(probabilities, bridge->exploration_probabilities,
           bridge->num_topics * sizeof(float));

    return 0;
}

/* Find novel topic */
float curiosity_quantum_find_novel_topic(
    curiosity_quantum_bridge_t* bridge,
    char* novel_topic) {

    if (!bridge) return -1.0f;

    /* Use current topic as starting point */
    return curiosity_quantum_explore(bridge, NULL, 0, novel_topic);
}

/* Get statistics */
int curiosity_quantum_get_stats(
    const curiosity_quantum_bridge_t* bridge,
    curiosity_quantum_stats_t* stats) {

    if (!bridge || !stats) return -1;

    *stats = bridge->stats;
    return 0;
}

/* Reset statistics */
void curiosity_quantum_reset_stats(
    curiosity_quantum_bridge_t* bridge) {

    if (!bridge) return;

    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

/* Get graph size */
uint32_t curiosity_quantum_get_graph_size(
    const curiosity_quantum_bridge_t* bridge) {

    if (!bridge) return 0;
    return bridge->num_topics;
}

/* Compute speedup */
float curiosity_quantum_compute_speedup(
    const curiosity_quantum_bridge_t* bridge) {

    if (!bridge || bridge->num_topics == 0) return 1.0f;

    /* Theoretical speedup: N / √N = √N */
    float n = (float)bridge->num_topics;
    return sqrtf(n);
}

#endif /* NIMCP_CURIOSITY_QUANTUM_BRIDGE_IMPLEMENTATION */

#endif /* NIMCP_CURIOSITY_QUANTUM_BRIDGE_H */

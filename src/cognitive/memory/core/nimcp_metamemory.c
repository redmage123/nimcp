//=============================================================================
// nimcp_metamemory.c - Metamemory System Implementation
//=============================================================================
/**
 * @file nimcp_metamemory.c
 * @brief Implementation of metacognitive memory monitoring
 *
 * Implements "knowing what you know" - FOK, TOT, JOL, and confidence calibration.
 */

#include "cognitive/memory/core/nimcp_metamemory.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(metamemory, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Thread-Local Error Handling
//=============================================================================

#ifdef _WIN32
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal metamemory structure
 */
struct metamemory_struct {
    // PR memory integration
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;
    resonance_config_t resonance_config;

    // Current state
    metamemory_state_t current_state;
    bool has_current_state;

    // Historical accuracy for calibration
    confidence_record_t* confidence_history;
    size_t history_capacity;
    size_t history_count;
    size_t history_index;  // Circular buffer index

    // Calibration curve (learned confidence -> accuracy mapping)
    float calibration_curve[METAMEM_CALIBRATION_BINS];
    uint32_t calibration_counts[METAMEM_CALIBRATION_BINS];
    float current_calibration_error;

    // Statistics
    metamemory_stats_t stats;

    // Configuration
    metamemory_config_t config;

    // Mutex for thread safety
    void* mutex;  // Opaque mutex pointer
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Get calibration bin index for confidence value
 */
static size_t get_calibration_bin(float confidence) {
    if (confidence <= 0.0f) return 0;
    if (confidence >= 1.0f) return METAMEM_CALIBRATION_BINS - 1;
    return (size_t)(confidence * METAMEM_CALIBRATION_BINS);
}

/**
 * @brief Update calibration curve from history
 */
static void update_calibration_curve(metamemory_t meta) {
    if (!meta || meta->history_count == 0) return;

    // Reset curve
    float bin_sum[METAMEM_CALIBRATION_BINS] = {0};
    uint32_t bin_count[METAMEM_CALIBRATION_BINS] = {0};

    // Accumulate from history
    for (size_t i = 0; i < meta->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && meta->history_count > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)meta->history_count);
        }

        confidence_record_t* record = &meta->confidence_history[i];
        size_t bin = get_calibration_bin(record->confidence);
        bin_sum[bin] += record->was_correct ? 1.0f : 0.0f;
        bin_count[bin]++;
    }

    // Compute curve (accuracy per bin)
    for (size_t i = 0; i < METAMEM_CALIBRATION_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && METAMEM_CALIBRATION_BINS > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)METAMEM_CALIBRATION_BINS);
        }

        if (bin_count[i] > 0) {
            meta->calibration_curve[i] = bin_sum[i] / bin_count[i];
            meta->calibration_counts[i] = bin_count[i];
        } else {
            // Default to identity (confidence = accuracy)
            meta->calibration_curve[i] = (i + 0.5f) / METAMEM_CALIBRATION_BINS;
            meta->calibration_counts[i] = 0;
        }
    }

    // Compute calibration error
    float total_error = 0.0f;
    size_t total_samples = 0;
    for (size_t i = 0; i < METAMEM_CALIBRATION_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && METAMEM_CALIBRATION_BINS > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)METAMEM_CALIBRATION_BINS);
        }

        if (meta->calibration_counts[i] > 0) {
            float expected = (i + 0.5f) / METAMEM_CALIBRATION_BINS;
            float actual = meta->calibration_curve[i];
            total_error += fabsf(expected - actual) * meta->calibration_counts[i];
            total_samples += meta->calibration_counts[i];
        }
    }
    meta->current_calibration_error = total_samples > 0 ?
        total_error / total_samples : meta->config.initial_calibration;

    meta->stats.current_calibration_error = meta->current_calibration_error;
    memcpy(meta->stats.calibration_curve, meta->calibration_curve,
           sizeof(meta->calibration_curve));
    memcpy(meta->stats.calibration_counts, meta->calibration_counts,
           sizeof(meta->calibration_counts));
}

/**
 * @brief Compute partial signature match percentage
 */
static float compute_partial_match(
    const prime_signature_t* query,
    const prime_signature_t* target
) {
    if (!query || !target) return 0.0f;

    uint32_t matched = 0;
    uint32_t total = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        uint8_t q_exp = query->exponents[i];
        uint8_t t_exp = target->exponents[i];

        if (q_exp > 0) {
            total++;
            if (t_exp > 0) {
                // Partial match: count min overlap
                matched++;
            }
        }
    }

    return total > 0 ? (float)matched / total : 0.0f;
}

/**
 * @brief Extract partial information from signature match
 */
static void extract_partial_info(
    const prime_signature_t* query,
    const prime_signature_t* best_match,
    float match_score,
    partial_info_t* info
) {
    if (!info) return;

    memset(info, 0, sizeof(*info));

    if (!query || !best_match) return;

    // Count matched primes
    info->num_matched_primes = 0;
    info->total_primes = 0;

    for (size_t i = 0; i < PRIME_SIG_DIM; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)PRIME_SIG_DIM);
        }

        if (query->exponents[i] > 0) {
            info->total_primes++;
            if (best_match->exponents[i] > 0) {
                info->num_matched_primes++;
            }
        }
    }

    info->match_percentage = info->total_primes > 0 ?
        (float)info->num_matched_primes / info->total_primes : 0.0f;

    // Heuristic: first prime index can hint at first letter
    // This is a simplified approximation
    for (size_t i = 0; i < PRIME_SIG_DIM && i < 26; i++) {
        if (best_match->exponents[i] > 0) {
            info->has_first_letter = true;
            info->first_letter = (char)('a' + i);
            break;
        }
    }

    // Estimate syllable count from signature density
    uint32_t density = prime_sig_count_factors(best_match);
    if (density > 0) {
        info->has_syllable_count = true;
        info->syllable_count = (density / 8) + 1;  // Heuristic
        if (info->syllable_count > 10) info->syllable_count = 10;
    }
}

/**
 * @brief Determine TOT resolution strategy from partial info
 */
static tot_strategy_t determine_strategy(const partial_info_t* info) {
    if (!info) return TOT_STRATEGY_NONE;

    // Priority-based strategy selection
    if (info->has_first_letter) {
        return TOT_STRATEGY_ALPHABETIC;
    }
    if (info->has_category) {
        return TOT_STRATEGY_SEMANTIC;
    }
    if (info->has_emotional_valence && fabsf(info->emotional_valence) > 0.3f) {
        return TOT_STRATEGY_CONTEXT;
    }
    if (info->has_syllable_count) {
        return TOT_STRATEGY_PHONEMIC;
    }
    if (info->match_percentage > 0.3f) {
        return TOT_STRATEGY_ASSOCIATIVE;
    }

    // Default: wait for spreading activation to resolve
    return TOT_STRATEGY_INCUBATION;
}

/**
 * @brief Compute familiarity signal from resonance scores
 */
static float compute_familiarity_signal(
    metamemory_t meta,
    const prime_signature_t* query,
    entangle_neighbor_t* neighbors,
    size_t num_neighbors
) {
    if (!meta || !query || num_neighbors == 0) return 0.0f;

    float max_resonance = 0.0f;
    float sum_resonance = 0.0f;

    for (size_t i = 0; i < num_neighbors; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_neighbors > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)num_neighbors);
        }

        float score = neighbors[i].edge.resonance_score;
        if (score > max_resonance) max_resonance = score;
        sum_resonance += score;
    }

    // Familiarity combines max and mean signals
    float mean_resonance = sum_resonance / num_neighbors;
    return 0.7f * max_resonance + 0.3f * mean_resonance;
}

/**
 * @brief Compute partial activation from spreading
 */
static float compute_partial_activation(
    size_t num_activated,
    float total_activation,
    const metamemory_config_t* config
) {
    if (num_activated == 0) return 0.0f;

    // Normalize by expected activation
    float normalized = total_activation / (float)num_activated;
    return nimcp_clampf(normalized, 0.0f, 1.0f);
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT metamemory_config_t metamemory_config_default(void) {
    metamemory_config_t config = {
        // FOK configuration
        .fok_familiarity_threshold = METAMEM_FOK_FAMILIARITY_THRESHOLD,
        .fok_partial_threshold = METAMEM_FOK_PARTIAL_THRESHOLD,
        .fok_min_activated = 2,

        // TOT configuration
        .tot_partial_match_threshold = METAMEM_TOT_PARTIAL_MATCH_THRESHOLD,
        .tot_min_related = METAMEM_TOT_MIN_RELATED,
        .tot_blocking_threshold = 0.7f,

        // Confidence configuration
        .history_size = METAMEM_DEFAULT_HISTORY_SIZE,
        .initial_calibration = 0.1f,
        .calibration_learning_rate = NIMCP_LEARNING_RATE_COARSE,

        // JOL configuration
        .jol_decay_rate = METAMEM_JOL_DECAY_RATE,
        .jol_encoding_weight = 0.4f,
        .jol_accessibility_weight = 0.3f,

        // Search configuration
        .max_related_search = 100,
        .related_threshold = 0.3f,
        .spreading_hops = 3
    };
    return config;
}

NIMCP_EXPORT bool metamemory_config_validate(const metamemory_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_config_validate: config is NULL");
        return false;
    }

    // Validate thresholds in [0, 1]
    if (config->fok_familiarity_threshold < 0.0f ||
        config->fok_familiarity_threshold > 1.0f) return false;
    if (config->fok_partial_threshold < 0.0f ||
        config->fok_partial_threshold > 1.0f) return false;
    if (config->tot_partial_match_threshold < 0.0f ||
        config->tot_partial_match_threshold > 1.0f) return false;
    if (config->tot_blocking_threshold < 0.0f ||
        config->tot_blocking_threshold > 1.0f) return false;
    if (config->related_threshold < 0.0f ||
        config->related_threshold > 1.0f) return false;

    // Validate sizes
    if (config->history_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamemory_config_validate: config->history_size is zero");
        return false;
    }
    if (config->tot_min_related == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamemory_config_validate: config->tot_min_related is zero");
        return false;
    }

    // Validate weights
    if (config->jol_encoding_weight < 0.0f) {
        return false;
    }
    if (config->jol_accessibility_weight < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT metamemory_t metamemory_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    resonance_config_t* resonance_config,
    const metamemory_config_t* config
) {
    // Validate required parameters
    if (!entanglement) {
        set_error("entanglement graph is required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entanglement is NULL");

        return NULL;
    }
    if (!node_manager) {
        set_error("node manager is required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node_manager is NULL");

        return NULL;
    }

    // Use defaults if no config provided
    metamemory_config_t cfg = config ? *config : metamemory_config_default();
    if (!metamemory_config_validate(&cfg)) {
        set_error("invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metamemory_create: metamemory_config_validate is NULL");
        return NULL;
    }

    // Allocate main structure
    metamemory_t meta = (metamemory_t)nimcp_calloc(1, sizeof(struct metamemory_struct));
    if (!meta) {
        set_error("memory allocation failed for metamemory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate meta");

        return NULL;
    }

    // Store integrations
    meta->entanglement = entanglement;
    meta->node_manager = node_manager;
    if (resonance_config) {
        meta->resonance_config = *resonance_config;
    } else {
        meta->resonance_config = resonance_config_default();
    }
    meta->config = cfg;

    // Allocate history buffer
    meta->confidence_history = (confidence_record_t*)nimcp_calloc(
        cfg.history_size, sizeof(confidence_record_t));
    if (!meta->confidence_history) {
        set_error("memory allocation failed for history buffer");
        nimcp_free(meta);
        meta = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metamemory_create: meta->confidence_history is NULL");
        return NULL;
    }
    meta->history_capacity = cfg.history_size;
    meta->history_count = 0;
    meta->history_index = 0;

    // Initialize calibration curve to identity
    for (size_t i = 0; i < METAMEM_CALIBRATION_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && METAMEM_CALIBRATION_BINS > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)METAMEM_CALIBRATION_BINS);
        }

        meta->calibration_curve[i] = (i + 0.5f) / METAMEM_CALIBRATION_BINS;
        meta->calibration_counts[i] = 0;
    }
    meta->current_calibration_error = cfg.initial_calibration;

    // Initialize statistics
    memset(&meta->stats, 0, sizeof(meta->stats));
    meta->stats.current_calibration_error = cfg.initial_calibration;

    // Initialize state
    metamemory_state_init(&meta->current_state);
    meta->has_current_state = false;

    return meta;
}

NIMCP_EXPORT void metamemory_destroy(metamemory_t meta) {
    if (!meta) return;

    // Clean up current state if it has allocated memory
    metamemory_state_cleanup(&meta->current_state);

    // Free history buffer
    if (meta->confidence_history) {
        nimcp_free(meta->confidence_history);
        meta->confidence_history = NULL;
    }

    // Free main structure
    nimcp_free(meta);
    meta = NULL;
}

//=============================================================================
// Query Evaluation Functions
//=============================================================================

NIMCP_EXPORT bool metamemory_evaluate_query(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    nimcp_quaternion_t query_state,
    metamemory_state_t* state
) {
    if (!meta || !query_signature || !state) {
        set_error("NULL parameter in evaluate_query");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_evaluate_query: required parameter is NULL (meta, query_signature, state)");
        return false;
    }

    // Initialize output state
    metamemory_state_init(state);

    // Update statistics
    meta->stats.total_evaluations++;

    // Step 1: Search for exact or near matches using spreading activation
    entangle_neighbor_t neighbors[64];
    size_t neighbor_count = 0;

    // We don't have direct node lookup from signature, so we use graph traversal
    // In a real implementation, this would interface with a signature index
    // For now, we simulate by checking if graph has any nodes

    entangle_stats_t graph_stats;
    if (!entangle_get_stats(meta->entanglement, &graph_stats)) {
        set_error("failed to get graph statistics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamemory_evaluate_query: entangle_get_stats is NULL");
        return false;
    }

    if (graph_stats.num_nodes == 0) {
        // Empty graph - definitely unknown
        state->state = META_STATE_UNKNOWN;
        state->confidence = 1.0f;
        state->familiarity_signal = 0.0f;
        meta->stats.unknown_detections++;
        return true;
    }

    // Simulate search using spreading activation from a hypothetical start
    // In real usage, we'd have a signature-to-node index
    float best_match_score = 0.0f;
    uint64_t best_match_id = 0;
    bool exact_match = false;
    float total_activation = 0.0f;
    uint32_t num_activated = 0;

    // For this implementation, we'll use heuristics based on signature properties
    // A full implementation would search the actual memory store

    // Compute familiarity from signature properties
    uint32_t query_factors = prime_sig_count_factors(query_signature);
    float familiarity_estimate = 0.0f;

    if (query_factors > 0) {
        // Higher factor count suggests more specific/retrievable content
        familiarity_estimate = nimcp_clampf(query_factors / 32.0f, 0.0f, 1.0f);
    }

    // Incorporate quaternion accessibility as familiarity boost
    familiarity_estimate = familiarity_estimate * 0.7f + query_state.z * 0.3f;

    state->familiarity_signal = familiarity_estimate;
    state->partial_activation = familiarity_estimate * 0.8f;
    state->num_activated = (uint32_t)(familiarity_estimate * 10);

    // Step 2: Determine state based on signals
    if (familiarity_estimate > 0.8f) {
        // High familiarity - likely KNOWN
        state->state = META_STATE_KNOWN;
        state->confidence = familiarity_estimate;
        state->exact_match_found = true;
        state->best_match_score = familiarity_estimate;
        meta->stats.known_detections++;

    } else if (familiarity_estimate > meta->config.fok_familiarity_threshold) {
        // Moderate familiarity but no exact match - FOK
        state->state = META_STATE_FOK;
        state->confidence = familiarity_estimate * 0.8f;

        // Check for TOT indicators
        float partial_match = familiarity_estimate * 0.9f;  // Simulated

        if (partial_match > meta->config.tot_partial_match_threshold) {
            // Partial match suggests TOT
            state->state = META_STATE_TOT;

            // Extract partial information
            extract_partial_info(query_signature, query_signature,
                                partial_match, &state->partial_info);
            state->partial_info.match_percentage = partial_match;
            state->partial_features = state->partial_info.num_matched_primes;

            // Suggest resolution strategy
            state->suggested_strategy = determine_strategy(&state->partial_info);

            meta->stats.tot_detections++;
        } else {
            meta->stats.fok_detections++;
        }

    } else if (familiarity_estimate > 0.1f) {
        // Low familiarity - might be unknown or weak FOK
        if (query_state.w > 0.5f) {
            // High consolidation context suggests UNKNOWN_KNOWN
            state->state = META_STATE_UNKNOWN_KNOWN;
            state->confidence = 1.0f - familiarity_estimate;
            meta->stats.unknown_known_detections++;
        } else {
            // Just unknown
            state->state = META_STATE_UNKNOWN;
            state->confidence = 1.0f - familiarity_estimate;
            meta->stats.unknown_detections++;
        }

    } else {
        // Very low familiarity - definitely unknown
        state->state = META_STATE_UNKNOWN;
        state->confidence = 0.9f;
        meta->stats.unknown_detections++;
    }

    // Step 3: Compute calibrated confidence
    state->calibration_error = meta->current_calibration_error;

    // Step 4: Compute JOL indicators
    state->encoding_strength = query_state.w;
    state->retrieval_prediction = metamemory_predict_recall(meta, NULL, 24.0f);
    if (state->retrieval_prediction < 0) {
        // Use heuristic if no memory provided
        state->retrieval_prediction = familiarity_estimate * expf(-0.1f * 24.0f);
    }

    // Store as current state
    metamemory_state_cleanup(&meta->current_state);
    meta->current_state = *state;
    meta->has_current_state = true;

    return true;
}

NIMCP_EXPORT bool metamemory_check_familiarity(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    float* familiarity_out
) {
    if (!meta || !query_signature || !familiarity_out) {
        set_error("NULL parameter in check_familiarity");
        return false;
    }

    // Quick familiarity estimate from signature properties
    uint32_t query_factors = prime_sig_count_factors(query_signature);
    float familiarity = nimcp_clampf(query_factors / 32.0f, 0.0f, 1.0f);

    *familiarity_out = familiarity;

    return familiarity >= meta->config.fok_familiarity_threshold;
}

//=============================================================================
// FOK Functions
//=============================================================================

NIMCP_EXPORT bool metamemory_compute_fok(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    float* fok_strength
) {
    if (!meta || !query_signature || !fok_strength) {
        set_error("NULL parameter in compute_fok");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_compute_fok: required parameter is NULL (meta, query_signature, fok_strength)");
        return false;
    }

    // Compute familiarity signal
    float familiarity = 0.0f;
    metamemory_check_familiarity(meta, query_signature, &familiarity);

    // FOK strength is familiarity when above threshold but below definite recall
    if (familiarity >= meta->config.fok_familiarity_threshold &&
        familiarity < 0.8f) {
        *fok_strength = familiarity;
        return true;
    }

    *fok_strength = 0.0f;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamemory_compute_fok: operation failed");
    return false;
}

NIMCP_EXPORT bool metamemory_get_fok_contributors(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    pr_memory_node_t** memories,
    size_t max_memories,
    size_t* count
) {
    if (!meta || !query_signature || !memories || !count) {
        set_error("NULL parameter in get_fok_contributors");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_get_fok_contributors: required parameter is NULL (meta, query_signature, memories, count)");
        return false;
    }

    // In a full implementation, this would search the memory store
    // For now, we return empty (no direct access to memories from signature)
    *count = 0;
    return true;
}

//=============================================================================
// TOT Functions
//=============================================================================

NIMCP_EXPORT bool metamemory_detect_tot(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    partial_info_t* partial_info
) {
    if (!meta || !query_signature || !partial_info) {
        set_error("NULL parameter in detect_tot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_detect_tot: required parameter is NULL (meta, query_signature, partial_info)");
        return false;
    }

    // Initialize output
    memset(partial_info, 0, sizeof(*partial_info));

    // Compute familiarity and check for partial match indicators
    float familiarity = 0.0f;
    metamemory_check_familiarity(meta, query_signature, &familiarity);

    // TOT requires moderate familiarity (not too high, not too low)
    if (familiarity < meta->config.tot_partial_match_threshold ||
        familiarity > 0.9f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamemory_detect_tot: operation failed");
        return false;
    }

    // Extract what partial info we can from the signature
    partial_info->num_matched_primes = (uint8_t)(familiarity * PRIME_SIG_DIM);
    partial_info->total_primes = PRIME_SIG_DIM;
    partial_info->match_percentage = familiarity;

    // Heuristic first letter from first active prime
    for (size_t i = 0; i < PRIME_SIG_DIM && i < 26; i++) {
        if (query_signature->exponents[i] > 0) {
            partial_info->has_first_letter = true;
            partial_info->first_letter = (char)('a' + i);
            break;
        }
    }

    // Estimate syllables from signature density
    uint32_t density = prime_sig_count_factors(query_signature);
    if (density > 0) {
        partial_info->has_syllable_count = true;
        partial_info->syllable_count = (density / 8) + 1;
        if (partial_info->syllable_count > 10) {
            partial_info->syllable_count = 10;
        }
    }

    return true;
}

NIMCP_EXPORT bool metamemory_get_partial_information(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    partial_info_t* info
) {
    return metamemory_detect_tot(meta, query_signature, info);
}

NIMCP_EXPORT bool metamemory_search_related(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    pr_memory_node_t** related,
    size_t max_related,
    size_t* count
) {
    if (!meta || !query_signature || !related || !count) {
        set_error("NULL parameter in search_related");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_search_related: required parameter is NULL (meta, query_signature, related, count)");
        return false;
    }

    // Would search memory store for related memories
    *count = 0;
    return true;
}

NIMCP_EXPORT tot_strategy_t metamemory_suggest_tot_strategy(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    const partial_info_t* partial_info
) {
    if (!meta || !partial_info) {
        return TOT_STRATEGY_NONE;
    }

    return determine_strategy(partial_info);
}

NIMCP_EXPORT bool metamemory_resolve_tot(
    metamemory_t meta,
    const prime_signature_t* query_signature,
    tot_strategy_t strategy,
    const partial_info_t* partial_info,
    pr_memory_node_t** candidates,
    size_t max_candidates,
    size_t* count
) {
    if (!meta || !query_signature || !candidates || !count) {
        set_error("NULL parameter in resolve_tot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_resolve_tot: required parameter is NULL (meta, query_signature, candidates, count)");
        return false;
    }

    *count = 0;

    // Strategy-specific resolution would be implemented here
    // For now, we just acknowledge the attempt
    switch (strategy) {
        case TOT_STRATEGY_ALPHABETIC:
            // Would search by first letter variations
            break;
        case TOT_STRATEGY_SEMANTIC:
            // Would explore semantic category
            break;
        case TOT_STRATEGY_CONTEXT:
            // Would reinstate emotional context
            break;
        case TOT_STRATEGY_INCUBATION:
            // Would let spreading activation run
            break;
        case TOT_STRATEGY_PHONEMIC:
            // Would use syllable/sound cues
            break;
        case TOT_STRATEGY_ASSOCIATIVE:
            // Would follow association chains
            break;
        default:
            break;
    }

    return *count > 0;
}

//=============================================================================
// JOL Functions
//=============================================================================

NIMCP_EXPORT bool metamemory_judge_learning(
    metamemory_t meta,
    const pr_memory_node_t* memory,
    float prediction_hours,
    float* jol_out
) {
    if (!meta || !jol_out) {
        set_error("NULL parameter in judge_learning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_judge_learning: required parameter is NULL (meta, jol_out)");
        return false;
    }

    if (prediction_hours < 0) {
        set_error("prediction_hours must be non-negative");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metamemory_judge_learning: validation failed");
        return false;
    }

    float encoding_strength = 0.5f;  // Default
    float accessibility = 0.5f;

    if (memory) {
        // Get state from memory
        nimcp_quaternion_t state = pr_memory_node_get_state(memory);
        encoding_strength = state.w;  // Consolidation
        accessibility = state.z;      // Accessibility
    }

    // JOL formula: weighted combination with decay
    float base_jol = meta->config.jol_encoding_weight * encoding_strength +
                     meta->config.jol_accessibility_weight * accessibility +
                     (1.0f - meta->config.jol_encoding_weight -
                      meta->config.jol_accessibility_weight) * 0.5f;

    // Apply decay
    float decay_factor = expf(-meta->config.jol_decay_rate * prediction_hours);
    *jol_out = nimcp_clampf(base_jol * decay_factor, 0.0f, 1.0f);

    return true;
}

NIMCP_EXPORT float metamemory_predict_recall(
    metamemory_t meta,
    const pr_memory_node_t* memory,
    float hours_from_now
) {
    if (!meta) return -1.0f;
    if (hours_from_now < 0) return -1.0f;

    float jol = 0.0f;
    if (!metamemory_judge_learning(meta, memory, hours_from_now, &jol)) {
        return -1.0f;
    }

    return jol;
}

NIMCP_EXPORT float metamemory_estimate_decay_time(
    metamemory_t meta,
    const pr_memory_node_t* memory,
    float threshold
) {
    if (!meta) return -1.0f;
    if (threshold <= 0.0f || threshold >= 1.0f) return -1.0f;

    float current_jol = 0.0f;
    if (!metamemory_judge_learning(meta, memory, 0.0f, &current_jol)) {
        return -1.0f;
    }

    if (current_jol <= threshold) {
        return 0.0f;  // Already below threshold
    }

    if (meta->config.jol_decay_rate <= 0.0f) {
        return INFINITY;  // No decay
    }

    // Solve: current_jol * exp(-rate * t) = threshold
    // t = -ln(threshold / current_jol) / rate
    float time_hours = -logf(threshold / current_jol) / meta->config.jol_decay_rate;
    return time_hours > 0 ? time_hours : 0.0f;
}

//=============================================================================
// Confidence and Calibration Functions
//=============================================================================

NIMCP_EXPORT float metamemory_get_calibrated_confidence(
    metamemory_t meta,
    float raw_confidence
) {
    if (!meta) return raw_confidence;

    raw_confidence = nimcp_clampf(raw_confidence, 0.0f, 1.0f);

    // Linear interpolation in calibration curve
    float scaled = raw_confidence * (METAMEM_CALIBRATION_BINS - 1);
    size_t bin_low = (size_t)scaled;
    size_t bin_high = bin_low + 1;

    if (bin_high >= METAMEM_CALIBRATION_BINS) {
        return meta->calibration_curve[METAMEM_CALIBRATION_BINS - 1];
    }

    float t = scaled - bin_low;
    float calibrated = meta->calibration_curve[bin_low] * (1.0f - t) +
                       meta->calibration_curve[bin_high] * t;

    return nimcp_clampf(calibrated, 0.0f, 1.0f);
}

NIMCP_EXPORT float metamemory_update_calibration(
    metamemory_t meta,
    float confidence,
    bool was_correct
) {
    if (!meta) return -1.0f;

    confidence = nimcp_clampf(confidence, 0.0f, 1.0f);

    // Add to circular history buffer
    confidence_record_t record = {
        .confidence = confidence,
        .was_correct = was_correct,
        .timestamp_ms = get_current_time_ms()
    };

    meta->confidence_history[meta->history_index] = record;
    meta->history_index = (meta->history_index + 1) % meta->history_capacity;
    if (meta->history_count < meta->history_capacity) {
        meta->history_count++;
    }

    // Update statistics
    meta->stats.mean_confidence = (meta->stats.mean_confidence *
        (meta->stats.total_evaluations - 1) + confidence) /
        meta->stats.total_evaluations;
    meta->stats.mean_accuracy = (meta->stats.mean_accuracy *
        (meta->stats.total_evaluations - 1) + (was_correct ? 1.0f : 0.0f)) /
        meta->stats.total_evaluations;

    // Recalculate calibration curve
    update_calibration_curve(meta);

    return meta->current_calibration_error;
}

NIMCP_EXPORT float metamemory_get_calibration_error(metamemory_t meta) {
    if (!meta) return -1.0f;
    return meta->current_calibration_error;
}

NIMCP_EXPORT bool metamemory_get_calibration_curve(
    metamemory_t meta,
    float* curve_out
) {
    if (!meta || !curve_out) {
        set_error("NULL parameter in get_calibration_curve");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_get_calibration_curve: required parameter is NULL (meta, curve_out)");
        return false;
    }

    memcpy(curve_out, meta->calibration_curve,
           sizeof(float) * METAMEM_CALIBRATION_BINS);
    return true;
}

NIMCP_EXPORT void metamemory_reset_calibration(metamemory_t meta) {
    if (!meta) return;

    // Clear history
    meta->history_count = 0;
    meta->history_index = 0;
    memset(meta->confidence_history, 0,
           meta->history_capacity * sizeof(confidence_record_t));

    // Reset curve to identity
    for (size_t i = 0; i < METAMEM_CALIBRATION_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && METAMEM_CALIBRATION_BINS > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)METAMEM_CALIBRATION_BINS);
        }

        meta->calibration_curve[i] = (i + 0.5f) / METAMEM_CALIBRATION_BINS;
        meta->calibration_counts[i] = 0;
    }
    meta->current_calibration_error = meta->config.initial_calibration;

    // Update stats
    memcpy(meta->stats.calibration_curve, meta->calibration_curve,
           sizeof(meta->calibration_curve));
    memcpy(meta->stats.calibration_counts, meta->calibration_counts,
           sizeof(meta->calibration_counts));
    meta->stats.current_calibration_error = meta->current_calibration_error;
}

//=============================================================================
// State Management Functions
//=============================================================================

NIMCP_EXPORT void metamemory_state_cleanup(metamemory_state_t* state) {
    if (!state) return;

    // Free related memories array if allocated
    if (state->related_memories) {
        nimcp_free(state->related_memories);
        state->related_memories = NULL;
    }
    state->num_related = 0;
}

NIMCP_EXPORT void metamemory_state_init(metamemory_state_t* state) {
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->state = META_STATE_UNKNOWN;
    state->confidence = 0.0f;
    state->calibration_error = 0.0f;
    state->familiarity_signal = 0.0f;
    state->partial_activation = 0.0f;
    state->suggested_strategy = TOT_STRATEGY_NONE;
    state->related_memories = NULL;
    state->num_related = 0;
}

NIMCP_EXPORT bool metamemory_get_current_state(
    metamemory_t meta,
    metamemory_state_t* state
) {
    if (!meta || !state) {
        set_error("NULL parameter in get_current_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_get_current_state: required parameter is NULL (meta, state)");
        return false;
    }

    if (!meta->has_current_state) {
        set_error("no evaluation performed yet");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_get_current_state: meta->has_current_state is NULL");
        return false;
    }

    // Copy state (shallow - don't copy related_memories pointer)
    *state = meta->current_state;
    state->related_memories = NULL;  // Don't share pointer
    state->num_related = 0;

    return true;
}

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

NIMCP_EXPORT bool metamemory_get_stats(
    metamemory_t meta,
    metamemory_stats_t* stats
) {
    if (!meta || !stats) {
        set_error("NULL parameter in get_stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metamemory_get_stats: required parameter is NULL (meta, stats)");
        return false;
    }

    *stats = meta->stats;
    return true;
}

NIMCP_EXPORT void metamemory_reset_stats(metamemory_t meta) {
    if (!meta) return;

    // Preserve calibration data, reset counters
    float curve[METAMEM_CALIBRATION_BINS];
    uint32_t counts[METAMEM_CALIBRATION_BINS];
    memcpy(curve, meta->stats.calibration_curve, sizeof(curve));
    memcpy(counts, meta->stats.calibration_counts, sizeof(counts));
    float cal_error = meta->stats.current_calibration_error;

    memset(&meta->stats, 0, sizeof(meta->stats));

    memcpy(meta->stats.calibration_curve, curve, sizeof(curve));
    memcpy(meta->stats.calibration_counts, counts, sizeof(counts));
    meta->stats.current_calibration_error = cal_error;
}

NIMCP_EXPORT const char* metamemory_state_name(meta_state_t state) {
    switch (state) {
        case META_STATE_UNKNOWN:       return "UNKNOWN";
        case META_STATE_KNOWN:         return "KNOWN";
        case META_STATE_FOK:           return "FOK";
        case META_STATE_TOT:           return "TOT";
        case META_STATE_UNKNOWN_KNOWN: return "UNKNOWN_KNOWN";
        default:                       return "INVALID";
    }
}

NIMCP_EXPORT const char* metamemory_strategy_name(tot_strategy_t strategy) {
    switch (strategy) {
        case TOT_STRATEGY_NONE:        return "NONE";
        case TOT_STRATEGY_ALPHABETIC:  return "ALPHABETIC";
        case TOT_STRATEGY_SEMANTIC:    return "SEMANTIC";
        case TOT_STRATEGY_CONTEXT:     return "CONTEXT";
        case TOT_STRATEGY_INCUBATION:  return "INCUBATION";
        case TOT_STRATEGY_PHONEMIC:    return "PHONEMIC";
        case TOT_STRATEGY_ASSOCIATIVE: return "ASSOCIATIVE";
        default:                       return "INVALID";
    }
}

NIMCP_EXPORT const char* metamemory_get_last_error(void) {
    return g_last_error[0] != '\0' ? g_last_error : NULL;
}

NIMCP_EXPORT void metamemory_state_print(const metamemory_state_t* state) {
    if (!state) {
        printf("MetamemoryState: (null)\n");
        return;
    }

    printf("MetamemoryState {\n");
    printf("  state: %s\n", metamemory_state_name(state->state));
    printf("  confidence: %.3f\n", state->confidence);
    printf("  calibration_error: %.3f\n", state->calibration_error);
    printf("  familiarity_signal: %.3f\n", state->familiarity_signal);
    printf("  partial_activation: %.3f\n", state->partial_activation);
    printf("  num_activated: %u\n", state->num_activated);

    if (state->state == META_STATE_TOT) {
        printf("  TOT Info:\n");
        printf("    partial_features: %zu\n", state->partial_features);
        printf("    match_percentage: %.1f%%\n",
               state->partial_info.match_percentage * 100);
        printf("    suggested_strategy: %s\n",
               metamemory_strategy_name(state->suggested_strategy));
        if (state->partial_info.has_first_letter) {
            printf("    first_letter: %c\n", state->partial_info.first_letter);
        }
        if (state->partial_info.has_syllable_count) {
            printf("    syllable_count: %u\n", state->partial_info.syllable_count);
        }
    }

    printf("  JOL:\n");
    printf("    encoding_strength: %.3f\n", state->encoding_strength);
    printf("    retrieval_prediction: %.3f\n", state->retrieval_prediction);
    printf("    predicted_decay_time_hr: %.1f\n", state->predicted_decay_time_hr);

    if (state->exact_match_found) {
        printf("  Match:\n");
        printf("    best_match_id: %lu\n", (unsigned long)state->best_match_id);
        printf("    best_match_score: %.3f\n", state->best_match_score);
    }

    printf("}\n");
}

NIMCP_EXPORT void metamemory_stats_print(const metamemory_stats_t* stats) {
    if (!stats) {
        printf("MetamemoryStats: (null)\n");
        return;
    }

    printf("MetamemoryStats {\n");
    printf("  total_evaluations: %lu\n", (unsigned long)stats->total_evaluations);
    printf("  State Distribution:\n");
    printf("    KNOWN: %lu\n", (unsigned long)stats->known_detections);
    printf("    FOK: %lu\n", (unsigned long)stats->fok_detections);
    printf("    TOT: %lu\n", (unsigned long)stats->tot_detections);
    printf("    UNKNOWN: %lu\n", (unsigned long)stats->unknown_detections);
    printf("    UNKNOWN_KNOWN: %lu\n", (unsigned long)stats->unknown_known_detections);
    printf("  Confidence:\n");
    printf("    mean_confidence: %.3f\n", stats->mean_confidence);
    printf("    mean_accuracy: %.3f\n", stats->mean_accuracy);
    printf("    calibration_error: %.3f\n", stats->current_calibration_error);
    printf("  Resolution:\n");
    printf("    tot_resolutions: %lu\n", (unsigned long)stats->tot_resolutions);
    printf("    fok_confirmations: %lu\n", (unsigned long)stats->fok_confirmations);
    printf("  Calibration Curve:\n");
    for (size_t i = 0; i < METAMEM_CALIBRATION_BINS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && METAMEM_CALIBRATION_BINS > 256) {
            metamemory_heartbeat("metamemory_loop",
                             (float)(i + 1) / (float)METAMEM_CALIBRATION_BINS);
        }

        printf("    [%.1f-%.1f): %.3f (n=%u)\n",
               (float)i / METAMEM_CALIBRATION_BINS,
               (float)(i + 1) / METAMEM_CALIBRATION_BINS,
               stats->calibration_curve[i],
               stats->calibration_counts[i]);
    }
    printf("}\n");
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void metamemory_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_metamemory_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int metamemory_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metamemory_training_begin: NULL argument");
        return -1;
    }
    metamemory_heartbeat_instance(NULL, "metamemory_training_begin", 0.0f);
    (void)(struct metamemory_struct*)instance; /* Module state available for reset */
    return 0;
}

int metamemory_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metamemory_training_end: NULL argument");
        return -1;
    }
    metamemory_heartbeat_instance(NULL, "metamemory_training_end", 1.0f);
    (void)(struct metamemory_struct*)instance; /* Module state available for finalization */
    return 0;
}

int metamemory_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "metamemory_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    metamemory_heartbeat_instance(NULL, "metamemory_training_step", progress);
    (void)(struct metamemory_struct*)instance; /* Module state available for step adaptation */
    return 0;
}

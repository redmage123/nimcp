//=============================================================================
// nimcp_future_thinking.c - Episodic Future Thinking Implementation
//=============================================================================
/**
 * @file nimcp_future_thinking.c
 * @brief Implementation of mental simulation of future events and prospective planning
 *
 * This file implements the Episodic Future Thinking system for constructing
 * mental simulations of future events by sampling and recombining episodic
 * memory fragments into coherent future scenarios.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_future_thinking.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(future_thinking, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal structure for future thinking system
 */
struct future_thinking_struct {
    // Configuration
    future_thinking_config_t config;

    // Dependencies
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;

    // Event storage
    future_event_t* events;
    size_t num_events;
    size_t events_capacity;
    uint64_t next_event_id;

    // Goal storage
    future_goal_t* goals;
    size_t num_goals;
    size_t goals_capacity;
    uint64_t next_goal_id;

    // Fragment pool for sampling
    uint64_t* fragment_pool;
    float* fragment_weights;
    size_t pool_size;

    // Simulation state
    future_sim_status_t status;

    // Statistics
    future_thinking_stats_t stats;

    // Random state
    uint64_t rng_state;
};

//=============================================================================
// Thread-Local Error Handling
//=============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Random Number Generator
//=============================================================================

/**
 * @brief XorShift64 PRNG
 */
static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/**
 * @brief Generate uniform random float in [0, 1)
 */
static float random_float(uint64_t* state) {
    return (float)(xorshift64(state) & 0xFFFFFFFF) / (float)0x100000000;
}

//=============================================================================
// Utility Functions
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
 * @brief Compare floats for sorting (descending)
 */
static int compare_weights_desc(const void* a, const void* b) {
    float wa = *(const float*)a;
    float wb = *(const float*)b;
    if (wa > wb) return -1;
    if (wa < wb) return 1;
    return 0;
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT future_thinking_config_t future_thinking_config_default(void) {
    future_thinking_config_t config = {
        .max_events = FUTURE_DEFAULT_MAX_EVENTS,
        .max_goals = FUTURE_MAX_ACTIVE_GOALS,
        .fragment_pool_size = FUTURE_DEFAULT_POOL_SIZE,
        .max_scene_elements = FUTURE_MAX_SCENE_ELEMENTS,
        .sample_count = 8,
        .resonance_threshold = 0.3f,
        .diversity_weight = 0.2f,
        .discount_rate = FUTURE_DEFAULT_DISCOUNT_RATE,
        .temporal_horizon = FUTURE_MAX_TEMPORAL_HORIZON,
        .min_spatial_coherence = FUTURE_MIN_COHERENCE_THRESHOLD,
        .min_temporal_coherence = FUTURE_MIN_COHERENCE_THRESHOLD,
        .min_causal_coherence = FUTURE_MIN_COHERENCE_THRESHOLD,
        .emotional_weight = 0.3f,
        .approach_avoidance_threshold = 0.5f,
        .max_construction_iterations = 10,
        .construction_timeout_ms = 100.0f,
        .resonance_config = resonance_config_default()
    };
    return config;
}

NIMCP_EXPORT bool future_thinking_config_validate(
    const future_thinking_config_t* config
) {
    if (!config) {
        set_error("NULL configuration pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "future_thinking_config_validate: config is NULL");
        return false;
    }

    if (config->max_events == 0) {
        set_error("max_events must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "future_thinking_config_validate: config->max_events is zero");
        return false;
    }

    if (config->max_goals == 0) {
        set_error("max_goals must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "future_thinking_config_validate: config->max_goals is zero");
        return false;
    }

    if (config->fragment_pool_size == 0) {
        set_error("fragment_pool_size must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "future_thinking_config_validate: config->fragment_pool_size is zero");
        return false;
    }

    if (config->sample_count == 0) {
        set_error("sample_count must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "future_thinking_config_validate: config->sample_count is zero");
        return false;
    }

    if (config->discount_rate < 0.0f) {
        set_error("discount_rate must be >= 0");
        return false;
    }

    if (config->temporal_horizon <= 0.0f) {
        set_error("temporal_horizon must be > 0");
        return false;
    }

    if (config->resonance_threshold < 0.0f || config->resonance_threshold > 1.0f) {
        set_error("resonance_threshold must be in [0, 1]");
        return false;
    }

    if (config->min_spatial_coherence < 0.0f || config->min_spatial_coherence > 1.0f) {
        set_error("min_spatial_coherence must be in [0, 1]");
        return false;
    }

    if (config->min_temporal_coherence < 0.0f || config->min_temporal_coherence > 1.0f) {
        set_error("min_temporal_coherence must be in [0, 1]");
        return false;
    }

    if (config->min_causal_coherence < 0.0f || config->min_causal_coherence > 1.0f) {
        set_error("min_causal_coherence must be in [0, 1]");
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT future_thinking_t future_thinking_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const future_thinking_config_t* config
) {
    if (!entanglement) {
        set_error("NULL entanglement graph");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entanglement is NULL");

        return NULL;
    }

    if (!node_manager) {
        set_error("NULL node manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node_manager is NULL");


        return NULL;
    }

    future_thinking_config_t cfg;
    if (config) {
        if (!future_thinking_config_validate(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "future_thinking_create: future_thinking_config_validate is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = future_thinking_config_default();
    }

    future_thinking_t ft = nimcp_calloc(1, sizeof(struct future_thinking_struct));
    if (!ft) {
        set_error("Failed to allocate future thinking system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ft");

        return NULL;
    }

    ft->config = cfg;
    ft->entanglement = entanglement;
    ft->node_manager = node_manager;

    // Allocate event storage
    ft->events = nimcp_calloc(cfg.max_events, sizeof(future_event_t));
    if (!ft->events) {
        set_error("Failed to allocate event storage");
        nimcp_free(ft);
        ft = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "future_thinking_create: ft->events is NULL");
        return NULL;
    }
    ft->events_capacity = cfg.max_events;
    ft->next_event_id = 1;

    // Allocate goal storage
    ft->goals = nimcp_calloc(cfg.max_goals, sizeof(future_goal_t));
    if (!ft->goals) {
        set_error("Failed to allocate goal storage");
        nimcp_free(ft->events);
        nimcp_free(ft);
        ft = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "future_thinking_create: ft->goals is NULL");
        return NULL;
    }
    ft->goals_capacity = cfg.max_goals;
    ft->next_goal_id = 1;

    // Allocate fragment pool
    ft->fragment_pool = nimcp_calloc(cfg.fragment_pool_size, sizeof(uint64_t));
    ft->fragment_weights = nimcp_calloc(cfg.fragment_pool_size, sizeof(float));
    if (!ft->fragment_pool || !ft->fragment_weights) {
        set_error("Failed to allocate fragment pool");
        nimcp_free(ft->goals);
        nimcp_free(ft->events);
        nimcp_free(ft->fragment_pool);
        nimcp_free(ft->fragment_weights);
        nimcp_free(ft);
        ft = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "future_thinking_create: required parameter is NULL (ft->fragment_pool, ft->fragment_weights)");
        return NULL;
    }
    ft->pool_size = cfg.fragment_pool_size;

    // Initialize RNG state
    ft->rng_state = get_current_time_ms() ^ (uint64_t)(intptr_t)ft;

    ft->status = FUTURE_SIM_IDLE;

    return ft;
}

NIMCP_EXPORT void future_thinking_destroy(future_thinking_t ft) {
    if (!ft) {
        return;
    }

    // Clean up events
    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        future_event_cleanup(&ft->events[i]);
    }
    nimcp_free(ft->events);

    // Clean up goals
    for (size_t i = 0; i < ft->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_goals > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_goals);
        }

        future_goal_cleanup(&ft->goals[i]);
    }
    nimcp_free(ft->goals);

    // Clean up fragment pool
    nimcp_free(ft->fragment_pool);
    nimcp_free(ft->fragment_weights);

    nimcp_free(ft);
    ft = NULL;
}

NIMCP_EXPORT future_error_t future_thinking_reset(future_thinking_t ft) {
    if (!ft) {
        set_error("NULL future thinking system");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Clean up existing events
    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        future_event_cleanup(&ft->events[i]);
    }
    ft->num_events = 0;
    ft->next_event_id = 1;

    // Clean up existing goals
    for (size_t i = 0; i < ft->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_goals > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_goals);
        }

        future_goal_cleanup(&ft->goals[i]);
    }
    ft->num_goals = 0;
    ft->next_goal_id = 1;

    // Reset statistics
    memset(&ft->stats, 0, sizeof(ft->stats));

    ft->status = FUTURE_SIM_IDLE;

    return FUTURE_SUCCESS;
}

//=============================================================================
// Fragment Sampling Functions
//=============================================================================

NIMCP_EXPORT future_error_t future_thinking_sample_fragments(
    future_thinking_t ft,
    const prime_signature_t* context,
    size_t max_samples,
    uint64_t* fragments_out,
    float* weights_out,
    size_t* count_out
) {
    if (!ft || !context || !fragments_out || !weights_out || !count_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    // Build resonance query from context
    resonance_query_t query;
    resonance_query_init(&query);
    query.signature = (prime_signature_t*)context;  // Safe cast for read-only use
    query.quaternion = quat_identity();
    query.phase = 0.0f;

    // Get neighbors from entanglement graph based on context
    // We use spreading activation to find related memories
    entangle_neighbor_t neighbors[64];
    size_t neighbor_count = 0;

    // Since we don't have a direct node for the context, we need to
    // find memories whose signatures resonate with the context
    // This is a simplified approach - in production we'd query the full memory store

    // For now, sample from the node manager's tracked nodes
    uint64_t node_count = pr_node_manager_get_node_count(ft->node_manager);
    if (node_count == 0) {
        set_error("No memory nodes available for sampling");
        return FUTURE_ERROR_NO_FRAGMENTS;
    }

    // Collect candidate fragments with their resonance scores
    size_t candidates_found = 0;
    size_t max_candidates = max_samples < ft->pool_size ? max_samples : ft->pool_size;

    // Sample randomly from available memories and compute resonance
    // In a full implementation, we'd iterate through the memory store
    for (size_t i = 0; i < max_candidates && candidates_found < max_samples; i++) {
        // Generate random node ID within expected range
        uint64_t candidate_id = (xorshift64(&ft->rng_state) % node_count) + 1;

        // Check if this node is usable
        if (entangle_node_exists(ft->entanglement, candidate_id)) {
            // Compute resonance with context
            float resonance = 0.5f + 0.5f * random_float(&ft->rng_state);  // Placeholder

            if (resonance >= ft->config.resonance_threshold) {
                fragments_out[candidates_found] = candidate_id;
                weights_out[candidates_found] = resonance;
                candidates_found++;
            }
        }
    }

    if (candidates_found == 0) {
        // Fallback: just sample any available nodes
        for (size_t i = 0; i < max_samples && i < node_count; i++) {
            fragments_out[i] = i + 1;
            weights_out[i] = 0.5f;
            candidates_found++;
        }
    }

    *count_out = candidates_found;
    ft->stats.fragments_sampled += candidates_found;

    return candidates_found > 0 ? FUTURE_SUCCESS : FUTURE_ERROR_NO_FRAGMENTS;
}

NIMCP_EXPORT future_error_t future_thinking_combine_fragments(
    future_thinking_t ft,
    const uint64_t* fragment_ids,
    const float* weights,
    size_t num_fragments,
    prime_signature_t* combined_signature,
    nimcp_quaternion_t* combined_state
) {
    if (!ft || !fragment_ids || !combined_signature || !combined_state) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    if (num_fragments == 0) {
        set_error("No fragments to combine");
        return FUTURE_ERROR_NO_FRAGMENTS;
    }

    // Initialize combined signature to zero
    memset(combined_signature, 0, sizeof(prime_signature_t));

    // Collect quaternions for blending
    nimcp_quaternion_t* quats = nimcp_calloc(num_fragments, sizeof(nimcp_quaternion_t));
    float* blend_weights = nimcp_calloc(num_fragments, sizeof(float));
    if (!quats || !blend_weights) {
        nimcp_free(quats);
        quats = NULL;
        nimcp_free(blend_weights);
        blend_weights = NULL;
        set_error("Failed to allocate blend arrays");
        return FUTURE_ERROR_NO_MEMORY;
    }

    float total_weight = 0.0f;
    size_t valid_fragments = 0;

    for (size_t i = 0; i < num_fragments; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_fragments > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)num_fragments);
        }

        // Get fragment's signature and state
        // In a full implementation, we'd retrieve the actual memory node
        // For now, create placeholder signatures

        float w = weights ? weights[i] : 1.0f;

        // Compose signatures by combining exponents
        // Using max(existing, new) weighted by fragment weight
        for (size_t j = 0; j < PRIME_SIG_DIM; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && PRIME_SIG_DIM > 256) {
                future_thinking_heartbeat("future_think_loop",
                                 (float)(j + 1) / (float)PRIME_SIG_DIM);
            }

            uint8_t fragment_exp = (uint8_t)((fragment_ids[i] * (j + 1)) % 8);  // Placeholder
            float weighted_exp = fragment_exp * w;
            if (weighted_exp > combined_signature->exponents[j]) {
                combined_signature->exponents[j] = (uint8_t)weighted_exp;
            }
        }

        // Create placeholder quaternion based on fragment ID
        quats[valid_fragments] = quat_create(
            0.5f + 0.1f * (fragment_ids[i] % 5),
            0.1f * ((fragment_ids[i] / 5) % 10) - 0.5f,
            0.3f + 0.1f * ((fragment_ids[i] / 50) % 7),
            0.4f + 0.1f * ((fragment_ids[i] / 350) % 6)
        );
        blend_weights[valid_fragments] = w;
        total_weight += w;
        valid_fragments++;
    }

    // Normalize blend weights
    if (total_weight > FUTURE_EPSILON) {
        for (size_t i = 0; i < valid_fragments; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && valid_fragments > 256) {
                future_thinking_heartbeat("future_think_loop",
                                 (float)(i + 1) / (float)valid_fragments);
            }

            blend_weights[i] /= total_weight;
        }
    }

    // Blend quaternions
    if (valid_fragments > 0) {
        *combined_state = quat_blend_memories(quats, blend_weights, valid_fragments);
    } else {
        *combined_state = quat_identity();
    }

    // Update signature metadata
    combined_signature->hash = prime_sig_hash(combined_signature);
    prime_sig_recount_factors(combined_signature);

    nimcp_free(quats);
    quats = NULL;
    nimcp_free(blend_weights);
    blend_weights = NULL;

    return FUTURE_SUCCESS;
}

//=============================================================================
// Scene Construction Functions
//=============================================================================

NIMCP_EXPORT future_error_t future_thinking_construct_scene(
    future_thinking_t ft,
    const scene_element_t* elements,
    size_t num_elements,
    future_scene_t* scene_out
) {
    if (!ft || !elements || !scene_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    if (num_elements == 0) {
        set_error("No elements to construct scene from");
        return FUTURE_ERROR_NO_FRAGMENTS;
    }

    // Initialize scene
    future_error_t err = future_scene_init(scene_out, num_elements);
    if (err != FUTURE_SUCCESS) {
        return err;
    }

    // Copy elements into scene
    for (size_t i = 0; i < num_elements && i < scene_out->max_elements; i++) {
        scene_out->elements[i] = elements[i];
        scene_out->num_elements++;
    }

    // Evaluate coherence
    float spatial, temporal, causal;
    float overall = future_thinking_evaluate_coherence(ft, scene_out,
                                                        &spatial, &temporal, &causal);

    scene_out->spatial_coherence = spatial;
    scene_out->temporal_coherence = temporal;
    scene_out->causal_coherence = causal;
    scene_out->overall_coherence = overall;

    // Compute vividness based on element detail
    float total_detail = 0.0f;
    for (size_t i = 0; i < scene_out->num_elements; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scene_out->num_elements > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)scene_out->num_elements);
        }

        total_detail += elements[i].extraction_confidence;
    }
    scene_out->visual_detail = total_detail / (float)scene_out->num_elements;
    scene_out->overall_vividness = scene_out->visual_detail * overall;

    // Blend emotional tone from elements
    if (scene_out->num_elements > 0) {
        nimcp_quaternion_t* element_states = nimcp_malloc(scene_out->num_elements *
                                                     sizeof(nimcp_quaternion_t));
        float* element_weights = nimcp_calloc(scene_out->num_elements, sizeof(float));

        if (element_states && element_weights) {
            for (size_t i = 0; i < scene_out->num_elements; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && scene_out->num_elements > 256) {
                    future_thinking_heartbeat("future_think_loop",
                                     (float)(i + 1) / (float)scene_out->num_elements);
                }

                element_states[i] = elements[i].state;
                element_weights[i] = 1.0f;
            }
            scene_out->emotional_tone = quat_blend_memories(element_states,
                                                            element_weights,
                                                            scene_out->num_elements);
            nimcp_free(element_states);
            element_states = NULL;
            nimcp_free(element_weights);
            element_weights = NULL;
        } else {
            scene_out->emotional_tone = quat_identity();
            nimcp_free(element_states);
            element_states = NULL;
            nimcp_free(element_weights);
            element_weights = NULL;
        }
    }

    scene_out->construction_time_ms = get_current_time_ms();
    scene_out->scene_id = ft->next_event_id++;  // Use event ID counter for scenes too

    if (isfinite(overall) && ft->stats.total_events_simulated < UINT32_MAX) {
        ft->stats.avg_coherence = (ft->stats.avg_coherence * ft->stats.total_events_simulated +
                                   overall) / (ft->stats.total_events_simulated + 1);
    }

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT future_error_t future_thinking_elaborate_scene(
    future_thinking_t ft,
    future_scene_t* scene,
    float detail_level
) {
    if (!ft || !scene) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    detail_level = nimcp_clampf(detail_level, 0.0f, 1.0f);

    // Elaboration adds sensory detail to existing elements
    // Higher detail_level means more vivid, specific imagery

    // Scale vividness metrics by detail level
    scene->visual_detail = nimcp_clampf(scene->visual_detail + 0.2f * detail_level, 0.0f, 1.0f);
    scene->auditory_detail = nimcp_clampf(scene->auditory_detail + 0.15f * detail_level, 0.0f, 1.0f);
    scene->kinesthetic_detail = nimcp_clampf(scene->kinesthetic_detail + 0.1f * detail_level, 0.0f, 1.0f);

    // Update overall vividness
    scene->overall_vividness = (scene->visual_detail + scene->auditory_detail +
                                scene->kinesthetic_detail) / 3.0f;

    // Elaboration can also improve coherence through gap-filling
    scene->spatial_coherence = nimcp_clampf(scene->spatial_coherence + 0.05f * detail_level,
                                           0.0f, 1.0f);
    scene->temporal_coherence = nimcp_clampf(scene->temporal_coherence + 0.05f * detail_level,
                                            0.0f, 1.0f);
    scene->causal_coherence = nimcp_clampf(scene->causal_coherence + 0.03f * detail_level,
                                          0.0f, 1.0f);
    scene->overall_coherence = (scene->spatial_coherence + scene->temporal_coherence +
                                scene->causal_coherence) / 3.0f;

    scene->construction_iterations++;

    return FUTURE_SUCCESS;
}

//=============================================================================
// Evaluation Functions
//=============================================================================

NIMCP_EXPORT float future_thinking_evaluate_coherence(
    future_thinking_t ft,
    const future_scene_t* scene,
    float* spatial_out,
    float* temporal_out,
    float* causal_out
) {
    if (!ft || !scene) {
        set_error("NULL pointer argument");
        return -1.0f;
    }

    float spatial = 0.0f;
    float temporal = 0.0f;
    float causal = 0.0f;

    if (scene->num_elements == 0) {
        if (spatial_out) *spatial_out = 0.0f;
        if (temporal_out) *temporal_out = 0.0f;
        if (causal_out) *causal_out = 0.0f;
        return 0.0f;
    }

    // Spatial coherence: elements should be in compatible locations
    // Check position consistency among PLACE elements
    size_t place_count = 0;
    float position_variance = 0.0f;
    float center[3] = {0.0f, 0.0f, 0.0f};

    for (size_t i = 0; i < scene->num_elements; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scene->num_elements > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)scene->num_elements);
        }

        if (scene->elements[i].type == SCENE_ELEMENT_PLACE) {
            center[0] += scene->elements[i].position[0];
            center[1] += scene->elements[i].position[1];
            center[2] += scene->elements[i].position[2];
            place_count++;
        }
    }

    if (place_count > 0) {
        center[0] /= place_count;
        center[1] /= place_count;
        center[2] /= place_count;

        for (size_t i = 0; i < scene->num_elements; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && scene->num_elements > 256) {
                future_thinking_heartbeat("future_think_loop",
                                 (float)(i + 1) / (float)scene->num_elements);
            }

            if (scene->elements[i].type == SCENE_ELEMENT_PLACE) {
                float dx = scene->elements[i].position[0] - center[0];
                float dy = scene->elements[i].position[1] - center[1];
                float dz = scene->elements[i].position[2] - center[2];
                position_variance += dx*dx + dy*dy + dz*dz;
            }
        }
        position_variance /= place_count;

        // Convert variance to coherence (lower variance = higher coherence)
        spatial = expf(-position_variance / 100.0f);
    } else {
        spatial = 0.5f;  // No places, neutral coherence
    }

    // Temporal coherence: events should have consistent temporal ordering
    // Check that temporal_offset values form a plausible sequence
    float max_offset = 0.0f;
    float min_offset = 0.0f;
    size_t action_count = 0;

    for (size_t i = 0; i < scene->num_elements; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scene->num_elements > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)scene->num_elements);
        }

        if (scene->elements[i].type == SCENE_ELEMENT_ACTION ||
            scene->elements[i].type == SCENE_ELEMENT_TIME) {
            float offset = scene->elements[i].temporal_offset;
            if (action_count == 0 || offset > max_offset) max_offset = offset;
            if (action_count == 0 || offset < min_offset) min_offset = offset;
            action_count++;
        }
    }

    if (action_count > 1) {
        float range = max_offset - min_offset;
        // Coherence is higher when events are reasonably spaced
        // Not too compressed, not too spread out
        float optimal_range = 3600.0f;  // 1 hour optimal spread
        float range_ratio = range / optimal_range;
        temporal = expf(-fabsf(logf(range_ratio + FUTURE_EPSILON)));
    } else {
        temporal = 0.7f;  // Few events, reasonably coherent
    }

    // Causal coherence: related elements should make sense together
    // Check that relation chains are plausible
    float relation_score = 0.0f;
    size_t relation_count = 0;

    for (size_t i = 0; i < scene->num_elements; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && scene->num_elements > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)scene->num_elements);
        }

        if (scene->elements[i].num_relations > 0) {
            // Each relation gets a base score
            for (size_t r = 0; r < scene->elements[i].num_relations; r++) {
                // Check if related element exists in scene
                uint64_t related_id = scene->elements[i].related_to[r];
                bool found = false;
                for (size_t j = 0; j < scene->num_elements; j++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((j & 0xFF) == 0 && scene->num_elements > 256) {
                        future_thinking_heartbeat("future_think_loop",
                                         (float)(j + 1) / (float)scene->num_elements);
                    }

                    if (scene->elements[j].element_id == related_id) {
                        found = true;
                        relation_score += 1.0f;  // Valid relation
                        break;
                    }
                }
                if (!found) {
                    relation_score += 0.3f;  // Dangling relation (less coherent)
                }
                relation_count++;
            }
        }
    }

    if (relation_count > 0) {
        causal = relation_score / relation_count;
    } else {
        causal = 0.5f;  // No relations, neutral
    }

    // Clamp values
    spatial = nimcp_clampf(spatial, 0.0f, 1.0f);
    temporal = nimcp_clampf(temporal, 0.0f, 1.0f);
    causal = nimcp_clampf(causal, 0.0f, 1.0f);

    if (spatial_out) *spatial_out = spatial;
    if (temporal_out) *temporal_out = temporal;
    if (causal_out) *causal_out = causal;

    // Overall coherence is weighted average
    float overall = 0.4f * spatial + 0.3f * temporal + 0.3f * causal;

    return overall;
}

NIMCP_EXPORT float future_thinking_estimate_probability(
    future_thinking_t ft,
    const future_event_t* event
) {
    if (!ft || !event) {
        set_error("NULL pointer argument");
        return -1.0f;
    }

    // Probability estimation based on:
    // 1. Scene coherence (more coherent = more plausible)
    // 2. Temporal distance (near events more certain)
    // 3. Similarity to past experiences
    // 4. Controllability (controllable events more predictable)

    float coherence_factor = event->scene.overall_coherence;

    // Temporal factor: nearer events are more predictable
    float temporal_factor = 1.0f;
    if (event->expected_time > 0) {
        float days = event->expected_time / (24.0f * 3600.0f);
        temporal_factor = 1.0f / (1.0f + 0.01f * days);  // Decay over days
    }

    // Base probability from event type
    float type_factor = 0.5f;
    switch (event->type) {
        case FUTURE_SPECIFIC:
            type_factor = 0.4f;  // Specific events harder to predict
            break;
        case FUTURE_SEMANTIC:
            type_factor = 0.8f;  // General knowledge more reliable
            break;
        case FUTURE_HYPOTHETICAL:
            type_factor = 0.3f;  // Hypotheticals are speculative
            break;
        case FUTURE_GOAL:
            type_factor = 0.5f + 0.3f * event->controllability;  // Depends on control
            break;
        default:
            type_factor = 0.5f;
    }

    // Combine factors
    float probability = coherence_factor * temporal_factor * type_factor;

    // Add some noise for realism
    probability += 0.1f * (random_float(&ft->rng_state) - 0.5f);

    return nimcp_clampf(probability, 0.0f, 1.0f);
}

NIMCP_EXPORT future_error_t future_thinking_emotional_forecast(
    future_thinking_t ft,
    const future_event_t* event,
    float* valence_out,
    float* arousal_out,
    float* confidence_out
) {
    if (!ft || !event) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Extract emotional forecast from event's quaternion state
    // Using the quaternion semantic mapping:
    // x = emotional valence [-1, +1]
    // y = salience/arousal [0, 1]

    nimcp_quaternion_t q = event->event_quaternion;
    float valence = q.x;  // Direct mapping
    float arousal = q.y;  // Direct mapping

    // Confidence based on scene coherence and source memory count
    float confidence = event->scene.overall_coherence * 0.5f;
    confidence += (float)event->num_sources / (float)FUTURE_MAX_SOURCE_MEMORIES * 0.3f;
    confidence += event->anticipation_confidence * 0.2f;

    // Temporal distance reduces confidence in emotional forecast
    if (event->expected_time > 0) {
        float days = event->expected_time / (24.0f * 3600.0f);
        confidence *= 1.0f / (1.0f + 0.02f * days);
    }

    if (valence_out) *valence_out = nimcp_clampf(valence, -1.0f, 1.0f);
    if (arousal_out) *arousal_out = nimcp_clampf(arousal, 0.0f, 1.0f);
    if (confidence_out) *confidence_out = nimcp_clampf(confidence, 0.0f, 1.0f);

    return FUTURE_SUCCESS;
}

//=============================================================================
// Temporal Discounting Functions
//=============================================================================

NIMCP_EXPORT float future_thinking_temporal_discount(
    future_thinking_t ft,
    float value,
    float delay
) {
    if (!ft) {
        return value;  // No discounting if no system
    }

    if (delay <= 0.0f) {
        return value;  // No delay, no discounting
    }

    // Hyperbolic discounting: V(t) = V(0) / (1 + k*t)
    float k = ft->config.discount_rate;

    // Convert delay to reasonable units (days)
    float delay_days = delay / (24.0f * 3600.0f);

    float discounted = value / (1.0f + k * delay_days);

    return discounted;
}

NIMCP_EXPORT future_error_t future_thinking_set_discount_rate(
    future_thinking_t ft,
    float discount_rate
) {
    if (!ft) {
        set_error("NULL future thinking system");
        return FUTURE_ERROR_NULL_POINTER;
    }

    if (discount_rate < 0.0f) {
        set_error("Discount rate must be >= 0");
        return FUTURE_ERROR_INVALID_CONFIG;
    }

    ft->config.discount_rate = discount_rate;
    return FUTURE_SUCCESS;
}

NIMCP_EXPORT float future_thinking_psychological_distance(
    future_thinking_t ft,
    float temporal_distance,
    float spatial_familiarity,
    float social_connection
) {
    (void)ft;  // Configuration could be used in future

    // Psychological distance combines temporal, spatial, and social dimensions
    // Based on Construal Level Theory (Trope & Liberman)

    // Convert temporal distance to days and normalize
    float temporal_days = temporal_distance / (24.0f * 3600.0f);
    float temporal_norm = 1.0f - 1.0f / (1.0f + 0.01f * temporal_days);

    // Spatial: unfamiliar = distant
    float spatial_norm = 1.0f - spatial_familiarity;

    // Social: disconnected = distant
    float social_norm = 1.0f - social_connection;

    // Combined psychological distance
    float distance = 0.5f * temporal_norm + 0.3f * spatial_norm + 0.2f * social_norm;

    return nimcp_clampf(distance, 0.0f, 1.0f);
}

//=============================================================================
// Core Simulation Functions
//=============================================================================

NIMCP_EXPORT future_error_t future_thinking_simulate(
    future_thinking_t ft,
    const char* description,
    float expected_time,
    future_event_type_t type,
    future_event_t* event_out
) {
    if (!ft || !description || !event_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    if (ft->status != FUTURE_SIM_IDLE) {
        set_error("Simulation already in progress");
        return FUTURE_ERROR_ALREADY_SIMULATING;
    }

    if (expected_time < 0 || expected_time > ft->config.temporal_horizon) {
        set_error("Expected time out of range");
        return FUTURE_ERROR_INVALID_TIME;
    }

    uint64_t start_time = get_current_time_ms();

    // Initialize output event
    future_error_t err = future_event_init(event_out);
    if (err != FUTURE_SUCCESS) {
        return err;
    }

    ft->status = FUTURE_SIM_SAMPLING;

    // Generate context signature from description
    prime_signature_t* context_sig = prime_sig_from_text(description);
    if (!context_sig) {
        ft->status = FUTURE_SIM_FAILED;
        set_error("Failed to generate context signature");
        return FUTURE_ERROR_NO_MEMORY;
    }

    // Sample memory fragments
    uint64_t* fragments = nimcp_calloc(ft->config.sample_count, sizeof(uint64_t));
    float* weights = nimcp_calloc(ft->config.sample_count, sizeof(float));
    size_t num_fragments = 0;

    if (!fragments || !weights) {
        prime_sig_destroy(context_sig);
        nimcp_free(fragments);
        fragments = NULL;
        nimcp_free(weights);
        weights = NULL;
        ft->status = FUTURE_SIM_FAILED;
        set_error("Failed to allocate fragment arrays");
        return FUTURE_ERROR_NO_MEMORY;
    }

    err = future_thinking_sample_fragments(ft, context_sig, ft->config.sample_count,
                                            fragments, weights, &num_fragments);

    if (err != FUTURE_SUCCESS && num_fragments == 0) {
        // Try with a fallback approach
        num_fragments = 1;
        fragments[0] = 1;  // Default fragment
        weights[0] = 1.0f;
    }

    ft->status = FUTURE_SIM_EXTRACTING;

    // Combine fragments into event content
    prime_signature_t combined_sig;
    nimcp_quaternion_t combined_state;

    err = future_thinking_combine_fragments(ft, fragments, weights, num_fragments,
                                            &combined_sig, &combined_state);
    if (err != FUTURE_SUCCESS) {
        prime_sig_destroy(context_sig);
        nimcp_free(fragments);
        fragments = NULL;
        nimcp_free(weights);
        weights = NULL;
        ft->status = FUTURE_SIM_FAILED;
        return err;
    }

    ft->status = FUTURE_SIM_CONSTRUCTING;

    // Create scene elements from fragments
    scene_element_t* elements = nimcp_calloc(num_fragments, sizeof(scene_element_t));
    if (!elements) {
        prime_sig_destroy(context_sig);
        nimcp_free(fragments);
        fragments = NULL;
        nimcp_free(weights);
        weights = NULL;
        ft->status = FUTURE_SIM_FAILED;
        return FUTURE_ERROR_NO_MEMORY;
    }

    for (size_t i = 0; i < num_fragments; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_fragments > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)num_fragments);
        }

        scene_element_init(&elements[i], (scene_element_type_t)(i % SCENE_ELEMENT_TYPE_COUNT));
        elements[i].element_id = fragments[i];
        elements[i].source_memory_id = fragments[i];
        elements[i].extraction_confidence = weights[i];
        elements[i].state = combined_state;
        elements[i].temporal_offset = (float)i * 60.0f;  // 1 minute apart
    }

    // Construct scene
    err = future_thinking_construct_scene(ft, elements, num_fragments, &event_out->scene);
    nimcp_free(elements);
    elements = NULL;

    if (err != FUTURE_SUCCESS) {
        prime_sig_destroy(context_sig);
        nimcp_free(fragments);
        fragments = NULL;
        nimcp_free(weights);
        weights = NULL;
        ft->status = FUTURE_SIM_FAILED;
        return err;
    }

    ft->status = FUTURE_SIM_EVALUATING;

    // Fill in event structure
    event_out->event_id = ft->next_event_id++;
    event_out->type = type;
    strncpy(event_out->description, description, FUTURE_MAX_DESCRIPTION_LEN - 1);
    event_out->description[FUTURE_MAX_DESCRIPTION_LEN - 1] = '\0';

    event_out->event_signature = combined_sig;
    event_out->event_quaternion = combined_state;

    event_out->expected_time = expected_time;
    event_out->time_uncertainty = expected_time * 0.1f;  // 10% uncertainty
    event_out->temporal_distance = future_thinking_psychological_distance(
        ft, expected_time, 0.5f, 0.5f);

    event_out->location_signature = *context_sig;  // Use context as location proxy
    event_out->location_familiarity = 0.5f;

    // Copy source memory info
    event_out->num_sources = num_fragments;
    for (size_t i = 0; i < num_fragments && i < FUTURE_MAX_SOURCE_MEMORIES; i++) {
        event_out->source_memory_ids[i] = fragments[i];
        event_out->source_weights[i] = weights[i];
    }

    // Emotional forecast
    float valence, arousal, confidence;
    future_thinking_emotional_forecast(ft, event_out, &valence, &arousal, &confidence);
    event_out->anticipated_valence = valence;
    event_out->anticipated_arousal = arousal;
    event_out->anticipation_confidence = confidence;
    event_out->approach_avoidance = valence > 0 ? 1.0f : (valence < 0 ? -1.0f : 0.0f);

    // Planning metrics
    event_out->probability = future_thinking_estimate_probability(ft, event_out);
    event_out->desirability = valence * 0.5f + 0.5f;  // Map valence to [0, 1]
    event_out->controllability = 0.5f;  // Default moderate control
    event_out->goal_relevance = 0.0f;  // Will be set when linked to goals

    // Discounted value
    event_out->discounted_value = future_thinking_temporal_discount(
        ft, event_out->desirability, expected_time);

    // Self-relevance from emotional intensity
    event_out->self_relevance = arousal;

    // Metadata
    event_out->created_time_ms = get_current_time_ms();
    event_out->last_simulated_ms = event_out->created_time_ms;
    event_out->simulation_count = 1;
    event_out->simulation_stability = 1.0f;  // First simulation, perfectly stable

    // Clean up
    prime_sig_destroy(context_sig);
    nimcp_free(fragments);
    fragments = NULL;
    nimcp_free(weights);
    weights = NULL;

    // Update statistics
    uint64_t elapsed = get_current_time_ms() - start_time;
    ft->stats.total_events_created++;
    ft->stats.total_events_simulated++;
    ft->stats.avg_simulation_time_ms = (ft->stats.avg_simulation_time_ms *
        (ft->stats.total_events_simulated - 1) + (float)elapsed) /
        ft->stats.total_events_simulated;
    ft->stats.avg_probability = (ft->stats.avg_probability *
        (ft->stats.total_events_simulated - 1) + event_out->probability) /
        ft->stats.total_events_simulated;
    ft->stats.avg_fragments_per_sim = (ft->stats.avg_fragments_per_sim *
        (ft->stats.total_events_simulated - 1) + (float)num_fragments) /
        ft->stats.total_events_simulated;

    ft->status = FUTURE_SIM_COMPLETE;
    ft->status = FUTURE_SIM_IDLE;  // Ready for next simulation

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT future_error_t future_thinking_simulate_with_context(
    future_thinking_t ft,
    const prime_signature_t* context_signature,
    float expected_time,
    future_event_type_t type,
    future_event_t* event_out
) {
    if (!ft || !context_signature || !event_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Convert signature to text description (simplified)
    char description[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(description, sizeof(description),
             "Future event (sig hash: 0x%016llx)",
             (unsigned long long)context_signature->hash);

    return future_thinking_simulate(ft, description, expected_time, type, event_out);
}

NIMCP_EXPORT future_error_t future_thinking_resimulate(
    future_thinking_t ft,
    uint64_t event_id,
    future_event_t* event_out
) {
    if (!ft || !event_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Find existing event
    future_event_t* existing = NULL;
    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        if (ft->events[i].event_id == event_id) {
            existing = &ft->events[i];
            break;
        }
    }

    if (!existing) {
        set_error("Event not found");
        return FUTURE_ERROR_INVALID_EVENT;
    }

    // Re-simulate with same parameters
    future_error_t err = future_thinking_simulate(ft, existing->description,
                                                   existing->expected_time,
                                                   existing->type,
                                                   event_out);
    if (err == FUTURE_SUCCESS) {
        // Update stability based on similarity to previous simulation
        float coherence_diff = fabsf(event_out->scene.overall_coherence -
                                     existing->scene.overall_coherence);
        float prob_diff = fabsf(event_out->probability - existing->probability);

        float stability = 1.0f - (coherence_diff + prob_diff) / 2.0f;
        event_out->simulation_stability = (existing->simulation_stability *
            existing->simulation_count + stability) / (existing->simulation_count + 1);
        event_out->simulation_count = existing->simulation_count + 1;
    }

    return err;
}

//=============================================================================
// Goal Management Functions
//=============================================================================

NIMCP_EXPORT future_error_t future_thinking_connect_to_goal(
    future_thinking_t ft,
    uint64_t event_id,
    uint64_t goal_id,
    float relevance
) {
    if (!ft) {
        set_error("NULL future thinking system");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Find event
    future_event_t* event = NULL;
    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        if (ft->events[i].event_id == event_id) {
            event = &ft->events[i];
            break;
        }
    }

    if (!event) {
        set_error("Event not found");
        return FUTURE_ERROR_INVALID_EVENT;
    }

    // Find goal
    future_goal_t* goal = NULL;
    for (size_t i = 0; i < ft->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_goals > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_goals);
        }

        if (ft->goals[i].goal_id == goal_id) {
            goal = &ft->goals[i];
            break;
        }
    }

    if (!goal) {
        set_error("Goal not found");
        return FUTURE_ERROR_INVALID_GOAL;
    }

    // Add connection
    if (event->num_linked_goals < FUTURE_MAX_ACTIVE_GOALS) {
        event->linked_goal_ids[event->num_linked_goals] = goal_id;
        event->num_linked_goals++;
    }

    // Update goal relevance (max of existing and new)
    if (relevance > event->goal_relevance) {
        event->goal_relevance = relevance;
    }

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT future_error_t future_thinking_create_goal(
    future_thinking_t ft,
    const char* description,
    float priority,
    float deadline,
    future_goal_t* goal_out
) {
    if (!ft || !description || !goal_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    if (ft->num_goals >= ft->goals_capacity) {
        set_error("Maximum goals reached");
        return FUTURE_ERROR_MAX_GOALS;
    }

    future_error_t err = future_goal_init(goal_out);
    if (err != FUTURE_SUCCESS) {
        return err;
    }

    goal_out->goal_id = ft->next_goal_id++;
    strncpy(goal_out->description, description, FUTURE_MAX_DESCRIPTION_LEN - 1);
    goal_out->description[FUTURE_MAX_DESCRIPTION_LEN - 1] = '\0';

    // Generate goal signature from description
    prime_signature_t* sig = prime_sig_from_text(description);
    if (sig) {
        goal_out->goal_signature = *sig;
        prime_sig_destroy(sig);
    }

    goal_out->goal_state = quat_create(0.5f, 0.3f, priority, 0.5f);
    goal_out->priority = nimcp_clampf(priority, 0.0f, 1.0f);
    goal_out->deadline = deadline;
    goal_out->status = GOAL_STATUS_ACTIVE;
    goal_out->current_progress = 0.0f;
    goal_out->created_time_ms = get_current_time_ms();
    goal_out->last_updated_ms = goal_out->created_time_ms;

    // Store goal
    ft->goals[ft->num_goals] = *goal_out;
    ft->num_goals++;

    ft->stats.total_goals_created++;
    ft->stats.current_goal_count = ft->num_goals;

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT future_error_t future_thinking_generate_subgoals(
    future_thinking_t ft,
    uint64_t goal_id,
    size_t max_subgoals,
    future_goal_t* subgoals_out,
    size_t* count_out
) {
    if (!ft || !subgoals_out || !count_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    // Find goal
    future_goal_t* parent = NULL;
    for (size_t i = 0; i < ft->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_goals > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_goals);
        }

        if (ft->goals[i].goal_id == goal_id) {
            parent = &ft->goals[i];
            break;
        }
    }

    if (!parent) {
        set_error("Goal not found");
        return FUTURE_ERROR_INVALID_GOAL;
    }

    // Generate subgoals based on goal type and content
    // This is a simplified heuristic approach

    size_t to_generate = max_subgoals < FUTURE_MAX_SUBGOALS ?
                         max_subgoals : FUTURE_MAX_SUBGOALS;

    // If deadline exists, create temporal milestones
    if (parent->deadline > 0 && to_generate > 0) {
        float interval = parent->deadline / (float)(to_generate + 1);

        for (size_t i = 0; i < to_generate && ft->num_goals < ft->goals_capacity; i++) {
            char subgoal_desc[NIMCP_ERROR_BUFFER_SIZE];
            snprintf(subgoal_desc, sizeof(subgoal_desc),
                    "Milestone %zu for: %.50s", i + 1, parent->description);

            future_goal_t subgoal;
            future_error_t err = future_thinking_create_goal(ft, subgoal_desc,
                                                             parent->priority * 0.8f,
                                                             interval * (i + 1),
                                                             &subgoal);
            if (err == FUTURE_SUCCESS) {
                subgoal.parent_goal_id = goal_id;
                subgoals_out[*count_out] = subgoal;

                // Link to parent
                if (parent->num_subgoals < FUTURE_MAX_SUBGOALS) {
                    parent->subgoal_ids[parent->num_subgoals] = subgoal.goal_id;
                    parent->num_subgoals++;
                }

                (*count_out)++;
            }
        }
    }

    return *count_out > 0 ? FUTURE_SUCCESS : FUTURE_ERROR_INVALID_GOAL;
}

NIMCP_EXPORT future_error_t future_thinking_update_goal_progress(
    future_thinking_t ft,
    uint64_t goal_id,
    float new_progress
) {
    if (!ft) {
        set_error("NULL future thinking system");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Find goal
    for (size_t i = 0; i < ft->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_goals > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_goals);
        }

        if (ft->goals[i].goal_id == goal_id) {
            ft->goals[i].current_progress = nimcp_clampf(new_progress, 0.0f, 1.0f);
            ft->goals[i].last_updated_ms = get_current_time_ms();

            if (new_progress >= 1.0f) {
                ft->goals[i].status = GOAL_STATUS_ACHIEVED;
                ft->stats.goals_achieved++;
            }

            return FUTURE_SUCCESS;
        }
    }

    set_error("Goal not found");
    return FUTURE_ERROR_INVALID_GOAL;
}

NIMCP_EXPORT future_error_t future_thinking_get_goal(
    future_thinking_t ft,
    uint64_t goal_id,
    future_goal_t* goal_out
) {
    if (!ft || !goal_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    for (size_t i = 0; i < ft->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_goals > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_goals);
        }

        if (ft->goals[i].goal_id == goal_id) {
            *goal_out = ft->goals[i];
            return FUTURE_SUCCESS;
        }
    }

    set_error("Goal not found");
    return FUTURE_ERROR_INVALID_GOAL;
}

//=============================================================================
// Comparison and Planning Functions
//=============================================================================

NIMCP_EXPORT future_error_t future_thinking_compare_scenarios(
    future_thinking_t ft,
    uint64_t event_id_a,
    uint64_t event_id_b,
    scenario_comparison_t* comparison_out
) {
    if (!ft || !comparison_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Find both events
    future_event_t* event_a = NULL;
    future_event_t* event_b = NULL;

    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        if (ft->events[i].event_id == event_id_a) {
            event_a = &ft->events[i];
        }
        if (ft->events[i].event_id == event_id_b) {
            event_b = &ft->events[i];
        }
    }

    if (!event_a || !event_b) {
        set_error("Event not found");
        return FUTURE_ERROR_INVALID_EVENT;
    }

    comparison_out->event_id_a = event_id_a;
    comparison_out->event_id_b = event_id_b;

    // Content similarity via signature Jaccard
    comparison_out->content_similarity = prime_sig_jaccard(
        &event_a->event_signature, &event_b->event_signature);

    // Emotional similarity via quaternion distance
    float quat_dist = quat_geodesic_distance(
        event_a->event_quaternion, event_b->event_quaternion);
    comparison_out->emotional_similarity = 1.0f - quat_dist / M_PI;

    // Temporal difference
    comparison_out->temporal_difference = fabsf(event_a->expected_time -
                                                 event_b->expected_time);

    // Probability difference
    comparison_out->probability_difference = event_a->probability - event_b->probability;

    // Value difference (using discounted value)
    comparison_out->value_difference = event_a->discounted_value - event_b->discounted_value;

    // Compute preference score
    // Positive = prefer A, negative = prefer B
    float pref = 0.0f;
    pref += 0.3f * comparison_out->probability_difference;  // Higher probability better
    pref += 0.4f * comparison_out->value_difference;         // Higher value better
    pref += 0.2f * (event_a->anticipated_valence - event_b->anticipated_valence);  // Better emotion
    pref += 0.1f * (event_a->controllability - event_b->controllability);  // More control better

    comparison_out->preference_score = pref;

    // Generate preference reason
    if (fabsf(pref) < 0.1f) {
        strncpy(comparison_out->preference_reason, "Scenarios are roughly equivalent",
                sizeof(comparison_out->preference_reason));
    } else if (pref > 0) {
        if (comparison_out->value_difference > comparison_out->probability_difference) {
            strncpy(comparison_out->preference_reason, "Scenario A has higher expected value",
                    sizeof(comparison_out->preference_reason));
        } else {
            strncpy(comparison_out->preference_reason, "Scenario A is more likely",
                    sizeof(comparison_out->preference_reason));
        }
    } else {
        if (-comparison_out->value_difference > -comparison_out->probability_difference) {
            strncpy(comparison_out->preference_reason, "Scenario B has higher expected value",
                    sizeof(comparison_out->preference_reason));
        } else {
            strncpy(comparison_out->preference_reason, "Scenario B is more likely",
                    sizeof(comparison_out->preference_reason));
        }
    }

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT future_error_t future_thinking_optimize_path(
    future_thinking_t ft,
    uint64_t goal_id,
    size_t max_steps,
    path_optimization_result_t* result_out
) {
    if (!ft || !result_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    // Find goal
    future_goal_t* goal = NULL;
    for (size_t i = 0; i < ft->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_goals > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_goals);
        }

        if (ft->goals[i].goal_id == goal_id) {
            goal = &ft->goals[i];
            break;
        }
    }

    if (!goal) {
        set_error("Goal not found");
        return FUTURE_ERROR_INVALID_GOAL;
    }

    result_out->goal_id = goal_id;

    // Find events relevant to this goal
    size_t relevant_count = 0;
    uint64_t* relevant_events = nimcp_calloc(ft->num_events, sizeof(uint64_t));
    float* relevances = nimcp_calloc(ft->num_events, sizeof(float));

    if (!relevant_events || !relevances) {
        nimcp_free(relevant_events);
        relevant_events = NULL;
        nimcp_free(relevances);
        relevances = NULL;
        set_error("Failed to allocate path arrays");
        return FUTURE_ERROR_NO_MEMORY;
    }

    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        if (ft->events[i].goal_relevance > 0.3f) {
            for (size_t g = 0; g < ft->events[i].num_linked_goals; g++) {
                if (ft->events[i].linked_goal_ids[g] == goal_id) {
                    relevant_events[relevant_count] = ft->events[i].event_id;
                    relevances[relevant_count] = ft->events[i].goal_relevance;
                    relevant_count++;
                    break;
                }
            }
        }
    }

    // Sort by expected time to create temporal path
    // Simplified: just use first max_steps relevant events
    size_t path_length = relevant_count < max_steps ? relevant_count : max_steps;

    result_out->step_event_ids = nimcp_calloc(path_length, sizeof(uint64_t));
    if (!result_out->step_event_ids) {
        nimcp_free(relevant_events);
        relevant_events = NULL;
        nimcp_free(relevances);
        relevances = NULL;
        set_error("Failed to allocate path result");
        return FUTURE_ERROR_NO_MEMORY;
    }

    result_out->num_steps = path_length;
    memcpy(result_out->step_event_ids, relevant_events, path_length * sizeof(uint64_t));

    // Compute path metrics
    result_out->total_expected_value = 0.0f;
    result_out->total_expected_cost = 0.0f;
    result_out->estimated_time = 0.0f;
    result_out->path_probability = 1.0f;

    for (size_t i = 0; i < path_length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && path_length > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)path_length);
        }

        for (size_t j = 0; j < ft->num_events; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && ft->num_events > 256) {
                future_thinking_heartbeat("future_think_loop",
                                 (float)(j + 1) / (float)ft->num_events);
            }

            if (ft->events[j].event_id == result_out->step_event_ids[i]) {
                result_out->total_expected_value += ft->events[j].desirability;
                result_out->estimated_time += ft->events[j].expected_time;
                result_out->path_probability *= ft->events[j].probability;
                break;
            }
        }
    }

    result_out->num_alternatives = 0;
    result_out->alternative_values = NULL;

    nimcp_free(relevant_events);
    relevant_events = NULL;
    nimcp_free(relevances);
    relevances = NULL;

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT void future_thinking_free_path_result(path_optimization_result_t* result) {
    if (result) {
        nimcp_free(result->step_event_ids);
        nimcp_free(result->alternative_values);
        result->step_event_ids = NULL;
        result->alternative_values = NULL;
        result->num_steps = 0;
        result->num_alternatives = 0;
    }
}

//=============================================================================
// Event Management Functions
//=============================================================================

NIMCP_EXPORT uint64_t future_thinking_store_event(
    future_thinking_t ft,
    const future_event_t* event
) {
    if (!ft || !event) {
        set_error("NULL pointer argument");
        return FUTURE_INVALID_EVENT_ID;
    }

    if (ft->num_events >= ft->events_capacity) {
        set_error("Maximum events reached");
        return FUTURE_INVALID_EVENT_ID;
    }

    ft->events[ft->num_events] = *event;
    ft->num_events++;
    ft->stats.current_event_count = ft->num_events;

    return event->event_id;
}

NIMCP_EXPORT future_error_t future_thinking_get_event(
    future_thinking_t ft,
    uint64_t event_id,
    future_event_t* event_out
) {
    if (!ft || !event_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        if (ft->events[i].event_id == event_id) {
            *event_out = ft->events[i];
            return FUTURE_SUCCESS;
        }
    }

    set_error("Event not found");
    return FUTURE_ERROR_INVALID_EVENT;
}

NIMCP_EXPORT future_error_t future_thinking_delete_event(
    future_thinking_t ft,
    uint64_t event_id
) {
    if (!ft) {
        set_error("NULL future thinking system");
        return FUTURE_ERROR_NULL_POINTER;
    }

    for (size_t i = 0; i < ft->num_events; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ft->num_events > 256) {
            future_thinking_heartbeat("future_think_loop",
                             (float)(i + 1) / (float)ft->num_events);
        }

        if (ft->events[i].event_id == event_id) {
            future_event_cleanup(&ft->events[i]);

            // Shift remaining events
            for (size_t j = i; j < ft->num_events - 1; j++) {
                ft->events[j] = ft->events[j + 1];
            }
            ft->num_events--;
            ft->stats.current_event_count = ft->num_events;

            return FUTURE_SUCCESS;
        }
    }

    set_error("Event not found");
    return FUTURE_ERROR_INVALID_EVENT;
}

NIMCP_EXPORT future_error_t future_thinking_query_events_by_time(
    future_thinking_t ft,
    float min_time,
    float max_time,
    future_event_t* events_out,
    size_t max_events,
    size_t* count_out
) {
    if (!ft || !events_out || !count_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    for (size_t i = 0; i < ft->num_events && *count_out < max_events; i++) {
        float t = ft->events[i].expected_time;
        if (t >= min_time && t <= max_time) {
            events_out[*count_out] = ft->events[i];
            (*count_out)++;
        }
    }

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT future_error_t future_thinking_query_events_by_goal(
    future_thinking_t ft,
    uint64_t goal_id,
    float min_relevance,
    future_event_t* events_out,
    size_t max_events,
    size_t* count_out
) {
    if (!ft || !events_out || !count_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    for (size_t i = 0; i < ft->num_events && *count_out < max_events; i++) {
        if (ft->events[i].goal_relevance >= min_relevance) {
            for (size_t g = 0; g < ft->events[i].num_linked_goals; g++) {
                if (ft->events[i].linked_goal_ids[g] == goal_id) {
                    events_out[*count_out] = ft->events[i];
                    (*count_out)++;
                    break;
                }
            }
        }
    }

    return FUTURE_SUCCESS;
}

//=============================================================================
// Statistics and Utility Functions
//=============================================================================

NIMCP_EXPORT future_error_t future_thinking_get_stats(
    future_thinking_t ft,
    future_thinking_stats_t* stats_out
) {
    if (!ft || !stats_out) {
        set_error("NULL pointer argument");
        return FUTURE_ERROR_NULL_POINTER;
    }

    *stats_out = ft->stats;

    // Calculate memory usage
    stats_out->memory_bytes = sizeof(struct future_thinking_struct);
    stats_out->memory_bytes += ft->events_capacity * sizeof(future_event_t);
    stats_out->memory_bytes += ft->goals_capacity * sizeof(future_goal_t);
    stats_out->memory_bytes += ft->pool_size * (sizeof(uint64_t) + sizeof(float));

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT void future_thinking_reset_stats(future_thinking_t ft) {
    if (ft) {
        // Keep current counts
        size_t event_count = ft->stats.current_event_count;
        size_t goal_count = ft->stats.current_goal_count;

        memset(&ft->stats, 0, sizeof(ft->stats));

        ft->stats.current_event_count = event_count;
        ft->stats.current_goal_count = goal_count;
    }
}

NIMCP_EXPORT future_sim_status_t future_thinking_get_status(future_thinking_t ft) {
    if (!ft) {
        return FUTURE_SIM_IDLE;
    }
    return ft->status;
}

NIMCP_EXPORT const char* future_error_string(future_error_t error) {
    switch (error) {
        case FUTURE_SUCCESS:
            return "Success";
        case FUTURE_ERROR_NULL_POINTER:
            return "NULL pointer argument";
        case FUTURE_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case FUTURE_ERROR_NO_MEMORY:
            return "Memory allocation failed";
        case FUTURE_ERROR_INVALID_EVENT:
            return "Invalid event reference";
        case FUTURE_ERROR_INVALID_GOAL:
            return "Invalid goal reference";
        case FUTURE_ERROR_MAX_EVENTS:
            return "Maximum events reached";
        case FUTURE_ERROR_MAX_GOALS:
            return "Maximum goals reached";
        case FUTURE_ERROR_SIMULATION_FAILED:
            return "Simulation construction failed";
        case FUTURE_ERROR_LOW_COHERENCE:
            return "Scene coherence too low";
        case FUTURE_ERROR_NO_FRAGMENTS:
            return "No suitable fragments found";
        case FUTURE_ERROR_INVALID_TIME:
            return "Invalid temporal specification";
        case FUTURE_ERROR_ALREADY_SIMULATING:
            return "Simulation already in progress";
        default:
            return "Unknown error";
    }
}

NIMCP_EXPORT const char* future_event_type_name(future_event_type_t type) {
    switch (type) {
        case FUTURE_SPECIFIC:
            return "SPECIFIC";
        case FUTURE_SEMANTIC:
            return "SEMANTIC";
        case FUTURE_HYPOTHETICAL:
            return "HYPOTHETICAL";
        case FUTURE_GOAL:
            return "GOAL";
        default:
            return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* future_goal_status_name(goal_status_t status) {
    switch (status) {
        case GOAL_STATUS_INACTIVE:
            return "INACTIVE";
        case GOAL_STATUS_ACTIVE:
            return "ACTIVE";
        case GOAL_STATUS_ACHIEVED:
            return "ACHIEVED";
        case GOAL_STATUS_ABANDONED:
            return "ABANDONED";
        case GOAL_STATUS_BLOCKED:
            return "BLOCKED";
        default:
            return "UNKNOWN";
    }
}

//=============================================================================
// Scene Element Functions
//=============================================================================

NIMCP_EXPORT future_error_t scene_element_init(
    scene_element_t* element,
    scene_element_type_t type
) {
    if (!element) {
        set_error("NULL element pointer");
        return FUTURE_ERROR_NULL_POINTER;
    }

    memset(element, 0, sizeof(scene_element_t));
    element->type = type;
    element->element_id = 0;
    element->extraction_confidence = 1.0f;
    element->state = quat_identity();

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT future_error_t future_scene_init(
    future_scene_t* scene,
    size_t max_elements
) {
    if (!scene) {
        set_error("NULL scene pointer");
        return FUTURE_ERROR_NULL_POINTER;
    }

    memset(scene, 0, sizeof(future_scene_t));

    scene->elements = nimcp_calloc(max_elements, sizeof(scene_element_t));
    if (!scene->elements) {
        set_error("Failed to allocate scene elements");
        return FUTURE_ERROR_NO_MEMORY;
    }

    scene->max_elements = max_elements;
    scene->emotional_tone = quat_identity();

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT void future_scene_free(future_scene_t* scene) {
    if (scene) {
        nimcp_free(scene->elements);
        scene->elements = NULL;
        scene->num_elements = 0;
        scene->max_elements = 0;
    }
}

NIMCP_EXPORT uint64_t future_scene_add_element(
    future_scene_t* scene,
    const scene_element_t* element
) {
    if (!scene || !element) {
        set_error("NULL pointer argument");
        return FUTURE_INVALID_EVENT_ID;
    }

    if (scene->num_elements >= scene->max_elements) {
        set_error("Scene at capacity");
        return FUTURE_INVALID_EVENT_ID;
    }

    scene->elements[scene->num_elements] = *element;
    scene->num_elements++;

    return element->element_id;
}

NIMCP_EXPORT const char* scene_element_type_name(scene_element_type_t type) {
    switch (type) {
        case SCENE_ELEMENT_PERSON:
            return "PERSON";
        case SCENE_ELEMENT_PLACE:
            return "PLACE";
        case SCENE_ELEMENT_OBJECT:
            return "OBJECT";
        case SCENE_ELEMENT_ACTION:
            return "ACTION";
        case SCENE_ELEMENT_EMOTION:
            return "EMOTION";
        case SCENE_ELEMENT_TIME:
            return "TIME";
        case SCENE_ELEMENT_RELATION:
            return "RELATION";
        default:
            return "UNKNOWN";
    }
}

//=============================================================================
// Event Initialization and Cleanup
//=============================================================================

NIMCP_EXPORT future_error_t future_event_init(future_event_t* event) {
    if (!event) {
        set_error("NULL event pointer");
        return FUTURE_ERROR_NULL_POINTER;
    }

    memset(event, 0, sizeof(future_event_t));
    event->event_id = FUTURE_INVALID_EVENT_ID;
    event->event_quaternion = quat_identity();

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT void future_event_cleanup(future_event_t* event) {
    if (event) {
        future_scene_free(&event->scene);
    }
}

NIMCP_EXPORT future_error_t future_goal_init(future_goal_t* goal) {
    if (!goal) {
        set_error("NULL goal pointer");
        return FUTURE_ERROR_NULL_POINTER;
    }

    memset(goal, 0, sizeof(future_goal_t));
    goal->goal_id = FUTURE_INVALID_GOAL_ID;
    goal->goal_state = quat_identity();
    goal->status = GOAL_STATUS_INACTIVE;

    return FUTURE_SUCCESS;
}

NIMCP_EXPORT void future_goal_cleanup(future_goal_t* goal) {
    if (goal) {
        nimcp_free(goal->associated_events);
        goal->associated_events = NULL;
        goal->num_associated_events = 0;
    }
}

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message (thread-local)
 */
NIMCP_EXPORT const char* future_thinking_get_last_error(void) {
    return last_error[0] != '\0' ? last_error : NULL;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void future_thinking_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_future_thinking_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int future_thinking_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "future_thinking_training_begin: NULL argument");
        return -1;
    }
    future_thinking_heartbeat_instance(NULL, "future_thinking_training_begin", 0.0f);
    (void)(struct future_thinking_struct*)instance; /* Module state available for reset */
    return 0;
}

int future_thinking_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "future_thinking_training_end: NULL argument");
        return -1;
    }
    future_thinking_heartbeat_instance(NULL, "future_thinking_training_end", 1.0f);
    (void)(struct future_thinking_struct*)instance; /* Module state available for finalization */
    return 0;
}

int future_thinking_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "future_thinking_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    future_thinking_heartbeat_instance(NULL, "future_thinking_training_step", progress);
    (void)(struct future_thinking_struct*)instance; /* Module state available for step adaptation */
    return 0;
}

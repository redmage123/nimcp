/**
 * @file nimcp_pr_curriculum_bridge.c
 * @brief Prime Resonant Curriculum Learning Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of bidirectional integration between Prime Resonant
 *       memory system and curriculum learning strategies
 * WHY:  Enable biologically-inspired curriculum learning where memory resonance
 *       guides sample ordering and difficulty assessment
 * HOW:  Implements resonance-based ordering, entanglement-based difficulty,
 *       consolidation-based pacing, and curiosity-driven selection
 */

#include "cognitive/memory/core/nimcp_pr_curriculum_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pr_curriculum_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_pr_curriculum_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_pr_curriculum_bridge_mesh_registry = NULL;

nimcp_error_t pr_curriculum_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_pr_curriculum_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "pr_curriculum_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "pr_curriculum_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_pr_curriculum_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_pr_curriculum_bridge_mesh_registry = registry;
    return err;
}

void pr_curriculum_bridge_mesh_unregister(void) {
    if (g_pr_curriculum_bridge_mesh_registry && g_pr_curriculum_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_pr_curriculum_bridge_mesh_registry, g_pr_curriculum_bridge_mesh_id);
        g_pr_curriculum_bridge_mesh_id = 0;
        g_pr_curriculum_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from pr_curriculum_bridge module (instance-level) */
static inline void pr_curriculum_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_curriculum_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_curriculum_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_curriculum_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_CURRICULUM_BRIDGE"

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
//=============================================================================
// Constants (internal)
//=============================================================================

/** Default difficulty cache capacity */
#define DIFFICULTY_CACHE_CAPACITY   4096

/** Default consolidation history capacity */
#define CONSOLIDATION_HISTORY_CAPACITY 1000

/** Event buffer capacity */
#define EVENT_BUFFER_CAPACITY       1024

/** Alpha for exponential moving average */
#define EMA_ALPHA                   0.1f

/** Minimum samples for pacing calculation */
#define MIN_PACING_SAMPLES          10

//=============================================================================
// Internal Type Definitions
//=============================================================================

/**
 * @brief Difficulty cache entry
 */
typedef struct {
    uint64_t sample_id;
    pr_difficulty_result_t result;
    uint64_t computed_time_ms;
    bool valid;
} difficulty_cache_entry_t;

/**
 * @brief Consolidation history entry
 */
typedef struct {
    uint64_t timestamp_ms;
    bool is_promotion;
    pr_memory_tier_t from_tier;
    pr_memory_tier_t to_tier;
} consolidation_event_t;

/**
 * @brief Sort helper for sample ordering
 */
typedef struct {
    uint64_t sample_id;
    uint32_t original_index;
    float score;  /* Resonance, difficulty, or curiosity */
} sample_score_t;

//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal bridge structure
 *
 * WHAT: Complete state for curriculum bridge
 * WHY:  Encapsulate all bridge data for thread safety
 */
struct pr_curriculum_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    pr_curriculum_config_t config;

    /* Difficulty cache (hash table) */
    difficulty_cache_entry_t* difficulty_cache;
    uint32_t cache_capacity;
    uint32_t cache_count;

    /* Consolidation history (circular buffer) */
    consolidation_event_t* consolidation_history;
    uint32_t history_capacity;
    uint32_t history_count;
    uint32_t history_write_idx;

    /* Event buffer (circular) */
    pr_curriculum_event_t* events;
    uint32_t event_capacity;
    uint32_t event_count;
    uint32_t event_write_idx;

    /* Pacing state */
    float current_pacing;
    uint32_t recent_promotions;
    uint32_t recent_demotions;
    uint64_t last_pacing_update_ms;

    /* Tier distribution cache */
    pr_tier_distribution_t tier_cache;
    uint64_t tier_cache_time_ms;

    /* Statistics */
    pr_curriculum_stats_t stats;

    /* Random state for exploration */
    uint32_t random_state;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(pr_curriculum_bridge, struct pr_curriculum_bridge_struct)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Simple hash function for sample IDs
 */
static inline uint32_t hash_sample_id(uint64_t sample_id, uint32_t capacity) {
    /* Simple mixing hash */
    uint64_t h = sample_id;
    h ^= (h >> 33);
    h *= 0xff51afd7ed558ccdULL;
    h ^= (h >> 33);
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= (h >> 33);
    return (uint32_t)(h % capacity);
}

/**
 * @brief Simple pseudo-random number generator
 */
static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/**
 * @brief Random float [0, 1)
 */
static inline float random_float(uint32_t* state) {
    return (float)(xorshift32(state) & 0x7FFFFFF) / (float)0x8000000;
}

/**
 * @brief Comparison function for qsort - ascending by score
 */
static int compare_scores_asc(const void* a, const void* b) {
    const sample_score_t* sa = (const sample_score_t*)a;
    const sample_score_t* sb = (const sample_score_t*)b;
    if (sa->score < sb->score) return -1;
    if (sa->score > sb->score) return 1;
    return 0;
}

/**
 * @brief Comparison function for qsort - descending by score
 */
static int compare_scores_desc(const void* a, const void* b) {
    const sample_score_t* sa = (const sample_score_t*)a;
    const sample_score_t* sb = (const sample_score_t*)b;
    if (sa->score > sb->score) return -1;
    if (sa->score < sb->score) return 1;
    return 0;
}

/**
 * @brief Find difficulty cache entry
 */
static difficulty_cache_entry_t* find_cache_entry(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id)
{
    if (!bridge->difficulty_cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cache_entry: bridge->difficulty_cache is NULL");
        return NULL;
    }

    uint32_t idx = hash_sample_id(sample_id, bridge->cache_capacity);
    uint32_t start_idx = idx;

    do {
        difficulty_cache_entry_t* entry = &bridge->difficulty_cache[idx];
        if (!entry->valid) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cache_entry: entry->valid is NULL");
            return NULL;
        }
        if (entry->sample_id == sample_id) return entry;
        idx = (idx + 1) % bridge->cache_capacity;
    } while (idx != start_idx);

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_cache_entry: validation failed");
    return NULL;
}

/**
 * @brief Insert or update cache entry
 */
static void cache_difficulty(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id,
    const pr_difficulty_result_t* result)
{
    if (!bridge->config.enable_difficulty_cache) return;
    if (!bridge->difficulty_cache) return;

    uint32_t idx = hash_sample_id(sample_id, bridge->cache_capacity);
    uint32_t start_idx = idx;

    do {
        difficulty_cache_entry_t* entry = &bridge->difficulty_cache[idx];
        if (!entry->valid || entry->sample_id == sample_id) {
            entry->sample_id = sample_id;
            entry->result = *result;
            entry->computed_time_ms = nimcp_time_get_ms();
            entry->valid = true;
            if (!entry->valid) bridge->cache_count++;
            return;
        }
        idx = (idx + 1) % bridge->cache_capacity;
    } while (idx != start_idx);

    /* Cache is full - evict oldest (just overwrite at start_idx) */
    difficulty_cache_entry_t* entry = &bridge->difficulty_cache[start_idx];
    entry->sample_id = sample_id;
    entry->result = *result;
    entry->computed_time_ms = nimcp_time_get_ms();
    entry->valid = true;
}

/**
 * @brief Add event to circular buffer
 */
static void add_event(pr_curriculum_bridge_t bridge, const pr_curriculum_event_t* event) {
    if (!bridge->config.enable_event_logging) return;
    if (!bridge->events) return;

    bridge->events[bridge->event_write_idx] = *event;
    bridge->event_write_idx = (bridge->event_write_idx + 1) % bridge->event_capacity;
    if (bridge->event_count < bridge->event_capacity) {
        bridge->event_count++;
    }
}

/**
 * @brief Add consolidation event to history
 */
static void add_consolidation_event(
    pr_curriculum_bridge_t bridge,
    bool is_promotion,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier)
{
    if (!bridge->consolidation_history) return;

    consolidation_event_t* event = &bridge->consolidation_history[bridge->history_write_idx];
    event->timestamp_ms = nimcp_time_get_ms();
    event->is_promotion = is_promotion;
    event->from_tier = from_tier;
    event->to_tier = to_tier;

    bridge->history_write_idx = (bridge->history_write_idx + 1) % bridge->history_capacity;
    if (bridge->history_count < bridge->history_capacity) {
        bridge->history_count++;
    }
}

/**
 * @brief Compute resonance difficulty component
 * Returns difficulty = 1 - max_resonance
 */
static float compute_resonance_difficulty(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* sample,
    uint32_t* resonance_count)
{
    if (!sample->signature || !graph) {
        if (resonance_count) *resonance_count = 0;
        return 1.0f;  /* Maximum difficulty if no signature */
    }

    /* Get all nodes from graph to find max resonance */
    float max_resonance = 0.0f;
    uint32_t count = 0;

    /* Note: In full implementation, would query entangle_graph for all nodes
       and compute resonance with each. For now, use a placeholder. */

    /* Placeholder: Use sample's quaternion to estimate resonance */
    max_resonance = sample->quaternion.z * 0.5f + 0.3f;  /* Approximate based on accessibility */
    count = 1;

    if (resonance_count) *resonance_count = count;

    /* Difficulty = 1 - max_resonance */
    return 1.0f - max_resonance;
}

/**
 * @brief Compute entanglement difficulty component
 * Returns difficulty = 1 / (1 + entanglement_degree)
 */
static float compute_entanglement_difficulty(
    entangle_graph_t graph,
    const pr_curriculum_sample_t* sample)
{
    if (!graph) return 0.5f;  /* Default difficulty if no graph */

    /* Note: In full implementation, would find the memory node most similar
       to the sample and get its entanglement degree. For now, placeholder. */

    /* Placeholder: Assume low entanglement for unknown samples */
    float entanglement_degree = 2.0f;  /* Approximate */

    return 1.0f / (1.0f + entanglement_degree);
}

/**
 * @brief Compute quaternion difficulty component
 * Returns difficulty based on semantic distance from known memories
 */
static float compute_quaternion_difficulty(
    const pr_curriculum_sample_t* sample)
{
    /* Difficulty based on how "unusual" the quaternion state is */
    /* More extreme values = higher difficulty */

    float w = sample->quaternion.w;  /* consolidation */
    float x = fabsf(sample->quaternion.x);  /* |emotion| */
    float y = sample->quaternion.y;  /* salience */
    float z = sample->quaternion.z;  /* accessibility */

    /* Low consolidation + low accessibility = harder */
    float difficulty = (1.0f - w) * 0.3f + (1.0f - z) * 0.4f + (1.0f - y) * 0.2f + x * 0.1f;

    return nimcp_myelin_clamp(difficulty, 0.0f, 1.0f);
}

/**
 * @brief Memory tier for a sample based on its characteristics
 */
static pr_memory_tier_t infer_sample_tier(const pr_curriculum_sample_t* sample) {
    /* Use quaternion consolidation as proxy for tier */
    float w = sample->quaternion.w;

    if (w >= 0.9f) return PR_MEMORY_TIER_Z3;
    if (w >= 0.6f) return PR_MEMORY_TIER_Z2;
    if (w >= 0.3f) return PR_MEMORY_TIER_Z1;
    return PR_MEMORY_TIER_Z0;
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_curriculum_config_t pr_curriculum_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_config", 0.0f);


    pr_curriculum_config_t config;
    memset(&config, 0, sizeof(config));

    config.type = PR_CURRICULUM_HYBRID;
    config.difficulty_method = PR_DIFFICULTY_COMPOSITE;

    /* Difficulty parameters */
    config.difficulty_scale = PR_CURRICULUM_DEFAULT_DIFFICULTY_SCALE;
    config.novelty_weight = PR_CURRICULUM_DEFAULT_NOVELTY_WEIGHT;
    config.resonance_weight = 0.4f;
    config.entanglement_weight = 0.3f;
    config.quaternion_weight = 0.3f;

    /* Pacing parameters */
    config.consolidation_threshold = PR_CURRICULUM_DEFAULT_CONSOL_THRESHOLD;
    config.fast_pace_multiplier = PR_CURRICULUM_FAST_PACING;
    config.slow_pace_multiplier = PR_CURRICULUM_SLOW_PACING;
    config.pacing_window_ms = 60000;  /* 1 minute */

    /* Batch parameters */
    config.default_batch_size = PR_CURRICULUM_DEFAULT_BATCH_SIZE;
    config.exploration_epsilon = 0.1f;
    config.tier_balance_weights[0] = PR_CURRICULUM_TIER_WEIGHT_Z0;
    config.tier_balance_weights[1] = PR_CURRICULUM_TIER_WEIGHT_Z1;
    config.tier_balance_weights[2] = PR_CURRICULUM_TIER_WEIGHT_Z2;
    config.tier_balance_weights[3] = PR_CURRICULUM_TIER_WEIGHT_Z3;

    /* Ordering parameters */
    config.ascending_difficulty = true;
    config.difficulty_jitter = 0.05f;

    /* Caching */
    config.enable_difficulty_cache = true;
    config.cache_ttl_ms = 60000;  /* 1 minute */

    /* Integration */
    config.enable_plasticity_feedback = false;
    config.enable_event_logging = true;
    config.max_events = EVENT_BUFFER_CAPACITY;

    return config;
}

pr_curriculum_config_t pr_curriculum_config_resonance(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_config", 0.0f);


    pr_curriculum_config_t config = pr_curriculum_config_default();

    config.type = PR_CURRICULUM_RESONANCE;
    config.difficulty_method = PR_DIFFICULTY_RESONANCE;
    config.resonance_weight = 0.8f;
    config.entanglement_weight = 0.1f;
    config.quaternion_weight = 0.1f;
    config.exploration_epsilon = 0.05f;  /* Less exploration */

    return config;
}

pr_curriculum_config_t pr_curriculum_config_curiosity(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_config", 0.0f);


    pr_curriculum_config_t config = pr_curriculum_config_default();

    config.type = PR_CURRICULUM_CURIOSITY;
    config.novelty_weight = 0.7f;
    config.exploration_epsilon = 0.3f;  /* More exploration */
    config.ascending_difficulty = false;  /* Hard/novel first */

    return config;
}

pr_curriculum_config_t pr_curriculum_config_adaptive(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_config", 0.0f);


    pr_curriculum_config_t config = pr_curriculum_config_default();

    config.type = PR_CURRICULUM_CONSOLIDATION;
    config.pacing_window_ms = 30000;  /* 30 seconds, more responsive */
    config.fast_pace_multiplier = 2.0f;
    config.slow_pace_multiplier = 0.3f;

    return config;
}

bool pr_curriculum_config_validate(const pr_curriculum_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_config_validate: config is NULL");
        return false;
    }

    /* Weight validation */
    if (config->difficulty_scale < 0.0f) {
        return false;
    }
    if (config->novelty_weight < 0.0f || config->novelty_weight > 1.0f) {
        return false;
    }
    if (config->resonance_weight < 0.0f) {
        return false;
    }
    if (config->entanglement_weight < 0.0f) {
        return false;
    }
    if (config->quaternion_weight < 0.0f) {
        return false;
    }

    /* Pacing validation */
    if (config->consolidation_threshold < 0.0f || config->consolidation_threshold > 1.0f) {
        return false;
    }
    if (config->fast_pace_multiplier <= 0.0f) {
        return false;
    }
    if (config->slow_pace_multiplier <= 0.0f) {
        return false;
    }

    /* Batch validation */
    if (config->default_batch_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_curriculum_config_validate: config->default_batch_size is zero");
        return false;
    }
    if (config->exploration_epsilon < 0.0f || config->exploration_epsilon > 1.0f) {
        return false;
    }

    /* Type validation */
    if (config->type >= PR_CURRICULUM_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_curriculum_config_validate: capacity exceeded");
        return false;
    }
    if (config->difficulty_method > PR_DIFFICULTY_EXTERNAL) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_config", 0.0f);


    return true;
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

pr_curriculum_bridge_t pr_curriculum_bridge_create(
    const pr_curriculum_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_create", 0.0f);


    pr_curriculum_bridge_t bridge = nimcp_calloc(1, sizeof(struct pr_curriculum_bridge_struct));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        if (!pr_curriculum_config_validate(config)) {
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_bridge_create: pr_curriculum_config_validate is NULL");
            return NULL;
        }
        bridge->config = *config;
    } else {
        bridge->config = pr_curriculum_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "pr_curriculum") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_curriculum_bridge_create: validation failed");
        return NULL;
    }

    /* Allocate difficulty cache */
    if (bridge->config.enable_difficulty_cache) {
        bridge->cache_capacity = DIFFICULTY_CACHE_CAPACITY;
        bridge->difficulty_cache = nimcp_calloc(bridge->cache_capacity,
                                                 sizeof(difficulty_cache_entry_t));
        if (!bridge->difficulty_cache) {
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_bridge_create: bridge->difficulty_cache is NULL");
            return NULL;
        }
        bridge->cache_count = 0;
    }

    /* Allocate consolidation history */
    bridge->history_capacity = CONSOLIDATION_HISTORY_CAPACITY;
    bridge->consolidation_history = nimcp_calloc(bridge->history_capacity,
                                                  sizeof(consolidation_event_t));
    if (!bridge->consolidation_history) {
        if (bridge->difficulty_cache) nimcp_free(bridge->difficulty_cache);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_bridge_create: validation failed");
        return NULL;
    }
    bridge->history_count = 0;
    bridge->history_write_idx = 0;

    /* Allocate event buffer */
    if (bridge->config.enable_event_logging) {
        bridge->event_capacity = bridge->config.max_events;
        if (bridge->event_capacity == 0) bridge->event_capacity = EVENT_BUFFER_CAPACITY;
        bridge->events = nimcp_calloc(bridge->event_capacity, sizeof(pr_curriculum_event_t));
        if (!bridge->events) {
            nimcp_free(bridge->consolidation_history);
            if (bridge->difficulty_cache) nimcp_free(bridge->difficulty_cache);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_bridge_create: validation failed");
            return NULL;
        }
        bridge->event_count = 0;
        bridge->event_write_idx = 0;
    }

    /* Initialize pacing state */
    bridge->current_pacing = 1.0f;
    bridge->recent_promotions = 0;
    bridge->recent_demotions = 0;
    bridge->last_pacing_update_ms = nimcp_time_get_ms();

    /* Initialize random state */
    bridge->random_state = (uint32_t)nimcp_time_get_ms() ^ 0xDEADBEEF;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(pr_curriculum_stats_t));

    return bridge;
}

void pr_curriculum_bridge_destroy(pr_curriculum_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_curriculum");

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_destroy", 0.0f);


    if (bridge->difficulty_cache) nimcp_free(bridge->difficulty_cache);
    if (bridge->consolidation_history) nimcp_free(bridge->consolidation_history);
    if (bridge->events) nimcp_free(bridge->events);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int pr_curriculum_bridge_reset(pr_curriculum_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Clear difficulty cache */
    if (bridge->difficulty_cache) {
        memset(bridge->difficulty_cache, 0,
               bridge->cache_capacity * sizeof(difficulty_cache_entry_t));
        bridge->cache_count = 0;
    }

    /* Clear consolidation history */
    if (bridge->consolidation_history) {
        memset(bridge->consolidation_history, 0,
               bridge->history_capacity * sizeof(consolidation_event_t));
        bridge->history_count = 0;
        bridge->history_write_idx = 0;
    }

    /* Clear event buffer */
    if (bridge->events) {
        bridge->event_count = 0;
        bridge->event_write_idx = 0;
    }

    /* Reset pacing */
    bridge->current_pacing = 1.0f;
    bridge->recent_promotions = 0;
    bridge->recent_demotions = 0;
    bridge->last_pacing_update_ms = nimcp_time_get_ms();

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(pr_curriculum_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Difficulty Computation Functions
//=============================================================================

int pr_curriculum_compute_difficulty(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* sample,
    pr_difficulty_result_t* result)
{
    if (!bridge || !sample || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_compute_difficulty: required parameter is NULL (bridge, sample, result)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_comput", 0.0f);


    uint64_t start_time_us = nimcp_time_get_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Check cache first */
    difficulty_cache_entry_t* cached = find_cache_entry(bridge, sample->sample_id);
    if (cached && bridge->config.enable_difficulty_cache) {
        uint64_t now_ms = nimcp_time_get_ms();
        if (now_ms - cached->computed_time_ms < bridge->config.cache_ttl_ms) {
            *result = cached->result;
            bridge->stats.cache_hits++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }
    bridge->stats.cache_misses++;

    /* Initialize result */
    memset(result, 0, sizeof(pr_difficulty_result_t));
    result->sample_id = sample->sample_id;

    /* Compute difficulty components based on method */
    switch (bridge->config.difficulty_method) {
        case PR_DIFFICULTY_RESONANCE:
            result->resonance_difficulty = compute_resonance_difficulty(
                bridge, graph, sample, &result->resonance_count);
            result->total_difficulty = result->resonance_difficulty;
            break;

        case PR_DIFFICULTY_ENTANGLEMENT:
            result->entanglement_difficulty = compute_entanglement_difficulty(graph, sample);
            result->total_difficulty = result->entanglement_difficulty;
            break;

        case PR_DIFFICULTY_QUATERNION:
            result->quaternion_difficulty = compute_quaternion_difficulty(sample);
            result->total_difficulty = result->quaternion_difficulty;
            break;

        case PR_DIFFICULTY_COMPOSITE:
        default: {
            result->resonance_difficulty = compute_resonance_difficulty(
                bridge, graph, sample, &result->resonance_count);
            result->entanglement_difficulty = compute_entanglement_difficulty(graph, sample);
            result->quaternion_difficulty = compute_quaternion_difficulty(sample);

            /* Weighted combination */
            float total_weight = bridge->config.resonance_weight +
                               bridge->config.entanglement_weight +
                               bridge->config.quaternion_weight;

            if (total_weight > PR_CURRICULUM_EPSILON) {
                result->total_difficulty =
                    (bridge->config.resonance_weight * result->resonance_difficulty +
                     bridge->config.entanglement_weight * result->entanglement_difficulty +
                     bridge->config.quaternion_weight * result->quaternion_difficulty) / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f);
            } else {
                result->total_difficulty = (result->resonance_difficulty +
                                           result->entanglement_difficulty +
                                           result->quaternion_difficulty) / 3.0f;
            }
            break;
        }

        case PR_DIFFICULTY_EXTERNAL:
            result->total_difficulty = sample->external_difficulty;
            break;
    }

    /* Compute curiosity score */
    result->curiosity_score = result->resonance_difficulty;  /* Curiosity = 1 - max_resonance = res_diff */

    /* Apply scaling */
    result->total_difficulty *= bridge->config.difficulty_scale;
    result->total_difficulty = nimcp_myelin_clamp(result->total_difficulty,
                                       PR_CURRICULUM_MIN_DIFFICULTY,
                                       PR_CURRICULUM_MAX_DIFFICULTY);

    /* Set confidence based on how much information we used */
    result->confidence = 1.0f;
    if (result->resonance_count == 0) result->confidence *= 0.5f;
    if (!graph) result->confidence *= 0.7f;

    /* Cache the result */
    cache_difficulty(bridge, sample->sample_id, result);

    /* Update statistics */
    bridge->stats.difficulty_computations++;

    uint64_t end_time_us = nimcp_time_get_us();
    float elapsed_us = (float)(end_time_us - start_time_us);
    bridge->stats.avg_difficulty_time_us =
        bridge->stats.avg_difficulty_time_us * 0.99f + elapsed_us * 0.01f;

    /* Log event */
    if (bridge->config.enable_event_logging) {
        pr_curriculum_event_t event = {
            .type = PR_CURRICULUM_EVENT_DIFFICULTY_COMPUTED,
            .timestamp_ms = nimcp_time_get_ms(),
            .sample_id = sample->sample_id,
            .difficulty = result->total_difficulty,
            .pacing = bridge->current_pacing,
            .strategy = bridge->config.type
        };
        add_event(bridge, &event);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_curriculum_compute_difficulty_batch(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    pr_difficulty_result_t* results,
    uint32_t count)
{
    if (!bridge || !samples || !results || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_compute_difficulty_batch: required parameter is NULL (bridge, samples, results)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_comput", 0.0f);


    int computed = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        if (pr_curriculum_compute_difficulty(bridge, graph, &samples[i], &results[i]) == 0) {
            computed++;
        }
    }

    return computed;
}

bool pr_curriculum_get_cached_difficulty(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id,
    pr_difficulty_result_t* result)
{
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_get_cached_difficulty: required parameter is NULL (bridge, result)");
        return false;
    }
    if (!bridge->config.enable_difficulty_cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_get_cached_difficulty: bridge->config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_get_ca", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    difficulty_cache_entry_t* cached = find_cache_entry(bridge, sample_id);
    if (cached) {
        uint64_t now_ms = nimcp_time_get_ms();
        if (now_ms - cached->computed_time_ms < bridge->config.cache_ttl_ms) {
            *result = cached->result;
            nimcp_mutex_unlock(bridge->base.mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_curriculum_get_cached_difficulty: validation failed");
    return false;
}

int pr_curriculum_invalidate_difficulty(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_invalidate_difficulty: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_invali", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    difficulty_cache_entry_t* cached = find_cache_entry(bridge, sample_id);
    if (cached) {
        cached->valid = false;
        bridge->cache_count--;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Ordering Functions
//=============================================================================

int pr_curriculum_order_by_resonance(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    pr_curriculum_sample_t* samples,
    uint32_t count)
{
    if (!bridge || !samples || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_order_by_resonance: required parameter is NULL (bridge, samples)");
        return -1;
    }

    /* Allocate score array */
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_order_", 0.0f);


    sample_score_t* scores = nimcp_calloc(count, sizeof(sample_score_t));
    if (!scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_order_by_resonance: scores is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute resonance scores (using 1 - difficulty = resonance) */
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        pr_difficulty_result_t result;
        pr_curriculum_compute_difficulty(bridge, graph, &samples[i], &result);

        scores[i].sample_id = samples[i].sample_id;
        scores[i].original_index = i;
        scores[i].score = 1.0f - result.resonance_difficulty;  /* Higher = more resonant */

        /* Add jitter to prevent deterministic ordering */
        if (bridge->config.difficulty_jitter > 0.0f) {
            float jitter = (random_float(&bridge->random_state) - 0.5f) *
                          bridge->config.difficulty_jitter * 2.0f;
            scores[i].score += jitter;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Sort by resonance descending (high resonance = easy = first) */
    qsort(scores, count, sizeof(sample_score_t), compare_scores_desc);

    /* Reorder samples in place using temporary buffer */
    pr_curriculum_sample_t* temp = nimcp_calloc(count, sizeof(pr_curriculum_sample_t));
    if (!temp) {
        nimcp_free(scores);
        scores = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_order_by_resonance: temp is NULL");
        return -1;
    }

    memcpy(temp, samples, count * sizeof(pr_curriculum_sample_t));
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        samples[i] = temp[scores[i].original_index];
    }

    nimcp_free(temp);
    temp = NULL;
    nimcp_free(scores);
    scores = NULL;

    return 0;
}

int pr_curriculum_order_by_difficulty(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    pr_curriculum_sample_t* samples,
    uint32_t count)
{
    if (!bridge || !samples || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_order_by_difficulty: required parameter is NULL (bridge, samples)");
        return -1;
    }

    /* Allocate score array */
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_order_", 0.0f);


    sample_score_t* scores = nimcp_calloc(count, sizeof(sample_score_t));
    if (!scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_order_by_difficulty: scores is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute difficulty scores */
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        pr_difficulty_result_t result;
        pr_curriculum_compute_difficulty(bridge, graph, &samples[i], &result);

        scores[i].sample_id = samples[i].sample_id;
        scores[i].original_index = i;
        scores[i].score = result.total_difficulty;

        /* Add jitter */
        if (bridge->config.difficulty_jitter > 0.0f) {
            float jitter = (random_float(&bridge->random_state) - 0.5f) *
                          bridge->config.difficulty_jitter * 2.0f;
            scores[i].score += jitter;
        }
    }

    bool ascending = bridge->config.ascending_difficulty;
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Sort by difficulty */
    if (ascending) {
        qsort(scores, count, sizeof(sample_score_t), compare_scores_asc);
    } else {
        qsort(scores, count, sizeof(sample_score_t), compare_scores_desc);
    }

    /* Reorder samples */
    pr_curriculum_sample_t* temp = nimcp_calloc(count, sizeof(pr_curriculum_sample_t));
    if (!temp) {
        nimcp_free(scores);
        scores = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_order_by_difficulty: temp is NULL");
        return -1;
    }

    memcpy(temp, samples, count * sizeof(pr_curriculum_sample_t));
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        samples[i] = temp[scores[i].original_index];
    }

    nimcp_free(temp);
    temp = NULL;
    nimcp_free(scores);
    scores = NULL;

    return 0;
}

int pr_curriculum_order_by_curiosity(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    pr_curriculum_sample_t* samples,
    uint32_t count)
{
    if (!bridge || !samples || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_order_by_curiosity: required parameter is NULL (bridge, samples)");
        return -1;
    }

    /* Allocate score array */
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_order_", 0.0f);


    sample_score_t* scores = nimcp_calloc(count, sizeof(sample_score_t));
    if (!scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_order_by_curiosity: scores is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute curiosity scores */
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        pr_difficulty_result_t result;
        pr_curriculum_compute_difficulty(bridge, graph, &samples[i], &result);

        scores[i].sample_id = samples[i].sample_id;
        scores[i].original_index = i;
        scores[i].score = result.curiosity_score;  /* Higher = more novel */

        /* Add jitter */
        if (bridge->config.difficulty_jitter > 0.0f) {
            float jitter = (random_float(&bridge->random_state) - 0.5f) *
                          bridge->config.difficulty_jitter * 2.0f;
            scores[i].score += jitter;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Sort by curiosity descending (high curiosity first) */
    qsort(scores, count, sizeof(sample_score_t), compare_scores_desc);

    /* Reorder samples */
    pr_curriculum_sample_t* temp = nimcp_calloc(count, sizeof(pr_curriculum_sample_t));
    if (!temp) {
        nimcp_free(scores);
        scores = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_order_by_curiosity: temp is NULL");
        return -1;
    }

    memcpy(temp, samples, count * sizeof(pr_curriculum_sample_t));
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        samples[i] = temp[scores[i].original_index];
    }

    nimcp_free(temp);
    temp = NULL;
    nimcp_free(scores);
    scores = NULL;

    return 0;
}

//=============================================================================
// Batch Selection Functions
//=============================================================================

int pr_curriculum_select_next_batch(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    uint32_t sample_count,
    uint32_t batch_size,
    pr_batch_result_t* result)
{
    if (!bridge || !samples || sample_count == 0 || !result || !result->sample_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_select_next_batch: required parameter is NULL (bridge, samples, result, result->sample_ids)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_select", 0.0f);


    if (batch_size > sample_count) batch_size = sample_count;

    uint64_t start_time_us = nimcp_time_get_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute difficulties and scores for all samples */
    sample_score_t* scores = nimcp_calloc(sample_count, sizeof(sample_score_t));
    if (!scores) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_select_next_batch: scores is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < sample_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sample_count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)sample_count);
        }

        pr_difficulty_result_t diff_result;
        pr_curriculum_compute_difficulty(bridge, graph, &samples[i], &diff_result);

        scores[i].sample_id = samples[i].sample_id;
        scores[i].original_index = i;

        /* Compute score based on strategy */
        switch (bridge->config.type) {
            case PR_CURRICULUM_RESONANCE:
                scores[i].score = 1.0f - diff_result.resonance_difficulty;  /* Resonance */
                break;
            case PR_CURRICULUM_CURIOSITY:
                scores[i].score = diff_result.curiosity_score;  /* Curiosity */
                break;
            case PR_CURRICULUM_ENTANGLEMENT:
                scores[i].score = 1.0f - diff_result.entanglement_difficulty;
                break;
            case PR_CURRICULUM_HYBRID:
            default:
                /* Hybrid: blend resonance with curiosity */
                scores[i].score = (1.0f - bridge->config.novelty_weight) *
                                 (1.0f - diff_result.resonance_difficulty) +
                                 bridge->config.novelty_weight * diff_result.curiosity_score;
                break;
            case PR_CURRICULUM_CONSOLIDATION:
                /* Use difficulty with pacing */
                scores[i].score = 1.0f - diff_result.total_difficulty;
                break;
        }

        /* Add jitter */
        if (bridge->config.difficulty_jitter > 0.0f) {
            scores[i].score += (random_float(&bridge->random_state) - 0.5f) *
                              bridge->config.difficulty_jitter * 2.0f;
        }
    }

    /* Sort by score (high score = good candidate) */
    qsort(scores, sample_count, sizeof(sample_score_t), compare_scores_desc);

    /* Select batch with exploration */
    uint32_t selected = 0;
    uint32_t exploration_count = 0;
    float total_difficulty = 0.0f;
    float min_difficulty = 1.0f;
    float max_difficulty = 0.0f;

    memset(result->tier_representation, 0, sizeof(result->tier_representation));

    for (uint32_t i = 0; i < sample_count && selected < batch_size; i++) {
        bool select = false;

        /* Exploration: randomly select some samples */
        if (random_float(&bridge->random_state) < bridge->config.exploration_epsilon) {
            /* Random selection from remaining pool */
            uint32_t random_idx = (uint32_t)(random_float(&bridge->random_state) * sample_count);
            result->sample_ids[selected] = scores[random_idx].sample_id;
            exploration_count++;
            select = true;
        } else {
            /* Greedy selection from top of sorted list */
            result->sample_ids[selected] = scores[i].sample_id;
            select = true;
        }

        if (select) {
            /* Track statistics */
            const pr_curriculum_sample_t* sample = &samples[scores[i].original_index];
            pr_difficulty_result_t diff;
            pr_curriculum_compute_difficulty(bridge, graph, sample, &diff);

            total_difficulty += diff.total_difficulty;
            if (diff.total_difficulty < min_difficulty) min_difficulty = diff.total_difficulty;
            if (diff.total_difficulty > max_difficulty) max_difficulty = diff.total_difficulty;

            /* Track tier */
            pr_memory_tier_t tier = infer_sample_tier(sample);
            if (tier < PR_CURRICULUM_NUM_TIERS) {
                result->tier_representation[tier]++;
            }

            selected++;
        }
    }

    /* Fill result */
    result->batch_size = selected;
    result->avg_difficulty = selected > 0 ? total_difficulty / (float)selected : 0.0f;
    result->min_difficulty = selected > 0 ? min_difficulty : 0.0f;
    result->max_difficulty = selected > 0 ? max_difficulty : 0.0f;
    result->exploration_ratio = selected > 0 ? (float)exploration_count / (float)selected : 0.0f;

    /* Update statistics */
    bridge->stats.batches_selected++;
    bridge->stats.samples_presented += selected;
    if (isfinite(result->avg_difficulty)) {
        bridge->stats.avg_difficulty = bridge->stats.avg_difficulty * 0.95f + result->avg_difficulty * 0.05f;
    }
    if (isfinite(result->exploration_ratio)) {
        bridge->stats.exploration_ratio = bridge->stats.exploration_ratio * 0.95f + result->exploration_ratio * 0.05f;
    }

    uint64_t end_time_us = nimcp_time_get_us();
    float elapsed_us = (float)(end_time_us - start_time_us);
    bridge->stats.avg_batch_select_time_us =
        bridge->stats.avg_batch_select_time_us * 0.95f + elapsed_us * 0.05f;
    bridge->stats.total_update_time_us += (uint64_t)elapsed_us;

    /* Log event */
    if (bridge->config.enable_event_logging) {
        pr_curriculum_event_t event = {
            .type = PR_CURRICULUM_EVENT_BATCH_SELECTED,
            .timestamp_ms = nimcp_time_get_ms(),
            .sample_id = 0,
            .difficulty = result->avg_difficulty,
            .pacing = bridge->current_pacing,
            .strategy = bridge->config.type
        };
        add_event(bridge, &event);
    }

    nimcp_free(scores);
    scores = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_curriculum_select_balanced_batch(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    uint32_t sample_count,
    uint32_t batch_size,
    const float tier_weights[PR_CURRICULUM_NUM_TIERS],
    pr_batch_result_t* result)
{
    if (!bridge || !samples || sample_count == 0 || !result || !result->sample_ids || !tier_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_select_balanced_batch: required parameter is NULL (bridge, samples, result, result->sample_ids, tier_weights)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_select", 0.0f);


    if (batch_size > sample_count) batch_size = sample_count;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Calculate target counts per tier */
    uint32_t tier_targets[PR_CURRICULUM_NUM_TIERS];
    float total_weight = 0.0f;
    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        total_weight += tier_weights[t];
    }

    uint32_t assigned = 0;
    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        tier_targets[t] = (uint32_t)((tier_weights[t] / (fabsf(total_weight) > 1e-7f ? total_weight : 1e-7f)) * batch_size);
        assigned += tier_targets[t];
    }
    /* Assign remainder to Z1 */
    tier_targets[1] += batch_size - assigned;

    /* Bucket samples by tier */
    uint32_t* tier_indices[PR_CURRICULUM_NUM_TIERS];
    uint32_t tier_counts[PR_CURRICULUM_NUM_TIERS] = {0};

    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        tier_indices[t] = nimcp_calloc(sample_count, sizeof(uint32_t));
        if (!tier_indices[t]) {
            for (int j = 0; j < t; j++) nimcp_free(tier_indices[j]);
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_select_balanced_batch: tier_indices is NULL");
            return -1;
        }
    }

    for (uint32_t i = 0; i < sample_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sample_count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculum_loop",
                             (float)(i + 1) / (float)sample_count);
        }

        pr_memory_tier_t tier = infer_sample_tier(&samples[i]);
        if (tier < PR_CURRICULUM_NUM_TIERS) {
            tier_indices[tier][tier_counts[tier]++] = i;
        }
    }

    /* Select from each tier */
    uint32_t selected = 0;
    float total_difficulty = 0.0f;
    memset(result->tier_representation, 0, sizeof(result->tier_representation));

    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS && selected < batch_size; t++) {
        uint32_t to_select = tier_targets[t];
        if (to_select > tier_counts[t]) to_select = tier_counts[t];

        for (uint32_t i = 0; i < to_select && selected < batch_size; i++) {
            uint32_t sample_idx = tier_indices[t][i];
            result->sample_ids[selected] = samples[sample_idx].sample_id;

            pr_difficulty_result_t diff;
            pr_curriculum_compute_difficulty(bridge, graph, &samples[sample_idx], &diff);
            total_difficulty += diff.total_difficulty;

            result->tier_representation[t]++;
            selected++;
        }
    }

    /* Fill any remaining slots */
    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS && selected < batch_size; t++) {
        for (uint32_t i = tier_targets[t]; i < tier_counts[t] && selected < batch_size; i++) {
            uint32_t sample_idx = tier_indices[t][i];
            result->sample_ids[selected] = samples[sample_idx].sample_id;

            pr_difficulty_result_t diff;
            pr_curriculum_compute_difficulty(bridge, graph, &samples[sample_idx], &diff);
            total_difficulty += diff.total_difficulty;

            result->tier_representation[t]++;
            selected++;
        }
    }

    result->batch_size = selected;
    result->avg_difficulty = selected > 0 ? total_difficulty / (float)selected : 0.0f;
    result->min_difficulty = 0.0f;
    result->max_difficulty = 1.0f;
    result->exploration_ratio = 0.0f;

    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        nimcp_free(tier_indices[t]);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Curiosity/Novelty Functions
//=============================================================================

float pr_curriculum_curiosity_score(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* sample)
{
    if (!bridge || !sample) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_curios", 0.0f);


    pr_difficulty_result_t result;
    if (pr_curriculum_compute_difficulty(bridge, graph, sample, &result) != 0) {
        return -1.0f;
    }

    return result.curiosity_score;
}

int pr_curriculum_select_most_curious(
    pr_curriculum_bridge_t bridge,
    entangle_graph_t graph,
    const pr_curriculum_sample_t* samples,
    uint32_t sample_count,
    uint32_t k,
    uint64_t* selected_ids,
    uint32_t* selected_count)
{
    if (!bridge || !samples || sample_count == 0 || !selected_ids || !selected_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_select_most_curious: required parameter is NULL (bridge, samples, selected_ids, selected_count)");
        return -1;
        BRIDGE_BBB_VALIDATE(bridge, selected_ids, sizeof(*selected_ids));
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_select", 0.0f);


    if (k > sample_count) k = sample_count;

    /* Compute curiosity scores and sort */
    sample_score_t* scores = nimcp_calloc(sample_count, sizeof(sample_score_t));
    if (!scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_curriculum_select_most_curious: scores is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < sample_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && sample_count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)sample_count);
        }

        pr_difficulty_result_t result;
        pr_curriculum_compute_difficulty(bridge, graph, &samples[i], &result);

        scores[i].sample_id = samples[i].sample_id;
        scores[i].original_index = i;
        scores[i].score = result.curiosity_score;
    }

    /* Sort by curiosity descending */
    qsort(scores, sample_count, sizeof(sample_score_t), compare_scores_desc);

    /* Select top k */
    for (uint32_t i = 0; i < k; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && k > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)k);
        }

        selected_ids[i] = scores[i].sample_id;
    }
    *selected_count = k;

    nimcp_free(scores);
    scores = NULL;
    return 0;
}

//=============================================================================
// Pacing Functions
//=============================================================================

int pr_curriculum_consolidation_pace(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder,
    pr_curriculum_pacing_t* pacing)
{
    if (!bridge || !pacing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_consolidation_pace: required parameter is NULL (bridge, pacing)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_consol", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now_ms = nimcp_time_get_ms();
    uint64_t window_start = now_ms - bridge->config.pacing_window_ms;

    /* Count recent promotions and demotions */
    uint32_t promotions = 0;
    uint32_t demotions = 0;

    for (uint32_t i = 0; i < bridge->history_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->history_count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)bridge->history_count);
        }

        consolidation_event_t* event = &bridge->consolidation_history[i];
        if (event->timestamp_ms >= window_start) {
            if (event->is_promotion) {
                promotions++;
            } else {
                demotions++;
            }
        }
    }

    /* Compute pacing */
    pacing->recent_promotions = promotions;
    pacing->recent_demotions = demotions;
    pacing->window_ms = bridge->config.pacing_window_ms;

    /* Consolidation rate = promotions / (promotions + demotions) */
    uint32_t total = promotions + demotions;
    if (total >= MIN_PACING_SAMPLES) {
        pacing->consolidation_rate = (float)promotions / (float)total;
        pacing->retention_rate = 1.0f - (float)demotions / (float)total;

        /* Compute pacing multiplier */
        if (pacing->consolidation_rate > bridge->config.consolidation_threshold) {
            /* Learning well - speed up */
            pacing->pacing_multiplier = bridge->config.fast_pace_multiplier;
        } else if (pacing->consolidation_rate < 1.0f / bridge->config.consolidation_threshold) {
            /* Struggling - slow down */
            pacing->pacing_multiplier = bridge->config.slow_pace_multiplier;
        } else {
            /* Normal pace */
            pacing->pacing_multiplier = 1.0f;
        }
    } else {
        /* Not enough data - use default */
        pacing->consolidation_rate = 0.5f;
        pacing->retention_rate = 0.9f;
        pacing->pacing_multiplier = 1.0f;
    }

    /* Get tier pressure from Z-ladder if available */
    if (ladder) {
        /* Would query Z-ladder for tier counts */
        pacing->z0_pressure = 0.3f;  /* Placeholder */
        pacing->z1_throughput = 0.5f;  /* Placeholder */
    } else {
        pacing->z0_pressure = 0.5f;
        pacing->z1_throughput = 0.5f;
    }

    /* Update cached pacing */
    bridge->current_pacing = pacing->pacing_multiplier;
    bridge->recent_promotions = promotions;
    bridge->recent_demotions = demotions;
    bridge->last_pacing_update_ms = now_ms;

    /* Log event */
    if (bridge->config.enable_event_logging) {
        pr_curriculum_event_t event = {
            .type = PR_CURRICULUM_EVENT_PACING_ADJUSTED,
            .timestamp_ms = now_ms,
            .sample_id = 0,
            .difficulty = pacing->consolidation_rate,
            .pacing = pacing->pacing_multiplier,
            .strategy = bridge->config.type
        };
        add_event(bridge, &event);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float pr_curriculum_get_epoch_pace(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder,
    uint32_t current_epoch)
{
    if (!bridge) return 1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_get_ep", 0.0f);


    pr_curriculum_pacing_t pacing;
    if (pr_curriculum_consolidation_pace(bridge, ladder, &pacing) != 0) {
        return 1.0f;
    }

    /* Adjust pacing based on epoch */
    /* Early epochs: slower, later epochs: faster if learning well */
    float epoch_factor = 1.0f;
    if (current_epoch < 5) {
        epoch_factor = 0.8f;  /* Start slower */
    } else if (current_epoch > 20) {
        epoch_factor = 1.2f;  /* Can push harder if still learning */
    }

    return pacing.pacing_multiplier * epoch_factor;
}

int pr_curriculum_record_consolidation(
    pr_curriculum_bridge_t bridge,
    bool is_promotion,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_record_consolidation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_record", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    add_consolidation_event(bridge, is_promotion, from_tier, to_tier);

    /* Update running counts */
    if (is_promotion) {
        bridge->recent_promotions++;
    } else {
        bridge->recent_demotions++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Tier Distribution Functions
//=============================================================================

int pr_curriculum_tier_distribution(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder,
    pr_tier_distribution_t* distribution)
{
    if (!bridge || !distribution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_tier_distribution: required parameter is NULL (bridge, distribution)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_tier_d", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check cache */
    uint64_t now_ms = nimcp_time_get_ms();
    if (now_ms - bridge->tier_cache_time_ms < 1000) {  /* 1 second cache */
        *distribution = bridge->tier_cache;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Get distribution from Z-ladder if available */
    memset(distribution, 0, sizeof(pr_tier_distribution_t));

    if (ladder) {
        /* Would query Z-ladder for actual distribution */
        /* For now, use placeholder values */
        distribution->counts[0] = 100;
        distribution->counts[1] = 200;
        distribution->counts[2] = 150;
        distribution->counts[3] = 50;
    } else {
        /* Default balanced distribution */
        for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
                pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                                 (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
            }

            distribution->counts[t] = 100;
        }
    }

    /* Compute totals and proportions */
    distribution->total_nodes = 0;
    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        distribution->total_nodes += distribution->counts[t];
    }

    if (distribution->total_nodes > 0) {
        for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
            /* Phase 8: Loop progress heartbeat */
            if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
                pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                                 (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
            }

            distribution->proportions[t] =
                (float)distribution->counts[t] / (float)distribution->total_nodes;
        }
    }

    /* Compute balance score (0 = all in one tier, 1 = perfectly balanced) */
    float ideal = 1.0f / PR_CURRICULUM_NUM_TIERS;
    float total_deviation = 0.0f;
    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        total_deviation += fabsf(distribution->proportions[t] - ideal);
    }
    distribution->balance_score = 1.0f - (total_deviation / 2.0f);

    /* Default average strengths */
    distribution->avg_strength[0] = 0.8f;
    distribution->avg_strength[1] = 0.6f;
    distribution->avg_strength[2] = 0.5f;
    distribution->avg_strength[3] = 0.9f;

    /* Cache result */
    bridge->tier_cache = *distribution;
    bridge->tier_cache_time_ms = now_ms;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

pr_curriculum_type_t pr_curriculum_recommend_strategy(
    pr_curriculum_bridge_t bridge,
    z_ladder_t ladder)
{
    if (!bridge) return PR_CURRICULUM_HYBRID;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_recomm", 0.0f);


    pr_tier_distribution_t dist;
    if (pr_curriculum_tier_distribution(bridge, ladder, &dist) != 0) {
        return PR_CURRICULUM_HYBRID;
    }

    /* Find dominant tier */
    int dominant_tier = 0;
    float max_proportion = 0.0f;
    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        if (dist.proportions[t] > max_proportion) {
            max_proportion = dist.proportions[t];
            dominant_tier = t;
        }
    }

    /* Recommend based on dominant tier */
    switch (dominant_tier) {
        case 0:  /* Z0 heavy - too much in working memory */
            return PR_CURRICULUM_RESONANCE;  /* Review and consolidate */
        case 1:  /* Z1 heavy - good short-term */
            return PR_CURRICULUM_HYBRID;  /* Balance */
        case 2:  /* Z2 heavy - strong consolidation */
            return PR_CURRICULUM_CURIOSITY;  /* Can challenge more */
        case 3:  /* Z3 heavy - rich knowledge base */
            return PR_CURRICULUM_CURIOSITY;  /* Focus on transfer */
        default:
            return PR_CURRICULUM_HYBRID;
    }
}

//=============================================================================
// Feedback Functions
//=============================================================================

int pr_curriculum_update_after_step(
    pr_curriculum_bridge_t bridge,
    const pr_step_result_t* results,
    uint32_t count)
{
    if (!bridge || !results || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_update_after_step: required parameter is NULL (bridge, results)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    float total_loss = 0.0f;
    uint32_t correct_count = 0;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(i + 1) / (float)count);
        }

        const pr_step_result_t* step = &results[i];

        total_loss += step->loss;
        if (step->correct) correct_count++;

        /* Update difficulty estimate from loss */
        /* Find cached entry and update */
        difficulty_cache_entry_t* cached = find_cache_entry(bridge, step->sample_id);
        if (cached && cached->valid) {
            /* Blend estimated difficulty with observed loss */
            float observed_difficulty = nimcp_myelin_clamp(step->loss, 0.0f, 1.0f);
            cached->result.total_difficulty =
                cached->result.total_difficulty * (1.0f - EMA_ALPHA) +
                observed_difficulty * EMA_ALPHA;
        }
    }

    /* Update curriculum efficiency metric */
    float accuracy = (float)correct_count / (float)count;
    bridge->stats.curriculum_efficiency =
        bridge->stats.curriculum_efficiency * 0.99f + accuracy * 0.01f;

    /* Update statistics */
    bridge->stats.avg_difficulty =
        bridge->stats.avg_difficulty * 0.95f + (total_loss / (float)count) * 0.05f;

    /* Adjust pacing based on performance */
    if (accuracy > 0.9f) {
        bridge->current_pacing = nimcp_myelin_clamp(bridge->current_pacing * 1.05f, 0.1f, 3.0f);
        bridge->stats.pace_increases++;
    } else if (accuracy < 0.6f) {
        bridge->current_pacing = nimcp_myelin_clamp(bridge->current_pacing * 0.95f, 0.1f, 3.0f);
        bridge->stats.pace_decreases++;
    }

    bridge->stats.avg_pacing =
        bridge->stats.avg_pacing * 0.99f + bridge->current_pacing * 0.01f;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float pr_curriculum_update_difficulty_from_loss(
    pr_curriculum_bridge_t bridge,
    uint64_t sample_id,
    float loss,
    bool was_correct)
{
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    difficulty_cache_entry_t* cached = find_cache_entry(bridge, sample_id);
    if (!cached || !cached->valid) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return -1.0f;
    }

    /* Compute observed difficulty from loss */
    float observed = nimcp_myelin_clamp(loss, 0.0f, 1.0f);
    if (was_correct) {
        observed *= 0.8f;  /* Discount if correct */
    }

    /* Blend with existing estimate */
    cached->result.total_difficulty =
        cached->result.total_difficulty * (1.0f - EMA_ALPHA) +
        observed * EMA_ALPHA;

    float new_difficulty = cached->result.total_difficulty;

    nimcp_mutex_unlock(bridge->base.mutex);

    return new_difficulty;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int pr_curriculum_get_stats(
    pr_curriculum_bridge_t bridge,
    pr_curriculum_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_get_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_curriculum_reset_stats(pr_curriculum_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(pr_curriculum_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_curriculum_get_events(
    pr_curriculum_bridge_t bridge,
    pr_curriculum_event_t* events,
    uint32_t max_events,
    uint32_t* event_count)
{
    if (!bridge || !events || !event_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_get_events: required parameter is NULL (bridge, events, event_count)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_get_ev", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, event_count, sizeof(*event_count));

    nimcp_mutex_lock(bridge->base.mutex);

    uint32_t to_copy = (max_events < bridge->event_count) ? max_events : bridge->event_count;

    if (to_copy > 0 && bridge->events) {
        uint32_t start_idx = 0;
        if (bridge->event_count < bridge->event_capacity) {
            start_idx = 0;
        } else {
            start_idx = bridge->event_write_idx;
        }

        for (uint32_t i = 0; i < to_copy; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && to_copy > 256) {
                pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                                 (float)(i + 1) / (float)to_copy);
            }

            uint32_t idx = (start_idx + i) % bridge->event_capacity;
            events[i] = bridge->events[idx];
        }
    }

    *event_count = to_copy;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Strategy Control Functions
//=============================================================================

int pr_curriculum_set_strategy(
    pr_curriculum_bridge_t bridge,
    pr_curriculum_type_t type)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_set_strategy: bridge is NULL");
        return -1;
    }
    if (type >= PR_CURRICULUM_TYPE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_curriculum_set_strategy: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_set_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    pr_curriculum_type_t old_type = bridge->config.type;
    bridge->config.type = type;

    /* Log strategy change */
    if (bridge->config.enable_event_logging && old_type != type) {
        pr_curriculum_event_t event = {
            .type = PR_CURRICULUM_EVENT_STRATEGY_CHANGED,
            .timestamp_ms = nimcp_time_get_ms(),
            .sample_id = 0,
            .difficulty = 0.0f,
            .pacing = bridge->current_pacing,
            .strategy = type
        };
        add_event(bridge, &event);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

pr_curriculum_type_t pr_curriculum_get_strategy(pr_curriculum_bridge_t bridge) {
    if (!bridge) return PR_CURRICULUM_HYBRID;
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_get_st", 0.0f);


    return bridge->config.type;
}

int pr_curriculum_set_exploration(
    pr_curriculum_bridge_t bridge,
    float epsilon)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_set_exploration: bridge is NULL");
        return -1;
    }
    if (epsilon < 0.0f || epsilon > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_curriculum_set_exploration: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_set_ex", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.exploration_epsilon = epsilon;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pr_curriculum_set_ascending_difficulty(
    pr_curriculum_bridge_t bridge,
    bool ascending)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_curriculum_set_ascending_difficulty: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_set_as", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config.ascending_difficulty = ascending;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_curriculum_type_name(pr_curriculum_type_t type) {
    switch (type) {
        case PR_CURRICULUM_RESONANCE:     return "Resonance";
        case PR_CURRICULUM_ENTANGLEMENT:  return "Entanglement";
        case PR_CURRICULUM_CONSOLIDATION: return "Consolidation";
        case PR_CURRICULUM_CURIOSITY:     return "Curiosity";
        case PR_CURRICULUM_HYBRID:        return "Hybrid";
        default:                          return "Unknown";
    }
}

const char* pr_difficulty_method_name(pr_difficulty_method_t method) {
    switch (method) {
        case PR_DIFFICULTY_RESONANCE:     return "Resonance";
        case PR_DIFFICULTY_ENTANGLEMENT:  return "Entanglement";
        case PR_DIFFICULTY_QUATERNION:    return "Quaternion";
        case PR_DIFFICULTY_COMPOSITE:     return "Composite";
        case PR_DIFFICULTY_EXTERNAL:      return "External";
        default:                          return "Unknown";
    }
}

const char* pr_curriculum_event_name(pr_curriculum_event_type_t type) {
    switch (type) {
        case PR_CURRICULUM_EVENT_BATCH_SELECTED:       return "BatchSelected";
        case PR_CURRICULUM_EVENT_SAMPLE_PRESENTED:     return "SamplePresented";
        case PR_CURRICULUM_EVENT_DIFFICULTY_COMPUTED:  return "DifficultyComputed";
        case PR_CURRICULUM_EVENT_PACING_ADJUSTED:      return "PacingAdjusted";
        case PR_CURRICULUM_EVENT_STRATEGY_CHANGED:     return "StrategyChanged";
        default:                                       return "Unknown";
    }
}

void pr_curriculum_print_difficulty(const pr_difficulty_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_print_", 0.0f);


    printf("Difficulty Result:\n");
    printf("  Sample ID:    %lu\n", (unsigned long)result->sample_id);
    printf("  Total:        %.4f\n", result->total_difficulty);
    printf("  Resonance:    %.4f\n", result->resonance_difficulty);
    printf("  Entanglement: %.4f\n", result->entanglement_difficulty);
    printf("  Quaternion:   %.4f\n", result->quaternion_difficulty);
    printf("  Curiosity:    %.4f\n", result->curiosity_score);
    printf("  Confidence:   %.4f\n", result->confidence);
    printf("  Res. Count:   %u\n", result->resonance_count);
}

void pr_curriculum_print_batch(const pr_batch_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_print_", 0.0f);


    printf("Batch Result:\n");
    printf("  Batch Size:    %u\n", result->batch_size);
    printf("  Avg Difficulty: %.4f\n", result->avg_difficulty);
    printf("  Min Difficulty: %.4f\n", result->min_difficulty);
    printf("  Max Difficulty: %.4f\n", result->max_difficulty);
    printf("  Exploration:   %.4f\n", result->exploration_ratio);
    printf("  Tier Representation:\n");
    for (int t = 0; t < PR_CURRICULUM_NUM_TIERS; t++) {
        /* Phase 8: Loop progress heartbeat */
        if ((t & 0xFF) == 0 && PR_CURRICULUM_NUM_TIERS > 256) {
            pr_curriculum_bridge_heartbeat("pr_curriculu_loop",
                             (float)(t + 1) / (float)PR_CURRICULUM_NUM_TIERS);
        }

        printf("    Z%d: %u\n", t, result->tier_representation[t]);
    }
}

void pr_curriculum_print_pacing(const pr_curriculum_pacing_t* pacing) {
    if (!pacing) return;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_print_", 0.0f);


    printf("Curriculum Pacing:\n");
    printf("  Consolidation Rate: %.4f\n", pacing->consolidation_rate);
    printf("  Retention Rate:     %.4f\n", pacing->retention_rate);
    printf("  Pacing Multiplier:  %.4f\n", pacing->pacing_multiplier);
    printf("  Recent Promotions:  %u\n", pacing->recent_promotions);
    printf("  Recent Demotions:   %u\n", pacing->recent_demotions);
    printf("  Z0 Pressure:        %.4f\n", pacing->z0_pressure);
    printf("  Z1 Throughput:      %.4f\n", pacing->z1_throughput);
    printf("  Window:             %lu ms\n", (unsigned long)pacing->window_ms);
}

void pr_curriculum_print_stats(pr_curriculum_bridge_t bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_print_", 0.0f);


    pr_curriculum_stats_t stats;
    if (pr_curriculum_get_stats(bridge, &stats) != 0) return;

    printf("=== Prime Resonant Curriculum Bridge Statistics ===\n");
    printf("\nSample Statistics:\n");
    printf("  Samples Presented:    %lu\n", (unsigned long)stats.samples_presented);
    printf("  Batches Selected:     %lu\n", (unsigned long)stats.batches_selected);
    printf("  Difficulty Comps:     %lu\n", (unsigned long)stats.difficulty_computations);
    printf("  Cache Hits:           %lu\n", (unsigned long)stats.cache_hits);
    printf("  Cache Misses:         %lu\n", (unsigned long)stats.cache_misses);

    printf("\nDifficulty Statistics:\n");
    printf("  Average Difficulty:   %.4f\n", stats.avg_difficulty);
    printf("  Min Difficulty:       %.4f\n", stats.min_difficulty);
    printf("  Max Difficulty:       %.4f\n", stats.max_difficulty);
    printf("  Difficulty Variance:  %.4f\n", stats.difficulty_variance);

    printf("\nPacing Statistics:\n");
    printf("  Average Pacing:       %.4f\n", stats.avg_pacing);
    printf("  Pace Increases:       %u\n", stats.pace_increases);
    printf("  Pace Decreases:       %u\n", stats.pace_decreases);

    printf("\nEffectiveness:\n");
    printf("  Curriculum Efficiency: %.4f\n", stats.curriculum_efficiency);
    printf("  Exploration Ratio:    %.4f\n", stats.exploration_ratio);
    printf("  Tier Balance:         %.4f\n", stats.tier_balance_achieved);

    printf("\nPerformance:\n");
    printf("  Avg Batch Select:     %.2f us\n", stats.avg_batch_select_time_us);
    printf("  Avg Difficulty Comp:  %.2f us\n", stats.avg_difficulty_time_us);
    printf("  Total Update Time:    %lu us\n", (unsigned long)stats.total_update_time_us);
    printf("==================================================\n");
}

void pr_curriculum_sample_init(pr_curriculum_sample_t* sample) {
    if (!sample) return;

    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_sample", 0.0f);


    memset(sample, 0, sizeof(pr_curriculum_sample_t));

    /* Set default quaternion (identity-like) */
    sample->quaternion.w = 0.5f;
    sample->quaternion.x = 0.0f;
    sample->quaternion.y = 0.5f;
    sample->quaternion.z = 0.5f;

    sample->external_difficulty = 0.5f;
}

uint64_t pr_curriculum_current_time_ms(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_curriculum_bridge_heartbeat("pr_curriculu_pr_curriculum_curren", 0.0f);


    return nimcp_time_get_ms();
}

/* NOTE: pr_memory_tier_t is defined in nimcp_pr_memory_node.h (included via header chain) */

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_curriculum_bridge_set_instance_health_agent(
    pr_curriculum_bridge_t bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_curriculum_bridge_training_begin(pr_curriculum_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_curriculum_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_curriculum_bridge_heartbeat_instance(bridge->health_agent, "pr_curriculum_bridge_training_begin", 0.0f);
    return 0;
}

int pr_curriculum_bridge_training_end(pr_curriculum_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_curriculum_bridge_training_end: NULL argument");
        return -1;
    }
    pr_curriculum_bridge_heartbeat_instance(bridge->health_agent, "pr_curriculum_bridge_training_end", 1.0f);
    return 0;
}

int pr_curriculum_bridge_training_step(pr_curriculum_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_curriculum_bridge_training_step: NULL argument");
        return -1;
    }
    pr_curriculum_bridge_heartbeat_instance(bridge->health_agent, "pr_curriculum_bridge_training_step", progress);
    return 0;
}

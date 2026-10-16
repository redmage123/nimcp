/**
 * @file nimcp_emotional_contagion.c
 * @brief Implementation of emotional contagion protocol for swarm agents
 *
 * This module implements biologically-inspired emotional contagion, where
 * emotions spread between agents based on susceptibility, resistance, and
 * network topology. Supports collective mood tracking and bio-async integration.
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "swarm/nimcp_emotional_contagion.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/statistics/nimcp_statistics.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotional_contagion)

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define CONTAGION_MODULE "EmotionalContagion"
#define HASH_TABLE_SIZE 1024
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL
#define MAX_BFS_QUEUE 10000

/* Bio-async message types */
#define BIOMSG_EMOTION_SPREAD 0x7000
#define BIOMSG_COLLECTIVE_EMOTION 0x7001
#define BIOMSG_EMOTION_TRIGGER 0x7002

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Agent hash table entry
 */
typedef struct agent_entry_t {
    agent_emotional_state_t state;
    emotional_connection_t** connections;  /* Array of outgoing connections */
    uint32_t connection_count;
    uint32_t connection_capacity;
    struct agent_entry_t* next;           /* Hash table chaining */
} agent_entry_t;

/**
 * @brief Emotional contagion system state
 */
struct emotional_contagion {
    emotional_contagion_config_t config;

    /* Agent hash table */
    agent_entry_t** agent_hash;
    size_t hash_size;
    uint32_t agent_count;

    /* Collective state */
    collective_emotion_state_t collective;
    bool collective_dirty;  /* Needs recomputation */

    /* Statistics */
    emotional_contagion_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bio_router_t* bio_router;
    bool bio_async_registered;
    uint64_t last_broadcast_ms;

    /* Synchronization */
    nimcp_platform_mutex_t mutex;
};

/* ============================================================================
 * Emotion Name Mappings
 * ============================================================================ */

static const char* emotion_names[EMOTION_TYPE_COUNT] = {
    "neutral", "joy", "sadness", "anger", "fear", "surprise",
    "disgust", "trust", "anticipation", "curiosity", "calm",
    "excitement", "frustration", "hope", "despair", "pride",
    "shame", "guilt", "envy", "gratitude"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Calculate hash for agent ID
 * WHY:  Fast agent lookup
 * HOW:  Simple modulo hash
 */
static size_t agent_hash_function(uint32_t agent_id, size_t hash_size) {
    return (size_t)agent_id % hash_size;
}

/**
 * WHAT: Find agent in hash table
 * WHY:  Access agent state
 * HOW:  Hash lookup with chaining
 */
static agent_entry_t* find_agent(
    const emotional_contagion_t* ec,
    uint32_t agent_id) {

    if (!ec || !ec->agent_hash) {
        return NULL;
    }

    size_t hash = agent_hash_function(agent_id, ec->hash_size);
    agent_entry_t* entry = ec->agent_hash[hash];

    while (entry) {
        if (entry->state.agent_id == agent_id) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * WHAT: Calculate transmission intensity
 * WHY:  Core contagion algorithm
 * HOW:  Multiply source intensity by factors
 */
static float calculate_transmission_intensity(
    const emotional_contagion_t* ec,
    float source_intensity,
    float susceptibility,
    float resistance,
    float connection_strength,
    float proximity) {

    if (!ec) return 0.0F;

    /* Base transmission */
    float intensity = source_intensity * ec->config.contagion_rate;

    /* Apply susceptibility */
    intensity *= susceptibility;

    /* Apply resistance (inverted) */
    if (ec->config.enable_resistance) {
        intensity *= (1.0F - resistance);
    }

    /* Apply connection strength */
    intensity *= connection_strength;

    /* Apply proximity decay */
    intensity *= powf(proximity, ec->config.proximity_decay);

    return intensity;
}

/**
 * WHAT: Apply exponential decay to emotion
 * WHY:  Emotions fade over time
 * HOW:  Exponential decay formula
 */
static float apply_decay(float intensity, float decay_rate, uint64_t delta_ms) {
    if (intensity <= EPSILON) return 0.0F;

    float decay_factor = expf(-decay_rate * (delta_ms / 1000.0F));
    return intensity * decay_factor;
}

/**
 * WHAT: Update resistance for emotion type
 * WHY:  Build immunity to repeated exposure
 * HOW:  Increase resistance when exposed
 */
static void update_resistance(
    agent_emotional_state_t* state,
    emotion_type_t emotion,
    float buildup_rate,
    float max_resistance) {

    if (!state || emotion >= EMOTION_TYPE_COUNT) return;

    /* Increase resistance to this emotion */
    state->resistance[emotion] += buildup_rate;

    /* Cap at maximum */
    if (state->resistance[emotion] > max_resistance) {
        state->resistance[emotion] = max_resistance;
    }
}

/**
 * WHAT: Decay all resistances
 * WHY:  Resistance fades over time
 * HOW:  Apply decay to all emotion types
 */
static void decay_resistances(
    agent_emotional_state_t* state,
    float decay_rate,
    uint64_t delta_ms) {

    if (!state) return;

    float decay_factor = expf(-decay_rate * (delta_ms / 1000.0F));

    for (size_t i = 0; i < EMOTION_TYPE_COUNT; i++) {
        state->resistance[i] *= decay_factor;
        if (state->resistance[i] < EPSILON) {
            state->resistance[i] = 0.0F;
        }
    }
}

/**
 * WHAT: Recompute collective emotional state
 * WHY:  Track swarm-wide emotion
 * HOW:  Aggregate all agent emotions
 */
static void recompute_collective_state(emotional_contagion_t* ec) {
    if (!ec) return;

    /* Reset collective state */
    memset(&ec->collective, 0, sizeof(collective_emotion_state_t));

    uint32_t emotion_counts[EMOTION_TYPE_COUNT] = {0};
    float total_intensity = 0.0F;
    uint32_t active_agents = 0;

    /* Count emotions across all agents */
    for (size_t h = 0; h < ec->hash_size; h++) {
        agent_entry_t* entry = ec->agent_hash[h];
        while (entry) {
            emotion_type_t emotion = entry->state.emotion;
            float intensity = entry->state.intensity;

            if (emotion < EMOTION_TYPE_COUNT) {
                emotion_counts[emotion]++;
                total_intensity += intensity;

                if (emotion != EMOTION_NEUTRAL && intensity > EPSILON) {
                    active_agents++;
                }
            }

            entry = entry->next;
        }
    }

    /* Copy counts */
    memcpy(ec->collective.emotion_counts, emotion_counts,
           sizeof(emotion_counts));

    /* Find dominant emotion */
    uint32_t max_count = 0;
    emotion_type_t dominant = EMOTION_NEUTRAL;

    for (size_t i = 0; i < EMOTION_TYPE_COUNT; i++) {
        if (emotion_counts[i] > max_count) {
            max_count = emotion_counts[i];
            dominant = (emotion_type_t)i;
        }
    }

    ec->collective.dominant_emotion = dominant;

    /* Calculate average intensity of dominant */
    if (max_count > 0) {
        float dominant_total = 0.0F;
        uint32_t dominant_count = 0;

        for (size_t h = 0; h < ec->hash_size; h++) {
            agent_entry_t* entry = ec->agent_hash[h];
            while (entry) {
                if (entry->state.emotion == dominant) {
                    dominant_total += entry->state.intensity;
                    dominant_count++;
                }
                entry = entry->next;
            }
        }

        ec->collective.dominant_intensity = dominant_total / dominant_count;
    }

    /* Calculate emotional diversity (Shannon entropy) using central stats module */
    if (ec->agent_count > 0) {
        /* Build probability distribution from emotion counts */
        float probs[EMOTION_TYPE_COUNT];
        for (size_t i = 0; i < EMOTION_TYPE_COUNT; i++) {
            probs[i] = (float)emotion_counts[i] / (float)ec->agent_count;
        }

        /* Use central statistics module for Shannon entropy */
        float entropy = nimcp_stats_entropy(probs, EMOTION_TYPE_COUNT);

        /* Normalize by maximum entropy */
        float max_entropy = log2f((float)EMOTION_TYPE_COUNT);
        ec->collective.emotional_diversity = (max_entropy > EPSILON) ?
            (entropy / max_entropy) : 0.0F;
    }

    /* Calculate coherence (simplified - fraction with dominant emotion) */
    if (ec->agent_count > 0) {
        ec->collective.emotional_coherence =
            (float)max_count / (float)ec->agent_count;
    }

    /* Average intensity */
    if (ec->agent_count > 0) {
        ec->collective.avg_intensity = total_intensity / (float)ec->agent_count;
    }

    ec->stats.active_agents = active_agents;
    ec->collective_dirty = false;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void emotional_contagion_get_default_config(emotional_contagion_config_t* out_config) {
    if (!out_config) return;

    out_config->contagion_rate = 0.3F;
    out_config->decay_rate = 0.1F;
    out_config->susceptibility_threshold = 0.2F;
    out_config->max_propagation_depth = 3;
    out_config->enable_resistance = true;

    out_config->max_agents = 1000;
    out_config->max_connections_per_agent = 10;
    out_config->proximity_decay = 0.5F;

    out_config->enable_emotional_dampening = true;
    out_config->dampening_threshold = 0.95F;
    out_config->enable_refractory_period = true;
    out_config->refractory_duration_ms = 1000;

    out_config->resistance_buildup_rate = 0.01F;
    out_config->resistance_decay_rate = 0.05F;
    out_config->max_resistance = 0.9F;

    out_config->enable_bio_async = false;
    out_config->broadcast_interval_ms = 1000;
}

nimcp_result_t emotional_contagion_validate_config(
    const emotional_contagion_config_t* config) {

    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate rates */
    NIMCP_CHECK_THROW(config->contagion_rate >= 0.0F && config->contagion_rate <= 1.0F,
                      NIMCP_ERROR_INVALID_PARAM, "contagion_rate must be in range [0.0, 1.0]");

    NIMCP_CHECK_THROW(config->decay_rate >= 0.0F && config->decay_rate <= 1.0F,
                      NIMCP_ERROR_INVALID_PARAM, "decay_rate must be in range [0.0, 1.0]");

    NIMCP_CHECK_THROW(config->susceptibility_threshold >= 0.0F && config->susceptibility_threshold <= 1.0F,
                      NIMCP_ERROR_INVALID_PARAM, "susceptibility_threshold must be in range [0.0, 1.0]");

    NIMCP_CHECK_THROW(config->max_agents > 0 && config->max_agents <= EMOTIONAL_CONTAGION_MAX_AGENTS,
                      NIMCP_ERROR_INVALID_PARAM, "max_agents must be in range (0, EMOTIONAL_CONTAGION_MAX_AGENTS]");

    return NIMCP_SUCCESS;
}

emotional_contagion_t* emotional_contagion_create(
    const emotional_contagion_config_t* config) {

    /* Validate configuration */
    if (emotional_contagion_validate_config(config) != NIMCP_SUCCESS) {
        return NULL;
    }

    /* Allocate system */
    emotional_contagion_t* ec =
        (emotional_contagion_t*)nimcp_malloc(sizeof(emotional_contagion_t));
    if (!ec) {
        LOG_ERROR(CONTAGION_MODULE, "Failed to allocate contagion system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotional_contagion_create: alloc failed");

        return NULL;
    }

    memset(ec, 0, sizeof(emotional_contagion_t));

    /* Copy configuration */
    ec->config = *config;

    /* Initialize agent hash table */
    ec->hash_size = HASH_TABLE_SIZE;
    ec->agent_hash = (agent_entry_t**)nimcp_malloc(
        ec->hash_size * sizeof(agent_entry_t*));

    if (!ec->agent_hash) {
        LOG_ERROR(CONTAGION_MODULE, "Failed to allocate agent hash table");
        nimcp_free(ec);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotional_contagion_get_default_config: ec->agent_hash is NULL");
        return NULL;
    }

    memset(ec->agent_hash, 0, ec->hash_size * sizeof(agent_entry_t*));

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&ec->mutex, false) != 0) {
        LOG_ERROR(CONTAGION_MODULE, "Failed to initialize mutex");
        nimcp_free(ec->agent_hash);
        nimcp_free(ec);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotional_contagion_create: mutex init failed");
        return NULL;
    }

    /* Initialize collective state */
    ec->collective_dirty = true;

    LOG_INFO(CONTAGION_MODULE,
             "Created emotional contagion system: max_agents=%u, contagion_rate=%.2f",
             config->max_agents, config->contagion_rate);

    return ec;
}

void emotional_contagion_destroy(emotional_contagion_t* ec) {
    if (!ec) return;

    LOG_DEBUG(CONTAGION_MODULE, "Destroying emotional contagion system");

    /* Free all agents and connections */
    if (ec->agent_hash) {
        for (size_t h = 0; h < ec->hash_size; h++) {
            agent_entry_t* entry = ec->agent_hash[h];
            while (entry) {
                agent_entry_t* next = entry->next;

                /* Free connections */
                if (entry->connections) {
                    for (uint32_t i = 0; i < entry->connection_count; i++) {
                        if (entry->connections[i]) {
                            nimcp_free(entry->connections[i]);
                        }
                    }
                    nimcp_free(entry->connections);
                }

                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(ec->agent_hash);
    }

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(&ec->mutex);

    /* Free system */
    nimcp_free(ec);
}

nimcp_result_t emotional_contagion_reset(
    emotional_contagion_t* ec,
    bool clear_connections) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    /* Reset all agent emotions to neutral */
    for (size_t h = 0; h < ec->hash_size; h++) {
        agent_entry_t* entry = ec->agent_hash[h];
        while (entry) {
            entry->state.emotion = EMOTION_NEUTRAL;
            entry->state.intensity = 0.0F;
            entry->state.in_refractory = false;
            memset(entry->state.resistance, 0, sizeof(entry->state.resistance));

            /* Clear connections if requested */
            if (clear_connections && entry->connections) {
                for (uint32_t i = 0; i < entry->connection_count; i++) {
                    if (entry->connections[i]) {
                        nimcp_free(entry->connections[i]);
                    }
                }
                nimcp_free(entry->connections);
                entry->connections = NULL;
                entry->connection_count = 0;
                entry->connection_capacity = 0;
            }

            entry = entry->next;
        }
    }

    /* Reset statistics */
    memset(&ec->stats, 0, sizeof(emotional_contagion_stats_t));

    /* Mark collective as dirty */
    ec->collective_dirty = true;

    nimcp_platform_mutex_unlock(&ec->mutex);

    LOG_INFO(CONTAGION_MODULE, "Reset emotional contagion system");

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Agent Management Functions
 * ============================================================================ */

nimcp_result_t emotional_contagion_register_agent(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    float susceptibility) {

    /* Validate parameters */
    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(susceptibility >= 0.0F && susceptibility <= 1.0F,
                      NIMCP_ERROR_INVALID_PARAM, "susceptibility must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(&ec->mutex);

    /* Check if agent already exists */
    if (find_agent(ec, agent_id)) {
        LOG_WARN(CONTAGION_MODULE, "Agent %u already registered", agent_id);
        nimcp_platform_mutex_unlock(&ec->mutex);
        return NIMCP_ALREADY_EXISTS;
    }

    /* Check agent limit */
    if (ec->agent_count >= ec->config.max_agents) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_QUEUE_FULL, "agent limit reached");
    }

    /* Allocate agent entry */
    agent_entry_t* entry = (agent_entry_t*)nimcp_malloc(sizeof(agent_entry_t));
    if (!entry) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "failed to allocate agent entry");
    }

    memset(entry, 0, sizeof(agent_entry_t));

    /* Initialize agent state */
    entry->state.agent_id = agent_id;
    entry->state.emotion = EMOTION_NEUTRAL;
    entry->state.intensity = 0.0F;
    entry->state.susceptibility = susceptibility;
    entry->state.emotional_inertia = 0.0F;
    entry->state.last_update_ms = nimcp_time_get_us() / 1000;
    entry->state.in_refractory = false;

    /* Add to hash table */
    size_t hash = agent_hash_function(agent_id, ec->hash_size);
    entry->next = ec->agent_hash[hash];
    ec->agent_hash[hash] = entry;

    ec->agent_count++;
    ec->collective_dirty = true;

    nimcp_platform_mutex_unlock(&ec->mutex);

    LOG_DEBUG(CONTAGION_MODULE, "Registered agent %u with susceptibility %.2f",
              agent_id, susceptibility);

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_unregister_agent(
    emotional_contagion_t* ec,
    uint32_t agent_id) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    size_t hash = agent_hash_function(agent_id, ec->hash_size);
    agent_entry_t** entry_ptr = &ec->agent_hash[hash];

    while (*entry_ptr) {
        agent_entry_t* entry = *entry_ptr;
        if (entry->state.agent_id == agent_id) {
            /* Remove from hash chain */
            *entry_ptr = entry->next;

            /* Free connections */
            if (entry->connections) {
                for (uint32_t i = 0; i < entry->connection_count; i++) {
                    if (entry->connections[i]) {
                        nimcp_free(entry->connections[i]);
                    }
                }
                nimcp_free(entry->connections);
            }

            nimcp_free(entry);
            ec->agent_count--;
            ec->collective_dirty = true;

            nimcp_platform_mutex_unlock(&ec->mutex);

            LOG_DEBUG(CONTAGION_MODULE, "Unregistered agent %u", agent_id);
            return NIMCP_SUCCESS;
        }
        entry_ptr = &entry->next;
    }

    nimcp_platform_mutex_unlock(&ec->mutex);

    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "agent not found");
}

nimcp_result_t emotional_contagion_set_emotion(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    emotion_type_t emotion,
    float intensity) {

    /* Validate parameters */
    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(emotion < EMOTION_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid emotion type");
    NIMCP_CHECK_THROW(intensity >= 0.0F && intensity <= 1.0F, NIMCP_ERROR_INVALID_PARAM, "intensity must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(&ec->mutex);

    agent_entry_t* entry = find_agent(ec, agent_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "agent not found");
    }

    /* Apply dampening if enabled */
    if (ec->config.enable_emotional_dampening &&
        intensity > ec->config.dampening_threshold) {
        intensity = ec->config.dampening_threshold;
    }

    /* Update emotion */
    emotion_type_t old_emotion = entry->state.emotion;
    entry->state.emotion = emotion;
    entry->state.intensity = intensity;
    entry->state.last_update_ms = nimcp_time_get_us() / 1000;

    /* Set refractory period if enabled and intensity is high */
    if (ec->config.enable_refractory_period && intensity > 0.8F) {
        entry->state.in_refractory = true;
        entry->state.refractory_until_ms =
            entry->state.last_update_ms + ec->config.refractory_duration_ms;
    }

    ec->collective_dirty = true;

    nimcp_platform_mutex_unlock(&ec->mutex);

    LOG_DEBUG(CONTAGION_MODULE,
              "Set agent %u emotion: %s -> %s (intensity: %.2f)",
              agent_id,
              emotion_names[old_emotion],
              emotion_names[emotion],
              intensity);

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_get_emotional_state(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    agent_emotional_state_t* state) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state output pointer is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    agent_entry_t* entry = find_agent(ec, agent_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "agent not found");
    }

    *state = entry->state;

    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_set_susceptibility(
    emotional_contagion_t* ec,
    uint32_t agent_id,
    float susceptibility) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(susceptibility >= 0.0F && susceptibility <= 1.0F, NIMCP_ERROR_INVALID_PARAM, "susceptibility must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(&ec->mutex);

    agent_entry_t* entry = find_agent(ec, agent_id);
    if (!entry) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "agent not found");
    }

    entry->state.susceptibility = susceptibility;

    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection Management Functions
 * ============================================================================ */

nimcp_result_t emotional_contagion_add_connection(
    emotional_contagion_t* ec,
    uint32_t from_agent,
    uint32_t to_agent,
    float connection_strength,
    float proximity) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(connection_strength >= 0.0F && connection_strength <= 1.0F, NIMCP_ERROR_INVALID_PARAM, "connection_strength must be in range [0.0, 1.0]");
    NIMCP_CHECK_THROW(proximity >= 0.0F && proximity <= 1.0F, NIMCP_ERROR_INVALID_PARAM, "proximity must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(&ec->mutex);

    agent_entry_t* from_entry = find_agent(ec, from_agent);
    if (!from_entry) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "source agent not found");
    }

    /* Verify target agent exists */
    if (!find_agent(ec, to_agent)) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "target agent not found");
    }

    /* Check connection limit */
    if (from_entry->connection_count >= ec->config.max_connections_per_agent) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_QUEUE_FULL, "connection limit reached for agent");
    }

    /* Grow connection array if needed */
    if (from_entry->connection_count >= from_entry->connection_capacity) {
        uint32_t new_capacity = from_entry->connection_capacity == 0 ?
            4 : from_entry->connection_capacity * 2;

        emotional_connection_t** new_connections =
            (emotional_connection_t**)nimcp_realloc(
                from_entry->connections,
                new_capacity * sizeof(emotional_connection_t*));

        if (!new_connections) {
            nimcp_platform_mutex_unlock(&ec->mutex);
            NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "failed to grow connection array");
        }

        from_entry->connections = new_connections;
        from_entry->connection_capacity = new_capacity;
    }

    /* Create connection */
    emotional_connection_t* conn =
        (emotional_connection_t*)nimcp_malloc(sizeof(emotional_connection_t));
    if (!conn) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "failed to allocate connection");
    }

    conn->from_agent = from_agent;
    conn->to_agent = to_agent;
    conn->connection_strength = connection_strength;
    conn->proximity = proximity;
    conn->interaction_count = 0;
    conn->last_interaction_ms = 0;

    from_entry->connections[from_entry->connection_count++] = conn;

    nimcp_platform_mutex_unlock(&ec->mutex);

    LOG_DEBUG(CONTAGION_MODULE,
              "Added connection: %u -> %u (strength: %.2f, proximity: %.2f)",
              from_agent, to_agent, connection_strength, proximity);

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_remove_connection(
    emotional_contagion_t* ec,
    uint32_t from_agent,
    uint32_t to_agent) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    agent_entry_t* from_entry = find_agent(ec, from_agent);
    if (!from_entry || !from_entry->connections) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "agent or connections not found");
    }

    /* Find and remove connection */
    for (uint32_t i = 0; i < from_entry->connection_count; i++) {
        emotional_connection_t* conn = from_entry->connections[i];
        if (conn && conn->to_agent == to_agent) {
            nimcp_free(conn);

            /* Shift remaining connections */
            for (uint32_t j = i; j < from_entry->connection_count - 1; j++) {
                from_entry->connections[j] = from_entry->connections[j + 1];
            }

            from_entry->connection_count--;

            nimcp_platform_mutex_unlock(&ec->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(&ec->mutex);
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "connection not found");
}

nimcp_result_t emotional_contagion_update_connection(
    emotional_contagion_t* ec,
    uint32_t from_agent,
    uint32_t to_agent,
    float new_strength,
    float new_proximity) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(new_strength >= 0.0F && new_strength <= 1.0F, NIMCP_ERROR_INVALID_PARAM, "new_strength must be in range [0.0, 1.0]");
    NIMCP_CHECK_THROW(new_proximity >= 0.0F && new_proximity <= 1.0F, NIMCP_ERROR_INVALID_PARAM, "new_proximity must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(&ec->mutex);

    agent_entry_t* from_entry = find_agent(ec, from_agent);
    if (!from_entry || !from_entry->connections) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "agent or connections not found");
    }

    /* Find connection */
    for (uint32_t i = 0; i < from_entry->connection_count; i++) {
        emotional_connection_t* conn = from_entry->connections[i];
        if (conn && conn->to_agent == to_agent) {
            conn->connection_strength = new_strength;
            conn->proximity = new_proximity;

            nimcp_platform_mutex_unlock(&ec->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(&ec->mutex);
    NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "connection not found");
}

/* ============================================================================
 * Propagation Functions
 * ============================================================================ */

static void apply_decay_unlocked(emotional_contagion_t* ec, uint64_t delta_ms);

nimcp_result_t emotional_contagion_propagate(
    emotional_contagion_t* ec,
    uint64_t delta_ms) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    uint64_t current_time = nimcp_time_get_us() / 1000;

    /* Phase 1: Propagate emotions through connections */
    for (size_t h = 0; h < ec->hash_size; h++) {
        agent_entry_t* source_entry = ec->agent_hash[h];

        while (source_entry) {
            /* Skip neutral or low-intensity emotions */
            if (source_entry->state.emotion == EMOTION_NEUTRAL ||
                source_entry->state.intensity < EPSILON) {
                source_entry = source_entry->next;
                continue;
            }

            /* Propagate to all connected agents */
            for (uint32_t i = 0; i < source_entry->connection_count; i++) {
                emotional_connection_t* conn = source_entry->connections[i];
                if (!conn) continue;

                agent_entry_t* target_entry = find_agent(ec, conn->to_agent);
                if (!target_entry) continue;

                /* Check refractory period */
                if (target_entry->state.in_refractory) {
                    if (current_time >= target_entry->state.refractory_until_ms) {
                        target_entry->state.in_refractory = false;
                    } else {
                        ec->stats.refractory_blocks++;
                        continue;
                    }
                }

                /* Check susceptibility threshold */
                if (target_entry->state.susceptibility < ec->config.susceptibility_threshold) {
                    ec->stats.blocked_by_susceptibility++;
                    continue;
                }

                /* Get resistance for this emotion type */
                float resistance = 0.0F;
                if (ec->config.enable_resistance &&
                    source_entry->state.emotion < EMOTION_TYPE_COUNT) {
                    resistance = target_entry->state.resistance[source_entry->state.emotion];
                }

                /* Check if resistance blocks transmission */
                if (resistance > 0.9F) {
                    ec->stats.blocked_by_resistance++;
                    continue;
                }

                /* Calculate transmitted intensity */
                float transmitted = calculate_transmission_intensity(
                    ec,
                    source_entry->state.intensity,
                    target_entry->state.susceptibility,
                    resistance,
                    conn->connection_strength,
                    conn->proximity);

                /* Apply transmission if above threshold */
                if (transmitted > EPSILON) {
                    /* Blend with current emotion or replace */
                    if (target_entry->state.emotion == EMOTION_NEUTRAL ||
                        target_entry->state.intensity < transmitted) {
                        target_entry->state.emotion = source_entry->state.emotion;
                        target_entry->state.intensity = transmitted;
                        target_entry->state.last_update_ms = current_time;

                        ec->stats.successful_transmissions++;

                        /* Update resistance */
                        if (ec->config.enable_resistance) {
                            update_resistance(
                                &target_entry->state,
                                source_entry->state.emotion,
                                ec->config.resistance_buildup_rate,
                                ec->config.max_resistance);
                        }
                    }
                }

                conn->interaction_count++;
                conn->last_interaction_ms = current_time;
                ec->stats.total_propagations++;
            }

            source_entry = source_entry->next;
        }
    }

    /* Phase 2: Apply decay to all emotions (already holding mutex) */
    apply_decay_unlocked(ec, delta_ms);

    /* Phase 3: Decay resistances */
    if (ec->config.enable_resistance) {
        for (size_t h = 0; h < ec->hash_size; h++) {
            agent_entry_t* entry = ec->agent_hash[h];
            while (entry) {
                decay_resistances(&entry->state,
                                  ec->config.resistance_decay_rate,
                                  delta_ms);
                entry = entry->next;
            }
        }
    }

    ec->collective_dirty = true;

    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

/* Internal: apply decay without locking (caller must hold mutex) */
static void apply_decay_unlocked(emotional_contagion_t* ec, uint64_t delta_ms) {
    for (size_t h = 0; h < ec->hash_size; h++) {
        agent_entry_t* entry = ec->agent_hash[h];
        while (entry) {
            if (entry->state.intensity > EPSILON) {
                entry->state.intensity = apply_decay(
                    entry->state.intensity,
                    ec->config.decay_rate,
                    delta_ms);

                /* Return to neutral if intensity too low */
                if (entry->state.intensity < EPSILON) {
                    entry->state.emotion = EMOTION_NEUTRAL;
                    entry->state.intensity = 0.0F;
                }
            }
            entry = entry->next;
        }
    }
}

nimcp_result_t emotional_contagion_apply_decay(
    emotional_contagion_t* ec,
    uint64_t delta_ms) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);
    apply_decay_unlocked(ec, delta_ms);
    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

/* Continued in next part... */

/* ============================================================================
 * Collective State Functions
 * ============================================================================ */

nimcp_result_t emotional_contagion_get_dominant_emotion(
    emotional_contagion_t* ec,
    emotion_type_t* emotion,
    float* avg_intensity) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(emotion, NIMCP_ERROR_NULL_POINTER, "emotion output pointer is NULL");
    NIMCP_CHECK_THROW(avg_intensity, NIMCP_ERROR_NULL_POINTER, "avg_intensity output pointer is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    if (ec->collective_dirty) {
        recompute_collective_state(ec);
    }

    *emotion = ec->collective.dominant_emotion;
    *avg_intensity = ec->collective.dominant_intensity;

    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_get_collective_state(
    emotional_contagion_t* ec,
    collective_emotion_state_t* state) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state output pointer is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    if (ec->collective_dirty) {
        recompute_collective_state(ec);
    }

    *state = ec->collective;

    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Utilities
 * ============================================================================ */

nimcp_result_t emotional_contagion_get_stats(
    emotional_contagion_t* ec,
    emotional_contagion_stats_t* stats) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats output pointer is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);
    *stats = ec->stats;
    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_reset_stats(emotional_contagion_t* ec) {
    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);
    memset(&ec->stats, 0, sizeof(emotional_contagion_stats_t));
    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

const char* emotional_contagion_emotion_name(emotion_type_t emotion) {
    if (emotion >= EMOTION_TYPE_COUNT) {
        return "unknown";
    }
    return emotion_names[emotion];
}

emotion_type_t emotional_contagion_emotion_from_name(const char* name) {
    if (!name) return EMOTION_NEUTRAL;

    for (size_t i = 0; i < EMOTION_TYPE_COUNT; i++) {
        if (strcasecmp(name, emotion_names[i]) == 0) {
            return (emotion_type_t)i;
        }
    }

    return EMOTION_NEUTRAL;
}

/* ============================================================================
 * Outbreak Functions
 * ============================================================================ */

/**
 * @brief Trigger emotional outbreak from source agent
 *
 * WHAT: Initiates emotional contagion from a source agent
 * WHY:  Enable rapid emotional spread through swarm network
 * HOW:  Sets source emotion and propagates through connections
 */
nimcp_result_t emotional_contagion_trigger_outbreak(
    emotional_contagion_t* ec,
    uint32_t source_agent,
    emotion_type_t emotion,
    float initial_intensity,
    uint32_t max_depth) {

    /* Guard: validate parameters */
    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(emotion < EMOTION_TYPE_COUNT, NIMCP_ERROR_INVALID_PARAM, "invalid emotion type");
    NIMCP_CHECK_THROW(initial_intensity >= 0.0F && initial_intensity <= 1.0F,
                      NIMCP_ERROR_INVALID_PARAM, "initial_intensity must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(&ec->mutex);

    /* Find source agent */
    agent_entry_t* source = find_agent(ec, source_agent);
    if (!source) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_NOT_FOUND, "source agent not found");
    }

    /* Set source agent's emotion */
    source->state.emotion = emotion;
    source->state.intensity = initial_intensity;
    source->state.last_update_ms = nimcp_time_get_us() / 1000;

    /* Use config max_propagation_depth if max_depth is 0 */
    uint32_t depth = (max_depth == 0) ? ec->config.max_propagation_depth : max_depth;
    if (depth == 0) depth = 3;  /* Default depth if config also 0 */

    /* Track agents visited to prevent re-infection */
    bool* visited = (bool*)nimcp_calloc(ec->agent_count, sizeof(bool));
    if (!visited) {
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "failed to allocate visited array");
    }

    /* BFS for propagation */
    uint32_t* queue = (uint32_t*)nimcp_malloc(ec->agent_count * sizeof(uint32_t));
    uint32_t* depths = (uint32_t*)nimcp_malloc(ec->agent_count * sizeof(uint32_t));
    if (!queue || !depths) {
        nimcp_free(visited);
        if (queue) nimcp_free(queue);
        if (depths) nimcp_free(depths);
        nimcp_platform_mutex_unlock(&ec->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_MEMORY, "failed to allocate BFS arrays");
    }

    size_t head = 0, tail = 0;
    queue[tail] = source_agent;
    depths[tail] = 0;
    tail++;
    visited[0] = true;  /* Mark source as visited */

    uint32_t infected_count = 1;

    while (head < tail) {
        uint32_t current_agent = queue[head];
        uint32_t current_depth = depths[head];
        head++;

        if (current_depth >= depth) continue;

        agent_entry_t* current = find_agent(ec, current_agent);
        if (!current) continue;

        /* Propagate to all connected agents */
        for (uint32_t i = 0; i < current->connection_count; i++) {
            emotional_connection_t* conn = current->connections[i];
            if (!conn) continue;

            agent_entry_t* target = find_agent(ec, conn->to_agent);
            if (!target) continue;

            /* Check if already visited */
            bool already_visited = false;
            for (size_t j = 0; j < tail; j++) {
                if (queue[j] == conn->to_agent) {
                    already_visited = true;
                    break;
                }
            }
            if (already_visited) continue;

            /* Calculate contagion strength */
            float strength = current->state.intensity *
                             ec->config.contagion_rate *
                             target->state.susceptibility *
                             conn->connection_strength;

            /* Apply resistance if enabled */
            if (ec->config.enable_resistance) {
                strength *= (1.0F - target->state.resistance[emotion]);
            }

            /* Skip if strength too low */
            if (strength < EPSILON) continue;

            /* Infect target */
            target->state.emotion = emotion;
            target->state.intensity = fminf(strength, 1.0F);
            target->state.last_update_ms = nimcp_time_get_us() / 1000;
            infected_count++;

            /* Add to queue for next depth */
            queue[tail] = conn->to_agent;
            depths[tail] = current_depth + 1;
            tail++;

            ec->stats.total_propagations++;
        }
    }

    nimcp_free(visited);
    nimcp_free(queue);
    nimcp_free(depths);

    ec->collective_dirty = true;
    ec->stats.successful_transmissions++;

    nimcp_platform_mutex_unlock(&ec->mutex);

    LOG_INFO(CONTAGION_MODULE, "Outbreak from agent %u: %s at %.2f, infected %u agents",
             source_agent, emotion_names[emotion], initial_intensity, infected_count);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration (Stub implementations)
 * ============================================================================ */

nimcp_result_t emotional_contagion_register_bioasync(
    emotional_contagion_t* ec,
    bio_router_t* router) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(router, NIMCP_ERROR_NULL_POINTER, "bio router is NULL");

    ec->bio_router = router;
    ec->bio_async_registered = true;

    LOG_INFO(CONTAGION_MODULE, "Registered with bio-async system");

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_broadcast_spread(
    emotional_contagion_t* ec,
    const emotional_propagation_event_t* event) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(event, NIMCP_ERROR_NULL_POINTER, "propagation event is NULL");
    NIMCP_CHECK_THROW(ec->bio_async_registered, NIMCP_ERROR_NULL_POINTER, "bio-async not registered");

    /* Would send bio-async message here */
    ec->stats.bio_broadcasts_sent++;

    return NIMCP_SUCCESS;
}

nimcp_result_t emotional_contagion_broadcast_collective(
    emotional_contagion_t* ec) {

    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(ec->bio_async_registered, NIMCP_ERROR_NULL_POINTER, "bio-async not registered");

    /* Would send collective state via bio-async */
    ec->stats.bio_broadcasts_sent++;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Coherence Measurement
 * ============================================================================ */

/**
 * @brief Get emotional coherence of the swarm
 *
 * WHAT: Measures alignment of emotional states across agents
 * WHY:  Coherence indicates emergent collective emotional state
 * HOW:  Compute variance-based coherence metric from agent emotions
 *
 * @param ec Emotional contagion system
 * @param coherence Output coherence value (0.0 = chaotic, 1.0 = fully aligned)
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t emotional_contagion_get_coherence(
    emotional_contagion_t* ec,
    float* coherence
) {
    NIMCP_CHECK_THROW(ec, NIMCP_ERROR_NULL_POINTER, "emotional contagion context is NULL");
    NIMCP_CHECK_THROW(coherence, NIMCP_ERROR_NULL_POINTER, "coherence output pointer is NULL");

    nimcp_platform_mutex_lock(&ec->mutex);

    /* Use pre-computed collective state coherence */
    *coherence = ec->collective.emotional_coherence;

    nimcp_platform_mutex_unlock(&ec->mutex);

    return NIMCP_SUCCESS;
}

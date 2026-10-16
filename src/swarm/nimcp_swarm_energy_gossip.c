/**
 * @file nimcp_swarm_energy_gossip.c
 * @brief Energy-Aware Gossip Protocol Implementation
 *
 * Biological inspiration: Metabolic efficiency in biological systems
 * - Honeybee energy conservation during foraging
 * - Bat torpor and hibernation strategies
 * - Neural energy management in the brain
 * - Cellular ATP management and regulation
 */

#include "swarm/nimcp_swarm_energy_gossip.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "swarm_energy_gossip"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_energy_gossip)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate distance between two 3D points
 */
static float calculate_distance(const float pos1[3], const float pos2[3]) {
    float dx = pos1[0] - pos2[0];
    float dy = pos1[1] - pos2[1];
    float dz = pos1[2] - pos2[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Generate random float between 0 and 1
 */
static float random_float(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

/**
 * @brief Update energy state based on current level
 */
static void update_energy_state(NimcpEnergyGossip* gossip) {
    if (!gossip) return;

    NimcpEnergyState old_state = gossip->current_state;

    if (gossip->stats.harvest_rate > 0.0F &&
        gossip->stats.harvest_rate > gossip->stats.consumption_rate) {
        gossip->current_state = NIMCP_ENERGY_CHARGING;
    } else {
        gossip->current_state = nimcp_energy_level_to_state(
            gossip->stats.current_level,
            false
        );
    }

    if (old_state != gossip->current_state) {
        gossip->energy_state_changes++;
        LOG_INFO("Node %u energy state changed: %s -> %s (%.1f%%)",
            gossip->node_id,
            nimcp_energy_state_to_string(old_state),
            nimcp_energy_state_to_string(gossip->current_state),
            gossip->stats.current_level);
    }
}

/**
 * @brief Check if message is already in cache
 */
static bool is_message_cached(const NimcpEnergyGossip* gossip, uint64_t message_id) {
    if (!gossip || !gossip->message_cache) {
        return false;  /* Defensive guard - not an error */
    }

    for (uint32_t i = 0; i < gossip->message_cache_size; i++) {
        if (gossip->message_cache[i] &&
            gossip->message_cache[i]->header.message_id == message_id) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Add message to cache
 */
static nimcp_result_t add_to_cache(NimcpEnergyGossip* gossip, const NimcpGossipMessage* message) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_ERROR_NULL_POINTER, "message is NULL");

    /* Check if cache is full */
    if (gossip->message_cache_size >= gossip->message_cache_capacity) {
        /* Remove oldest message (FIFO) */
        if (gossip->message_cache[0]) {
            if (gossip->message_cache[0]->payload) {
                nimcp_free(gossip->message_cache[0]->payload);
            }
            nimcp_free(gossip->message_cache[0]);
        }

        /* Shift messages */
        for (uint32_t i = 0; i < gossip->message_cache_size - 1; i++) {
            gossip->message_cache[i] = gossip->message_cache[i + 1];
        }
        gossip->message_cache_size--;
    }

    /* Allocate new message */
    NimcpGossipMessage* cached = (NimcpGossipMessage*)nimcp_malloc(sizeof(NimcpGossipMessage));
    NIMCP_CHECK_THROW(cached, NIMCP_NO_MEMORY, "failed to allocate gossip message cache entry");

    memcpy(cached, message, sizeof(NimcpGossipMessage));

    /* Copy payload */
    if (message->payload && message->payload_size > 0) {
        cached->payload = nimcp_malloc(message->payload_size);
        if (!cached->payload) {
            nimcp_free(cached);
            return NIMCP_NO_MEMORY;
        }
        memcpy(cached->payload, message->payload, message->payload_size);
    }

    gossip->message_cache[gossip->message_cache_size++] = cached;
    return NIMCP_SUCCESS;
}

/**
 * @brief Find relay node by ID
 */
static NimcpRelayNode* find_relay_node(NimcpEnergyGossip* gossip, uint32_t node_id) {
    if (!gossip || !gossip->relay_nodes) {
        return NULL;  /* Defensive guard - not an error */
    }

    for (uint32_t i = 0; i < gossip->relay_node_count; i++) {
        if (gossip->relay_nodes[i].node_id == node_id) {
            return &gossip->relay_nodes[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Calculate relay score (higher is better)
 */
static float calculate_relay_score(const NimcpRelayNode* relay) {
    if (!relay || !relay->is_available) return 0.0F;

    /* Scoring factors:
     * - Energy level (40% weight)
     * - Reliability (30% weight)
     * - Inverse distance (20% weight)
     * - Message count balance (10% weight)
     */
    float energy_score = relay->energy_level / 100.0F;
    float reliability_score = relay->reliability_score;
    float distance_score = (relay->distance > 0.0F) ? (1.0F / (1.0F + relay->distance)) : 1.0F;
    float load_score = (relay->message_count > 0) ? (1.0F / (1.0F + relay->message_count / 100.0F)) : 1.0F;

    return 0.4F * energy_score +
           0.3F * reliability_score +
           0.2F * distance_score +
           0.1F * load_score;
}

/**
 * @brief Find harvest opportunity by position
 */
static NimcpHarvestOpportunity* find_harvest_opportunity(
    NimcpEnergyGossip* gossip,
    const float position[3]
) {
    if (!gossip || !gossip->opportunities || !position) {
        return NULL;  /* Defensive guard - not an error */
    }

    const float tolerance = 1.0F; /* 1 unit tolerance */

    for (uint32_t i = 0; i < gossip->opportunity_count; i++) {
        float dist = calculate_distance(gossip->opportunities[i].position, position);
        if (dist < tolerance) {
            return &gossip->opportunities[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/* ============================================================================
 * Core Functions Implementation
 * ============================================================================ */

void nimcp_energy_gossip_default_config(NimcpEnergyGossipConfig* config) {
    if (!config) return;

    memset(config, 0, sizeof(NimcpEnergyGossipConfig));

    /* Adaptive intervals (milliseconds) */
    config->intervals.interval_critical_ms = 10000;  /* 10 seconds */
    config->intervals.interval_low_ms = 5000;        /* 5 seconds */
    config->intervals.interval_normal_ms = 1000;     /* 1 second */
    config->intervals.interval_high_ms = 500;        /* 500 ms */
    config->intervals.interval_full_ms = 250;        /* 250 ms */
    config->intervals.interval_charging_ms = 2000;   /* 2 seconds */

    /* Forwarding probabilities */
    config->forwarding_probs.prob_critical = 0.05F;  /* 5% when critical */
    config->forwarding_probs.prob_low = 0.25F;       /* 25% when low */
    config->forwarding_probs.prob_normal = 0.75F;    /* 75% normal */
    config->forwarding_probs.prob_high = 0.95F;      /* 95% when high */
    config->forwarding_probs.prob_full = 1.0F;       /* 100% when full */
    config->forwarding_probs.prob_charging = 0.50F;  /* 50% while charging */

    /* Relay configuration */
    config->max_relay_candidates = 10;
    config->min_relay_energy = 30.0F;  /* 30% minimum */

    /* Sleep configuration */
    config->sleep_check_interval_ms = 1000;  /* Check every second */

    /* Feature flags */
    config->enable_harvest_awareness = true;
    config->enable_coordinated_sleep = true;

    /* Gossip parameters */
    config->gossip_fanout = 3;  /* Gossip to 3 nodes */
    config->max_message_cache = 100;
    config->convergence_threshold = 0.95F;
}

NimcpEnergyGossip* nimcp_energy_gossip_create(
    uint32_t node_id,
    const NimcpEnergyGossipConfig* config
) {
    NimcpEnergyGossip* gossip = (NimcpEnergyGossip*)nimcp_malloc(sizeof(NimcpEnergyGossip));
    if (!gossip) {
        LOG_ERROR("Failed to allocate energy gossip protocol");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_energy_gossip_create: alloc failed");

        return NULL;
    }

    memset(gossip, 0, sizeof(NimcpEnergyGossip));
    gossip->node_id = node_id;

    /* Apply configuration */
    if (config) {
        memcpy(&gossip->config, config, sizeof(NimcpEnergyGossipConfig));
    } else {
        nimcp_energy_gossip_default_config(&gossip->config);
    }

    /* Initialize energy state */
    gossip->current_state = NIMCP_ENERGY_NORMAL;
    gossip->stats.current_level = 100.0F;
    gossip->stats.consumption_rate = 1.0F;  /* Default rate */
    gossip->stats.harvest_rate = 0.0F;
    gossip->stats.last_update = time(NULL);

    /* Initialize emergency reserve */
    gossip->reserve.reserve_percentage = 15.0F;
    gossip->reserve.auto_return_enabled = true;
    gossip->reserve.emergency_threshold = 10.0F;

    /* Allocate message cache */
    gossip->message_cache_capacity = gossip->config.max_message_cache;
    gossip->message_cache = (NimcpGossipMessage**)nimcp_calloc(
        gossip->message_cache_capacity,
        sizeof(NimcpGossipMessage*)
    );
    if (!gossip->message_cache) {
        LOG_ERROR("Failed to allocate message cache");
        nimcp_free(gossip);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_energy_gossip_create: gossip->message_cache is NULL");
        return NULL;
    }

    /* Allocate relay nodes array */
    gossip->relay_node_capacity = gossip->config.max_relay_candidates * 2;
    gossip->relay_nodes = (NimcpRelayNode*)nimcp_calloc(
        gossip->relay_node_capacity,
        sizeof(NimcpRelayNode)
    );
    if (!gossip->relay_nodes) {
        LOG_ERROR("Failed to allocate relay nodes");
        nimcp_free(gossip->message_cache);
        nimcp_free(gossip);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_energy_gossip_create: gossip->relay_nodes is NULL");
        return NULL;
    }

    /* Allocate harvest opportunities array */
    if (gossip->config.enable_harvest_awareness) {
        gossip->opportunities = (NimcpHarvestOpportunity*)nimcp_calloc(
            10,  /* Initial capacity */
            sizeof(NimcpHarvestOpportunity)
        );
    }

    /* Create mutex */
    gossip->mutex = nimcp_platform_mutex_create();
    if (!gossip->mutex) {
        LOG_ERROR("Failed to create mutex");
        nimcp_free(gossip->opportunities);
        nimcp_free(gossip->relay_nodes);
        nimcp_free(gossip->message_cache);
        nimcp_free(gossip);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_energy_gossip_create: gossip->mutex is NULL");
        return NULL;
    }

    gossip->next_message_id = 1;
    gossip->sequence_number = 0;
    gossip->is_initialized = true;

    LOG_INFO("Energy-aware gossip protocol created for node %u", node_id);
    return gossip;
}

void nimcp_energy_gossip_destroy(NimcpEnergyGossip* gossip) {
    if (!gossip) return;

    LOG_INFO("Destroying energy gossip protocol for node %u", gossip->node_id);

    /* Free message cache */
    if (gossip->message_cache) {
        for (uint32_t i = 0; i < gossip->message_cache_size; i++) {
            if (gossip->message_cache[i]) {
                if (gossip->message_cache[i]->payload) {
                    nimcp_free(gossip->message_cache[i]->payload);
                }
                nimcp_free(gossip->message_cache[i]);
            }
        }
        nimcp_free(gossip->message_cache);
    }

    /* Free relay nodes */
    nimcp_free(gossip->relay_nodes);

    /* Free sleep schedule */
    nimcp_free(gossip->sleep_schedule);

    /* Free harvest opportunities */
    nimcp_free(gossip->opportunities);

    /* Destroy mutex */
    if (gossip->mutex) {
        nimcp_platform_mutex_destroy(gossip->mutex);
        nimcp_free(gossip->mutex);
        gossip->mutex = NULL;
    }

    nimcp_free(gossip);
}

nimcp_result_t nimcp_energy_gossip_init_bio_async(
    NimcpEnergyGossip* gossip,
    void* inbox
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(inbox, NIMCP_ERROR_NULL_POINTER, "inbox is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);
    gossip->inbox = inbox;
    gossip->bio_async_enabled = true;
    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_INFO("Bio-async enabled for node %u", gossip->node_id);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Energy Management Implementation
 * ============================================================================ */

nimcp_result_t nimcp_energy_gossip_update_energy(
    NimcpEnergyGossip* gossip,
    float energy_level
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(energy_level >= 0.0F && energy_level <= 100.0F, NIMCP_INVALID_PARAM, "energy_level must be in range [0.0, 100.0]");

    nimcp_platform_mutex_lock(gossip->mutex);

    float old_level = gossip->stats.current_level;
    gossip->stats.current_level = energy_level;

    /* Update consumption tracking */
    time_t now = time(NULL);
    float time_delta = (float)difftime(now, gossip->stats.last_update);
    if (time_delta > 0.0F) {
        float energy_delta = old_level - energy_level;
        if (energy_delta > 0.0F) {
            gossip->stats.total_consumed += (uint64_t)(energy_delta * 1000);
            /* Update consumption rate (exponential moving average) */
            float current_rate = energy_delta / time_delta;
            gossip->stats.consumption_rate =
                0.8F * gossip->stats.consumption_rate + 0.2F * current_rate;
        } else if (energy_delta < 0.0F) {
            /* Harvesting energy */
            gossip->stats.total_harvested += (uint64_t)(-energy_delta * 1000);
        }
    }
    gossip->stats.last_update = now;

    /* Update predicted lifetime */
    if (gossip->stats.consumption_rate > gossip->stats.harvest_rate) {
        float net_consumption = gossip->stats.consumption_rate - gossip->stats.harvest_rate;
        gossip->stats.predicted_lifetime =
            (net_consumption > 0.0F) ? (energy_level / net_consumption) : INFINITY;
    } else {
        gossip->stats.predicted_lifetime = INFINITY;
    }

    /* Update energy state */
    update_energy_state(gossip);

    /* Check emergency return */
    if (gossip->reserve.auto_return_enabled &&
        nimcp_energy_gossip_needs_emergency_return(gossip)) {
        LOG_WARN("Node %u: Emergency return needed (%.1f%%)",
            gossip->node_id, energy_level);
    }

    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

NimcpEnergyState nimcp_energy_gossip_get_state(const NimcpEnergyGossip* gossip) {
    if (!gossip) return NIMCP_ENERGY_CRITICAL;
    return gossip->current_state;
}

nimcp_result_t nimcp_energy_gossip_set_consumption_rate(
    NimcpEnergyGossip* gossip,
    float rate
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(rate >= 0.0F, NIMCP_INVALID_PARAM, "consumption rate must be non-negative");

    nimcp_platform_mutex_lock(gossip->mutex);
    gossip->stats.consumption_rate = rate;
    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_set_harvest_rate(
    NimcpEnergyGossip* gossip,
    float rate
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(rate >= 0.0F, NIMCP_INVALID_PARAM, "harvest rate must be non-negative");

    nimcp_platform_mutex_lock(gossip->mutex);
    gossip->stats.harvest_rate = rate;

    /* Update energy state in case we're now charging */
    update_energy_state(gossip);

    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

float nimcp_energy_gossip_predict_lifetime(const NimcpEnergyGossip* gossip) {
    if (!gossip) return 0.0F;
    return gossip->stats.predicted_lifetime;
}

nimcp_result_t nimcp_energy_gossip_get_stats(
    const NimcpEnergyGossip* gossip,
    NimcpEnergyStats* stats
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats output is NULL");

    memcpy(stats, &gossip->stats, sizeof(NimcpEnergyStats));
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Emergency Reserve Implementation
 * ============================================================================ */

nimcp_result_t nimcp_energy_gossip_configure_reserve(
    NimcpEnergyGossip* gossip,
    float reserve_percentage,
    float return_distance,
    float return_energy_cost
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(reserve_percentage >= 0.0F && reserve_percentage <= 50.0F, NIMCP_INVALID_PARAM, "reserve_percentage must be in range [0.0, 50.0]");

    nimcp_platform_mutex_lock(gossip->mutex);
    gossip->reserve.reserve_percentage = reserve_percentage;
    gossip->reserve.return_distance = return_distance;
    gossip->reserve.return_energy_cost = return_energy_cost;
    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_INFO("Node %u: Emergency reserve configured (%.1f%%, cost: %.1f)",
        gossip->node_id, reserve_percentage, return_energy_cost);

    return NIMCP_SUCCESS;
}

bool nimcp_energy_gossip_needs_emergency_return(const NimcpEnergyGossip* gossip) {
    if (!gossip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_energy_gossip_needs_emergency_return: gossip is NULL");
        return false;
    }

    /* Need emergency return if:
     * 1. Energy below emergency threshold, OR
     * 2. Current energy < return cost + reserve
     */
    float required_energy = gossip->reserve.return_energy_cost +
                           gossip->reserve.reserve_percentage;

    return gossip->stats.current_level <= gossip->reserve.emergency_threshold ||
           gossip->stats.current_level < required_energy;
}

nimcp_result_t nimcp_energy_gossip_emergency_mode(NimcpEnergyGossip* gossip) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);
    gossip->reserve.emergency_mode = true;
    gossip->current_state = NIMCP_ENERGY_CRITICAL;
    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_WARN("Node %u: EMERGENCY MODE ACTIVATED", gossip->node_id);

    /* Broadcast emergency message if bio-async enabled */
    if (gossip->bio_async_enabled && gossip->inbox) {
        /* Emergency broadcast implementation would go here */
        LOG_INFO("Node %u: Broadcasting emergency status", gossip->node_id);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Gossip Protocol Implementation
 * ============================================================================ */

uint64_t nimcp_energy_gossip_broadcast(
    NimcpEnergyGossip* gossip,
    const void* payload,
    size_t payload_size,
    NimcpMessagePriority priority,
    uint32_t ttl
) {
    if (!gossip || !payload || payload_size == 0) return 0;

    /* Check if we should send based on energy state */
    if (gossip->current_state == NIMCP_ENERGY_CRITICAL &&
        priority < NIMCP_PRIORITY_HIGH) {
        LOG_DEBUG("Node %u: Dropping non-critical message due to energy",
            gossip->node_id);
        gossip->messages_dropped++;
        return 0;
    }

    nimcp_platform_mutex_lock(gossip->mutex);

    /* Create message */
    NimcpGossipMessage message;
    memset(&message, 0, sizeof(NimcpGossipMessage));

    message.header.message_id = gossip->next_message_id++;
    message.header.originator_id = gossip->node_id;
    message.header.hop_count = 0;
    message.header.ttl = ttl;
    message.header.priority = priority;
    message.header.timestamp = time(NULL);
    message.header.sequence_number = gossip->sequence_number++;
    message.payload_size = payload_size;
    message.payload = (void*)payload;  /* Casting away const for structure */
    message.forwarded = false;
    message.forward_count = 0;

    /* Add to cache */
    add_to_cache(gossip, &message);

    gossip->messages_sent++;

    uint64_t message_id = message.header.message_id;
    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_DEBUG("Node %u: Broadcast message %llu (priority: %d, ttl: %u)",
        gossip->node_id, (unsigned long long)message_id, priority, ttl);

    return message_id;
}

nimcp_result_t nimcp_energy_gossip_receive(
    NimcpEnergyGossip* gossip,
    const NimcpGossipMessage* message
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_ERROR_NULL_POINTER, "message is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    /* Check if already received */
    if (is_message_cached(gossip, message->header.message_id)) {
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_SUCCESS;  /* Duplicate, ignore */
    }

    /* Check if we should process based on priority and energy */
    if (!nimcp_energy_gossip_should_process(gossip, message->header.priority)) {
        gossip->messages_dropped++;
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_SUCCESS;
    }

    /* Add to cache */
    add_to_cache(gossip, message);

    gossip->messages_received++;

    LOG_DEBUG("Node %u: Received message %llu from node %u (hop %u/%u)",
        gossip->node_id,
        (unsigned long long)message->header.message_id,
        message->header.originator_id,
        message->header.hop_count,
        message->header.ttl);

    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_forward(
    NimcpEnergyGossip* gossip,
    NimcpGossipMessage* message
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_ERROR_NULL_POINTER, "message is NULL");

    /* Check TTL */
    if (message->header.hop_count >= message->header.ttl) {
        return NIMCP_SUCCESS;
    }

    /* Check if we're sleeping */
    if (gossip->is_sleeping && message->header.priority < NIMCP_PRIORITY_URGENT) {
        return NIMCP_SUCCESS;
    }

    /* Probabilistic forwarding based on energy state */
    if (!nimcp_energy_gossip_should_forward(gossip, message)) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(gossip->mutex);

    message->header.hop_count++;
    message->forwarded = true;
    message->forward_count++;
    gossip->messages_forwarded++;

    LOG_DEBUG("Node %u: Forwarding message %llu (hop %u/%u)",
        gossip->node_id,
        (unsigned long long)message->header.message_id,
        message->header.hop_count,
        message->header.ttl);

    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

float nimcp_energy_gossip_get_forward_probability(const NimcpEnergyGossip* gossip) {
    if (!gossip) return 0.0F;

    switch (gossip->current_state) {
        case NIMCP_ENERGY_CRITICAL:
            return gossip->config.forwarding_probs.prob_critical;
        case NIMCP_ENERGY_LOW:
            return gossip->config.forwarding_probs.prob_low;
        case NIMCP_ENERGY_NORMAL:
            return gossip->config.forwarding_probs.prob_normal;
        case NIMCP_ENERGY_HIGH:
            return gossip->config.forwarding_probs.prob_high;
        case NIMCP_ENERGY_FULL:
            return gossip->config.forwarding_probs.prob_full;
        case NIMCP_ENERGY_CHARGING:
            return gossip->config.forwarding_probs.prob_charging;
        default:
            return 0.5F;
    }
}

bool nimcp_energy_gossip_should_forward(
    const NimcpEnergyGossip* gossip,
    const NimcpGossipMessage* message
) {
    if (!gossip || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_energy_gossip_should_forward: required parameter is NULL (gossip, message)");
        return false;
    }

    /* Always forward critical messages */
    if (message->header.priority == NIMCP_PRIORITY_URGENT) {
        return true;
    }

    /* Never forward in emergency mode unless critical */
    if (gossip->reserve.emergency_mode) {
        return false;  /* Normal policy - conserve energy in emergency mode */
    }

    /* Probabilistic forwarding based on energy state */
    float probability = nimcp_energy_gossip_get_forward_probability(gossip);
    return random_float() < probability;
}

/* ============================================================================
 * Relay Selection Implementation
 * ============================================================================ */

nimcp_result_t nimcp_energy_gossip_register_relay(
    NimcpEnergyGossip* gossip,
    uint32_t node_id,
    float energy_level,
    float distance
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(node_id != gossip->node_id, NIMCP_INVALID_PARAM, "cannot register self as relay");

    nimcp_platform_mutex_lock(gossip->mutex);

    /* Check if already registered */
    NimcpRelayNode* existing = find_relay_node(gossip, node_id);
    if (existing) {
        /* Update existing */
        existing->energy_level = energy_level;
        existing->energy_state = nimcp_energy_level_to_state(energy_level, false);
        existing->distance = distance;
        existing->last_seen = time(NULL);
        existing->is_available = (energy_level >= gossip->config.min_relay_energy);
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_SUCCESS;
    }

    /* Add new relay node */
    if (gossip->relay_node_count >= gossip->relay_node_capacity) {
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_NO_MEMORY;
    }

    NimcpRelayNode* relay = &gossip->relay_nodes[gossip->relay_node_count++];
    memset(relay, 0, sizeof(NimcpRelayNode));

    relay->node_id = node_id;
    relay->energy_state = nimcp_energy_level_to_state(energy_level, false);
    relay->energy_level = energy_level;
    relay->reliability_score = 1.0F;  /* Initial score */
    relay->distance = distance;
    relay->message_count = 0;
    relay->last_seen = time(NULL);
    relay->is_available = (energy_level >= gossip->config.min_relay_energy);

    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_DEBUG("Node %u: Registered relay node %u (energy: %.1f%%, distance: %.1f)",
        gossip->node_id, node_id, energy_level, distance);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_update_relay(
    NimcpEnergyGossip* gossip,
    uint32_t node_id,
    float energy_level
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    NimcpRelayNode* relay = find_relay_node(gossip, node_id);
    if (!relay) {
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_NOT_FOUND;
    }

    relay->energy_level = energy_level;
    relay->energy_state = nimcp_energy_level_to_state(energy_level, false);
    relay->last_seen = time(NULL);
    relay->is_available = (energy_level >= gossip->config.min_relay_energy);

    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_select_relays(
    NimcpEnergyGossip* gossip,
    uint32_t max_relays,
    uint32_t* selected,
    uint32_t* selected_count
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(selected, NIMCP_ERROR_NULL_POINTER, "selected output array is NULL");
    NIMCP_CHECK_THROW(selected_count, NIMCP_ERROR_NULL_POINTER, "selected_count output is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    *selected_count = 0;

    /* Calculate scores for all available relays */
    float* scores = (float*)nimcp_calloc(gossip->relay_node_count, sizeof(float));
    if (!scores) {
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_NO_MEMORY;
    }

    for (uint32_t i = 0; i < gossip->relay_node_count; i++) {
        scores[i] = calculate_relay_score(&gossip->relay_nodes[i]);
    }

    /* Select top N relays by score */
    for (uint32_t n = 0; n < max_relays && n < gossip->relay_node_count; n++) {
        /* Find highest scoring relay */
        float max_score = -1.0F;
        uint32_t max_idx = 0;

        for (uint32_t i = 0; i < gossip->relay_node_count; i++) {
            if (scores[i] > max_score) {
                max_score = scores[i];
                max_idx = i;
            }
        }

        if (max_score > 0.0F) {
            selected[*selected_count] = gossip->relay_nodes[max_idx].node_id;
            (*selected_count)++;
            scores[max_idx] = -1.0F;  /* Mark as selected */
        } else {
            break;  /* No more available relays */
        }
    }

    nimcp_free(scores);
    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_DEBUG("Node %u: Selected %u relay nodes", gossip->node_id, *selected_count);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_get_relay_info(
    const NimcpEnergyGossip* gossip,
    uint32_t node_id,
    NimcpRelayNode* relay
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(relay, NIMCP_ERROR_NULL_POINTER, "relay output is NULL");

    for (uint32_t i = 0; i < gossip->relay_node_count; i++) {
        if (gossip->relay_nodes[i].node_id == node_id) {
            memcpy(relay, &gossip->relay_nodes[i], sizeof(NimcpRelayNode));
            return NIMCP_SUCCESS;
        }
    }

    return NIMCP_NOT_FOUND;
}

/* ============================================================================
 * Sleep Scheduling Implementation
 * ============================================================================ */

nimcp_result_t nimcp_energy_gossip_schedule_sleep(
    NimcpEnergyGossip* gossip,
    time_t start_time,
    uint32_t duration_seconds,
    bool wake_on_emergency
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    /* Expand schedule array if needed */
    if (!gossip->sleep_schedule) {
        gossip->sleep_schedule = (NimcpSleepSchedule*)nimcp_calloc(
            10, sizeof(NimcpSleepSchedule)
        );
        if (!gossip->sleep_schedule) {
            nimcp_platform_mutex_unlock(gossip->mutex);
            return NIMCP_NO_MEMORY;
        }
    }

    NimcpSleepSchedule* schedule = &gossip->sleep_schedule[gossip->sleep_schedule_count++];
    schedule->sleep_start = start_time;
    schedule->sleep_end = start_time + duration_seconds;
    schedule->duration_seconds = duration_seconds;
    schedule->wake_on_emergency = wake_on_emergency;
    schedule->wake_events = 0;

    /* Estimate energy saved */
    float active_consumption = gossip->stats.consumption_rate;
    float sleep_consumption = active_consumption * 0.1F;  /* 10% consumption while sleeping */
    schedule->energy_saved = (active_consumption - sleep_consumption) * duration_seconds;

    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_INFO("Node %u: Sleep scheduled for %u seconds (saving ~%.1f energy units)",
        gossip->node_id, duration_seconds, schedule->energy_saved);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_sleep(
    NimcpEnergyGossip* gossip,
    uint32_t duration_seconds
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    gossip->is_sleeping = true;
    gossip->sleep_until = time(NULL) + duration_seconds;
    gossip->sleep_cycles++;

    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_INFO("Node %u: Entering sleep mode for %u seconds",
        gossip->node_id, duration_seconds);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_wake(NimcpEnergyGossip* gossip) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    if (gossip->is_sleeping) {
        gossip->is_sleeping = false;
        gossip->sleep_until = 0;

        LOG_INFO("Node %u: Waking from sleep", gossip->node_id);
    }

    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

bool nimcp_energy_gossip_is_sleeping(const NimcpEnergyGossip* gossip) {
    if (!gossip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_energy_gossip_is_sleeping: gossip is NULL");
        return false;
    }

    /* Check if sleep period has expired */
    if (gossip->is_sleeping && gossip->sleep_until > 0) {
        time_t now = time(NULL);
        if (now >= gossip->sleep_until) {
            return false;  /* Sleep period expired - normal condition */
        }
    }

    return gossip->is_sleeping;
}

nimcp_result_t nimcp_energy_gossip_coordinate_sleep(
    NimcpEnergyGossip* gossip,
    const NimcpSleepSchedule* node_schedules,
    uint32_t node_count
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(node_schedules, NIMCP_ERROR_NULL_POINTER, "node_schedules is NULL");

    /* Analyze other nodes' schedules to find optimal sleep time */
    time_t now = time(NULL);
    uint32_t sleeping_nodes[24] = {0};  /* Nodes sleeping per hour */

    for (uint32_t i = 0; i < node_count; i++) {
        time_t start = node_schedules[i].sleep_start;
        time_t end = node_schedules[i].sleep_end;

        for (time_t t = start; t < end; t += 3600) {
            uint32_t hour = (uint32_t)((t - now) / 3600) % 24;
            sleeping_nodes[hour]++;
        }
    }

    /* Find hour with minimum sleeping nodes */
    uint32_t min_sleeping = UINT32_MAX;
    uint32_t best_hour = 0;

    for (uint32_t h = 0; h < 24; h++) {
        if (sleeping_nodes[h] < min_sleeping) {
            min_sleeping = sleeping_nodes[h];
            best_hour = h;
        }
    }

    /* Schedule sleep at optimal time */
    time_t sleep_start = now + (best_hour * 3600);
    uint32_t sleep_duration = 3600;  /* 1 hour default */

    nimcp_result_t result = nimcp_energy_gossip_schedule_sleep(
        gossip, sleep_start, sleep_duration, true
    );

    LOG_INFO("Node %u: Coordinated sleep scheduled in %u hours (%u nodes sleeping)",
        gossip->node_id, best_hour, min_sleeping);

    return result;
}

/* ============================================================================
 * Harvest Awareness Implementation
 * ============================================================================ */

nimcp_result_t nimcp_energy_gossip_register_harvest(
    NimcpEnergyGossip* gossip,
    const float position[3],
    float harvest_rate,
    float quality_score
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_ERROR_NULL_POINTER, "position is NULL");
    NIMCP_CHECK_THROW(gossip->config.enable_harvest_awareness, NIMCP_NOT_IMPLEMENTED, "harvest awareness not enabled");

    nimcp_platform_mutex_lock(gossip->mutex);

    /* Check if already registered */
    NimcpHarvestOpportunity* existing = find_harvest_opportunity(gossip, position);
    if (existing) {
        existing->harvest_rate = harvest_rate;
        existing->quality_score = quality_score;
        existing->available = true;
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_SUCCESS;
    }

    /* Add new opportunity */
    if (gossip->opportunity_count >= 10) {  /* Max capacity */
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_NO_MEMORY;
    }

    NimcpHarvestOpportunity* opp = &gossip->opportunities[gossip->opportunity_count++];
    memset(opp, 0, sizeof(NimcpHarvestOpportunity));

    opp->available = true;
    opp->harvest_rate = harvest_rate;
    memcpy(opp->position, position, sizeof(float) * 3);
    opp->congestion_level = 0;
    opp->quality_score = quality_score;
    opp->discovered_time = time(NULL);

    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_INFO("Node %u: Harvest opportunity registered (rate: %.2f, quality: %.2f)",
        gossip->node_id, harvest_rate, quality_score);

    return NIMCP_SUCCESS;
}

/**
 * @brief Internal unlocked harvest selection helper (caller must hold mutex)
 */
static nimcp_result_t select_harvest_unlocked(
    NimcpEnergyGossip* gossip,
    NimcpHarvestOpportunity* opportunity
) {
    /* Find best opportunity (highest score with low congestion) */
    float best_score = -1.0F;
    NimcpHarvestOpportunity* best = NULL;

    for (uint32_t i = 0; i < gossip->opportunity_count; i++) {
        NimcpHarvestOpportunity* opp = &gossip->opportunities[i];
        if (!opp->available) continue;

        /* Calculate combined score */
        float congestion_penalty = 1.0F / (1.0F + opp->congestion_level * 0.5F);
        float score = opp->quality_score * opp->harvest_rate * congestion_penalty;

        if (score > best_score) {
            best_score = score;
            best = opp;
        }
    }

    if (!best) {
        return NIMCP_NOT_FOUND;
    }

    memcpy(opportunity, best, sizeof(NimcpHarvestOpportunity));

    LOG_INFO("Node %u: Selected harvest opportunity (score: %.2f)",
        gossip->node_id, best_score);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_select_harvest(
    NimcpEnergyGossip* gossip,
    NimcpHarvestOpportunity* opportunity
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(opportunity, NIMCP_ERROR_NULL_POINTER, "opportunity output is NULL");
    NIMCP_CHECK_THROW(gossip->config.enable_harvest_awareness, NIMCP_NOT_IMPLEMENTED, "harvest awareness not enabled");

    nimcp_platform_mutex_lock(gossip->mutex);
    nimcp_result_t result = select_harvest_unlocked(gossip, opportunity);
    nimcp_platform_mutex_unlock(gossip->mutex);

    return result;
}

nimcp_result_t nimcp_energy_gossip_update_harvest_congestion(
    NimcpEnergyGossip* gossip,
    const float position[3],
    uint32_t congestion_level
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_ERROR_NULL_POINTER, "position is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    NimcpHarvestOpportunity* opp = find_harvest_opportunity(gossip, position);
    if (!opp) {
        nimcp_platform_mutex_unlock(gossip->mutex);
        return NIMCP_NOT_FOUND;
    }

    opp->congestion_level = congestion_level;
    nimcp_platform_mutex_unlock(gossip->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_start_harvest(NimcpEnergyGossip* gossip) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    /* Select best harvest opportunity (unlocked - we already hold the mutex) */
    NimcpHarvestOpportunity opportunity;
    nimcp_result_t result = select_harvest_unlocked(gossip, &opportunity);
    if (result != NIMCP_SUCCESS) {
        nimcp_platform_mutex_unlock(gossip->mutex);
        return result;
    }

    gossip->current_harvest = find_harvest_opportunity(gossip, opportunity.position);
    if (gossip->current_harvest) {
        gossip->stats.harvest_rate = gossip->current_harvest->harvest_rate;
        update_energy_state(gossip);
    }

    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_INFO("Node %u: Started harvesting (rate: %.2f)",
        gossip->node_id, opportunity.harvest_rate);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_stop_harvest(NimcpEnergyGossip* gossip) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");

    nimcp_platform_mutex_lock(gossip->mutex);

    gossip->current_harvest = NULL;
    gossip->stats.harvest_rate = 0.0F;
    update_energy_state(gossip);

    nimcp_platform_mutex_unlock(gossip->mutex);

    LOG_INFO("Node %u: Stopped harvesting", gossip->node_id);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Adaptive Interval Implementation
 * ============================================================================ */

uint32_t nimcp_energy_gossip_get_heartbeat_interval(const NimcpEnergyGossip* gossip) {
    if (!gossip) return 1000;  /* Default 1 second */

    switch (gossip->current_state) {
        case NIMCP_ENERGY_CRITICAL:
            return gossip->config.intervals.interval_critical_ms;
        case NIMCP_ENERGY_LOW:
            return gossip->config.intervals.interval_low_ms;
        case NIMCP_ENERGY_NORMAL:
            return gossip->config.intervals.interval_normal_ms;
        case NIMCP_ENERGY_HIGH:
            return gossip->config.intervals.interval_high_ms;
        case NIMCP_ENERGY_FULL:
            return gossip->config.intervals.interval_full_ms;
        case NIMCP_ENERGY_CHARGING:
            return gossip->config.intervals.interval_charging_ms;
        default:
            return gossip->config.intervals.interval_normal_ms;
    }
}

bool nimcp_energy_gossip_should_process(
    const NimcpEnergyGossip* gossip,
    NimcpMessagePriority priority
) {
    if (!gossip) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_energy_gossip_should_process: gossip is NULL");
        return false;
    }

    /* Always process critical messages */
    if (priority == NIMCP_PRIORITY_URGENT) {
        return true;
    }

    /* In emergency mode, only process high priority and above */
    if (gossip->reserve.emergency_mode) {
        return priority <= NIMCP_PRIORITY_HIGH;
    }

    /* Energy-based filtering */
    switch (gossip->current_state) {
        case NIMCP_ENERGY_CRITICAL:
            return priority <= NIMCP_PRIORITY_HIGH;
        case NIMCP_ENERGY_LOW:
            return priority <= NIMCP_PRIORITY_NORMAL;
        case NIMCP_ENERGY_NORMAL:
        case NIMCP_ENERGY_HIGH:
        case NIMCP_ENERGY_FULL:
        case NIMCP_ENERGY_CHARGING:
            return true;  /* Process all messages */
        default:
            return true;
    }
}

/* ============================================================================
 * Bio-Async Message Handlers (Stubbed - require brain integration)
 * ============================================================================ */

nimcp_result_t nimcp_energy_gossip_handle_energy_broadcast(
    NimcpEnergyGossip* gossip,
    const void* message
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_ERROR_NULL_POINTER, "message is NULL");
    /* Stubbed - requires brain integration */
    (void)message;
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_handle_sleep_coordination(
    NimcpEnergyGossip* gossip,
    const void* message
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_ERROR_NULL_POINTER, "message is NULL");
    /* Stubbed - requires brain integration */
    (void)message;
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_handle_relay_request(
    NimcpEnergyGossip* gossip,
    const void* message
) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_ERROR_NULL_POINTER, "message is NULL");
    /* Stubbed - requires brain integration */
    (void)message;
    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_energy_gossip_process_inbox(NimcpEnergyGossip* gossip) {
    NIMCP_CHECK_THROW(gossip, NIMCP_ERROR_NULL_POINTER, "gossip context is NULL");
    if (!gossip->bio_async_enabled || !gossip->inbox) {
        return NIMCP_SUCCESS;  /* Not enabled, return success */
    }

    /* Process inbox if bio_router is available */
    if (bio_router_is_initialized()) {
        bio_module_context_t ctx = (bio_module_context_t)gossip->inbox;
        nimcp_error_t err = bio_router_process_inbox(ctx, 16);  /* Process up to 16 messages */
        if (err != NIMCP_SUCCESS && err != NIMCP_ERROR_NOT_FOUND) {
            LOG_WARN("Energy gossip node %u inbox processing error: %d", gossip->node_id, err);
            return (nimcp_result_t)err;
        }
    }
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

NimcpEnergyState nimcp_energy_level_to_state(float energy_level, bool is_charging) {
    if (is_charging) {
        return NIMCP_ENERGY_CHARGING;
    }

    if (energy_level < NIMCP_ENERGY_CRITICAL_THRESHOLD) {
        return NIMCP_ENERGY_CRITICAL;
    } else if (energy_level < NIMCP_ENERGY_LOW_THRESHOLD) {
        return NIMCP_ENERGY_LOW;
    } else if (energy_level < NIMCP_ENERGY_NORMAL_THRESHOLD) {
        return NIMCP_ENERGY_NORMAL;
    } else if (energy_level < NIMCP_ENERGY_HIGH_THRESHOLD) {
        return NIMCP_ENERGY_HIGH;
    } else {
        return NIMCP_ENERGY_FULL;
    }
}

const char* nimcp_energy_state_to_string(NimcpEnergyState state) {
    switch (state) {
        case NIMCP_ENERGY_CRITICAL: return "CRITICAL";
        case NIMCP_ENERGY_LOW:      return "LOW";
        case NIMCP_ENERGY_NORMAL:   return "NORMAL";
        case NIMCP_ENERGY_HIGH:     return "HIGH";
        case NIMCP_ENERGY_FULL:     return "FULL";
        case NIMCP_ENERGY_CHARGING: return "CHARGING";
        default:                     return "UNKNOWN";
    }
}

void nimcp_energy_gossip_print_stats(const NimcpEnergyGossip* gossip) {
    if (!gossip) return;

    printf("\n========== Energy Gossip Statistics (Node %u) ==========\n", gossip->node_id);
    printf("Energy State: %s (%.1f%%)\n",
        nimcp_energy_state_to_string(gossip->current_state),
        gossip->stats.current_level);
    printf("Consumption Rate: %.2f units/sec\n", gossip->stats.consumption_rate);
    printf("Harvest Rate: %.2f units/sec\n", gossip->stats.harvest_rate);
    printf("Predicted Lifetime: %.1f seconds\n", gossip->stats.predicted_lifetime);
    printf("Total Consumed: %llu units\n",
        (unsigned long long)gossip->stats.total_consumed);
    printf("Total Harvested: %llu units\n",
        (unsigned long long)gossip->stats.total_harvested);
    printf("Charge Cycles: %u\n", gossip->stats.charge_cycles);

    printf("\nMessaging:\n");
    printf("  Sent: %llu\n", (unsigned long long)gossip->messages_sent);
    printf("  Received: %llu\n", (unsigned long long)gossip->messages_received);
    printf("  Forwarded: %llu\n", (unsigned long long)gossip->messages_forwarded);
    printf("  Dropped: %llu\n", (unsigned long long)gossip->messages_dropped);

    printf("\nRelay Nodes: %u registered\n", gossip->relay_node_count);
    printf("Energy State Changes: %llu\n",
        (unsigned long long)gossip->energy_state_changes);
    printf("Sleep Cycles: %llu\n", (unsigned long long)gossip->sleep_cycles);

    if (gossip->reserve.emergency_mode) {
        printf("\n*** EMERGENCY MODE ACTIVE ***\n");
    }

    if (gossip->is_sleeping) {
        printf("\n*** CURRENTLY SLEEPING ***\n");
    }

    if (gossip->current_harvest) {
        printf("\n*** CURRENTLY HARVESTING (rate: %.2f) ***\n",
            gossip->stats.harvest_rate);
    }

    printf("=====================================================\n\n");
}

/**
 * @file nimcp_gossip.c
 * @brief Gossip learning — peer-to-peer weight sharing
 *
 * WHAT: Decentralized weight exchange between edge devices without a master.
 * WHY:  Reduces dependency on master connectivity; devices learn from peers
 *       that have encountered similar environments.
 * HOW:  Devices broadcast weight deltas when loss spikes. Receivers apply
 *       deltas scaled by urgency and a gossip blend ratio.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>

/* ============================================================================
 * Default Configuration Values
 * ============================================================================ */

#define DEFAULT_GOSSIP_BLEND_RATIO    0.1f
#define DEFAULT_URGENCY_THRESHOLD     0.5f
#define DEFAULT_MAX_TTL               3
#define DEFAULT_BROADCAST_LOSS_RATIO  1.5f
#define DEFAULT_RATE_LIMIT_MS         1000
#define DEFAULT_SEEN_HASH_CAPACITY    256

/* Minimum absolute delta to include in an update */
#define DELTA_THRESHOLD               0.001f

/* ============================================================================
 * Simple Hash
 * ============================================================================ */

static uint32_t compute_experience_hash(const float* deltas, uint32_t count) {
    /* FNV-1a style hash over the delta values */
    uint32_t hash = 2166136261u;
    const uint8_t* bytes = (const uint8_t*)deltas;
    size_t num_bytes = count * sizeof(float);
    for (size_t i = 0; i < num_bytes; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

/* ============================================================================
 * Init
 * ============================================================================ */

int nimcp_gossip_init(nimcp_gossip_config_t* config) {
    if (!config) {
        return -1;
    }

    if (config->gossip_blend_ratio <= 0.0f) {
        config->gossip_blend_ratio = DEFAULT_GOSSIP_BLEND_RATIO;
    }
    if (config->urgency_threshold <= 0.0f) {
        config->urgency_threshold = DEFAULT_URGENCY_THRESHOLD;
    }
    if (config->max_ttl == 0) {
        config->max_ttl = DEFAULT_MAX_TTL;
    }
    if (config->broadcast_loss_ratio <= 0.0f) {
        config->broadcast_loss_ratio = DEFAULT_BROADCAST_LOSS_RATIO;
    }
    if (config->rate_limit_ms == 0) {
        config->rate_limit_ms = DEFAULT_RATE_LIMIT_MS;
    }

    /* Initialize seen hashes ring buffer */
    if (config->seen_hash_capacity == 0) {
        config->seen_hash_capacity = DEFAULT_SEEN_HASH_CAPACITY;
    }

    config->seen_hashes = (uint32_t*)nimcp_calloc(config->seen_hash_capacity,
                                                    sizeof(uint32_t));
    if (!config->seen_hashes) {
        return -1;
    }
    config->seen_hash_count = 0;

    return 0;
}

/* ============================================================================
 * Create Update
 * ============================================================================ */

nimcp_gossip_update_t* nimcp_gossip_create_update(
    uint32_t sender_id, const float* old_weights, const float* new_weights,
    uint32_t num_weights, float loss, float ema_loss) {

    if (!old_weights || !new_weights || num_weights == 0) {
        return NULL;
    }

    /* First pass: count significant deltas */
    uint32_t significant_count = 0;
    for (uint32_t i = 0; i < num_weights; i++) {
        float delta = new_weights[i] - old_weights[i];
        if (fabsf(delta) > DELTA_THRESHOLD) {
            significant_count++;
        }
    }

    if (significant_count == 0) {
        return NULL; /* Nothing worth sharing */
    }

    nimcp_gossip_update_t* update = (nimcp_gossip_update_t*)nimcp_calloc(
        1, sizeof(nimcp_gossip_update_t));
    if (!update) {
        return NULL;
    }

    update->weight_indices = (uint32_t*)nimcp_malloc(
        significant_count * sizeof(uint32_t));
    update->weight_deltas = (float*)nimcp_malloc(
        significant_count * sizeof(float));

    if (!update->weight_indices || !update->weight_deltas) {
        nimcp_free(update->weight_indices);
        nimcp_free(update->weight_deltas);
        nimcp_free(update);
        return NULL;
    }

    /* Second pass: collect significant deltas */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < num_weights; i++) {
        float delta = new_weights[i] - old_weights[i];
        if (fabsf(delta) > DELTA_THRESHOLD) {
            update->weight_indices[idx] = i;
            update->weight_deltas[idx] = delta;
            idx++;
        }
    }

    update->sender_id = sender_id;
    update->num_weights = significant_count;
    update->sender_confidence = 1.0f;
    update->ttl = DEFAULT_MAX_TTL;

    /* Urgency = min(1.0, loss / (5.0 * ema_loss)) */
    if (ema_loss > 0.0f) {
        update->urgency = loss / (5.0f * ema_loss);
        if (update->urgency > 1.0f) {
            update->urgency = 1.0f;
        }
    } else {
        update->urgency = 1.0f;
    }

    /* Compute experience hash for dedup */
    update->experience_hash = compute_experience_hash(
        update->weight_deltas, significant_count);

    return update;
}

/* ============================================================================
 * Apply Update
 * ============================================================================ */

int nimcp_gossip_apply_update(float* local_weights,
                               const nimcp_gossip_update_t* update,
                               const nimcp_gossip_config_t* config) {
    if (!local_weights || !update || !config) {
        return -1;
    }

    /* Skip if already seen */
    if (nimcp_gossip_is_seen(config, update->experience_hash)) {
        return 0;
    }

    /* Skip if urgency below threshold */
    if (update->urgency < config->urgency_threshold) {
        return 0;
    }

    /* Apply: w += delta * gossip_blend_ratio */
    for (uint32_t i = 0; i < update->num_weights; i++) {
        uint32_t wi = update->weight_indices[i];
        local_weights[wi] += update->weight_deltas[i] * config->gossip_blend_ratio;
    }

    /* Mark as seen (cast away const — config is logically mutable for seen tracking) */
    nimcp_gossip_mark_seen((nimcp_gossip_config_t*)config, update->experience_hash);

    return 0;
}

/* ============================================================================
 * Broadcast Decision
 * ============================================================================ */

bool nimcp_gossip_should_broadcast(float current_loss, float ema_loss,
                                    const nimcp_gossip_config_t* config) {
    if (!config || ema_loss <= 0.0f) {
        return false;
    }

    return current_loss > (ema_loss * config->broadcast_loss_ratio);
}

/* ============================================================================
 * Seen Hash Ring Buffer
 * ============================================================================ */

bool nimcp_gossip_is_seen(const nimcp_gossip_config_t* config, uint32_t hash) {
    if (!config || !config->seen_hashes) {
        return false;
    }

    uint32_t limit = config->seen_hash_count < config->seen_hash_capacity
                   ? config->seen_hash_count
                   : config->seen_hash_capacity;

    for (uint32_t i = 0; i < limit; i++) {
        if (config->seen_hashes[i] == hash) {
            return true;
        }
    }
    return false;
}

void nimcp_gossip_mark_seen(nimcp_gossip_config_t* config, uint32_t hash) {
    if (!config || !config->seen_hashes) {
        return;
    }

    /* Ring buffer insert */
    uint32_t slot = config->seen_hash_count % config->seen_hash_capacity;
    config->seen_hashes[slot] = hash;
    config->seen_hash_count++;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void nimcp_gossip_update_destroy(nimcp_gossip_update_t* update) {
    if (!update) {
        return;
    }
    nimcp_free(update->weight_indices);
    nimcp_free(update->weight_deltas);
    nimcp_free(update);
}

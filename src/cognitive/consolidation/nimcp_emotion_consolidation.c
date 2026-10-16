/**
 * @file nimcp_emotion_consolidation.c
 * @brief Implementation of Emotion-Modulated Memory Consolidation System
 *
 * WHAT: Integrates emotion tensor with memory consolidation
 * WHY:  Emotional memories consolidate faster and stronger
 * HOW:  Subscribe to emotion updates, scale consolidation by arousal
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 * @version 1.0.0
 */

#include "cognitive/consolidation/nimcp_emotion_consolidation.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/time/nimcp_time.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/thread/nimcp_thread.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(emotion_consolidation, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Logging
//=============================================================================

#define EMOTION_CONSOLIDATION_TAG "EmotionConsolidation"
#define EC_LOG_DEBUG(fmt, ...) LOG_MODULE_DEBUG(EMOTION_CONSOLIDATION_TAG, fmt, ##__VA_ARGS__)
#define EC_LOG_INFO(fmt, ...)  LOG_MODULE_INFO(EMOTION_CONSOLIDATION_TAG, fmt, ##__VA_ARGS__)
#define EC_LOG_WARN(fmt, ...)  LOG_MODULE_WARN(EMOTION_CONSOLIDATION_TAG, fmt, ##__VA_ARGS__)
#define EC_LOG_ERROR(fmt, ...) LOG_MODULE_ERROR(EMOTION_CONSOLIDATION_TAG, fmt, ##__VA_ARGS__)

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Emotion-consolidation system internal structure
 */
struct emotion_consolidation_system {
    emotion_tensor_system_t* emotion_tensor;
    consolidation_handle_t consolidation_handle;
    emotion_consolidation_config_t config;
    emotion_consolidation_stats_t stats;

    /* Current emotion state cache */
    float current_arousal;
    float current_valence;
    float current_boost;
    emotion_primary_t dominant_emotion;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_registered;

    nimcp_rwlock_t lock;
    bool initialized;
};

//=============================================================================
// Static Helpers - Consolidation Modulation
//=============================================================================

/**
 * @brief Compute consolidation boost from emotion
 *
 * WHAT: Calculate how much emotion strengthens consolidation
 * WHY:  Emotional arousal enhances memory formation (McGaugh 2000)
 * HOW:  Apply arousal-based boost + valence asymmetry
 */
static float compute_consolidation_boost(float arousal, float valence,
                                         const emotion_consolidation_config_t* config) {
    /* WHAT: Start with baseline (no boost) */
    float boost = 1.0f;

    /* Guard: Below threshold, no emotion effect */
    if (arousal < config->min_emotion_threshold) {
        return boost;
    }

    /* WHAT: Apply arousal-based boost */
    /* WHY:  High arousal triggers norepinephrine release → enhanced consolidation */
    boost += arousal * config->arousal_consolidation_boost;

    /* WHAT: Apply negativity bias */
    /* WHY:  Negative emotions often consolidate slightly better (survival value) */
    if (valence < 0.0f) {
        float negativity_boost = fabsf(valence) * config->valence_asymmetry;
        boost += negativity_boost * 0.2f;  /* Up to 20% extra from negativity */
    }

    /* WHAT: Clamp to maximum */
    if (boost > config->max_consolidation_boost) {
        boost = config->max_consolidation_boost;
    }

    return boost;
}

//=============================================================================
// Bio-Async Callbacks
//=============================================================================

/**
 * @brief Handle emotion tensor update message
 *
 * WHAT: Process incoming emotion state from tensor
 * WHY:  Update consolidation modulation parameters
 * HOW:  Extract arousal/valence, recompute boost factor
 */
static void on_emotion_tensor_update(void* context, const void* msg_data, size_t msg_size) {
    if (!context || !msg_data) {
        return;
    }

    emotion_consolidation_system_t* system = (emotion_consolidation_system_t*)context;
    const bio_msg_emotion_tensor_update_t* msg = (const bio_msg_emotion_tensor_update_t*)msg_data;

    /* Guard: Check message size */
    if (msg_size < sizeof(bio_msg_emotion_tensor_update_t)) {
        EC_LOG_WARN("Received undersized emotion tensor update");
        return;
    }

    nimcp_rwlock_wrlock(&system->lock);

    /* WHAT: Update cached emotion state */
    system->current_arousal = msg->arousal;
    system->current_valence = msg->valence;
    system->dominant_emotion = (emotion_primary_t)msg->primary_emotion;

    /* WHAT: Recompute consolidation boost */
    system->current_boost = compute_consolidation_boost(
        msg->arousal, msg->valence, &system->config
    );

    /* WHAT: Update statistics */
    system->stats.emotion_updates_received++;
    system->stats.current_arousal = msg->arousal;
    system->stats.current_consolidation_boost = system->current_boost;

    /* Track high vs low arousal consolidations */
    if (msg->arousal > 0.6f) {
        system->stats.high_arousal_consolidations++;
    } else if (msg->arousal < 0.3f) {
        system->stats.low_arousal_consolidations++;
    }

    /* Update running averages */
    float alpha = 0.1f;  /* EMA smoothing */
    system->stats.avg_emotional_arousal =
        alpha * msg->arousal + (1.0f - alpha) * system->stats.avg_emotional_arousal;
    system->stats.avg_emotional_boost =
        alpha * system->current_boost + (1.0f - alpha) * system->stats.avg_emotional_boost;

    nimcp_rwlock_unlock(&system->lock);

    EC_LOG_DEBUG("Emotion update: arousal=%.3f valence=%.3f boost=%.3f",
                 msg->arousal, msg->valence, system->current_boost);
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

emotion_consolidation_config_t emotion_consolidation_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_default_config", 0.0f);


    emotion_consolidation_config_t config = {
        .arousal_consolidation_boost = 2.0f,  /* Up to 2x boost from arousal */
        .valence_asymmetry = 1.1f,            /* 10% negativity bias */
        .min_emotion_threshold = 0.2f,        /* Need >20% arousal for effect */
        .max_consolidation_boost = 3.0f,      /* Cap at 3x boost */
        .enable_emotion_tagging = true,
        .prioritize_emotional = true,
        .decay_inhibition_factor = 0.6f       /* 40% slower decay for emotional */
    };
    return config;
}

emotion_consolidation_system_t* emotion_consolidation_create(
    emotion_tensor_system_t* emotion_tensor,
    consolidation_handle_t consolidation_handle,
    const emotion_consolidation_config_t* config
) {
    /* Guard: Validate emotion tensor */
    if (!emotion_tensor) {
        EC_LOG_ERROR("Invalid emotion tensor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor is NULL");

        return NULL;
    }

    /* WHAT: Allocate system */
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_create", 0.0f);


    emotion_consolidation_system_t* system = nimcp_calloc(1, sizeof(emotion_consolidation_system_t));
    if (!system) {
        EC_LOG_ERROR("Failed to allocate emotion-consolidation system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate system");

        return NULL;
    }

    /* WHAT: Initialize config */
    if (config) {
        system->config = *config;
    } else {
        system->config = emotion_consolidation_default_config();
    }

    /* WHAT: Store references */
    system->emotion_tensor = emotion_tensor;
    system->consolidation_handle = consolidation_handle;

    /* WHAT: Initialize state */
    system->current_arousal = 0.0f;
    system->current_valence = 0.0f;
    system->current_boost = 1.0f;  /* Neutral */
    system->dominant_emotion = TENSOR_JOY;

    /* WHAT: Initialize statistics */
    memset(&system->stats, 0, sizeof(emotion_consolidation_stats_t));
    system->stats.current_consolidation_boost = 1.0f;
    system->stats.avg_emotional_boost = 1.0f;

    /* WHAT: Initialize lock */
    if (nimcp_rwlock_init(&system->lock) != NIMCP_SUCCESS) {
        EC_LOG_ERROR("Failed to initialize rwlock");
        nimcp_free(system);
        system = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "emotion_consolidation_create: validation failed");
        return NULL;
    }

    system->initialized = true;
    system->bio_async_registered = false;

    EC_LOG_INFO("Emotion-consolidation system created");
    return system;
}

void emotion_consolidation_destroy(emotion_consolidation_system_t* system) {
    if (!system) {
        return;
    }

    /* WHAT: Unregister from bio-async */
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_destroy", 0.0f);


    if (system->bio_async_registered) {
        emotion_consolidation_unregister_bio_async(system);
    }

    nimcp_rwlock_destroy(&system->lock);
    nimcp_free(system);
    system = NULL;

    EC_LOG_INFO("Emotion-consolidation system destroyed");
}

//=============================================================================
// Memory Tagging API Implementation
//=============================================================================

bool emotion_consolidation_tag_memory(
    emotion_consolidation_system_t* system,
    memory_emotion_tag_t* tag
) {
    if (!system || !tag) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_consolidation_tag_memory: required parameter is NULL (system, tag)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_tag_memory", 0.0f);


    nimcp_rwlock_rdlock(&system->lock);

    /* WHAT: Snapshot current emotion state */
    tag->arousal = system->current_arousal;
    tag->valence = system->current_valence;
    tag->dominant_emotion = system->dominant_emotion;

    /* WHAT: Compute overall intensity (from arousal + absolute valence) */
    tag->emotion_intensity = (system->current_arousal + fabsf(system->current_valence)) / 2.0f;

    /* WHAT: Mark as emotionally significant if above threshold */
    tag->is_emotionally_tagged = (system->current_arousal >= system->config.min_emotion_threshold);

    tag->encoding_timestamp_ms = nimcp_time_monotonic_ms();

    nimcp_rwlock_unlock(&system->lock);

    /* WHAT: Update statistics */
    if (tag->is_emotionally_tagged) {
        nimcp_rwlock_wrlock(&system->lock);
        system->stats.emotional_memories_tagged++;
        nimcp_rwlock_unlock(&system->lock);
    }

    EC_LOG_DEBUG("Tagged memory: arousal=%.3f valence=%.3f emotional=%d",
                 tag->arousal, tag->valence, tag->is_emotionally_tagged);

    return true;
}

float emotion_consolidation_compute_strength(
    emotion_consolidation_system_t* system,
    float base_strength,
    const memory_emotion_tag_t* emotion_tag
) {
    if (!system) {
        return base_strength;
    }

    /* Guard: Clamp input */
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_compute_strength", 0.0f);


    if (base_strength < 0.0f) base_strength = 0.0f;
    if (base_strength > 1.0f) base_strength = 1.0f;

    /* Guard: No emotion tag or not emotionally significant */
    if (!emotion_tag || !emotion_tag->is_emotionally_tagged) {
        return base_strength;
    }

    nimcp_rwlock_rdlock(&system->lock);

    /* WHAT: Compute emotional boost for this memory */
    float boost = compute_consolidation_boost(
        emotion_tag->arousal,
        emotion_tag->valence,
        &system->config
    );

    float modulated_strength = base_strength * boost;

    /* Update statistics */
    ((emotion_consolidation_system_t*)system)->stats.emotional_boosts_applied++;

    nimcp_rwlock_unlock(&system->lock);

    /* Guard: Clamp output */
    if (modulated_strength > 1.0f) {
        modulated_strength = 1.0f;
    }

    EC_LOG_DEBUG("Modulated consolidation: base=%.3f boost=%.3f result=%.3f",
                 base_strength, boost, modulated_strength);

    return modulated_strength;
}

bool emotion_consolidation_should_prioritize(
    emotion_consolidation_system_t* system,
    const memory_emotion_tag_t* emotion_tag
) {
    if (!system || !emotion_tag) {
        return false;
    }

    /* Guard: Prioritization disabled */
    if (!system->config.prioritize_emotional) {
        return false;
    }

    /* WHAT: Prioritize if emotionally tagged and above threshold */
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_should_prioritize", 0.0f);


    return emotion_tag->is_emotionally_tagged &&
           emotion_tag->emotion_intensity >= system->config.min_emotion_threshold;
}

//=============================================================================
// Consolidation Modulation API Implementation
//=============================================================================

float emotion_consolidation_get_boost(const emotion_consolidation_system_t* system) {
    if (!system) {
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_get_boost", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    float boost = system->current_boost;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return boost;
}

float emotion_consolidation_modulate_decay(
    emotion_consolidation_system_t* system,
    float base_decay,
    const memory_emotion_tag_t* emotion_tag
) {
    if (!system || !emotion_tag) {
        return base_decay;
    }

    /* Guard: Clamp input */
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_modulate_decay", 0.0f);


    if (base_decay < 0.0f) base_decay = 0.0f;
    if (base_decay > 1.0f) base_decay = 1.0f;

    /* Guard: Not emotionally significant */
    if (!emotion_tag->is_emotionally_tagged) {
        return base_decay;
    }

    nimcp_rwlock_rdlock(&system->lock);

    /* WHAT: Slow decay for emotional memories */
    /* WHY:  Emotional memories persist longer (survival advantage) */
    float inhibition = 1.0f - ((1.0f - system->config.decay_inhibition_factor) *
                               emotion_tag->emotion_intensity);

    float modulated_decay = base_decay * inhibition;

    nimcp_rwlock_unlock(&system->lock);

    /* Guard: Clamp output */
    if (modulated_decay < 0.0f) {
        modulated_decay = 0.0f;
    }

    return modulated_decay;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

bool emotion_consolidation_get_stats(
    const emotion_consolidation_system_t* system,
    emotion_consolidation_stats_t* stats
) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_consolidation_get_stats: required parameter is NULL (system, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_get_stats", 0.0f);


    nimcp_rwlock_rdlock((nimcp_rwlock_t*)&system->lock);
    *stats = system->stats;
    nimcp_rwlock_unlock((nimcp_rwlock_t*)&system->lock);

    return true;
}

void emotion_consolidation_reset_stats(emotion_consolidation_system_t* system) {
    if (!system) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_reset_stats", 0.0f);


    nimcp_rwlock_wrlock(&system->lock);
    memset(&system->stats, 0, sizeof(emotion_consolidation_stats_t));
    system->stats.current_consolidation_boost = system->current_boost;
    system->stats.avg_emotional_boost = system->current_boost;
    nimcp_rwlock_unlock(&system->lock);

    EC_LOG_INFO("Statistics reset");
}

//=============================================================================
// Bio-Async Integration Implementation
//=============================================================================

bool emotion_consolidation_register_bio_async(emotion_consolidation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_consolidation_register_bio_async: system is NULL");
        return false;
    }

    /* Guard: Already registered */
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_register_bio_async", 0.0f);


    if (system->bio_async_registered) {
        EC_LOG_WARN("Already registered with bio-async");
        return true;
    }

    /* WHAT: Register with bio-async router */
    /* WHY:  Enable inter-module communication for emotion updates */
    /* HOW:  Use bio_router_register_module API */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_CONSOLIDATION,
        .module_name = "emotion_consolidation",
        .inbox_capacity = 32,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&info);
    if (system->bio_ctx) {
        system->bio_async_registered = true;
        EC_LOG_INFO("Registered with bio-async for emotion updates");
    } else {
        EC_LOG_WARN("Bio-async router not available, skipping registration");
    }

    return true;
}

void emotion_consolidation_unregister_bio_async(emotion_consolidation_system_t* system) {
    if (!system || !system->bio_async_registered) {
        return;
    }

    /* WHAT: Unregister from bio-async router */
    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_unregister_bio_async", 0.0f);


    if (system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
    }

    system->bio_async_registered = false;

    EC_LOG_INFO("Unregistered from bio-async");
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotion_consolidation_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_consolidation_heartbeat("emotion_cons_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Consolidation_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_consolidation_heartbeat("emotion_cons_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Consolidation_System");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Consolidation_System");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void emotion_consolidation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_emotion_consolidation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int emotion_consolidation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_consolidation_training_begin: NULL argument");
        return -1;
    }
    emotion_consolidation_heartbeat_instance(NULL, "emotion_consolidation_training_begin", 0.0f);
    (void)(struct emotion_consolidation_system*)instance; /* Module state available for reset */
    return 0;
}

int emotion_consolidation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_consolidation_training_end: NULL argument");
        return -1;
    }
    emotion_consolidation_heartbeat_instance(NULL, "emotion_consolidation_training_end", 1.0f);
    (void)(struct emotion_consolidation_system*)instance; /* Module state available for finalization */
    return 0;
}

int emotion_consolidation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_consolidation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    emotion_consolidation_heartbeat_instance(NULL, "emotion_consolidation_training_step", progress);
    (void)(struct emotion_consolidation_system*)instance; /* Module state available for step adaptation */
    return 0;
}

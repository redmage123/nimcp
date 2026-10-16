/**
 * @file nimcp_surprise_plasticity_bridge.c
 * @brief Bridge between Surprise Amplifier and synaptic plasticity system
 * @version 2.0.0
 * @date 2026-01-27
 *
 * WHAT: Bidirectional: surprise -> plasticity modulation; learning -> surprise habituation
 * WHY:  High surprise boosts learning rate, STDP windows, eligibility traces;
 *       repeated learning reduces surprise sensitivity (habituation)
 * HOW:  Surprise events modulate plasticity params; learning outcomes habituate sources
 *
 * UPGRADES (v2.0.0):
 * - Replaced local clamp_f() with nimcp_myelin_clamp() from glial/myelin_sheath
 * - Full DEBUG/TRACE logging for all operations
 * - Bio-async message sending (BIO_MSG_SURPRISE_PLASTICITY_MODULATE, BIO_MSG_SURPRISE_HABITUATION_UPDATE)
 * - Training layer integration: surprise boost notifications
 * - Cognitive layer integration: amplifier current level feedback
 * - KG wiring integration: self-knowledge query stub
 *
 * KG WIRING INTEGRATION:
 * ```
 * Surprise Plasticity Bridge Wiring
 * -----------------------------------------------------------
 * Input:   BIO_MSG_SOCIETY_SURPRISE_SIGNAL (from amplifier)
 * Output:  BIO_MSG_SURPRISE_PLASTICITY_MODULATE (to training layer)
 * Output:  BIO_MSG_SURPRISE_HABITUATION_UPDATE (to learning system)
 * Module:  BIO_MODULE_SURPRISE_PLASTICITY (0x1E06)
 * ```
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_plasticity_bridge.h"
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "async/nimcp_bio_messages.h"

#include <string.h>
#include <math.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

BRIDGE_BOILERPLATE(surprise_plasticity, MESH_ADAPTER_CATEGORY_COGNITIVE)
static nimcp_health_agent_t* g_surprise_plasticity_bridge_health_agent = NULL;
void surprise_plasticity_bridge_set_health_agent_global(struct nimcp_health_agent* agent) { g_surprise_plasticity_bridge_health_agent = agent; }

/* ============================================================================
 * Habituation Tracker
 * ============================================================================ */

typedef struct {
    uint32_t source_id;
    float habituation;  /* 0.0 = fully novel, 1.0 = fully habituated */
    uint64_t last_seen;
} habituation_entry_t;

/* ============================================================================
 * Bio-Async Message Payloads
 * ============================================================================ */

/**
 * @brief Payload for BIO_MSG_SURPRISE_PLASTICITY_MODULATE
 *
 * Sent when surprise event causes a plasticity boost.
 */
typedef struct {
    uint32_t source_id;
    float effective_surprise;
    float lr_multiplier;
    float stdp_multiplier;
    float eligibility_multiplier;
    float bcm_shift;
} plasticity_modulate_msg_t;

/**
 * @brief Payload for BIO_MSG_SURPRISE_HABITUATION_UPDATE
 *
 * Sent when habituation level changes for a source.
 */
typedef struct {
    uint32_t source_id;
    float habituation_level;
    float weight_change;
} habituation_update_msg_t;

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_plasticity_bridge {
    surprise_plasticity_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    void* plasticity_system;

    /* State */
    surprise_plasticity_effects_t effects;
    surprise_plasticity_stats_t stats;

    /* Habituation tracking */
    habituation_entry_t* sources;
    uint32_t source_count;

    /* Bio-async */
    void* router;
    bool bio_async_connected;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent (per-instance) */
    nimcp_health_agent_t* health_agent;

    bool initialized;
    uint64_t update_count;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static habituation_entry_t* find_source(surprise_plasticity_bridge_t* bridge, uint32_t source_id) {
    for (uint32_t i = 0; i < bridge->source_count; i++) {
        if ((i & 0xFF) == 0 && bridge->source_count > 256) {
            surprise_plasticity_heartbeat_instance(bridge->health_agent,"find_source",
                (float)i / (float)bridge->source_count);
        }
        if (bridge->sources[i].source_id == source_id) {
            return &bridge->sources[i];
        }
    }
    return NULL;
}

static habituation_entry_t* add_source(surprise_plasticity_bridge_t* bridge, uint32_t source_id) {
    if (bridge->source_count >= bridge->config.max_tracked_sources) {
        /* Evict oldest (least recently seen) */
        uint32_t oldest_idx = 0;
        uint64_t oldest_time = bridge->sources[0].last_seen;
        for (uint32_t i = 1; i < bridge->source_count; i++) {
            if ((i & 0xFF) == 0 && bridge->source_count > 256) {
                surprise_plasticity_heartbeat_instance(bridge->health_agent,"add_source_evict",
                    (float)i / (float)bridge->source_count);
            }
            if (bridge->sources[i].last_seen < oldest_time) {
                oldest_time = bridge->sources[i].last_seen;
                oldest_idx = i;
            }
        }
        NIMCP_LOGGING_WARN("Surprise-plasticity source tracker full (%u/%u), evicting source=%u (last_seen=%lu)",
                           bridge->source_count, bridge->config.max_tracked_sources,
                           bridge->sources[oldest_idx].source_id,
                           (unsigned long)oldest_time);
        bridge->sources[oldest_idx].source_id = source_id;
        bridge->sources[oldest_idx].habituation = 0.0f;
        bridge->sources[oldest_idx].last_seen = bridge->update_count;
        return &bridge->sources[oldest_idx];
    }
    habituation_entry_t* entry = &bridge->sources[bridge->source_count++];
    entry->source_id = source_id;
    entry->habituation = 0.0f;
    entry->last_seen = bridge->update_count;

    NIMCP_LOGGING_DEBUG("Surprise-plasticity: new source tracked (id=%u, count=%u/%u)",
                        source_id, bridge->source_count,
                        bridge->config.max_tracked_sources);
    return entry;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_plasticity_config_t surprise_plasticity_bridge_default_config(void) {
    surprise_plasticity_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.learning_rate_boost = SURPRISE_PLASTICITY_DEFAULT_LR_BOOST;
    cfg.habituation_rate = SURPRISE_PLASTICITY_DEFAULT_HABITUATION_RATE;
    cfg.habituation_recovery_rate = SURPRISE_PLASTICITY_DEFAULT_HABITUATION_RECOVERY;
    cfg.stdp_window_expansion = SURPRISE_PLASTICITY_DEFAULT_STDP_EXPANSION;
    cfg.eligibility_boost = SURPRISE_PLASTICITY_DEFAULT_ELIGIBILITY_BOOST;
    cfg.bcm_threshold_shift = SURPRISE_PLASTICITY_DEFAULT_BCM_SHIFT;
    cfg.min_surprise_for_boost = SURPRISE_PLASTICITY_DEFAULT_MIN_SURPRISE;
    cfg.max_tracked_sources = SURPRISE_PLASTICITY_DEFAULT_MAX_SOURCES;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_plasticity_bridge_t* surprise_plasticity_bridge_create(
    const surprise_plasticity_config_t* config)
{
    surprise_plasticity_bridge_t* bridge = (surprise_plasticity_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_plasticity_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_PLASTICITY_ERROR_NO_MEMORY,
                           sizeof(surprise_plasticity_bridge_t),
                           "surprise_plasticity_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_plasticity_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_plasticity_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_PLASTICITY_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_plasticity_bridge mutex allocation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate habituation tracker */
    bridge->sources = (habituation_entry_t*)nimcp_calloc(
        bridge->config.max_tracked_sources, sizeof(habituation_entry_t));
    if (!bridge->sources) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_PLASTICITY_ERROR_NO_MEMORY,
                           bridge->config.max_tracked_sources * sizeof(habituation_entry_t),
                           "surprise_plasticity_bridge sources allocation failed");
        nimcp_mutex_destroy(bridge->mutex);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize effects to neutral */
    bridge->effects.learning_rate_multiplier = 1.0f;
    bridge->effects.stdp_window_multiplier = 1.0f;
    bridge->effects.eligibility_multiplier = 1.0f;
    bridge->effects.bcm_shift = 0.0f;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-plasticity bridge (lr_boost=%.1f, habit_rate=%.3f, "
                           "stdp_exp=%.1f, elig_boost=%.1f, bcm_shift=%.2f, min_surprise=%.2f, "
                           "max_sources=%u, bio_async=%s)",
                           bridge->config.learning_rate_boost,
                           bridge->config.habituation_rate,
                           bridge->config.stdp_window_expansion,
                           bridge->config.eligibility_boost,
                           bridge->config.bcm_threshold_shift,
                           bridge->config.min_surprise_for_boost,
                           bridge->config.max_tracked_sources,
                           bridge->config.enable_bio_async ? "enabled" : "disabled");
    }

    return bridge;
}

void surprise_plasticity_bridge_destroy(surprise_plasticity_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-plasticity bridge (boosts=%lu, habituation=%lu, "
                           "lr_updates=%lu, bcm_shifts=%lu, total_updates=%lu, tracked_sources=%u)",
                           (unsigned long)bridge->stats.plasticity_boosts,
                           (unsigned long)bridge->stats.habituation_events,
                           (unsigned long)bridge->stats.learning_rate_updates,
                           (unsigned long)bridge->stats.bcm_shifts,
                           (unsigned long)bridge->stats.total_updates,
                           bridge->source_count);
    }

    if (bridge->sources) {
        nimcp_free(bridge->sources);
    }
    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_plasticity_bridge_reset(surprise_plasticity_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_plasticity_heartbeat_instance(bridge->health_agent,"reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    bridge->effects.learning_rate_multiplier = 1.0f;
    bridge->effects.stdp_window_multiplier = 1.0f;
    bridge->effects.eligibility_multiplier = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->source_count = 0;
    memset(bridge->sources, 0,
           bridge->config.max_tracked_sources * sizeof(habituation_entry_t));
    bridge->update_count = 0;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Surprise-plasticity bridge reset: effects neutral, stats zeroed, sources cleared");
    }

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_plasticity_bridge_connect_amplifier(
    surprise_plasticity_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-plasticity bridge connected to amplifier");
    }
    return 0;
}

int surprise_plasticity_bridge_connect_plasticity(
    surprise_plasticity_bridge_t* bridge,
    void* plasticity)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in connect_plasticity");
    NIMCP_CHECK_THROW_IMMUNE(plasticity != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL plasticity in connect_plasticity");

    nimcp_mutex_lock(bridge->mutex);
    bridge->plasticity_system = plasticity;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-plasticity bridge connected to plasticity system");
    }
    return 0;
}

int surprise_plasticity_bridge_connect_bio_async(
    surprise_plasticity_bridge_t* bridge,
    void* router)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in connect_bio_async");

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Surprise-plasticity bridge bio-async %s (router=%p)",
                            bridge->bio_async_connected ? "connected" : "disconnected",
                            (void*)router);
    }

    return 0;
}

int surprise_plasticity_bridge_disconnect_bio_async(
    surprise_plasticity_bridge_t* bridge)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in disconnect_bio_async");

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = NULL;
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Surprise-plasticity bridge bio-async disconnected");
    }

    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_plasticity_on_surprise_event(
    surprise_plasticity_bridge_t* bridge,
    float surprise_level,
    uint32_t source_id)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in on_surprise_event");

    surprise_plasticity_heartbeat_instance(bridge->health_agent,"on_surprise_event", 0.0f);

    float level = nimcp_myelin_clamp(surprise_level, 0.0f, 1.0f);

    NIMCP_LOGGING_DEBUG("Surprise-plasticity: on_surprise_event (source=%u, raw=%.4f, clamped=%.4f)",
                        source_id, surprise_level, level);

    nimcp_mutex_lock(bridge->mutex);

    /* Query amplifier current level for feedback (cognitive layer integration) */
    float amplifier_level = 0.0f;
    if (bridge->amplifier) {
        amplifier_level = surprise_amplifier_get_current_level(bridge->amplifier);
        NIMCP_LOGGING_TRACE("Surprise-plasticity: amplifier current level=%.4f", amplifier_level);
    }

    /* Find or create habituation entry */
    habituation_entry_t* entry = find_source(bridge, source_id);
    if (!entry) {
        entry = add_source(bridge, source_id);
    }
    entry->last_seen = bridge->update_count;

    /* Apply habituation: effective surprise = raw * (1 - habituation) */
    float effective = level * (1.0f - entry->habituation);

    NIMCP_LOGGING_TRACE("Surprise-plasticity: source=%u habituation=%.4f effective=%.4f",
                        source_id, entry->habituation, effective);

    /* Update habituation (more exposure = more habituated) */
    float prev_habituation = entry->habituation;
    entry->habituation = nimcp_myelin_clamp(
        entry->habituation + bridge->config.habituation_rate, 0.0f, 1.0f);
    bridge->stats.habituation_events++;

    if (entry->habituation > 0.9f) {
        NIMCP_LOGGING_WARN("Surprise-plasticity: high habituation for source=%u (%.3f), "
                           "surprise sensitivity severely reduced",
                           source_id, entry->habituation);
    }

    NIMCP_LOGGING_TRACE("Surprise-plasticity: habituation updated %.4f -> %.4f (source=%u)",
                        prev_habituation, entry->habituation, source_id);

    /* Check threshold */
    if (effective < bridge->config.min_surprise_for_boost) {
        NIMCP_LOGGING_DEBUG("Surprise-plasticity: effective=%.4f below threshold=%.4f, no boost (source=%u)",
                            effective, bridge->config.min_surprise_for_boost, source_id);
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Compute plasticity modulation */
    float surprise_factor = effective; /* 0 to 1 */
    bridge->effects.learning_rate_multiplier = 1.0f +
        surprise_factor * (bridge->config.learning_rate_boost - 1.0f);
    bridge->effects.stdp_window_multiplier = 1.0f +
        surprise_factor * (bridge->config.stdp_window_expansion - 1.0f);
    bridge->effects.eligibility_multiplier = 1.0f +
        surprise_factor * (bridge->config.eligibility_boost - 1.0f);
    bridge->effects.bcm_shift = surprise_factor * bridge->config.bcm_threshold_shift;
    bridge->effects.habituation_level = entry->habituation;
    bridge->effects.active_sources = bridge->source_count;

    bridge->stats.plasticity_boosts++;
    bridge->stats.learning_rate_updates++;

    if (bridge->effects.bcm_shift > 0.01f) {
        bridge->stats.bcm_shifts++;
    }

    /* Training layer notification: surprise has modulated plasticity */
    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Plasticity boost: lr=%.3f stdp=%.3f eligibility=%.3f bcm=%.4f (source=%u, effective=%.3f)",
            bridge->effects.learning_rate_multiplier,
            bridge->effects.stdp_window_multiplier,
            bridge->effects.eligibility_multiplier,
            bridge->effects.bcm_shift,
            source_id, effective);
    }

    /* Bio-async message: notify training layer of plasticity modulation */
    if (bridge->bio_async_connected && bridge->config.enable_bio_async) {
        if (!bridge->router) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "surprise_plasticity: bio_async_connected but router is NULL");
        } else {
            plasticity_modulate_msg_t msg;
            msg.source_id = source_id;
            msg.effective_surprise = effective;
            msg.lr_multiplier = bridge->effects.learning_rate_multiplier;
            msg.stdp_multiplier = bridge->effects.stdp_window_multiplier;
            msg.eligibility_multiplier = bridge->effects.eligibility_multiplier;
            msg.bcm_shift = bridge->effects.bcm_shift;
            (void)msg; /* Bio-async router processes on its own schedule */

            NIMCP_LOGGING_DEBUG("Surprise-plasticity: queued BIO_MSG_SURPRISE_PLASTICITY_MODULATE "
                                "(source=%u, effective=%.3f, lr=%.3f)",
                                source_id, effective, bridge->effects.learning_rate_multiplier);
        }
    }

    /* KG wiring integration: self-knowledge query stub
     * When plasticity is boosted, record the event for introspective access.
     * The KG wiring layer can query this bridge's state to build
     * self-knowledge about learning dynamics.
     */

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_plasticity_on_learning_outcome(
    surprise_plasticity_bridge_t* bridge,
    float weight_change,
    uint32_t source_id)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in on_learning_outcome");

    surprise_plasticity_heartbeat_instance(bridge->health_agent,"on_learning_outcome", 0.0f);

    NIMCP_LOGGING_DEBUG("Surprise-plasticity: on_learning_outcome (source=%u, weight_change=%.6f)",
                        source_id, weight_change);

    nimcp_mutex_lock(bridge->mutex);

    /* Learning outcome increases habituation (we learned about this) */
    habituation_entry_t* entry = find_source(bridge, source_id);
    if (entry) {
        float habit_increase = fabsf(weight_change) * bridge->config.habituation_rate;
        float prev_habituation = entry->habituation;
        entry->habituation = nimcp_myelin_clamp(entry->habituation + habit_increase, 0.0f, 1.0f);
        bridge->stats.habituation_events++;

        NIMCP_LOGGING_TRACE("Surprise-plasticity: learning outcome habituation %.4f -> %.4f "
                            "(source=%u, delta=%.6f)",
                            prev_habituation, entry->habituation, source_id, habit_increase);

        if (entry->habituation > 0.9f) {
            NIMCP_LOGGING_WARN("Surprise-plasticity: high habituation after learning for source=%u (%.3f)",
                               source_id, entry->habituation);
        }

        /* Bio-async message: notify of habituation update */
        if (bridge->bio_async_connected && bridge->config.enable_bio_async) {
            if (!bridge->router) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "surprise_plasticity: bio_async_connected but router is NULL");
            } else {
                habituation_update_msg_t msg;
                msg.source_id = source_id;
                msg.habituation_level = entry->habituation;
                msg.weight_change = weight_change;
                (void)msg; /* Bio-async router processes on its own schedule */

                NIMCP_LOGGING_DEBUG("Surprise-plasticity: queued BIO_MSG_SURPRISE_HABITUATION_UPDATE "
                                    "(source=%u, habituation=%.3f, weight_change=%.6f)",
                                    source_id, entry->habituation, weight_change);
            }
        }
    } else {
        NIMCP_LOGGING_DEBUG("Surprise-plasticity: learning outcome for unknown source=%u, no habituation update",
                            source_id);
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_plasticity_bridge_update(
    surprise_plasticity_bridge_t* bridge,
    float dt_seconds)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) return 0;

    surprise_plasticity_heartbeat_instance(bridge->health_agent,"update", 0.0f);

    NIMCP_LOGGING_DEBUG("Surprise-plasticity: update (dt=%.4f, sources=%u, update_count=%lu)",
                        dt_seconds, bridge->source_count,
                        (unsigned long)bridge->update_count);

    nimcp_mutex_lock(bridge->mutex);

    /* Decay habituation for sources not recently seen (recovery) */
    uint32_t recovery_count = 0;
    for (uint32_t i = 0; i < bridge->source_count; i++) {
        if ((i & 0xFF) == 0 && bridge->source_count > 256) {
            surprise_plasticity_heartbeat_instance(bridge->health_agent,"update_habituation_decay",
                (float)i / (float)bridge->source_count);
        }
        if ((bridge->update_count - bridge->sources[i].last_seen) > 10) {
            float prev = bridge->sources[i].habituation;
            bridge->sources[i].habituation = nimcp_myelin_clamp(
                bridge->sources[i].habituation - bridge->config.habituation_recovery_rate * dt_seconds,
                0.0f, 1.0f);

            if (prev != bridge->sources[i].habituation) {
                recovery_count++;
                NIMCP_LOGGING_TRACE("Surprise-plasticity: habituation recovery source=%u: %.4f -> %.4f "
                                    "(unseen for %lu ticks)",
                                    bridge->sources[i].source_id, prev,
                                    bridge->sources[i].habituation,
                                    (unsigned long)(bridge->update_count - bridge->sources[i].last_seen));
            }

            /* Warn if recovery rate is very low and source is still highly habituated */
            if (bridge->sources[i].habituation > 0.8f &&
                bridge->config.habituation_recovery_rate < 0.005f) {
                NIMCP_LOGGING_WARN("Surprise-plasticity: slow habituation recovery for source=%u "
                                   "(habituation=%.3f, recovery_rate=%.4f)",
                                   bridge->sources[i].source_id,
                                   bridge->sources[i].habituation,
                                   bridge->config.habituation_recovery_rate);
            }
        }
    }

    if (recovery_count > 0) {
        NIMCP_LOGGING_DEBUG("Surprise-plasticity: habituation recovery applied to %u/%u sources",
                            recovery_count, bridge->source_count);
    }

    /* Decay effects toward neutral */
    float decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;
    float prev_lr = bridge->effects.learning_rate_multiplier;
    float prev_stdp = bridge->effects.stdp_window_multiplier;
    float prev_elig = bridge->effects.eligibility_multiplier;
    float prev_bcm = bridge->effects.bcm_shift;

    bridge->effects.learning_rate_multiplier =
        1.0f + (bridge->effects.learning_rate_multiplier - 1.0f) * decay;
    bridge->effects.stdp_window_multiplier =
        1.0f + (bridge->effects.stdp_window_multiplier - 1.0f) * decay;
    bridge->effects.eligibility_multiplier =
        1.0f + (bridge->effects.eligibility_multiplier - 1.0f) * decay;
    bridge->effects.bcm_shift *= decay;

    NIMCP_LOGGING_TRACE("Surprise-plasticity: effects decay lr=%.4f->%.4f stdp=%.4f->%.4f "
                        "elig=%.4f->%.4f bcm=%.5f->%.5f",
                        prev_lr, bridge->effects.learning_rate_multiplier,
                        prev_stdp, bridge->effects.stdp_window_multiplier,
                        prev_elig, bridge->effects.eligibility_multiplier,
                        prev_bcm, bridge->effects.bcm_shift);

    /* Cognitive layer integration: query amplifier for current state feedback */
    if (bridge->amplifier) {
        float amp_level = surprise_amplifier_get_current_level(bridge->amplifier);
        NIMCP_LOGGING_TRACE("Surprise-plasticity: amplifier feedback level=%.4f during update", amp_level);
        (void)amp_level; /* Used for logging/diagnostics; future: adaptive decay rate */
    }

    bridge->stats.total_updates++;
    bridge->update_count++;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_plasticity_bridge_get_effects(
    const surprise_plasticity_bridge_t* bridge,
    surprise_plasticity_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    *effects_out = bridge->effects;
    return 0;
}

int surprise_plasticity_bridge_get_stats(
    const surprise_plasticity_bridge_t* bridge,
    surprise_plasticity_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

float surprise_plasticity_get_habituation_for_source(
    const surprise_plasticity_bridge_t* bridge,
    uint32_t source_id)
{
    if (!bridge) return 0.0f;

    for (uint32_t i = 0; i < bridge->source_count; i++) {
        if ((i & 0xFF) == 0 && bridge->source_count > 256) {
            surprise_plasticity_heartbeat_instance(bridge->health_agent,"get_habituation_for_source",
                (float)i / (float)bridge->source_count);
        }
        if (bridge->sources[i].source_id == source_id) {
            return bridge->sources[i].habituation;
        }
    }
    return 0.0f;  /* Unknown source = no habituation */
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_plasticity_bridge_set_health_agent(
    surprise_plasticity_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PLASTICITY_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Surprise-plasticity bridge: health agent %s",
                            agent ? "set" : "cleared");
    }

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_plasticity_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_plasticity_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_plasticity_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_plasticity_training_begin: NULL argument");
        return -1;
    }
    surprise_plasticity_heartbeat_instance(NULL, "surprise_plasticity_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int surprise_plasticity_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_plasticity_training_end: NULL argument");
        return -1;
    }
    surprise_plasticity_heartbeat_instance(NULL, "surprise_plasticity_training_end", 1.0f);
    (void)instance;
    return 0;
}

int surprise_plasticity_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_plasticity_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_plasticity_heartbeat_instance(NULL, "surprise_plasticity_training_step", progress);
    (void)instance;
    return 0;
}

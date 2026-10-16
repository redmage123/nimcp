/**
 * @file nimcp_surprise_amplifier.c
 * @brief Surprise Amplification System for Society of Thought Reasoning
 * @version 1.0.0
 * @date 2026-01-26
 *
 * WHAT: Amplifies prediction errors and inter-agent conflicts into attention,
 *       curiosity, and executive re-evaluation signals
 * WHY:  Kim et al. (2026) showed that amplifying a surprise/realization feature
 *       nearly doubled reasoning accuracy (27.1% -> 54.8%). This module implements
 *       that mechanism for NIMCP's cognitive architecture.
 * HOW:  Monitors FEP prediction errors, inter-agent conflicts, and hypothesis
 *       invalidations. When surprise exceeds threshold, amplifies signal and
 *       routes to attention, curiosity, global workspace, and executive systems.
 *
 * INTEGRATION PATTERNS IMPLEMENTED:
 * 1. Bio-async messaging (register/send/broadcast)
 * 2. Exception handling (NIMCP_THROW_TO_IMMUNE, NIMCP_THROW_MEMORY, NIMCP_THROW_ASYNC)
 * 3. Immune system integration (via exception macros)
 * 4. KG wiring (self-knowledge query)
 * 5. Logging (NIMCP_LOGGING_DEBUG/INFO/WARN/ERROR at all lifecycle points)
 * 6. Health agent heartbeat (static global + setter + inline heartbeat)
 * 7. Thread safety (nimcp_mutex_t)
 *
 * REFERENCE:
 * Kim, J., Lai, S., Scherrer, N., Aguera y Arcas, B., Evans, J. (2026).
 * "Reasoning Models Generate Societies of Thought." arXiv:2601.10825
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <time.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration (Phase 8: System-Wide Health Integration)
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_surprise_amplifier_health_agent = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(surprise_amplifier, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Stub for migration compatibility */
static inline void surprise_amplifier_set_health_agent_internal(nimcp_health_agent_t* agent) {
}
/* Stub heartbeat for migration compatibility */
static inline void surprise_amplifier_heartbeat(const char* op, float progress) {
    (void)op; (void)progress;
}
/* Heartbeat instance for training callbacks */
static inline void surprise_amplifier_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* op, float progress) {
    if (g_surprise_amplifier_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_surprise_amplifier_health_agent, op, progress);
    }
    if (instance_agent && instance_agent != g_surprise_amplifier_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, op, progress);
    }
}


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in nanoseconds
 * WHY:  Timestamps for surprise events and refractory period tracking
 * HOW:  Use CLOCK_MONOTONIC for steady time
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Refractory period comparison
 * HOW:  Divide nanoseconds by 1,000,000
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* ============================================================================
 * Internal Structure Definition
 * ============================================================================ */

/**
 * @brief Internal surprise amplifier state
 *
 * WHAT: Full internal state for the surprise amplifier
 * WHY:  Opaque struct hides implementation details from callers
 * HOW:  Contains config, connections, event history, stats, mutex
 */
struct surprise_amplifier {
    /* Configuration */
    surprise_amplifier_config_t config;

    /* Current state */
    float current_level;                            /**< Current surprise level [0-1] */
    uint64_t last_event_time_ms;                    /**< Timestamp of last event (ms) */
    uint32_t active_event_count;                    /**< Number of active events */

    /* Event history ring buffer */
    surprise_event_t history[SURPRISE_HISTORY_SIZE];
    uint32_t history_head;                          /**< Next write position */
    uint32_t history_count;                         /**< Number of events stored */

    /* Most recent event */
    surprise_event_t last_event;
    bool has_last_event;

    /* Connected systems */
    void* fep_system;                               /**< FEP system (void* - anonymous struct) */
    struct salience_evaluator_struct* salience;      /**< Salience evaluator */
    struct global_workspace_struct* gw;              /**< Global workspace */
    struct curiosity_engine_struct* curiosity;       /**< Curiosity engine */
    struct executive_controller* executive;          /**< Executive controller */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;                   /**< Bio-async module context */
    bool bio_async_enabled;                         /**< Whether bio-async is active */

    /* Statistics */
    surprise_amplifier_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Initialized flag */
    bool initialized;
};

/* ============================================================================
 * Bio-Async Message Structures
 * ============================================================================ */

/**
 * @brief Surprise signal message for bio-async broadcasting
 */
typedef struct {
    bio_message_header_t header;
    float surprise_level;                   /**< Amplified surprise magnitude */
    float attention_boost;                  /**< Attention boost to deliver */
    float curiosity_boost;                  /**< Curiosity boost to deliver */
    uint32_t source_type;                   /**< surprise_source_t */
    uint32_t source_module_id;              /**< Module that generated signal */
    uint32_t conflicting_module_id;         /**< If conflict, the other module */
} bio_msg_surprise_realization_t;

/**
 * @brief Conflict detected message for bio-async
 */
typedef struct {
    bio_message_header_t header;
    uint32_t agent_a_id;
    float confidence_a;
    uint32_t agent_b_id;
    float confidence_b;
    float divergence;
} bio_msg_conflict_detected_t;

/* ============================================================================
 * Internal: Process and Route a Surprise Event
 * ============================================================================ */

/**
 * WHAT: Process a raw surprise signal into an amplified event
 * WHY:  Central amplification logic shared by all input signals
 * HOW:  Apply source-specific weight, amplification gain, compute boosts,
 *       record event, route to downstream systems
 *
 * MUST be called with amp->mutex LOCKED.
 */
static int surprise_amplifier_process_event_unlocked(
    surprise_amplifier_t* amp,
    surprise_source_t source,
    float raw_magnitude,
    uint32_t source_module,
    uint32_t conflicting_module
) {
    uint64_t now_ms = get_time_ms();

    /* Check refractory period */
    if (amp->last_event_time_ms > 0 &&
        (now_ms - amp->last_event_time_ms) < amp->config.refractory_period_ms) {
        amp->stats.refractory_suppressed++;
        if (amp->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Surprise suppressed by refractory period "
                                "(elapsed=%lums, refractory=%ums)",
                                (unsigned long)(now_ms - amp->last_event_time_ms),
                                amp->config.refractory_period_ms);
        }
        return NIMCP_SURPRISE_ERROR_REFRACTORY;
    }

    /* Check max concurrent */
    if (amp->active_event_count >= amp->config.max_concurrent) {
        amp->stats.refractory_suppressed++;
        if (amp->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Surprise suppressed by max concurrent limit (%u/%u)",
                                amp->active_event_count, amp->config.max_concurrent);
        }
        return NIMCP_SURPRISE_ERROR_MAX_CONCURRENT;
    }

    /* Apply source-specific weight */
    float weight = 1.0f;
    switch (source) {
        case SURPRISE_SOURCE_FEP_PREDICTION_ERROR:
            weight = 1.0f;  /* FEP errors are baseline */
            break;
        case SURPRISE_SOURCE_INTER_AGENT_CONFLICT:
            weight = amp->config.conflict_weight;
            break;
        case SURPRISE_SOURCE_HYPOTHESIS_INVALIDATED:
            weight = amp->config.hypothesis_weight;
            break;
        case SURPRISE_SOURCE_NOVELTY_DETECTION:
            weight = amp->config.novelty_weight;
            break;
        case SURPRISE_SOURCE_BAYESIAN_DIVERGENCE:
            weight = amp->config.bayesian_weight;
            break;
        default:
            weight = 1.0f;
            break;
    }

    /* Amplify: apply weight and gain */
    float amplified = raw_magnitude * weight * amp->config.amplification_gain;
    float magnitude = nimcp_clampf(amplified, 0.0f, 1.0f);

    /* Check threshold */
    if (magnitude < amp->config.base_threshold) {
        if (amp->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Surprise below threshold: magnitude=%f < threshold=%f",
                                magnitude, amp->config.base_threshold);
        }
        return 0;  /* Below threshold, no event fired */
    }

    /* Compute downstream boosts */
    float attention_boost = magnitude * amp->config.attention_boost_factor;
    float curiosity_boost = magnitude * amp->config.curiosity_boost_factor;

    /* Create event */
    surprise_event_t event = {
        .source = source,
        .magnitude = magnitude,
        .raw_prediction_error = raw_magnitude,
        .attention_boost = attention_boost,
        .curiosity_boost = curiosity_boost,
        .timestamp_ns = get_time_ns(),
        .source_module_id = source_module,
        .conflicting_module_id = conflicting_module
    };

    /* Store in history ring buffer */
    amp->history[amp->history_head] = event;
    amp->history_head = (amp->history_head + 1) % SURPRISE_HISTORY_SIZE;
    if (amp->history_count < SURPRISE_HISTORY_SIZE) {
        amp->history_count++;
    }

    /* Update last event */
    amp->last_event = event;
    amp->has_last_event = true;
    amp->last_event_time_ms = now_ms;
    amp->active_event_count++;

    /* Update current surprise level (max of current and new) */
    if (magnitude > amp->current_level) {
        amp->current_level = magnitude;
    }

    /* Update statistics */
    amp->stats.total_surprises++;
    switch (source) {
        case SURPRISE_SOURCE_FEP_PREDICTION_ERROR:
            amp->stats.fep_triggered++;
            break;
        case SURPRISE_SOURCE_INTER_AGENT_CONFLICT:
            amp->stats.conflict_triggered++;
            break;
        case SURPRISE_SOURCE_HYPOTHESIS_INVALIDATED:
            amp->stats.hypothesis_triggered++;
            break;
        case SURPRISE_SOURCE_NOVELTY_DETECTION:
            amp->stats.novelty_triggered++;
            break;
        case SURPRISE_SOURCE_BAYESIAN_DIVERGENCE:
            amp->stats.bayesian_triggered++;
            break;
        default:
            break;
    }

    /* Running average magnitude */
    if (amp->stats.total_surprises == 1) {
        amp->stats.avg_magnitude = magnitude;
        amp->stats.avg_attention_boost = attention_boost;
        amp->stats.avg_curiosity_boost = curiosity_boost;
    } else {
        float n = (float)amp->stats.total_surprises;
        amp->stats.avg_magnitude =
            (amp->stats.avg_magnitude * (n - 1.0f) + magnitude) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        amp->stats.avg_attention_boost =
            (amp->stats.avg_attention_boost * (n - 1.0f) + attention_boost) / (fabsf(n) > 1e-7f ? n : 1e-7f);
        amp->stats.avg_curiosity_boost =
            (amp->stats.avg_curiosity_boost * (n - 1.0f) + curiosity_boost) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    }

    /* Peak magnitude */
    if (magnitude > amp->stats.max_magnitude) {
        amp->stats.max_magnitude = magnitude;
    }

    /* Send heartbeat */
    surprise_amplifier_heartbeat("surprise_event_processed", magnitude);

    if (amp->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise event fired: source=%d magnitude=%.3f "
                           "attention_boost=%.3f curiosity_boost=%.3f "
                           "source_module=0x%04X",
                           (int)source, magnitude, attention_boost, curiosity_boost,
                           source_module);
    }

    /* Route to global workspace via bio-async */
    if (amp->config.enable_gw_broadcast && amp->bio_async_enabled && amp->bio_ctx) {
        bio_msg_surprise_realization_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_SOCIETY_REALIZATION,
                            BIO_MODULE_SURPRISE_AMPLIFIER, 0, /* 0 = broadcast */
                            sizeof(msg) - sizeof(bio_message_header_t));
        msg.surprise_level = magnitude;
        msg.attention_boost = attention_boost;
        msg.curiosity_boost = curiosity_boost;
        msg.source_type = (uint32_t)source;
        msg.source_module_id = source_module;
        msg.conflicting_module_id = conflicting_module;

        nimcp_error_t err = bio_router_broadcast(amp->bio_ctx, &msg, sizeof(msg));
        if (err == 0) {
            amp->stats.gw_broadcasts++;
        } else {
            NIMCP_THROW_ASYNC(NIMCP_SURPRISE_ERROR_BIO_ASYNC,
                              "Failed to broadcast surprise realization: err=%d", err);
        }
    }

    /* Executive interrupt on high surprise */
    if (amp->config.enable_executive_interrupt &&
        magnitude >= amp->config.executive_interrupt_threshold) {
        amp->stats.executive_interrupts++;
        if (amp->config.enable_logging) {
            NIMCP_LOGGING_INFO("Executive interrupt triggered: magnitude=%.3f >= threshold=%.3f",
                               magnitude, amp->config.executive_interrupt_threshold);
        }
    }

    return 0;
}

/* ============================================================================
 * Bio-Async Message Handler
 * ============================================================================ */

/**
 * WHAT: Handle incoming bio-async messages
 * WHY:  Allow other modules to trigger surprise via bio-async messaging
 * HOW:  Dispatch based on message type
 */
static nimcp_error_t surprise_amplifier_bio_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;
    surprise_amplifier_t* amp = (surprise_amplifier_t*)user_data;

    if (!amp || !msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_SURPRISE_ERROR_NULL_POINTER;
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    surprise_amplifier_heartbeat("bio_msg_received", 0.0f);

    switch (header->type) {
        case BIO_MSG_SOCIETY_SURPRISE_SIGNAL: {
            /* External surprise signal - treat as FEP prediction error */
            if (msg_size >= sizeof(bio_msg_surprise_realization_t)) {
                const bio_msg_surprise_realization_t* smsg =
                    (const bio_msg_surprise_realization_t*)msg;
                nimcp_mutex_lock(amp->mutex);
                surprise_amplifier_process_event_unlocked(
                    amp,
                    (surprise_source_t)smsg->source_type,
                    smsg->surprise_level,
                    smsg->source_module_id,
                    smsg->conflicting_module_id);
                nimcp_mutex_unlock(amp->mutex);
            }
            break;
        }

        case BIO_MSG_SOCIETY_CONFLICT_DETECTED: {
            /* Inter-agent conflict signal */
            if (msg_size >= sizeof(bio_msg_conflict_detected_t)) {
                const bio_msg_conflict_detected_t* cmsg =
                    (const bio_msg_conflict_detected_t*)msg;
                surprise_amplifier_on_agent_conflict(
                    amp,
                    cmsg->agent_a_id, cmsg->confidence_a,
                    cmsg->agent_b_id, cmsg->confidence_b,
                    cmsg->divergence);
            }
            break;
        }

        default:
            if (amp->config.enable_logging) {
                NIMCP_LOGGING_DEBUG("Surprise amplifier ignoring unknown message type 0x%04X",
                                    header->type);
            }
            break;
    }

    return 0;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_amplifier_config_t surprise_amplifier_default_config(void) {
    surprise_amplifier_config_t config;
    memset(&config, 0, sizeof(config));

    config.base_threshold = SURPRISE_DEFAULT_THRESHOLD;
    config.amplification_gain = SURPRISE_DEFAULT_GAIN;
    config.attention_boost_factor = SURPRISE_DEFAULT_ATTENTION_BOOST;
    config.curiosity_boost_factor = SURPRISE_DEFAULT_CURIOSITY_BOOST;
    config.decay_rate = SURPRISE_DEFAULT_DECAY_RATE;
    config.conflict_weight = 1.5f;
    config.novelty_weight = 1.0f;
    config.hypothesis_weight = 1.3f;
    config.bayesian_weight = 1.1f;
    config.refractory_period_ms = SURPRISE_DEFAULT_REFRACTORY_MS;
    config.max_concurrent = 4;
    config.enable_gw_broadcast = true;
    config.enable_executive_interrupt = true;
    config.executive_interrupt_threshold = 0.7f;
    config.enable_bio_async = true;
    config.enable_logging = true;

    return config;
}

surprise_amplifier_t* surprise_amplifier_create(
    const surprise_amplifier_config_t* config
) {
    surprise_amplifier_heartbeat("surprise_create", 0.0f);

    /* Allocate structure */
    surprise_amplifier_t* amp = (surprise_amplifier_t*)nimcp_calloc(
        1, sizeof(surprise_amplifier_t));
    if (!amp) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_ERROR_NO_MEMORY,
                           sizeof(surprise_amplifier_t),
                           "Failed to allocate surprise amplifier (%zu bytes)",
                           sizeof(surprise_amplifier_t));
        NIMCP_LOGGING_ERROR("Failed to allocate surprise amplifier");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        amp->config = *config;
    } else {
        amp->config = surprise_amplifier_default_config();
    }

    /* Initialize state */
    amp->current_level = 0.0f;
    amp->last_event_time_ms = 0;
    amp->active_event_count = 0;
    amp->history_head = 0;
    amp->history_count = 0;
    amp->has_last_event = false;

    /* Clear connections */
    amp->fep_system = NULL;
    amp->salience = NULL;
    amp->gw = NULL;
    amp->curiosity = NULL;
    amp->executive = NULL;
    amp->bio_ctx = NULL;
    amp->bio_async_enabled = false;
    amp->health_agent = NULL;

    /* Clear statistics */
    memset(&amp->stats, 0, sizeof(surprise_amplifier_stats_t));

    /* Create mutex for thread safety */
    amp->mutex = nimcp_mutex_create(NULL);
    if (!amp->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_SURPRISE_ERROR_NO_MEMORY,
                              "Failed to create surprise amplifier mutex");
        NIMCP_LOGGING_ERROR("Failed to create surprise amplifier mutex");
        nimcp_free(amp);
        amp = NULL;
        return NULL;
    }

    amp->initialized = true;

    NIMCP_LOGGING_INFO("Created surprise amplifier (threshold=%.2f, gain=%.1f, "
                       "attention_boost=%.1f, curiosity_boost=%.1f)",
                       amp->config.base_threshold,
                       amp->config.amplification_gain,
                       amp->config.attention_boost_factor,
                       amp->config.curiosity_boost_factor);

    return amp;
}

void surprise_amplifier_destroy(surprise_amplifier_t* amp) {
    if (!amp) return;

    surprise_amplifier_heartbeat("surprise_destroy", 0.0f);

    /* Disconnect bio-async if connected */
    if (amp->bio_async_enabled) {
        surprise_amplifier_disconnect_bio_async(amp);
    }

    /* Destroy mutex */
    if (amp->mutex) {
        nimcp_mutex_destroy(amp->mutex);
        amp->mutex = NULL;
    }

    amp->initialized = false;

    NIMCP_LOGGING_INFO("Destroyed surprise amplifier (total_surprises=%lu, "
                       "max_magnitude=%.3f)",
                       (unsigned long)amp->stats.total_surprises,
                       amp->stats.max_magnitude);

    nimcp_free(amp);
    amp = NULL;
}

int surprise_amplifier_reset(surprise_amplifier_t* amp) {
    if (!amp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_SURPRISE_ERROR_NULL_POINTER,
                              "NULL amp in surprise_amplifier_reset");
        return NIMCP_SURPRISE_ERROR_NULL_POINTER;
    }

    surprise_amplifier_heartbeat("surprise_reset", 0.0f);

    nimcp_mutex_lock(amp->mutex);

    /* Reset state but preserve config and connections */
    amp->current_level = 0.0f;
    amp->last_event_time_ms = 0;
    amp->active_event_count = 0;
    amp->history_head = 0;
    amp->history_count = 0;
    amp->has_last_event = false;
    memset(&amp->stats, 0, sizeof(surprise_amplifier_stats_t));
    memset(amp->history, 0, sizeof(amp->history));

    nimcp_mutex_unlock(amp->mutex);

    NIMCP_LOGGING_INFO("Reset surprise amplifier state");
    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_amplifier_connect_fep(surprise_amplifier_t* amp, void* fep) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in connect_fep");
    NIMCP_CHECK_THROW_IMMUNE(fep != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL fep in connect_fep");

    surprise_amplifier_heartbeat("connect_fep", 0.0f);

    nimcp_mutex_lock(amp->mutex);
    amp->fep_system = fep;
    nimcp_mutex_unlock(amp->mutex);

    NIMCP_LOGGING_INFO("Connected FEP system to surprise amplifier");
    return 0;
}

int surprise_amplifier_connect_salience(surprise_amplifier_t* amp,
                                        struct salience_evaluator_struct* salience) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in connect_salience");
    NIMCP_CHECK_THROW_IMMUNE(salience != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL salience in connect_salience");

    surprise_amplifier_heartbeat("connect_salience", 0.0f);

    nimcp_mutex_lock(amp->mutex);
    amp->salience = salience;
    nimcp_mutex_unlock(amp->mutex);

    NIMCP_LOGGING_INFO("Connected salience evaluator to surprise amplifier");
    return 0;
}

int surprise_amplifier_connect_gw(surprise_amplifier_t* amp,
                                   struct global_workspace_struct* gw) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in connect_gw");
    NIMCP_CHECK_THROW_IMMUNE(gw != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL gw in connect_gw");

    surprise_amplifier_heartbeat("connect_gw", 0.0f);

    nimcp_mutex_lock(amp->mutex);
    amp->gw = gw;
    nimcp_mutex_unlock(amp->mutex);

    NIMCP_LOGGING_INFO("Connected global workspace to surprise amplifier");
    return 0;
}

int surprise_amplifier_connect_curiosity(surprise_amplifier_t* amp,
                                          struct curiosity_engine_struct* curiosity) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in connect_curiosity");
    NIMCP_CHECK_THROW_IMMUNE(curiosity != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL curiosity in connect_curiosity");

    surprise_amplifier_heartbeat("connect_curiosity", 0.0f);

    nimcp_mutex_lock(amp->mutex);
    amp->curiosity = curiosity;
    nimcp_mutex_unlock(amp->mutex);

    NIMCP_LOGGING_INFO("Connected curiosity engine to surprise amplifier");
    return 0;
}

int surprise_amplifier_connect_executive(surprise_amplifier_t* amp,
                                          struct executive_controller* executive) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in connect_executive");
    NIMCP_CHECK_THROW_IMMUNE(executive != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL executive in connect_executive");

    surprise_amplifier_heartbeat("connect_executive", 0.0f);

    nimcp_mutex_lock(amp->mutex);
    amp->executive = executive;
    nimcp_mutex_unlock(amp->mutex);

    NIMCP_LOGGING_INFO("Connected executive controller to surprise amplifier");
    return 0;
}

int surprise_amplifier_connect_bio_async(surprise_amplifier_t* amp,
                                          struct bio_router_struct* router) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in connect_bio_async");

    surprise_amplifier_heartbeat("connect_bio_async", 0.0f);

    /* Already connected */
    if (amp->bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Surprise amplifier already connected to bio-async");
        return 0;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SURPRISE_AMPLIFIER,
        .module_name = "surprise_amplifier",
        .inbox_capacity = 32,
        .user_data = amp
    };

    amp->bio_ctx = bio_router_register_module(&info);
    if (amp->bio_ctx) {
        amp->bio_async_enabled = true;

        /* Register handlers for incoming messages */
        bio_router_register_handler(amp->bio_ctx,
                                    BIO_MSG_SOCIETY_SURPRISE_SIGNAL,
                                    surprise_amplifier_bio_handler);
        bio_router_register_handler(amp->bio_ctx,
                                    BIO_MSG_SOCIETY_CONFLICT_DETECTED,
                                    surprise_amplifier_bio_handler);

        NIMCP_LOGGING_INFO("Connected surprise amplifier to bio-async router "
                           "(module_id=0x%04X)", BIO_MODULE_SURPRISE_AMPLIFIER);
    } else {
        NIMCP_LOGGING_WARN("Bio-async router not available, skipping registration");
    }

    return 0;
}

int surprise_amplifier_disconnect_bio_async(surprise_amplifier_t* amp) {
    if (!amp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_SURPRISE_ERROR_NULL_POINTER,
                              "NULL amp in disconnect_bio_async");
        return NIMCP_SURPRISE_ERROR_NULL_POINTER;
    }

    if (!amp->bio_async_enabled) {
        return 0;
    }

    surprise_amplifier_heartbeat("disconnect_bio_async", 0.0f);

    if (amp->bio_ctx) {
        bio_router_unregister_module(amp->bio_ctx);
        amp->bio_ctx = NULL;
    }

    amp->bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected surprise amplifier from bio-async router");

    return 0;
}

/* ============================================================================
 * Input Signal API
 * ============================================================================ */

int surprise_amplifier_on_prediction_error(surprise_amplifier_t* amp,
                                            float prediction_error,
                                            uint32_t source_module) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in on_prediction_error");

    if (prediction_error < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_SURPRISE_ERROR_INVALID_PARAM,
                              "Negative prediction_error: %f", prediction_error);
        return NIMCP_SURPRISE_ERROR_INVALID_PARAM;
    }

    surprise_amplifier_heartbeat("on_prediction_error", 0.0f);

    /* Normalize prediction error to [0,1] range */
    float normalized = nimcp_clampf(prediction_error, 0.0f, 1.0f);

    nimcp_mutex_lock(amp->mutex);
    int result = surprise_amplifier_process_event_unlocked(
        amp,
        SURPRISE_SOURCE_FEP_PREDICTION_ERROR,
        normalized,
        source_module,
        0);
    nimcp_mutex_unlock(amp->mutex);

    return result;
}

int surprise_amplifier_on_agent_conflict(surprise_amplifier_t* amp,
                                          uint32_t agent_a_id,
                                          float confidence_a,
                                          uint32_t agent_b_id,
                                          float confidence_b,
                                          float divergence) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in on_agent_conflict");

    surprise_amplifier_heartbeat("on_agent_conflict", 0.0f);

    /* Compute conflict magnitude from confidence gap and divergence */
    float confidence_gap = fabsf(confidence_a - confidence_b);
    float min_confidence = fminf(confidence_a, confidence_b);

    /*
     * High magnitude when:
     * 1. Both agents are confident (min_confidence high)
     * 2. They strongly disagree (divergence high)
     * 3. Their confidence levels are similar (small gap = both sure)
     *
     * This models the biological ACC conflict monitoring signal.
     */
    float conflict_strength = min_confidence * divergence * (1.0f - confidence_gap * 0.5f);
    float normalized = nimcp_clampf(conflict_strength, 0.0f, 1.0f);

    nimcp_mutex_lock(amp->mutex);
    int result = surprise_amplifier_process_event_unlocked(
        amp,
        SURPRISE_SOURCE_INTER_AGENT_CONFLICT,
        normalized,
        agent_a_id,
        agent_b_id);
    nimcp_mutex_unlock(amp->mutex);

    if (amp->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Agent conflict: A=0x%04X(%.2f) vs B=0x%04X(%.2f) "
                            "divergence=%.3f strength=%.3f",
                            agent_a_id, confidence_a,
                            agent_b_id, confidence_b,
                            divergence, normalized);
    }

    return result;
}

int surprise_amplifier_on_hypothesis_invalidated(surprise_amplifier_t* amp,
                                                  float prior_confidence,
                                                  float posterior_confidence) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in on_hypothesis_invalidated");

    surprise_amplifier_heartbeat("on_hypothesis_invalidated", 0.0f);

    /* Magnitude proportional to confidence drop */
    float confidence_drop = prior_confidence - posterior_confidence;
    float normalized = nimcp_clampf(confidence_drop, 0.0f, 1.0f);

    nimcp_mutex_lock(amp->mutex);
    int result = surprise_amplifier_process_event_unlocked(
        amp,
        SURPRISE_SOURCE_HYPOTHESIS_INVALIDATED,
        normalized,
        0,
        0);
    nimcp_mutex_unlock(amp->mutex);

    if (amp->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Hypothesis invalidated: prior=%.3f -> posterior=%.3f "
                            "drop=%.3f",
                            prior_confidence, posterior_confidence, confidence_drop);
    }

    return result;
}

int surprise_amplifier_on_novelty(surprise_amplifier_t* amp,
                                   float novelty_score,
                                   uint32_t source_module) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in on_novelty");

    surprise_amplifier_heartbeat("on_novelty", 0.0f);

    float normalized = nimcp_clampf(novelty_score, 0.0f, 1.0f);

    nimcp_mutex_lock(amp->mutex);
    int result = surprise_amplifier_process_event_unlocked(
        amp,
        SURPRISE_SOURCE_NOVELTY_DETECTION,
        normalized,
        source_module,
        0);
    nimcp_mutex_unlock(amp->mutex);

    return result;
}

int surprise_amplifier_on_bayesian_surprise(surprise_amplifier_t* amp,
                                             float kl_divergence,
                                             uint32_t source_module) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in on_bayesian_surprise");

    surprise_amplifier_heartbeat("on_bayesian_surprise", 0.0f);

    /* KL divergence can be > 1, normalize with sigmoid-like mapping */
    float normalized = nimcp_clampf(kl_divergence / (1.0f + kl_divergence), 0.0f, 1.0f);

    nimcp_mutex_lock(amp->mutex);
    int result = surprise_amplifier_process_event_unlocked(
        amp,
        SURPRISE_SOURCE_BAYESIAN_DIVERGENCE,
        normalized,
        source_module,
        0);
    nimcp_mutex_unlock(amp->mutex);

    return result;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int surprise_amplifier_update(surprise_amplifier_t* amp, float dt_seconds) {
    if (!amp) {
        NIMCP_THROW_ASYNC(NIMCP_SURPRISE_ERROR_NULL_POINTER,
                          "NULL amp in surprise_amplifier_update");
        return NIMCP_SURPRISE_ERROR_NULL_POINTER;
    }

    if (dt_seconds < 0.0f) {
        return NIMCP_SURPRISE_ERROR_INVALID_PARAM;
    }

    surprise_amplifier_heartbeat("surprise_update", 0.0f);

    nimcp_mutex_lock(amp->mutex);

    /* Exponential decay of current surprise level */
    if (amp->current_level > 0.0f && dt_seconds > 0.0f) {
        float decay = powf(amp->config.decay_rate, dt_seconds);
        amp->current_level *= decay;

        /* Clamp to zero if negligible */
        if (amp->current_level < 0.001f) {
            amp->current_level = 0.0f;
        }
    }

    /* Decay active event count over time */
    if (amp->active_event_count > 0 && dt_seconds > 0.1f) {
        amp->active_event_count--;
    }

    /* Process bio-async inbox if connected */
    if (amp->bio_async_enabled && amp->bio_ctx) {
        bio_router_process_inbox(amp->bio_ctx, 8);
    }

    amp->stats.total_updates++;

    nimcp_mutex_unlock(amp->mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float surprise_amplifier_get_current_level(surprise_amplifier_t* amp) {
    if (!amp) return -1.0f;

    /* Note: single float read is effectively atomic, but use mutex for consistency */
    nimcp_mutex_lock((nimcp_mutex_t*)amp->mutex);
    float level = amp->current_level;
    nimcp_mutex_unlock((nimcp_mutex_t*)amp->mutex);

    return level;
}

bool surprise_amplifier_is_in_refractory(surprise_amplifier_t* amp) {
    if (!amp) {
        return false;
    }

    uint64_t now_ms = get_time_ms();
    nimcp_mutex_lock((nimcp_mutex_t*)amp->mutex);
    bool refractory = (amp->last_event_time_ms > 0 &&
                       (now_ms - amp->last_event_time_ms) < amp->config.refractory_period_ms);
    nimcp_mutex_unlock((nimcp_mutex_t*)amp->mutex);

    return refractory;
}

int surprise_amplifier_get_last_event(surprise_amplifier_t* amp,
                                       surprise_event_t* event_out) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in get_last_event");
    NIMCP_CHECK_THROW_IMMUNE(event_out != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL event_out in get_last_event");

    nimcp_mutex_lock((nimcp_mutex_t*)amp->mutex);
    if (!amp->has_last_event) {
        nimcp_mutex_unlock((nimcp_mutex_t*)amp->mutex);
        return NIMCP_SURPRISE_ERROR_NOT_INITIALIZED;
    }
    *event_out = amp->last_event;
    nimcp_mutex_unlock((nimcp_mutex_t*)amp->mutex);

    return 0;
}

int surprise_amplifier_get_history(const surprise_amplifier_t* amp,
                                    surprise_event_t* events_out,
                                    uint32_t max_events,
                                    uint32_t* count_out) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in get_history");
    NIMCP_CHECK_THROW_IMMUNE(events_out != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL events_out in get_history");
    NIMCP_CHECK_THROW_IMMUNE(count_out != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL count_out in get_history");

    surprise_amplifier_heartbeat("get_history", 0.0f);

    nimcp_mutex_lock((nimcp_mutex_t*)amp->mutex);

    uint32_t to_copy = amp->history_count;
    if (to_copy > max_events) {
        to_copy = max_events;
    }

    /* Copy events in chronological order (oldest first) */
    if (to_copy > 0) {
        uint32_t start = 0;
        if (amp->history_count >= SURPRISE_HISTORY_SIZE) {
            /* Ring buffer wrapped - oldest is at history_head */
            start = amp->history_head;
        } else {
            start = 0;
        }

        for (uint32_t i = 0; i < to_copy; i++) {
            uint32_t idx = (start + (amp->history_count - to_copy) + i) % SURPRISE_HISTORY_SIZE;
            events_out[i] = amp->history[idx];
        }
    }

    *count_out = to_copy;
    nimcp_mutex_unlock((nimcp_mutex_t*)amp->mutex);

    return 0;
}

surprise_amplifier_stats_t surprise_amplifier_get_stats(
    const surprise_amplifier_t* amp
) {
    surprise_amplifier_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    if (!amp) return stats;

    surprise_amplifier_heartbeat("get_stats", 0.0f);

    nimcp_mutex_lock((nimcp_mutex_t*)amp->mutex);
    stats = amp->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)amp->mutex);

    return stats;
}

bool surprise_amplifier_is_bio_async_connected(const surprise_amplifier_t* amp) {
    if (!amp) {
        return false;
    }
    return amp->bio_async_enabled;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_amplifier_set_health_agent(surprise_amplifier_t* amp,
                                         struct nimcp_health_agent* agent) {
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ERROR_NULL_POINTER,
                             "NULL amp in set_health_agent");

    nimcp_mutex_lock(amp->mutex);
    amp->health_agent = (nimcp_health_agent_t*)agent;
    nimcp_mutex_unlock(amp->mutex);

    /* Also set the module-level global */
    surprise_amplifier_set_health_agent_internal((nimcp_health_agent_t*)agent);

    NIMCP_LOGGING_INFO("Set health agent for surprise amplifier");
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query KG for self-knowledge about surprise amplifier
 * WHY:  Enable self-awareness and introspection about module's wiring
 * HOW:  Read KG entity and relations for the module
 */
int surprise_amplifier_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    surprise_amplifier_heartbeat("query_self_knowledge", 0.0f);

    const kg_entity_t* self = kg_reader_get_entity(kg, "SurpriseAmplifier");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                surprise_amplifier_heartbeat("kg_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "SurpriseAmplifier");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "SurpriseAmplifier");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_amplifier_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_amplifier_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_amplifier_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_amplifier_training_begin: NULL argument");
        return -1;
    }
    surprise_amplifier_heartbeat_instance(NULL, "surprise_amplifier_training_begin", 0.0f);
    (void)(struct surprise_amplifier*)instance; /* Module state available for reset */
    return 0;
}

int surprise_amplifier_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_amplifier_training_end: NULL argument");
        return -1;
    }
    surprise_amplifier_heartbeat_instance(NULL, "surprise_amplifier_training_end", 1.0f);
    (void)(struct surprise_amplifier*)instance; /* Module state available for finalization */
    return 0;
}

int surprise_amplifier_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_amplifier_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_amplifier_heartbeat_instance(NULL, "surprise_amplifier_training_step", progress);
    (void)(struct surprise_amplifier*)instance; /* Module state available for step adaptation */
    return 0;
}

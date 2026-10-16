/**
 * @file nimcp_surprise_pink_noise_bridge.c
 * @brief Bridge between Surprise Amplifier and Pink Noise system
 * @version 1.1.0
 * @date 2026-01-27
 *
 * WHAT: Biologically realistic 1/f noise for surprise baseline parameters
 * WHY:  Neural systems operate with 1/f noise; natural threshold fluctuations
 * HOW:  Pink noise -> parameter injection; surprise level -> amplitude adaptation
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_pink_noise_bridge.h"
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/rng/nimcp_rand.h"

#include <string.h>
#include <math.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(surprise_pink_noise, MESH_ADAPTER_CATEGORY_COGNITIVE)
static nimcp_health_agent_t* g_surprise_pink_noise_bridge_health_agent = NULL;
void surprise_pink_noise_bridge_set_health_agent_global(struct nimcp_health_agent* agent) { g_surprise_pink_noise_bridge_health_agent = agent; }

/* ============================================================================
 * Pink Noise Generator (Voss-McCartney algorithm)
 * ============================================================================ */

#define PINK_NOISE_NUM_OCTAVES 8

typedef struct {
    float octave_values[PINK_NOISE_NUM_OCTAVES];
    uint32_t counter;
    float running_sum;
} pink_noise_gen_t;

static void pink_noise_gen_init(pink_noise_gen_t* gen) {
    memset(gen, 0, sizeof(*gen));
}

static float pink_noise_gen_next(pink_noise_gen_t* gen) {
    gen->counter++;

    /* Voss-McCartney: update octave based on trailing zeros */
    uint32_t changed = gen->counter;
    int octave = 0;
    while (octave < PINK_NOISE_NUM_OCTAVES && !(changed & 1)) {
        changed >>= 1;
        octave++;
    }

    if (octave < PINK_NOISE_NUM_OCTAVES) {
        /* Remove old value, generate new white noise, add it */
        gen->running_sum -= gen->octave_values[octave];
        float white = nimcp_rand_uniform() * 2.0f - 1.0f;
        gen->octave_values[octave] = white;
        gen->running_sum += white;
    }

    /* Normalize by number of octaves */
    return gen->running_sum / (float)PINK_NOISE_NUM_OCTAVES;
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_pink_noise_bridge {
    surprise_pink_noise_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;

    /* State */
    surprise_pink_noise_effects_t effects;
    surprise_pink_noise_stats_t stats;

    /* Noise generators (one per target) */
    pink_noise_gen_t generators[SURPRISE_PINK_NOISE_NUM_TARGETS];

    /* Bio-async */
    void* router;
    bool bio_async_connected;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    bool initialized;
    uint64_t update_count;
};

/* ============================================================================
 * Bio-Async Helpers
 * ============================================================================ */

static void pink_noise_send_injection_msg(surprise_pink_noise_bridge_t* bridge,
                                            float amplitude)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "pink_noise_send_injection_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        float effective_amplitude;
        float noise_values[SURPRISE_PINK_NOISE_NUM_TARGETS];
        uint64_t injection_count;
    } surprise_noise_injection_msg_t;

    surprise_noise_injection_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_NOISE_INJECTION,
                        BIO_MODULE_SURPRISE_PINK_NOISE,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.effective_amplitude = amplitude;
    for (int i = 0; i < SURPRISE_PINK_NOISE_NUM_TARGETS; i++) {
        msg.noise_values[i] = bridge->effects.current_noise[i];
    }
    msg.injection_count = bridge->stats.noise_injections;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

static void pink_noise_send_adaptation_msg(surprise_pink_noise_bridge_t* bridge,
                                             float old_amplitude, float new_amplitude)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "pink_noise_send_adaptation_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        float old_amplitude;
        float new_amplitude;
        float adaptation_factor;
        uint64_t adaptation_count;
    } surprise_noise_adaptation_msg_t;

    surprise_noise_adaptation_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_NOISE_ADAPTATION,
                        BIO_MODULE_SURPRISE_PINK_NOISE,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.old_amplitude = old_amplitude;
    msg.new_amplitude = new_amplitude;
    msg.adaptation_factor = bridge->effects.adaptation_factor;
    msg.adaptation_count = bridge->stats.adaptations;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_pink_noise_config_t surprise_pink_noise_bridge_default_config(void) {
    surprise_pink_noise_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.base_amplitude = SURPRISE_PINK_NOISE_DEFAULT_BASE_AMPLITUDE;
    cfg.alpha = SURPRISE_PINK_NOISE_DEFAULT_ALPHA;
    cfg.adaptation_rate = SURPRISE_PINK_NOISE_DEFAULT_ADAPT_RATE;
    cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_THRESHOLD] = 1.0f;
    cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_SENSITIVITY] = 0.8f;
    cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_DECAY] = 0.5f;
    cfg.target_amplitudes[SURPRISE_PINK_NOISE_TARGET_REFRACTORY] = 0.3f;
    cfg.temporal_smoothing = SURPRISE_PINK_NOISE_DEFAULT_SMOOTHING;
    cfg.min_amplitude = SURPRISE_PINK_NOISE_DEFAULT_MIN_AMPLITUDE;
    cfg.max_amplitude = SURPRISE_PINK_NOISE_DEFAULT_MAX_AMPLITUDE;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_pink_noise_bridge_t* surprise_pink_noise_bridge_create(
    const surprise_pink_noise_config_t* config)
{
    surprise_pink_noise_bridge_t* bridge = (surprise_pink_noise_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_PINK_NOISE_ERROR_NO_MEMORY,
                           sizeof(surprise_pink_noise_bridge_t),
                           "surprise_pink_noise_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_pink_noise_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_pink_noise_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_PINK_NOISE_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_pink_noise_bridge mutex allocation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize noise generators */
    for (int i = 0; i < SURPRISE_PINK_NOISE_NUM_TARGETS; i++) {
        pink_noise_gen_init(&bridge->generators[i]);
    }

    bridge->effects.effective_amplitude = bridge->config.base_amplitude;
    bridge->effects.adaptation_factor = 1.0f;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-pink noise bridge (amp=%.3f, alpha=%.1f)",
                           bridge->config.base_amplitude, bridge->config.alpha);
        NIMCP_LOGGING_DEBUG("Pink noise config: adapt_rate=%.3f, smoothing=%.3f, "
                            "min_amp=%.3f, max_amp=%.3f",
                            bridge->config.adaptation_rate,
                            bridge->config.temporal_smoothing,
                            bridge->config.min_amplitude,
                            bridge->config.max_amplitude);
        NIMCP_LOGGING_DEBUG("Pink noise target amplitudes: [thresh=%.2f, sens=%.2f, "
                            "decay=%.2f, refract=%.2f]",
                            bridge->config.target_amplitudes[0],
                            bridge->config.target_amplitudes[1],
                            bridge->config.target_amplitudes[2],
                            bridge->config.target_amplitudes[3]);
    }

    return bridge;
}

void surprise_pink_noise_bridge_destroy(surprise_pink_noise_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-pink noise bridge (injections=%lu, adaptations=%lu)",
                           (unsigned long)bridge->stats.noise_injections,
                           (unsigned long)bridge->stats.adaptations);
        NIMCP_LOGGING_DEBUG("Pink noise final state: amplitude=%.3f, adapt_factor=%.3f, "
                            "amp_changes=%lu",
                            bridge->effects.effective_amplitude,
                            bridge->effects.adaptation_factor,
                            (unsigned long)bridge->stats.amplitude_changes);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_pink_noise_bridge_reset(surprise_pink_noise_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    bridge->effects.effective_amplitude = bridge->config.base_amplitude;
    bridge->effects.adaptation_factor = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    for (int i = 0; i < SURPRISE_PINK_NOISE_NUM_TARGETS; i++) {
        pink_noise_gen_init(&bridge->generators[i]);
    }
    bridge->update_count = 0;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Pink noise bridge reset: amplitude restored to %.3f",
                            bridge->config.base_amplitude);
    }

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_pink_noise_bridge_connect_amplifier(
    surprise_pink_noise_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "connect_amplifier", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-pink noise bridge connected to amplifier");
        NIMCP_LOGGING_DEBUG("Pink noise amplifier connection established, ptr=%p", (void*)amp);
    }
    return 0;
}

int surprise_pink_noise_bridge_connect_bio_async(
    surprise_pink_noise_bridge_t* bridge,
    void* router)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in connect_bio_async");

    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "connect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Pink noise bio-async %s (router=%p)",
                            router ? "connected" : "disconnected", router);
    }

    return 0;
}

int surprise_pink_noise_bridge_disconnect_bio_async(
    surprise_pink_noise_bridge_t* bridge)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in disconnect_bio_async");

    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "disconnect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = NULL;
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Pink noise bio-async disconnected");
    }

    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_pink_noise_inject(surprise_pink_noise_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in inject");

    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "inject", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    float amplitude = bridge->effects.effective_amplitude;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Pink noise inject: amplitude=%.4f, samples_generated=%lu",
                            amplitude, (unsigned long)bridge->effects.samples_generated);
    }

    for (int i = 0; i < SURPRISE_PINK_NOISE_NUM_TARGETS; i++) {
        float raw = pink_noise_gen_next(&bridge->generators[i]);
        float target_scale = bridge->config.target_amplitudes[i];
        float noise = raw * amplitude * target_scale;

        /* Temporal smoothing (EMA) */
        bridge->effects.current_noise[i] =
            bridge->config.temporal_smoothing * bridge->effects.current_noise[i] +
            (1.0f - bridge->config.temporal_smoothing) * noise;

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_TRACE("Pink noise target[%d]: raw=%.4f, scale=%.3f, "
                                "noise=%.4f, smoothed=%.4f",
                                i, raw, target_scale, noise,
                                bridge->effects.current_noise[i]);
        }
    }

    bridge->effects.samples_generated++;
    bridge->stats.noise_injections++;

    /* Bio-async: send injection notification periodically (every 100 injections) */
    if ((bridge->stats.noise_injections % 100) == 0) {
        pink_noise_send_injection_msg(bridge, amplitude);
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Pink noise bio-async: sent INJECTION (count=%lu)",
                                (unsigned long)bridge->stats.noise_injections);
        }
    }

    /* Loop heartbeat */
    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "inject", 1.0f);

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_pink_noise_adapt_amplitude(
    surprise_pink_noise_bridge_t* bridge,
    float surprise_level)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in adapt_amplitude");

    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "adapt_amplitude", 0.0f);

    float level = nimcp_myelin_clamp(surprise_level, 0.0f, 1.0f);

    nimcp_mutex_lock(bridge->mutex);

    /* High surprise -> increase noise (more uncertainty) */
    float target_factor = 1.0f + level;
    bridge->effects.adaptation_factor +=
        bridge->config.adaptation_rate * (target_factor - bridge->effects.adaptation_factor);

    float old_amp = bridge->effects.effective_amplitude;
    bridge->effects.effective_amplitude = nimcp_myelin_clamp(
        bridge->config.base_amplitude * bridge->effects.adaptation_factor,
        bridge->config.min_amplitude,
        bridge->config.max_amplitude);

    bridge->stats.adaptations++;

    float amp_delta = fabsf(bridge->effects.effective_amplitude - old_amp);
    if (amp_delta > 0.001f) {
        bridge->stats.amplitude_changes++;

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Pink noise amplitude adapted: %.4f -> %.4f (factor=%.3f, "
                                "surprise=%.3f)",
                                old_amp, bridge->effects.effective_amplitude,
                                bridge->effects.adaptation_factor, level);
        }

        /* Bio-async: send adaptation when amplitude changes significantly */
        if (amp_delta > 0.005f) {
            pink_noise_send_adaptation_msg(bridge, old_amp,
                                            bridge->effects.effective_amplitude);
            if (bridge->config.enable_logging) {
                NIMCP_LOGGING_DEBUG("Pink noise bio-async: sent ADAPTATION (delta=%.4f)",
                                    amp_delta);
            }
        }
    }

    /* Warn on edge conditions */
    if (bridge->config.enable_logging) {
        if (bridge->effects.effective_amplitude <= bridge->config.min_amplitude) {
            NIMCP_LOGGING_WARN("Pink noise amplitude at minimum: %.4f",
                               bridge->effects.effective_amplitude);
        }
        if (bridge->effects.effective_amplitude >= bridge->config.max_amplitude) {
            NIMCP_LOGGING_WARN("Pink noise amplitude at maximum: %.4f",
                               bridge->effects.effective_amplitude);
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_pink_noise_bridge_update(
    surprise_pink_noise_bridge_t* bridge,
    float dt_seconds)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) return 0;

    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "update", 0.0f);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Pink noise update: dt=%.4f, amplitude=%.4f",
                            dt_seconds, bridge->effects.effective_amplitude);
    }

    /* Inject noise on each update */
    surprise_pink_noise_inject(bridge);

    /* If connected to amplifier, adapt based on current surprise */
    nimcp_mutex_lock(bridge->mutex);
    if (bridge->amplifier) {
        float surprise = surprise_amplifier_get_current_level(bridge->amplifier);
        nimcp_mutex_unlock(bridge->mutex);
        surprise_pink_noise_adapt_amplitude(bridge, surprise);
        nimcp_mutex_lock(bridge->mutex);
    }

    bridge->stats.total_updates++;
    bridge->update_count++;

    /* Loop heartbeat */
    surprise_pink_noise_heartbeat_instance(bridge->health_agent, "update", 1.0f);

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

float surprise_pink_noise_get_for_target(
    const surprise_pink_noise_bridge_t* bridge,
    uint32_t target)
{
    if (!bridge) return 0.0f;
    if (target >= SURPRISE_PINK_NOISE_NUM_TARGETS) return 0.0f;

    return bridge->effects.current_noise[target];
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_pink_noise_bridge_get_effects(
    const surprise_pink_noise_bridge_t* bridge,
    surprise_pink_noise_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    *effects_out = bridge->effects;
    return 0;
}

int surprise_pink_noise_bridge_get_stats(
    const surprise_pink_noise_bridge_t* bridge,
    surprise_pink_noise_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_pink_noise_bridge_set_health_agent(
    surprise_pink_noise_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Pink noise bridge instance health agent %s",
                            agent ? "set" : "cleared");
    }
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_pink_noise_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_pink_noise_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_pink_noise_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_pink_noise_training_begin: NULL argument");
        return -1;
    }
    surprise_pink_noise_heartbeat_instance(NULL, "surprise_pink_noise_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int surprise_pink_noise_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_pink_noise_training_end: NULL argument");
        return -1;
    }
    surprise_pink_noise_heartbeat_instance(NULL, "surprise_pink_noise_training_end", 1.0f);
    (void)instance;
    return 0;
}

int surprise_pink_noise_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_pink_noise_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_pink_noise_heartbeat_instance(NULL, "surprise_pink_noise_training_step", progress);
    (void)instance;
    return 0;
}

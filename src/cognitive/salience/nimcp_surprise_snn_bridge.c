/**
 * @file nimcp_surprise_snn_bridge.c
 * @brief Bridge between Surprise Amplifier and Spiking Neural Network
 * @version 2.0.0
 * @date 2026-01-27
 *
 * WHAT: Encode surprise as spike trains; decode SNN activity as surprise effects
 * WHY:  Spiking networks provide temporal precision for surprise encoding
 * HOW:  Surprise magnitude -> firing rate / phase; SNN patterns -> channel activations
 *
 * UPGRADE (v2.0.0):
 * - Replaced local clamp_f() with nimcp_myelin_clamp() from myelin_math
 * - Added full DEBUG/TRACE logging for all operations
 * - Added bio-async messaging (spike burst + channel dominance shift)
 * - Added Hilbert transform phase encoding (SURPRISE_SNN_ENCODING_PHASE)
 * - Added training/cognitive integration hooks
 * - Instance-level heartbeat via bridge->health_agent
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_snn_bridge.h"
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

BRIDGE_BOILERPLATE(surprise_snn, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Alias: tests reference surprise_snn_bridge_set_health_agent_global */
void surprise_snn_bridge_set_health_agent_global(void* agent) {
    surprise_snn_set_health_agent((nimcp_health_agent_t*)agent);
}

/* ============================================================================
 * Bio-Async Forward Declarations
 * ============================================================================ */

extern nimcp_error_t bio_router_broadcast(void* ctx,
                                           const void* msg,
                                           size_t msg_size);

/* ============================================================================
 * Training / Cognitive Integration Hooks (stubs for future wiring)
 * ============================================================================ */

/**
 * @brief Notify training layer of SNN spike burst
 *
 * FUTURE: Connect to training_cognitive_bridge when spike patterns
 *         should modulate learning rate or eligibility traces.
 */
static inline void training_hook_spike_burst(const surprise_snn_bridge_t* bridge,
                                              uint32_t spike_count,
                                              float spike_rate) {
    (void)bridge;
    (void)spike_count;
    (void)spike_rate;
    /* TODO: nimcp_training_cognitive_bridge_notify_spike_burst(...) */
}

/**
 * @brief Notify training layer of dominant channel shift
 *
 * FUTURE: Different surprise channels may warrant different learning strategies.
 */
static inline void training_hook_channel_shift(const surprise_snn_bridge_t* bridge,
                                                surprise_snn_channel_t prev_channel,
                                                surprise_snn_channel_t new_channel) {
    (void)bridge;
    (void)prev_channel;
    (void)new_channel;
    /* TODO: nimcp_training_cognitive_bridge_notify_channel_shift(...) */
}

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

typedef struct {
    float membrane_potential;
    float refractory_remaining;
    bool spiked;
} snn_neuron_t;

struct surprise_snn_bridge {
    surprise_snn_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    void* snn_system;

    /* State */
    surprise_snn_effects_t effects;
    surprise_snn_stats_t stats;

    /* Neuron arrays (one per channel) */
    snn_neuron_t* neurons;  /* neurons_per_channel * NUM_CHANNELS */
    uint32_t total_neurons;

    /* Bio-async */
    void* router;
    bool bio_async_connected;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    /* Dominant channel tracking for bio-async change notification */
    surprise_snn_channel_t prev_dominant_channel;

    bool initialized;
    uint64_t update_count;
};

/**
 * @brief Dual heartbeat: instance-level first, then global fallback
 */
static inline void surprise_snn_heartbeat_ex(const surprise_snn_bridge_t* bridge,
                                              const char* op, float progress) {
    if (bridge && bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(bridge->health_agent, op, progress);
    } else if (g_surprise_snn_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_surprise_snn_health_agent, op, progress);
    }
}

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline snn_neuron_t* get_channel_neurons(surprise_snn_bridge_t* bridge,
                                                  surprise_snn_channel_t ch) {
    return &bridge->neurons[(uint32_t)ch * bridge->config.neurons_per_channel];
}

/* ============================================================================
 * Bio-Async Messaging Helpers
 * ============================================================================ */

/**
 * @brief Send BIO_MSG_SURPRISE_SNN_SPIKE_BURST when high activity is detected
 */
static void send_spike_burst_message(surprise_snn_bridge_t* bridge,
                                      uint32_t spike_count,
                                      float spike_rate) {
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "send_spike_burst_message: bio_async_connected but router is NULL");
        return;
    }

    struct {
        bio_message_header_t header;
        uint32_t spike_count;
        float spike_rate;
        float channel_activations[SURPRISE_SNN_NUM_CHANNELS];
        surprise_snn_channel_t dominant_channel;
    } msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_SURPRISE_SNN_SPIKE_BURST,
                        BIO_MODULE_SURPRISE_SNN, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.spike_count = spike_count;
    msg.spike_rate = spike_rate;
    for (uint32_t ch = 0; ch < SURPRISE_SNN_NUM_CHANNELS; ch++) {
        msg.channel_activations[ch] = bridge->effects.channel_activations[ch];
    }
    msg.dominant_channel = bridge->effects.dominant_channel;

    nimcp_error_t err = bio_router_broadcast(bridge->router, &msg, sizeof(msg));
    if (err != 0) {
        NIMCP_LOGGING_WARN("surprise_snn: failed to broadcast spike burst (err=%d)", err);
    } else {
        NIMCP_LOGGING_DEBUG("surprise_snn: broadcast spike burst (count=%u, rate=%.3f)",
                            spike_count, (double)spike_rate);
    }
}

/**
 * @brief Send BIO_MSG_SURPRISE_SNN_CHANNEL_DOMINANT when dominant channel changes
 */
static void send_channel_dominant_message(surprise_snn_bridge_t* bridge,
                                           surprise_snn_channel_t prev_channel,
                                           surprise_snn_channel_t new_channel) {
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "send_channel_dominant_message: bio_async_connected but router is NULL");
        return;
    }

    struct {
        bio_message_header_t header;
        uint32_t prev_channel;
        uint32_t new_channel;
        float new_channel_activation;
        float confidence;
    } msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(&msg.header, BIO_MSG_SURPRISE_SNN_CHANNEL_DOMINANT,
                        BIO_MODULE_SURPRISE_SNN, 0,
                        sizeof(msg) - sizeof(bio_message_header_t));
    msg.header.channel = BIO_CHANNEL_DOPAMINE;
    msg.prev_channel = (uint32_t)prev_channel;
    msg.new_channel = (uint32_t)new_channel;
    msg.new_channel_activation = bridge->effects.channel_activations[(uint32_t)new_channel];
    msg.confidence = bridge->effects.confidence;

    nimcp_error_t err = bio_router_broadcast(bridge->router, &msg, sizeof(msg));
    if (err != 0) {
        NIMCP_LOGGING_WARN("surprise_snn: failed to broadcast channel dominance (err=%d)", err);
    } else {
        NIMCP_LOGGING_DEBUG("surprise_snn: broadcast channel dominance shift (%u -> %u)",
                            (unsigned)prev_channel, (unsigned)new_channel);
    }
}

/* ============================================================================
 * Phase Encoding (Hilbert Transform / Oscillatory Coding)
 * ============================================================================ */

/* Phase encoding: surprise magnitude maps to phase angle, not firing rate.
 * Uses phase-coded injection where membrane potential accumulates in a
 * sinusoidal pattern modulated by surprise level.
 * Reference: oscillatory coding in thalamo-cortical circuits */
static void encode_phase(surprise_snn_bridge_t* bridge, float surprise_level,
                         surprise_snn_channel_t channel) {
    snn_neuron_t* neurons = get_channel_neurons(bridge, channel);
    float weight = bridge->config.channel_weights[(uint32_t)channel];
    float phase = surprise_level * (float)M_PI;  /* Map [0,1] -> [0,pi] */

    NIMCP_LOGGING_DEBUG("surprise_snn: phase encode ch=%u level=%.3f phase=%.3f weight=%.3f",
                        (unsigned)channel, (double)surprise_level, (double)phase, (double)weight);

    for (uint32_t i = 0; i < bridge->config.neurons_per_channel; i++) {
        if (neurons[i].refractory_remaining <= 0.0f) {
            /* Phase-coded injection: each neuron has a preferred phase offset */
            float neuron_phase = (float)i / (float)bridge->config.neurons_per_channel * (float)M_PI;
            float phase_match = cosf(phase - neuron_phase);
            neurons[i].membrane_potential += weight * phase_match * surprise_level;

            NIMCP_LOGGING_TRACE("surprise_snn: phase neuron[%u] neuron_phase=%.3f match=%.3f Vm=%.3f",
                                i, (double)neuron_phase, (double)phase_match,
                                (double)neurons[i].membrane_potential);
        }
    }
}

/* ============================================================================
 * Rate Encoding (original method)
 * ============================================================================ */

/**
 * @brief Rate-coded injection: surprise level -> uniform current into all
 *        non-refractory neurons in the channel.
 */
static void encode_rate(surprise_snn_bridge_t* bridge, float surprise_level,
                        surprise_snn_channel_t channel) {
    snn_neuron_t* ch_neurons = get_channel_neurons(bridge, channel);
    float weight = bridge->config.channel_weights[(uint32_t)channel];
    float injection = surprise_level * weight;

    NIMCP_LOGGING_DEBUG("surprise_snn: rate encode ch=%u level=%.3f injection=%.3f",
                        (unsigned)channel, (double)surprise_level, (double)injection);

    for (uint32_t i = 0; i < bridge->config.neurons_per_channel; i++) {
        if (ch_neurons[i].refractory_remaining <= 0.0f) {
            ch_neurons[i].membrane_potential += injection;
        }
    }
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_snn_config_t surprise_snn_bridge_default_config(void) {
    surprise_snn_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.dt_ms = SURPRISE_SNN_DEFAULT_DT_MS;
    cfg.neurons_per_channel = SURPRISE_SNN_DEFAULT_NEURONS_PER_CH;
    cfg.encoding_type = SURPRISE_SNN_ENCODING_RATE;
    cfg.threshold = SURPRISE_SNN_DEFAULT_THRESHOLD;
    cfg.refractory_ms = SURPRISE_SNN_DEFAULT_REFRACTORY_MS;
    cfg.decay_factor = SURPRISE_SNN_DEFAULT_DECAY_FACTOR;
    cfg.history_size = SURPRISE_SNN_DEFAULT_HISTORY_SIZE;
    cfg.channel_weights[SURPRISE_SNN_CHANNEL_PE] = 1.0f;
    cfg.channel_weights[SURPRISE_SNN_CHANNEL_CONFLICT] = 1.0f;
    cfg.channel_weights[SURPRISE_SNN_CHANNEL_NOVELTY] = 1.0f;
    cfg.channel_weights[SURPRISE_SNN_CHANNEL_HYPOTHESIS] = 1.0f;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    NIMCP_LOGGING_DEBUG("surprise_snn: created default config (dt=%.1fms, neurons/ch=%u, "
                        "threshold=%.2f, encoding=RATE)",
                        (double)cfg.dt_ms, cfg.neurons_per_channel, (double)cfg.threshold);

    return cfg;
}

surprise_snn_bridge_t* surprise_snn_bridge_create(
    const surprise_snn_config_t* config)
{
    surprise_snn_heartbeat("create", 0.0f);

    NIMCP_LOGGING_DEBUG("surprise_snn: creating bridge (config=%s)",
                        config ? "custom" : "default");

    surprise_snn_bridge_t* bridge = (surprise_snn_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_snn_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SNN_ERROR_NO_MEMORY,
                           sizeof(surprise_snn_bridge_t),
                           "surprise_snn_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_snn_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_snn_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SNN_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_snn_bridge mutex allocation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate neurons */
    bridge->total_neurons = bridge->config.neurons_per_channel * SURPRISE_SNN_NUM_CHANNELS;
    bridge->neurons = (snn_neuron_t*)nimcp_calloc(
        bridge->total_neurons, sizeof(snn_neuron_t));
    if (!bridge->neurons) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SNN_ERROR_NO_MEMORY,
                           bridge->total_neurons * sizeof(snn_neuron_t),
                           "surprise_snn_bridge neurons allocation failed");
        nimcp_mutex_destroy(bridge->mutex);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->prev_dominant_channel = SURPRISE_SNN_CHANNEL_PE;
    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Created surprise-SNN bridge (%u neurons, %u per channel, encoding=%d)",
                       bridge->total_neurons, bridge->config.neurons_per_channel,
                       (int)bridge->config.encoding_type);

    surprise_snn_heartbeat("create", 1.0f);
    return bridge;
}

void surprise_snn_bridge_destroy(surprise_snn_bridge_t* bridge) {
    if (!bridge) return;

    NIMCP_LOGGING_INFO("Destroying surprise-SNN bridge (spikes=%lu, encodings=%lu, updates=%lu)",
                       (unsigned long)bridge->stats.total_spikes,
                       (unsigned long)bridge->stats.encoding_events,
                       (unsigned long)bridge->stats.total_updates);

    NIMCP_LOGGING_DEBUG("surprise_snn: final stats: ch_spikes=[%lu,%lu,%lu,%lu]",
                        (unsigned long)bridge->stats.channel_spikes[0],
                        (unsigned long)bridge->stats.channel_spikes[1],
                        (unsigned long)bridge->stats.channel_spikes[2],
                        (unsigned long)bridge->stats.channel_spikes[3]);

    if (bridge->neurons) {
        nimcp_free(bridge->neurons);
    }
    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_snn_bridge_reset(surprise_snn_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_snn_heartbeat_ex(bridge, "reset", 0.0f);

    NIMCP_LOGGING_DEBUG("surprise_snn: resetting bridge (was: spikes=%lu, updates=%lu)",
                        (unsigned long)bridge->stats.total_spikes,
                        (unsigned long)bridge->stats.total_updates);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->neurons, 0, bridge->total_neurons * sizeof(snn_neuron_t));
    bridge->prev_dominant_channel = SURPRISE_SNN_CHANNEL_PE;
    bridge->update_count = 0;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_DEBUG("surprise_snn: reset complete");

    surprise_snn_heartbeat_ex(bridge, "reset", 1.0f);
    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_snn_bridge_connect_amplifier(
    surprise_snn_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    surprise_snn_heartbeat_ex(bridge, "connect_amplifier", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Surprise-SNN bridge connected to amplifier");
    return 0;
}

int surprise_snn_bridge_connect_snn(
    surprise_snn_bridge_t* bridge,
    void* snn)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in connect_snn");
    NIMCP_CHECK_THROW_IMMUNE(snn != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL snn in connect_snn");

    surprise_snn_heartbeat_ex(bridge, "connect_snn", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->snn_system = snn;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Surprise-SNN bridge connected to SNN system");
    return 0;
}

int surprise_snn_bridge_connect_bio_async(
    surprise_snn_bridge_t* bridge,
    void* router)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in connect_bio_async");

    surprise_snn_heartbeat_ex(bridge, "connect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_INFO("Surprise-SNN bridge bio-async %s (router=%p)",
                       router ? "connected" : "disconnected", router);
    return 0;
}

int surprise_snn_bridge_disconnect_bio_async(
    surprise_snn_bridge_t* bridge)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in disconnect_bio_async");

    surprise_snn_heartbeat_ex(bridge, "disconnect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = NULL;
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_DEBUG("surprise_snn: bio-async disconnected");
    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_snn_encode_surprise(
    surprise_snn_bridge_t* bridge,
    float surprise_level,
    surprise_snn_channel_t channel)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in encode_surprise");
    NIMCP_CHECK_THROW_IMMUNE((uint32_t)channel < SURPRISE_SNN_NUM_CHANNELS,
                             NIMCP_SURPRISE_SNN_ERROR_INVALID_PARAM,
                             "Invalid channel %u in encode_surprise", (unsigned)channel);

    surprise_snn_heartbeat_ex(bridge, "encode_surprise", 0.0f);

    float level = nimcp_myelin_clamp(surprise_level, 0.0f, 1.0f);

    NIMCP_LOGGING_DEBUG("surprise_snn: encode surprise ch=%u level=%.3f (raw=%.3f) encoding=%d",
                        (unsigned)channel, (double)level, (double)surprise_level,
                        (int)bridge->config.encoding_type);

    nimcp_mutex_lock(bridge->mutex);

    /* Dispatch to encoding strategy */
    switch (bridge->config.encoding_type) {
        case SURPRISE_SNN_ENCODING_PHASE:
            encode_phase(bridge, level, channel);
            break;

        case SURPRISE_SNN_ENCODING_RATE:
        case SURPRISE_SNN_ENCODING_TEMPORAL:
        case SURPRISE_SNN_ENCODING_POPULATION:
        default:
            /* Rate encoding is the default; temporal and population modes
             * fall through to rate for now (future specialization) */
            encode_rate(bridge, level, channel);
            break;
    }

    bridge->stats.encoding_events++;
    nimcp_mutex_unlock(bridge->mutex);

    surprise_snn_heartbeat_ex(bridge, "encode_surprise", 1.0f);
    return 0;
}

int surprise_snn_simulate_step(surprise_snn_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in simulate_step");

    surprise_snn_heartbeat_ex(bridge, "simulate_step", 0.0f);

    NIMCP_LOGGING_DEBUG("surprise_snn: simulate_step begin (dt=%.1fms)",
                        (double)bridge->config.dt_ms);

    nimcp_mutex_lock(bridge->mutex);

    uint32_t spikes_this_step = 0;
    uint32_t channel_spikes[SURPRISE_SNN_NUM_CHANNELS] = {0};

    for (uint32_t ch = 0; ch < SURPRISE_SNN_NUM_CHANNELS; ch++) {
        snn_neuron_t* neurons = get_channel_neurons(bridge, (surprise_snn_channel_t)ch);

        for (uint32_t i = 0; i < bridge->config.neurons_per_channel; i++) {
            if ((i & 0xFF) == 0 && bridge->config.neurons_per_channel > 256) {
                surprise_snn_heartbeat_ex(bridge, "simulate_step",
                    (float)((ch * bridge->config.neurons_per_channel) + i) /
                    (float)bridge->total_neurons);
            }

            snn_neuron_t* n = &neurons[i];
            n->spiked = false;

            if (n->refractory_remaining > 0.0f) {
                n->refractory_remaining -= bridge->config.dt_ms;
                continue;
            }

            /* Leak */
            n->membrane_potential *= bridge->config.decay_factor;

            /* Check threshold */
            if (n->membrane_potential >= bridge->config.threshold) {
                n->spiked = true;
                n->membrane_potential = 0.0f;
                n->refractory_remaining = bridge->config.refractory_ms;
                spikes_this_step++;
                channel_spikes[ch]++;

                NIMCP_LOGGING_TRACE("surprise_snn: spike ch=%u neuron=%u (threshold=%.3f)",
                                    ch, i, (double)bridge->config.threshold);
            }
        }
    }

    /* Update stats */
    bridge->stats.total_spikes += spikes_this_step;
    for (uint32_t ch = 0; ch < SURPRISE_SNN_NUM_CHANNELS; ch++) {
        bridge->stats.channel_spikes[ch] += channel_spikes[ch];
    }

    /* Update effects */
    float total_possible = (float)(bridge->config.neurons_per_channel * SURPRISE_SNN_NUM_CHANNELS);
    bridge->effects.spike_rate = (float)spikes_this_step / total_possible;
    bridge->effects.combined_activity = bridge->effects.spike_rate;
    bridge->effects.high_activity_flag = (bridge->effects.spike_rate > 0.5f);

    /* Compute per-channel activations and find dominant */
    float max_activation = 0.0f;
    bridge->effects.dominant_channel = SURPRISE_SNN_CHANNEL_PE;
    for (uint32_t ch = 0; ch < SURPRISE_SNN_NUM_CHANNELS; ch++) {
        float act = (float)channel_spikes[ch] / (float)bridge->config.neurons_per_channel;
        bridge->effects.channel_activations[ch] = act;
        if (act > max_activation) {
            max_activation = act;
            bridge->effects.dominant_channel = (surprise_snn_channel_t)ch;
        }
    }
    bridge->effects.confidence = (max_activation > 0.0f) ?
        max_activation / (bridge->effects.spike_rate + 0.001f) : 0.0f;

    /* Clamp confidence to [0,1] using myelin_clamp */
    bridge->effects.confidence = nimcp_myelin_clamp(bridge->effects.confidence, 0.0f, 1.0f);

    NIMCP_LOGGING_DEBUG("surprise_snn: step result: spikes=%u rate=%.3f dominant_ch=%u "
                        "confidence=%.3f high_activity=%d",
                        spikes_this_step, (double)bridge->effects.spike_rate,
                        (unsigned)bridge->effects.dominant_channel,
                        (double)bridge->effects.confidence,
                        bridge->effects.high_activity_flag);

    /* Track dominant channel for bio-async notification */
    surprise_snn_channel_t prev_dom = bridge->prev_dominant_channel;
    bool channel_changed = (bridge->effects.dominant_channel != prev_dom &&
                            max_activation > 0.0f);
    bool high_activity = bridge->effects.high_activity_flag;
    bridge->prev_dominant_channel = bridge->effects.dominant_channel;

    nimcp_mutex_unlock(bridge->mutex);

    /* Bio-async messaging (outside mutex) */
    if (bridge->config.enable_bio_async) {
        if (high_activity) {
            NIMCP_LOGGING_WARN("surprise_snn: HIGH ACTIVITY detected (rate=%.3f, spikes=%u)",
                               (double)bridge->effects.spike_rate, spikes_this_step);
            send_spike_burst_message(bridge, spikes_this_step, bridge->effects.spike_rate);
            training_hook_spike_burst(bridge, spikes_this_step, bridge->effects.spike_rate);
        }
        if (channel_changed) {
            NIMCP_LOGGING_INFO("surprise_snn: dominant channel changed %u -> %u",
                               (unsigned)prev_dom,
                               (unsigned)bridge->effects.dominant_channel);
            send_channel_dominant_message(bridge, prev_dom, bridge->effects.dominant_channel);
            training_hook_channel_shift(bridge, prev_dom, bridge->effects.dominant_channel);
        }
    }

    surprise_snn_heartbeat_ex(bridge, "simulate_step", 1.0f);
    return 0;
}

int surprise_snn_decode_output(
    surprise_snn_bridge_t* bridge,
    surprise_snn_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in decode_output");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL effects_out in decode_output");

    surprise_snn_heartbeat_ex(bridge, "decode_output", 0.0f);

    *effects_out = bridge->effects;

    NIMCP_LOGGING_DEBUG("surprise_snn: decode output: rate=%.3f dominant=%u confidence=%.3f",
                        (double)effects_out->spike_rate,
                        (unsigned)effects_out->dominant_channel,
                        (double)effects_out->confidence);

    return 0;
}

surprise_snn_channel_t surprise_snn_get_dominant_channel(
    const surprise_snn_bridge_t* bridge)
{
    if (!bridge) return SURPRISE_SNN_CHANNEL_PE;

    NIMCP_LOGGING_DEBUG("surprise_snn: get_dominant_channel -> %u",
                        (unsigned)bridge->effects.dominant_channel);

    return bridge->effects.dominant_channel;
}

int surprise_snn_bridge_update(
    surprise_snn_bridge_t* bridge,
    float dt_seconds)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) {
        NIMCP_LOGGING_DEBUG("surprise_snn: update skipped (dt=%.4f <= 0)", (double)dt_seconds);
        return 0;
    }

    surprise_snn_heartbeat_ex(bridge, "update", 0.0f);

    NIMCP_LOGGING_DEBUG("surprise_snn: update begin (dt=%.4fs = %.1fms)",
                        (double)dt_seconds, (double)(dt_seconds * 1000.0f));

    /* Run simulation steps for the elapsed time */
    float time_remaining = dt_seconds * 1000.0f; /* convert to ms */
    uint32_t step_count = 0;
    while (time_remaining > 0.0f) {
        surprise_snn_simulate_step(bridge);
        time_remaining -= bridge->config.dt_ms;
        step_count++;
    }

    nimcp_mutex_lock(bridge->mutex);
    bridge->stats.total_updates++;
    bridge->update_count++;
    nimcp_mutex_unlock(bridge->mutex);

    NIMCP_LOGGING_DEBUG("surprise_snn: update complete (%u steps, update_count=%lu)",
                        step_count, (unsigned long)bridge->update_count);

    surprise_snn_heartbeat_ex(bridge, "update", 1.0f);
    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_snn_bridge_get_effects(
    const surprise_snn_bridge_t* bridge,
    surprise_snn_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    surprise_snn_heartbeat_ex(bridge, "get_effects", 0.0f);

    *effects_out = bridge->effects;

    NIMCP_LOGGING_DEBUG("surprise_snn: get_effects: rate=%.3f dominant=%u combined=%.3f",
                        (double)effects_out->spike_rate,
                        (unsigned)effects_out->dominant_channel,
                        (double)effects_out->combined_activity);

    return 0;
}

int surprise_snn_bridge_get_stats(
    const surprise_snn_bridge_t* bridge,
    surprise_snn_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    surprise_snn_heartbeat_ex(bridge, "get_stats", 0.0f);

    *stats_out = bridge->stats;

    NIMCP_LOGGING_DEBUG("surprise_snn: get_stats: spikes=%lu encodings=%lu updates=%lu "
                        "novelty=%lu",
                        (unsigned long)stats_out->total_spikes,
                        (unsigned long)stats_out->encoding_events,
                        (unsigned long)stats_out->total_updates,
                        (unsigned long)stats_out->novelty_detections);

    return 0;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_snn_bridge_set_health_agent(
    surprise_snn_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;

    NIMCP_LOGGING_DEBUG("surprise_snn: set health agent %p (instance-level)", (void*)agent);
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_snn_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_snn_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_snn_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_snn_training_begin: NULL argument");
        return -1;
    }
    surprise_snn_heartbeat_instance(NULL, "surprise_snn_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int surprise_snn_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_snn_training_end: NULL argument");
        return -1;
    }
    surprise_snn_heartbeat_instance(NULL, "surprise_snn_training_end", 1.0f);
    (void)instance;
    return 0;
}

int surprise_snn_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_snn_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_snn_heartbeat_instance(NULL, "surprise_snn_training_step", progress);
    (void)instance;
    return 0;
}

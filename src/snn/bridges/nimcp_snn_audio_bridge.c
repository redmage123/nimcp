/**
 * @file nimcp_snn_audio_bridge.c
 * @brief SNN-Audio Cortex integration bridge implementation
 *
 * WHAT: Bidirectional bridge between SNN and audio_cortex_t
 * WHY:  Enable spike-based auditory processing with temporal patterns
 * HOW:  Rate coding for spectrograms, temporal coding for audio events
 *
 * BIOLOGICAL BASIS:
 * - Auditory cortex (A1) exhibits precise spike timing for sound processing
 * - Cochlear neurons encode frequency via tonotopic spike patterns
 * - Phase locking in auditory neurons encodes frequency information
 * - Temporal spike patterns critical for speech and music perception
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_audio_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_audio_bridge)

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point for audio processing
 * HOW:  Literature-based parameter values from A1 studies
 */
void snn_audio_config_default(snn_audio_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_config_default: null config pointer");
        return;
    }

    /* Encoding configuration */
    config->encoding_method = SNN_ENCODE_RATE;
    config->max_spike_rate = 300.0f;         /* A1 neurons: up to 300 Hz */
    config->min_spike_rate = 5.0f;           /* Baseline spontaneous activity */
    config->temporal_window_ms = 10.0f;      /* Fast for audio */
    config->neurons_per_freq_bin = 4;        /* Population per frequency */

    /* Decoding configuration */
    config->decoding_method = SNN_DECODE_RATE;
    config->decode_window_ms = 20.0f;
    config->use_first_spike = false;

    /* Audio processing */
    config->sample_rate = 16000;             /* 16 kHz speech processing */
    config->frame_size = 512;                /* ~32ms frames */
    config->num_freq_bins = 256;             /* FFT bins */
    config->num_mel_filters = 128;           /* Mel scale filters */
    config->encode_mfcc = true;              /* Use MFCC by default */

    /* Temporal coding */
    config->use_onset_detection = true;      /* Detect sound onsets */
    config->use_phase_locking = true;        /* Phase-lock low frequencies */
    config->phase_lock_freq_max = 1000.0f;   /* Phase locking up to 1 kHz */

    /* Attention modulation */
    config->use_attention_modulation = true;
    config->attention_gain_min = 0.5f;
    config->attention_gain_max = 2.0f;       /* Strong modulation for audio */

    /* Bio-async */
    config->enable_bio_async = false;
    config->update_interval_ms = 10.0f;      /* 100 Hz for audio */
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-audio bridge
 * WHY:  Initialize bidirectional auditory processing
 * HOW:  Allocate buffers, create encoder/decoder, validate connections
 */
snn_audio_bridge_t* snn_audio_bridge_create(
    const snn_audio_config_t* config,
    snn_network_t* snn,
    audio_cortex_t* audio_cortex
) {
    /* Guard: Validate inputs */
    if (!config || !snn || !audio_cortex) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_audio_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_create: config/snn/audio_cortex is NULL");
        return NULL;
    }

    /* Validate sample rate */
    if (config->sample_rate < 8000 || config->sample_rate > 96000) {
        NIMCP_LOGGING_ERROR("Invalid sample rate: %u", config->sample_rate);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_audio_bridge_create: invalid sample_rate");
        return NULL;
    }

    /* Allocate bridge */
    snn_audio_bridge_t* bridge = nimcp_malloc(sizeof(snn_audio_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-audio bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_audio_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_audio_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_audio") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->audio_cortex = audio_cortex;
    bridge->config = *config;

    /* Calculate buffer sizes */
    uint32_t num_neurons = config->num_freq_bins * config->neurons_per_freq_bin;

    /* Allocate working buffers */
    bridge->audio_buffer = nimcp_malloc(config->frame_size * sizeof(float));
    bridge->spectrum_buffer = nimcp_malloc(config->num_freq_bins * sizeof(float));
    bridge->mel_buffer = nimcp_malloc(config->num_mel_filters * sizeof(float));
    bridge->mfcc_buffer = nimcp_malloc(config->num_mel_filters * sizeof(float));
    bridge->spike_input_buffer = nimcp_malloc(num_neurons * sizeof(float));
    bridge->spike_output_buffer = nimcp_malloc(num_neurons * sizeof(float));
    bridge->spike_mask = nimcp_malloc(num_neurons);

    if (!bridge->audio_buffer || !bridge->spectrum_buffer ||
        !bridge->mel_buffer || !bridge->mfcc_buffer ||
        !bridge->spike_input_buffer || !bridge->spike_output_buffer ||
        !bridge->spike_mask) {
        NIMCP_LOGGING_ERROR("Failed to allocate audio bridge buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_audio_bridge_create: failed to allocate buffers");
        snn_audio_bridge_destroy(bridge);
        return NULL;
    }

    /* Allocate onset detection buffers */
    if (config->use_onset_detection) {
        bridge->onset_strength = nimcp_malloc(config->num_freq_bins * sizeof(float));
        bridge->prev_spectrum = nimcp_malloc(config->num_freq_bins * sizeof(float));
        bridge->onset_detected = nimcp_malloc(config->num_freq_bins * sizeof(bool));

        if (!bridge->onset_strength || !bridge->prev_spectrum ||
            !bridge->onset_detected) {
            NIMCP_LOGGING_ERROR("Failed to allocate onset detection buffers");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_audio_bridge_create: failed to allocate onset buffers");
            snn_audio_bridge_destroy(bridge);
            return NULL;
        }
        memset(bridge->prev_spectrum, 0, config->num_freq_bins * sizeof(float));
    }

    /* Allocate attention gains if enabled */
    if (config->use_attention_modulation) {
        bridge->attention_gains = nimcp_malloc(num_neurons * sizeof(float));
        if (!bridge->attention_gains) {
            NIMCP_LOGGING_ERROR("Failed to allocate audio attention gains");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_audio_bridge_create: failed to allocate attention_gains");
            snn_audio_bridge_destroy(bridge);
            return NULL;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            bridge->attention_gains[i] = 1.0f;
        }
    }

    /* Create rate encoder - uses rate coding for audio spectral input */
    snn_rate_encoder_config_t rate_enc_cfg;
    snn_rate_encoder_config_default(&rate_enc_cfg);
    rate_enc_cfg.max_rate = config->max_spike_rate;
    rate_enc_cfg.min_rate = config->min_spike_rate;
    rate_enc_cfg.value_min = 0.0f;
    rate_enc_cfg.value_max = 1.0f;
    rate_enc_cfg.use_poisson = true;  /* Biologically realistic */
    bridge->encoder = snn_encoder_create_rate(num_neurons, &rate_enc_cfg);

    /* Create rate decoder */
    snn_rate_decoder_config_t rate_dec_cfg;
    snn_rate_decoder_config_default(&rate_dec_cfg);
    rate_dec_cfg.time_window = config->decode_window_ms;
    rate_dec_cfg.max_rate = config->max_spike_rate;
    rate_dec_cfg.use_exponential = false;
    bridge->decoder = snn_decoder_create_rate(num_neurons, num_neurons, &rate_dec_cfg);

    /* Mark as connected */
    bridge->connected = true;

    bbb_register_module("snn_audio_bridge", BBB_MODULE_TYPE_COGNITIVE);
    NIMCP_LOGGING_INFO("Created SNN-audio bridge (%u bins, %u neurons)",
                       config->num_freq_bins, num_neurons);
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup and memory management
 * HOW:  Disconnect, free all buffers, destroy encoder/decoder
 */
void snn_audio_bridge_destroy(snn_audio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_audio_bridge_disconnect_bio_async(bridge);
    }

    /* Free working buffers */
    if (bridge->audio_buffer) nimcp_free(bridge->audio_buffer);
    if (bridge->spectrum_buffer) nimcp_free(bridge->spectrum_buffer);
    if (bridge->mel_buffer) nimcp_free(bridge->mel_buffer);
    if (bridge->mfcc_buffer) nimcp_free(bridge->mfcc_buffer);
    if (bridge->spike_input_buffer) nimcp_free(bridge->spike_input_buffer);
    if (bridge->spike_output_buffer) nimcp_free(bridge->spike_output_buffer);
    if (bridge->spike_mask) nimcp_free(bridge->spike_mask);

    /* Free onset detection buffers */
    if (bridge->onset_strength) nimcp_free(bridge->onset_strength);
    if (bridge->prev_spectrum) nimcp_free(bridge->prev_spectrum);
    if (bridge->onset_detected) nimcp_free(bridge->onset_detected);

    if (bridge->attention_gains) nimcp_free(bridge->attention_gains);

    /* Destroy encoder/decoder */
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-audio bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed audio event coordination
 * HOW:  Register with router as BIO_MODULE_SNN_AUDIO
 */
int snn_audio_bridge_connect_bio_async(snn_audio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_AUDIO,
        .module_name = "snn_audio_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Audio bridge connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for audio bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_audio_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_audio_bridge_disconnect_bio_async(snn_audio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Audio bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query before sending messages
 * HOW:  Return flag
 */
bool snn_audio_bridge_is_bio_async_connected(const snn_audio_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * WHAT: Encode audio spectrum to spike trains
 * WHY:  Convert audio frequency content to SNN input
 * HOW:  Rate encoding with tonotopic organization
 */
int snn_audio_bridge_encode(
    snn_audio_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    uint8_t num_channels,
    snn_spike_train_t** spike_trains
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode: null bridge pointer");
        return -1;
    }
    if (!audio_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode: null audio_data pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode: null spike_trains pointer");
        return -1;
    }

    /* Mix to mono if stereo */
    const float* mono_data = audio_data;
    if (num_channels == 2) {
        uint32_t samples = num_samples / 2;
        for (uint32_t i = 0; i < samples && i < bridge->config.frame_size; i++) {
            bridge->audio_buffer[i] = (audio_data[i*2] + audio_data[i*2+1]) * 0.5f;
        }
        mono_data = bridge->audio_buffer;
    } else {
        /* Copy to audio buffer */
        uint32_t count = (num_samples < bridge->config.frame_size) ?
            num_samples : bridge->config.frame_size;
        memcpy(bridge->audio_buffer, audio_data, count * sizeof(float));
    }

    /* Compute spectrum (simplified - would use FFT in real implementation) */
    uint32_t num_bins = bridge->config.num_freq_bins;
    for (uint32_t i = 0; i < num_bins; i++) {
        /* Simplified spectrum computation - sum magnitude in bin range */
        float sum = 0.0f;
        uint32_t bin_start = i * bridge->config.frame_size / num_bins;
        uint32_t bin_end = (i + 1) * bridge->config.frame_size / num_bins;
        for (uint32_t j = bin_start; j < bin_end && j < bridge->config.frame_size; j++) {
            sum += fabsf(bridge->audio_buffer[j]);
        }
        bridge->spectrum_buffer[i] = sum / (bin_end - bin_start + 1);
    }

    /* Detect onsets if enabled */
    if (bridge->config.use_onset_detection) {
        snn_audio_bridge_detect_onsets(bridge, bridge->onset_detected);
    }

    /* Normalize spectrum to [0, 1] */
    float max_val = 0.0f;
    for (uint32_t i = 0; i < num_bins; i++) {
        if (bridge->spectrum_buffer[i] > max_val) {
            max_val = bridge->spectrum_buffer[i];
        }
    }
    if (max_val > 0.0f) {
        for (uint32_t i = 0; i < num_bins; i++) {
            bridge->spectrum_buffer[i] /= max_val;
        }
    }

    /* Expand to population coding */
    uint32_t npf = bridge->config.neurons_per_freq_bin;
    for (uint32_t i = 0; i < num_bins; i++) {
        float val = bridge->spectrum_buffer[i];

        /* Apply attention modulation */
        if (bridge->config.use_attention_modulation && bridge->attention_gains) {
            val *= bridge->attention_gains[i * npf];
            if (val > 1.0f) val = 1.0f;
        }

        /* Boost for onset events */
        if (bridge->config.use_onset_detection && bridge->onset_detected &&
            bridge->onset_detected[i]) {
            val = fminf(val * 1.5f, 1.0f);
            bridge->encode_stats.onset_events_detected++;
        }

        /* Set population */
        for (uint32_t n = 0; n < npf; n++) {
            bridge->spike_input_buffer[i * npf + n] = val;
        }
    }

    /* Store previous spectrum for onset detection */
    if (bridge->config.use_onset_detection) {
        memcpy(bridge->prev_spectrum, bridge->spectrum_buffer,
               num_bins * sizeof(float));
    }

    /* Encode to spike mask using rate coding */
    uint32_t num_neurons = num_bins * npf;
    float dt = bridge->config.update_interval_ms;
    int ret = snn_encode_rate(bridge->encoder, bridge->spike_input_buffer,
                               dt, bridge->spike_mask);
    if (ret != 0) {
        NIMCP_LOGGING_ERROR("Failed to encode audio to spikes");
        return ret;
    }

    /* Allocate spike trains if needed */
    if (!*spike_trains) {
        *spike_trains = (snn_spike_train_t*)nimcp_calloc(num_neurons,
                                                          sizeof(snn_spike_train_t));
        if (!*spike_trains) {
            return SNN_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            (*spike_trains)[i].neuron_id = i;
        }
    }

    /* Convert binary spike mask to spike trains */
    uint64_t current_time_us = (uint64_t)(bridge->last_update_time_ms * 1000.0f);
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (bridge->spike_mask[i]) {
            snn_spike_train_t* train = &((*spike_trains)[i]);
            train->spike_times[train->write_idx] = current_time_us;
            train->write_idx = (train->write_idx + 1) % SNN_SPIKE_BUFFER_SIZE;
            if (train->count < SNN_SPIKE_BUFFER_SIZE) {
                train->count++;
            }
            train->total_spikes++;
        }
    }

    /* Update statistics */
    bridge->encode_stats.frames_encoded++;
    bridge->frame_count++;

    return 0;
}

/**
 * WHAT: Encode audio features (MFCC) to spikes
 * WHY:  Convert high-level audio features
 * HOW:  Population coding of MFCC coefficients
 */
int snn_audio_bridge_encode_features(
    snn_audio_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    snn_spike_train_t** spike_trains
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode_features: null bridge pointer");
        return -1;
    }
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode_features: null features pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode_features: null spike_trains pointer");
        return -1;
    }

    /* Normalize features to [0, 1] */
    for (uint32_t i = 0; i < num_features; i++) {
        float val = features[i];
        /* MFCC typically ranges roughly -50 to 50, normalize */
        val = (val + 50.0f) / 100.0f;
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;
        bridge->spike_input_buffer[i] = val;
    }

    /* Encode features to binary spike mask */
    float dt = bridge->config.update_interval_ms;
    int ret = snn_encode_rate(bridge->encoder, bridge->spike_input_buffer,
                               dt, bridge->spike_mask);
    if (ret != 0) {
        return ret;
    }

    /* Allocate spike trains if needed */
    if (!*spike_trains) {
        *spike_trains = (snn_spike_train_t*)nimcp_calloc(num_features,
                                                          sizeof(snn_spike_train_t));
        if (!*spike_trains) {
            return SNN_ERROR_OUT_OF_MEMORY;
        }
        for (uint32_t i = 0; i < num_features; i++) {
            (*spike_trains)[i].neuron_id = i;
        }
    }

    /* Convert binary spike mask to spike trains */
    uint64_t current_time_us = (uint64_t)(bridge->last_update_time_ms * 1000.0f);
    for (uint32_t i = 0; i < num_features; i++) {
        if (bridge->spike_mask[i]) {
            snn_spike_train_t* train = &((*spike_trains)[i]);
            train->spike_times[train->write_idx] = current_time_us;
            train->write_idx = (train->write_idx + 1) % SNN_SPIKE_BUFFER_SIZE;
            if (train->count < SNN_SPIKE_BUFFER_SIZE) {
                train->count++;
            }
            train->total_spikes++;
        }
    }

    return 0;
}

//=============================================================================
// Temporal Pattern Functions
//=============================================================================

/**
 * WHAT: Detect audio onset events
 * WHY:  Identify sound beginnings for temporal coding
 * HOW:  Compare current and previous spectral energy
 */
uint32_t snn_audio_bridge_detect_onsets(
    snn_audio_bridge_t* bridge,
    bool* onset_detected
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_detect_onsets: null bridge pointer");
        return 0;
    }
    if (!onset_detected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_detect_onsets: null onset_detected pointer");
        return 0;
    }
    if (!bridge->prev_spectrum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_audio_bridge_detect_onsets: prev_spectrum not initialized");
        return 0;
    }

    uint32_t num_bins = bridge->config.num_freq_bins;
    uint32_t onset_count = 0;
    float threshold = 0.3f;  /* 30% increase triggers onset */

    for (uint32_t i = 0; i < num_bins; i++) {
        float current = bridge->spectrum_buffer[i];
        float prev = bridge->prev_spectrum[i];

        /* Onset detected when current significantly exceeds previous */
        if (prev > 0.01f && (current - prev) / prev > threshold) {
            onset_detected[i] = true;
            if (bridge->onset_strength) {
                bridge->onset_strength[i] = (current - prev) / prev;
            }
            onset_count++;
        } else {
            onset_detected[i] = false;
            if (bridge->onset_strength) {
                bridge->onset_strength[i] = 0.0f;
            }
        }
    }

    return onset_count;
}

/**
 * WHAT: Encode temporal audio pattern to spike timing
 * WHY:  Temporal coding for rapid audio processing
 * HOW:  Latency coding based on audio intensity
 */
int snn_audio_bridge_encode_temporal(
    snn_audio_bridge_t* bridge,
    const float* envelope,
    uint32_t num_samples,
    float* spike_times_out
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode_temporal: null bridge pointer");
        return -1;
    }
    if (!envelope) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode_temporal: null envelope pointer");
        return -1;
    }
    if (!spike_times_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_encode_temporal: null spike_times_out pointer");
        return -1;
    }

    /* Convert envelope to spike times using latency coding */
    float max_latency = bridge->config.temporal_window_ms;

    for (uint32_t i = 0; i < num_samples; i++) {
        float intensity = envelope[i];
        if (intensity < 0.0f) intensity = 0.0f;
        if (intensity > 1.0f) intensity = 1.0f;

        /* Higher intensity = shorter latency */
        spike_times_out[i] = max_latency * (1.0f - intensity);
    }

    return 0;
}

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * WHAT: Decode spike trains to audio spectrum
 * WHY:  Reconstruct spectrum from SNN output
 * HOW:  Rate decoding to frequency bins
 */
int snn_audio_bridge_decode(
    snn_audio_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    float* spectrum_out
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_decode: null bridge pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_decode: null spike_trains pointer");
        return -1;
    }
    if (!spectrum_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_decode: null spectrum_out pointer");
        return -1;
    }

    /* Extract spike counts from spike trains into input buffer */
    for (uint32_t i = 0; i < num_trains; i++) {
        bridge->spike_input_buffer[i] = (float)spike_trains[i].count;
    }

    /* Decode spike counts to normalized values using rate decoding */
    int ret = snn_decode_rate(bridge->decoder, bridge->spike_input_buffer,
                               bridge->spike_output_buffer);
    if (ret != 0) {
        return ret;
    }

    /* Average population to get per-bin values */
    uint32_t npf = bridge->config.neurons_per_freq_bin;
    uint32_t num_bins = bridge->config.num_freq_bins;

    for (uint32_t i = 0; i < num_bins && i * npf < num_trains; i++) {
        float sum = 0.0f;
        for (uint32_t n = 0; n < npf && i * npf + n < num_trains; n++) {
            sum += bridge->spike_output_buffer[i * npf + n];
        }
        spectrum_out[i] = sum / npf;
    }

    /* Update statistics */
    bridge->decode_stats.frames_decoded++;

    return 0;
}

/**
 * WHAT: Decode spike trains to audio features
 * WHY:  Extract MFCC from SNN output
 * HOW:  Population vector decoding
 */
int snn_audio_bridge_decode_features(
    snn_audio_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    float* features_out,
    uint32_t num_features
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_decode_features: null bridge pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_decode_features: null spike_trains pointer");
        return -1;
    }
    if (!features_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_decode_features: null features_out pointer");
        return -1;
    }

    /* Extract spike counts from spike trains into input buffer */
    for (uint32_t i = 0; i < num_trains; i++) {
        bridge->spike_input_buffer[i] = (float)spike_trains[i].count;
    }

    /* Decode spike counts to output values using rate decoding */
    int ret = snn_decode_rate(bridge->decoder, bridge->spike_input_buffer,
                               bridge->spike_output_buffer);
    if (ret != 0) {
        return ret;
    }

    /* Copy and denormalize to MFCC range */
    uint32_t count = (num_trains < num_features) ? num_trains : num_features;
    for (uint32_t i = 0; i < count; i++) {
        /* Convert from [0,1] back to MFCC range [-50, 50] */
        features_out[i] = bridge->spike_output_buffer[i] * 100.0f - 50.0f;
    }

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

/**
 * WHAT: Update bridge state
 * WHY:  Full update cycle for audio-SNN integration
 * HOW:  Get audio from cortex, encode, update SNN
 */
int snn_audio_bridge_update(snn_audio_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_update: null bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_audio_bridge_update: bridge not connected");
        return -1;
    }

    /* Check update interval */
    bridge->last_update_time_ms += dt;
    if (bridge->last_update_time_ms < bridge->config.update_interval_ms) {
        return 0;
    }
    bridge->last_update_time_ms = 0.0f;

    /* Update attention modulation if enabled */
    if (bridge->config.use_attention_modulation) {
        snn_audio_bridge_update_attention(bridge);
    }

    return 0;
}

/**
 * WHAT: Update attention modulation from audio cortex
 * WHY:  Modulate spike rates by auditory salience
 * HOW:  Use spectral energy as attention proxy
 */
int snn_audio_bridge_update_attention(snn_audio_bridge_t* bridge) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_update_attention: null bridge pointer");
        return -1;
    }
    if (!bridge->attention_gains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_audio_bridge_update_attention: attention_gains not initialized");
        return -1;
    }

    /* Compute attention gains from spectral energy (proxy for salience) */
    uint32_t num_bins = bridge->config.num_freq_bins;
    uint32_t npf = bridge->config.neurons_per_freq_bin;
    float gain_range = bridge->config.attention_gain_max -
                       bridge->config.attention_gain_min;

    for (uint32_t i = 0; i < num_bins; i++) {
        /* Use spectrum buffer as attention proxy if available */
        float attention = 0.5f;  /* Neutral by default */
        if (bridge->spectrum_buffer) {
            attention = bridge->spectrum_buffer[i];
            if (attention > 1.0f) attention = 1.0f;
        }
        float gain = bridge->config.attention_gain_min + attention * gain_range;

        for (uint32_t n = 0; n < npf; n++) {
            bridge->attention_gains[i * npf + n] = gain;
        }
    }

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

/**
 * WHAT: Get encoding statistics
 * WHY:  Monitor encoding performance
 * HOW:  Copy statistics structure
 */
int snn_audio_bridge_get_encode_stats(
    const snn_audio_bridge_t* bridge,
    snn_audio_encode_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_get_encode_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_get_encode_stats: null stats pointer");
        return -1;
    }
    *stats = bridge->encode_stats;
    return 0;
}

/**
 * WHAT: Get decoding statistics
 * WHY:  Monitor decoding performance
 * HOW:  Copy statistics structure
 */
int snn_audio_bridge_get_decode_stats(
    const snn_audio_bridge_t* bridge,
    snn_audio_decode_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_get_decode_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_get_decode_stats: null stats pointer");
        return -1;
    }
    *stats = bridge->decode_stats;
    return 0;
}

/**
 * WHAT: Get current spike rate for frequency bin
 * WHY:  Query individual frequency activity
 * HOW:  Return rate based on spectrum buffer value
 */
float snn_audio_bridge_get_spike_rate(
    const snn_audio_bridge_t* bridge,
    uint32_t freq_bin
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_get_spike_rate: null bridge pointer");
        return -1.0f;
    }
    if (!bridge->spike_input_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_audio_bridge_get_spike_rate: spike_input_buffer not initialized");
        return -1.0f;
    }

    uint32_t idx = freq_bin * bridge->config.neurons_per_freq_bin;
    float normalized = bridge->spike_input_buffer[idx];
    float rate_range = bridge->config.max_spike_rate - bridge->config.min_spike_rate;
    return bridge->config.min_spike_rate + normalized * rate_range;
}

/**
 * WHAT: Check if bridge is active
 * WHY:  Validate bridge state
 * HOW:  Return connected flag
 */
bool snn_audio_bridge_is_active(const snn_audio_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->connected;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * WHAT: Reset bridge statistics
 * WHY:  Start fresh measurement
 * HOW:  Zero all counters
 */
void snn_audio_bridge_reset_stats(snn_audio_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_audio_bridge_reset_stats: null bridge pointer");
        return;
    }

    memset(&bridge->encode_stats, 0, sizeof(snn_audio_encode_stats_t));
    memset(&bridge->decode_stats, 0, sizeof(snn_audio_decode_stats_t));
    bridge->frame_count = 0;
}

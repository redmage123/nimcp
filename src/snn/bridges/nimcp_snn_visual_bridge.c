/**
 * @file nimcp_snn_visual_bridge.c
 * @brief SNN-Visual Cortex integration bridge implementation
 *
 * WHAT: Bidirectional bridge between SNN and visual_cortex_t
 * WHY:  Enable spike-based visual processing with encoding/decoding
 * HOW:  Rate coding for frames, population coding for features
 *
 * BIOLOGICAL BASIS:
 * - V1 (primary visual cortex) exhibits spike-based computation
 * - Retinal ganglion cells encode visual information via spike trains
 * - Gabor-like receptive fields modeled with spiking neurons
 * - Temporal coding carries information in visual processing
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#include "snn/bridges/nimcp_snn_visual_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(snn_visual_bridge)

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Convenient starting point for visual processing
 * HOW:  Literature-based parameter values from V1 studies
 */
void snn_visual_config_default(snn_visual_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_config_default: null config pointer");
        return;
    }

    /* Encoding configuration */
    config->encoding_method = SNN_ENCODE_RATE;
    config->max_spike_rate = 200.0f;         /* V1 neurons: up to 200 Hz */
    config->min_spike_rate = 5.0f;           /* Baseline spontaneous activity */
    config->temporal_window_ms = 20.0f;      /* 50 Hz update rate */
    config->neurons_per_pixel = 1;           /* Default: 1 neuron per pixel */

    /* Decoding configuration */
    config->decoding_method = SNN_DECODE_RATE;
    config->decode_window_ms = 25.0f;        /* Integration window */
    config->use_population_vector = false;   /* Simple rate decoding */

    /* Frame processing */
    config->frame_width = 640;               /* Standard VGA width */
    config->frame_height = 480;              /* Standard VGA height */
    config->frame_channels = 1;              /* Grayscale default */
    config->downsample_frames = false;
    config->downsample_factor = 2;

    /* Attention modulation */
    config->use_attention_modulation = true; /* Enable salience modulation */
    config->attention_gain_min = 0.5f;       /* 50% minimum gain */
    config->attention_gain_max = 1.5f;       /* 150% maximum gain */

    /* Bio-async */
    config->enable_bio_async = false;
    config->update_interval_ms = 20.0f;      /* 50 Hz update rate */
}

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * WHAT: Create SNN-visual bridge
 * WHY:  Initialize bidirectional visual processing
 * HOW:  Allocate buffers, create encoder/decoder, validate connections
 */
snn_visual_bridge_t* snn_visual_bridge_create(
    const snn_visual_config_t* config,
    snn_network_t* snn,
    visual_cortex_t* visual_cortex
) {
    /* Guard: Validate inputs */
    if (!config || !snn || !visual_cortex) {
        NIMCP_LOGGING_ERROR("Null parameters to snn_visual_bridge_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_create: config/snn/visual_cortex is NULL");
        return NULL;
    }

    /* Allocate bridge */
    snn_visual_bridge_t* bridge = nimcp_malloc(sizeof(snn_visual_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate SNN-visual bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_visual_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize structure */
    memset(bridge, 0, sizeof(snn_visual_bridge_t));
    if (bridge_base_init(&bridge->base, 0, "snn_visual") != 0) { nimcp_free(bridge); return NULL; }
    bridge->snn = snn;
    bridge->visual_cortex = visual_cortex;
    bridge->config = *config;

    /* Calculate buffer sizes with overflow protection */
    uint64_t width = config->downsample_frames ?
        config->frame_width / config->downsample_factor : config->frame_width;
    uint64_t height = config->downsample_frames ?
        config->frame_height / config->downsample_factor : config->frame_height;
    uint64_t num_pixels = width * height * config->frame_channels;
    uint64_t num_neurons = num_pixels * config->neurons_per_pixel;
    uint64_t frame_buf_size = (uint64_t)config->frame_width * config->frame_height *
                              config->frame_channels;

    /* Guard against integer overflow producing insane allocation sizes */
    if (num_neurons > SIZE_MAX / sizeof(float) || frame_buf_size > SIZE_MAX) {
        NIMCP_LOGGING_ERROR("Visual bridge buffer size overflow: %llu neurons",
                           (unsigned long long)num_neurons);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "snn_visual_bridge_create: buffer size overflow");
        snn_visual_bridge_destroy(bridge);
        return NULL;
    }

    /* Allocate working buffers */
    bridge->frame_buffer = nimcp_malloc((size_t)frame_buf_size);
    bridge->spike_input_buffer = nimcp_malloc((size_t)(num_neurons * sizeof(float)));
    bridge->spike_output_buffer = nimcp_malloc((size_t)(num_neurons * sizeof(float)));
    bridge->spike_mask = nimcp_malloc((size_t)num_neurons);

    if (!bridge->frame_buffer || !bridge->spike_input_buffer ||
        !bridge->spike_output_buffer || !bridge->spike_mask) {
        NIMCP_LOGGING_ERROR("Failed to allocate visual bridge buffers");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_visual_bridge_create: failed to allocate buffers");
        snn_visual_bridge_destroy(bridge);
        return NULL;
    }

    /* Allocate downsample buffer if needed */
    if (config->downsample_frames) {
        bridge->downsample_buffer = nimcp_malloc((size_t)num_pixels);
        if (!bridge->downsample_buffer) {
            NIMCP_LOGGING_ERROR("Failed to allocate downsample buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_visual_bridge_create: failed to allocate downsample buffer");
            snn_visual_bridge_destroy(bridge);
            return NULL;
        }
    }

    /* Allocate attention gains if enabled */
    if (config->use_attention_modulation) {
        bridge->attention_gains = nimcp_malloc(num_neurons * sizeof(float));
        if (!bridge->attention_gains) {
            NIMCP_LOGGING_ERROR("Failed to allocate attention gains buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "snn_visual_bridge_create: failed to allocate attention gains");
            snn_visual_bridge_destroy(bridge);
            return NULL;
        }
        /* Initialize to neutral gain */
        for (uint32_t i = 0; i < num_neurons; i++) {
            bridge->attention_gains[i] = 1.0f;
        }
    }

    /* Create rate encoder - uses rate coding by default for visual input */
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

    bbb_register_module("snn_visual_bridge", BBB_MODULE_TYPE_COGNITIVE);
    NIMCP_LOGGING_INFO("Created SNN-visual bridge (%ux%u, %u neurons)",
                       width, height, num_neurons);
    return bridge;
}

/**
 * WHAT: Destroy bridge and free resources
 * WHY:  Proper cleanup and memory management
 * HOW:  Disconnect, free all buffers, destroy encoder/decoder
 */
void snn_visual_bridge_destroy(snn_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_destroy: null bridge pointer");
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        snn_visual_bridge_disconnect_bio_async(bridge);
    }

    /* Free working buffers */
    if (bridge->frame_buffer) nimcp_free(bridge->frame_buffer);
    if (bridge->downsample_buffer) nimcp_free(bridge->downsample_buffer);
    if (bridge->spike_input_buffer) nimcp_free(bridge->spike_input_buffer);
    if (bridge->spike_output_buffer) nimcp_free(bridge->spike_output_buffer);
    if (bridge->spike_mask) nimcp_free(bridge->spike_mask);
    if (bridge->attention_gains) nimcp_free(bridge->attention_gains);

    /* Destroy encoder/decoder */
    if (bridge->encoder) snn_encoder_destroy(bridge->encoder);
    if (bridge->decoder) snn_decoder_destroy(bridge->decoder);

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed SNN-visual bridge");
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Connect to bio-async messaging
 * WHY:  Enable distributed visual event coordination
 * HOW:  Register with router as BIO_MODULE_SNN_VISUAL
 */
int snn_visual_bridge_connect_bio_async(snn_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_connect_bio_async: null bridge pointer");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SNN_VISUAL,
        .module_name = "snn_visual_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Visual bridge connected to bio-async router");
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for visual bridge");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_visual_bridge_connect_bio_async: validation failed");
    return -1;
}

/**
 * WHAT: Disconnect from bio-async
 * WHY:  Clean shutdown
 * HOW:  Unregister from router
 */
int snn_visual_bridge_disconnect_bio_async(snn_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_disconnect_bio_async: null bridge pointer");
        return -1;
    }
    if (!bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Visual bridge disconnected from bio-async");
    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query before sending messages
 * HOW:  Return flag
 */
bool snn_visual_bridge_is_bio_async_connected(const snn_visual_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * WHAT: Encode visual frame to spike trains
 * WHY:  Convert image pixels to SNN input
 * HOW:  Normalize pixels, apply attention, rate encode
 */
int snn_visual_bridge_encode(
    snn_visual_bridge_t* bridge,
    const uint8_t* frame,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    snn_spike_train_t** spike_trains
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_encode: null bridge pointer");
        return -1;
    }
    if (!frame) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_encode: null frame pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_encode: null spike_trains pointer");
        return -1;
    }

    /* Validate dimensions */
    if (width != bridge->config.frame_width ||
        height != bridge->config.frame_height ||
        channels != bridge->config.frame_channels) {
        NIMCP_LOGGING_ERROR("Frame dimension mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "snn_visual_bridge_encode: operation failed");
        return -1;
    }

    uint32_t num_pixels = width * height * channels;
    const uint8_t* src = frame;

    /* Downsample if configured */
    if (bridge->config.downsample_frames && bridge->downsample_buffer) {
        uint32_t factor = bridge->config.downsample_factor;
        uint32_t new_w = width / factor;
        uint32_t new_h = height / factor;

        for (uint32_t y = 0; y < new_h; y++) {
            for (uint32_t x = 0; x < new_w; x++) {
                for (uint32_t c = 0; c < channels; c++) {
                    /* Average pooling */
                    uint32_t sum = 0;
                    for (uint32_t dy = 0; dy < factor; dy++) {
                        for (uint32_t dx = 0; dx < factor; dx++) {
                            uint32_t src_idx = ((y * factor + dy) * width +
                                               (x * factor + dx)) * channels + c;
                            sum += frame[src_idx];
                        }
                    }
                    uint32_t dst_idx = (y * new_w + x) * channels + c;
                    bridge->downsample_buffer[dst_idx] = sum / (factor * factor);
                }
            }
        }
        src = bridge->downsample_buffer;
        num_pixels = new_w * new_h * channels;
    }

    /* Normalize pixels to [0, 1] */
    for (uint32_t i = 0; i < num_pixels; i++) {
        bridge->spike_input_buffer[i] = src[i] / 255.0f;
    }

    /* Apply attention modulation if enabled */
    if (bridge->config.use_attention_modulation && bridge->attention_gains) {
        for (uint32_t i = 0; i < num_pixels; i++) {
            bridge->spike_input_buffer[i] *= bridge->attention_gains[i];
            /* Clamp to [0, 1] */
            if (bridge->spike_input_buffer[i] > 1.0f) {
                bridge->spike_input_buffer[i] = 1.0f;
            }
        }
    }

    /* Encode to binary spike mask using rate coding */
    float dt = bridge->config.update_interval_ms;
    int ret = snn_encode_rate(bridge->encoder, bridge->spike_input_buffer,
                               dt, bridge->spike_mask);
    if (ret != 0) {
        NIMCP_LOGGING_ERROR("Failed to encode visual frame to spikes");
        return ret;
    }

    /* Allocate spike trains if needed */
    if (!*spike_trains) {
        *spike_trains = (snn_spike_train_t*)nimcp_calloc(num_pixels,
                                                          sizeof(snn_spike_train_t));
        if (!*spike_trains) {
            NIMCP_LOGGING_ERROR("Failed to allocate spike trains");
            return SNN_ERROR_OUT_OF_MEMORY;
        }
        /* Initialize neuron IDs */
        for (uint32_t i = 0; i < num_pixels; i++) {
            (*spike_trains)[i].neuron_id = i;
        }
    }

    /* Convert binary spike mask to spike trains */
    uint64_t current_time_us = (uint64_t)(bridge->last_update_time_ms * 1000.0f);
    uint64_t spike_count = 0;
    for (uint32_t i = 0; i < num_pixels; i++) {
        if (bridge->spike_mask[i]) {
            snn_spike_train_t* train = &((*spike_trains)[i]);
            /* Add spike time to circular buffer */
            train->spike_times[train->write_idx] = current_time_us;
            train->write_idx = (train->write_idx + 1) % SNN_SPIKE_BUFFER_SIZE;
            if (train->count < SNN_SPIKE_BUFFER_SIZE) {
                train->count++;
            }
            train->total_spikes++;
            spike_count++;
        }
    }

    /* Update statistics */
    bridge->encode_stats.frames_encoded++;
    bridge->frame_count++;
    bridge->encode_stats.total_spikes += spike_count;
    bridge->encode_stats.avg_spikes_per_frame =
        (float)bridge->encode_stats.total_spikes / bridge->encode_stats.frames_encoded;

    return 0;
}

/**
 * WHAT: Encode visual features to spikes
 * WHY:  Convert high-level features from visual cortex
 * HOW:  Population coding of feature vector
 */
int snn_visual_bridge_encode_features(
    snn_visual_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    snn_spike_train_t** spike_trains
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_encode_features: null bridge pointer");
        return -1;
    }
    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_encode_features: null features pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_encode_features: null spike_trains pointer");
        return -1;
    }

    /* Copy features to input buffer (assuming normalized [0,1]) */
    for (uint32_t i = 0; i < num_features; i++) {
        float val = features[i];
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
// Decoding Functions
//=============================================================================

/**
 * WHAT: Decode spike trains to visual frame
 * WHY:  Reconstruct image from SNN output
 * HOW:  Rate decoding to pixel values
 */
int snn_visual_bridge_decode(
    snn_visual_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    uint8_t* frame_out
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_decode: null bridge pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_decode: null spike_trains pointer");
        return -1;
    }
    if (!frame_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_decode: null frame_out pointer");
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
        NIMCP_LOGGING_ERROR("Failed to decode spikes to visual");
        return ret;
    }

    /* Convert normalized values to pixel bytes */
    for (uint32_t i = 0; i < num_trains; i++) {
        float val = bridge->spike_output_buffer[i];
        if (val < 0.0f) val = 0.0f;
        if (val > 1.0f) val = 1.0f;
        frame_out[i] = (uint8_t)(val * 255.0f);
    }

    /* Update statistics */
    bridge->decode_stats.frames_decoded++;

    return 0;
}

/**
 * WHAT: Decode spike trains to visual features
 * WHY:  Extract features from SNN output
 * HOW:  Population vector decoding
 */
int snn_visual_bridge_decode_features(
    snn_visual_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    float* features_out,
    uint32_t num_features
) {
    /* Guard: Validate inputs */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_decode_features: null bridge pointer");
        return -1;
    }
    if (!spike_trains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_decode_features: null spike_trains pointer");
        return -1;
    }
    if (!features_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_decode_features: null features_out pointer");
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

    /* Copy to features output */
    uint32_t count = (num_trains < num_features) ? num_trains : num_features;
    memcpy(features_out, bridge->spike_output_buffer, count * sizeof(float));

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

/**
 * WHAT: Update bridge state and process visual input
 * WHY:  Full update cycle for visual-SNN integration
 * HOW:  Get frame from visual cortex, encode, update SNN
 */
int snn_visual_bridge_update(snn_visual_bridge_t* bridge, float dt) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_update: null bridge pointer");
        return -1;
    }
    if (!bridge->connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_visual_bridge_update: bridge not connected");
        return -1;
    }

    /* Check update interval */
    bridge->last_update_time_ms += dt;
    if (bridge->last_update_time_ms < bridge->config.update_interval_ms) {
        return 0; /* Skip, not time yet */
    }
    bridge->last_update_time_ms = 0.0f;

    /* Update attention modulation if enabled */
    if (bridge->config.use_attention_modulation) {
        snn_visual_bridge_update_attention(bridge);
    }

    return 0;
}

/**
 * WHAT: Update attention modulation from visual cortex
 * WHY:  Modulate spike rates by visual salience
 * HOW:  Sample attention map, compute per-neuron gains
 */
int snn_visual_bridge_update_attention(snn_visual_bridge_t* bridge) {
    /* Guard: Validate bridge */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_update_attention: null bridge pointer");
        return -1;
    }
    if (!bridge->attention_gains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_visual_bridge_update_attention: attention gains not initialized");
        return -1;
    }

    /* Get attention peak from visual cortex's attention map */
    /* Default values if attention map not available */
    float attention_peak = 1.0f;
    float peak_x = 0.5f, peak_y = 0.5f;

    /* Try to get attention map and peak from visual cortex */
    if (bridge->attention_map) {
        uint32_t max_x = 0, max_y = 0;
        float max_val = 0.0f;
        if (visual_cortex_get_attention_peak(bridge->attention_map, &max_x, &max_y, &max_val)) {
            attention_peak = max_val;
            /* Normalize peak position using configured frame dimensions */
            uint32_t width = bridge->config.frame_width;
            uint32_t height = bridge->config.frame_height;
            if (bridge->config.downsample_frames) {
                width /= bridge->config.downsample_factor;
                height /= bridge->config.downsample_factor;
            }
            peak_x = (width > 0) ? (float)max_x / (float)width : 0.5f;
            peak_y = (height > 0) ? (float)max_y / (float)height : 0.5f;
        }
    }

    /* Compute per-neuron gains based on distance from attention peak */
    uint32_t width = bridge->config.downsample_frames ?
        bridge->config.frame_width / bridge->config.downsample_factor :
        bridge->config.frame_width;
    uint32_t height = bridge->config.downsample_frames ?
        bridge->config.frame_height / bridge->config.downsample_factor :
        bridge->config.frame_height;

    float gain_range = bridge->config.attention_gain_max - bridge->config.attention_gain_min;
    float sigma = 0.2f;  /* Attention falloff */

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            /* Compute Gaussian attention based on distance from peak */
            float dx = (float)x / width - peak_x;
            float dy = (float)y / height - peak_y;
            float dist_sq = dx * dx + dy * dy;
            float attention = attention_peak * expf(-dist_sq / (2.0f * sigma * sigma));

            /* Map attention [0,1] to gain [min, max] */
            float gain = bridge->config.attention_gain_min + attention * gain_range;

            uint32_t idx = y * width + x;
            for (uint32_t c = 0; c < bridge->config.frame_channels; c++) {
                bridge->attention_gains[idx * bridge->config.frame_channels + c] = gain;
            }
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
int snn_visual_bridge_get_encode_stats(
    const snn_visual_bridge_t* bridge,
    snn_visual_encode_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_get_encode_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_get_encode_stats: null stats pointer");
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
int snn_visual_bridge_get_decode_stats(
    const snn_visual_bridge_t* bridge,
    snn_visual_decode_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_get_decode_stats: null bridge pointer");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_get_decode_stats: null stats pointer");
        return -1;
    }
    *stats = bridge->decode_stats;
    return 0;
}

/**
 * WHAT: Get current spike rate for pixel/feature
 * WHY:  Query individual neuron activity
 * HOW:  Return cached rate from input buffer (normalized [0,1] * max_rate)
 */
float snn_visual_bridge_get_spike_rate(
    const snn_visual_bridge_t* bridge,
    uint32_t index
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_get_spike_rate: null bridge pointer");
        return -1.0f;
    }
    if (!bridge->spike_input_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "snn_visual_bridge_get_spike_rate: spike_input_buffer not initialized");
        return -1.0f;
    }

    /* Return rate based on input buffer value scaled to spike rate */
    float normalized = bridge->spike_input_buffer[index];
    float rate_range = bridge->config.max_spike_rate - bridge->config.min_spike_rate;
    return bridge->config.min_spike_rate + normalized * rate_range;
}

/**
 * WHAT: Check if bridge is active
 * WHY:  Validate bridge state
 * HOW:  Return connected flag
 */
bool snn_visual_bridge_is_active(const snn_visual_bridge_t* bridge) {
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
void snn_visual_bridge_reset_stats(snn_visual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "snn_visual_bridge_reset_stats: null bridge pointer");
        return;
    }

    memset(&bridge->encode_stats, 0, sizeof(snn_visual_encode_stats_t));
    memset(&bridge->decode_stats, 0, sizeof(snn_visual_decode_stats_t));
    bridge->frame_count = 0;
}

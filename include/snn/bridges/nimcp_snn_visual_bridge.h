/**
 * @file nimcp_snn_visual_bridge.h
 * @brief SNN integration bridge for Visual Cortex
 *
 * WHAT: Bidirectional bridge between SNN and visual_cortex_t
 * WHY:  Enable spike-based visual processing with encoding/decoding
 * HOW:  Rate coding for frames, population coding for features, bio-async messaging
 *
 * BIOLOGICAL BASIS:
 * - V1 (primary visual cortex) exhibits spike-based computation
 * - Retinal ganglion cells encode visual information via spike trains
 * - Gabor-like receptive fields can be modeled with spiking neurons
 * - Temporal coding carries information in visual processing
 *
 * INTEGRATION PATTERN:
 * - Visual frames → SNN encoding → Spike trains
 * - SNN output spikes → Decoding → Visual features
 * - Attention maps modulate spike rates
 * - Bio-async for visual event messaging
 *
 * NIMCP STANDARDS:
 * - WHAT-WHY-HOW documentation
 * - Guard clauses (early returns)
 * - Functions < 50 lines
 * - Bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_VISUAL_BRIDGE_H
#define NIMCP_SNN_VISUAL_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_encoding.h"
#include "perception/nimcp_visual_cortex.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Visual-SNN bridge configuration
 *
 * WHAT: Parameters for visual cortex to SNN integration
 * WHY:  Control encoding/decoding and update behavior
 * HOW:  Thresholds, encoding method, frame buffer settings
 */
typedef struct snn_visual_config_s {
    /* Encoding configuration */
    snn_encoding_t encoding_method;     /**< Encoding type (rate/population) */
    float max_spike_rate;               /**< Maximum firing rate (Hz) */
    float min_spike_rate;               /**< Baseline firing rate (Hz) */
    float temporal_window_ms;           /**< Spike integration window (ms) */
    uint32_t neurons_per_pixel;         /**< Neurons encoding each pixel */

    /* Decoding configuration */
    snn_decoding_t decoding_method;     /**< Decoding type (rate/population) */
    float decode_window_ms;             /**< Decoding time window (ms) */
    bool use_population_vector;         /**< Use population vector decoding */

    /* Frame processing */
    uint32_t frame_width;               /**< Expected frame width */
    uint32_t frame_height;              /**< Expected frame height */
    uint32_t frame_channels;            /**< 1=grayscale, 3=RGB */
    bool downsample_frames;             /**< Downsample before encoding */
    uint32_t downsample_factor;         /**< Downsample factor (2, 4, 8) */

    /* Attention modulation */
    bool use_attention_modulation;      /**< Modulate rates by attention */
    float attention_gain_min;           /**< Min attention gain factor */
    float attention_gain_max;           /**< Max attention gain factor */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    float update_interval_ms;           /**< Update interval (ms) */

} snn_visual_config_t;

/**
 * @brief Visual encoding statistics
 *
 * WHAT: Metrics for visual-to-spike conversion
 * WHY:  Monitor encoding efficiency and quality
 * HOW:  Track spikes, rates, and frame processing
 */
typedef struct snn_visual_encode_stats_s {
    uint64_t frames_encoded;            /**< Total frames encoded */
    uint64_t total_spikes;              /**< Total spikes generated */
    float avg_spikes_per_frame;         /**< Average spikes per frame */
    float avg_encoding_time_ms;         /**< Average encoding time (ms) */
    float peak_spike_rate;              /**< Peak firing rate observed (Hz) */
    float mean_spike_rate;              /**< Mean firing rate (Hz) */
} snn_visual_encode_stats_t;

/**
 * @brief Visual decoding statistics
 *
 * WHAT: Metrics for spike-to-visual conversion
 * WHY:  Monitor decoding quality
 * HOW:  Track decoded frames and reconstruction error
 */
typedef struct snn_visual_decode_stats_s {
    uint64_t frames_decoded;            /**< Total frames decoded */
    float avg_decoding_time_ms;         /**< Average decoding time (ms) */
    float reconstruction_error;         /**< MSE of reconstruction */
    uint32_t silent_outputs;            /**< Outputs with no spikes */
} snn_visual_decode_stats_t;

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Visual-SNN bridge structure
 *
 * WHAT: Context for visual cortex to SNN integration
 * WHY:  Maintain state and references for bidirectional flow
 * HOW:  Store SNN, visual cortex, encoders/decoders, and stats
 */
typedef struct snn_visual_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Connected systems */
    snn_network_t* snn;                 /**< SNN network */
    visual_cortex_t* visual_cortex;     /**< Visual cortex module */

    /* Configuration */
    snn_visual_config_t config;         /**< Bridge configuration */

    /* Encoding/Decoding */
    snn_encoder_t* encoder;             /**< Visual-to-spike encoder */
    snn_decoder_t* decoder;             /**< Spike-to-visual decoder */

    /* Working buffers */
    uint8_t* frame_buffer;              /**< Input frame buffer */
    uint8_t* downsample_buffer;         /**< Downsampled frame buffer */
    float* spike_input_buffer;          /**< Normalized pixel values */
    float* spike_output_buffer;         /**< Decoded output values */
    uint8_t* spike_mask;                /**< Binary spike mask */

    /* Attention integration */
    attention_map_t* attention_map;     /**< Attention map from visual cortex */
    float* attention_gains;             /**< Per-neuron attention gains */

    /* Statistics */
    snn_visual_encode_stats_t encode_stats; /**< Encoding statistics */
    snn_visual_decode_stats_t decode_stats; /**< Decoding statistics */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time_ms;          /**< Last update timestamp */
    uint32_t frame_count;               /**< Frame counter */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Thread safety */
    void* mutex;                        /**< Mutex for thread safety */

} snn_visual_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize visual bridge config with defaults
 *
 * WHAT: Set sensible default parameters
 * WHY:  Convenient initialization
 * HOW:  Values for typical visual processing
 *
 * DEFAULTS:
 * - Rate coding at 50-200 Hz
 * - 640x480 grayscale frames
 * - 1 neuron per pixel
 * - Attention modulation enabled
 *
 * @param config Config to initialize
 */
void snn_visual_config_default(snn_visual_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create visual-SNN bridge
 *
 * WHAT: Initialize bidirectional bridge between visual cortex and SNN
 * WHY:  Enable spike-based visual processing
 * HOW:  Allocate buffers, create encoder/decoder, set up connections
 *
 * @param config Bridge configuration
 * @param snn SNN network to connect
 * @param visual_cortex Visual cortex module to connect
 * @return Bridge instance or NULL on failure
 */
snn_visual_bridge_t* snn_visual_bridge_create(
    const snn_visual_config_t* config,
    snn_network_t* snn,
    visual_cortex_t* visual_cortex
);

/**
 * @brief Destroy visual-SNN bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper memory management
 * HOW:  Free buffers, destroy encoder/decoder, disconnect
 *
 * @param bridge Bridge to destroy
 */
void snn_visual_bridge_destroy(snn_visual_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging for visual events
 * WHY:  Distributed spike event coordination
 * HOW:  Register with bio-router as BIO_MODULE_SNN_VISUAL
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_visual_bridge_connect_bio_async(snn_visual_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_visual_bridge_disconnect_bio_async(snn_visual_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_visual_bridge_is_bio_async_connected(const snn_visual_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode visual frame to spike trains
 *
 * WHAT: Convert image pixels to spike trains
 * WHY:  Enable SNN processing of visual input
 * HOW:  Rate/population encoding with optional attention modulation
 *
 * ALGORITHM:
 * 1. Optionally downsample frame
 * 2. Normalize pixel values [0, 1]
 * 3. Apply attention modulation if enabled
 * 4. Encode to spike trains using configured method
 * 5. Feed spikes to SNN input layer
 *
 * @param bridge Visual-SNN bridge
 * @param frame Input frame [height × width × channels]
 * @param width Frame width
 * @param height Frame height
 * @param channels Number of channels (1 or 3)
 * @param spike_trains Output spike trains
 * @return 0 on success, error code on failure
 */
int snn_visual_bridge_encode(
    snn_visual_bridge_t* bridge,
    const uint8_t* frame,
    uint32_t width,
    uint32_t height,
    uint32_t channels,
    snn_spike_train_t** spike_trains
);

/**
 * @brief Encode visual features to spikes
 *
 * WHAT: Convert visual cortex features to spike trains
 * WHY:  Enable SNN processing of high-level visual features
 * HOW:  Population coding of feature vector
 *
 * @param bridge Visual-SNN bridge
 * @param features Feature vector from visual_cortex_process()
 * @param num_features Feature vector dimension
 * @param spike_trains Output spike trains
 * @return 0 on success, error code on failure
 */
int snn_visual_bridge_encode_features(
    snn_visual_bridge_t* bridge,
    const float* features,
    uint32_t num_features,
    snn_spike_train_t** spike_trains
);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Decode spike trains to visual frame
 *
 * WHAT: Convert SNN output spikes to image pixels
 * WHY:  Reconstruct visual output from spike activity
 * HOW:  Rate/population decoding to pixel values
 *
 * @param bridge Visual-SNN bridge
 * @param spike_trains Input spike trains
 * @param num_trains Number of spike trains
 * @param frame_out Output frame [height × width × channels]
 * @return 0 on success, error code on failure
 */
int snn_visual_bridge_decode(
    snn_visual_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    uint8_t* frame_out
);

/**
 * @brief Decode spike trains to visual features
 *
 * WHAT: Convert SNN output spikes to feature vector
 * WHY:  Extract high-level visual features from SNN
 * HOW:  Population vector decoding
 *
 * @param bridge Visual-SNN bridge
 * @param spike_trains Input spike trains
 * @param num_trains Number of spike trains
 * @param features_out Output feature vector
 * @param num_features Feature vector dimension
 * @return 0 on success, error code on failure
 */
int snn_visual_bridge_decode_features(
    snn_visual_bridge_t* bridge,
    const snn_spike_train_t* spike_trains,
    uint32_t num_trains,
    float* features_out,
    uint32_t num_features
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update bridge state and process visual input
 *
 * WHAT: Full update cycle for visual-SNN bridge
 * WHY:  Single call for regular processing
 * HOW:  Encode frame, update SNN, decode output
 *
 * @param bridge Bridge to update
 * @param dt Simulation timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_visual_bridge_update(snn_visual_bridge_t* bridge, float dt);

/**
 * @brief Update attention modulation from visual cortex
 *
 * WHAT: Get attention map and compute per-neuron gains
 * WHY:  Modulate spike rates by visual salience
 * HOW:  Sample attention map, scale encoding rates
 *
 * @param bridge Bridge to update
 * @return 0 on success, error code on failure
 */
int snn_visual_bridge_update_attention(snn_visual_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get encoding statistics
 *
 * @param bridge Bridge to query
 * @param stats Output encoding statistics
 * @return 0 on success
 */
int snn_visual_bridge_get_encode_stats(
    const snn_visual_bridge_t* bridge,
    snn_visual_encode_stats_t* stats
);

/**
 * @brief Get decoding statistics
 *
 * @param bridge Bridge to query
 * @param stats Output decoding statistics
 * @return 0 on success
 */
int snn_visual_bridge_get_decode_stats(
    const snn_visual_bridge_t* bridge,
    snn_visual_decode_stats_t* stats
);

/**
 * @brief Get current spike rate for pixel/feature
 *
 * @param bridge Bridge to query
 * @param index Pixel/feature index
 * @return Current spike rate (Hz), -1.0 on error
 */
float snn_visual_bridge_get_spike_rate(
    const snn_visual_bridge_t* bridge,
    uint32_t index
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge to check
 * @return true if connected and active
 */
bool snn_visual_bridge_is_active(const snn_visual_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_visual_bridge_reset_stats(snn_visual_bridge_t* bridge);

//=============================================================================
// Bio-Async Module ID
//=============================================================================

/** Bio-async module ID for visual-SNN bridge (0x0610) */
#define BIO_MODULE_SNN_VISUAL 0x0610

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_VISUAL_BRIDGE_H */

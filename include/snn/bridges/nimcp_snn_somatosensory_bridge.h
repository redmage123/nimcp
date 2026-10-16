/**
 * @file nimcp_snn_somatosensory_bridge.h
 * @brief SNN integration bridge for Somatosensory Cortex
 *
 * WHAT: Bidirectional bridge between SNN and somatosensory cortex
 * WHY:  Enable spike-based body-state processing with population coding
 * HOW:  Population coding for receptors, Gaussian tuning for proprioception
 *
 * BIOLOGICAL BASIS:
 * - S1 cortex uses population coding for touch location and intensity
 * - Proprioceptive neurons have Gaussian tuning curves for joint angles
 * - Cortical magnification: fingertips ~15x, lips ~25x receptor density
 * - Pain pathways use high-rate burst coding for sharp (A-delta) pain
 *   and sustained firing for dull (C-fiber) pain
 *
 * INTEGRATION PATTERN:
 * - Touch events -> population-coded spike trains per body segment
 * - Joint angles -> Gaussian tuning curve population firing rates
 * - Pain signals -> priority burst/sustained spike encoding
 * - SNN output spikes -> population vector decoded body state
 * - Bio-async for somatosensory event messaging
 *
 * @author NIMCP Development Team
 * @date 2026-03-09
 * @version 1.0.0
 */

#ifndef NIMCP_SNN_SOMATOSENSORY_BRIDGE_H
#define NIMCP_SNN_SOMATOSENSORY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_encoding.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Constants
//=============================================================================

#define SNN_SOMA_DEFAULT_BODY_SEGMENTS     45
#define SNN_SOMA_DEFAULT_NEURONS_PER_SEG   16
#define SNN_SOMA_DEFAULT_MAX_RATE          100.0f   /* Hz */
#define SNN_SOMA_DEFAULT_MIN_RATE          2.0f     /* Hz */
#define SNN_SOMA_DEFAULT_TEMPORAL_WINDOW   50.0f    /* ms */
#define SNN_SOMA_MAX_BODY_SEGMENTS         64

/** Pain type classification */
typedef enum {
    SNN_SOMA_PAIN_SHARP = 0,    /**< A-delta fiber: fast, sharp, localized */
    SNN_SOMA_PAIN_DULL,         /**< C-fiber: slow, dull, diffuse */
    SNN_SOMA_PAIN_BURNING,      /**< Thermal nociceptor */
    SNN_SOMA_PAIN_COUNT
} snn_soma_pain_type_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Somatosensory-SNN bridge configuration
 *
 * WHAT: Parameters for somatosensory cortex to SNN integration
 * WHY:  Control encoding/decoding and body map processing
 * HOW:  Body segment count, encoding method, cortical magnification
 */
typedef struct snn_somatosensory_config_s {
    /* Encoding configuration */
    snn_encoding_t encoding_method;     /**< Encoding type (rate/population) */
    float max_spike_rate;               /**< Maximum firing rate (Hz) */
    float min_spike_rate;               /**< Baseline firing rate (Hz) */
    float temporal_window_ms;           /**< Spike integration window (ms) */

    /* Body map configuration */
    uint32_t body_segments;             /**< Number of body segments (homunculus) */
    uint32_t neurons_per_segment;       /**< Neurons encoding each body segment */

    /* Cortical magnification factors (relative to trunk=1.0) */
    float magnification_fingers;        /**< Finger magnification (~15x) */
    float magnification_lips;           /**< Lip magnification (~25x) */
    float magnification_tongue;         /**< Tongue magnification (~20x) */
    float magnification_face;           /**< Face magnification (~10x) */

    /* Proprioception */
    uint32_t proprioception_dims;       /**< Dimensions per joint (pos, vel, tension) */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    float update_interval_ms;           /**< Update interval (ms) */

} snn_somatosensory_config_t;

/**
 * @brief Somatosensory encoding statistics
 *
 * WHAT: Metrics for body-state-to-spike conversion
 * WHY:  Monitor encoding efficiency and coverage
 * HOW:  Track spikes, body segments, pain events
 */
typedef struct snn_somatosensory_encode_stats_s {
    uint64_t touch_events_encoded;      /**< Total touch events encoded */
    uint64_t proprioception_updates;    /**< Total proprioception updates */
    uint64_t pain_events_encoded;       /**< Total pain events encoded */
    uint64_t total_spikes;              /**< Total spikes generated */
    float avg_spikes_per_event;         /**< Average spikes per event */
    float peak_spike_rate;              /**< Peak firing rate observed (Hz) */
    float mean_spike_rate;              /**< Mean firing rate (Hz) */
} snn_somatosensory_encode_stats_t;

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Somatosensory-SNN bridge structure
 *
 * WHAT: Context for somatosensory cortex to SNN integration
 * WHY:  Maintain state for body-state to spike flow
 * HOW:  Store SNN, body map buffers, encoders/decoders, stats
 */
typedef struct snn_somatosensory_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    /* Connected systems */
    snn_network_t* snn;                 /**< SNN network */

    /* Configuration */
    snn_somatosensory_config_t config;  /**< Bridge configuration */

    /* Encoding/Decoding */
    snn_encoder_t* encoder;             /**< Body-state-to-spike encoder */
    snn_decoder_t* decoder;             /**< Spike-to-body-state decoder */

    /* Working buffers */
    float* receptor_buffer;             /**< Per-receptor activation [segments * neurons_per_seg] */
    float* proprioception_buffer;       /**< Joint angles/velocities [segments * proprio_dims] */
    float* body_map_buffer;             /**< Homunculus activation map [segments] */
    float* magnification_table;         /**< Per-segment magnification factor [segments] */
    float* tuning_preferred_angles;     /**< Preferred angle per neuron [segments * neurons_per_seg] */

    /* Statistics */
    snn_somatosensory_encode_stats_t stats; /**< Encoding statistics */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time_ms;          /**< Last update timestamp */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Thread safety */
    void* mutex;                        /**< Mutex for thread safety */

} snn_somatosensory_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize somatosensory bridge config with defaults
 *
 * WHAT: Set sensible default parameters for body-state encoding
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Rate coding, 45 body segments, cortical magnification
 *
 * @param config Config to initialize
 */
void snn_somatosensory_config_default(snn_somatosensory_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create somatosensory-SNN bridge
 *
 * WHAT: Initialize bridge for body-state to spike encoding
 * WHY:  Enable spike-based somatosensory processing
 * HOW:  Allocate buffers, create encoder, init magnification table
 *
 * @param config Bridge configuration
 * @param snn SNN network to connect (may be NULL)
 * @return Bridge instance or NULL on failure
 */
snn_somatosensory_bridge_t* snn_somatosensory_bridge_create(
    const snn_somatosensory_config_t* config,
    snn_network_t* snn
);

/**
 * @brief Destroy somatosensory-SNN bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper memory management
 * HOW:  Free all buffers and encoder/decoder
 *
 * @param bridge Bridge to destroy
 */
void snn_somatosensory_bridge_destroy(snn_somatosensory_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

int snn_somatosensory_bridge_connect_bio_async(snn_somatosensory_bridge_t* bridge);
int snn_somatosensory_bridge_disconnect_bio_async(snn_somatosensory_bridge_t* bridge);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode touch event to spike trains
 *
 * WHAT: Convert touch receptor activation to population-coded spikes
 * WHY:  Spike-based representation of tactile information
 * HOW:  Cortical magnification scales spike rate by body region;
 *        population code distributes across neurons_per_segment
 *
 * @param bridge Somatosensory-SNN bridge
 * @param body_segment Body segment index (0-based)
 * @param intensity Touch intensity [0, 1]
 * @param velocity Stimulus velocity (affects rapid-adapting receptors)
 * @param texture Texture roughness [0, 1]
 * @return 0 on success, -1 on failure
 */
int snn_somatosensory_bridge_encode_touch(
    snn_somatosensory_bridge_t* bridge,
    uint32_t body_segment,
    float intensity,
    float velocity,
    float texture
);

/**
 * @brief Encode proprioceptive state to spike trains
 *
 * WHAT: Convert joint angle/velocity to Gaussian-tuned population spikes
 * WHY:  Spike-based body position awareness
 * HOW:  Each neuron has a preferred angle; Gaussian tuning curve
 *        centered on that angle produces firing rate
 *
 * @param bridge Somatosensory-SNN bridge
 * @param segment Body segment index
 * @param position Joint angle [0, 2*PI]
 * @param velocity Angular velocity (rad/s)
 * @param muscle_tension Muscle tension [0, 1]
 * @return 0 on success, -1 on failure
 */
int snn_somatosensory_bridge_encode_proprioception(
    snn_somatosensory_bridge_t* bridge,
    uint32_t segment,
    float position,
    float velocity,
    float muscle_tension
);

/**
 * @brief Encode pain signal to spike trains
 *
 * WHAT: Convert nociceptive signal to priority-encoded spikes
 * WHY:  Pain must override other sensory processing
 * HOW:  Sharp pain -> high-rate burst; dull pain -> sustained lower rate
 *
 * @param bridge Somatosensory-SNN bridge
 * @param segment Body segment index
 * @param intensity Pain intensity [0, 1]
 * @param type Pain type (sharp/dull/burning)
 * @return 0 on success, -1 on failure
 */
int snn_somatosensory_bridge_encode_pain(
    snn_somatosensory_bridge_t* bridge,
    uint32_t segment,
    float intensity,
    snn_soma_pain_type_t type
);

/**
 * @brief Decode spike trains to body state estimate
 *
 * WHAT: Population vector decoding of joint angles from spikes
 * WHY:  Recover continuous body state from spike representation
 * HOW:  Weighted sum of preferred angles by firing rate
 *
 * @param bridge Somatosensory-SNN bridge
 * @param spike_rates Per-neuron firing rates [segments * neurons_per_seg]
 * @param position_out Decoded positions per segment [segments]
 * @param velocity_out Decoded velocities per segment [segments]
 * @return 0 on success, -1 on failure
 */
int snn_somatosensory_bridge_decode_body_state(
    snn_somatosensory_bridge_t* bridge,
    const float* spike_rates,
    float* position_out,
    float* velocity_out
);

//=============================================================================
// Query Functions
//=============================================================================

int snn_somatosensory_bridge_get_stats(
    const snn_somatosensory_bridge_t* bridge,
    snn_somatosensory_encode_stats_t* stats
);

//=============================================================================
// Bio-Async Module ID
//=============================================================================

/** Bio-async module ID for somatosensory-SNN bridge (0x0614) */
#define BIO_MODULE_SNN_SOMATOSENSORY 0x0614

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_SOMATOSENSORY_BRIDGE_H */

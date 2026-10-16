/**
 * @file nimcp_cochlea_occipital_bridge.h
 * @brief Cochlea-Occipital (Visual) bidirectional bridge
 *
 * WHAT: Audiovisual binding between cochlea and visual cortex
 * WHY:  McGurk effect, lip reading, sound-source localization
 * HOW:  Temporal alignment, spatial binding, cross-modal attention
 *
 * BIOLOGICAL BASIS:
 * - Superior temporal sulcus (STS): Audiovisual integration
 * - McGurk effect: Visual lip movements alter auditory percept
 * - Sound-induced flash illusion: Audio affects visual perception
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea -> Occipital: Speech envelope, sound azimuth, echo map
 * - INBOUND:  Occipital -> Cochlea: Lip positions, visual gaze, object locations
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_OCCIPITAL_BRIDGE_H
#define NIMCP_COCHLEA_OCCIPITAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_cochlea.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct occipital_adapter occipital_adapter_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_OCCIPITAL_ENVELOPE_SIZE     256
#define COCHLEA_OCCIPITAL_PHONEME_SIZE      64
#define COCHLEA_OCCIPITAL_AV_OFFSET_MS      150.0f  /* Lips lead audio */

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Audio to visual data (outbound)
 */
typedef struct {
    float speech_envelope[COCHLEA_OCCIPITAL_ENVELOPE_SIZE];
    float phoneme_features[COCHLEA_OCCIPITAL_PHONEME_SIZE];
    float sound_azimuth_deg;          /**< Sound direction */
    float sound_elevation_deg;        /**< Sound elevation */
    bool has_echo_map;                /**< Echolocation data available */
    void* echo_spatial_map;           /**< Echolocation result pointer */
} cochlea_audio_to_visual_t;

/**
 * @brief Visual to audio data (inbound)
 */
typedef struct {
    float lip_aperture;               /**< Mouth opening */
    float lip_protrusion;             /**< Lip protrusion */
    float jaw_position;               /**< Jaw position */
    float visual_gaze_x;              /**< Gaze X position */
    float visual_gaze_y;              /**< Gaze Y position */
    float expected_sound_azimuth;     /**< Expected sound direction from vision */
    bool face_detected;               /**< Face detected in visual field */
} cochlea_visual_to_audio_t;

/**
 * @brief Visual features from occipital
 */
typedef struct {
    float* feature_vector;            /**< Visual feature vector */
    uint32_t feature_dim;             /**< Feature dimension */
    float lip_aperture;               /**< Lip opening */
    float lip_protrusion;             /**< Lip protrusion */
    float jaw_position;               /**< Jaw position */
    float gaze_x, gaze_y;             /**< Gaze direction */
} occipital_visual_features_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Temporal alignment */
    float av_offset_ms;               /**< A/V offset (lips lead) */
    uint32_t alignment_buffer_size;   /**< Buffer size for alignment */

    /* McGurk processing */
    bool enable_mcgurk;               /**< Enable McGurk effect */
    float visual_weight;              /**< Visual influence weight 0-1 */

    /* Echolocation to visual */
    bool enable_echo_to_visual;       /**< Enable echo -> visual mapping */

    /* Spatial binding */
    float spatial_binding_radius_deg; /**< Spatial binding threshold */
} cochlea_occipital_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_occipital_bridge cochlea_occipital_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_occipital_config_t cochlea_occipital_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_occipital_bridge_t* cochlea_occipital_bridge_create(
    cochlea_t* cochlea,
    occipital_adapter_t* occipital,
    const cochlea_occipital_config_t* config
);

void cochlea_occipital_bridge_destroy(cochlea_occipital_bridge_t* bridge);

nimcp_error_t cochlea_occipital_bridge_update(
    cochlea_occipital_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_occipital_bridge_reset(cochlea_occipital_bridge_t* bridge);

//=============================================================================
// Audio to Visual (Outbound)
//=============================================================================

/**
 * @brief Send audio features to occipital
 */
nimcp_error_t cochlea_occipital_send_audio(
    cochlea_occipital_bridge_t* bridge,
    const cochlea_output_t* output
);

/**
 * @brief Get outbound state
 */
nimcp_error_t cochlea_occipital_get_audio_to_visual(
    const cochlea_occipital_bridge_t* bridge,
    cochlea_audio_to_visual_t* state
);

//=============================================================================
// Visual to Audio (Inbound)
//=============================================================================

/**
 * @brief Receive visual features from occipital
 */
nimcp_error_t cochlea_occipital_receive_visual(
    cochlea_occipital_bridge_t* bridge,
    const occipital_visual_features_t* features
);

/**
 * @brief Get inbound state
 */
nimcp_error_t cochlea_occipital_get_visual_to_audio(
    const cochlea_occipital_bridge_t* bridge,
    cochlea_visual_to_audio_t* state
);

//=============================================================================
// Audiovisual Binding
//=============================================================================

/**
 * @brief Perform audiovisual binding
 */
nimcp_error_t cochlea_occipital_bind(cochlea_occipital_bridge_t* bridge);

/**
 * @brief Check if A/V are bound (temporally/spatially aligned)
 */
bool cochlea_occipital_is_bound(const cochlea_occipital_bridge_t* bridge);

/**
 * @brief Get binding confidence
 */
float cochlea_occipital_get_binding_confidence(const cochlea_occipital_bridge_t* bridge);

//=============================================================================
// McGurk Effect
//=============================================================================

/**
 * @brief Enable/disable McGurk processing
 */
nimcp_error_t cochlea_occipital_set_mcgurk(
    cochlea_occipital_bridge_t* bridge,
    bool enable
);

/**
 * @brief Set visual influence weight
 */
nimcp_error_t cochlea_occipital_set_visual_weight(
    cochlea_occipital_bridge_t* bridge,
    float weight
);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_occipital_verify_bidirectional(const cochlea_occipital_bridge_t* bridge);
uint64_t cochlea_occipital_get_last_outbound(const cochlea_occipital_bridge_t* bridge);
uint64_t cochlea_occipital_get_last_inbound(const cochlea_occipital_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_OCCIPITAL_BRIDGE_H */

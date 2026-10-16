/**
 * @file nimcp_cochlea_broca_bridge.h
 * @brief Cochlea-Broca's Region bidirectional bridge
 *
 * WHAT: Connect auditory input to speech production system
 * WHY:  Speech perception-production link, phonological processing
 * HOW:  Feed phonemes to Broca, receive articulatory expectations
 *
 * BIOLOGICAL BASIS:
 * - Wernicke-Geschwind model: Auditory -> Wernicke -> Broca
 * - Mirror neurons: Hearing speech activates speech production areas
 * - Phonological loop: Subvocal rehearsal for working memory
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea -> Broca: Phoneme features, speech envelope, prosody
 * - INBOUND:  Broca -> Cochlea: Predicted phonemes, articulatory templates
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_BROCA_BRIDGE_H
#define NIMCP_COCHLEA_BROCA_BRIDGE_H

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

typedef struct broca_adapter broca_adapter_t;
typedef struct phonological_processor phonological_processor_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_BROCA_MAX_PHONEMES          64
#define COCHLEA_BROCA_ENVELOPE_SIZE         256
#define COCHLEA_BROCA_PITCH_SIZE            64
#define COCHLEA_BROCA_LOOP_SIZE             1024

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Audio to speech data (outbound)
 */
typedef struct {
    float* phoneme_activations;       /**< Detected phonemes */
    uint32_t num_phonemes;            /**< Number of phonemes */
    float speech_envelope[COCHLEA_BROCA_ENVELOPE_SIZE];
    float pitch_contour[COCHLEA_BROCA_PITCH_SIZE];
    float speech_rate;                /**< Speech rate (syllables/sec) */
    bool voice_detected;              /**< Voice activity detected */
    float fundamental_freq_hz;        /**< F0 frequency */
} cochlea_audio_to_speech_t;

/**
 * @brief Speech to audio data (inbound) - Predictions
 */
typedef struct {
    float* expected_phonemes;         /**< What Broca expects to hear */
    uint32_t num_expected;            /**< Number of expected phonemes */
    float* articulatory_template;     /**< Motor-to-auditory mapping */
    uint32_t template_size;           /**< Template size */
    bool subvocal_active;             /**< Inner speech mode */
} cochlea_speech_to_audio_t;

/**
 * @brief Broca prediction structure
 */
typedef struct {
    float* expected_phonemes;         /**< Expected phoneme activations */
    uint32_t num_expected;            /**< Number of expectations */
    float* articulatory_template;     /**< Articulatory template */
    uint32_t template_size;           /**< Template size */
    float confidence;                 /**< Prediction confidence */
} broca_prediction_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Phonological processing */
    bool enable_phoneme_detection;    /**< Detect phonemes */
    uint32_t max_phonemes;            /**< Max phonemes to track */

    /* Phonological loop */
    bool enable_phonological_loop;    /**< Enable rehearsal loop */
    uint32_t loop_buffer_size;        /**< Loop buffer size */
    float loop_decay_rate;            /**< Decay rate per second */

    /* Mirror activation */
    bool enable_mirror_activation;    /**< Enable mirror neurons */
    float mirror_threshold;           /**< Activation threshold */

    /* Predictions */
    bool enable_predictions;          /**< Receive predictions from Broca */
} cochlea_broca_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_broca_bridge cochlea_broca_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_broca_config_t cochlea_broca_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_broca_bridge_t* cochlea_broca_bridge_create(
    cochlea_t* cochlea,
    broca_adapter_t* broca,
    const cochlea_broca_config_t* config
);

void cochlea_broca_bridge_destroy(cochlea_broca_bridge_t* bridge);

nimcp_error_t cochlea_broca_bridge_update(
    cochlea_broca_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_broca_bridge_reset(cochlea_broca_bridge_t* bridge);

//=============================================================================
// Phoneme Sending (Outbound)
//=============================================================================

/**
 * @brief Send phoneme features to Broca
 */
nimcp_error_t cochlea_broca_send_phonemes(
    cochlea_broca_bridge_t* bridge,
    const float* phoneme_features,
    uint32_t num_features
);

/**
 * @brief Get outbound state
 */
nimcp_error_t cochlea_broca_get_audio_to_speech(
    const cochlea_broca_bridge_t* bridge,
    cochlea_audio_to_speech_t* state
);

//=============================================================================
// Predictions (Inbound)
//=============================================================================

/**
 * @brief Receive articulatory predictions from Broca
 */
nimcp_error_t cochlea_broca_receive_predictions(
    cochlea_broca_bridge_t* bridge,
    broca_prediction_t* predictions
);

/**
 * @brief Get inbound state
 */
nimcp_error_t cochlea_broca_get_speech_to_audio(
    const cochlea_broca_bridge_t* bridge,
    cochlea_speech_to_audio_t* state
);

//=============================================================================
// Phonological Loop
//=============================================================================

/**
 * @brief Activate phonological loop
 */
nimcp_error_t cochlea_broca_activate_loop(cochlea_broca_bridge_t* bridge);

/**
 * @brief Deactivate phonological loop
 */
nimcp_error_t cochlea_broca_deactivate_loop(cochlea_broca_bridge_t* bridge);

/**
 * @brief Check if loop is active
 */
bool cochlea_broca_is_loop_active(const cochlea_broca_bridge_t* bridge);

/**
 * @brief Get loop content
 */
nimcp_error_t cochlea_broca_get_loop_content(
    const cochlea_broca_bridge_t* bridge,
    float* buffer,
    uint32_t* buffer_size
);

//=============================================================================
// Mirror Activation
//=============================================================================

/**
 * @brief Get mirror neuron activation level
 */
float cochlea_broca_get_mirror_activation(const cochlea_broca_bridge_t* bridge);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_broca_verify_bidirectional(const cochlea_broca_bridge_t* bridge);
uint64_t cochlea_broca_get_last_outbound(const cochlea_broca_bridge_t* bridge);
uint64_t cochlea_broca_get_last_inbound(const cochlea_broca_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_BROCA_BRIDGE_H */

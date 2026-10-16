/**
 * @file nimcp_occipital_audiovisual_bridge.c
 * @brief Implementation of Occipital-Audiovisual integration bridge
 *
 * WHAT: Integrates occipital visual processing with audio cortex and Broca's region
 * WHY: Enable audiovisual speech processing (lip reading, gesture-speech binding)
 * HOW: Extracts visual speech features and routes to audio/speech modules
 *
 * BIOLOGICAL BASIS:
 * - Superior Temporal Sulcus (STS) integrates visual and auditory speech cues
 * - Lip movements precede audio by ~150ms, enabling predictive processing
 * - McGurk effect demonstrates visual-audio speech integration
 * - Mirror neurons link gesture observation to motor plans in Broca's area
 *
 * @version Phase O1: Occipital Audiovisual Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#define _POSIX_C_SOURCE 200809L

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_audiovisual_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_audiovisual_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OCCIPITAL_AUDIOVISUAL_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define AV_BRIDGE_LOG_MODULE "OCC_AV_BRIDGE"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define MAX_OBSERVATIONS 64
#define MAX_BINDINGS 32
#define MAX_PREDICTIONS 16
#define LIP_FEATURE_SIZE 16
#define PREDICTION_HISTORY_SIZE 8

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Lip reading state
 */
typedef struct {
    float lip_aperture;           /**< Vertical opening (0=closed, 1=wide) */
    float lip_rounding;           /**< Lip rounding (0=spread, 1=pursed) */
    float jaw_position;           /**< Jaw position (0=closed, 1=open) */
    float tongue_visible;         /**< Visible tongue tip position */
    float lip_velocity[2];        /**< Lip motion velocity (dx, dy) */
    uint32_t estimated_phoneme;   /**< Estimated phoneme from visemes */
    float phoneme_confidence;     /**< Confidence in phoneme estimate */
    uint64_t last_update_us;      /**< Last update timestamp */
} lip_reading_state_t;

/**
 * @brief Gesture state for gesture-speech binding
 */
typedef struct {
    float hand_position[3];       /**< Hand centroid (x, y, z) */
    float hand_velocity[3];       /**< Hand motion velocity */
    float gesture_phase;          /**< 0-1 gesture phase (prep/stroke/retract) */
    uint32_t gesture_type;        /**< Detected gesture category */
    float gesture_confidence;     /**< Detection confidence */
    bool is_co_speech;            /**< True if co-speech gesture */
    uint64_t onset_time_us;       /**< Gesture onset time */
} gesture_state_t;

/**
 * @brief Prediction entry for temporal prediction
 */
typedef struct {
    uint64_t prediction_time_us;  /**< When prediction was made */
    float predicted_onset_ms;     /**< Predicted audio onset */
    uint32_t predicted_phoneme;   /**< Predicted phoneme */
    float prediction_confidence;  /**< Confidence at prediction time */
    float actual_onset_ms;        /**< Actual onset (filled after event) */
    uint32_t actual_phoneme;      /**< Actual phoneme (filled after event) */
    bool verified;                /**< True if verified with actual */
} prediction_entry_t;

/**
 * @brief Audiovisual binding entry
 */
typedef struct {
    uint32_t visual_feature_id;   /**< Visual feature ID */
    uint32_t audio_feature_id;    /**< Audio feature ID */
    float binding_strength;       /**< Binding strength [0-1] */
    float temporal_coherence;     /**< Temporal coherence score */
    uint64_t creation_time_us;    /**< When binding was created */
    uint64_t last_reinforced_us;  /**< Last reinforcement time */
    uint32_t reinforcement_count; /**< Number of reinforcements */
} av_binding_entry_t;

/**
 * @brief Internal bridge structure
 */
struct occipital_audiovisual_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    occipital_av_config_t config;

    /* Connected modules */
    occipital_adapter_t* occipital;
    audio_cortex_t* audio_cortex;
    broca_adapter_t* broca;
    speech_cortex_t* speech_cortex;
    bio_router_t router;

    /* Module ID for bio-async */
    uint32_t module_id;

    /* Lip reading state */
    lip_reading_state_t lip_state;

    /* Gesture state */
    gesture_state_t gesture_state;

    /* Observation buffer */
    visual_speech_observation_t observations[MAX_OBSERVATIONS];
    uint32_t observation_count;
    uint32_t observation_write_idx;

    /* Audiovisual bindings */
    av_binding_entry_t bindings[MAX_BINDINGS];
    uint32_t binding_count;

    /* Temporal predictions */
    prediction_entry_t predictions[MAX_PREDICTIONS];
    uint32_t prediction_write_idx;

    /* Prediction history for learning */
    float prediction_errors[PREDICTION_HISTORY_SIZE];
    uint32_t error_write_idx;
    float learned_offset_ms;      /**< Learned lip-audio offset */

    /* Current effects */
    occipital_av_effects_t effects;

    /* Statistics */
    occipital_av_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    uint64_t creation_time_us;
};

/*=============================================================================
 * VISEME TABLE - Mapping lip shapes to phoneme categories
 *===========================================================================*/

/**
 * @brief Viseme categories (visual phoneme groupings)
 */
typedef enum {
    VISEME_CLOSED = 0,    /**< /p/, /b/, /m/ - bilabial */
    VISEME_LABIODENTAL,   /**< /f/, /v/ - labiodental */
    VISEME_DENTAL,        /**< /th/ - dental */
    VISEME_ALVEOLAR,      /**< /t/, /d/, /n/, /s/, /z/ */
    VISEME_ROUNDED,       /**< /w/, /oo/, /u/ - rounded lips */
    VISEME_SPREAD,        /**< /ee/, /i/ - spread lips */
    VISEME_OPEN,          /**< /a/, /ah/ - open mouth */
    VISEME_NEUTRAL,       /**< /schwa/ - neutral position */
    VISEME_COUNT
} viseme_category_t;

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Classify lip shape into viseme category
 */
static viseme_category_t classify_lip_viseme(const lip_reading_state_t* lip) {
    /* Simple rule-based classification */

    /* Closed mouth - bilabial */
    if (lip->lip_aperture < 0.1f && lip->lip_rounding < 0.3f) {
        return VISEME_CLOSED;
    }

    /* Labiodental - lower lip tucked under upper teeth */
    if (lip->lip_aperture < 0.2f && lip->lip_rounding > 0.4f &&
        lip->jaw_position < 0.2f) {
        return VISEME_LABIODENTAL;
    }

    /* Rounded - pursed lips */
    if (lip->lip_rounding > 0.7f) {
        return VISEME_ROUNDED;
    }

    /* Spread - wide smile-like */
    if (lip->lip_aperture > 0.3f && lip->lip_rounding < 0.2f &&
        lip->jaw_position < 0.4f) {
        return VISEME_SPREAD;
    }

    /* Open - wide open mouth */
    if (lip->lip_aperture > 0.6f && lip->jaw_position > 0.5f) {
        return VISEME_OPEN;
    }

    /* Dental - tongue visible */
    if (lip->tongue_visible > 0.5f) {
        return VISEME_DENTAL;
    }

    /* Alveolar - slight opening */
    if (lip->lip_aperture > 0.2f && lip->lip_aperture < 0.5f) {
        return VISEME_ALVEOLAR;
    }

    return VISEME_NEUTRAL;
}

/**
 * @brief Update lip state from observation
 */
static void update_lip_state(lip_reading_state_t* state,
                             const visual_speech_observation_t* obs) {
    if (obs->type != VISUAL_SPEECH_LIP_SHAPE &&
        obs->type != VISUAL_SPEECH_LIP_MOTION) {
        return;
    }

    /* Calculate time delta for velocity */
    uint64_t now = get_time_us();
    float dt_s = (float)(now - state->last_update_us) / 1000000.0f;
    if (dt_s < 0.001f) dt_s = 0.016f; /* Default 60fps */

    if (obs->type == VISUAL_SPEECH_LIP_SHAPE) {
        /* Features: [aperture, rounding, jaw, tongue_visible, ...] */
        float old_aperture = state->lip_aperture;
        float old_rounding = state->lip_rounding;

        state->lip_aperture = obs->features[0];
        state->lip_rounding = obs->features[1];
        state->jaw_position = obs->features[2];
        state->tongue_visible = obs->features[3];

        /* Calculate velocity */
        state->lip_velocity[0] = (state->lip_aperture - old_aperture) / dt_s;
        state->lip_velocity[1] = (state->lip_rounding - old_rounding) / dt_s;
    } else if (obs->type == VISUAL_SPEECH_LIP_MOTION) {
        /* Direct velocity from motion detection */
        state->lip_velocity[0] = obs->features[0];
        state->lip_velocity[1] = obs->features[1];
    }

    /* Estimate phoneme from current viseme */
    viseme_category_t viseme = classify_lip_viseme(state);
    state->estimated_phoneme = (uint32_t)viseme;
    state->phoneme_confidence = obs->confidence;
    state->last_update_us = now;
}

/**
 * @brief Update gesture state from observation
 */
static void update_gesture_state(gesture_state_t* state,
                                 const visual_speech_observation_t* obs) {
    if (obs->type != VISUAL_SPEECH_HAND_GESTURE) {
        return;
    }

    uint64_t now = get_time_us();

    /* Update hand position */
    state->hand_position[0] = obs->x;
    state->hand_position[1] = obs->y;
    state->hand_position[2] = obs->features[0]; /* Depth if available */

    /* Velocity from features */
    state->hand_velocity[0] = obs->features[1];
    state->hand_velocity[1] = obs->features[2];
    state->hand_velocity[2] = obs->features[3];

    /* Gesture phase from velocity magnitude */
    float velocity_mag = sqrtf(
        state->hand_velocity[0] * state->hand_velocity[0] +
        state->hand_velocity[1] * state->hand_velocity[1] +
        state->hand_velocity[2] * state->hand_velocity[2]
    );

    /* Simple phase estimation: low=prep, high=stroke, decreasing=retract */
    if (velocity_mag < 0.1f) {
        state->gesture_phase = 0.0f; /* Preparation/hold */
    } else if (velocity_mag > 0.5f) {
        state->gesture_phase = 0.5f; /* Stroke */
    } else {
        state->gesture_phase = 0.8f; /* Retraction */
    }

    state->gesture_type = (uint32_t)obs->features[4];
    state->gesture_confidence = obs->confidence;

    /* Detect gesture onset */
    if (velocity_mag > 0.3f && state->onset_time_us == 0) {
        state->onset_time_us = now;
    } else if (velocity_mag < 0.1f) {
        state->onset_time_us = 0;
    }

    /* Co-speech detection based on timing with lip movement */
    state->is_co_speech = (state->gesture_phase > 0.3f);
}

/**
 * @brief Calculate temporal coherence between visual and audio
 */
static float calculate_temporal_coherence(float visual_time_ms,
                                          float audio_time_ms,
                                          float expected_offset_ms) {
    float actual_offset = audio_time_ms - visual_time_ms;
    float error = fabsf(actual_offset - expected_offset_ms);

    /* Coherence decays with error - gaussian-like falloff */
    float sigma = 50.0f; /* 50ms tolerance */
    float coherence = expf(-(error * error) / (2.0f * sigma * sigma));

    return nimcp_clampf(coherence, 0.0f, 1.0f);
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Send visual speech cue to audio cortex
 * Note: Uses placeholder for bio-async messaging
 */
static int send_visual_speech_cue(occipital_audiovisual_bridge_t* bridge,
                                  const lip_reading_state_t* lip) {
    if (!bridge->router || !bridge->config.enable_bio_async) {
        return 0; /* Silent success if no router */
    }

    /* TODO: Implement actual bio-async message sending
     * For now, just track that we would send a message */
    bridge->stats.messages_sent++;

    (void)lip; /* Unused for now */

    return 0;
}

/**
 * @brief Send gesture cue to Broca's region
 * Note: Uses placeholder for bio-async messaging
 */
static int send_gesture_to_broca(occipital_audiovisual_bridge_t* bridge,
                                 const gesture_state_t* gesture) {
    if (!bridge->router || !bridge->config.enable_bio_async) {
        return 0;
    }

    if (!gesture->is_co_speech) {
        return 0; /* Only send co-speech gestures */
    }

    /* TODO: Implement actual bio-async message sending */
    bridge->stats.messages_sent++;

    return 0;
}

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

occipital_av_config_t occipital_av_default_config(void) {
    occipital_av_config_t config = {
        /* Integration enables */
        .enable_lip_reading = true,
        .enable_gesture_binding = true,
        .enable_face_voice_binding = true,
        .enable_temporal_prediction = true,
        .enable_bio_async = true,

        /* Timing parameters - biologically motivated */
        .lip_audio_offset_ms = 150.0f,   /* Lip leads audio by ~150ms */
        .gesture_window_ms = 500.0f,     /* Gesture-speech binding window */
        .prediction_horizon_ms = 200.0f, /* Forward prediction window */

        /* Confidence thresholds */
        .lip_confidence_threshold = NIMCP_CONFIDENCE_LOW,
        .gesture_confidence_threshold = 0.4f,
        .binding_strength = 0.7f,

        /* Default face ROI (center of frame) */
        .face_roi_x = 0.5f,
        .face_roi_y = 0.4f,
        .face_roi_radius = 0.3f
    };

    return config;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

occipital_audiovisual_bridge_t* occipital_av_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_av_config_t* config) {

    if (!occipital) {
        LOG_ERROR(AV_BRIDGE_LOG_MODULE, "NULL occipital adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital is NULL");
        return NULL;
    }

    occipital_audiovisual_bridge_t* bridge =
        (occipital_audiovisual_bridge_t*)nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR(AV_BRIDGE_LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_av_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    if (bridge_base_init(&bridge->base, 0, "occipital_audiovisual") != 0) { nimcp_free(bridge); return NULL; }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = occipital_av_default_config();
    }

    /* Store occipital reference */
    bridge->occipital = occipital;

    /* Initialize learned offset with biological default */
    bridge->learned_offset_ms = bridge->config.lip_audio_offset_ms;

    /* Set timestamps */
    bridge->creation_time_us = get_time_us();
    bridge->last_update_us = bridge->creation_time_us;

    /* Generate module ID */
    bridge->module_id = BIO_MODULE_OCCIPITAL + 0x10; /* Offset for AV bridge */

    LOG_INFO(AV_BRIDGE_LOG_MODULE, "Audiovisual bridge created");

    return bridge;
}

void occipital_av_bridge_destroy(occipital_audiovisual_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_audiovisual");

    LOG_INFO(AV_BRIDGE_LOG_MODULE, "Destroying audiovisual bridge");

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int occipital_av_bridge_reset(occipital_audiovisual_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Reset lip state */
    memset(&bridge->lip_state, 0, sizeof(bridge->lip_state));

    /* Reset gesture state */
    memset(&bridge->gesture_state, 0, sizeof(bridge->gesture_state));

    /* Clear observations */
    memset(bridge->observations, 0, sizeof(bridge->observations));
    bridge->observation_count = 0;
    bridge->observation_write_idx = 0;

    /* Clear bindings */
    memset(bridge->bindings, 0, sizeof(bridge->bindings));
    bridge->binding_count = 0;

    /* Reset predictions */
    memset(bridge->predictions, 0, sizeof(bridge->predictions));
    bridge->prediction_write_idx = 0;

    /* Reset effects */
    memset(&bridge->effects, 0, sizeof(bridge->effects));

    /* Keep learned offset */

    bridge->last_update_us = get_time_us();

    LOG_DEBUG(AV_BRIDGE_LOG_MODULE, "Bridge reset");

    return 0;
}

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

int occipital_av_connect_audio_cortex(
    occipital_audiovisual_bridge_t* bridge,
    audio_cortex_t* audio) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->audio_cortex = audio;

    LOG_INFO(AV_BRIDGE_LOG_MODULE, "Connected to audio cortex");

    return 0;
}

int occipital_av_connect_broca(
    occipital_audiovisual_bridge_t* bridge,
    broca_adapter_t* broca) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->broca = broca;

    LOG_INFO(AV_BRIDGE_LOG_MODULE, "Connected to Broca's region");

    return 0;
}

int occipital_av_connect_speech_cortex(
    occipital_audiovisual_bridge_t* bridge,
    speech_cortex_t* speech) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->speech_cortex = speech;

    LOG_INFO(AV_BRIDGE_LOG_MODULE, "Connected to speech cortex");

    return 0;
}

int occipital_av_bridge_register_bio_async(
    occipital_audiovisual_bridge_t* bridge,
    struct bio_router_struct* router) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->router = router;

    if (router) {
        /* Router stored for future bio-async messaging
         * Actual handler registration would be done here once
         * the full bio-async infrastructure is integrated */
        LOG_INFO(AV_BRIDGE_LOG_MODULE, "Registered with bio-async router");
    }

    return 0;
}

/*=============================================================================
 * PROCESSING API
 *===========================================================================*/

int occipital_av_process_observation(
    occipital_audiovisual_bridge_t* bridge,
    const visual_speech_observation_t* observation) {

    if (!bridge || !observation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_av_process_observation: required parameter is NULL");
        return -1;
    }

    /* Store observation in circular buffer */
    uint32_t idx = bridge->observation_write_idx;
    bridge->observations[idx] = *observation;
    bridge->observation_write_idx = (idx + 1) % MAX_OBSERVATIONS;
    if (bridge->observation_count < MAX_OBSERVATIONS) {
        bridge->observation_count++;
    }

    /* Update appropriate state */
    switch (observation->type) {
        case VISUAL_SPEECH_LIP_SHAPE:
        case VISUAL_SPEECH_LIP_MOTION:
        case VISUAL_SPEECH_JAW_POSITION:
            if (observation->confidence >= bridge->config.lip_confidence_threshold) {
                update_lip_state(&bridge->lip_state, observation);
                bridge->stats.lip_observations++;

                /* Send to audio cortex if enabled */
                if (bridge->config.enable_lip_reading) {
                    send_visual_speech_cue(bridge, &bridge->lip_state);
                }
            }
            break;

        case VISUAL_SPEECH_HAND_GESTURE:
            if (observation->confidence >= bridge->config.gesture_confidence_threshold) {
                update_gesture_state(&bridge->gesture_state, observation);
                bridge->stats.gesture_observations++;

                /* Send to Broca if co-speech */
                if (bridge->config.enable_gesture_binding) {
                    send_gesture_to_broca(bridge, &bridge->gesture_state);
                }
            }
            break;

        default:
            break;
    }

    return 0;
}

int occipital_av_bridge_update(occipital_audiovisual_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint64_t now = get_time_us();
    float dt_ms = (float)(now - bridge->last_update_us) / 1000.0f;

    /* Update effects based on current state */

    /* Lip-phoneme mapping quality */
    if (bridge->lip_state.last_update_us > 0) {
        uint64_t lip_age_us = now - bridge->lip_state.last_update_us;
        float freshness = expf(-(float)lip_age_us / 100000.0f); /* 100ms decay */
        bridge->effects.lip_phoneme_confidence =
            bridge->lip_state.phoneme_confidence * freshness;
        bridge->effects.predicted_phoneme_id =
            (float)bridge->lip_state.estimated_phoneme;
    }

    /* Gesture-speech binding */
    if (bridge->gesture_state.is_co_speech) {
        bridge->effects.gesture_speech_binding =
            bridge->gesture_state.gesture_confidence;

        /* Emphasis boost from stroke phase gestures */
        if (bridge->gesture_state.gesture_phase > 0.3f &&
            bridge->gesture_state.gesture_phase < 0.7f) {
            bridge->effects.gesture_emphasis_boost =
                0.3f * bridge->gesture_state.gesture_confidence;
        }
    } else {
        bridge->effects.gesture_speech_binding = 0.0f;
        bridge->effects.gesture_emphasis_boost = 0.0f;
    }

    /* Visual speech saliency - combined lip and gesture */
    float lip_saliency = bridge->effects.lip_phoneme_confidence;
    float gesture_saliency = bridge->effects.gesture_speech_binding * 0.5f;
    bridge->effects.visual_speech_saliency =
        nimcp_clampf(lip_saliency + gesture_saliency, 0.0f, 1.0f);

    /* Multimodal integration quality */
    float lip_audio_sync = bridge->effects.lip_audio_coherence;
    float gesture_sync = bridge->effects.gesture_speech_binding;
    bridge->effects.multimodal_integration =
        (lip_audio_sync * 0.6f + gesture_sync * 0.4f);

    /* Update prediction accuracy from history */
    float total_error = 0.0f;
    uint32_t error_count = 0;
    for (uint32_t i = 0; i < PREDICTION_HISTORY_SIZE; i++) {
        if (bridge->prediction_errors[i] > 0.0f) {
            total_error += bridge->prediction_errors[i];
            error_count++;
        }
    }
    if (error_count > 0) {
        float avg_error = total_error / (float)error_count;
        /* Convert error to accuracy (100ms error = 0.5 accuracy) */
        float new_accuracy = expf(-avg_error / 100.0f);
        if (isfinite(new_accuracy)) {
            bridge->effects.prediction_accuracy = new_accuracy;
        }
    }

    /* Decay old bindings */
    for (uint32_t i = 0; i < bridge->binding_count; i++) {
        av_binding_entry_t* binding = &bridge->bindings[i];
        uint64_t age_us = now - binding->last_reinforced_us;
        float decay = expf(-(float)age_us / 1000000.0f); /* 1s decay */
        binding->binding_strength *= decay;
    }

    /* Remove very weak bindings */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < bridge->binding_count; i++) {
        if (bridge->bindings[i].binding_strength > 0.1f) {
            if (write_idx != i) {
                bridge->bindings[write_idx] = bridge->bindings[i];
            }
            write_idx++;
        }
    }
    bridge->binding_count = write_idx;

    /* Calculate average binding strength */
    if (bridge->binding_count > 0) {
        float total_strength = 0.0f;
        for (uint32_t i = 0; i < bridge->binding_count; i++) {
            total_strength += bridge->bindings[i].binding_strength;
        }
        bridge->stats.avg_binding_strength =
            total_strength / (float)bridge->binding_count;
    }

    bridge->last_update_us = now;

    return 0;
}

int occipital_av_bridge_get_effects(
    const occipital_audiovisual_bridge_t* bridge,
    occipital_av_effects_t* effects) {

    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_av_bridge_get_effects: required parameter is NULL");
        return -1;
    }

    *effects = bridge->effects;

    return 0;
}

int occipital_av_bridge_apply_effects(occipital_audiovisual_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Effects are applied via bio-async messages */
    /* The connected modules receive updates through message handlers */

    return 0;
}

/*=============================================================================
 * PREDICTION API
 *===========================================================================*/

int occipital_av_predict_audio_timing(
    occipital_audiovisual_bridge_t* bridge,
    float* predicted_onset_ms,
    uint32_t* predicted_phoneme_id,
    float* confidence) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_temporal_prediction) {
        /* Feature disabled by config — not an error, just return gracefully */
        return 0;
    }

    /* Use lip velocity to predict timing */
    float velocity_mag = sqrtf(
        bridge->lip_state.lip_velocity[0] * bridge->lip_state.lip_velocity[0] +
        bridge->lip_state.lip_velocity[1] * bridge->lip_state.lip_velocity[1]
    );

    /* High velocity = phoneme transition imminent */
    float urgency = nimcp_clampf(velocity_mag * 2.0f, 0.0f, 1.0f);

    /* Predicted onset = now + learned offset, adjusted by urgency */
    float base_offset = bridge->learned_offset_ms;
    float adjusted_offset = base_offset * (1.0f - 0.3f * urgency);

    if (predicted_onset_ms) {
        *predicted_onset_ms = adjusted_offset;
    }

    if (predicted_phoneme_id) {
        *predicted_phoneme_id = bridge->lip_state.estimated_phoneme;
    }

    if (confidence) {
        /* Confidence based on lip detection quality and prediction history */
        *confidence = bridge->lip_state.phoneme_confidence *
                      (0.5f + 0.5f * bridge->effects.prediction_accuracy);
    }

    /* Store prediction */
    uint32_t idx = bridge->prediction_write_idx;
    bridge->predictions[idx].prediction_time_us = get_time_us();
    bridge->predictions[idx].predicted_onset_ms = adjusted_offset;
    bridge->predictions[idx].predicted_phoneme = bridge->lip_state.estimated_phoneme;
    bridge->predictions[idx].prediction_confidence =
        bridge->lip_state.phoneme_confidence;
    bridge->predictions[idx].verified = false;
    bridge->prediction_write_idx = (idx + 1) % MAX_PREDICTIONS;

    bridge->stats.predictions_made++;

    return 0;
}

int occipital_av_report_audio_event(
    occipital_audiovisual_bridge_t* bridge,
    float actual_onset_ms,
    uint32_t actual_phoneme_id) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    uint64_t now = get_time_us();

    /* Find matching prediction and verify */
    for (uint32_t i = 0; i < MAX_PREDICTIONS; i++) {
        prediction_entry_t* pred = &bridge->predictions[i];

        if (pred->verified || pred->prediction_time_us == 0) continue;

        /* Check if this prediction is for the reported event */
        uint64_t pred_age_us = now - pred->prediction_time_us;
        float pred_age_ms = (float)pred_age_us / 1000.0f;

        /* Prediction should be old enough for the event to have occurred */
        if (pred_age_ms < actual_onset_ms - 50.0f) continue;
        if (pred_age_ms > actual_onset_ms + 200.0f) continue;

        /* Verify prediction */
        pred->actual_onset_ms = actual_onset_ms;
        pred->actual_phoneme = actual_phoneme_id;
        pred->verified = true;

        /* Calculate error */
        float timing_error = fabsf(actual_onset_ms - pred->predicted_onset_ms);

        /* Store error for learning */
        bridge->prediction_errors[bridge->error_write_idx] = timing_error;
        bridge->error_write_idx = (bridge->error_write_idx + 1) % PREDICTION_HISTORY_SIZE;

        /* Update learned offset with small learning rate */
        float offset_error = actual_onset_ms - pred->predicted_onset_ms;
        bridge->learned_offset_ms += 0.1f * offset_error;
        bridge->learned_offset_ms = nimcp_clampf(
            bridge->learned_offset_ms, 50.0f, 300.0f);

        /* Track correct predictions */
        bool phoneme_correct = (pred->predicted_phoneme == actual_phoneme_id);
        bool timing_accurate = (timing_error < 50.0f);
        if (phoneme_correct && timing_accurate) {
            bridge->stats.predictions_correct++;
        }

        /* Update coherence */
        bridge->effects.lip_audio_coherence = calculate_temporal_coherence(
            pred_age_ms - pred->predicted_onset_ms,
            pred_age_ms,
            bridge->learned_offset_ms
        );

        break;
    }

    return 0;
}

/*=============================================================================
 * QUERY API
 *===========================================================================*/

int occipital_av_bridge_get_stats(
    const occipital_audiovisual_bridge_t* bridge,
    occipital_av_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_av_bridge_get_stats: required parameter is NULL");
        return -1;
    }

    *stats = bridge->stats;

    /* Calculate average prediction error */
    float total_error = 0.0f;
    uint32_t error_count = 0;
    for (uint32_t i = 0; i < PREDICTION_HISTORY_SIZE; i++) {
        if (bridge->prediction_errors[i] > 0.0f) {
            total_error += bridge->prediction_errors[i];
            error_count++;
        }
    }
    if (error_count > 0) {
        stats->avg_prediction_error_ms = total_error / (float)error_count;
    }

    return 0;
}

void occipital_av_bridge_reset_stats(occipital_audiovisual_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_av_bridge_reset_stats: bridge is NULL");
        return;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->prediction_errors, 0, sizeof(bridge->prediction_errors));
    bridge->error_write_idx = 0;
}

bool occipital_av_is_audio_connected(const occipital_audiovisual_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->audio_cortex != NULL;
}

bool occipital_av_is_broca_connected(const occipital_audiovisual_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->broca != NULL;
}

int occipital_av_bridge_get_config(
    const occipital_audiovisual_bridge_t* bridge,
    occipital_av_config_t* config) {

    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_av_bridge_get_config: required parameter is NULL");
        return -1;
    }

    *config = bridge->config;

    return 0;
}

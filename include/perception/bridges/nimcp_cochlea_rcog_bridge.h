/**
 * @file nimcp_cochlea_rcog_bridge.h
 * @brief Cochlea-Recursive Cognition Engine integration bridge
 *
 * WHAT: Connect cochlear processing to recursive language-model-style cognition
 * WHY:  Enable goal-directed auditory processing, context-aware listening
 * HOW:  Register as tool router capability, provide context variables
 *
 * BIOLOGICAL BASIS:
 * - Top-down auditory attention from prefrontal cortex
 * - Goal-directed listening (cocktail party, selective attention)
 * - Predictive processing of expected sounds
 *
 * BIDIRECTIONAL DATA FLOWS:
 * - OUTBOUND: Cochlea -> RCOG: Audio events, speech detection, sound features
 * - INBOUND:  RCOG -> Cochlea: Attention commands, listening goals, predictions
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_RCOG_BRIDGE_H
#define NIMCP_COCHLEA_RCOG_BRIDGE_H

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

typedef struct rcog_engine rcog_engine_t;
typedef struct rcog_tool rcog_tool_t;
typedef struct rcog_variable rcog_variable_t;

//=============================================================================
// Constants
//=============================================================================

#define COCHLEA_RCOG_MAX_SOUND_CLASSES  16
#define COCHLEA_RCOG_MAX_GOALS          8

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Listening goal from RCOG
 */
typedef struct {
    char target_sound_class[64];     /**< "speech", "alarm", "music" */
    float target_frequency_hz;        /**< Frequency to attend */
    float attention_bandwidth;        /**< Bandwidth of attention (octaves) */
    bool suppress_background;         /**< Cocktail party mode */
    float priority;                   /**< Goal priority 0-1 */
} cochlea_listening_goal_t;

/**
 * @brief Audio event to send to RCOG
 */
typedef struct {
    bool speech_detected;             /**< Speech in audio */
    bool alarm_detected;              /**< Alarm sound detected */
    float dominant_frequency;         /**< Dominant frequency Hz */
    float sound_azimuth_deg;          /**< Sound direction */
    float sound_elevation_deg;        /**< Sound elevation */
    float confidence;                 /**< Detection confidence */
    uint64_t timestamp_ms;            /**< Event timestamp */
} cochlea_audio_event_t;

/**
 * @brief RCOG goal (simplified from rcog engine)
 */
typedef struct {
    char goal_type[64];               /**< Goal type identifier */
    void* goal_data;                  /**< Goal-specific data */
    size_t data_size;                 /**< Size of goal data */
} rcog_goal_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Tool registration */
    bool register_listen_tool;        /**< Register "listen" tool */
    bool register_detect_tool;        /**< Register "detect_sound" tool */
    bool register_localize_tool;      /**< Register "localize" tool */

    /* Context variables */
    bool create_audio_context;        /**< Create audio context variable */
    bool create_sounds_variable;      /**< Create detected sounds variable */

    /* Goal processing */
    float goal_timeout_ms;            /**< Goal timeout */
    uint32_t max_concurrent_goals;    /**< Max concurrent goals */

    /* Update frequency */
    float update_interval_ms;         /**< How often to update RCOG */
} cochlea_rcog_config_t;

/**
 * @brief Bridge instance (opaque)
 */
typedef struct cochlea_rcog_bridge cochlea_rcog_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

cochlea_rcog_config_t cochlea_rcog_config_default(void);

//=============================================================================
// Core API
//=============================================================================

cochlea_rcog_bridge_t* cochlea_rcog_bridge_create(
    cochlea_t* cochlea,
    rcog_engine_t* engine,
    const cochlea_rcog_config_t* config
);

void cochlea_rcog_bridge_destroy(cochlea_rcog_bridge_t* bridge);

nimcp_error_t cochlea_rcog_bridge_update(
    cochlea_rcog_bridge_t* bridge,
    const cochlea_output_t* cochlea_output,
    float dt_ms
);

nimcp_error_t cochlea_rcog_bridge_reset(cochlea_rcog_bridge_t* bridge);

//=============================================================================
// Tool Registration
//=============================================================================

/**
 * @brief Register cochlea as RCOG tool
 */
nimcp_error_t cochlea_rcog_register_tool(cochlea_rcog_bridge_t* bridge);

/**
 * @brief Unregister cochlea tool
 */
nimcp_error_t cochlea_rcog_unregister_tool(cochlea_rcog_bridge_t* bridge);

//=============================================================================
// Goal Processing (Inbound)
//=============================================================================

/**
 * @brief Process listening goal from RCOG
 */
nimcp_error_t cochlea_rcog_receive_goal(
    cochlea_rcog_bridge_t* bridge,
    const rcog_goal_t* goal
);

/**
 * @brief Set listening goal directly
 */
nimcp_error_t cochlea_rcog_set_listening_goal(
    cochlea_rcog_bridge_t* bridge,
    const cochlea_listening_goal_t* goal
);

/**
 * @brief Get current listening goal
 */
nimcp_error_t cochlea_rcog_get_listening_goal(
    const cochlea_rcog_bridge_t* bridge,
    cochlea_listening_goal_t* goal
);

/**
 * @brief Clear listening goal
 */
nimcp_error_t cochlea_rcog_clear_goal(cochlea_rcog_bridge_t* bridge);

//=============================================================================
// Event Sending (Outbound)
//=============================================================================

/**
 * @brief Send audio event to RCOG
 */
nimcp_error_t cochlea_rcog_send_event(
    cochlea_rcog_bridge_t* bridge,
    const cochlea_audio_event_t* event
);

/**
 * @brief Update context variables in RCOG
 */
nimcp_error_t cochlea_rcog_update_context(cochlea_rcog_bridge_t* bridge);

//=============================================================================
// Bidirectional Verification
//=============================================================================

bool cochlea_rcog_verify_bidirectional(const cochlea_rcog_bridge_t* bridge);
uint64_t cochlea_rcog_get_last_outbound(const cochlea_rcog_bridge_t* bridge);
uint64_t cochlea_rcog_get_last_inbound(const cochlea_rcog_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_RCOG_BRIDGE_H */

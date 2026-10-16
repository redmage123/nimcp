//=============================================================================
// nimcp_creative_emotion_bridge.h - Creative-Emotion Integration Bridge
//=============================================================================
/**
 * @file nimcp_creative_emotion_bridge.h
 * @brief Bridge connecting creative system to emotional processing
 *
 * WHAT: Integrates aesthetic experiences with emotional system
 * WHY:  Art appreciation is inherently emotional
 * HOW:  Bidirectional mapping between aesthetic and emotional states
 *
 * THEORETICAL BASIS:
 * - Affect-based aesthetics (Silvia, 2005)
 * - Emotional contagion in art (Juslin & Västfjäll, 2008)
 * - Aesthetic emotions (Scherer, 2004)
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_EMOTION_BRIDGE_H
#define NIMCP_CREATIVE_EMOTION_BRIDGE_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct creative_emotion_bridge creative_emotion_bridge_t;

//=============================================================================
// Emotion-Aesthetic Mapping Types
//=============================================================================

/**
 * @brief Aesthetic emotion categories (beyond basic emotions)
 */
typedef enum {
    AESTHETIC_EMOTION_AWE = 0,         /**< Overwhelming wonder */
    AESTHETIC_EMOTION_SUBLIME,         /**< Transcendent beauty/terror */
    AESTHETIC_EMOTION_BEAUTY,          /**< Pure beauty response */
    AESTHETIC_EMOTION_NOSTALGIA,       /**< Bittersweet remembrance */
    AESTHETIC_EMOTION_CATHARSIS,       /**< Emotional purging */
    AESTHETIC_EMOTION_CHILLS,          /**< Musical frisson */
    AESTHETIC_EMOTION_BEING_MOVED,     /**< Kama muta */
    AESTHETIC_EMOTION_CONTEMPLATION,   /**< Deep reflection */
    AESTHETIC_EMOTION_WONDER,          /**< Curious amazement */
    AESTHETIC_EMOTION_POIGNANCY,       /**< Sweet sadness */
    AESTHETIC_EMOTION_COUNT
} aesthetic_emotion_type_t;

/**
 * @brief Aesthetic-emotional event
 */
typedef struct {
    aesthetic_emotion_type_t type;     /**< Emotion type */
    float intensity;                   /**< [0-1] Intensity */
    float valence;                     /**< [-1,+1] Pleasant/unpleasant */
    float arousal;                     /**< [0-1] Activation level */
    art_modality_t trigger_modality;   /**< What modality triggered this */
    uint64_t timestamp_us;             /**< When it occurred */
    float duration_seconds;            /**< How long it lasted */
} aesthetic_emotion_event_t;

/**
 * @brief Current emotional state from art engagement
 */
typedef struct {
    /* Primary aesthetic emotions */
    float awe;
    float sublime;
    float beauty;
    float nostalgia;
    float catharsis;
    float chills;
    float being_moved;
    float contemplation;
    float wonder;
    float poignancy;

    /* Derived measures */
    float overall_engagement;          /**< [0-1] How engaged with the art */
    float aesthetic_distance;          /**< [0-1] Psychological distance */
    float flow_state;                  /**< [0-1] Flow/absorption level */
    float peak_experience;             /**< [0-1] Peak/transcendent experience */

    /* Temporal dynamics */
    float emotional_trajectory;        /**< [-1,+1] Rising/falling emotion */
    float emotional_variance;          /**< [0-1] Emotional stability */
} aesthetic_emotional_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Emotion bridge configuration
 */
typedef struct {
    /* Sensitivity settings */
    float emotion_sensitivity;         /**< [0-2] Overall sensitivity */
    float awe_threshold;               /**< Threshold for awe response */
    float chills_threshold;            /**< Threshold for musical chills */

    /* Integration settings */
    bool propagate_to_emotion_system;  /**< Push emotions to main system */
    bool receive_from_emotion_system;  /**< Receive emotions from main system */
    float emotion_decay_rate;          /**< How fast emotions decay */

    /* Memory settings */
    bool store_emotional_memories;     /**< Store significant experiences */
    float memory_threshold;            /**< Minimum intensity to store */
    uint32_t max_emotional_memories;   /**< Maximum stored experiences */
} creative_emotion_bridge_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_emotion_bridge_config_defaults(creative_emotion_bridge_config_t* config);

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Creative-emotion bridge
 */
struct creative_emotion_bridge {
    creative_emotion_bridge_config_t config;

    /* Current state */
    aesthetic_emotional_state_t current_state;

    /* Event history */
    aesthetic_emotion_event_t* event_history;
    uint32_t history_capacity;
    uint32_t history_count;
    uint32_t history_index;

    /* Integration */
    void* emotion_system;              /**< External emotion system */
    void* hippocampus;                 /**< For emotional memory storage */

    /* Statistics */
    uint64_t events_processed;
    float avg_engagement;
    uint64_t last_update_us;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create emotion bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge or NULL on error
 */
creative_emotion_bridge_t* creative_emotion_bridge_create(
    const creative_emotion_bridge_config_t* config);

/**
 * @brief Destroy emotion bridge
 *
 * @param bridge Bridge to destroy
 */
void creative_emotion_bridge_destroy(creative_emotion_bridge_t* bridge);

//=============================================================================
// Processing API
//=============================================================================

/**
 * @brief Process aesthetic evaluation for emotional response
 *
 * Takes aesthetic evaluation and extracts emotional components,
 * updating internal state and optionally propagating to emotion system.
 *
 * @param bridge Bridge
 * @param eval Aesthetic evaluation
 * @param out Output emotional event (optional)
 * @return 0 on success, -1 on error
 */
int creative_emotion_process_evaluation(creative_emotion_bridge_t* bridge,
                                         const aesthetic_evaluation_t* eval,
                                         aesthetic_emotion_event_t* out);

/**
 * @brief Update emotional state over time
 *
 * Call periodically to decay emotions and update temporal dynamics.
 *
 * @param bridge Bridge
 * @param dt_us Time delta in microseconds
 * @return 0 on success, -1 on error
 */
int creative_emotion_update(creative_emotion_bridge_t* bridge, uint64_t dt_us);

/**
 * @brief Get current emotional state
 *
 * @param bridge Bridge
 * @param out Output state
 * @return 0 on success, -1 on error
 */
int creative_emotion_get_state(const creative_emotion_bridge_t* bridge,
                                aesthetic_emotional_state_t* out);

//=============================================================================
// Emotion Analysis API
//=============================================================================

/**
 * @brief Detect awe from content
 *
 * Awe = perceived vastness + need for accommodation
 *
 * @param bridge Bridge
 * @param eval Aesthetic evaluation
 * @return Awe intensity [0-1]
 */
float creative_emotion_detect_awe(const creative_emotion_bridge_t* bridge,
                                   const aesthetic_evaluation_t* eval);

/**
 * @brief Detect sublime from content
 *
 * Sublime = beauty + terror/overwhelm
 *
 * @param bridge Bridge
 * @param eval Aesthetic evaluation
 * @return Sublime intensity [0-1]
 */
float creative_emotion_detect_sublime(const creative_emotion_bridge_t* bridge,
                                       const aesthetic_evaluation_t* eval);

/**
 * @brief Detect musical chills/frisson
 *
 * Physical response to musical moments
 *
 * @param bridge Bridge
 * @param music_eval Music evaluation
 * @return Chills intensity [0-1]
 */
float creative_emotion_detect_chills(const creative_emotion_bridge_t* bridge,
                                      const aesthetic_evaluation_t* music_eval);

/**
 * @brief Detect being moved (kama muta)
 *
 * Warm, communal, connection emotion
 *
 * @param bridge Bridge
 * @param eval Aesthetic evaluation
 * @return Being moved intensity [0-1]
 */
float creative_emotion_detect_being_moved(const creative_emotion_bridge_t* bridge,
                                           const aesthetic_evaluation_t* eval);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Set external emotion system
 *
 * @param bridge Bridge
 * @param emotion_system Emotion system pointer
 */
void creative_emotion_set_emotion_system(creative_emotion_bridge_t* bridge,
                                          void* emotion_system);

/**
 * @brief Set hippocampus for memory storage
 *
 * @param bridge Bridge
 * @param hippocampus Hippocampus pointer
 */
void creative_emotion_set_hippocampus(creative_emotion_bridge_t* bridge,
                                       void* hippocampus);

/**
 * @brief Push current emotions to external system
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int creative_emotion_push_to_system(creative_emotion_bridge_t* bridge);

/**
 * @brief Pull emotions from external system
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int creative_emotion_pull_from_system(creative_emotion_bridge_t* bridge);

//=============================================================================
// Memory API
//=============================================================================

/**
 * @brief Store significant emotional experience
 *
 * @param bridge Bridge
 * @param event Event to store
 * @param content_hash Hash of content that triggered event
 * @return 0 on success, -1 on error
 */
int creative_emotion_store_memory(creative_emotion_bridge_t* bridge,
                                   const aesthetic_emotion_event_t* event,
                                   uint64_t content_hash);

/**
 * @brief Recall emotional memory
 *
 * @param bridge Bridge
 * @param content_hash Hash of content
 * @param out Output event (if found)
 * @return 0 if found, -1 if not found
 */
int creative_emotion_recall_memory(const creative_emotion_bridge_t* bridge,
                                    uint64_t content_hash,
                                    aesthetic_emotion_event_t* out);

/**
 * @brief Get recent emotional events
 *
 * @param bridge Bridge
 * @param events Output array
 * @param max_events Maximum to retrieve
 * @return Number of events retrieved
 */
uint32_t creative_emotion_get_recent_events(const creative_emotion_bridge_t* bridge,
                                             aesthetic_emotion_event_t* events,
                                             uint32_t max_events);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_EMOTION_BRIDGE_H */

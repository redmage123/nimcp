/**
 * @file nimcp_occipital_cognitive_bridge.h
 * @brief Unified bridge between Occipital Cortex and all Cognitive modules
 *
 * WHAT: Central integration point for occipital-cognitive communication
 * WHY: Enable visual perception to influence and be influenced by cognition
 * HOW: Routes visual information to emotion, attention, memory, salience, etc.
 *
 * BIOLOGICAL BASIS:
 * - Visual cortex has reciprocal connections with all major cognitive areas
 * - Top-down attention modulates visual processing (prefrontal -> V1-V4)
 * - Emotional salience affects visual processing (amygdala -> V1)
 * - Memory influences visual recognition (hippocampus -> IT)
 * - Global workspace integrates visual info with consciousness
 *
 * CONNECTED COGNITIVE MODULES:
 * 1. EMOTION - Visual input affects emotional state, emotions bias visual attention
 * 2. ATTENTION - Top-down attention modulates visual processing
 * 3. MEMORY - Visual working memory, pattern recognition
 * 4. SALIENCE - Visual salience detection and prioritization
 * 5. CURIOSITY - Novel visual stimuli drive exploration
 * 6. INTROSPECTION - Visual self-awareness, metacognition about vision
 * 7. GLOBAL_WORKSPACE - Visual access to conscious awareness
 * 8. KNOWLEDGE - Visual concept grounding, semantic memory
 * 9. THEORY_OF_MIND - Understanding visual cues about others' mental states
 * 10. SOCIAL - Face processing, gaze following, social attention
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                OCCIPITAL-COGNITIVE BRIDGE                               │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  occipital_adapter_t                                                    │
 * │  ┌─────────────────┐                                                    │
 * │  │ Visual Features │────┬────────────────────────────────────────────── │
 * │  │ V1-V5 Hierarchy │    │                                               │
 * │  └─────────────────┘    │                                               │
 * │                         ▼                                               │
 * │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐        │
 * │  │  EMOTION   │  │ ATTENTION  │  │   MEMORY   │  │  SALIENCE  │        │
 * │  │  Bridge    │  │   Bridge   │  │   Bridge   │  │   Bridge   │        │
 * │  └────────────┘  └────────────┘  └────────────┘  └────────────┘        │
 * │         │              │               │               │                │
 * │         ▼              ▼               ▼               ▼                │
 * │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐        │
 * │  │ CURIOSITY  │  │INTROSPECT  │  │ WORKSPACE  │  │ KNOWLEDGE  │        │
 * │  │  Bridge    │  │   Bridge   │  │   Bridge   │  │   Bridge   │        │
 * │  └────────────┘  └────────────┘  └────────────┘  └────────────┘        │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @version Phase O1: Occipital Cognitive Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_OCCIPITAL_COGNITIVE_BRIDGE_H
#define NIMCP_OCCIPITAL_COGNITIVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct occipital_adapter occipital_adapter_t;
typedef struct neural_substrate neural_substrate_t;

/* Forward declare bio_router_struct for bio-async (defined in nimcp_bio_router.h) */
struct bio_router_struct;

/* Opaque bridge type */
typedef struct occipital_cognitive_bridge occipital_cognitive_bridge_t;

/*=============================================================================
 * Cognitive Module Identifiers
 *===========================================================================*/

/**
 * @brief Cognitive module types for connection
 */
typedef enum {
    COG_MODULE_EMOTION = 0,        /**< Emotion processing */
    COG_MODULE_ATTENTION,          /**< Attention system */
    COG_MODULE_MEMORY,             /**< Memory systems */
    COG_MODULE_SALIENCE,           /**< Salience network */
    COG_MODULE_CURIOSITY,          /**< Curiosity/exploration */
    COG_MODULE_INTROSPECTION,      /**< Metacognition */
    COG_MODULE_GLOBAL_WORKSPACE,   /**< Conscious access */
    COG_MODULE_KNOWLEDGE,          /**< Semantic knowledge */
    COG_MODULE_THEORY_OF_MIND,     /**< Social cognition */
    COG_MODULE_SOCIAL,             /**< Social processing */
    COG_MODULE_REASONING,          /**< Logical reasoning */
    COG_MODULE_PREDICTIVE,         /**< Predictive processing */
    COG_MODULE_FREE_ENERGY,        /**< FEP/Active inference */
    COG_MODULE_PERSONALITY,        /**< Personality traits */
    COG_MODULE_MIRROR_NEURONS,     /**< Mirror neuron system */
    COG_MODULE_COUNT
} cognitive_module_type_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Module-specific configuration
 */
typedef struct {
    cognitive_module_type_t type;  /**< Module type */
    bool enabled;                  /**< Whether connection is enabled */
    float weight;                  /**< Connection weight [0-1] */
    float update_rate_hz;          /**< Update rate for this module */
} cognitive_module_config_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Per-module settings */
    cognitive_module_config_t modules[COG_MODULE_COUNT];

    /* Global settings */
    bool enable_bio_async;         /**< Enable bio-async messaging */
    bool enable_bidirectional;     /**< Allow top-down modulation */
    float global_gain;             /**< Global modulation gain */
    uint32_t max_active_modules;   /**< Max simultaneous active modules */

    /* Visual feature routing */
    bool route_edges;              /**< Route V1 edge features */
    bool route_color;              /**< Route V4 color features */
    bool route_motion;             /**< Route V5 motion features */
    bool route_faces;              /**< Route face features */
    bool route_objects;            /**< Route object features */
} occipital_cognitive_config_t;

/**
 * @brief Visual-cognitive event
 */
typedef struct {
    cognitive_module_type_t target;/**< Target module */
    uint32_t event_type;           /**< Event type (module-specific) */
    float visual_features[8];      /**< Visual feature vector */
    float salience;                /**< Event salience */
    float urgency;                 /**< Event urgency */
    uint64_t timestamp_us;         /**< Event timestamp */
} visual_cognitive_event_t;

/**
 * @brief Top-down modulation signal
 */
typedef struct {
    cognitive_module_type_t source;/**< Source module */
    float attention_gain;          /**< Attention-based gain modulation */
    float spatial_focus_x;         /**< Spatial attention X */
    float spatial_focus_y;         /**< Spatial attention Y */
    float feature_bias[8];         /**< Feature-based attention bias */
    float emotional_valence;       /**< Emotional modulation (-1 to +1) */
    float emotional_arousal;       /**< Arousal level (0 to 1) */
    uint64_t timestamp_us;         /**< Modulation timestamp */
} cognitive_modulation_t;

/**
 * @brief Bridge effects summary
 */
typedef struct {
    /* Per-module activity */
    float module_activity[COG_MODULE_COUNT];

    /* Aggregated effects */
    float total_top_down_gain;     /**< Combined top-down modulation */
    float emotional_modulation;    /**< Net emotional influence */
    float attention_focus;         /**< Attention focus strength */
    float memory_retrieval_boost;  /**< Memory-based recognition boost */
    float curiosity_drive;         /**< Curiosity-driven exploration */
    float salience_enhancement;    /**< Salience-based enhancement */

    /* Overall integration */
    float cognitive_load;          /**< Total cognitive processing load */
    float visual_cognitive_sync;   /**< Sync between vision and cognition */
} occipital_cognitive_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Per-module statistics */
    uint64_t events_sent[COG_MODULE_COUNT];
    uint64_t modulations_received[COG_MODULE_COUNT];

    /* Aggregated statistics */
    uint64_t total_events_sent;
    uint64_t total_modulations_received;
    float avg_processing_time_us;
    float avg_event_salience;
    uint64_t messages_sent;
    uint64_t messages_received;
} occipital_cognitive_stats_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
occipital_cognitive_config_t occipital_cognitive_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create cognitive bridge
 *
 * @param occipital Occipital adapter (required)
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
occipital_cognitive_bridge_t* occipital_cognitive_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_cognitive_config_t* config);

/**
 * @brief Destroy cognitive bridge
 */
void occipital_cognitive_bridge_destroy(occipital_cognitive_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int occipital_cognitive_bridge_reset(occipital_cognitive_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to a cognitive module by handle
 *
 * @param bridge Bridge instance
 * @param type Module type
 * @param module_handle Opaque handle to the module
 * @return 0 on success, -1 on failure
 */
int occipital_cognitive_connect_module(
    occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type,
    void* module_handle);

/**
 * @brief Disconnect a cognitive module
 */
int occipital_cognitive_disconnect_module(
    occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type);

/**
 * @brief Register with bio-async router
 */
int occipital_cognitive_bridge_register_bio_async(
    occipital_cognitive_bridge_t* bridge,
    struct bio_router_struct* router);

/**
 * @brief Connect to neural substrate for metabolic modulation
 */
int occipital_cognitive_connect_substrate(
    occipital_cognitive_bridge_t* bridge,
    neural_substrate_t* substrate);

/*=============================================================================
 * Event API
 *===========================================================================*/

/**
 * @brief Send visual event to cognitive module
 *
 * @param bridge Bridge instance
 * @param event Visual-cognitive event
 * @return 0 on success, -1 on failure
 */
int occipital_cognitive_send_event(
    occipital_cognitive_bridge_t* bridge,
    const visual_cognitive_event_t* event);

/**
 * @brief Broadcast visual features to all enabled modules
 *
 * @param bridge Bridge instance
 * @param features Visual feature vector
 * @param feature_count Number of features
 * @param salience Feature salience
 * @return Number of modules notified, -1 on error
 */
int occipital_cognitive_broadcast(
    occipital_cognitive_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    float salience);

/**
 * @brief Apply top-down modulation from cognitive module
 *
 * @param bridge Bridge instance
 * @param modulation Modulation signal
 * @return 0 on success, -1 on failure
 */
int occipital_cognitive_apply_modulation(
    occipital_cognitive_bridge_t* bridge,
    const cognitive_modulation_t* modulation);

/*=============================================================================
 * Processing API
 *===========================================================================*/

/**
 * @brief Update bridge state
 *
 * Processes pending modulations and updates effects.
 */
int occipital_cognitive_bridge_update(occipital_cognitive_bridge_t* bridge);

/**
 * @brief Process visual frame through cognitive modules
 *
 * Extracts features and routes to appropriate modules.
 */
int occipital_cognitive_bridge_process(occipital_cognitive_bridge_t* bridge);

/**
 * @brief Get current effects
 */
int occipital_cognitive_bridge_get_effects(
    const occipital_cognitive_bridge_t* bridge,
    occipital_cognitive_effects_t* effects);

/**
 * @brief Get aggregated top-down modulation
 */
int occipital_cognitive_get_modulation(
    const occipital_cognitive_bridge_t* bridge,
    cognitive_modulation_t* modulation);

/*=============================================================================
 * Module-Specific API
 *===========================================================================*/

/**
 * @brief Send emotional visual cue (face expression, body language)
 */
int occipital_cognitive_send_emotion_cue(
    occipital_cognitive_bridge_t* bridge,
    float valence,
    float arousal,
    uint32_t expression_id,
    float confidence);

/**
 * @brief Report salient visual event
 */
int occipital_cognitive_report_salience(
    occipital_cognitive_bridge_t* bridge,
    float x, float y,
    float salience,
    uint32_t feature_type);

/**
 * @brief Query visual memory for pattern
 */
int occipital_cognitive_query_memory(
    occipital_cognitive_bridge_t* bridge,
    const float* pattern,
    uint32_t pattern_size,
    float* match_score,
    uint32_t* match_id);

/**
 * @brief Report novel visual stimulus for curiosity
 */
int occipital_cognitive_report_novelty(
    occipital_cognitive_bridge_t* bridge,
    float novelty_score,
    const float* features,
    uint32_t feature_count);

/*=============================================================================
 * Query API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 */
int occipital_cognitive_bridge_get_stats(
    const occipital_cognitive_bridge_t* bridge,
    occipital_cognitive_stats_t* stats);

/**
 * @brief Reset statistics
 */
void occipital_cognitive_bridge_reset_stats(
    occipital_cognitive_bridge_t* bridge);

/**
 * @brief Check if module is connected
 */
bool occipital_cognitive_is_module_connected(
    const occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type);

/**
 * @brief Get number of active modules
 */
uint32_t occipital_cognitive_get_active_module_count(
    const occipital_cognitive_bridge_t* bridge);

/**
 * @brief Get configuration
 */
int occipital_cognitive_bridge_get_config(
    const occipital_cognitive_bridge_t* bridge,
    occipital_cognitive_config_t* config);

/**
 * @brief Enable/disable module connection
 */
int occipital_cognitive_set_module_enabled(
    occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type,
    bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OCCIPITAL_COGNITIVE_BRIDGE_H */

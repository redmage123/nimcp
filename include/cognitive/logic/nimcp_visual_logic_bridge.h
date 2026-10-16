/**
 * @file nimcp_visual_logic_bridge.h
 * @brief Bridge between Visual cortex and Logic module
 *
 * WHAT: Connects visual perception to symbolic reasoning
 * WHY: Enables visual concept grounding and perception-based inference
 * HOW: Converts visual features to logical predicates; routes conclusions back
 *
 * BIOLOGICAL BASIS:
 * - Ventral stream ("what" pathway) provides object/concept info for reasoning
 * - Inferotemporal cortex links visual features to semantic concepts
 * - Prefrontal cortex integrates visual evidence with logical reasoning
 * - Top-down attention from logic modulates visual processing
 *
 * INTEGRATION PATHWAYS:
 * - Visual → Logic: Feature extraction, object recognition → predicate grounding
 * - Logic → Visual: Conclusion-based attention guidance, expectation priming
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_VISUAL_LOGIC_BRIDGE_H
#define NIMCP_VISUAL_LOGIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Signal Types
//=============================================================================

/** Visual-to-logic signal types */
#define VISUAL_LOGIC_OBJECT_DETECTED    0x3001  /**< Object recognized */
#define VISUAL_LOGIC_FEATURE_EXTRACTED  0x3002  /**< Low-level feature */
#define VISUAL_LOGIC_SCENE_PARSED       0x3003  /**< Scene understanding */
#define VISUAL_LOGIC_RELATION_OBSERVED  0x3004  /**< Spatial relationship */

/** Logic-to-visual signal types */
#define LOGIC_VISUAL_ATTEND_OBJECT      0x3101  /**< Direct attention to object */
#define LOGIC_VISUAL_EXPECT_FEATURE     0x3102  /**< Prime for expected feature */
#define LOGIC_VISUAL_VERIFY_PREDICATE   0x3103  /**< Request visual verification */

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Visual observation for logical grounding
 */
typedef struct {
    uint32_t signal_type;           /**< Signal type from defines above */
    char concept_name[64];          /**< Concept/object name for predicate */
    float confidence;               /**< Recognition confidence [0,1] */
    float salience;                 /**< Visual salience [0,1] */
    uint32_t location_x;            /**< Retinotopic X coordinate */
    uint32_t location_y;            /**< Retinotopic Y coordinate */
    uint32_t object_id;             /**< Unique object identifier */
    void* feature_vector;           /**< Optional feature data */
    uint32_t feature_size;          /**< Size of feature data */
    uint64_t timestamp_us;          /**< Observation timestamp */
} visual_logic_observation_t;

/**
 * @brief Spatial relation between visual objects
 */
typedef struct {
    uint32_t subject_id;            /**< Subject object ID */
    uint32_t object_id;             /**< Object of relation ID */
    char relation_name[32];         /**< Relation type (above, left_of, etc.) */
    float confidence;               /**< Relation confidence [0,1] */
} visual_logic_relation_t;

/**
 * @brief Logic-to-visual attention command
 */
typedef struct {
    uint32_t signal_type;           /**< Command type */
    char target_concept[64];        /**< Concept to attend/verify */
    float priority;                 /**< Attention priority [0,1] */
    uint32_t target_x;              /**< Optional target location X */
    uint32_t target_y;              /**< Optional target location Y */
    bool spatial_hint;              /**< If true, use target_x/y hint */
} logic_visual_command_t;

/**
 * @brief Visual-logic bridge configuration
 */
typedef struct {
    bool enable_object_grounding;   /**< Convert objects to predicates */
    bool enable_relation_extraction;/**< Extract spatial relations */
    bool enable_top_down_attention; /**< Allow logic to guide attention */
    bool enable_verification;       /**< Allow predicate verification requests */
    float min_confidence_threshold; /**< Minimum confidence for grounding */
    float min_salience_threshold;   /**< Minimum salience for processing */
    uint32_t max_objects_per_frame; /**< Maximum objects to process */
    uint32_t max_relations_per_frame;/**< Maximum relations to extract */
} visual_logic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t objects_grounded;      /**< Objects converted to predicates */
    uint64_t relations_extracted;   /**< Spatial relations detected */
    uint64_t attention_commands;    /**< Top-down attention requests */
    uint64_t verifications_requested;/**< Predicate verifications */
    uint64_t verifications_confirmed;/**< Successful verifications */
    float avg_grounding_confidence; /**< Average grounding confidence */
    float avg_relation_confidence;  /**< Average relation confidence */
} visual_logic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct visual_logic_bridge visual_logic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default config with reasonable values
 */
visual_logic_config_t visual_logic_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create visual-logic bridge
 * @param visual Visual cortex handle (void* for flexibility)
 * @param logic Logic module handle (void* for flexibility)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
visual_logic_bridge_t* visual_logic_bridge_create(
    void* visual,
    void* logic,
    const visual_logic_config_t* config
);

/**
 * @brief Destroy visual-logic bridge
 * @param bridge Bridge to destroy
 */
void visual_logic_bridge_destroy(visual_logic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int visual_logic_bridge_reset(visual_logic_bridge_t* bridge);

//=============================================================================
// Visual → Logic API (Perception to Reasoning)
//=============================================================================

/**
 * @brief Ground visual observation as logical predicate
 * @param bridge Bridge handle
 * @param obs Visual observation to ground
 * @return 0 on success, -1 on error
 *
 * Converts visual detection into logical predicate:
 * e.g., detected "cat" at (x,y) → cat(obj_123), at(obj_123, x, y)
 */
int visual_logic_ground_observation(
    visual_logic_bridge_t* bridge,
    const visual_logic_observation_t* obs
);

/**
 * @brief Report spatial relation between objects
 * @param bridge Bridge handle
 * @param relation Observed relation
 * @return 0 on success, -1 on error
 *
 * Creates logical relation predicate:
 * e.g., left_of(obj_123, obj_456)
 */
int visual_logic_report_relation(
    visual_logic_bridge_t* bridge,
    const visual_logic_relation_t* relation
);

/**
 * @brief Batch process visual frame
 * @param bridge Bridge handle
 * @param observations Array of observations
 * @param num_observations Number of observations
 * @param relations Array of relations (can be NULL)
 * @param num_relations Number of relations
 * @return Number processed, -1 on error
 */
int visual_logic_process_frame(
    visual_logic_bridge_t* bridge,
    const visual_logic_observation_t* observations,
    uint32_t num_observations,
    const visual_logic_relation_t* relations,
    uint32_t num_relations
);

//=============================================================================
// Logic → Visual API (Reasoning to Perception)
//=============================================================================

/**
 * @brief Request attention to concept
 * @param bridge Bridge handle
 * @param concept_name Concept to attend to
 * @param priority Attention priority [0,1]
 * @return 0 on success, -1 on error
 *
 * Logic module requests visual system to look for specific concept
 */
int visual_logic_request_attention(
    visual_logic_bridge_t* bridge,
    const char* concept_name,
    float priority
);

/**
 * @brief Request visual verification of predicate
 * @param bridge Bridge handle
 * @param predicate_name Predicate to verify
 * @param expected_value Expected truth value
 * @param verified Output: verification result
 * @param confidence Output: verification confidence
 * @return 0 on success, -1 on error
 *
 * Logic requests visual confirmation of a predicate
 * e.g., "Is there a cat in the scene?" → visual search
 */
int visual_logic_verify_predicate(
    visual_logic_bridge_t* bridge,
    const char* predicate_name,
    bool expected_value,
    bool* verified,
    float* confidence
);

/**
 * @brief Send top-down attention command
 * @param bridge Bridge handle
 * @param command Attention command
 * @return 0 on success, -1 on error
 */
int visual_logic_send_command(
    visual_logic_bridge_t* bridge,
    const logic_visual_command_t* command
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if object currently grounded
 * @param bridge Bridge handle
 * @param object_id Object ID
 * @param grounded Output: true if grounded
 * @return 0 on success, -1 on error
 */
int visual_logic_is_grounded(
    const visual_logic_bridge_t* bridge,
    uint32_t object_id,
    bool* grounded
);

/**
 * @brief Get current grounded object count
 * @param bridge Bridge handle
 * @return Number of grounded objects, -1 on error
 */
int visual_logic_get_grounded_count(const visual_logic_bridge_t* bridge);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int visual_logic_bridge_get_stats(
    const visual_logic_bridge_t* bridge,
    visual_logic_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 */
void visual_logic_bridge_reset_stats(visual_logic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_LOGIC_BRIDGE_H */

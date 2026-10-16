/**
 * @file nimcp_somatosensory_logic_bridge.h
 * @brief Bridge between Somatosensory cortex and Logic module
 *
 * WHAT: Connects tactile/proprioceptive perception to symbolic reasoning
 * WHY: Enables embodied cognition and physical reasoning
 * HOW: Converts body-state to logical predicates; routes motor conclusions back
 *
 * BIOLOGICAL BASIS:
 * - Primary somatosensory cortex (S1) provides touch and position sense
 * - Posterior parietal cortex integrates body schema with reasoning
 * - Prefrontal cortex uses body state for action planning and inference
 * - Motor cortex receives logical conclusions for action execution
 *
 * INTEGRATION PATHWAYS:
 * - Somatosensory → Logic: Touch events, body position → grounded predicates
 * - Logic → Somatosensory: Attention guidance, expected contact predictions
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_SOMATOSENSORY_LOGIC_BRIDGE_H
#define NIMCP_SOMATOSENSORY_LOGIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Signal Types
//=============================================================================

/** Somatosensory-to-logic signal types */
#define SOMATO_LOGIC_TOUCH_DETECTED     0x3201  /**< Touch/contact event */
#define SOMATO_LOGIC_POSITION_UPDATE    0x3202  /**< Body position update */
#define SOMATO_LOGIC_FORCE_SENSED       0x3203  /**< Force/pressure sensed */
#define SOMATO_LOGIC_PAIN_SIGNAL        0x3204  /**< Pain/damage signal */
#define SOMATO_LOGIC_TEMPERATURE        0x3205  /**< Temperature sensation */

/** Logic-to-somatosensory signal types */
#define LOGIC_SOMATO_EXPECT_CONTACT     0x3301  /**< Expect contact at region */
#define LOGIC_SOMATO_ATTEND_REGION      0x3302  /**< Attend to body region */
#define LOGIC_SOMATO_VERIFY_POSITION    0x3303  /**< Verify body position */

//=============================================================================
// Body Region Identifiers
//=============================================================================

typedef enum {
    BODY_REGION_HEAD = 0,
    BODY_REGION_NECK,
    BODY_REGION_LEFT_SHOULDER,
    BODY_REGION_RIGHT_SHOULDER,
    BODY_REGION_LEFT_ARM,
    BODY_REGION_RIGHT_ARM,
    BODY_REGION_LEFT_HAND,
    BODY_REGION_RIGHT_HAND,
    BODY_REGION_CHEST,
    BODY_REGION_ABDOMEN,
    BODY_REGION_BACK,
    BODY_REGION_LEFT_LEG,
    BODY_REGION_RIGHT_LEG,
    BODY_REGION_LEFT_FOOT,
    BODY_REGION_RIGHT_FOOT,
    BODY_REGION_COUNT
} body_region_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Somatosensory observation for logical grounding
 */
typedef struct {
    uint32_t signal_type;           /**< Signal type from defines above */
    body_region_t body_region;      /**< Body region affected */
    float intensity;                /**< Sensation intensity [0,1] */
    float confidence;               /**< Detection confidence [0,1] */

    /* Touch/contact specifics */
    float contact_x;                /**< Contact point X (local coords) */
    float contact_y;                /**< Contact point Y (local coords) */
    uint32_t contacted_object_id;   /**< ID of contacted object (if known) */
    char contacted_object_name[64]; /**< Name of contacted object */

    /* Proprioception specifics */
    float joint_angle;              /**< Joint angle (radians) */
    float position[3];              /**< 3D position (x, y, z) */
    float velocity[3];              /**< 3D velocity */

    /* Force/pressure */
    float force_magnitude;          /**< Force magnitude */
    float force_direction[3];       /**< Force direction vector */

    /* Pain/temperature */
    float pain_level;               /**< Pain intensity [0,1] */
    float temperature_celsius;      /**< Temperature reading */

    uint64_t timestamp_us;          /**< Observation timestamp */
} somato_logic_observation_t;

/**
 * @brief Body state predicate (for logic grounding)
 */
typedef struct {
    body_region_t region;           /**< Body region */
    char predicate_name[64];        /**< Predicate (touching, holding, etc.) */
    char object_name[64];           /**< Object involved (if any) */
    float confidence;               /**< Predicate confidence [0,1] */
    bool active;                    /**< Currently active */
} body_state_predicate_t;

/**
 * @brief Logic-to-somatosensory attention command
 */
typedef struct {
    uint32_t signal_type;           /**< Command type */
    body_region_t target_region;    /**< Target body region */
    char expected_object[64];       /**< Expected contacted object */
    float priority;                 /**< Attention priority [0,1] */
    float expected_position[3];     /**< Expected position (for verification) */
} logic_somato_command_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_touch_grounding;    /**< Ground touch events */
    bool enable_position_grounding; /**< Ground body position */
    bool enable_pain_priority;      /**< Prioritize pain signals */
    bool enable_top_down_attention; /**< Allow logic to guide attention */
    bool enable_verification;       /**< Allow predicate verification */
    float min_intensity_threshold;  /**< Minimum intensity for processing */
    float min_confidence_threshold; /**< Minimum confidence for grounding */
    float pain_priority_boost;      /**< Priority boost for pain [1-3] */
} somato_logic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t touch_events_grounded; /**< Touch events converted to predicates */
    uint64_t position_updates;      /**< Position updates processed */
    uint64_t pain_signals;          /**< Pain signals processed */
    uint64_t attention_commands;    /**< Top-down attention requests */
    uint64_t verifications_requested;/**< Predicate verifications */
    uint64_t verifications_confirmed;/**< Successful verifications */
    float avg_touch_confidence;     /**< Average touch confidence */
    float avg_position_confidence;  /**< Average position confidence */
} somato_logic_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct somato_logic_bridge somato_logic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default config with reasonable values
 */
somato_logic_config_t somato_logic_default_config(void);

/**
 * @brief Get body region name
 * @param region Body region enum
 * @return String name of region
 */
const char* somato_logic_region_name(body_region_t region);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create somatosensory-logic bridge
 * @param somatosensory Somatosensory cortex handle
 * @param logic Logic module handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
somato_logic_bridge_t* somato_logic_bridge_create(
    void* somatosensory,
    void* logic,
    const somato_logic_config_t* config
);

/**
 * @brief Destroy somatosensory-logic bridge
 * @param bridge Bridge to destroy
 */
void somato_logic_bridge_destroy(somato_logic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int somato_logic_bridge_reset(somato_logic_bridge_t* bridge);

//=============================================================================
// Somatosensory → Logic API
//=============================================================================

/**
 * @brief Ground somatosensory observation as logical predicate
 * @param bridge Bridge handle
 * @param obs Somatosensory observation
 * @return 0 on success, -1 on error
 *
 * Converts touch/position into logical predicates:
 * e.g., touching(left_hand, cup), holding(right_hand, ball)
 */
int somato_logic_ground_observation(
    somato_logic_bridge_t* bridge,
    const somato_logic_observation_t* obs
);

/**
 * @brief Report body state predicate
 * @param bridge Bridge handle
 * @param predicate Body state predicate
 * @return 0 on success, -1 on error
 */
int somato_logic_report_body_state(
    somato_logic_bridge_t* bridge,
    const body_state_predicate_t* predicate
);

/**
 * @brief Process batch of somatosensory observations
 * @param bridge Bridge handle
 * @param observations Array of observations
 * @param count Number of observations
 * @return Number processed, -1 on error
 */
int somato_logic_process_batch(
    somato_logic_bridge_t* bridge,
    const somato_logic_observation_t* observations,
    uint32_t count
);

//=============================================================================
// Logic → Somatosensory API
//=============================================================================

/**
 * @brief Request attention to body region
 * @param bridge Bridge handle
 * @param region Target body region
 * @param priority Attention priority [0,1]
 * @return 0 on success, -1 on error
 */
int somato_logic_request_attention(
    somato_logic_bridge_t* bridge,
    body_region_t region,
    float priority
);

/**
 * @brief Predict expected contact
 * @param bridge Bridge handle
 * @param region Expected contact region
 * @param object_name Expected object
 * @return 0 on success, -1 on error
 */
int somato_logic_expect_contact(
    somato_logic_bridge_t* bridge,
    body_region_t region,
    const char* object_name
);

/**
 * @brief Verify body position predicate
 * @param bridge Bridge handle
 * @param region Body region to verify
 * @param expected_position Expected 3D position
 * @param verified Output: verification result
 * @param confidence Output: verification confidence
 * @return 0 on success, -1 on error
 */
int somato_logic_verify_position(
    somato_logic_bridge_t* bridge,
    body_region_t region,
    const float expected_position[3],
    bool* verified,
    float* confidence
);

/**
 * @brief Send top-down command
 * @param bridge Bridge handle
 * @param command Command structure
 * @return 0 on success, -1 on error
 */
int somato_logic_send_command(
    somato_logic_bridge_t* bridge,
    const logic_somato_command_t* command
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if region has active contact
 * @param bridge Bridge handle
 * @param region Body region
 * @param has_contact Output: true if contact active
 * @return 0 on success, -1 on error
 */
int somato_logic_has_contact(
    const somato_logic_bridge_t* bridge,
    body_region_t region,
    bool* has_contact
);

/**
 * @brief Get current position of body region
 * @param bridge Bridge handle
 * @param region Body region
 * @param position Output: 3D position
 * @param confidence Output: position confidence
 * @return 0 on success, -1 on error
 */
int somato_logic_get_position(
    const somato_logic_bridge_t* bridge,
    body_region_t region,
    float position[3],
    float* confidence
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int somato_logic_bridge_get_stats(
    const somato_logic_bridge_t* bridge,
    somato_logic_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 */
void somato_logic_bridge_reset_stats(somato_logic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOMATOSENSORY_LOGIC_BRIDGE_H */

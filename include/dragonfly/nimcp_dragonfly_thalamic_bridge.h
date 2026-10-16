/**
 * @file nimcp_dragonfly_thalamic_bridge.h
 * @brief Dragonfly-to-Thalamic System Bridge
 *
 * WHAT: Bridges dragonfly target tracking to thalamic nuclei for signal relay
 * WHY:  Thalamus is the gateway to cortex - target signals must route through it
 * HOW:  Routes visual targets through LGN, attention through Pulvinar, decisions through MD
 *
 * BIOLOGICAL BASIS:
 * - In dragonflies, descending neurons project through protocerebral bridge
 * - In mammals, thalamus relays all sensory/motor signals to cortex
 * - This bridge maps dragonfly TSDN outputs to appropriate thalamic nuclei:
 *   * LGN: Visual target position/motion (from retina/optic flow)
 *   * Pulvinar: Attention modulation, target salience gating
 *   * VA/VL: Motor commands for interception (basal ganglia pathway)
 *   * MD: Executive decisions (pursue/abort/switch)
 *
 * @author NIMCP Development Team
 * @date 2025-12-28
 */

#ifndef NIMCP_DRAGONFLY_THALAMIC_BRIDGE_H
#define NIMCP_DRAGONFLY_THALAMIC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - dragonfly system */
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define THAL_BRIDGE_MAX_VISUAL_CHANNELS 64    /**< Max visual channels to LGN */
#define THAL_BRIDGE_MAX_MOTOR_CHANNELS 16     /**< Max motor channels to VA/VL */
#define THAL_BRIDGE_MAX_ATTENTION_DIMS 8      /**< Max attention dimensions to Pulvinar */
#define THAL_BRIDGE_TSDN_CHANNELS 16          /**< TSDN population size */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Thalamic routing mode for dragonfly signals
 */
typedef enum {
    THAL_ROUTE_DISCOVERY = 0,   /**< Burst mode - scanning for targets */
    THAL_ROUTE_TRACKING,        /**< Tonic mode - faithful tracking relay */
    THAL_ROUTE_INTERCEPT,       /**< High attention - interception phase */
    THAL_ROUTE_SUPPRESSED       /**< Inhibited - no target of interest */
} thal_routing_mode_t;

/**
 * @brief Target signal type for routing
 */
typedef enum {
    THAL_SIGNAL_POSITION = 0,   /**< Target position signal */
    THAL_SIGNAL_VELOCITY,       /**< Target velocity signal */
    THAL_SIGNAL_SALIENCE,       /**< Target salience/attention */
    THAL_SIGNAL_MOTOR_CMD,      /**< Motor command for interception */
    THAL_SIGNAL_DECISION        /**< Executive decision signal */
} thal_signal_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Visual target signal for LGN relay
 */
typedef struct {
    float position[3];          /**< Target position (x, y, z) */
    float angular_position[2];  /**< Azimuth and elevation */
    float angular_velocity[2];  /**< Angular velocity */
    float size;                 /**< Angular size */
    float contrast;             /**< Target contrast */
    float motion_energy;        /**< Motion energy (for MT/V5) */
} thal_visual_target_t;

/**
 * @brief Motor command signal for VA/VL relay
 */
typedef struct {
    float heading_adjustment[3];  /**< Desired heading change */
    float thrust;                 /**< Thrust magnitude */
    float urgency;                /**< Command urgency [0-1] */
    bool is_pursuit;              /**< Pursuit vs. interception */
} thal_motor_command_t;

/**
 * @brief Attention signal for Pulvinar
 */
typedef struct {
    float spatial_attention[3];   /**< Attention focus location */
    float attention_width;        /**< Attention spotlight width */
    float salience;               /**< Target salience [0-1] */
    float priority;               /**< Attention priority [0-1] */
    bool is_covert;               /**< Covert vs overt attention */
} thal_attention_signal_t;

/**
 * @brief Executive decision signal for MD
 */
typedef struct {
    uint32_t action_code;         /**< Decision code (pursue, abort, switch) */
    float confidence;             /**< Decision confidence [0-1] */
    float expected_reward;        /**< Expected interception success */
    float time_pressure;          /**< Time urgency [0-1] */
} thal_decision_signal_t;

/**
 * @brief Thalamic bridge configuration
 */
typedef struct {
    thal_routing_mode_t initial_mode;   /**< Initial routing mode */

    /* LGN (visual) settings */
    uint32_t lgn_channels;              /**< Number of LGN channels */
    float lgn_attention_baseline;       /**< Baseline attention for visual */
    bool enable_lgn_burst_suppression;  /**< Suppress bursts during tracking */

    /* Pulvinar (attention) settings */
    float pulvinar_gain;                /**< Attention signal gain */
    float pulvinar_threshold;           /**< Min salience to relay */
    bool enable_pulvinar_feedback;      /**< Enable cortical feedback */

    /* VA/VL (motor) settings */
    uint32_t motor_channels;            /**< Motor output channels */
    float motor_gain;                   /**< Motor signal gain */
    float motor_urgency_boost;          /**< Urgency boost factor */

    /* MD (executive) settings */
    float decision_threshold;           /**< Min confidence to relay */
    bool enable_decision_gating;        /**< Gate low-confidence decisions */

    /* Integration settings */
    float update_rate_hz;               /**< Bridge update rate */
    bool sync_on_detection;             /**< Sync on new target detection */
    bool enable_trn_gating;             /**< Use TRN for attention gating */
} dragonfly_thalamic_config_t;

/**
 * @brief Thalamic bridge statistics
 */
typedef struct {
    uint64_t visual_signals_relayed;
    uint64_t motor_signals_relayed;
    uint64_t attention_signals_relayed;
    uint64_t decision_signals_relayed;
    uint64_t signals_gated;             /**< Signals blocked by TRN */
    uint64_t mode_switches;
    float avg_lgn_attention;
    float avg_pulvinar_salience;
    float avg_relay_latency_us;
    uint64_t total_processing_time_us;
} thal_bridge_stats_t;

/**
 * @brief Thalamic bridge handle
 */
typedef struct dragonfly_thalamic_bridge_s dragonfly_thalamic_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Initialize default thalamic bridge configuration
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_bridge_default_config(dragonfly_thalamic_config_t* config);

/**
 * @brief Validate thalamic bridge configuration
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int dragonfly_thalamic_bridge_validate_config(const dragonfly_thalamic_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create thalamic bridge
 * @param dragonfly Dragonfly system to connect (may be NULL for standalone)
 * @param thalamus Thalamus system to connect (may be NULL for standalone)
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
dragonfly_thalamic_bridge_t* dragonfly_thalamic_bridge_create(
    dragonfly_system_t* dragonfly,
    void* thalamus,
    const dragonfly_thalamic_config_t* config
);

/**
 * @brief Destroy thalamic bridge
 * @param bridge Bridge to destroy
 */
void dragonfly_thalamic_bridge_destroy(dragonfly_thalamic_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_bridge_reset(dragonfly_thalamic_bridge_t* bridge);

//=============================================================================
// Signal Routing
//=============================================================================

/**
 * @brief Route visual target signal through LGN
 * @param bridge Thalamic bridge
 * @param target Visual target signal
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_relay_visual(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_visual_target_t* target
);

/**
 * @brief Route motor command through VA/VL
 * @param bridge Thalamic bridge
 * @param command Motor command signal
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_relay_motor(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_motor_command_t* command
);

/**
 * @brief Route attention signal through Pulvinar
 * @param bridge Thalamic bridge
 * @param attention Attention signal
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_relay_attention(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_attention_signal_t* attention
);

/**
 * @brief Route decision signal through MD
 * @param bridge Thalamic bridge
 * @param decision Decision signal
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_relay_decision(
    dragonfly_thalamic_bridge_t* bridge,
    const thal_decision_signal_t* decision
);

/**
 * @brief Route TSDN population vector through thalamus
 *
 * Automatically routes to appropriate nuclei based on signal content
 *
 * @param bridge Thalamic bridge
 * @param tsdn_population TSDN population vector [16 elements]
 * @param heading_angle Decoded heading angle
 * @param confidence Population confidence
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_relay_tsdn(
    dragonfly_thalamic_bridge_t* bridge,
    const float* tsdn_population,
    float heading_angle,
    float confidence
);

//=============================================================================
// Mode Control
//=============================================================================

/**
 * @brief Set routing mode
 * @param bridge Thalamic bridge
 * @param mode New routing mode
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_set_mode(
    dragonfly_thalamic_bridge_t* bridge,
    thal_routing_mode_t mode
);

/**
 * @brief Get current routing mode
 * @param bridge Thalamic bridge
 * @return Current mode
 */
thal_routing_mode_t dragonfly_thalamic_get_mode(
    const dragonfly_thalamic_bridge_t* bridge
);

/**
 * @brief Set attention level for specific nucleus pathway
 * @param bridge Thalamic bridge
 * @param signal_type Signal type (determines nucleus)
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_set_attention(
    dragonfly_thalamic_bridge_t* bridge,
    thal_signal_type_t signal_type,
    float attention
);

/**
 * @brief Apply TRN inhibition to block signals
 * @param bridge Thalamic bridge
 * @param signal_type Signal type to inhibit
 * @param inhibition Inhibition strength [0-1]
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_apply_inhibition(
    dragonfly_thalamic_bridge_t* bridge,
    thal_signal_type_t signal_type,
    float inhibition
);

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Connect to dragonfly system
 * @param bridge Thalamic bridge
 * @param dragonfly Dragonfly system to connect
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_connect_dragonfly(
    dragonfly_thalamic_bridge_t* bridge,
    dragonfly_system_t* dragonfly
);

/**
 * @brief Connect to thalamus system
 * @param bridge Thalamic bridge
 * @param thalamus Thalamus system to connect
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_connect_thalamus(
    dragonfly_thalamic_bridge_t* bridge,
    void* thalamus
);

/**
 * @brief Check if dragonfly is connected
 * @param bridge Thalamic bridge
 * @return true if connected
 */
bool dragonfly_thalamic_has_dragonfly(const dragonfly_thalamic_bridge_t* bridge);

/**
 * @brief Check if thalamus is connected
 * @param bridge Thalamic bridge
 * @return true if connected
 */
bool dragonfly_thalamic_has_thalamus(const dragonfly_thalamic_bridge_t* bridge);

//=============================================================================
// Update
//=============================================================================

/**
 * @brief Update bridge state from connected dragonfly
 *
 * Pulls current target info from dragonfly and relays through thalamus
 *
 * @param bridge Thalamic bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_update(dragonfly_thalamic_bridge_t* bridge);

/**
 * @brief Step bridge simulation
 * @param bridge Thalamic bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_step(
    dragonfly_thalamic_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Thalamic bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_bridge_get_stats(
    const dragonfly_thalamic_bridge_t* bridge,
    thal_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Thalamic bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_thalamic_bridge_reset_stats(dragonfly_thalamic_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

/**
 * @brief Get routing mode name
 * @param mode Routing mode
 * @return Mode name string
 */
const char* dragonfly_thalamic_mode_name(thal_routing_mode_t mode);

/**
 * @brief Get signal type name
 * @param type Signal type
 * @return Type name string
 */
const char* dragonfly_thalamic_signal_name(thal_signal_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_THALAMIC_BRIDGE_H */

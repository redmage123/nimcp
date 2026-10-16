/**
 * @file nimcp_basal_ganglia_thalamus_bridge.h
 * @brief Basal ganglia-thalamus motor relay bridge
 *
 * WHAT: Integration bridge between basal ganglia and thalamus for motor output
 * WHY:  BG action selection must route through thalamus to reach motor cortex
 * HOW:  GPi/SNr disinhibition signals relayed through VA/VL nuclei
 *
 * BIOLOGICAL BASIS:
 * - GPi/SNr tonically inhibit thalamic VA/VL nuclei
 * - Action selection disinhibits specific thalamic channels
 * - VA receives BG input, VL receives cerebellar input
 * - Attention and arousal modulate thalamic gating
 * - TRN provides additional inhibitory control
 *
 * PATHWAY:
 * BG (GPi/SNr) → Thalamus (VA/VL) → Motor Cortex → Movement
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BASAL_GANGLIA_THALAMUS_BRIDGE_H
#define NIMCP_BASAL_GANGLIA_THALAMUS_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_thalamus.h"
#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BGT_MAX_CHANNELS          64    /**< Maximum motor channels */
#define BGT_DEFAULT_GAIN          1.0f  /**< Default relay gain */
#define BGT_DEFAULT_THRESHOLD     0.3f  /**< Default disinhibition threshold */
#define BGT_DEFAULT_ATTENTION     0.5f  /**< Default attention level */
#define BGT_URGENCY_BOOST_MAX     0.5f  /**< Maximum urgency boost */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge relay mode
 */
typedef enum {
    BGT_MODE_NORMAL = 0,      /**< Normal relay with attention gating */
    BGT_MODE_URGENT,          /**< Urgency-boosted relay */
    BGT_MODE_SUPPRESSED,      /**< TRN-mediated suppression */
    BGT_MODE_BURST            /**< Thalamic burst mode (drowsy) */
} bgt_relay_mode_t;

/**
 * @brief Motor output type
 */
typedef enum {
    BGT_OUTPUT_DISCRETE = 0,  /**< Discrete action selection */
    BGT_OUTPUT_CONTINUOUS,    /**< Continuous motor output */
    BGT_OUTPUT_VELOCITY       /**< Velocity-based control */
} bgt_output_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Channel mapping between BG and thalamus
 */
typedef struct {
    uint32_t bg_action_id;        /**< BG action index */
    uint32_t thal_channel;        /**< Thalamic VA/VL channel */
    float weight;                 /**< Connection weight */
    bool is_active;               /**< Channel is active */
} bgt_channel_map_t;

/**
 * @brief Motor relay result
 */
typedef struct {
    float* motor_output;          /**< Motor cortex activation */
    uint32_t output_size;         /**< Size of output */
    uint32_t selected_action;     /**< Selected action (if discrete) */
    float selection_confidence;   /**< Confidence in selection */
    float relay_latency_ms;       /**< Simulated relay latency */
    bgt_relay_mode_t mode;        /**< Current relay mode */
} bgt_relay_result_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    uint32_t num_channels;        /**< Number of motor channels */
    float relay_gain;             /**< Output gain multiplier */
    float disinhibition_threshold; /**< Threshold for action pass-through */
    float attention_weight;       /**< Attention modulation strength */
    float urgency_weight;         /**< Urgency boost strength */
    float trn_sensitivity;        /**< Sensitivity to TRN inhibition */
    bgt_output_type_t output_type; /**< Motor output format */
    bool enable_attention_gating; /**< Enable attention modulation */
    bool enable_urgency_boost;    /**< Enable urgency-based boosting */
    bool enable_burst_detection;  /**< Enable burst mode detection */
} bgt_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_relays;        /**< Total relay operations */
    uint64_t successful_relays;   /**< Successful motor commands */
    uint64_t suppressed_relays;   /**< TRN-suppressed relays */
    uint64_t burst_events;        /**< Thalamic burst events */
    float avg_relay_gain;         /**< Average effective gain */
    float avg_latency_ms;         /**< Average relay latency */
    float avg_attention;          /**< Average attention level */
} bgt_bridge_stats_t;

/**
 * @brief BG-Thalamus bridge instance
 */
typedef struct bgt_bridge {
    bridge_base_t base;           /**< MUST be first: base bridge infrastructure */

    /* Connected components */
    basal_ganglia_t* bg;          /**< Connected basal ganglia */
    thalamus_t* thalamus;         /**< Connected thalamus */

    /* Channel mapping */
    bgt_channel_map_t* channels;  /**< Channel mappings */
    uint32_t num_channels;        /**< Number of channels */

    /* Relay state */
    float* bg_output_buffer;      /**< BG disinhibition signals */
    float* thal_input_buffer;     /**< Thalamus input buffer */
    float* motor_output_buffer;   /**< Motor cortex output */
    bgt_relay_mode_t current_mode; /**< Current relay mode */

    /* Modulation state */
    float current_attention;      /**< Current attention level */
    float current_urgency;        /**< Current urgency level */
    float trn_inhibition;         /**< Current TRN inhibition */

    /* Configuration */
    bgt_bridge_config_t config;   /**< Configuration */

    /* Statistics */
    bgt_bridge_stats_t stats;     /**< Runtime statistics */

} bgt_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Configuration to initialize
 */
void bgt_bridge_default_config(bgt_bridge_config_t* config);

/**
 * @brief Create BG-thalamus bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
bgt_bridge_t* bgt_bridge_create(const bgt_bridge_config_t* config);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void bgt_bridge_destroy(bgt_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int bgt_bridge_reset(bgt_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect basal ganglia to bridge
 * @param bridge Bridge instance
 * @param bg Basal ganglia to connect
 * @return 0 on success, -1 on error
 */
int bgt_bridge_connect_bg(bgt_bridge_t* bridge, basal_ganglia_t* bg);

/**
 * @brief Connect thalamus to bridge
 * @param bridge Bridge instance
 * @param thalamus Thalamus to connect
 * @return 0 on success, -1 on error
 */
int bgt_bridge_connect_thalamus(bgt_bridge_t* bridge, thalamus_t* thalamus);

/**
 * @brief Check if fully connected
 * @param bridge Bridge instance
 * @return true if both BG and thalamus connected
 */
bool bgt_bridge_is_connected(const bgt_bridge_t* bridge);

/* ============================================================================
 * Channel Mapping Functions
 * ============================================================================ */

/**
 * @brief Set channel mapping
 * @param bridge Bridge instance
 * @param bg_action BG action index
 * @param thal_channel Thalamic channel
 * @param weight Connection weight
 * @return 0 on success, -1 on error
 */
int bgt_bridge_set_channel_map(
    bgt_bridge_t* bridge,
    uint32_t bg_action,
    uint32_t thal_channel,
    float weight
);

/**
 * @brief Create default one-to-one mapping
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int bgt_bridge_create_default_mapping(bgt_bridge_t* bridge);

/**
 * @brief Get channel weight
 * @param bridge Bridge instance
 * @param bg_action BG action index
 * @return Channel weight or -1 on error
 */
float bgt_bridge_get_channel_weight(
    const bgt_bridge_t* bridge,
    uint32_t bg_action
);

/* ============================================================================
 * Relay Functions
 * ============================================================================ */

/**
 * @brief Relay BG output through thalamus to motor cortex
 *
 * Main processing function:
 * 1. Get BG thalamic output (disinhibition signals)
 * 2. Apply attention and urgency modulation
 * 3. Route through thalamic VA/VL nuclei
 * 4. Apply TRN gating if enabled
 * 5. Generate motor cortex output
 *
 * @param bridge Bridge instance
 * @param result Output: relay result
 * @return 0 on success, -1 on error
 */
int bgt_bridge_relay(bgt_bridge_t* bridge, bgt_relay_result_t* result);

/**
 * @brief Relay with explicit BG output
 * @param bridge Bridge instance
 * @param bg_output BG disinhibition signals
 * @param num_actions Number of actions
 * @param result Output: relay result
 * @return 0 on success, -1 on error
 */
int bgt_bridge_relay_explicit(
    bgt_bridge_t* bridge,
    const float* bg_output,
    uint32_t num_actions,
    bgt_relay_result_t* result
);

/**
 * @brief Get motor output for specific action
 * @param bridge Bridge instance
 * @param action_id Action index
 * @return Motor activation [0-1], or -1 on error
 */
float bgt_bridge_get_action_output(
    const bgt_bridge_t* bridge,
    uint32_t action_id
);

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

/**
 * @brief Set attention level
 * @param bridge Bridge instance
 * @param attention Attention level [0-1]
 * @return 0 on success, -1 on error
 */
int bgt_bridge_set_attention(bgt_bridge_t* bridge, float attention);

/**
 * @brief Set urgency level
 * @param bridge Bridge instance
 * @param urgency Urgency level [0-1]
 * @return 0 on success, -1 on error
 */
int bgt_bridge_set_urgency(bgt_bridge_t* bridge, float urgency);

/**
 * @brief Set TRN inhibition
 * @param bridge Bridge instance
 * @param inhibition TRN inhibition [0-1]
 * @return 0 on success, -1 on error
 */
int bgt_bridge_set_trn_inhibition(bgt_bridge_t* bridge, float inhibition);

/**
 * @brief Set relay mode
 * @param bridge Bridge instance
 * @param mode Relay mode
 * @return 0 on success, -1 on error
 */
int bgt_bridge_set_mode(bgt_bridge_t* bridge, bgt_relay_mode_t mode);

/**
 * @brief Get current attention level
 * @param bridge Bridge instance
 * @return Current attention [0-1]
 */
float bgt_bridge_get_attention(const bgt_bridge_t* bridge);

/**
 * @brief Get current relay mode
 * @param bridge Bridge instance
 * @return Current relay mode
 */
bgt_relay_mode_t bgt_bridge_get_mode(const bgt_bridge_t* bridge);

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int bgt_bridge_get_stats(
    const bgt_bridge_t* bridge,
    bgt_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge instance
 */
void bgt_bridge_reset_stats(bgt_bridge_t* bridge);

/**
 * @brief Get relay mode name
 * @param mode Relay mode
 * @return Mode name string
 */
const char* bgt_relay_mode_name(bgt_relay_mode_t mode);

/**
 * @brief Get output type name
 * @param type Output type
 * @return Type name string
 */
const char* bgt_output_type_name(bgt_output_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_THALAMUS_BRIDGE_H */

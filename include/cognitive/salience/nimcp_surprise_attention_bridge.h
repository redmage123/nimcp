/**
 * @file nimcp_surprise_attention_bridge.h
 * @brief Bridge between Surprise Amplifier and Attention System
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Bidirectional integration between surprise amplifier and attention
 * WHY:  Surprise redirects attention (exogenous capture); attention state
 *       modulates surprise sensitivity (attended vs unattended channels)
 * HOW:  High surprise → attention boost/shift; attention focus → sensitivity map
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SURPRISE → ATTENTION:
 * - Locus coeruleus: NE release on surprise → global attention boost
 * - Stimulus-driven (exogenous) attention capture by salient events
 * - P300 ERP: surprise triggers involuntary attention reorientation
 * - Superior colliculus: rapid gaze shifts to surprising stimuli
 * - Reference: Corbetta & Shulman (2002) "Attentional control of behavior"
 *
 * ATTENTION → SURPRISE:
 * - Attended channels have higher surprise sensitivity (precision weighting)
 * - Unattended channels have reduced PE propagation (inattentional blindness)
 * - Current focus modulates which prediction errors reach amplifier
 * - Reference: Mack & Rock (1998) "Inattentional Blindness"
 *
 * SOCIETY OF THOUGHT CONNECTION:
 * - Kim et al. (2026): surprise feature triggers reasoning re-evaluation
 * - Attention bridge ensures surprise → actual cognitive resource reallocation
 * - Implements the "executive re-evaluation" component of surprise amplification
 *
 * ERROR CODE RANGE: 28300-28399 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_ATTENTION_BRIDGE_H
#define NIMCP_SURPRISE_ATTENTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct surprise_amplifier;
struct nimcp_attention_system;
struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 28300-28399)
 * ============================================================================ */

#define NIMCP_SURPRISE_ATT_ERROR_BASE           28300
#define NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER   (NIMCP_SURPRISE_ATT_ERROR_BASE + 1)
#define NIMCP_SURPRISE_ATT_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_ATT_ERROR_BASE + 2)
#define NIMCP_SURPRISE_ATT_ERROR_NO_MEMORY      (NIMCP_SURPRISE_ATT_ERROR_BASE + 3)
#define NIMCP_SURPRISE_ATT_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_ATT_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_ATT_DEFAULT_BOOST_GAIN         1.5f
#define SURPRISE_ATT_DEFAULT_SHIFT_THRESHOLD    0.6f
#define SURPRISE_ATT_DEFAULT_SENSITIVITY_FLOOR  0.3f
#define SURPRISE_ATT_DEFAULT_DECAY_RATE         0.9f
#define SURPRISE_ATT_MAX_CHANNEL_SENSITIVITY    8

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Channel-specific sensitivity configuration
 */
typedef struct {
    uint32_t channel_id;            /**< Which attention channel */
    float sensitivity;              /**< Surprise sensitivity for this channel [0-1] */
} surprise_att_channel_t;

/**
 * @brief Configuration for the surprise-attention bridge
 */
typedef struct {
    float boost_gain;              /**< Surprise → attention boost multiplier [1.5] */
    float shift_threshold;         /**< Min surprise for attention shift [0.6] */
    float sensitivity_floor;       /**< Min sensitivity when unattended [0.3] */
    float attention_decay_rate;    /**< Decay of boost per second [0.9] */
    bool enable_attention_boost;   /**< Surprise boosts attention [true] */
    bool enable_attention_shift;   /**< High surprise shifts focus [true] */
    bool enable_sensitivity_mod;   /**< Attention modulates sensitivity [true] */
    bool enable_bio_async;         /**< Bio-async messaging [true] */
    bool enable_logging;           /**< Diagnostic logging [true] */
} surprise_att_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float current_attention_boost; /**< Active attention boost from surprise */
    float current_sensitivity;     /**< Attention-derived sensitivity [0-1] */
    bool shift_active;             /**< An attention shift is in progress */
    uint32_t shift_target;         /**< Module target of active shift */
    float boost_remaining;         /**< Remaining boost after decay */
} surprise_att_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t attention_boosts;     /**< Total attention boost events */
    uint64_t attention_shifts;     /**< Total attention shift triggers */
    uint64_t sensitivity_updates;  /**< Times sensitivity was recomputed */
    float avg_boost_magnitude;     /**< Running avg boost applied */
    float max_boost_magnitude;     /**< Peak boost applied */
    float avg_sensitivity;         /**< Running avg sensitivity */
    uint64_t total_updates;        /**< Total update cycles */
} surprise_att_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_att_bridge surprise_att_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_att_config_t surprise_att_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_att_bridge_t* surprise_att_bridge_create(
    const surprise_att_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_att_bridge_destroy(surprise_att_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_att_bridge_reset(surprise_att_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_att_bridge_connect_amplifier(
    surprise_att_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to attention system */
int surprise_att_bridge_connect_attention(
    surprise_att_bridge_t* bridge,
    struct nimcp_attention_system* attention);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Apply attention boost from surprise event
 * @param bridge Bridge handle
 * @param surprise_magnitude Current surprise level [0-1]
 * @param attention_boost Computed attention boost from amplifier
 * @return 0 on success, error code otherwise
 */
int surprise_att_apply_boost(
    surprise_att_bridge_t* bridge,
    float surprise_magnitude,
    float attention_boost);

/**
 * @brief Request attention shift to surprising source
 * @param bridge Bridge handle
 * @param surprise_magnitude Surprise level [0-1]
 * @param source_module Module ID of surprising source
 * @return 0 on success, error code otherwise
 */
int surprise_att_request_shift(
    surprise_att_bridge_t* bridge,
    float surprise_magnitude,
    uint32_t source_module);

/**
 * @brief Get attention-derived surprise sensitivity
 * @return Sensitivity [0-1], 1.0f = max sensitivity
 */
float surprise_att_get_sensitivity(const surprise_att_bridge_t* bridge);

/**
 * @brief Set per-channel sensitivity override
 * @param bridge Bridge handle
 * @param channel_id Channel to configure
 * @param sensitivity Sensitivity [0-1]
 * @return 0 on success, error code otherwise
 */
int surprise_att_set_channel_sensitivity(
    surprise_att_bridge_t* bridge,
    uint32_t channel_id,
    float sensitivity);

/* ============================================================================
 * Update API
 * ============================================================================ */

/** @brief Update bridge: decay boosts, recompute sensitivity */
int surprise_att_bridge_update(surprise_att_bridge_t* bridge, float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_att_bridge_get_effects(
    const surprise_att_bridge_t* bridge,
    surprise_att_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_att_bridge_get_stats(
    const surprise_att_bridge_t* bridge,
    surprise_att_stats_t* stats_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_att_bridge_set_health_agent(
    surprise_att_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_ATTENTION_BRIDGE_H */

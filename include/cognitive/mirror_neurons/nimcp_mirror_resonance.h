/**
 * @file nimcp_mirror_resonance.h
 * @brief Motor Resonance System for Mirror Neurons
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Motor resonance (automatic imitation tendency) with suppression circuits
 * WHY:  Model the automatic motor activation when observing actions, and its control
 * HOW:  Compute resonance based on observation, with basal ganglia gating
 *
 * Motor Resonance Theory:
 * When we observe someone perform an action, our motor system automatically
 * "resonates" - it becomes primed to execute that same action. This is the
 * basis for imitation learning and social cognition.
 *
 * However, we don't automatically imitate everything we see. The basal ganglia
 * (BG) acts as a "gate" that suppresses inappropriate imitation while allowing
 * appropriate imitation through (e.g., during learning or social interaction).
 *
 * Key Components:
 * 1. Motor Resonance: Automatic activation of motor representations
 * 2. BG Suppression: Tonic inhibition preventing unwanted imitation
 * 3. Selective Release: Contextual release of suppression for learning
 * 4. Inhibitory Control: Active suppression of conflicting actions
 *
 * Biological Basis:
 * - Fadiga et al. (1995): MEP facilitation during action observation
 * - Brass et al. (2001): Automatic imitation interference effects
 * - Brass & Heyes (2005): Imitation vs. inhibition in prefrontal cortex
 * - Mukamel et al. (2010): Human mirror neuron recordings
 *
 * Integration Points:
 * - Mirror neuron observation pathway (input)
 * - Premotor cortex motor commands (output)
 * - Basal ganglia (suppression circuit)
 * - Prefrontal cortex (voluntary control)
 *
 * @see Phase 10.11.5 - Enhanced Mirror Neuron Motor Resonance
 */

#ifndef NIMCP_MIRROR_RESONANCE_H
#define NIMCP_MIRROR_RESONANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Defaults
//=============================================================================

/** @brief Default resonance gain (observation to motor mapping) */
#define NIMCP_RESONANCE_DEFAULT_GAIN        0.7f

/** @brief Default BG tonic inhibition level */
#define NIMCP_RESONANCE_BG_TONIC_INHIB      0.5f

/** @brief Resonance decay time constant (ms) */
#define NIMCP_RESONANCE_TAU_DECAY           100.0f

/** @brief BG adaptation time constant (ms) */
#define NIMCP_RESONANCE_TAU_BG_ADAPT        500.0f

/** @brief Maximum resonance level */
#define NIMCP_RESONANCE_MAX                 1.0f

/** @brief Threshold for motor execution trigger */
#define NIMCP_RESONANCE_EXEC_THRESHOLD      0.8f

/** @brief Suppression threshold for conflict detection */
#define NIMCP_RESONANCE_CONFLICT_THRESH     0.3f

/** @brief Maximum motor channels */
#define NIMCP_RESONANCE_MAX_CHANNELS        256

//=============================================================================
// Resonance Types
//=============================================================================

/**
 * @brief Suppression reason codes
 *
 * WHAT: Why motor resonance is being suppressed
 * WHY:  Enable appropriate behavioral control
 */
typedef enum {
    RESONANCE_SUPPRESS_NONE = 0,         /**< No suppression */
    RESONANCE_SUPPRESS_BG_TONIC,         /**< Basal ganglia tonic inhibition */
    RESONANCE_SUPPRESS_PFC_VOLUNTARY,    /**< Voluntary prefrontal suppression */
    RESONANCE_SUPPRESS_CONFLICT,         /**< Conflicting actions detected */
    RESONANCE_SUPPRESS_SOCIAL,           /**< Social inappropriateness */
    RESONANCE_SUPPRESS_FATIGUE,          /**< Motor fatigue/resource depletion */
    RESONANCE_SUPPRESS_LEARNING          /**< Deliberate learning mode (observe only) */
} resonance_suppress_t;

/**
 * @brief Release reason codes
 *
 * WHAT: Why suppression is being released
 * WHY:  Enable appropriate imitation
 */
typedef enum {
    RESONANCE_RELEASE_NONE = 0,          /**< No release (still suppressed) */
    RESONANCE_RELEASE_LEARNING,          /**< Learning context - allow imitation */
    RESONANCE_RELEASE_SOCIAL,            /**< Social bonding context */
    RESONANCE_RELEASE_EMERGENCY,         /**< Urgent action required */
    RESONANCE_RELEASE_REWARD,            /**< Reward anticipation */
    RESONANCE_RELEASE_VOLUNTARY          /**< Voluntary decision to imitate */
} resonance_release_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Motor resonance configuration
 *
 * WHAT: Configuration for motor resonance system
 * WHY:  Allow tuning of resonance and suppression parameters
 */
typedef struct {
    // Resonance parameters
    float resonance_gain;            /**< Obs-to-motor mapping gain (default: 0.7) */
    float tau_resonance_decay;       /**< Resonance decay constant (ms, default: 100) */
    float tau_resonance_rise;        /**< Resonance rise constant (ms, default: 20) */
    float execution_threshold;       /**< Threshold for execution (default: 0.8) */

    // Basal ganglia parameters
    float bg_tonic_inhibition;       /**< Baseline BG inhibition (default: 0.5) */
    float tau_bg_adaptation;         /**< BG adaptation constant (ms, default: 500) */
    float bg_release_rate;           /**< How fast suppression releases (default: 0.1) */

    // Conflict detection
    float conflict_threshold;        /**< Threshold for conflict (default: 0.3) */
    float conflict_decay;            /**< Conflict signal decay (default: 0.05) */

    // Voluntary control
    float pfc_gain;                  /**< Prefrontal control gain (default: 1.0) */
    bool enable_voluntary_override;  /**< Allow voluntary override (default: true) */

    // Fatigue modeling
    bool enable_fatigue;             /**< Enable motor fatigue (default: true) */
    float fatigue_threshold;         /**< Fatigue onset threshold (default: 0.8) */
    float tau_fatigue_recovery;      /**< Fatigue recovery constant (ms, default: 5000) */

    // Social modulation
    bool enable_social_context;      /**< Enable social modulation (default: true) */
    float social_gain;               /**< Social context amplification (default: 1.2) */

} motor_resonance_config_t;

/**
 * @brief Motor channel state
 *
 * WHAT: State for one motor channel/effector
 * WHY:  Track resonance and suppression per-channel
 */
typedef struct {
    uint32_t channel_id;             /**< Channel identifier */
    uint32_t action_id;              /**< Associated action ID */

    // Resonance state
    float resonance_level;           /**< Current resonance [0, 1] */
    float peak_resonance;            /**< Recent peak resonance */
    float target_resonance;          /**< Target resonance level */

    // Suppression state
    float suppression_level;         /**< Current suppression [0, 1] */
    resonance_suppress_t suppress_reason;  /**< Why suppressed */
    resonance_release_t release_reason;    /**< Why released (if any) */

    // Computed output
    float motor_output;              /**< Final motor output (resonance - suppression) */
    bool above_threshold;            /**< Above execution threshold */

    // Conflict tracking
    float conflict_signal;           /**< Conflict with other channels */
    uint32_t conflicting_channel;    /**< ID of conflicting channel */

    // Fatigue state
    float fatigue_level;             /**< Motor fatigue [0, 1] */
    uint64_t last_activation_us;     /**< Last activation time */

    // Statistics
    uint32_t activation_count;       /**< Times above threshold */
    uint32_t suppression_count;      /**< Times suppressed */
    float total_resonance_time;      /**< Cumulative resonance time (ms) */

} motor_channel_t;

/**
 * @brief Motor resonance system state
 *
 * WHAT: Complete motor resonance system
 * WHY:  Manage all channels and suppression circuits
 */
typedef struct motor_resonance_system motor_resonance_system_t;
typedef motor_resonance_system_t* motor_resonance_t;

/**
 * @brief Motor resonance statistics
 *
 * WHAT: Runtime statistics for resonance system
 * WHY:  Monitor system behavior
 */
typedef struct {
    uint32_t num_channels;           /**< Total motor channels */
    uint32_t active_channels;        /**< Channels with resonance > 0.1 */
    uint32_t suppressed_channels;    /**< Channels being suppressed */
    uint32_t above_threshold;        /**< Channels above execution threshold */

    // Resonance statistics
    float mean_resonance;            /**< Mean resonance level */
    float max_resonance;             /**< Maximum resonance */
    uint32_t total_activations;      /**< Total threshold crossings */

    // Suppression statistics
    float mean_suppression;          /**< Mean suppression level */
    uint32_t bg_suppressions;        /**< BG-mediated suppressions */
    uint32_t pfc_suppressions;       /**< PFC-mediated suppressions */
    uint32_t conflict_suppressions;  /**< Conflict-mediated suppressions */

    // Release statistics
    uint32_t learning_releases;      /**< Releases for learning */
    uint32_t social_releases;        /**< Releases for social bonding */
    uint32_t voluntary_releases;     /**< Voluntary releases */

    // Conflict statistics
    float conflict_rate;             /**< Proportion with active conflict */
    float avg_conflict_signal;       /**< Average conflict signal */

} motor_resonance_stats_t;

//=============================================================================
// Lifecycle Management
//=============================================================================

/**
 * @brief Get default motor resonance configuration
 *
 * WHAT: Return sensible defaults for resonance system
 * WHY:  Provide biologically plausible starting point
 *
 * @return Default configuration
 */
motor_resonance_config_t motor_resonance_get_default_config(void);

/**
 * @brief Create motor resonance system
 *
 * WHAT: Initialize motor resonance with suppression circuits
 * WHY:  Enable automatic imitation with appropriate control
 *
 * @param config Configuration (NULL = use defaults)
 * @param max_channels Maximum number of motor channels
 * @return Resonance system handle or NULL on error
 */
motor_resonance_t motor_resonance_create(const motor_resonance_config_t* config,
                                          uint32_t max_channels);

/**
 * @brief Destroy motor resonance system
 *
 * WHAT: Free all resonance resources
 * WHY:  Prevent memory leaks
 *
 * @param resonance System to destroy (NULL-safe)
 */
void motor_resonance_destroy(motor_resonance_t resonance);

//=============================================================================
// Channel Management
//=============================================================================

/**
 * @brief Create motor channel
 *
 * WHAT: Add a new motor channel to the system
 * WHY:  Enable resonance tracking for this effector
 *
 * @param resonance Resonance system
 * @param action_id Associated action ID
 * @return Channel ID or UINT32_MAX on error
 */
uint32_t motor_resonance_create_channel(motor_resonance_t resonance, uint32_t action_id);

/**
 * @brief Get channel state
 *
 * WHAT: Query current state of a motor channel
 * WHY:  Monitor resonance and suppression
 *
 * @param resonance Resonance system
 * @param channel_id Channel to query
 * @param out_channel Output: channel state
 * @return true on success
 */
bool motor_resonance_get_channel(motor_resonance_t resonance, uint32_t channel_id,
                                  motor_channel_t* out_channel);

/**
 * @brief Find channel by action ID
 *
 * WHAT: Look up channel for a specific action
 * WHY:  Map actions to their motor channels
 *
 * @param resonance Resonance system
 * @param action_id Action to find
 * @return Channel ID or UINT32_MAX if not found
 */
uint32_t motor_resonance_find_channel(motor_resonance_t resonance, uint32_t action_id);

//=============================================================================
// Resonance Input
//=============================================================================

/**
 * @brief Process observation input
 *
 * WHAT: Drive resonance from observed action
 * WHY:  Core resonance computation - observation activates motor
 *
 * @param resonance Resonance system
 * @param channel_id Motor channel receiving observation
 * @param observation_strength Strength of observed action (0-1)
 * @param timestamp_us Current time in microseconds
 * @return Resulting resonance level
 */
float motor_resonance_observe(motor_resonance_t resonance, uint32_t channel_id,
                               float observation_strength, uint64_t timestamp_us);

/**
 * @brief Batch process multiple observations
 *
 * WHAT: Update multiple channels from observation
 * WHY:  Efficient batch processing
 *
 * @param resonance Resonance system
 * @param channel_ids Array of channel IDs
 * @param strengths Array of observation strengths
 * @param count Number of channels
 * @param timestamp_us Current time
 */
void motor_resonance_observe_batch(motor_resonance_t resonance,
                                    const uint32_t* channel_ids,
                                    const float* strengths,
                                    uint32_t count,
                                    uint64_t timestamp_us);

//=============================================================================
// Suppression Control
//=============================================================================

/**
 * @brief Set basal ganglia inhibition level
 *
 * WHAT: Update global BG inhibition
 * WHY:  Control tonic suppression of imitation
 *
 * @param resonance Resonance system
 * @param level Inhibition level (0-1)
 */
void motor_resonance_set_bg_inhibition(motor_resonance_t resonance, float level);

/**
 * @brief Set prefrontal voluntary suppression
 *
 * WHAT: Apply voluntary suppression to a channel
 * WHY:  Allow deliberate suppression of specific actions
 *
 * @param resonance Resonance system
 * @param channel_id Channel to suppress
 * @param level Suppression level (0-1)
 */
void motor_resonance_set_pfc_suppression(motor_resonance_t resonance,
                                          uint32_t channel_id, float level);

/**
 * @brief Release suppression for learning
 *
 * WHAT: Reduce suppression to allow imitation learning
 * WHY:  Context-appropriate release of motor control
 *
 * @param resonance Resonance system
 * @param channel_id Channel to release (-1 for all)
 * @param learning_context How strong the learning context is (0-1)
 */
void motor_resonance_release_for_learning(motor_resonance_t resonance,
                                           int32_t channel_id, float learning_context);

/**
 * @brief Release suppression for social context
 *
 * WHAT: Reduce suppression for social bonding imitation
 * WHY:  Enable appropriate social mirroring
 *
 * @param resonance Resonance system
 * @param channel_id Channel to release (-1 for all)
 * @param social_strength Social context strength (0-1)
 */
void motor_resonance_release_for_social(motor_resonance_t resonance,
                                         int32_t channel_id, float social_strength);

//=============================================================================
// Conflict Detection
//=============================================================================

/**
 * @brief Check for conflicting actions
 *
 * WHAT: Detect when multiple incompatible actions are resonating
 * WHY:  Prevent conflicting motor commands
 *
 * @param resonance Resonance system
 * @param channel_id Channel to check
 * @return Conflict signal strength (0 = no conflict)
 */
float motor_resonance_get_conflict(motor_resonance_t resonance, uint32_t channel_id);

/**
 * @brief Set action conflict relationship
 *
 * WHAT: Define that two actions cannot co-occur
 * WHY:  Enable proper conflict detection
 *
 * @param resonance Resonance system
 * @param channel_a First conflicting channel
 * @param channel_b Second conflicting channel
 */
void motor_resonance_set_conflict(motor_resonance_t resonance,
                                   uint32_t channel_a, uint32_t channel_b);

//=============================================================================
// Output Query
//=============================================================================

/**
 * @brief Get motor output for a channel
 *
 * WHAT: Query final motor output (resonance - suppression)
 * WHY:  Determine effective motor command
 *
 * @param resonance Resonance system
 * @param channel_id Channel to query
 * @return Motor output level (0-1) or -1 on error
 */
float motor_resonance_get_output(motor_resonance_t resonance, uint32_t channel_id);

/**
 * @brief Check if channel is above execution threshold
 *
 * WHAT: Test if resonance is strong enough for execution
 * WHY:  Trigger decision point for imitation
 *
 * @param resonance Resonance system
 * @param channel_id Channel to check
 * @return true if above threshold
 */
bool motor_resonance_above_threshold(motor_resonance_t resonance, uint32_t channel_id);

/**
 * @brief Get all channels above threshold
 *
 * WHAT: Find all channels ready for execution
 * WHY:  Enable batch action selection
 *
 * @param resonance Resonance system
 * @param out_channels Output array for channel IDs
 * @param max_channels Size of output array
 * @return Number of channels above threshold
 */
uint32_t motor_resonance_get_active_channels(motor_resonance_t resonance,
                                              uint32_t* out_channels,
                                              uint32_t max_channels);

//=============================================================================
// Time Update
//=============================================================================

/**
 * @brief Step resonance simulation
 *
 * WHAT: Advance system by one timestep
 * WHY:  Update decay, adaptation, conflict resolution
 *
 * @param resonance Resonance system
 * @param dt_ms Time step in milliseconds
 */
void motor_resonance_step(motor_resonance_t resonance, float dt_ms);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get resonance statistics
 *
 * WHAT: Retrieve comprehensive system statistics
 * WHY:  Monitor system behavior
 *
 * @param resonance Resonance system
 * @param stats Output: statistics
 * @return true on success
 */
bool motor_resonance_get_stats(motor_resonance_t resonance, motor_resonance_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 *
 * @param resonance Resonance system
 */
void motor_resonance_reset_stats(motor_resonance_t resonance);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIRROR_RESONANCE_H

//=============================================================================
// nimcp_bg_beta_oscillations.h - Basal Ganglia Beta Oscillation Dynamics
//=============================================================================
/**
 * @file nimcp_bg_beta_oscillations.h
 * @brief Beta oscillation modeling for basal ganglia pathological states
 *
 * BIOLOGICAL BASIS:
 * Beta oscillations (13-30 Hz) in the basal ganglia are critical for:
 * - Movement initiation (beta suppression enables movement)
 * - Motor preparation (beta increase during hold periods)
 * - Pathological states (excessive beta in Parkinson's disease)
 *
 * KEY PHENOMENA:
 * - Beta desynchronization: Precedes voluntary movement by ~1s
 * - Post-movement beta rebound: Returns after movement completion
 * - Pathological beta: Locked oscillations prevent movement initiation
 * - STN-GPe loop: Primary generator of beta oscillations
 *
 * INTEGRATION:
 * - STN: Primary oscillation generator
 * - GPe: Feedback loop with STN
 * - Striatum: Modulates oscillation amplitude
 * - Dopamine: Reduces pathological beta
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BG_BETA_OSCILLATIONS_H
#define NIMCP_BG_BETA_OSCILLATIONS_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Beta frequency band limits (Hz) */
#define BG_BETA_LOW_HZ          13.0f
#define BG_BETA_HIGH_HZ         30.0f
#define BG_BETA_PEAK_HZ         20.0f

/** Oscillation parameters */
#define BG_BETA_HISTORY_SIZE    256     /**< Samples for spectral analysis */
#define BG_BETA_MAX_CHANNELS    64      /**< Maximum oscillation channels */

/** Pathological thresholds */
#define BG_BETA_NORMAL_POWER    0.3f    /**< Normal beta power level */
#define BG_BETA_ELEVATED_POWER  0.6f    /**< Elevated (prodromal) */
#define BG_BETA_PATHOLOGICAL    0.8f    /**< Pathological (Parkinson's-like) */

/** Movement-related beta dynamics */
#define BG_BETA_SUPPRESSION_RATE    0.15f   /**< Rate of pre-movement suppression */
#define BG_BETA_REBOUND_RATE        0.08f   /**< Rate of post-movement rebound */
#define BG_BETA_SUPPRESSION_DEPTH   0.2f    /**< Minimum during movement */

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Beta oscillation state
 */
typedef enum {
    BG_BETA_STATE_BASELINE,         /**< Resting state, normal beta */
    BG_BETA_STATE_SUPPRESSING,      /**< Pre-movement suppression */
    BG_BETA_STATE_SUPPRESSED,       /**< Movement execution */
    BG_BETA_STATE_REBOUNDING,       /**< Post-movement rebound */
    BG_BETA_STATE_LOCKED,           /**< Pathological locked state */
    BG_BETA_STATE_COUNT
} bg_beta_state_t;

/**
 * @brief Pathological condition type
 */
typedef enum {
    BG_PATHOLOGY_NONE,              /**< Healthy state */
    BG_PATHOLOGY_PARKINSON_EARLY,   /**< Early Parkinson's (mild beta excess) */
    BG_PATHOLOGY_PARKINSON_MOD,     /**< Moderate Parkinson's */
    BG_PATHOLOGY_PARKINSON_SEVERE,  /**< Severe Parkinson's */
    BG_PATHOLOGY_DYSTONIA,          /**< Dystonia (abnormal beta patterns) */
    BG_PATHOLOGY_TREMOR,            /**< Tremor-dominant (~4-6 Hz interference) */
    BG_PATHOLOGY_COUNT
} bg_pathology_t;

/**
 * @brief Beta frequency sub-band
 */
typedef enum {
    BG_BETA_BAND_LOW,               /**< 13-20 Hz (movement preparation) */
    BG_BETA_BAND_HIGH,              /**< 20-30 Hz (motor maintenance) */
    BG_BETA_BAND_FULL,              /**< Full 13-30 Hz band */
    BG_BETA_BAND_COUNT
} bg_beta_band_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Configuration for beta oscillation system
 */
typedef struct {
    float baseline_frequency;       /**< Center frequency (default: 20 Hz) */
    float bandwidth;                /**< Bandwidth around center (default: 8 Hz) */
    float baseline_power;           /**< Resting beta power (default: 0.3) */
    float suppression_threshold;    /**< Movement intent threshold (default: 0.5) */
    float lock_threshold;           /**< Pathological lock threshold (default: 0.8) */
    float dopamine_sensitivity;     /**< DA modulation strength (default: 0.5) */
    float stn_gpe_coupling;         /**< STN-GPe loop gain (default: 0.7) */
    uint32_t num_channels;          /**< Number of oscillation channels */
    bool enable_pathology;          /**< Enable pathological state modeling */
    bool enable_tremor;             /**< Enable tremor frequency component */
    float tremor_frequency;         /**< Tremor frequency if enabled (4-6 Hz) */
} bg_beta_config_t;

/**
 * @brief Per-channel oscillation state
 */
typedef struct {
    float phase;                    /**< Current phase (0 to 2*PI) */
    float frequency;                /**< Current instantaneous frequency */
    float amplitude;                /**< Current amplitude */
    float power;                    /**< Band power estimate */
    float history[BG_BETA_HISTORY_SIZE];  /**< Recent samples for analysis */
    uint32_t history_idx;           /**< Circular buffer index */
} bg_beta_channel_t;

/**
 * @brief Beta oscillation statistics
 */
typedef struct {
    float mean_power;               /**< Mean beta power */
    float peak_frequency;           /**< Peak frequency in band */
    float phase_coherence;          /**< Inter-channel coherence */
    float suppression_depth;        /**< Current suppression level */
    float time_in_locked;           /**< Time spent in locked state (s) */
    uint32_t movement_events;       /**< Number of movement-related suppressions */
    uint32_t lock_events;           /**< Number of pathological lock events */
    float dopamine_effect;          /**< Current dopamine modulation */
} bg_beta_stats_t;

/**
 * @brief Main beta oscillation system handle
 */
typedef struct bg_beta_system bg_beta_system_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default beta oscillation configuration
 * @param config Output configuration structure
 */
void bg_beta_default_config(bg_beta_config_t* config);

/**
 * @brief Create beta oscillation system
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on error
 */
bg_beta_system_t* bg_beta_create(const bg_beta_config_t* config);

/**
 * @brief Destroy beta oscillation system
 * @param system System handle
 */
void bg_beta_destroy(bg_beta_system_t* system);

/**
 * @brief Reset system to initial state
 * @param system System handle
 * @return 0 on success
 */
int bg_beta_reset(bg_beta_system_t* system);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step beta oscillation dynamics forward
 * @param system System handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int bg_beta_step(bg_beta_system_t* system, float dt_ms);

/**
 * @brief Process STN-GPe feedback loop
 * @param system System handle
 * @param stn_activity STN population activity [0,1]
 * @param gpe_activity GPe population activity [0,1]
 * @return Updated oscillation amplitude
 */
float bg_beta_process_stn_gpe_loop(bg_beta_system_t* system,
                                    float stn_activity,
                                    float gpe_activity);

/**
 * @brief Apply dopamine modulation to beta power
 * @param system System handle
 * @param dopamine_level Dopamine concentration [0,1]
 * @return Modulated beta power
 */
float bg_beta_apply_dopamine(bg_beta_system_t* system, float dopamine_level);

/**
 * @brief Signal movement intention (triggers suppression)
 * @param system System handle
 * @param intention_strength Movement intention [0,1]
 * @return 0 on success
 */
int bg_beta_signal_movement_intent(bg_beta_system_t* system,
                                    float intention_strength);

/**
 * @brief Signal movement completion (triggers rebound)
 * @param system System handle
 * @return 0 on success
 */
int bg_beta_signal_movement_complete(bg_beta_system_t* system);

/* ============================================================================
 * PATHOLOGY API
 * ============================================================================ */

/**
 * @brief Set pathological condition
 * @param system System handle
 * @param pathology Pathology type
 * @param severity Severity level [0,1]
 * @return 0 on success
 */
int bg_beta_set_pathology(bg_beta_system_t* system,
                          bg_pathology_t pathology,
                          float severity);

/**
 * @brief Get current pathology state
 * @param system System handle
 * @param out_pathology Output pathology type
 * @param out_severity Output severity
 * @return 0 on success
 */
int bg_beta_get_pathology(const bg_beta_system_t* system,
                          bg_pathology_t* out_pathology,
                          float* out_severity);

/**
 * @brief Simulate DBS (Deep Brain Stimulation) effect
 * @param system System handle
 * @param dbs_frequency DBS frequency (typically 130 Hz)
 * @param dbs_amplitude DBS amplitude [0,1]
 * @return Reduction in pathological beta
 */
float bg_beta_apply_dbs(bg_beta_system_t* system,
                        float dbs_frequency,
                        float dbs_amplitude);

/**
 * @brief Simulate L-DOPA medication effect
 * @param system System handle
 * @param ldopa_level L-DOPA equivalent dose [0,1]
 * @return Improvement in beta dynamics
 */
float bg_beta_apply_ldopa(bg_beta_system_t* system, float ldopa_level);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

/**
 * @brief Get current beta state
 * @param system System handle
 * @return Current beta state
 */
bg_beta_state_t bg_beta_get_state(const bg_beta_system_t* system);

/**
 * @brief Get current beta power
 * @param system System handle
 * @param band Frequency band
 * @return Beta power [0,1]
 */
float bg_beta_get_power(const bg_beta_system_t* system, bg_beta_band_t band);

/**
 * @brief Get beta power for specific channel
 * @param system System handle
 * @param channel Channel index
 * @return Channel beta power
 */
float bg_beta_get_channel_power(const bg_beta_system_t* system,
                                 uint32_t channel);

/**
 * @brief Get phase coherence between channels
 * @param system System handle
 * @return Phase coherence [0,1]
 */
float bg_beta_get_coherence(const bg_beta_system_t* system);

/**
 * @brief Check if movement is blocked by pathological beta
 * @param system System handle
 * @return true if movement is blocked
 */
bool bg_beta_is_movement_blocked(const bg_beta_system_t* system);

/**
 * @brief Get movement readiness based on beta state
 * @param system System handle
 * @return Movement readiness [0,1] (0 = blocked, 1 = ready)
 */
float bg_beta_get_movement_readiness(const bg_beta_system_t* system);

/**
 * @brief Get oscillation output for downstream processing
 * @param system System handle
 * @param output Output array (size = num_channels)
 * @return 0 on success
 */
int bg_beta_get_output(const bg_beta_system_t* system, float* output);

/**
 * @brief Get system statistics
 * @param system System handle
 * @param stats Output statistics
 * @return 0 on success
 */
int bg_beta_get_stats(const bg_beta_system_t* system, bg_beta_stats_t* stats);

/* ============================================================================
 * INTEGRATION API
 * ============================================================================ */

/**
 * @brief Modulate action selection threshold based on beta state
 * @param system System handle
 * @param base_threshold Base action threshold
 * @return Modulated threshold (higher when beta is elevated)
 */
float bg_beta_modulate_action_threshold(const bg_beta_system_t* system,
                                         float base_threshold);

/**
 * @brief Get STN output modulation factor
 * @param system System handle
 * @return STN modulation [0,2] (1 = no change)
 */
float bg_beta_get_stn_modulation(const bg_beta_system_t* system);

/**
 * @brief Get motor cortex gating signal
 * @param system System handle
 * @return Gating signal [0,1] (0 = blocked, 1 = enabled)
 */
float bg_beta_get_motor_gate(const bg_beta_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_BETA_OSCILLATIONS_H */

/**
 * @file nimcp_cortical_oscillations_integration.h
 * @brief Cortical Oscillations Integration for Cortical Columns
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Integrates neural oscillations with cortical column dynamics
 * WHY:  Cortical processing is temporally organized by oscillatory rhythms - gamma
 *       for local binding, theta for sequential processing, alpha for gating
 * HOW:  Phase-locked competition, cross-frequency coupling, oscillatory gating
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * OSCILLATORY ORGANIZATION OF CORTICAL COLUMNS:
 * ----------------------------------------------
 * 1. Gamma Oscillations (30-100 Hz):
 *    - Synchronize minicolumns within hypercolumn
 *    - Competition occurs at gamma peaks (20-25ms cycles)
 *    - Local feature binding through phase coherence
 *    - Reference: Fries (2009) "Neuronal gamma-band synchronization"
 *
 * 2. Theta Oscillations (4-8 Hz):
 *    - Organize sequential processing across columns
 *    - Different features processed at different theta phases
 *    - Theta-gamma coupling: gamma nested within theta
 *    - Reference: Lisman & Jensen (2013) "The theta-gamma neural code"
 *
 * 3. Alpha Oscillations (8-12 Hz):
 *    - Gate feedforward vs feedback information flow
 *    - Alpha desynchronization = active processing
 *    - Alpha power inversely correlates with attention
 *    - Reference: Jensen & Mazaheri (2010) "Shaping functional architecture"
 *
 * 4. Phase-Locked Competition:
 *    - Winner-take-all occurs at specific gamma phases
 *    - Phase reset synchronizes processing across columns
 *    - Phase coherence measures inter-columnar binding
 *    - Reference: Womelsdorf et al. (2007) "Modulation of neuronal interactions"
 *
 * OSCILLATION-COLUMN INTEGRATION:
 * --------------------------------
 * - Gamma phase → Competition timing (hypercolumn winner selected at peak)
 * - Theta phase → Sequential feature processing (phase precession)
 * - Alpha power → Feedforward/feedback gating (low alpha = active)
 * - Phase coherence → Inter-columnar binding strength
 * - Cross-frequency coupling → Hierarchical coordination
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_OSCILLATIONS_INTEGRATION_H
#define NIMCP_CORTICAL_OSCILLATIONS_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/brain/oscillations/nimcp_brain_complex_oscillations.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Configuration for cortical oscillations integration
 */
typedef struct {
    /* Oscillation frequencies (Hz) */
    float gamma_frequency;              /**< Gamma frequency 30-100 Hz (default: 40 Hz) */
    float theta_frequency;              /**< Theta frequency 4-8 Hz (default: 6 Hz) */
    float alpha_frequency;              /**< Alpha frequency 8-12 Hz (default: 10 Hz) */
    float beta_frequency;               /**< Beta frequency 12-30 Hz (default: 20 Hz) */

    /* Phase-locking parameters */
    float phase_lock_threshold;         /**< Phase coherence threshold [0-1] (default: 0.7) */
    float phase_lock_window;            /**< Time window for phase-locking (ms) (default: 25) */
    bool enable_phase_reset;            /**< Enable phase reset on salient events */
    float phase_reset_strength;         /**< Phase reset strength [0-1] (default: 0.8) */

    /* Cross-frequency coupling */
    bool enable_theta_gamma_coupling;   /**< Enable theta-gamma PAC */
    bool enable_alpha_gating;           /**< Enable alpha-based gating */
    float pac_modulation_depth;         /**< PAC modulation depth [0-1] (default: 0.5) */
    float alpha_gating_threshold;       /**< Alpha power threshold for gating (default: 0.3) */

    /* Competition timing */
    bool gate_competition_by_phase;     /**< Only compete at gamma peaks */
    float competition_phase_window;     /**< Phase window around peak (radians) (default: π/4) */
    float min_competition_interval_ms;  /**< Minimum interval between competitions (default: 20ms) */

    /* Coherence parameters */
    float coherence_update_rate;        /**< Coherence update rate (default: 0.1) */
    uint32_t coherence_window_size;     /**< Sliding window for coherence (samples) (default: 100) */
    float min_coherence_for_binding;    /**< Minimum coherence for binding (default: 0.6) */
} cortical_oscillation_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Oscillation phase state
 */
typedef struct {
    float gamma_phase;                  /**< Current gamma phase [0-2π] */
    float theta_phase;                  /**< Current theta phase [0-2π] */
    float alpha_phase;                  /**< Current alpha phase [0-2π] */
    float beta_phase;                   /**< Current beta phase [0-2π] */

    float gamma_amplitude;              /**< Gamma amplitude [0-1] */
    float theta_amplitude;              /**< Theta amplitude [0-1] */
    float alpha_amplitude;              /**< Alpha amplitude [0-1] */
    float beta_amplitude;               /**< Beta amplitude [0-1] */

    uint64_t last_update_us;            /**< Last update timestamp (microseconds) */
} oscillation_phase_state_t;

/**
 * @brief Cross-frequency coupling state
 */
typedef struct {
    float theta_gamma_coupling;         /**< Theta-gamma PAC strength [0-1] */
    float preferred_theta_phase;        /**< Preferred theta phase for gamma [-π, π] */
    float alpha_beta_coupling;          /**< Alpha-beta coupling [0-1] */
    float coupling_quality;             /**< Overall coupling quality [0-1] */
} coupling_state_t;

/**
 * @brief Phase coherence state
 */
typedef struct {
    float gamma_coherence;              /**< Gamma phase coherence [0-1] */
    float theta_coherence;              /**< Theta phase coherence [0-1] */
    float mean_phase_gamma;             /**< Mean gamma phase [-π, π] */
    float mean_phase_theta;             /**< Mean theta phase [-π, π] */
    float phase_variance;               /**< Phase variance across columns [0-1] */
    bool binding_active;                /**< Whether binding threshold is met */
} coherence_state_t;

/**
 * @brief Gating state for information flow
 */
typedef struct {
    float feedforward_gain;             /**< Feedforward gain [0-1] */
    float feedback_gain;                /**< Feedback gain [0-1] */
    float lateral_gain;                 /**< Lateral gain [0-1] */
    bool competition_allowed;           /**< Whether competition is allowed */
    uint64_t last_competition_us;       /**< Last competition timestamp */
} gating_state_t;

/**
 * @brief Statistics for oscillations integration
 */
typedef struct {
    uint64_t total_updates;
    uint64_t phase_resets;
    uint64_t competition_events;
    uint64_t binding_events;
    float avg_gamma_coherence;
    float avg_theta_gamma_coupling;
    float avg_competition_phase;
    uint32_t missed_competition_windows;
} cortical_oscillation_stats_t;

/**
 * @brief Complete cortical oscillations integration state
 */
typedef struct {
    /* Configuration */
    cortical_oscillation_config_t config;

    /* Connected systems */
    hypercolumn_t* hypercolumn;
    brain_complex_oscillation_state_t* oscillator;

    /* State */
    oscillation_phase_state_t phase_state;
    coupling_state_t coupling_state;
    coherence_state_t coherence_state;
    gating_state_t gating_state;

    /* Statistics */
    cortical_oscillation_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} cortical_oscillation_integration_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default cortical oscillation configuration
 *
 * WHAT: Provide sensible default parameters
 * WHY:  Easy initialization with biologically-plausible values
 * HOW:  Set defaults based on cortical physiology
 *
 * @param config Output configuration
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER on error
 */
int cortical_oscillation_default_config(cortical_oscillation_config_t* config);

/**
 * @brief Create cortical oscillations integration
 *
 * WHAT: Initialize oscillation integration for cortical columns
 * WHY:  Enable temporally-organized cortical processing
 * HOW:  Allocate state, connect systems, initialize phases
 *
 * @param config Configuration (NULL for defaults)
 * @param hypercolumn Hypercolumn to integrate with
 * @param oscillator Optional oscillator (NULL to create internal)
 * @return New integration or NULL on failure
 */
cortical_oscillation_integration_t* cortical_oscillation_create(
    const cortical_oscillation_config_t* config,
    hypercolumn_t* hypercolumn,
    brain_complex_oscillation_state_t* oscillator
);

/**
 * @brief Destroy cortical oscillations integration
 *
 * @param integration Integration to destroy (NULL safe)
 */
void cortical_oscillation_destroy(cortical_oscillation_integration_t* integration);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update oscillation phases
 *
 * WHAT: Advance all oscillation phases based on elapsed time
 * WHY:  Maintain continuous oscillatory dynamics
 * HOW:  Update phases using: phase += 2π * frequency * dt
 *
 * @param integration Oscillation integration
 * @param delta_time_ms Time step in milliseconds
 * @return 0 on success
 */
int cortical_oscillation_update_phase(
    cortical_oscillation_integration_t* integration,
    float delta_time_ms
);

/**
 * @brief Compute phase coherence across columns
 *
 * WHAT: Calculate phase synchrony across minicolumns
 * WHY:  Coherence indicates binding strength
 * HOW:  Compute circular mean and variance of phases
 *
 * @param integration Oscillation integration
 * @return 0 on success
 */
int cortical_oscillation_compute_coherence(
    cortical_oscillation_integration_t* integration
);

/**
 * @brief Apply theta-gamma coupling
 *
 * WHAT: Modulate gamma amplitude by theta phase
 * WHY:  Theta phase organizes gamma bursts (theta-gamma code)
 * HOW:  gamma_amp *= (1 + depth * cos(theta_phase - preferred_phase))
 *
 * @param integration Oscillation integration
 * @return 0 on success
 */
int cortical_oscillation_apply_theta_gamma_coupling(
    cortical_oscillation_integration_t* integration
);

/**
 * @brief Update alpha-based gating
 *
 * WHAT: Modulate feedforward/feedback gains by alpha power
 * WHY:  Alpha gates information flow (inhibition hypothesis)
 * HOW:  Low alpha → high feedforward, high alpha → high feedback
 *
 * @param integration Oscillation integration
 * @return 0 on success
 */
int cortical_oscillation_update_gating(
    cortical_oscillation_integration_t* integration
);

/* ============================================================================
 * Competition and Phase Locking API
 * ============================================================================ */

/**
 * @brief Gate hypercolumn competition by gamma phase
 *
 * WHAT: Only allow competition near gamma peaks
 * WHY:  Competition is temporally organized by oscillations
 * HOW:  Check if gamma phase is within competition_phase_window of peak (π/2)
 *
 * @param integration Oscillation integration
 * @return true if competition is allowed
 */
bool cortical_oscillation_gate_competition(
    const cortical_oscillation_integration_t* integration
);

/**
 * @brief Reset phases for phase-locked processing
 *
 * WHAT: Reset oscillation phases to synchronize processing
 * WHY:  Phase reset coordinates processing across columns
 * HOW:  Set phases to target values with reset strength
 *
 * @param integration Oscillation integration
 * @param target_gamma_phase Target gamma phase (default: 0)
 * @param target_theta_phase Target theta phase (default: 0)
 * @return 0 on success
 */
int cortical_oscillation_reset_phase(
    cortical_oscillation_integration_t* integration,
    float target_gamma_phase,
    float target_theta_phase
);

/**
 * @brief Check if inter-columnar binding is active
 *
 * WHAT: Determine if phase coherence exceeds binding threshold
 * WHY:  High coherence indicates successful feature binding
 * HOW:  Compare gamma_coherence to min_coherence_for_binding
 *
 * @param integration Oscillation integration
 * @return true if binding is active
 */
bool cortical_oscillation_is_binding_active(
    const cortical_oscillation_integration_t* integration
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current phase state
 *
 * @param integration Oscillation integration
 * @param state Output phase state
 * @return 0 on success
 */
int cortical_oscillation_get_phase_state(
    const cortical_oscillation_integration_t* integration,
    oscillation_phase_state_t* state
);

/**
 * @brief Get coherence state
 *
 * @param integration Oscillation integration
 * @param state Output coherence state
 * @return 0 on success
 */
int cortical_oscillation_get_coherence_state(
    const cortical_oscillation_integration_t* integration,
    coherence_state_t* state
);

/**
 * @brief Get coupling state
 *
 * @param integration Oscillation integration
 * @param state Output coupling state
 * @return 0 on success
 */
int cortical_oscillation_get_coupling_state(
    const cortical_oscillation_integration_t* integration,
    coupling_state_t* state
);

/**
 * @brief Get gating state
 *
 * @param integration Oscillation integration
 * @param state Output gating state
 * @return 0 on success
 */
int cortical_oscillation_get_gating_state(
    const cortical_oscillation_integration_t* integration,
    gating_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param integration Oscillation integration
 * @param stats Output statistics
 * @return 0 on success
 */
int cortical_oscillation_get_stats(
    const cortical_oscillation_integration_t* integration,
    cortical_oscillation_stats_t* stats
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to external oscillator
 *
 * WHAT: Connect to existing brain oscillation state
 * WHY:  Share oscillation state across multiple systems
 * HOW:  Replace internal oscillator with external reference
 *
 * @param integration Oscillation integration
 * @param oscillator External oscillator to connect
 * @return 0 on success
 */
int cortical_oscillation_connect_oscillator(
    cortical_oscillation_integration_t* integration,
    brain_complex_oscillation_state_t* oscillator
);

/**
 * @brief Disconnect from oscillator
 *
 * @param integration Oscillation integration
 * @return 0 on success
 */
int cortical_oscillation_disconnect_oscillator(
    cortical_oscillation_integration_t* integration
);

/* ============================================================================
 * Bio-async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param integration Oscillation integration
 * @return 0 on success
 */
int cortical_oscillation_connect_bio_async(
    cortical_oscillation_integration_t* integration
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param integration Oscillation integration
 * @return 0 on success
 */
int cortical_oscillation_disconnect_bio_async(
    cortical_oscillation_integration_t* integration
);

/**
 * @brief Check if bio-async is connected
 *
 * @param integration Oscillation integration
 * @return true if connected
 */
bool cortical_oscillation_is_bio_async_connected(
    const cortical_oscillation_integration_t* integration
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_OSCILLATIONS_INTEGRATION_H */

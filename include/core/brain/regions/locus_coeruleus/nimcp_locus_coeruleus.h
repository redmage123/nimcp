/**
 * @file nimcp_locus_coeruleus.h
 * @brief Locus Coeruleus (LC) Module - Norepinephrine Center
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Locus Coeruleus modeling for arousal and exploration
 * WHY:  LC is the brain's primary norepinephrine source, critical for arousal/vigilance
 * HOW:  Model tonic/phasic firing, NE release, and projections to cortex/hippocampus/amygdala
 *
 * KEY CONCEPTS:
 * - Tonic Mode: Low, sustained firing (~1-3 Hz) for exploratory behavior
 * - Phasic Mode: Burst firing (up to 20 Hz) for focused attention
 * - Norepinephrine (NE): Volume transmission to widespread brain regions
 * - Adaptive Gain: NE modulates signal-to-noise in target regions
 *
 * BIOLOGICAL BASIS:
 * - Small nucleus in brainstem (~15,000-30,000 neurons in humans)
 * - Projects to virtually all brain regions
 * - Receives input from PFC, amygdala, hypothalamus
 * - NE release enhances processing of salient stimuli
 * - Phasic bursts triggered by unexpected stimuli (novelty/surprise)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LOCUS_COERULEUS_H
#define NIMCP_LOCUS_COERULEUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Baseline tonic firing rate (Hz) */
#define LC_TONIC_BASELINE_HZ            2.0f

/** Maximum phasic burst rate (Hz) */
#define LC_PHASIC_MAX_HZ                20.0f

/** Baseline NE concentration (nM) */
#define LC_NE_BASELINE_NM               0.5f

/** Maximum NE concentration (nM) */
#define LC_NE_MAX_NM                    100.0f

/** NE release time constant (ms) */
#define LC_NE_RELEASE_TAU_MS            50.0f

/** NE clearance time constant (ms) */
#define LC_NE_CLEARANCE_TAU_MS          200.0f

/** Mode switch threshold (default) */
#define LC_MODE_SWITCH_THRESHOLD        0.6f

/** Maximum projections */
#define LC_MAX_PROJECTIONS              64

/** History buffer size */
#define LC_HISTORY_SIZE                 128

/** Refractory period (ms) */
#define LC_REFRACTORY_MS                50.0f

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    LC_OK = 0,
    LC_ERR_NULL_PTR = -1,
    LC_ERR_INVALID_PARAM = -2,
    LC_ERR_NOT_INITIALIZED = -3,
    LC_ERR_ALREADY_INITIALIZED = -4,
    LC_ERR_NO_MEMORY = -5,
    LC_ERR_PROJECTION_NOT_FOUND = -6,
    LC_ERR_CAPACITY_EXCEEDED = -7,
    LC_ERR_INVALID_STATE = -8,
    LC_ERR_REFRACTORY = -9
} nimcp_lc_error_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief LC firing mode
 */
typedef enum {
    LC_MODE_TONIC = 0,                  /**< Low sustained firing (exploration) */
    LC_MODE_PHASIC,                     /**< Burst firing (focused attention) */
    LC_MODE_QUIESCENT,                  /**< Very low activity (sleep) */
    LC_MODE_COUNT
} nimcp_lc_mode_t;

/**
 * @brief LC operational state
 */
typedef enum {
    LC_STATE_IDLE = 0,                  /**< Ready for processing */
    LC_STATE_FIRING,                    /**< Currently firing */
    LC_STATE_REFRACTORY,                /**< In refractory period */
    LC_STATE_BURSTING,                  /**< In phasic burst */
    LC_STATE_TRANSITIONING              /**< Mode transition */
} nimcp_lc_state_t;

/**
 * @brief LC status for overall health
 */
typedef enum {
    LC_STATUS_NORMAL = 0,               /**< Normal operation */
    LC_STATUS_HYPERACTIVE,              /**< Excessive NE release */
    LC_STATUS_HYPOACTIVE,               /**< Insufficient NE release */
    LC_STATUS_STRESSED,                 /**< Stress-induced elevation */
    LC_STATUS_DEPLETED                  /**< NE reserves depleted */
} nimcp_lc_status_t;

/**
 * @brief Projection target type
 */
typedef enum {
    LC_TARGET_CORTEX = 0,               /**< Neocortex */
    LC_TARGET_HIPPOCAMPUS,              /**< Hippocampus (memory) */
    LC_TARGET_AMYGDALA,                 /**< Amygdala (emotion) */
    LC_TARGET_THALAMUS,                 /**< Thalamus (relay) */
    LC_TARGET_CEREBELLUM,               /**< Cerebellum (motor) */
    LC_TARGET_HYPOTHALAMUS,             /**< Hypothalamus (autonomic) */
    LC_TARGET_SPINAL_CORD,              /**< Spinal cord (motor) */
    LC_TARGET_COUNT
} nimcp_lc_target_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_lc_projection_s nimcp_lc_projection_t;
typedef struct nimcp_lc_neuron_pool_s nimcp_lc_neuron_pool_t;
typedef struct nimcp_lc_config_s nimcp_lc_config_t;
typedef struct nimcp_lc_metrics_s nimcp_lc_metrics_t;
typedef struct nimcp_lc_system_s nimcp_lc_system_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for NE release events
 */
typedef void (*nimcp_lc_release_callback_t)(
    nimcp_lc_system_t* lc,
    nimcp_lc_target_t target,
    float ne_concentration,
    void* user_data
);

/**
 * @brief Callback for mode transitions
 */
typedef void (*nimcp_lc_mode_callback_t)(
    nimcp_lc_system_t* lc,
    nimcp_lc_mode_t old_mode,
    nimcp_lc_mode_t new_mode,
    void* user_data
);

/**
 * @brief Callback for novelty detection
 */
typedef void (*nimcp_lc_novelty_callback_t)(
    nimcp_lc_system_t* lc,
    float novelty_score,
    const float* input,
    uint32_t input_size,
    void* user_data
);

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Projection to target brain region
 */
struct nimcp_lc_projection_s {
    uint32_t id;                        /**< Projection ID */
    nimcp_lc_target_t target;           /**< Target region type */
    char target_name[64];               /**< Target region name */

    /* Connectivity */
    float strength;                     /**< Base connection strength (0-1) */
    float ne_sensitivity;               /**< Target's NE sensitivity */
    float current_ne;                   /**< Current NE at target (nM) */

    /* Dynamics */
    float conduction_delay_ms;          /**< Axonal conduction delay */
    float release_probability;          /**< Probability of release per spike */
    float uptake_rate;                  /**< NE reuptake rate */

    /* Modulation */
    float gain_modulation;              /**< NE effect on signal gain */
    float noise_suppression;            /**< NE effect on noise */

    /* State */
    bool active;
    float time_since_release;
};

/**
 * @brief LC neuron pool (simplified model)
 */
struct nimcp_lc_neuron_pool_s {
    uint32_t num_neurons;               /**< Number of neurons */

    /* Firing state */
    float membrane_potential;           /**< Average membrane potential */
    float firing_rate;                  /**< Current firing rate (Hz) */
    float spike_probability;            /**< Instantaneous spike probability */

    /* Adaptation */
    float adaptation;                   /**< Spike-rate adaptation */
    float fatigue;                      /**< Fatigue from sustained firing */

    /* Inputs */
    float excitatory_input;             /**< Total excitatory input */
    float inhibitory_input;             /**< Total inhibitory input */

    /* Autoreceptors */
    float autoreceptor_feedback;        /**< Alpha-2 autoreceptor inhibition */
    bool autoreceptors_enabled;

    /* State */
    float refractory_remaining;         /**< Time remaining in refractory */
    uint32_t spikes_this_update;        /**< Spikes generated this timestep */
};

/**
 * @brief LC system configuration
 */
struct nimcp_lc_config_s {
    /* Neuron parameters */
    uint32_t num_neurons;               /**< Number of LC neurons */
    float tonic_rate_hz;                /**< Baseline tonic firing rate */
    float phasic_rate_hz;               /**< Maximum phasic rate */
    float refractory_ms;                /**< Refractory period */

    /* NE parameters */
    float ne_baseline_nm;               /**< Baseline NE concentration */
    float ne_max_nm;                    /**< Maximum NE concentration */
    float ne_release_tau_ms;            /**< Release time constant */
    float ne_clearance_tau_ms;          /**< Clearance time constant */
    float ne_per_spike;                 /**< NE released per spike */

    /* Mode switching */
    float mode_switch_threshold;        /**< Threshold for mode switching */
    float phasic_duration_ms;           /**< Duration of phasic bursts */
    float tonic_phasic_balance;         /**< Tonic/phasic balance (0-1) */

    /* Novelty detection */
    float novelty_threshold;            /**< Threshold for novelty response */
    float surprise_gain;                /**< Gain on surprise signal */
    float habituation_rate;             /**< Rate of novelty habituation */

    /* Arousal */
    float arousal_coupling;             /**< Coupling to arousal system */
    float vigilance_decay_rate;         /**< Vigilance decay without input */

    /* Autoreceptors */
    bool enable_autoreceptors;          /**< Enable alpha-2 feedback */
    float autoreceptor_gain;            /**< Autoreceptor feedback strength */

    /* Callbacks */
    nimcp_lc_release_callback_t on_release;
    nimcp_lc_mode_callback_t on_mode_change;
    nimcp_lc_novelty_callback_t on_novelty;
    void* callback_data;

    /* Logging */
    bool enable_logging;
};

/**
 * @brief LC system metrics
 */
struct nimcp_lc_metrics_s {
    /* Firing statistics */
    float mean_firing_rate;             /**< Average firing rate (Hz) */
    float peak_firing_rate;             /**< Maximum observed rate */
    uint64_t total_spikes;              /**< Total spikes generated */
    uint64_t total_bursts;              /**< Total phasic bursts */

    /* NE statistics */
    float mean_ne_concentration;        /**< Average NE level */
    float peak_ne_concentration;        /**< Maximum NE level */
    float total_ne_released;            /**< Total NE released */

    /* Mode statistics */
    float time_in_tonic;                /**< Time in tonic mode (s) */
    float time_in_phasic;               /**< Time in phasic mode (s) */
    uint64_t mode_transitions;          /**< Number of mode changes */

    /* Arousal statistics */
    float mean_arousal;                 /**< Average arousal level */
    float mean_vigilance;               /**< Average vigilance */

    /* Novelty statistics */
    float mean_novelty;                 /**< Average novelty signal */
    uint64_t novelty_events;            /**< Number of novelty responses */

    /* System */
    float total_simulation_time;        /**< Total simulated time (s) */
    uint64_t update_count;              /**< Number of update calls */
};

/**
 * @brief Main Locus Coeruleus system
 */
struct nimcp_lc_system_s {
    /* Core firing parameters */
    float tonic_firing_rate;            /**< Hz (baseline ~1-3 Hz) */
    float phasic_firing_rate;           /**< Hz (burst up to 20 Hz) */
    float ne_concentration;             /**< Current NE level (nM) */
    float ne_release_rate;              /**< NE release rate (nM/s) */

    /* Arousal state */
    float arousal_level;                /**< Global arousal (0-1) */
    float alertness;                    /**< Alertness level (0-1) */
    float vigilance;                    /**< Sustained attention (0-1) */

    /* Novelty/surprise detection */
    float novelty_signal;               /**< Current novelty (0-1) */
    float surprise_magnitude;           /**< Surprise level (0-1) */
    float exploration_drive;            /**< Exploration motivation (0-1) */

    /* Mode switching */
    nimcp_lc_mode_t mode;               /**< Current firing mode */
    bool phasic_mode;                   /**< True = focused, false = exploratory */
    float mode_switch_threshold;
    float time_in_current_mode;         /**< Time since mode switch (ms) */

    /* Neuron pool */
    nimcp_lc_neuron_pool_t neurons;

    /* Projections */
    nimcp_lc_projection_t projections[LC_MAX_PROJECTIONS];
    uint32_t num_projections;

    /* Sensory input history (for novelty detection) */
    float input_history[LC_HISTORY_SIZE];
    uint32_t history_index;
    uint32_t history_count;
    float running_mean;
    float running_variance;

    /* Configuration */
    nimcp_lc_config_t config;

    /* Metrics */
    nimcp_lc_metrics_t metrics;

    /* State */
    nimcp_lc_state_t state;
    nimcp_lc_status_t status;
    bool initialized;
    float current_time;                 /**< Simulation time (ms) */
    uint64_t update_count;
};

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default LC configuration
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_lc_config_t nimcp_lc_default_config(void);

/**
 * @brief Initialize LC system
 * @param lc System to initialize
 * @param config Configuration (NULL for defaults)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_init(
    nimcp_lc_system_t* lc,
    const nimcp_lc_config_t* config
);

/**
 * @brief Shutdown LC system
 * @param lc System to shutdown
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_shutdown(nimcp_lc_system_t* lc);

/**
 * @brief Reset LC to baseline
 * @param lc System to reset
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_reset(nimcp_lc_system_t* lc);

//=============================================================================
// Projection API
//=============================================================================

/**
 * @brief Add projection to target region
 * @param lc LC system
 * @param target Target region type
 * @param name Target name (optional)
 * @param strength Connection strength (0-1)
 * @param[out] projection_id Assigned ID
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_add_projection(
    nimcp_lc_system_t* lc,
    nimcp_lc_target_t target,
    const char* name,
    float strength,
    uint32_t* projection_id
);

/**
 * @brief Get projection by ID
 * @param lc LC system
 * @param projection_id Projection ID
 * @return Projection pointer or NULL
 */
NIMCP_EXPORT nimcp_lc_projection_t* nimcp_lc_get_projection(
    nimcp_lc_system_t* lc,
    uint32_t projection_id
);

/**
 * @brief Get projection by target type
 * @param lc LC system
 * @param target Target type
 * @return First projection to target or NULL
 */
NIMCP_EXPORT nimcp_lc_projection_t* nimcp_lc_get_projection_by_target(
    nimcp_lc_system_t* lc,
    nimcp_lc_target_t target
);

/**
 * @brief Set projection parameters
 * @param projection Projection to configure
 * @param sensitivity Target NE sensitivity
 * @param delay Conduction delay (ms)
 * @param uptake_rate NE uptake rate
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_set_projection_params(
    nimcp_lc_projection_t* projection,
    float sensitivity,
    float delay,
    float uptake_rate
);

//=============================================================================
// Core Operations API
//=============================================================================

/**
 * @brief Update LC system (single timestep)
 * @param lc System
 * @param dt Time delta (ms)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_update(
    nimcp_lc_system_t* lc,
    float dt
);

/**
 * @brief Detect novelty from sensory input
 * @param lc LC system
 * @param sensory_input Input array
 * @param input_size Size of input array
 * @param[out] novelty_score Computed novelty (0-1)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_detect_novelty(
    nimcp_lc_system_t* lc,
    const float* sensory_input,
    uint32_t input_size,
    float* novelty_score
);

/**
 * @brief Modulate arousal level
 * @param lc LC system
 * @param target_arousal Target arousal level (0-1)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_modulate_arousal(
    nimcp_lc_system_t* lc,
    float target_arousal
);

/**
 * @brief Trigger attention reset (phasic burst)
 * @param lc LC system
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_trigger_attention_reset(
    nimcp_lc_system_t* lc
);

//=============================================================================
// Mode Control API
//=============================================================================

/**
 * @brief Set firing mode
 * @param lc LC system
 * @param mode Target mode
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_set_mode(
    nimcp_lc_system_t* lc,
    nimcp_lc_mode_t mode
);

/**
 * @brief Get current mode
 * @param lc LC system
 * @return Current firing mode
 */
NIMCP_EXPORT nimcp_lc_mode_t nimcp_lc_get_mode(const nimcp_lc_system_t* lc);

/**
 * @brief Trigger phasic burst
 * @param lc LC system
 * @param intensity Burst intensity (0-1)
 * @param duration Burst duration (ms)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_trigger_burst(
    nimcp_lc_system_t* lc,
    float intensity,
    float duration
);

//=============================================================================
// Input API
//=============================================================================

/**
 * @brief Apply excitatory input
 * @param lc LC system
 * @param input Input strength (0-1)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_apply_excitation(
    nimcp_lc_system_t* lc,
    float input
);

/**
 * @brief Apply inhibitory input
 * @param lc LC system
 * @param input Input strength (0-1)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_apply_inhibition(
    nimcp_lc_system_t* lc,
    float input
);

/**
 * @brief Signal stress response
 * @param lc LC system
 * @param stress_level Stress intensity (0-1)
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_signal_stress(
    nimcp_lc_system_t* lc,
    float stress_level
);

//=============================================================================
// Output API
//=============================================================================

/**
 * @brief Get NE concentration at target
 * @param lc LC system
 * @param target Target region
 * @param[out] ne_concentration NE level at target
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_get_ne_at_target(
    const nimcp_lc_system_t* lc,
    nimcp_lc_target_t target,
    float* ne_concentration
);

/**
 * @brief Get gain modulation for target
 * @param lc LC system
 * @param target Target region
 * @param[out] gain Computed gain modulation
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_get_gain_modulation(
    const nimcp_lc_system_t* lc,
    nimcp_lc_target_t target,
    float* gain
);

/**
 * @brief Get current firing rate
 * @param lc LC system
 * @param[out] rate_hz Firing rate in Hz
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_get_firing_rate(
    const nimcp_lc_system_t* lc,
    float* rate_hz
);

//=============================================================================
// State/Metrics API
//=============================================================================

/**
 * @brief Get current state
 * @param lc LC system
 * @return Current state
 */
NIMCP_EXPORT nimcp_lc_state_t nimcp_lc_get_state(const nimcp_lc_system_t* lc);

/**
 * @brief Get current status
 * @param lc LC system
 * @return Current status
 */
NIMCP_EXPORT nimcp_lc_status_t nimcp_lc_get_status(const nimcp_lc_system_t* lc);

/**
 * @brief Get metrics
 * @param lc LC system
 * @param[out] metrics Metrics output
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_get_metrics(
    const nimcp_lc_system_t* lc,
    nimcp_lc_metrics_t* metrics
);

/**
 * @brief Reset metrics
 * @param lc LC system
 * @return LC_OK on success
 */
NIMCP_EXPORT nimcp_lc_error_t nimcp_lc_reset_metrics(nimcp_lc_system_t* lc);

/**
 * @brief Get error string
 * @param error Error code
 * @return Human-readable string
 */
NIMCP_EXPORT const char* nimcp_lc_error_string(nimcp_lc_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOCUS_COERULEUS_H */

/**
 * @file nimcp_raphe.h
 * @brief Raphe Nuclei - Serotonin/Mood Regulation Center
 * @date 2026-01-11
 *
 * Raphe nuclei are the primary serotonergic nuclei, critical for:
 * - Mood regulation and emotional stability
 * - Impulse control and behavioral inhibition
 * - Temporal discounting and patience
 * - Sleep-wake regulation
 *
 * Key functions:
 * 1. 5-HT (serotonin) release to modulate mood
 * 2. Impulse control via prefrontal projections
 * 3. Anxiety modulation via amygdala connections
 * 4. Sleep-wake cycle coordination
 */

#ifndef NIMCP_RAPHE_H
#define NIMCP_RAPHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define RAPHE_MAX_PROJECTIONS       16
#define RAPHE_MAX_NAME_LEN          64
#define RAPHE_DEFAULT_TONIC_RATE    2.0f      /* Hz - baseline 5-HT neuron firing */
#define RAPHE_PHASIC_MAX_RATE       15.0f     /* Hz - burst firing max */
#define RAPHE_DEFAULT_5HT_BASELINE  20.0f     /* nM - baseline 5-HT concentration */
#define RAPHE_DEFAULT_MOOD_NEUTRAL  0.0f      /* Neutral mood valence */

/*=============================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    RAPHE_OK = 0,
    RAPHE_ERROR_NULL,
    RAPHE_ERROR_NOT_INITIALIZED,
    RAPHE_ERROR_ALREADY_INITIALIZED,
    RAPHE_ERROR_INVALID_PARAM,
    RAPHE_ERROR_CAPACITY,
    RAPHE_ERROR_NOT_FOUND,
    RAPHE_ERROR_STATE,
    RAPHE_ERROR_INTERNAL
} nimcp_raphe_error_t;

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Raphe operating modes
 */
typedef enum {
    RAPHE_MODE_TONIC = 0,           /**< Baseline regular firing */
    RAPHE_MODE_ELEVATED,            /**< Increased activity (positive mood) */
    RAPHE_MODE_SUPPRESSED,          /**< Reduced activity (negative mood) */
    RAPHE_MODE_SLEEP                /**< Sleep-state low activity */
} nimcp_raphe_mode_t;

/**
 * @brief Mood state classification
 */
typedef enum {
    MOOD_SEVERELY_DEPRESSED = -2,
    MOOD_DEPRESSED = -1,
    MOOD_NEUTRAL = 0,
    MOOD_POSITIVE = 1,
    MOOD_EUPHORIC = 2
} nimcp_mood_state_t;

/**
 * @brief Projection targets for 5-HT release
 */
typedef enum {
    RAPHE_TARGET_PFC = 0,           /**< Prefrontal cortex (impulse control) */
    RAPHE_TARGET_AMYGDALA,          /**< Amygdala (anxiety) */
    RAPHE_TARGET_HIPPOCAMPUS,       /**< Memory consolidation */
    RAPHE_TARGET_HYPOTHALAMUS,      /**< Sleep-wake, appetite */
    RAPHE_TARGET_STRIATUM,          /**< Motor/habit learning */
    RAPHE_TARGET_VTA,               /**< Dopamine modulation */
    RAPHE_TARGET_HABENULA,          /**< Aversion processing */
    RAPHE_TARGET_COUNT
} nimcp_raphe_target_t;

/**
 * @brief 5-HT receptor types
 */
typedef enum {
    HT_RECEPTOR_5HT1A = 0,          /**< 5-HT1A (anxiolytic, autoreceptor) */
    HT_RECEPTOR_5HT1B,              /**< 5-HT1B (autoreceptor) */
    HT_RECEPTOR_5HT2A,              /**< 5-HT2A (hallucinogenic, mood) */
    HT_RECEPTOR_5HT2C,              /**< 5-HT2C (appetite, mood) */
    HT_RECEPTOR_5HT3,               /**< 5-HT3 (nausea, fast) */
    HT_RECEPTOR_5HT4,               /**< 5-HT4 (cognition) */
    HT_RECEPTOR_COUNT
} nimcp_ht_receptor_t;

/**
 * @brief Raphe system status
 */
typedef enum {
    RAPHE_STATUS_NORMAL = 0,
    RAPHE_STATUS_HYPOSEROTONERGIC,    /**< Low 5-HT - depression/anxiety */
    RAPHE_STATUS_HYPERSEROTONERGIC,   /**< High 5-HT - serotonin syndrome */
    RAPHE_STATUS_DYSREGULATED
} nimcp_raphe_status_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief Raphe neuron pool state
 */
typedef struct {
    float membrane_potential;        /**< mV */
    float firing_rate;              /**< Hz */
    float excitatory_input;         /**< Total excitation */
    float inhibitory_input;         /**< Total inhibition */
    float refractory_time;          /**< ms remaining */
    uint32_t spike_count;           /**< Spikes in current window */
    bool in_burst;                  /**< Currently in burst mode */
} nimcp_raphe_neuron_pool_t;

/**
 * @brief Projection to target region
 */
typedef struct {
    uint32_t id;
    nimcp_raphe_target_t target;
    char name[RAPHE_MAX_NAME_LEN];
    float weight;                   /**< Projection strength [0-1] */
    float ht_delivered;             /**< 5-HT at target (nM) */
    float receptor_activation[HT_RECEPTOR_COUNT];
    bool enabled;
} nimcp_raphe_projection_t;

/**
 * @brief Mood regulation state
 */
typedef struct {
    float valence;                  /**< Mood valence [-1, +1] */
    float stability;                /**< Emotional stability [0-1] */
    float anxiety;                  /**< Anxiety level [0-1] */
    float irritability;             /**< Irritability [0-1] */
    nimcp_mood_state_t state;       /**< Categorical mood state */
    float mood_momentum;            /**< Rate of mood change */
} nimcp_mood_state_struct_t;

/**
 * @brief Impulse control state
 */
typedef struct {
    float inhibition_strength;      /**< Behavioral inhibition [0-1] */
    float patience;                 /**< Delay tolerance [0-1] */
    float risk_aversion;            /**< Risk avoidance [0-1] */
    float impulsivity;              /**< Impulsive tendency [0-1] */
} nimcp_impulse_state_t;

/**
 * @brief Temporal discounting state
 */
typedef struct {
    float discount_rate;            /**< Hyperbolic discount k */
    float future_orientation;       /**< Long-term thinking [0-1] */
    float delay_tolerance;          /**< Waiting capacity */
} nimcp_temporal_state_t;

/**
 * @brief Sleep-wake state
 */
typedef struct {
    float sleep_pressure;           /**< Homeostatic drive [0-1] */
    float wake_promotion;           /**< Arousal signal [0-1] */
    float circadian_phase;          /**< 0-24 hour cycle position */
} nimcp_sleep_wake_state_t;

/**
 * @brief Raphe configuration
 */
typedef struct {
    float baseline_firing_rate;     /**< Hz */
    float baseline_5ht;             /**< nM */
    float ht_decay_tau;             /**< 5-HT decay time constant (ms) */
    float mood_time_constant;       /**< Mood change rate (ms) */
    float impulse_sensitivity;      /**< 5-HT -> impulse control gain */
    bool enable_autoreceptors;      /**< 5-HT1A autoreceptor feedback */
    float autoreceptor_sensitivity; /**< Autoreceptor gain */
} nimcp_raphe_config_t;

/**
 * @brief Raphe metrics
 */
typedef struct {
    uint32_t update_count;
    float total_simulation_time;
    uint32_t total_spikes;
    float total_5ht_released;
    float avg_mood_valence;
    uint32_t mood_transitions;
    float time_depressed;
    float time_positive;
} nimcp_raphe_metrics_t;

/**
 * @brief Main Raphe system structure
 */
typedef struct nimcp_raphe_system {
    bool initialized;

    /* Neuron pool */
    nimcp_raphe_neuron_pool_t neurons;

    /* Serotonin state */
    float ht_concentration;         /**< Current 5-HT level (nM) */
    float ht_release_rate;          /**< Release rate (nM/ms) */
    float tonic_firing_rate;        /**< Tonic mode rate (Hz) */

    /* Operating mode */
    nimcp_raphe_mode_t mode;
    float mode_duration;            /**< Time in current mode (ms) */

    /* Mood state */
    nimcp_mood_state_struct_t mood;

    /* Impulse control */
    nimcp_impulse_state_t impulse;

    /* Temporal discounting */
    nimcp_temporal_state_t temporal;

    /* Sleep-wake */
    nimcp_sleep_wake_state_t sleep_wake;

    /* Projections */
    nimcp_raphe_projection_t projections[RAPHE_MAX_PROJECTIONS];
    uint32_t num_projections;

    /* Configuration */
    nimcp_raphe_config_t config;

    /* Metrics */
    nimcp_raphe_metrics_t metrics;

    /* Timing */
    float simulation_time;          /**< Total time (ms) */

    /* Status */
    nimcp_raphe_status_t status;

} nimcp_raphe_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Initialize Raphe system with default configuration
 */
nimcp_raphe_error_t nimcp_raphe_init(nimcp_raphe_system_t* raphe, const nimcp_raphe_config_t* config);

/**
 * @brief Shutdown Raphe system
 */
nimcp_raphe_error_t nimcp_raphe_shutdown(nimcp_raphe_system_t* raphe);

/**
 * @brief Reset Raphe to initial state
 */
nimcp_raphe_error_t nimcp_raphe_reset(nimcp_raphe_system_t* raphe);

/**
 * @brief Get default configuration
 */
nimcp_raphe_config_t nimcp_raphe_default_config(void);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update Raphe simulation
 * @param raphe Raphe system
 * @param dt Time step (ms)
 */
nimcp_raphe_error_t nimcp_raphe_update(nimcp_raphe_system_t* raphe, float dt);

/*=============================================================================
 * Mood Regulation API
 *===========================================================================*/

/**
 * @brief Get current mood valence
 */
nimcp_raphe_error_t nimcp_raphe_get_mood(nimcp_raphe_system_t* raphe, float* valence);

/**
 * @brief Get mood state classification
 */
nimcp_raphe_error_t nimcp_raphe_get_mood_state(nimcp_raphe_system_t* raphe, nimcp_mood_state_t* state);

/**
 * @brief Apply mood influence (from limbic inputs)
 */
nimcp_raphe_error_t nimcp_raphe_apply_mood_input(nimcp_raphe_system_t* raphe, float input);

/**
 * @brief Get anxiety level
 */
nimcp_raphe_error_t nimcp_raphe_get_anxiety(nimcp_raphe_system_t* raphe, float* anxiety);

/**
 * @brief Apply anxiety modulation
 */
nimcp_raphe_error_t nimcp_raphe_modulate_anxiety(nimcp_raphe_system_t* raphe, float input);

/*=============================================================================
 * Impulse Control API
 *===========================================================================*/

/**
 * @brief Get impulse control strength
 */
nimcp_raphe_error_t nimcp_raphe_get_inhibition(nimcp_raphe_system_t* raphe, float* inhibition);

/**
 * @brief Get patience/delay tolerance
 */
nimcp_raphe_error_t nimcp_raphe_get_patience(nimcp_raphe_system_t* raphe, float* patience);

/**
 * @brief Get impulsivity level
 */
nimcp_raphe_error_t nimcp_raphe_get_impulsivity(nimcp_raphe_system_t* raphe, float* impulsivity);

/**
 * @brief Compute impulse inhibition signal for behavior
 */
nimcp_raphe_error_t nimcp_raphe_compute_inhibition(
    nimcp_raphe_system_t* raphe,
    float impulse_strength,
    float* inhibition_output
);

/*=============================================================================
 * Temporal Discounting API
 *===========================================================================*/

/**
 * @brief Get temporal discount rate
 */
nimcp_raphe_error_t nimcp_raphe_get_discount_rate(nimcp_raphe_system_t* raphe, float* rate);

/**
 * @brief Compute discounted value of delayed reward
 */
nimcp_raphe_error_t nimcp_raphe_discount_value(
    nimcp_raphe_system_t* raphe,
    float value,
    float delay,
    float* discounted_value
);

/**
 * @brief Get future orientation
 */
nimcp_raphe_error_t nimcp_raphe_get_future_orientation(nimcp_raphe_system_t* raphe, float* orientation);

/*=============================================================================
 * 5-HT Control API
 *===========================================================================*/

/**
 * @brief Apply excitation to Raphe
 */
nimcp_raphe_error_t nimcp_raphe_apply_excitation(nimcp_raphe_system_t* raphe, float strength);

/**
 * @brief Apply inhibition to Raphe
 */
nimcp_raphe_error_t nimcp_raphe_apply_inhibition(nimcp_raphe_system_t* raphe, float strength);

/**
 * @brief Get current 5-HT concentration
 */
nimcp_raphe_error_t nimcp_raphe_get_5ht(nimcp_raphe_system_t* raphe, float* ht);

/**
 * @brief Get current firing rate
 */
nimcp_raphe_error_t nimcp_raphe_get_firing_rate(nimcp_raphe_system_t* raphe, float* rate);

/*=============================================================================
 * Mode API
 *===========================================================================*/

/**
 * @brief Get current operating mode
 */
nimcp_raphe_error_t nimcp_raphe_get_mode(nimcp_raphe_system_t* raphe, nimcp_raphe_mode_t* mode);

/**
 * @brief Set operating mode
 */
nimcp_raphe_error_t nimcp_raphe_set_mode(nimcp_raphe_system_t* raphe, nimcp_raphe_mode_t mode);

/*=============================================================================
 * Projection API
 *===========================================================================*/

/**
 * @brief Add projection to target region
 */
nimcp_raphe_error_t nimcp_raphe_add_projection(
    nimcp_raphe_system_t* raphe,
    nimcp_raphe_target_t target,
    const char* name,
    float weight,
    uint32_t* id
);

/**
 * @brief Get projection by ID
 */
nimcp_raphe_projection_t* nimcp_raphe_get_projection(nimcp_raphe_system_t* raphe, uint32_t id);

/**
 * @brief Get projection by target type
 */
nimcp_raphe_projection_t* nimcp_raphe_get_projection_by_target(
    nimcp_raphe_system_t* raphe,
    nimcp_raphe_target_t target
);

/**
 * @brief Get 5-HT level at target
 */
nimcp_raphe_error_t nimcp_raphe_get_5ht_at_target(
    nimcp_raphe_system_t* raphe,
    nimcp_raphe_target_t target,
    float* ht
);

/*=============================================================================
 * Sleep-Wake API
 *===========================================================================*/

/**
 * @brief Get sleep pressure
 */
nimcp_raphe_error_t nimcp_raphe_get_sleep_pressure(nimcp_raphe_system_t* raphe, float* pressure);

/**
 * @brief Set circadian phase (0-24)
 */
nimcp_raphe_error_t nimcp_raphe_set_circadian_phase(nimcp_raphe_system_t* raphe, float phase);

/**
 * @brief Update sleep-wake dynamics
 */
nimcp_raphe_error_t nimcp_raphe_update_sleep_wake(nimcp_raphe_system_t* raphe, float dt);

/*=============================================================================
 * Status API
 *===========================================================================*/

/**
 * @brief Get Raphe state summary
 */
nimcp_raphe_error_t nimcp_raphe_get_state(
    nimcp_raphe_system_t* raphe,
    float* ht,
    float* mood,
    float* anxiety,
    nimcp_raphe_mode_t* mode
);

/**
 * @brief Get Raphe status
 */
nimcp_raphe_error_t nimcp_raphe_get_status(nimcp_raphe_system_t* raphe, nimcp_raphe_status_t* status);

/**
 * @brief Get metrics
 */
nimcp_raphe_error_t nimcp_raphe_get_metrics(nimcp_raphe_system_t* raphe, nimcp_raphe_metrics_t* metrics);

/**
 * @brief Reset metrics
 */
nimcp_raphe_error_t nimcp_raphe_reset_metrics(nimcp_raphe_system_t* raphe);

/*=============================================================================
 * Utility API
 *===========================================================================*/

/**
 * @brief Get error string
 */
const char* nimcp_raphe_error_string(nimcp_raphe_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_H */

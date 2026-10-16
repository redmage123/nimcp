/**
 * @file nimcp_phasic_tonic.h
 * @brief Phasic vs tonic neuromodulator dynamics (Phase C2.2 Enhancement #2)
 *
 * WHAT: Models burst (phasic) vs baseline (tonic) neurotransmitter release
 * WHY:  Dopamine encodes reward prediction errors via phasic bursts
 * HOW:  Dual-mode concentration with burst triggering and exponential decay
 *
 * BIOLOGICAL BACKGROUND:
 * - Tonic dopamine: 1-5 Hz baseline firing, ~50-100 nM, sustained
 * - Phasic dopamine: 10-20 Hz burst firing, ~1 µM, 100-300ms duration
 * - Phasic bursts signal unexpected rewards (positive TD errors)
 * - Tonic dips signal worse-than-expected outcomes (negative TD errors)
 *
 * @version Phase C2.2 Enhancement #2
 * @date 2025-11-12
 */

#ifndef NIMCP_PHASIC_TONIC_H
#define NIMCP_PHASIC_TONIC_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Phasic-Tonic Configuration and State
// ============================================================================

/**
 * @brief Phasic-tonic neuromodulator system state (tensor-based)
 *
 * Models dual-mode release:
 * - Tonic: Sustained baseline concentration (motivation, mood)
 * - Phasic: Transient bursts (learning signals, salience)
 *
 * TENSOR FORMAT:
 * - tonic_params: [5] tensor - tonic state parameters
 *   [0] = tonic_level (current baseline, µM)
 *   [1] = tonic_target (homeostatic setpoint, µM)
 *   [2] = tonic_min (minimum, µM)
 *   [3] = tonic_max (maximum, µM)
 *   [4] = homeostatic_tau (time constant, seconds)
 *
 * - phasic_params: [4] tensor - phasic burst parameters
 *   [0] = phasic_burst (current burst amplitude, µM)
 *   [1] = burst_decay_tau (decay time constant, seconds)
 *   [2] = burst_amplitude_scale (scaling factor)
 *   [3] = burst_threshold (minimum to trigger)
 *
 * - burst_limits: [2] tensor - burst amplitude limits
 *   [0] = max_burst_amplitude (µM)
 *   [1] = min_burst_amplitude (µM)
 *
 * - autoreceptor_params: [2] tensor - feedback parameters
 *   [0] = autoreceptor_sensitivity [0-1]
 *   [1] = feedback_tau (seconds)
 *
 * - output: [2] tensor - current output state
 *   [0] = total_concentration (tonic + phasic)
 *   [1] = release_rate (µM/s)
 */
typedef struct {
    // === Tensor-based state ===
    nimcp_tensor_t* tonic_params;       /**< Tonic state parameters [5] */
    nimcp_tensor_t* phasic_params;      /**< Phasic burst parameters [4] */
    nimcp_tensor_t* burst_limits;       /**< Burst amplitude limits [2] */
    nimcp_tensor_t* autoreceptor_params;/**< Autoreceptor feedback [2] */
    nimcp_tensor_t* output;             /**< Current output [2] */

    // === Non-tensor state (timestamps and flags) ===
    bool in_burst_state;            /**< Currently in burst mode */
    uint64_t burst_start_time_us;   /**< Burst start timestamp (microseconds) */
    uint32_t burst_duration_ms;     /**< Burst duration (milliseconds) */

    // === Statistics (for monitoring) ===
    uint32_t burst_count;           /**< Number of bursts triggered */
    uint64_t last_burst_time_us;    /**< Last burst timestamp */
    float avg_inter_burst_interval; /**< Average time between bursts (seconds) */

    // === Ownership flag ===
    bool owns_tensors;              /**< Whether tensors should be freed on destroy */
} phasic_tonic_state_t;

/* Tensor indices for tonic_params */
#define PHASIC_TONIC_IDX_TONIC_LEVEL      0
#define PHASIC_TONIC_IDX_TONIC_TARGET     1
#define PHASIC_TONIC_IDX_TONIC_MIN        2
#define PHASIC_TONIC_IDX_TONIC_MAX        3
#define PHASIC_TONIC_IDX_HOMEOSTATIC_TAU  4

/* Tensor indices for phasic_params */
#define PHASIC_TONIC_IDX_PHASIC_BURST     0
#define PHASIC_TONIC_IDX_BURST_DECAY_TAU  1
#define PHASIC_TONIC_IDX_BURST_AMP_SCALE  2
#define PHASIC_TONIC_IDX_BURST_THRESHOLD  3

/* Tensor indices for burst_limits */
#define PHASIC_TONIC_IDX_MAX_BURST_AMP    0
#define PHASIC_TONIC_IDX_MIN_BURST_AMP    1

/* Tensor indices for autoreceptor_params */
#define PHASIC_TONIC_IDX_AUTO_SENSITIVITY 0
#define PHASIC_TONIC_IDX_FEEDBACK_TAU     1

/* Tensor indices for output */
#define PHASIC_TONIC_IDX_TOTAL_CONC       0
#define PHASIC_TONIC_IDX_RELEASE_RATE     1

/**
 * @brief Create phasic-tonic state with tensor storage
 *
 * WHAT: Allocates tensors for phasic-tonic state
 * WHY:  Initialize state with proper tensor memory
 *
 * @return Initialized state (call phasic_tonic_state_destroy to free)
 */
phasic_tonic_state_t phasic_tonic_state_create(void);

/**
 * @brief Destroy phasic-tonic state and free tensor memory
 *
 * @param state State to destroy
 */
void phasic_tonic_state_destroy(phasic_tonic_state_t* state);

/* Backward compatibility accessors for tonic parameters */
float phasic_tonic_get_tonic_level(const phasic_tonic_state_t* state);
float phasic_tonic_get_tonic_target(const phasic_tonic_state_t* state);
float phasic_tonic_get_tonic_min(const phasic_tonic_state_t* state);
float phasic_tonic_get_tonic_max(const phasic_tonic_state_t* state);
float phasic_tonic_get_homeostatic_tau(const phasic_tonic_state_t* state);

void phasic_tonic_set_tonic_level(phasic_tonic_state_t* state, float value);

/* Backward compatibility accessors for phasic parameters */
float phasic_tonic_get_phasic_burst(const phasic_tonic_state_t* state);
float phasic_tonic_get_burst_decay_tau(const phasic_tonic_state_t* state);
float phasic_tonic_get_burst_amplitude_scale(const phasic_tonic_state_t* state);
float phasic_tonic_get_burst_threshold(const phasic_tonic_state_t* state);
float phasic_tonic_get_max_burst_amplitude(const phasic_tonic_state_t* state);
float phasic_tonic_get_min_burst_amplitude(const phasic_tonic_state_t* state);

void phasic_tonic_set_phasic_burst(phasic_tonic_state_t* state, float value);

/* Backward compatibility accessors for autoreceptor parameters */
float phasic_tonic_get_autoreceptor_sensitivity(const phasic_tonic_state_t* state);
float phasic_tonic_get_feedback_tau(const phasic_tonic_state_t* state);

/* Backward compatibility accessors for output */
float phasic_tonic_get_total_concentration(const phasic_tonic_state_t* state);
float phasic_tonic_get_release_rate_value(const phasic_tonic_state_t* state);

void phasic_tonic_set_total_concentration(phasic_tonic_state_t* state, float value);
void phasic_tonic_set_release_rate_value(phasic_tonic_state_t* state, float value);

/**
 * @brief Configuration for phasic-tonic system initialization
 */
typedef struct {
    // Tonic defaults
    float initial_tonic;            /**< Initial tonic level (µM) */
    float tonic_target;             /**< Target tonic level (µM) */
    float tonic_range_min;          /**< Minimum tonic (µM) */
    float tonic_range_max;          /**< Maximum tonic (µM) */
    float homeostatic_tau;          /**< Homeostatic time constant (s) */

    // Phasic defaults
    float burst_decay_tau;          /**< Burst decay time constant (s) */
    float burst_amplitude_scale;    /**< Burst scaling factor */
    float max_burst_amplitude;      /**< Max burst (µM) */

    // Autoreceptor
    float autoreceptor_sensitivity; /**< Feedback strength [0-1] */
    float feedback_tau;             /**< Feedback time constant (s) */
} phasic_tonic_config_t;

// ============================================================================
// Biological Parameter Constants
// ============================================================================

/**
 * @brief Dopamine phasic-tonic parameters (from Schultz et al. 2015)
 */
#define DOPAMINE_TONIC_BASELINE     (0.00005f)  /**< 50 nM = 0.00005 µM */
#define DOPAMINE_TONIC_RANGE_MIN    (0.00001f)  /**< 10 nM */
#define DOPAMINE_TONIC_RANGE_MAX    (0.0001f)   /**< 100 nM */
#define DOPAMINE_PHASIC_PEAK        (0.001f)    /**< 1 µM = 1000 nM */
#define DOPAMINE_BURST_DURATION_MS  (200)       /**< 200 ms typical */
#define DOPAMINE_BURST_DECAY_TAU    (0.15f)     /**< 150 ms decay */
#define DOPAMINE_HOMEOSTATIC_TAU    (60.0f)     /**< 60 s regulation */

/**
 * @brief Serotonin phasic-tonic parameters
 */
#define SEROTONIN_TONIC_BASELINE    (0.00003f)  /**< 30 nM */
#define SEROTONIN_BURST_DURATION_MS (500)       /**< Longer bursts */
#define SEROTONIN_BURST_DECAY_TAU   (0.4f)      /**< 400 ms decay */

/**
 * @brief Norepinephrine phasic-tonic parameters
 */
#define NOREPINEPHRINE_TONIC_BASELINE   (0.00002f)  /**< 20 nM */
#define NOREPINEPHRINE_BURST_DURATION_MS (300)       /**< 300 ms */
#define NOREPINEPHRINE_BURST_DECAY_TAU  (0.2f)      /**< 200 ms decay */

// ============================================================================
// Function Declarations
// ============================================================================

/**
 * @brief Create default phasic-tonic configuration for dopamine
 *
 * @return Configuration with biological dopamine parameters
 */
phasic_tonic_config_t phasic_tonic_config_dopamine_default(void);

/**
 * @brief Create default phasic-tonic configuration for serotonin
 *
 * @return Configuration with biological serotonin parameters
 */
phasic_tonic_config_t phasic_tonic_config_serotonin_default(void);

/**
 * @brief Create default phasic-tonic configuration for norepinephrine
 *
 * @return Configuration with biological norepinephrine parameters
 */
phasic_tonic_config_t phasic_tonic_config_norepinephrine_default(void);

/**
 * @brief Initialize phasic-tonic state
 *
 * @param state State structure to initialize
 * @param config Configuration parameters
 * @param current_time_us Current timestamp in microseconds
 */
void phasic_tonic_init(
    phasic_tonic_state_t* state,
    const phasic_tonic_config_t* config,
    uint64_t current_time_us
);

/**
 * @brief Update phasic-tonic dynamics (call every timestep)
 *
 * @param state State to update
 * @param dt Time step (seconds)
 * @param current_time_us Current timestamp (microseconds)
 */
void phasic_tonic_update(
    phasic_tonic_state_t* state,
    float dt,
    uint64_t current_time_us
);

/**
 * @brief Trigger phasic burst (e.g., from TD error)
 *
 * @param state State to update
 * @param amplitude Burst amplitude (typically proportional to TD error)
 * @param duration_ms Burst duration in milliseconds (0 = use default)
 * @param current_time_us Current timestamp
 * @return true if burst triggered, false if suppressed
 */
bool phasic_tonic_trigger_burst(
    phasic_tonic_state_t* state,
    float amplitude,
    uint32_t duration_ms,
    uint64_t current_time_us
);

/**
 * @brief Induce tonic dip (negative reward prediction error)
 *
 * @param state State to update
 * @param magnitude Dip magnitude [0-1] (0 = no effect, 1 = complete suppression)
 */
void phasic_tonic_induce_dip(
    phasic_tonic_state_t* state,
    float magnitude
);

/**
 * @brief Get total neurotransmitter concentration (tonic + phasic)
 *
 * @param state Current state
 * @return Total concentration (µM)
 */
float phasic_tonic_get_concentration(const phasic_tonic_state_t* state);

/**
 * @brief Get current release rate (for integration with vesicle pools)
 *
 * @param state Current state
 * @return Release rate (µM/second)
 */
float phasic_tonic_get_release_rate(const phasic_tonic_state_t* state);

/**
 * @brief Set tonic target (homeostatic setpoint)
 *
 * Allows modeling chronic effects like:
 * - Depression: Lower tonic target
 * - Mania: Higher tonic target
 * - Medication: Adjust target
 *
 * @param state State to modify
 * @param new_target New tonic target (µM)
 */
void phasic_tonic_set_tonic_target(
    phasic_tonic_state_t* state,
    float new_target
);

/**
 * @brief Apply autoreceptor modulation (external regulation)
 *
 * @param state State to modify
 * @param modulation Modulation factor [0-2] (1.0 = no change, <1 = suppress, >1 = enhance)
 */
void phasic_tonic_apply_autoreceptor_modulation(
    phasic_tonic_state_t* state,
    float modulation
);

/**
 * @brief Encode reward prediction error as dopamine dynamics
 *
 * Maps TD error to phasic/tonic changes:
 * - Positive TD error → phasic burst
 * - Negative TD error → tonic dip
 * - Zero TD error → no change
 *
 * @param state Dopamine phasic-tonic state
 * @param td_error Temporal difference error [-1 to +1]
 * @param current_time_us Current timestamp
 * @return true if burst triggered, false otherwise
 */
bool phasic_tonic_encode_td_error(
    phasic_tonic_state_t* state,
    float td_error,
    uint64_t current_time_us
);

/**
 * @brief Get burst statistics for monitoring
 *
 * @param state Current state
 * @param burst_count Output: Number of bursts
 * @param avg_interval Output: Average inter-burst interval (seconds)
 * @param time_since_last Output: Time since last burst (seconds)
 * @param current_time_us Current timestamp
 */
void phasic_tonic_get_burst_statistics(
    const phasic_tonic_state_t* state,
    uint32_t* burst_count,
    float* avg_interval,
    float* time_since_last,
    uint64_t current_time_us
);

/**
 * @brief Reset state to baseline (useful for experiments)
 *
 * @param state State to reset
 * @param current_time_us Current timestamp
 */
void phasic_tonic_reset(
    phasic_tonic_state_t* state,
    uint64_t current_time_us
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHASIC_TONIC_H */

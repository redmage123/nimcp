/**
 * @file nimcp_auditory_nerve.h
 * @brief Auditory Nerve Fiber (ANF) model with spike generation
 *
 * WHAT: Convert hair cell output to spike trains
 * WHY:  Bridge between peripheral hearing and central auditory processing
 * HOW:  Poisson spiking with refractory periods and phase locking
 *
 * BIOLOGICAL BASIS:
 * - Each IHC innervates 10-30 Type I spiral ganglion neurons (ANFs)
 * - Three fiber types: High-SR (60%), Med-SR (25%), Low-SR (15%)
 * - Phase locking: Precise timing for frequencies < 4 kHz
 * - Rate-place coding: Firing rate encodes sound level and frequency
 *
 * SPECIES ADAPTATIONS:
 * - Dog: Enhanced temporal coding for high frequencies
 * - Bat: Microsecond precision (<50 μs) for echolocation
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_AUDITORY_NERVE_H
#define NIMCP_AUDITORY_NERVE_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_basilar_membrane.h"
#include "perception/nimcp_hair_cells.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Maximum spike rate (spikes/sec) */
#define ANF_MAX_SPIKE_RATE          400.0f

/** Spontaneous rate thresholds */
#define ANF_HIGH_SR_THRESHOLD       18.0f   /**< High SR: > 18 sp/s */
#define ANF_LOW_SR_THRESHOLD        0.5f    /**< Low SR: < 0.5 sp/s */

/** Phase locking limit */
#define ANF_PHASE_LOCK_LIMIT_HZ     4000.0f /**< Phase locking below 4 kHz */

/** Refractory periods */
#define ANF_ABSOLUTE_REFRACTORY_MS  0.6f    /**< Absolute refractory (ms) */
#define ANF_RELATIVE_REFRACTORY_MS  1.5f    /**< Relative refractory (ms) */

/** Fiber population defaults */
#define ANF_FIBERS_PER_IHC          20      /**< Average fibers per IHC */
#define ANF_HIGH_SR_FRACTION        0.60f   /**< 60% high SR fibers */
#define ANF_MED_SR_FRACTION         0.25f   /**< 25% medium SR fibers */
#define ANF_LOW_SR_FRACTION         0.15f   /**< 15% low SR fibers */

/** Bat mode: microsecond precision */
#define ANF_BAT_TEMPORAL_RES_US     10.0f   /**< 10 μs precision for bat */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spontaneous rate type
 *
 * BIOLOGICAL:
 * - High-SR: Low threshold, saturate early, fast response
 * - Medium-SR: Intermediate properties
 * - Low-SR: High threshold, wide dynamic range, slow onset
 */
typedef enum {
    ANF_SR_HIGH,                    /**< High spontaneous rate (>18 sp/s) */
    ANF_SR_MEDIUM,                  /**< Medium spontaneous rate (0.5-18) */
    ANF_SR_LOW                      /**< Low spontaneous rate (<0.5 sp/s) */
} anf_sr_type_t;

/**
 * @brief Spike generation model
 */
typedef enum {
    ANF_MODEL_POISSON,              /**< Inhomogeneous Poisson process */
    ANF_MODEL_RENEWAL,              /**< Renewal process with adaptation */
    ANF_MODEL_INTEGRATE_FIRE,       /**< Leaky integrate-and-fire */
    ANF_MODEL_BRUCE                 /**< Bruce et al. 2018 model */
} anf_model_type_t;

/**
 * @brief Output encoding type
 */
typedef enum {
    ANF_ENCODE_SPIKE_TRAIN,         /**< Binary spike train */
    ANF_ENCODE_FIRING_RATE,         /**< Instantaneous firing rate */
    ANF_ENCODE_PSTH,                /**< Peri-stimulus time histogram */
    ANF_ENCODE_NEUROGRAM            /**< Population neurogram */
} anf_encode_type_t;

/**
 * @brief Fiber health status
 */
typedef enum {
    ANF_FIBER_HEALTHY,              /**< Normal function */
    ANF_FIBER_DEGRADED,             /**< Reduced sensitivity */
    ANF_FIBER_SYNAPTOPATHY,         /**< Synapse damage (hidden hearing loss) */
    ANF_FIBER_DEAD                  /**< No response */
} anf_fiber_health_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Single auditory nerve fiber state
 *
 * BIOLOGICAL: Each fiber has distinct SR type and tuning
 */
typedef struct {
    /* Fiber properties */
    anf_sr_type_t sr_type;          /**< Spontaneous rate type */
    float spontaneous_rate;         /**< Actual spontaneous rate (sp/s) */
    float threshold_db;             /**< Response threshold (dB SPL) */

    /* Dynamic range */
    float dynamic_range_db;         /**< Dynamic range (dB) */
    float saturation_rate;          /**< Saturation firing rate */

    /* Spiking state */
    float last_spike_time;          /**< Time since last spike (ms) */
    bool in_refractory;             /**< Currently in refractory period */
    float refractory_remaining;     /**< Remaining refractory time (ms) */

    /* Phase locking */
    float phase;                    /**< Current phase for locking */
    float phase_lock_strength;      /**< Vector strength (0-1) */

    /* Adaptation */
    float rapid_adapt_state;        /**< Rapid adaptation (onset response) */
    float short_term_adapt;         /**< Short-term adaptation */
    float long_term_adapt;          /**< Long-term adaptation */

    /* Health */
    anf_fiber_health_t health;      /**< Fiber health status */
    float efficiency;               /**< Transmission efficiency (0-1) */

} anf_fiber_t;

/**
 * @brief ANF population for one frequency channel
 *
 * Contains multiple fibers of different SR types
 */
typedef struct {
    float center_freq_hz;           /**< Characteristic frequency */
    uint32_t num_fibers;            /**< Total fibers in population */

    /* Fiber counts by type */
    uint32_t num_high_sr;           /**< Number of high-SR fibers */
    uint32_t num_med_sr;            /**< Number of medium-SR fibers */
    uint32_t num_low_sr;            /**< Number of low-SR fibers */

    /* Population firing */
    float population_rate;          /**< Average population firing rate */
    float population_synchrony;     /**< Population synchrony (0-1) */

    /* Individual fibers (optional detailed mode) */
    anf_fiber_t* fibers;            /**< Array of individual fibers */

} anf_population_t;

/**
 * @brief ANF bank - all ANF populations
 */
typedef struct anf_bank anf_bank_t;

/**
 * @brief ANF configuration
 */
typedef struct {
    uint32_t num_channels;          /**< Number of frequency channels */
    uint32_t fibers_per_channel;    /**< Fibers per frequency channel */

    /* SR distribution */
    float high_sr_fraction;         /**< Fraction of high-SR fibers */
    float med_sr_fraction;          /**< Fraction of medium-SR fibers */
    float low_sr_fraction;          /**< Fraction of low-SR fibers */

    /* Model selection */
    anf_model_type_t model;         /**< Spike generation model */
    anf_encode_type_t encoding;     /**< Output encoding type */

    /* Temporal parameters */
    float abs_refractory_ms;        /**< Absolute refractory period */
    float rel_refractory_ms;        /**< Relative refractory period */

    /* Phase locking */
    float phase_lock_cutoff_hz;     /**< Phase locking cutoff frequency */
    bool enable_phase_locking;      /**< Enable phase-locked spiking */

    /* Adaptation */
    bool enable_adaptation;         /**< Enable spike rate adaptation */
    float rapid_adapt_tau_ms;       /**< Rapid adaptation time constant */
    float short_term_tau_ms;        /**< Short-term adaptation tau */

    /* Mode */
    bm_hearing_mode_t hearing_mode; /**< Species/hearing mode */

    /* Sample rate */
    uint32_t sample_rate;           /**< Audio sample rate (Hz) */

    /* Output options */
    bool output_individual_fibers;  /**< Track individual fiber spikes */
    uint32_t psth_bin_ms;           /**< PSTH bin width (ms) */

} anf_config_t;

/**
 * @brief ANF output structure
 */
typedef struct {
    /* Population outputs (always available) */
    float* firing_rate;             /**< Population firing rate [num_channels] */
    float* synchrony;               /**< Phase synchrony [num_channels] */

    /* Spike train output (if enabled) */
    uint8_t** spike_trains;         /**< Spike trains [channels][samples] */
    uint32_t num_samples;           /**< Samples in spike train */

    /* Neurogram output (population histogram) */
    float** neurogram;              /**< Neurogram [channels][time_bins] */
    uint32_t num_time_bins;         /**< Number of time bins */

    /* PSTH output */
    float** psth;                   /**< PSTH [channels][bins] */
    uint32_t num_psth_bins;         /**< Number of PSTH bins */

    /* Per-type firing rates */
    float* high_sr_rate;            /**< High-SR population rate */
    float* med_sr_rate;             /**< Medium-SR population rate */
    float* low_sr_rate;             /**< Low-SR population rate */

    uint32_t num_channels;          /**< Number of channels */

} anf_output_t;

/**
 * @brief ANF statistics
 */
typedef struct {
    /* Processing stats */
    uint64_t samples_processed;     /**< Total samples processed */
    uint64_t spikes_generated;      /**< Total spikes generated */
    float avg_processing_time_us;   /**< Average processing time */

    /* Firing statistics */
    float avg_firing_rate;          /**< Average firing rate (sp/s) */
    float max_firing_rate;          /**< Peak firing rate */
    float avg_synchrony;            /**< Average phase synchrony */

    /* Health statistics */
    float avg_fiber_efficiency;     /**< Average fiber efficiency */
    uint32_t healthy_fiber_count;   /**< Number of healthy fibers */
    uint32_t degraded_fiber_count;  /**< Number of degraded fibers */

} anf_stats_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default ANF configuration
 *
 * @param num_channels Number of frequency channels
 * @param mode Hearing mode (human/dog/bat)
 * @return Default configuration
 */
anf_config_t anf_config_default(uint32_t num_channels, bm_hearing_mode_t mode);

/**
 * @brief Validate ANF configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid, error code otherwise
 */
nimcp_error_t anf_config_validate(const anf_config_t* config);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create ANF bank
 *
 * WHAT: Initialize auditory nerve fiber populations
 * WHY:  Convert hair cell output to spike trains
 * HOW:  Create fiber populations with SR type distribution
 *
 * @param config ANF configuration
 * @return ANF bank or NULL on failure
 */
anf_bank_t* anf_bank_create(const anf_config_t* config);

/**
 * @brief Destroy ANF bank
 *
 * @param bank ANF bank to destroy
 */
void anf_bank_destroy(anf_bank_t* bank);

/**
 * @brief Process hair cell output through ANF
 *
 * WHAT: Generate spike trains from IHC glutamate release
 * WHY:  Create neural representation for central processing
 * HOW:  Poisson spiking modulated by synaptic drive
 *
 * BIOLOGICAL:
 * - Glutamate release drives spike generation
 * - Refractory periods limit maximum rate
 * - Phase locking preserves temporal fine structure
 * - Adaptation shapes onset/offset responses
 *
 * @param bank ANF bank
 * @param hc_output Hair cell output (glutamate release)
 * @param dt_ms Time step (milliseconds)
 * @param output ANF output structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_process(
    anf_bank_t* bank,
    const hc_bank_output_t* hc_output,
    float dt_ms,
    anf_output_t* output
);

/**
 * @brief Process with explicit synaptic input
 *
 * Alternative input method using glutamate levels directly
 *
 * @param bank ANF bank
 * @param glutamate_release Release rates [num_channels]
 * @param bm_phase BM phase for phase locking [num_channels] (can be NULL)
 * @param dt_ms Time step
 * @param output ANF output
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_process_direct(
    anf_bank_t* bank,
    const float* glutamate_release,
    const float* bm_phase,
    float dt_ms,
    anf_output_t* output
);

/**
 * @brief Reset ANF bank state
 *
 * @param bank ANF bank
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_reset(anf_bank_t* bank);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get number of channels
 *
 * @param bank ANF bank
 * @return Number of channels
 */
uint32_t anf_bank_get_num_channels(const anf_bank_t* bank);

/**
 * @brief Get total number of fibers
 *
 * @param bank ANF bank
 * @return Total fiber count
 */
uint32_t anf_bank_get_num_fibers(const anf_bank_t* bank);

/**
 * @brief Get population for channel
 *
 * @param bank ANF bank
 * @param channel Channel index
 * @param population Output population structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_get_population(
    const anf_bank_t* bank,
    uint32_t channel,
    anf_population_t* population
);

/**
 * @brief Get statistics
 *
 * @param bank ANF bank
 * @param stats Output statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_get_stats(
    const anf_bank_t* bank,
    anf_stats_t* stats
);

//=============================================================================
// Health and Damage Simulation
//=============================================================================

/**
 * @brief Set fiber health for channel
 *
 * CLINICAL: Model cochlear synaptopathy (hidden hearing loss)
 *
 * @param bank ANF bank
 * @param channel Channel index
 * @param sr_type SR type to affect (or all if < 0)
 * @param health Health status
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_set_fiber_health(
    anf_bank_t* bank,
    uint32_t channel,
    anf_sr_type_t sr_type,
    anf_fiber_health_t health
);

/**
 * @brief Apply age-related synaptopathy
 *
 * CLINICAL: Model age-related loss of low-SR fibers
 *
 * @param bank ANF bank
 * @param age_years Simulated age
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_apply_aging(anf_bank_t* bank, float age_years);

/**
 * @brief Apply noise damage pattern
 *
 * CLINICAL: Model noise-induced synaptopathy
 *
 * @param bank ANF bank
 * @param exposure_db Noise exposure level (dB SPL)
 * @param duration_hours Exposure duration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_apply_noise_damage(
    anf_bank_t* bank,
    float exposure_db,
    float duration_hours
);

//=============================================================================
// Phase Locking Analysis
//=============================================================================

/**
 * @brief Compute vector strength for channel
 *
 * BIOLOGICAL: Vector strength measures phase locking quality
 * - VS = 0: No phase locking (random phases)
 * - VS = 1: Perfect phase locking (all spikes at same phase)
 *
 * @param bank ANF bank
 * @param channel Channel index
 * @return Vector strength (0-1)
 */
float anf_bank_compute_vector_strength(
    const anf_bank_t* bank,
    uint32_t channel
);

/**
 * @brief Get phase distribution
 *
 * @param bank ANF bank
 * @param channel Channel index
 * @param phase_histogram Output histogram [num_bins]
 * @param num_bins Number of phase bins
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_get_phase_histogram(
    const anf_bank_t* bank,
    uint32_t channel,
    float* phase_histogram,
    uint32_t num_bins
);

//=============================================================================
// Neurogram Generation
//=============================================================================

/**
 * @brief Generate neurogram from spike trains
 *
 * WHAT: Create population activity visualization
 * WHY:  Analyze temporal-spectral representation
 * HOW:  Bin spikes across time and frequency
 *
 * @param bank ANF bank
 * @param duration_ms Duration to analyze
 * @param time_bin_ms Time bin width
 * @param neurogram Output neurogram [channels][time_bins]
 * @param num_time_bins Output: number of time bins
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_generate_neurogram(
    const anf_bank_t* bank,
    float duration_ms,
    float time_bin_ms,
    float** neurogram,
    uint32_t* num_time_bins
);

/**
 * @brief Get compound action potential (CAP)
 *
 * CLINICAL: Sum of all fiber responses (like ABR Wave I)
 *
 * @param bank ANF bank
 * @param cap_waveform Output waveform
 * @param num_samples Number of samples
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_get_cap(
    const anf_bank_t* bank,
    float* cap_waveform,
    uint32_t num_samples
);

//=============================================================================
// Bat Mode - Microsecond Precision
//=============================================================================

/**
 * @brief Enable bat echolocation mode
 *
 * BIOLOGICAL: Bats achieve ~10 μs temporal precision
 * for target range discrimination
 *
 * @param bank ANF bank
 * @param temporal_res_us Temporal resolution (microseconds)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_enable_bat_mode(
    anf_bank_t* bank,
    float temporal_res_us
);

/**
 * @brief Get precise spike times (bat mode)
 *
 * @param bank ANF bank
 * @param channel Channel index
 * @param spike_times_us Output spike times in microseconds
 * @param max_spikes Maximum spikes to return
 * @param num_spikes Output: actual number of spikes
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t anf_bank_get_precise_spikes(
    const anf_bank_t* bank,
    uint32_t channel,
    float* spike_times_us,
    uint32_t max_spikes,
    uint32_t* num_spikes
);

//=============================================================================
// Output Allocation
//=============================================================================

/**
 * @brief Create ANF output structure
 *
 * @param bank ANF bank
 * @param max_samples Maximum samples per call
 * @return Output structure or NULL
 */
anf_output_t* anf_output_create(anf_bank_t* bank, uint32_t max_samples);

/**
 * @brief Destroy ANF output
 *
 * @param output Output to destroy
 */
void anf_output_destroy(anf_output_t* output);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDITORY_NERVE_H */

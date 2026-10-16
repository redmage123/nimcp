/**
 * @file nimcp_hair_cells.h
 * @brief Inner and Outer Hair Cell transduction models
 *
 * WHAT: Mechanotransduction converting basilar membrane motion to neural signals
 * WHY:  Bridge between mechanical (BM) and neural (ANF) stages of hearing
 * HOW:  Boltzmann transduction, prestin electromotility, adaptation
 *
 * BIOLOGICAL BASIS:
 * - Inner Hair Cells (IHC): Primary sensory transducers, drive ANF
 *   - ~3500 IHCs per cochlea (human)
 *   - Ribbon synapses release glutamate to ANF
 *   - Rate/place coding of sound intensity and frequency
 *
 * - Outer Hair Cells (OHC): Active amplifiers, frequency sharpening
 *   - ~12000 OHCs per cochlea (human)
 *   - Prestin motor protein provides electromotility
 *   - Provide 40-60 dB gain at low sound levels
 *   - Generate otoacoustic emissions (OAEs)
 *
 * SPECIES ADAPTATIONS:
 * - Dog: Enhanced OHC sensitivity for ultrasonic range
 * - Bat: Extended OHC bandwidth for echolocation frequencies
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_HAIR_CELLS_H
#define NIMCP_HAIR_CELLS_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_basilar_membrane.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Inner Hair Cell constants */
#define HC_IHC_SATURATION_DB        120.0f  /**< IHC saturation level */
#define HC_IHC_THRESHOLD_DB         0.0f    /**< IHC threshold (hearing threshold) */
#define HC_IHC_SPONTANEOUS_RATE     50.0f   /**< Baseline vesicle release rate (Hz) */

/** Outer Hair Cell constants */
#define HC_OHC_MAX_GAIN_DB          60.0f   /**< Maximum OHC amplification */
#define HC_OHC_COMPRESSION_RATIO    0.3f    /**< Compression at high levels */
#define HC_OHC_KNEE_POINT_DB        20.0f   /**< Compression knee point */

/** Adaptation time constants */
#define HC_ADAPT_FAST_MS            1.0f    /**< Fast adaptation (ms) */
#define HC_ADAPT_SLOW_MS            50.0f   /**< Slow adaptation (ms) */

/** Ribbon synapse parameters */
#define HC_RIBBON_VESICLES          300     /**< Vesicles per ribbon synapse */
#define HC_RIBBON_RESTOCK_MS        10.0f   /**< Vesicle restocking time */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Hair cell type
 */
typedef enum {
    HC_TYPE_INNER,                  /**< Inner hair cell (sensory) */
    HC_TYPE_OUTER                   /**< Outer hair cell (amplifier) */
} hc_type_t;

/**
 * @brief Transduction model
 *
 * BIOLOGICAL: Different biophysical models with accuracy tradeoffs
 */
typedef enum {
    HC_MODEL_BOLTZMANN,             /**< Simple Boltzmann sigmoid */
    HC_MODEL_MEDDIS,                /**< Meddis IHC model */
    HC_MODEL_SUMNER,                /**< Sumner et al. model */
    HC_MODEL_ZILANY                 /**< Zilany et al. 2014 model */
} hc_model_type_t;

/**
 * @brief OHC health status
 *
 * CLINICAL: OHCs are vulnerable to damage (noise, ototoxicity)
 */
typedef enum {
    HC_OHC_HEALTHY,                 /**< Full OHC function */
    HC_OHC_MILD_DAMAGE,             /**< ~20% function loss */
    HC_OHC_MODERATE_DAMAGE,         /**< ~50% function loss */
    HC_OHC_SEVERE_DAMAGE,           /**< ~80% function loss */
    HC_OHC_DEAD                     /**< Complete OHC loss */
} hc_ohc_health_t;

/**
 * @brief IHC health status
 */
typedef enum {
    HC_IHC_HEALTHY,                 /**< Full IHC function */
    HC_IHC_MILD_DAMAGE,             /**< Reduced sensitivity */
    HC_IHC_MODERATE_DAMAGE,         /**< Significant loss */
    HC_IHC_SEVERE_DAMAGE,           /**< Minimal function */
    HC_IHC_DEAD                     /**< No response */
} hc_ihc_health_t;

//=============================================================================
// Inner Hair Cell Structures
//=============================================================================

/**
 * @brief Single Inner Hair Cell state
 *
 * BIOLOGICAL: IHC converts BM velocity to receptor potential,
 * then to neurotransmitter release to auditory nerve
 */
typedef struct {
    /* Transduction */
    float receptor_potential;       /**< Receptor potential (mV) */
    float resting_potential;        /**< Resting potential (default -70mV) */
    float saturation_potential;     /**< Saturation potential (default -30mV) */

    /* Adaptation */
    float fast_adapt_state;         /**< Fast adaptation state */
    float slow_adapt_state;         /**< Slow adaptation state */

    /* Synaptic release */
    float glutamate_release;        /**< Current release rate */
    float vesicle_pool;             /**< Available vesicles (0-1) */
    float restock_rate;             /**< Vesicle restocking rate */

    /* Channel properties */
    float mechano_channel_open;     /**< Mechanotransduction channel open prob */
    float calcium_current;          /**< Ca2+ current driving release */

    /* Health */
    hc_ihc_health_t health;         /**< IHC health status */
    float efficiency;               /**< Transduction efficiency (0-1) */

} ihc_state_t;

/**
 * @brief IHC Bank - all IHCs for filterbank channels
 */
typedef struct ihc_bank ihc_bank_t;

/**
 * @brief IHC configuration
 */
typedef struct {
    uint32_t num_channels;          /**< Number of IHCs (= BM channels) */
    hc_model_type_t model;          /**< Transduction model */

    /* Transduction parameters */
    float operating_point;          /**< Resting open probability */
    float asymmetry;                /**< Rectification asymmetry */
    float slope;                    /**< Transduction slope */

    /* Adaptation */
    float fast_tau_ms;              /**< Fast adaptation time constant */
    float slow_tau_ms;              /**< Slow adaptation time constant */

    /* Synaptic parameters */
    float max_release_rate;         /**< Maximum vesicle release rate */
    float spontaneous_rate;         /**< Spontaneous release rate */

    /* Sample rate */
    uint32_t sample_rate;           /**< Audio sample rate (Hz) */

} ihc_config_t;

/**
 * @brief IHC output
 */
typedef struct {
    float* receptor_potential;      /**< Receptor potentials [num_channels] */
    float* glutamate_release;       /**< Release rates [num_channels] */
    float* adaptation_state;        /**< Adaptation level [num_channels] */
    uint32_t num_channels;
} ihc_output_t;

//=============================================================================
// Outer Hair Cell Structures
//=============================================================================

/**
 * @brief Single Outer Hair Cell state
 *
 * BIOLOGICAL: OHC provides active amplification via prestin motor
 * - Electromotility: Cell length changes with receptor potential
 * - Nonlinear compression: 0.3 dB/dB at high levels
 * - Frequency sharpening via local feedback
 */
typedef struct {
    /* Electromotility */
    float receptor_potential;       /**< Receptor potential (mV) */
    float cell_length_change;       /**< Electromotile response (nm) */
    float prestin_state;            /**< Prestin motor activation (0-1) */

    /* Gain control */
    float gain_linear;              /**< Current gain (linear) */
    float compression_state;        /**< Compression level (0-1) */

    /* ACh modulation (efferent) */
    float ach_level;                /**< Acetylcholine level (0-1) */
    float ach_gain_modulation;      /**< Gain modulation from ACh */

    /* Health */
    hc_ohc_health_t health;         /**< OHC health status */
    float survival_fraction;        /**< Fraction of healthy OHCs (0-1) */

    /* Otoacoustic emission */
    float oae_contribution;         /**< OAE signal contribution */

} ohc_state_t;

/**
 * @brief OHC Bank - all OHCs for filterbank channels
 */
typedef struct ohc_bank ohc_bank_t;

/**
 * @brief OHC configuration
 */
typedef struct {
    uint32_t num_channels;          /**< Number of OHC channels */

    /* Gain parameters */
    float max_gain_db;              /**< Maximum gain (dB) */
    float compression_ratio;        /**< Compression ratio above knee */
    float knee_point_db;            /**< Compression knee (dB SPL) */

    /* Prestin parameters */
    float prestin_saturation;       /**< Prestin saturation level */
    float electromotility_gain;     /**< Electromotility coefficient */

    /* Frequency-dependent parameters */
    float* gain_per_channel;        /**< Per-channel maximum gain */
    float* bandwidth_adjustment;    /**< Per-channel bandwidth mod */

    /* ACh modulation (efferent control) */
    bool enable_efferent;           /**< Enable ACh modulation */
    float ach_decay_ms;             /**< ACh decay time constant */

    /* Sample rate */
    uint32_t sample_rate;           /**< Audio sample rate (Hz) */

} ohc_config_t;

/**
 * @brief OHC output
 */
typedef struct {
    float* gain;                    /**< Active gain values [num_channels] */
    float* amplified_bm;            /**< Amplified BM motion [num_channels] */
    float* oae_signal;              /**< OAE contribution [num_channels] */
    uint32_t num_channels;
} ohc_output_t;

//=============================================================================
// Combined Hair Cell Bank
//=============================================================================

/**
 * @brief Complete hair cell bank (IHC + OHC)
 */
typedef struct hair_cell_bank hair_cell_bank_t;

/**
 * @brief Hair cell bank configuration
 */
typedef struct {
    ihc_config_t ihc_config;        /**< IHC configuration */
    ohc_config_t ohc_config;        /**< OHC configuration */

    /* Coupling parameters */
    bool enable_ohc_ihc_coupling;   /**< OHC output feeds IHC */
    float coupling_strength;        /**< Coupling coefficient */

    /* Mode */
    bm_hearing_mode_t hearing_mode; /**< Species/hearing mode */

} hc_bank_config_t;

/**
 * @brief Hair cell bank output
 */
typedef struct {
    ihc_output_t ihc;               /**< IHC outputs */
    ohc_output_t ohc;               /**< OHC outputs */

    /* Combined outputs */
    float* neural_drive;            /**< Signal to auditory nerve */
    uint32_t num_channels;

} hc_bank_output_t;

/**
 * @brief Hair cell statistics
 */
typedef struct {
    /* Processing stats */
    uint64_t samples_processed;
    float avg_processing_time_us;

    /* Health stats */
    float avg_ihc_efficiency;       /**< Average IHC efficiency */
    float avg_ohc_survival;         /**< Average OHC survival */
    uint32_t dead_ihc_count;        /**< Number of dead IHCs */
    uint32_t dead_ohc_count;        /**< Number of dead OHCs */

    /* Signal stats */
    float avg_ohc_gain_db;          /**< Average OHC gain */
    float avg_release_rate;         /**< Average glutamate release */

} hc_bank_stats_t;

//=============================================================================
// IHC Bank API
//=============================================================================

/**
 * @brief Get default IHC configuration
 *
 * @param num_channels Number of frequency channels
 * @param mode Hearing mode
 * @return Default configuration
 */
ihc_config_t ihc_config_default(uint32_t num_channels, bm_hearing_mode_t mode);

/**
 * @brief Create IHC bank
 *
 * @param config IHC configuration
 * @return IHC bank or NULL on failure
 */
ihc_bank_t* ihc_bank_create(const ihc_config_t* config);

/**
 * @brief Destroy IHC bank
 *
 * @param bank IHC bank to destroy
 */
void ihc_bank_destroy(ihc_bank_t* bank);

/**
 * @brief Process basilar membrane input through IHCs
 *
 * WHAT: Convert BM velocity to glutamate release
 * WHY:  Drive auditory nerve fibers
 * HOW:  Boltzmann transduction + adaptation + ribbon synapse
 *
 * BIOLOGICAL:
 * - Stereocilia deflection opens mechanotransduction channels
 * - K+ influx depolarizes cell → Ca2+ triggers vesicle release
 * - Adaptation reduces response to sustained stimuli
 *
 * @param bank IHC bank
 * @param bm_velocity BM velocity [num_channels] (from basilar membrane)
 * @param num_samples Number of samples
 * @param output IHC output structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ihc_bank_process(
    ihc_bank_t* bank,
    const float* bm_velocity,
    uint32_t num_samples,
    ihc_output_t* output
);

/**
 * @brief Reset IHC bank state
 *
 * @param bank IHC bank
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ihc_bank_reset(ihc_bank_t* bank);

/**
 * @brief Get IHC state for channel
 *
 * @param bank IHC bank
 * @param channel Channel index
 * @param state Output state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ihc_bank_get_state(
    const ihc_bank_t* bank,
    uint32_t channel,
    ihc_state_t* state
);

/**
 * @brief Set IHC health for channel
 *
 * CLINICAL: Simulate hearing loss from IHC damage
 *
 * @param bank IHC bank
 * @param channel Channel index
 * @param health Health status
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ihc_bank_set_health(
    ihc_bank_t* bank,
    uint32_t channel,
    hc_ihc_health_t health
);

//=============================================================================
// OHC Bank API
//=============================================================================

/**
 * @brief Get default OHC configuration
 *
 * @param num_channels Number of frequency channels
 * @param mode Hearing mode
 * @return Default configuration
 */
ohc_config_t ohc_config_default(uint32_t num_channels, bm_hearing_mode_t mode);

/**
 * @brief Create OHC bank
 *
 * @param config OHC configuration
 * @return OHC bank or NULL on failure
 */
ohc_bank_t* ohc_bank_create(const ohc_config_t* config);

/**
 * @brief Destroy OHC bank
 *
 * @param bank OHC bank to destroy
 */
void ohc_bank_destroy(ohc_bank_t* bank);

/**
 * @brief Process basilar membrane through OHCs
 *
 * WHAT: Apply active amplification to BM motion
 * WHY:  Enhance weak sounds, compress loud sounds
 * HOW:  Prestin electromotility with level-dependent gain
 *
 * BIOLOGICAL:
 * - OHC prestin changes cell length with membrane potential
 * - Provides 40-60 dB gain for soft sounds
 * - Compressive nonlinearity at high levels (~0.3 dB/dB)
 * - Sharpens frequency tuning (critical bands)
 *
 * @param bank OHC bank
 * @param bm_input BM displacement [num_channels]
 * @param num_samples Number of samples
 * @param output OHC output structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ohc_bank_process(
    ohc_bank_t* bank,
    const float* bm_input,
    uint32_t num_samples,
    ohc_output_t* output
);

/**
 * @brief Reset OHC bank state
 *
 * @param bank OHC bank
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ohc_bank_reset(ohc_bank_t* bank);

/**
 * @brief Set OHC health for channel
 *
 * CLINICAL: Simulate noise-induced hearing loss
 * OHCs are more vulnerable than IHCs to damage
 *
 * @param bank OHC bank
 * @param channel Channel index
 * @param health Health status
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ohc_bank_set_health(
    ohc_bank_t* bank,
    uint32_t channel,
    hc_ohc_health_t health
);

/**
 * @brief Set efferent ACh level
 *
 * BIOLOGICAL: Medial olivocochlear (MOC) efferents release ACh
 * to modulate OHC gain (protective, attentional)
 *
 * @param bank OHC bank
 * @param ach_level ACh level (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ohc_bank_set_ach(ohc_bank_t* bank, float ach_level);

/**
 * @brief Get otoacoustic emission signal
 *
 * BIOLOGICAL: OAEs are sounds produced by OHC electromotility
 * Used clinically for hearing screening
 *
 * @param bank OHC bank
 * @param oae_signal Output OAE signal [num_channels]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t ohc_bank_get_oae(
    const ohc_bank_t* bank,
    float* oae_signal
);

//=============================================================================
// Combined Hair Cell Bank API
//=============================================================================

/**
 * @brief Get default hair cell bank configuration
 *
 * @param num_channels Number of frequency channels
 * @param mode Hearing mode
 * @return Default configuration
 */
hc_bank_config_t hc_bank_config_default(
    uint32_t num_channels,
    bm_hearing_mode_t mode
);

/**
 * @brief Create combined hair cell bank
 *
 * @param config Bank configuration
 * @return Hair cell bank or NULL on failure
 */
hair_cell_bank_t* hair_cell_bank_create(const hc_bank_config_t* config);

/**
 * @brief Destroy hair cell bank
 *
 * @param bank Hair cell bank to destroy
 */
void hair_cell_bank_destroy(hair_cell_bank_t* bank);

/**
 * @brief Process basilar membrane through all hair cells
 *
 * WHAT: Complete hair cell transduction (OHC + IHC)
 * WHY:  Generate neural drive for auditory nerve
 * HOW:  BM → OHC amplification → IHC transduction → release
 *
 * @param bank Hair cell bank
 * @param bm_output Basilar membrane output
 * @param output Hair cell bank output
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hair_cell_bank_process(
    hair_cell_bank_t* bank,
    const bm_output_t* bm_output,
    hc_bank_output_t* output
);

/**
 * @brief Reset hair cell bank
 *
 * @param bank Hair cell bank
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hair_cell_bank_reset(hair_cell_bank_t* bank);

/**
 * @brief Get hair cell bank statistics
 *
 * @param bank Hair cell bank
 * @param stats Output statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hair_cell_bank_get_stats(
    const hair_cell_bank_t* bank,
    hc_bank_stats_t* stats
);

/**
 * @brief Get IHC bank from combined bank
 *
 * @param bank Hair cell bank
 * @return IHC bank pointer (do not free)
 */
ihc_bank_t* hair_cell_bank_get_ihc(hair_cell_bank_t* bank);

/**
 * @brief Get OHC bank from combined bank
 *
 * @param bank Hair cell bank
 * @return OHC bank pointer (do not free)
 */
ohc_bank_t* hair_cell_bank_get_ohc(hair_cell_bank_t* bank);

//=============================================================================
// Output Allocation
//=============================================================================

/**
 * @brief Create IHC output structure
 *
 * @param num_channels Number of channels
 * @return Output structure or NULL
 */
ihc_output_t* ihc_output_create(uint32_t num_channels);

/**
 * @brief Destroy IHC output
 *
 * @param output Output to destroy
 */
void ihc_output_destroy(ihc_output_t* output);

/**
 * @brief Create OHC output structure
 *
 * @param num_channels Number of channels
 * @return Output structure or NULL
 */
ohc_output_t* ohc_output_create(uint32_t num_channels);

/**
 * @brief Destroy OHC output
 *
 * @param output Output to destroy
 */
void ohc_output_destroy(ohc_output_t* output);

/**
 * @brief Create hair cell bank output
 *
 * @param num_channels Number of channels
 * @return Output structure or NULL
 */
hc_bank_output_t* hc_bank_output_create(uint32_t num_channels);

/**
 * @brief Destroy hair cell bank output
 *
 * @param output Output to destroy
 */
void hc_bank_output_destroy(hc_bank_output_t* output);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HAIR_CELLS_H */

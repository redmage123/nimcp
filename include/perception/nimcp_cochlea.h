/**
 * @file nimcp_cochlea.h
 * @brief Complete cochlear processing module with brain integration
 *
 * WHAT: Unified cochlear processing pipeline (BM → HC → ANF)
 * WHY:  Provide biologically-accurate auditory front-end for NIMCP brain
 * HOW:  Integrate basilar membrane, hair cells, and auditory nerve
 *
 * COMPLETE AUDITORY PATHWAY:
 * 1. Audio input → Basilar membrane (gammatone filterbank)
 * 2. Basilar membrane → Outer hair cells (active amplification)
 * 3. OHC output → Inner hair cells (transduction)
 * 4. IHC output → Auditory nerve fibers (spike generation)
 * 5. ANF output → Brainstem/thalamus/cortex (brain integration)
 *
 * SPECIES MODES:
 * - Human: 20 Hz - 20 kHz, standard processing
 * - Dog: 67 Hz - 65 kHz, pinnae mobility, enhanced localization
 * - Bat: 1 kHz - 200 kHz, echolocation, microsecond precision
 * - Hybrid: Combined capabilities
 *
 * BRAIN INTEGRATION BRIDGES:
 * - Medulla: Brainstem pathway, protective reflexes
 * - Thalamus: MGN relay, attention gating
 * - Audio cortex: A1 tonotopic mapping
 * - Cortical columns: Frequency hypercolumns
 * - Immune system: Damage monitoring
 * - Bio-async: Cross-module messaging
 * - Brain KG: Self-awareness registration
 * - Recursive cognition: Goal-directed listening
 * - Collective cognition: Distributed hearing
 * - Occipital: Audiovisual binding
 * - Broca: Speech perception
 *
 * @author NIMCP Development Team
 * @date 2026
 * @version 3.0
 */

#ifndef NIMCP_COCHLEA_H
#define NIMCP_COCHLEA_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/error/nimcp_error_codes.h"
#include "perception/nimcp_basilar_membrane.h"
#include "perception/nimcp_hair_cells.h"
#include "perception/nimcp_auditory_nerve.h"
#include "perception/nimcp_cochlea_extended.h"

/* Bio-async communication */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/* Brain integration */
typedef struct brain_struct* brain_t;
typedef struct medulla medulla_t;
typedef struct thalamus thalamus_t;
typedef struct audio_cortex audio_cortex_t;
typedef struct brain_kg brain_kg_t;

//=============================================================================
// Configuration Constants
//=============================================================================

/** Default processing buffer size */
#define COCHLEA_DEFAULT_BUFFER_MS   10

/** Bio-async module ID */
#define BIO_MODULE_COCHLEA          0x1100
#define BIO_MODULE_COCHLEA_DOG      0x1101
#define BIO_MODULE_COCHLEA_BAT      0x1102

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Cochlea configuration
 */
typedef struct {
    /* Basic parameters */
    uint32_t sample_rate;           /**< Audio sample rate (Hz) */
    uint32_t num_channels;          /**< Number of frequency channels */

    /* Hearing mode */
    bm_hearing_mode_t hearing_mode; /**< Human/dog/bat/hybrid */

    /* Component configurations */
    bm_config_t bm_config;          /**< Basilar membrane config */
    hc_bank_config_t hc_config;     /**< Hair cell config */
    anf_config_t anf_config;        /**< Auditory nerve config */

    /* Extended hearing (optional) */
    bool enable_extended_hearing;   /**< Enable dog/bat modes */
    ext_hearing_config_t ext_config;/**< Extended hearing config */

    /* Processing options */
    bool enable_ohc_amplification;  /**< Enable OHC active gain */
    bool enable_phase_locking;      /**< Enable ANF phase locking */
    bool enable_adaptation;         /**< Enable adaptation */

    /* Brain integration */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    bool enable_brain_kg;           /**< Register with brain KG */
    bool enable_logging;            /**< Enable logging */

    /* Output options */
    bool output_envelope;           /**< Output envelope */
    bool output_spikes;             /**< Output spike trains */
    bool output_neurogram;          /**< Output neurogram */

} cochlea_config_t;

/**
 * @brief Cochlea output structure
 *
 * Contains all processed outputs from cochlear pipeline
 */
typedef struct {
    /* Basilar membrane outputs */
    bm_output_t* bm_output;         /**< BM frequency decomposition */

    /* Hair cell outputs */
    hc_bank_output_t* hc_output;    /**< Hair cell transduction */

    /* Auditory nerve outputs */
    anf_output_t* anf_output;       /**< Spike generation */

    /* Extended hearing outputs (if enabled) */
    dog_localization_t* dog_localization;   /**< Dog sound localization */
    echolocation_result_t* echo_result;     /**< Bat echolocation */

    /* Derived features */
    float* channel_energy;          /**< Per-channel energy [num_channels] */
    float* channel_db;              /**< Per-channel level (dB) */

    /* Aggregate measures */
    float total_energy;             /**< Total signal energy */
    float peak_frequency_hz;        /**< Dominant frequency */
    float overall_level_db;         /**< Overall level (dB SPL) */

    /* Temporal features */
    float* onset_strength;          /**< Onset detection [num_channels] */
    bool sound_onset_detected;      /**< Global onset flag */
    bool speech_detected;           /**< Speech likelihood flag */

    /* Metadata */
    uint32_t num_channels;          /**< Number of channels */
    uint32_t num_samples;           /**< Samples processed */
    uint64_t timestamp_ms;          /**< Processing timestamp */

} cochlea_output_t;

/**
 * @brief Cochlea statistics
 */
typedef struct {
    /* Processing statistics */
    uint64_t samples_processed;     /**< Total samples processed */
    uint64_t frames_processed;      /**< Total frames processed */
    float avg_processing_time_us;   /**< Average processing time */
    float peak_processing_time_us;  /**< Peak processing time */

    /* Signal statistics */
    float avg_level_db;             /**< Average signal level */
    float peak_level_db;            /**< Peak signal level */

    /* Component statistics */
    bm_stats_t bm_stats;            /**< Basilar membrane stats */
    hc_bank_stats_t hc_stats;       /**< Hair cell stats */
    anf_stats_t anf_stats;          /**< Auditory nerve stats */

    /* Health statistics */
    float avg_ohc_survival;         /**< Average OHC health */
    float avg_ihc_efficiency;       /**< Average IHC health */
    uint32_t damaged_channels;      /**< Number of damaged channels */

} cochlea_stats_t;

/**
 * @brief Cochlea instance (opaque)
 */
typedef struct cochlea cochlea_t;

//=============================================================================
// Configuration Helpers
//=============================================================================

/**
 * @brief Get default cochlea configuration
 *
 * @param mode Hearing mode (human/dog/bat/hybrid)
 * @param sample_rate Audio sample rate (Hz)
 * @return Default configuration
 */
cochlea_config_t cochlea_config_default(
    bm_hearing_mode_t mode,
    uint32_t sample_rate
);

/**
 * @brief Validate cochlea configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid, error code otherwise
 */
nimcp_error_t cochlea_config_validate(const cochlea_config_t* config);

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create cochlea instance
 *
 * WHAT: Initialize complete cochlear processing pipeline
 * WHY:  Prepare for biologically-accurate audio processing
 * HOW:  Create and link BM, HC, and ANF components
 *
 * @param config Cochlea configuration
 * @return Cochlea instance or NULL on failure
 */
cochlea_t* cochlea_create(const cochlea_config_t* config);

/**
 * @brief Destroy cochlea instance
 *
 * @param cochlea Cochlea to destroy (can be NULL)
 */
void cochlea_destroy(cochlea_t* cochlea);

/**
 * @brief Process audio through complete cochlear pipeline
 *
 * WHAT: Full cochlear processing (BM → HC → ANF)
 * WHY:  Generate neural representation of audio
 * HOW:  Sequential processing through all stages
 *
 * BIOLOGICAL: Simulates complete peripheral auditory pathway
 *
 * @param cochlea Cochlea instance
 * @param audio_in Input audio samples (mono, float32)
 * @param num_samples Number of input samples
 * @param output Output structure (pre-allocated)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_process(
    cochlea_t* cochlea,
    const float* audio_in,
    uint32_t num_samples,
    cochlea_output_t* output
);

/**
 * @brief Process stereo audio (for localization)
 *
 * @param cochlea Cochlea instance
 * @param audio_left Left channel
 * @param audio_right Right channel
 * @param num_samples Number of samples per channel
 * @param output Output structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_process_stereo(
    cochlea_t* cochlea,
    const float* audio_left,
    const float* audio_right,
    uint32_t num_samples,
    cochlea_output_t* output
);

/**
 * @brief Reset cochlea state
 *
 * @param cochlea Cochlea instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_reset(cochlea_t* cochlea);

//=============================================================================
// Output Management
//=============================================================================

/**
 * @brief Create cochlea output structure
 *
 * @param cochlea Cochlea instance
 * @param max_samples Maximum samples per call
 * @return Output structure or NULL
 */
cochlea_output_t* cochlea_output_create(
    cochlea_t* cochlea,
    uint32_t max_samples
);

/**
 * @brief Destroy cochlea output
 *
 * @param output Output to destroy
 */
void cochlea_output_destroy(cochlea_output_t* output);

/**
 * @brief Clear cochlea output (reset for reuse)
 *
 * @param output Output to clear
 */
void cochlea_output_clear(cochlea_output_t* output);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get number of frequency channels
 *
 * @param cochlea Cochlea instance
 * @return Number of channels
 */
uint32_t cochlea_get_num_channels(const cochlea_t* cochlea);

/**
 * @brief Get center frequency for channel
 *
 * @param cochlea Cochlea instance
 * @param channel Channel index
 * @return Center frequency (Hz), or -1 on error
 */
float cochlea_get_channel_freq(const cochlea_t* cochlea, uint32_t channel);

/**
 * @brief Get all center frequencies
 *
 * @param cochlea Cochlea instance
 * @param freqs Output array [num_channels]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_get_all_freqs(
    const cochlea_t* cochlea,
    float* freqs
);

/**
 * @brief Get current hearing mode
 *
 * @param cochlea Cochlea instance
 * @return Hearing mode
 */
bm_hearing_mode_t cochlea_get_hearing_mode(const cochlea_t* cochlea);

/**
 * @brief Get statistics
 *
 * @param cochlea Cochlea instance
 * @param stats Output statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_get_stats(
    const cochlea_t* cochlea,
    cochlea_stats_t* stats
);

//=============================================================================
// Component Access
//=============================================================================

/**
 * @brief Get basilar membrane component
 *
 * @param cochlea Cochlea instance
 * @return Basilar membrane (do not free)
 */
basilar_membrane_t* cochlea_get_basilar_membrane(cochlea_t* cochlea);

/**
 * @brief Get hair cell bank component
 *
 * @param cochlea Cochlea instance
 * @return Hair cell bank (do not free)
 */
hair_cell_bank_t* cochlea_get_hair_cells(cochlea_t* cochlea);

/**
 * @brief Get auditory nerve bank component
 *
 * @param cochlea Cochlea instance
 * @return ANF bank (do not free)
 */
anf_bank_t* cochlea_get_auditory_nerve(cochlea_t* cochlea);

/**
 * @brief Get extended hearing processor
 *
 * @param cochlea Cochlea instance
 * @return Extended hearing (or NULL if not enabled)
 */
ext_hearing_t* cochlea_get_extended_hearing(cochlea_t* cochlea);

//=============================================================================
// Hearing Mode Control
//=============================================================================

/**
 * @brief Switch hearing mode
 *
 * @param cochlea Cochlea instance
 * @param mode Target mode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_set_hearing_mode(
    cochlea_t* cochlea,
    bm_hearing_mode_t mode
);

/**
 * @brief Enable extended hearing mode
 *
 * @param cochlea Cochlea instance
 * @param mode Extended mode (dog/bat/hybrid)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_enable_extended_mode(
    cochlea_t* cochlea,
    ext_hearing_mode_t mode
);

//=============================================================================
// Gain and Modulation
//=============================================================================

/**
 * @brief Set global gain
 *
 * @param cochlea Cochlea instance
 * @param gain_db Gain in dB
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_set_gain(cochlea_t* cochlea, float gain_db);

/**
 * @brief Set channel-specific gain
 *
 * @param cochlea Cochlea instance
 * @param channel Channel index
 * @param gain_db Gain in dB
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_set_channel_gain(
    cochlea_t* cochlea,
    uint32_t channel,
    float gain_db
);

/**
 * @brief Apply attention-based gain modulation
 *
 * BIOLOGICAL: Models top-down attention effects on cochlear gain
 *
 * @param cochlea Cochlea instance
 * @param attention_freq_hz Attended frequency (Hz)
 * @param attention_bandwidth Bandwidth of attention (octaves)
 * @param attention_gain Additional gain at attended frequency
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_apply_attention(
    cochlea_t* cochlea,
    float attention_freq_hz,
    float attention_bandwidth,
    float attention_gain
);

/**
 * @brief Apply efferent modulation
 *
 * BIOLOGICAL: MOC efferents release ACh to modulate OHC gain
 *
 * @param cochlea Cochlea instance
 * @param ach_level ACh level (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_apply_efferent(cochlea_t* cochlea, float ach_level);

//=============================================================================
// Health and Damage Simulation
//=============================================================================

/**
 * @brief Apply hearing damage to channel
 *
 * @param cochlea Cochlea instance
 * @param channel Channel index
 * @param ohc_damage OHC damage level (0-1)
 * @param ihc_damage IHC damage level (0-1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_apply_damage(
    cochlea_t* cochlea,
    uint32_t channel,
    float ohc_damage,
    float ihc_damage
);

/**
 * @brief Apply age-related hearing loss profile
 *
 * @param cochlea Cochlea instance
 * @param age_years Simulated age
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_apply_aging(cochlea_t* cochlea, float age_years);

/**
 * @brief Apply noise-induced hearing loss
 *
 * @param cochlea Cochlea instance
 * @param exposure_db Noise exposure (dB SPL)
 * @param duration_hours Exposure duration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_apply_noise_damage(
    cochlea_t* cochlea,
    float exposure_db,
    float duration_hours
);

/**
 * @brief Get overall hearing health
 *
 * @param cochlea Cochlea instance
 * @return Health score (0-1, 1 = fully healthy)
 */
float cochlea_get_health(const cochlea_t* cochlea);

//=============================================================================
// Brain Integration
//=============================================================================

/**
 * @brief Associate brain with cochlea
 *
 * WHAT: Set brain reference for full integration
 * WHY:  Enable neuromodulation and cross-system communication
 * HOW:  Store brain pointer, register with bio-async router
 *
 * @param cochlea Cochlea instance
 * @param brain Brain instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_set_brain(cochlea_t* cochlea, brain_t brain);

/**
 * @brief Get bio-async module context
 *
 * @param cochlea Cochlea instance
 * @return Bio-async context, or NULL if not enabled
 */
bio_module_context_t cochlea_get_bio_context(cochlea_t* cochlea);

/**
 * @brief Process pending bio-async messages
 *
 * @param cochlea Cochlea instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t cochlea_process_bio_messages(
    cochlea_t* cochlea,
    uint32_t max_messages
);

/**
 * @brief Register cochlea with brain knowledge graph
 *
 * WHAT: Create KG nodes for cochlea self-awareness
 * WHY:  Enable introspection of cochlear capabilities
 * HOW:  Register module, components, and connections as nodes
 *
 * @param cochlea Cochlea instance
 * @param brain_kg Brain knowledge graph
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_register_with_kg(
    cochlea_t* cochlea,
    brain_kg_t* brain_kg
);

//=============================================================================
// Event Broadcasting
//=============================================================================

/**
 * @brief Broadcast audio onset event
 *
 * @param cochlea Cochlea instance
 * @param peak_freq_hz Peak frequency
 * @param level_db Signal level
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_broadcast_audio_onset(
    cochlea_t* cochlea,
    float peak_freq_hz,
    float level_db
);

/**
 * @brief Broadcast speech detected event
 *
 * @param cochlea Cochlea instance
 * @param speech_confidence Speech detection confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_broadcast_speech_detected(
    cochlea_t* cochlea,
    float speech_confidence
);

/**
 * @brief Broadcast echolocation target event (bat mode)
 *
 * @param cochlea Cochlea instance
 * @param target Target information
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_broadcast_echo_target(
    cochlea_t* cochlea,
    const echolocation_target_t* target
);

//=============================================================================
// Protective Mechanisms
//=============================================================================

/**
 * @brief Trigger acoustic reflex (stapedius)
 *
 * BIOLOGICAL: Loud sounds trigger stapedius muscle contraction
 * to protect inner ear (latency ~25-150 ms)
 *
 * @param cochlea Cochlea instance
 * @param sound_level_db Sound level (dB SPL)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cochlea_trigger_acoustic_reflex(
    cochlea_t* cochlea,
    float sound_level_db
);

/**
 * @brief Check if cochlea is in protective mode
 *
 * @param cochlea Cochlea instance
 * @return true if protective mechanisms active
 */
bool cochlea_is_in_protection_mode(const cochlea_t* cochlea);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COCHLEA_H */

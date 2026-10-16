//=============================================================================
// nimcp_inferior_colliculus.h - Inferior Colliculus (Auditory Midbrain)
//=============================================================================
/**
 * @file nimcp_inferior_colliculus.h
 * @brief Inferior colliculus implementation for auditory midbrain processing
 *
 * BIOLOGICAL BASIS:
 * The inferior colliculus (IC) is a mandatory relay nucleus in the auditory
 * midbrain. ALL ascending auditory information passes through IC before
 * reaching the thalamic MGN and auditory cortex. It performs:
 *
 * - Tonotopic frequency analysis (ICC - central nucleus)
 * - Sound source localization via ITD/ILD (ICX - external nucleus)
 * - Temporal pattern detection (onset, sustained, offset responses)
 * - Multisensory integration with superior colliculus
 *
 * SUBDIVISIONS:
 * - ICC (Central Nucleus): Tonotopic, receives ascending lemniscal input
 * - ICX (External Nucleus): Spatial map, computes azimuth/elevation
 * - ICD (Dorsal Cortex): Descending cortical modulation
 *
 * PATHWAYS:
 * - IC -> MGN (thalamus): Primary ascending auditory relay
 * - IC -> SC: Auditory-driven orienting (sound localization -> gaze)
 * - IC <- auditory cortex: Top-down modulation via ICD
 *
 * @version 1.0.0
 * @date 2026-03-05
 */

#ifndef NIMCP_INFERIOR_COLLICULUS_H
#define NIMCP_INFERIOR_COLLICULUS_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define IC_DEFAULT_NUM_CHANNELS     64      /**< Frequency channels (log-spaced) */
#define IC_MIN_FREQ_HZ              20.0f   /**< Lowest center frequency */
#define IC_MAX_FREQ_HZ              20000.0f /**< Highest center frequency */
#define IC_MAGIC                    0x1C011C5U   /**< Magic number */
#define IC_MAX_AZIMUTH_DEG          90.0f   /**< Maximum azimuth estimate */
#define IC_MAX_ELEVATION_DEG        45.0f   /**< Maximum elevation estimate */
#define IC_ITD_MAX_US               700.0f  /**< Max interaural time diff (us) */
#define IC_ILD_MAX_DB               20.0f   /**< Max interaural level diff (dB) */

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief IC subdivision
 */
typedef enum {
    IC_SUBDIVISION_ICC,             /**< Central nucleus (tonotopic) */
    IC_SUBDIVISION_ICX,             /**< External nucleus (spatial) */
    IC_SUBDIVISION_ICD,             /**< Dorsal cortex (descending) */
    IC_SUBDIVISION_COUNT
} ic_subdivision_t;

/**
 * @brief Temporal response type
 */
typedef enum {
    IC_RESPONSE_ONSET,              /**< Onset transient */
    IC_RESPONSE_SUSTAINED,          /**< Sustained response */
    IC_RESPONSE_OFFSET,             /**< Offset response */
    IC_RESPONSE_COUNT
} ic_response_type_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief IC configuration
 */
typedef struct {
    uint32_t num_frequency_channels;    /**< Number of tonotopic channels */
    float min_freq_hz;                  /**< Lowest center frequency */
    float max_freq_hz;                  /**< Highest center frequency */

    float itd_weight;                   /**< ITD contribution to localization */
    float ild_weight;                   /**< ILD contribution to localization */
    float onset_decay;                  /**< Onset response decay rate */
    float sustained_adaptation;         /**< Sustained response adaptation */

    float spatial_resolution;           /**< Spatial map resolution [0-1] */
    float frequency_resolution;         /**< Frequency resolution [0-1] */

    bool enable_cortical_modulation;    /**< Enable top-down from ICD */
    bool enable_sc_relay;               /**< Enable relay to superior colliculus */
} ic_config_t;

/**
 * @brief IC statistics
 */
typedef struct {
    float azimuth_estimate;             /**< Current azimuth (-90 to +90 deg) */
    float elevation_estimate;           /**< Current elevation (-45 to +45 deg) */
    float mean_activation;              /**< Mean channel activation */
    float peak_frequency_hz;            /**< Frequency of peak activation */
    uint32_t peak_channel;              /**< Channel with peak activation */
    uint32_t update_count;              /**< Number of updates processed */
    uint64_t last_update_us;            /**< Last update timestamp (us) */
} ic_stats_t;

/**
 * @brief Main IC handle (opaque)
 */
typedef struct inferior_colliculus inferior_colliculus_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Get default IC configuration
 * @return Default configuration struct
 */
ic_config_t ic_default_config(void);

/**
 * @brief Create inferior colliculus
 * @param config Configuration (NULL for defaults)
 * @return IC handle, or NULL on failure
 */
inferior_colliculus_t* ic_create(const ic_config_t* config);

/**
 * @brief Destroy inferior colliculus
 * @param ic IC handle (NULL-safe)
 */
void ic_destroy(inferior_colliculus_t* ic);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Update IC dynamics
 * @param ic IC handle
 * @param dt_s Timestep in seconds
 * @return 0 on success, -1 on error
 */
int ic_update(inferior_colliculus_t* ic, float dt_s);

/**
 * @brief Process binaural audio input
 *
 * Computes tonotopic response (ICC), spatial localization (ICX via ITD/ILD),
 * and temporal pattern detection (onset/sustained).
 *
 * @param ic IC handle
 * @param left Left ear samples
 * @param right Right ear samples
 * @param num_samples Number of samples per channel
 * @return 0 on success, -1 on error
 */
int ic_process_audio(inferior_colliculus_t* ic,
                     const float* left,
                     const float* right,
                     uint32_t num_samples);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

/**
 * @brief Get estimated sound source azimuth
 * @param ic IC handle
 * @return Azimuth in degrees (-90 to +90), or 0 on error
 */
float ic_get_azimuth(const inferior_colliculus_t* ic);

/**
 * @brief Get estimated sound source elevation
 * @param ic IC handle
 * @return Elevation in degrees (-45 to +45), or 0 on error
 */
float ic_get_elevation(const inferior_colliculus_t* ic);

/**
 * @brief Get activation for a specific frequency channel
 * @param ic IC handle
 * @param channel Channel index
 * @return Activation [0-1], or -1 on error
 */
float ic_get_channel_activation(const inferior_colliculus_t* ic, uint32_t channel);

/**
 * @brief Get ICC (tonotopic) response array
 * @param ic IC handle
 * @param out_response Output buffer (must hold num_frequency_channels floats)
 * @param buf_size Buffer size
 * @return 0 on success, -1 on error
 */
int ic_get_icc_response(const inferior_colliculus_t* ic,
                        float* out_response,
                        uint32_t buf_size);

/**
 * @brief Get ICX (spatial) response array
 * @param ic IC handle
 * @param out_response Output buffer (must hold num_frequency_channels floats)
 * @param buf_size Buffer size
 * @return 0 on success, -1 on error
 */
int ic_get_icx_response(const inferior_colliculus_t* ic,
                        float* out_response,
                        uint32_t buf_size);

/**
 * @brief Get IC statistics
 * @param ic IC handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int ic_get_stats(const inferior_colliculus_t* ic, ic_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFERIOR_COLLICULUS_H */

/**
 * @file nimcp_surprise_pink_noise_bridge.h
 * @brief Bridge between Surprise Amplifier and Pink Noise system
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Biologically realistic 1/f noise for surprise baseline parameters
 * WHY:  Neural systems operate with 1/f noise; surprise thresholds and sensitivity
 *       should fluctuate naturally; high surprise adapts noise amplitude
 * HOW:  Pink noise → surprise parameter injection; surprise level → noise adaptation
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * PINK NOISE → SURPRISE:
 * - 1/f noise injected into threshold, sensitivity, decay, refractory parameters
 * - Natural fluctuations prevent rigid threshold behavior
 * - Per-target amplitude scaling for nuanced noise application
 * - Reference: He (2014) "Scale-free brain activity"
 *
 * SURPRISE → NOISE:
 * - High surprise increases noise amplitude (uncertainty-driven adaptation)
 * - Low surprise allows noise to return to baseline
 * - Temporal smoothing prevents rapid noise oscillations
 *
 * ERROR CODE RANGE: 28800-28899 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_PINK_NOISE_BRIDGE_H
#define NIMCP_SURPRISE_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct surprise_amplifier;
struct nimcp_health_agent;

/* ============================================================================
 * Error Codes (Range: 28800-28899)
 * ============================================================================ */

#define NIMCP_SURPRISE_PINK_NOISE_ERROR_BASE           28800
#define NIMCP_SURPRISE_PINK_NOISE_ERROR_NULL_POINTER   (NIMCP_SURPRISE_PINK_NOISE_ERROR_BASE + 1)
#define NIMCP_SURPRISE_PINK_NOISE_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_PINK_NOISE_ERROR_BASE + 2)
#define NIMCP_SURPRISE_PINK_NOISE_ERROR_NO_MEMORY      (NIMCP_SURPRISE_PINK_NOISE_ERROR_BASE + 3)
#define NIMCP_SURPRISE_PINK_NOISE_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_PINK_NOISE_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_PINK_NOISE_NUM_TARGETS             4

/** @brief Noise target indices */
#define SURPRISE_PINK_NOISE_TARGET_THRESHOLD     0
#define SURPRISE_PINK_NOISE_TARGET_SENSITIVITY   1
#define SURPRISE_PINK_NOISE_TARGET_DECAY         2
#define SURPRISE_PINK_NOISE_TARGET_REFRACTORY    3

#define SURPRISE_PINK_NOISE_DEFAULT_BASE_AMPLITUDE   0.05f
#define SURPRISE_PINK_NOISE_DEFAULT_ALPHA            1.0f
#define SURPRISE_PINK_NOISE_DEFAULT_ADAPT_RATE       0.01f
#define SURPRISE_PINK_NOISE_DEFAULT_SMOOTHING        0.9f
#define SURPRISE_PINK_NOISE_DEFAULT_MIN_AMPLITUDE    0.001f
#define SURPRISE_PINK_NOISE_DEFAULT_MAX_AMPLITUDE    0.2f

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for surprise-pink noise bridge
 */
typedef struct {
    float base_amplitude;               /**< Base noise amplitude [0.05] */
    float alpha;                        /**< 1/f^alpha exponent (1.0 = pink) [1.0] */
    float adaptation_rate;              /**< Noise adaptation rate to surprise [0.01] */
    float target_amplitudes[SURPRISE_PINK_NOISE_NUM_TARGETS]; /**< Per-target noise scales */
    float temporal_smoothing;           /**< EMA smoothing factor [0.9] */
    float min_amplitude;                /**< Minimum noise amplitude [0.001] */
    float max_amplitude;                /**< Maximum noise amplitude [0.2] */
    bool enable_bio_async;              /**< Bio-async messaging [true] */
    bool enable_logging;                /**< Diagnostic logging [true] */
} surprise_pink_noise_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float current_noise[SURPRISE_PINK_NOISE_NUM_TARGETS]; /**< Current noise per target */
    float effective_amplitude;          /**< Current effective amplitude */
    float adaptation_factor;            /**< Current adaptation factor */
    uint64_t samples_generated;         /**< Total noise samples generated */
} surprise_pink_noise_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t noise_injections;          /**< Noise injection events */
    uint64_t adaptations;               /**< Amplitude adaptation events */
    uint64_t amplitude_changes;         /**< Significant amplitude changes */
    uint64_t total_updates;             /**< Total update cycles */
} surprise_pink_noise_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_pink_noise_bridge surprise_pink_noise_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_pink_noise_config_t surprise_pink_noise_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_pink_noise_bridge_t* surprise_pink_noise_bridge_create(
    const surprise_pink_noise_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_pink_noise_bridge_destroy(surprise_pink_noise_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_pink_noise_bridge_reset(surprise_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_pink_noise_bridge_connect_amplifier(
    surprise_pink_noise_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to bio-async router */
int surprise_pink_noise_bridge_connect_bio_async(
    surprise_pink_noise_bridge_t* bridge,
    void* router);

/** @brief Disconnect from bio-async router */
int surprise_pink_noise_bridge_disconnect_bio_async(
    surprise_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Inject noise into surprise parameters
 * @param bridge Bridge handle
 * @return 0 on success, error code otherwise
 */
int surprise_pink_noise_inject(surprise_pink_noise_bridge_t* bridge);

/**
 * @brief Adapt noise amplitude based on current surprise level
 * @param bridge Bridge handle
 * @param surprise_level Current surprise level [0-1]
 * @return 0 on success, error code otherwise
 */
int surprise_pink_noise_adapt_amplitude(
    surprise_pink_noise_bridge_t* bridge,
    float surprise_level);

/** @brief Periodic update */
int surprise_pink_noise_bridge_update(
    surprise_pink_noise_bridge_t* bridge,
    float dt_seconds);

/**
 * @brief Get current noise value for a specific target
 * @return Noise value, 0.0f on error
 */
float surprise_pink_noise_get_for_target(
    const surprise_pink_noise_bridge_t* bridge,
    uint32_t target);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_pink_noise_bridge_get_effects(
    const surprise_pink_noise_bridge_t* bridge,
    surprise_pink_noise_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_pink_noise_bridge_get_stats(
    const surprise_pink_noise_bridge_t* bridge,
    surprise_pink_noise_stats_t* stats_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_pink_noise_bridge_set_health_agent(
    surprise_pink_noise_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_PINK_NOISE_BRIDGE_H */

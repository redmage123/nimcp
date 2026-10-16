/**
 * @file nimcp_surprise_snn_bridge.h
 * @brief Bridge between Surprise Amplifier and Spiking Neural Network
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Encode surprise signals as spike trains; decode SNN activity as surprise
 * WHY:  Spiking networks provide temporal precision for surprise encoding
 * HOW:  Surprise magnitude → firing rate/temporal patterns; SNN activity → surprise
 *
 * BIOLOGICAL BASIS:
 * ==========================================================================
 *
 * SURPRISE → SNN:
 * - Surprise events encoded as spike bursts (rate/temporal/population/phase coding)
 * - Magnitude maps to instantaneous firing rate
 * - Four channels for PE, conflict, novelty, and hypothesis surprise
 * - Reference: Izhikevich (2006) "Polychronization"
 *
 * SNN → SURPRISE:
 * - Network activity level modulates surprise sensitivity
 * - Spike patterns carry temporal information about surprise dynamics
 * - Dominant channel indicates surprise type
 *
 * ERROR CODE RANGE: 28500-28599 (Module-specific)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURPRISE_SNN_BRIDGE_H
#define NIMCP_SURPRISE_SNN_BRIDGE_H

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
 * Error Codes (Range: 28500-28599)
 * ============================================================================ */

#define NIMCP_SURPRISE_SNN_ERROR_BASE           28500
#define NIMCP_SURPRISE_SNN_ERROR_NULL_POINTER   (NIMCP_SURPRISE_SNN_ERROR_BASE + 1)
#define NIMCP_SURPRISE_SNN_ERROR_INVALID_PARAM  (NIMCP_SURPRISE_SNN_ERROR_BASE + 2)
#define NIMCP_SURPRISE_SNN_ERROR_NO_MEMORY      (NIMCP_SURPRISE_SNN_ERROR_BASE + 3)
#define NIMCP_SURPRISE_SNN_ERROR_NOT_CONNECTED  (NIMCP_SURPRISE_SNN_ERROR_BASE + 4)

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURPRISE_SNN_DEFAULT_DT_MS              1.0f
#define SURPRISE_SNN_DEFAULT_NEURONS_PER_CH     16
#define SURPRISE_SNN_DEFAULT_THRESHOLD          0.5f
#define SURPRISE_SNN_DEFAULT_REFRACTORY_MS      2.0f
#define SURPRISE_SNN_DEFAULT_DECAY_FACTOR       0.95f
#define SURPRISE_SNN_DEFAULT_HISTORY_SIZE       32
#define SURPRISE_SNN_NUM_CHANNELS               4

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** @brief Spike encoding method */
typedef enum {
    SURPRISE_SNN_ENCODING_RATE = 0,     /**< Rate coding */
    SURPRISE_SNN_ENCODING_TEMPORAL,     /**< Temporal coding */
    SURPRISE_SNN_ENCODING_POPULATION,   /**< Population coding */
    SURPRISE_SNN_ENCODING_PHASE         /**< Phase coding */
} surprise_snn_encoding_t;

/** @brief Surprise SNN channels */
typedef enum {
    SURPRISE_SNN_CHANNEL_PE = 0,        /**< Prediction error channel */
    SURPRISE_SNN_CHANNEL_CONFLICT,      /**< Conflict channel */
    SURPRISE_SNN_CHANNEL_NOVELTY,       /**< Novelty channel */
    SURPRISE_SNN_CHANNEL_HYPOTHESIS     /**< Hypothesis channel */
} surprise_snn_channel_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for surprise-SNN bridge
 */
typedef struct {
    float dt_ms;                        /**< Simulation timestep [1.0] */
    uint32_t neurons_per_channel;       /**< Neurons per surprise channel [16] */
    surprise_snn_encoding_t encoding_type; /**< Encoding method [RATE] */
    float threshold;                    /**< Spike threshold [0.5] */
    float refractory_ms;                /**< Refractory period [2.0] */
    float decay_factor;                 /**< Membrane potential decay [0.95] */
    uint32_t history_size;              /**< History buffer for novelty [32] */
    float channel_weights[SURPRISE_SNN_NUM_CHANNELS]; /**< Per-channel weights */
    bool enable_bio_async;              /**< Bio-async messaging [true] */
    bool enable_logging;                /**< Diagnostic logging [true] */
} surprise_snn_config_t;

/**
 * @brief Effects computed by the bridge
 */
typedef struct {
    float combined_activity;            /**< Combined activity across channels */
    float channel_activations[SURPRISE_SNN_NUM_CHANNELS]; /**< Per-channel activation */
    surprise_snn_channel_t dominant_channel; /**< Most active channel */
    float spike_rate;                   /**< Current average spike rate */
    float confidence;                   /**< Decoding confidence */
    bool high_activity_flag;            /**< True if activity exceeds threshold */
} surprise_snn_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_spikes;              /**< Total spikes generated */
    uint64_t encoding_events;           /**< Surprise encoding events */
    uint64_t channel_spikes[SURPRISE_SNN_NUM_CHANNELS]; /**< Per-channel spike counts */
    uint64_t novelty_detections;        /**< Novelty events detected */
    uint64_t total_updates;             /**< Total update cycles */
} surprise_snn_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct surprise_snn_bridge surprise_snn_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/** @brief Create default configuration */
surprise_snn_config_t surprise_snn_bridge_default_config(void);

/** @brief Create bridge (NULL config = defaults) */
surprise_snn_bridge_t* surprise_snn_bridge_create(
    const surprise_snn_config_t* config);

/** @brief Destroy bridge (NULL-safe) */
void surprise_snn_bridge_destroy(surprise_snn_bridge_t* bridge);

/** @brief Reset state, preserving config and connections */
int surprise_snn_bridge_reset(surprise_snn_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/** @brief Connect to surprise amplifier */
int surprise_snn_bridge_connect_amplifier(
    surprise_snn_bridge_t* bridge,
    struct surprise_amplifier* amp);

/** @brief Connect to SNN system */
int surprise_snn_bridge_connect_snn(
    surprise_snn_bridge_t* bridge,
    void* snn);

/** @brief Connect to bio-async router */
int surprise_snn_bridge_connect_bio_async(
    surprise_snn_bridge_t* bridge,
    void* router);

/** @brief Disconnect from bio-async router */
int surprise_snn_bridge_disconnect_bio_async(
    surprise_snn_bridge_t* bridge);

/* ============================================================================
 * Operations API
 * ============================================================================ */

/**
 * @brief Encode surprise signal as spike train
 * @param bridge Bridge handle
 * @param surprise_level Surprise magnitude [0-1]
 * @param channel Which surprise channel to encode on
 * @return 0 on success, error code otherwise
 */
int surprise_snn_encode_surprise(
    surprise_snn_bridge_t* bridge,
    float surprise_level,
    surprise_snn_channel_t channel);

/**
 * @brief Simulate one timestep of the SNN
 * @param bridge Bridge handle
 * @return 0 on success, error code otherwise
 */
int surprise_snn_simulate_step(surprise_snn_bridge_t* bridge);

/**
 * @brief Decode SNN output into surprise effects
 * @param bridge Bridge handle
 * @param effects_out Output effects
 * @return 0 on success, error code otherwise
 */
int surprise_snn_decode_output(
    surprise_snn_bridge_t* bridge,
    surprise_snn_effects_t* effects_out);

/**
 * @brief Get the currently dominant surprise channel
 * @return Dominant channel, SURPRISE_SNN_CHANNEL_PE on error
 */
surprise_snn_channel_t surprise_snn_get_dominant_channel(
    const surprise_snn_bridge_t* bridge);

/** @brief Periodic update */
int surprise_snn_bridge_update(
    surprise_snn_bridge_t* bridge,
    float dt_seconds);

/* ============================================================================
 * Query API
 * ============================================================================ */

/** @brief Get current effects */
int surprise_snn_bridge_get_effects(
    const surprise_snn_bridge_t* bridge,
    surprise_snn_effects_t* effects_out);

/** @brief Get accumulated statistics */
int surprise_snn_bridge_get_stats(
    const surprise_snn_bridge_t* bridge,
    surprise_snn_stats_t* stats_out);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/** @brief Set health agent for heartbeat monitoring */
int surprise_snn_bridge_set_health_agent(
    surprise_snn_bridge_t* bridge,
    struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURPRISE_SNN_BRIDGE_H */

/**
 * @file nimcp_vae_snn_bridge.h
 * @brief Bridge between VAE and Spiking Neural Networks for Latent-Spike Conversion
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integrates VAE latent space with SNN spike encoding/decoding
 *
 * WHY:  VAE + SNN enables:
 *       - Continuous latent variables encoded as spike trains
 *       - Spike trains decoded back to latent representations
 *       - Precision (1/variance) mapped to spike timing precision
 *       - Imagination via VAE sampling decoded to neural activity
 *       - Efficient compression of spike data for storage/transmission
 *
 * HOW:  Bridge performs bidirectional conversion:
 *       - Encode: Spike trains → VAE encoder → latent (mu, sigma)
 *       - Decode: latent (z) → VAE decoder → spike patterns
 *       - Precision: VAE variance → spike timing jitter
 *       - Population: VAE dimensions → neuron populations
 *
 * SPIKE ENCODING METHODS:
 * =======================
 * - Rate coding: latent value → firing rate
 * - Temporal coding: latent value → first spike timing
 * - Population coding: latent vector → population activity pattern
 * - Phase coding: latent value → spike phase relative to oscillation
 *
 * BIO_MODULE: 0x1F1B (VAE-SNN Bridge)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VAE_SNN_BRIDGE_H
#define NIMCP_VAE_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/vae/nimcp_vae.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define VAE_SNN_BRIDGE_VERSION        "1.0.0"
#define BIO_MODULE_VAE_SNN_BRIDGE     0x1F1B

/** Maximum neurons per population */
#define VAE_SNN_MAX_NEURONS           4096

/** Maximum populations */
#define VAE_SNN_MAX_POPULATIONS       64

/** Maximum time window (ms) */
#define VAE_SNN_MAX_WINDOW_MS         1000

/** Default encoding window (ms) */
#define VAE_SNN_DEFAULT_WINDOW_MS     100

/** Error code range (32520-32529) */
#define NIMCP_ERROR_VAE_SNN_BASE          32520
#define NIMCP_ERROR_VAE_SNN_NULL          32521
#define NIMCP_ERROR_VAE_SNN_NOT_CONNECTED 32522
#define NIMCP_ERROR_VAE_SNN_ENCODE_FAILED 32523
#define NIMCP_ERROR_VAE_SNN_DECODE_FAILED 32524
#define NIMCP_ERROR_VAE_SNN_DIM_MISMATCH  32525
#define NIMCP_ERROR_VAE_SNN_NO_MEMORY     32526

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Spike encoding method for latent → spikes
 */
typedef enum {
    VAE_SNN_ENCODE_RATE = 0,       /**< Value → firing rate */
    VAE_SNN_ENCODE_TEMPORAL,        /**< Value → first spike time */
    VAE_SNN_ENCODE_POPULATION,      /**< Value → population pattern */
    VAE_SNN_ENCODE_PHASE,           /**< Value → spike phase */
    VAE_SNN_ENCODE_BURST,           /**< Value → burst count */
    VAE_SNN_ENCODE_POISSON,         /**< Value → Poisson rate */
    VAE_SNN_ENCODE_RANK_ORDER       /**< Values → spike order */
} vae_snn_encode_method_t;

/**
 * @brief Spike decoding method for spikes → latent
 */
typedef enum {
    VAE_SNN_DECODE_RATE = 0,       /**< Spike count / window */
    VAE_SNN_DECODE_FIRST_SPIKE,    /**< Time to first spike */
    VAE_SNN_DECODE_POPULATION,      /**< Population vector readout */
    VAE_SNN_DECODE_MEMBRANE,        /**< Final membrane potential */
    VAE_SNN_DECODE_WEIGHTED_SUM     /**< Weighted spike count */
} vae_snn_decode_method_t;

/**
 * @brief Precision mapping strategy
 */
typedef enum {
    VAE_SNN_PREC_TIMING_JITTER = 0, /**< Precision → spike timing precision */
    VAE_SNN_PREC_RATE_VARIANCE,      /**< Precision → firing rate variance */
    VAE_SNN_PREC_POPULATION_WIDTH,   /**< Precision → population tuning width */
    VAE_SNN_PREC_BURST_REGULARITY    /**< Precision → burst timing regularity */
} vae_snn_precision_map_t;

/**
 * @brief Bridge state
 */
typedef enum {
    VAE_SNN_STATE_DISCONNECTED = 0,
    VAE_SNN_STATE_CONNECTED,
    VAE_SNN_STATE_ENCODING,
    VAE_SNN_STATE_DECODING,
    VAE_SNN_STATE_ERROR
} vae_snn_bridge_state_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Population configuration for latent dimension mapping
 */
typedef struct {
    uint32_t population_id;
    uint32_t num_neurons;
    uint32_t latent_dim_start;    /**< First latent dimension mapped */
    uint32_t latent_dim_count;    /**< Number of latent dims mapped */
    float tuning_width;           /**< Population coding tuning width */
    float max_rate_hz;            /**< Maximum firing rate */
} vae_snn_population_config_t;

/**
 * @brief Rate coding configuration
 */
typedef struct {
    float min_rate_hz;            /**< Minimum firing rate */
    float max_rate_hz;            /**< Maximum firing rate */
    float latent_min;             /**< Latent value for min rate */
    float latent_max;             /**< Latent value for max rate */
    bool use_softplus;            /**< Softplus activation for rate */
} vae_snn_rate_config_t;

/**
 * @brief Temporal coding configuration
 */
typedef struct {
    float window_ms;              /**< Encoding window duration */
    float min_latency_ms;         /**< Minimum spike latency */
    float max_latency_ms;         /**< Maximum spike latency */
    bool inverse_latency;         /**< Higher value → shorter latency */
} vae_snn_temporal_config_t;

/**
 * @brief Main bridge configuration
 */
typedef struct {
    vae_snn_encode_method_t encode_method;
    vae_snn_decode_method_t decode_method;
    vae_snn_precision_map_t precision_map;

    /* Encoding parameters */
    vae_snn_rate_config_t rate_config;
    vae_snn_temporal_config_t temporal_config;

    /* Population parameters */
    uint32_t neurons_per_latent_dim;
    float population_overlap;     /**< Overlap between adjacent populations */

    /* Time window */
    float encoding_window_ms;
    float decoding_window_ms;
    float dt_ms;                  /**< Simulation time step */

    /* Precision parameters */
    float precision_to_jitter_scale;
    float min_timing_jitter_ms;
    float max_timing_jitter_ms;

    /* Options */
    bool enable_adaptation;       /**< Spike rate adaptation */
    bool enable_noise;            /**< Stochastic spike generation */
    float noise_level;

    bool enable_logging;
} vae_snn_bridge_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Spike train representation
 */
typedef struct {
    uint32_t* spike_times_ms;     /**< Spike times in ms (sorted) */
    uint32_t num_spikes;
    uint32_t neuron_id;
    float window_ms;
} vae_snn_spike_train_t;

/**
 * @brief Result of encoding latent to spikes
 */
typedef struct {
    vae_snn_spike_train_t* spike_trains;  /**< Per-neuron spike trains */
    uint32_t num_neurons;
    float window_ms;
    float total_spike_count;
    float avg_firing_rate_hz;
    float encoding_time_us;
} vae_snn_encode_result_t;

/**
 * @brief Result of decoding spikes to latent
 */
typedef struct {
    float* latent_mu;             /**< Decoded latent mean */
    float* latent_log_var;        /**< Decoded latent log variance */
    uint32_t latent_dim;
    float decoding_confidence;
    float reconstruction_error;
    float decoding_time_us;
} vae_snn_decode_result_t;

/**
 * @brief Bidirectional conversion result
 */
typedef struct {
    vae_snn_encode_result_t encode;
    vae_snn_decode_result_t decode;
    float round_trip_error;       /**< Latent → spikes → latent error */
} vae_snn_roundtrip_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t total_encodes;
    uint64_t total_decodes;
    uint64_t total_spikes_generated;
    uint64_t total_spikes_processed;
    float avg_firing_rate_hz;
    float avg_encoding_time_us;
    float avg_decoding_time_us;
    float avg_reconstruction_error;
    uint64_t creation_time_us;
    uint64_t last_operation_us;
} vae_snn_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

typedef struct vae_snn_bridge {
    vae_snn_bridge_config_t config;
    vae_system_t* vae;
    void* snn_network;            /**< SNN network context */
    vae_snn_bridge_state_t state;
    bool is_initialized;

    /* Population mapping */
    vae_snn_population_config_t* populations;
    uint32_t num_populations;

    /* Working buffers */
    float* encode_buffer;
    float* decode_buffer;
    uint32_t* spike_buffer;

    /* Precision state */
    float* current_precision;     /**< Per-dimension precision */
    float* timing_jitter;         /**< Per-neuron timing jitter */

    /* Adaptation state */
    float* adaptation_state;      /**< Per-neuron adaptation */

    /* Statistics */
    vae_snn_bridge_stats_t stats;
    uint64_t creation_time_us;
} vae_snn_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int vae_snn_bridge_default_config(vae_snn_bridge_config_t* config);
vae_snn_bridge_t* vae_snn_bridge_create(const vae_snn_bridge_config_t* config);
void vae_snn_bridge_destroy(vae_snn_bridge_t* bridge);
int vae_snn_bridge_connect_vae(vae_snn_bridge_t* bridge, vae_system_t* vae);
int vae_snn_bridge_connect_snn(vae_snn_bridge_t* bridge, void* snn_network);
int vae_snn_bridge_disconnect(vae_snn_bridge_t* bridge);
bool vae_snn_bridge_is_connected(const vae_snn_bridge_t* bridge);

/* ============================================================================
 * Population Configuration API
 * ============================================================================ */

int vae_snn_add_population(vae_snn_bridge_t* bridge,
                            const vae_snn_population_config_t* pop_config);

int vae_snn_configure_mapping(vae_snn_bridge_t* bridge,
                               uint32_t latent_dim,
                               uint32_t neurons_per_dim);

int vae_snn_auto_configure(vae_snn_bridge_t* bridge);

/* ============================================================================
 * Encoding API (Latent → Spikes)
 * ============================================================================ */

int vae_snn_encode_latent(vae_snn_bridge_t* bridge,
                           const float* latent, uint32_t latent_dim,
                           float window_ms,
                           vae_snn_encode_result_t* result);

int vae_snn_encode_with_precision(vae_snn_bridge_t* bridge,
                                   const float* latent_mu,
                                   const float* latent_var,
                                   uint32_t latent_dim,
                                   float window_ms,
                                   vae_snn_encode_result_t* result);

int vae_snn_encode_from_input(vae_snn_bridge_t* bridge,
                               const float* input, uint32_t input_dim,
                               float window_ms,
                               vae_snn_encode_result_t* result);

/* ============================================================================
 * Decoding API (Spikes → Latent)
 * ============================================================================ */

int vae_snn_decode_spikes(vae_snn_bridge_t* bridge,
                           const vae_snn_spike_train_t* spike_trains,
                           uint32_t num_neurons,
                           vae_snn_decode_result_t* result);

int vae_snn_decode_to_output(vae_snn_bridge_t* bridge,
                              const vae_snn_spike_train_t* spike_trains,
                              uint32_t num_neurons,
                              float* output, uint32_t* output_dim);

/* ============================================================================
 * Roundtrip API
 * ============================================================================ */

int vae_snn_roundtrip(vae_snn_bridge_t* bridge,
                       const float* latent, uint32_t latent_dim,
                       float window_ms,
                       vae_snn_roundtrip_result_t* result);

float vae_snn_compute_reconstruction_error(const vae_snn_bridge_t* bridge,
                                            const float* original,
                                            const float* reconstructed,
                                            uint32_t dim);

/* ============================================================================
 * Precision API
 * ============================================================================ */

int vae_snn_set_precision(vae_snn_bridge_t* bridge,
                           const float* precision, uint32_t dim);

int vae_snn_update_precision_from_vae(vae_snn_bridge_t* bridge);

float vae_snn_precision_to_jitter(const vae_snn_bridge_t* bridge,
                                   float precision);

/* ============================================================================
 * Query API
 * ============================================================================ */

vae_snn_bridge_state_t vae_snn_bridge_get_state(const vae_snn_bridge_t* bridge);
int vae_snn_bridge_get_stats(const vae_snn_bridge_t* bridge,
                              vae_snn_bridge_stats_t* stats);
uint32_t vae_snn_get_total_neurons(const vae_snn_bridge_t* bridge);
const char* vae_snn_encode_method_to_string(vae_snn_encode_method_t method);
const char* vae_snn_decode_method_to_string(vae_snn_decode_method_t method);

/* ============================================================================
 * Result Management
 * ============================================================================ */

void vae_snn_encode_result_free(vae_snn_encode_result_t* result);
void vae_snn_decode_result_free(vae_snn_decode_result_t* result);
void vae_snn_roundtrip_result_free(vae_snn_roundtrip_result_t* result);
void vae_snn_spike_train_free(vae_snn_spike_train_t* train);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VAE_SNN_BRIDGE_H */

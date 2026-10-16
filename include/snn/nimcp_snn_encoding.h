/**
 * @file nimcp_snn_encoding.h
 * @brief SNN Spike Encoding and Decoding Module
 *
 * WHAT: Conversions between continuous values and spike trains
 * WHY:  SNNs operate on discrete spikes, not continuous values
 * HOW:  Rate, temporal, population, and latency coding schemes
 *
 * INTEGRATION:
 * - Uses existing nimcp_tensor_t for I/O
 * - Integrates with snn_population_t for population coding
 * - Bio-async enabled for inter-module messaging
 *
 * BIOLOGICAL BASIS:
 * - Rate coding: Firing rate encodes stimulus intensity (Adrian 1928)
 * - Temporal coding: Spike timing encodes information (Thorpe 2001)
 * - Population coding: Distributed representation across neurons
 * - Latency coding: Time-to-first-spike for rapid processing
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_ENCODING_H
#define NIMCP_SNN_ENCODING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Encoder Configuration
//=============================================================================

/**
 * @brief Rate encoder configuration
 *
 * WHAT: Settings for rate-based spike encoding
 * WHY:  Convert continuous values to firing rates
 * HOW:  Value maps to Poisson spike rate
 */
typedef struct snn_rate_encoder_config_s {
    float max_rate;             /**< Maximum firing rate (Hz) */
    float min_rate;             /**< Minimum firing rate (Hz) */
    float value_min;            /**< Input value lower bound */
    float value_max;            /**< Input value upper bound */
    bool use_poisson;           /**< Use Poisson process (stochastic) */
} snn_rate_encoder_config_t;

/**
 * @brief Temporal encoder configuration
 *
 * WHAT: Settings for temporal spike encoding
 * WHY:  Encode values in spike timing
 * HOW:  Higher values = earlier spikes
 */
typedef struct snn_temporal_encoder_config_s {
    float t_min;                /**< Minimum spike time (ms) */
    float t_max;                /**< Maximum spike time (ms) */
    float value_min;            /**< Input value lower bound */
    float value_max;            /**< Input value upper bound */
    bool inverse;               /**< Invert: high value = late spike */
} snn_temporal_encoder_config_t;

/**
 * @brief Population encoder configuration
 *
 * WHAT: Settings for population-based spike encoding
 * WHY:  Distributed representation across neuron populations
 * HOW:  Gaussian receptive fields across value range
 */
typedef struct snn_population_encoder_config_s {
    uint32_t n_neurons;         /**< Neurons per value dimension */
    float sigma;                /**< Receptive field width (std dev) */
    float value_min;            /**< Input value lower bound */
    float value_max;            /**< Input value upper bound */
    bool normalize_rates;       /**< Normalize firing rates */
} snn_population_encoder_config_t;

/**
 * @brief Latency encoder configuration
 *
 * WHAT: Settings for latency-based spike encoding
 * WHY:  Time-to-first-spike carries information
 * HOW:  Stronger input = faster spike
 */
typedef struct snn_latency_encoder_config_s {
    float tau;                  /**< Time constant (ms) */
    float threshold;            /**< Minimum activation threshold */
    float t_max;                /**< Maximum spike latency (ms) */
    bool use_log;               /**< Use logarithmic mapping */
} snn_latency_encoder_config_t;

/**
 * @brief Burst encoder configuration
 *
 * WHAT: Settings for burst-based spike encoding
 * WHY:  Burst count encodes intensity
 * HOW:  Higher value = more spikes in burst
 */
typedef struct snn_burst_encoder_config_s {
    uint32_t max_spikes;        /**< Maximum spikes per burst */
    float burst_isi;            /**< Inter-spike interval in burst (ms) */
    float value_min;            /**< Input value lower bound */
    float value_max;            /**< Input value upper bound */
} snn_burst_encoder_config_t;

/**
 * @brief Phase encoder configuration
 *
 * WHAT: Settings for phase-based spike encoding
 * WHY:  Spike phase relative to oscillation carries info
 * HOW:  Value maps to phase of background oscillation
 */
typedef struct snn_phase_encoder_config_s {
    float frequency;            /**< Oscillation frequency (Hz) */
    float phase_range;          /**< Phase encoding range (radians) */
    float value_min;            /**< Input value lower bound */
    float value_max;            /**< Input value upper bound */
} snn_phase_encoder_config_t;

//=============================================================================
// Decoder Configuration
//=============================================================================

/**
 * @brief Rate decoder configuration
 *
 * WHAT: Settings for rate-based spike decoding
 * WHY:  Convert spike counts to continuous values
 * HOW:  Count spikes in time window, normalize
 */
typedef struct snn_rate_decoder_config_s {
    float time_window;          /**< Integration window (ms) */
    float max_rate;             /**< Expected maximum rate (Hz) */
    bool use_exponential;       /**< Exponential decay weighting */
    float decay_tau;            /**< Decay time constant (ms) */
} snn_rate_decoder_config_t;

/**
 * @brief First-spike decoder configuration
 *
 * WHAT: Settings for first-spike decoding
 * WHY:  Winner-take-all based on earliest spike
 * HOW:  Neuron with first spike = output class
 */
typedef struct snn_first_spike_decoder_config_s {
    float max_latency;          /**< Maximum wait time (ms) */
    bool use_softmax;           /**< Convert latencies to softmax */
    float temperature;          /**< Softmax temperature */
} snn_first_spike_decoder_config_t;

/**
 * @brief Population decoder configuration
 *
 * WHAT: Settings for population vector decoding
 * WHY:  Weighted sum of population activity
 * HOW:  Population vector reconstruction
 */
typedef struct snn_population_decoder_config_s {
    float time_window;          /**< Integration window (ms) */
    bool normalize;             /**< Normalize by population size */
    float* preferred_values;    /**< Preferred value per neuron [n_neurons] */
    uint32_t n_neurons;         /**< Number of neurons in population */
} snn_population_decoder_config_t;

//=============================================================================
// Encoder/Decoder Structures
//=============================================================================

/**
 * @brief Spike encoder structure
 *
 * WHAT: Encapsulates encoding method and configuration
 * WHY:  Uniform interface for all encoding types
 * HOW:  Method enum + union of configs
 */
struct snn_encoder_s {
    snn_encoding_t method;          /**< Encoding method */
    uint32_t n_inputs;              /**< Number of input values */
    uint32_t n_outputs;             /**< Number of output neurons */

    union {
        snn_rate_encoder_config_t rate;
        snn_temporal_encoder_config_t temporal;
        snn_population_encoder_config_t population;
        snn_latency_encoder_config_t latency;
        snn_burst_encoder_config_t burst;
        snn_phase_encoder_config_t phase;
    } config;

    /* Working buffers */
    float* spike_times;             /**< Computed spike times [n_outputs] */
    uint8_t* spike_mask;            /**< Binary spike mask [n_outputs] */

    /* Statistics */
    uint64_t total_spikes;          /**< Total spikes generated */
    uint64_t encode_count;          /**< Number of encode calls */
};

/**
 * @brief Spike decoder structure
 *
 * WHAT: Encapsulates decoding method and configuration
 * WHY:  Uniform interface for all decoding types
 * HOW:  Method enum + union of configs
 */
struct snn_decoder_s {
    snn_decoding_t method;          /**< Decoding method */
    uint32_t n_inputs;              /**< Number of input neurons */
    uint32_t n_outputs;             /**< Number of output values */

    union {
        snn_rate_decoder_config_t rate;
        snn_first_spike_decoder_config_t first_spike;
        snn_population_decoder_config_t population;
    } config;

    /* Working buffers */
    float* spike_counts;            /**< Spike counts per neuron [n_inputs] */
    float* first_times;             /**< First spike times [n_inputs] */

    /* Statistics */
    uint64_t total_outputs;         /**< Total decode outputs */
    uint64_t decode_count;          /**< Number of decode calls */
};

//=============================================================================
// Encoder Lifecycle Functions
//=============================================================================

/**
 * @brief Create a rate encoder
 *
 * WHAT: Create encoder for rate coding
 * WHY:  Most common encoding method
 * HOW:  Allocate and configure rate encoder
 *
 * @param n_inputs Number of input values
 * @param config Rate encoder configuration
 * @return Encoder pointer or NULL on failure
 */
snn_encoder_t* snn_encoder_create_rate(uint32_t n_inputs,
                                        const snn_rate_encoder_config_t* config);

/**
 * @brief Create a temporal encoder
 *
 * WHAT: Create encoder for temporal coding
 * WHY:  Fast, precise timing-based encoding
 * HOW:  Allocate and configure temporal encoder
 *
 * @param n_inputs Number of input values
 * @param config Temporal encoder configuration
 * @return Encoder pointer or NULL on failure
 */
snn_encoder_t* snn_encoder_create_temporal(uint32_t n_inputs,
                                            const snn_temporal_encoder_config_t* config);

/**
 * @brief Create a population encoder
 *
 * WHAT: Create encoder for population coding
 * WHY:  Robust distributed representation
 * HOW:  Allocate and configure population encoder
 *
 * @param n_inputs Number of input values
 * @param config Population encoder configuration
 * @return Encoder pointer or NULL on failure
 */
snn_encoder_t* snn_encoder_create_population(uint32_t n_inputs,
                                              const snn_population_encoder_config_t* config);

/**
 * @brief Create a latency encoder
 *
 * WHAT: Create encoder for latency coding
 * WHY:  Time-to-first-spike encoding
 * HOW:  Allocate and configure latency encoder
 *
 * @param n_inputs Number of input values
 * @param config Latency encoder configuration
 * @return Encoder pointer or NULL on failure
 */
snn_encoder_t* snn_encoder_create_latency(uint32_t n_inputs,
                                           const snn_latency_encoder_config_t* config);

/**
 * @brief Destroy an encoder
 *
 * WHAT: Free encoder resources
 * WHY:  Proper cleanup
 * HOW:  Free all allocated memory
 *
 * @param encoder Encoder to destroy
 */
void snn_encoder_destroy(snn_encoder_t* encoder);

//=============================================================================
// Decoder Lifecycle Functions
//=============================================================================

/**
 * @brief Create a rate decoder
 *
 * WHAT: Create decoder for rate decoding
 * WHY:  Standard spike count to value conversion
 * HOW:  Allocate and configure rate decoder
 *
 * @param n_inputs Number of input neurons
 * @param n_outputs Number of output values
 * @param config Rate decoder configuration
 * @return Decoder pointer or NULL on failure
 */
snn_decoder_t* snn_decoder_create_rate(uint32_t n_inputs,
                                        uint32_t n_outputs,
                                        const snn_rate_decoder_config_t* config);

/**
 * @brief Create a first-spike decoder
 *
 * WHAT: Create decoder for first-spike decoding
 * WHY:  Winner-take-all classification
 * HOW:  Allocate and configure first-spike decoder
 *
 * @param n_inputs Number of input neurons
 * @param config First-spike decoder configuration
 * @return Decoder pointer or NULL on failure
 */
snn_decoder_t* snn_decoder_create_first_spike(uint32_t n_inputs,
                                               const snn_first_spike_decoder_config_t* config);

/**
 * @brief Create a population decoder
 *
 * WHAT: Create decoder for population vector decoding
 * WHY:  Reconstruct continuous values from population
 * HOW:  Allocate and configure population decoder
 *
 * @param n_inputs Number of input neurons
 * @param n_outputs Number of output values
 * @param config Population decoder configuration
 * @return Decoder pointer or NULL on failure
 */
snn_decoder_t* snn_decoder_create_population(uint32_t n_inputs,
                                              uint32_t n_outputs,
                                              const snn_population_decoder_config_t* config);

/**
 * @brief Destroy a decoder
 *
 * WHAT: Free decoder resources
 * WHY:  Proper cleanup
 * HOW:  Free all allocated memory
 *
 * @param decoder Decoder to destroy
 */
void snn_decoder_destroy(snn_decoder_t* decoder);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode values to spikes using rate coding
 *
 * WHAT: Generate spike train from continuous values
 * WHY:  Convert sensor input to spike format
 * HOW:  Poisson process with rate proportional to value
 *
 * @param encoder Rate encoder
 * @param values Input values [n_inputs]
 * @param dt Simulation timestep (ms)
 * @param spikes_out Output spike mask [n_outputs]
 * @return SNN_SUCCESS or error code
 */
int snn_encode_rate(snn_encoder_t* encoder,
                    const float* values,
                    float dt,
                    uint8_t* spikes_out);

/**
 * @brief Encode values to spike times using temporal coding
 *
 * WHAT: Generate spike times from continuous values
 * WHY:  Precise timing-based representation
 * HOW:  Value maps to spike time in encoding window
 *
 * @param encoder Temporal encoder
 * @param values Input values [n_inputs]
 * @param spike_times_out Output spike times [n_outputs]
 * @return SNN_SUCCESS or error code
 */
int snn_encode_temporal(snn_encoder_t* encoder,
                        const float* values,
                        float* spike_times_out);

/**
 * @brief Encode values using population coding
 *
 * WHAT: Generate population activity from values
 * WHY:  Robust distributed representation
 * HOW:  Gaussian receptive fields across value range
 *
 * @param encoder Population encoder
 * @param values Input values [n_inputs]
 * @param rates_out Output firing rates [n_outputs]
 * @return SNN_SUCCESS or error code
 */
int snn_encode_population(snn_encoder_t* encoder,
                          const float* values,
                          float* rates_out);

/**
 * @brief Encode values to latencies
 *
 * WHAT: Generate time-to-first-spike from values
 * WHY:  Fast encoding with single spike
 * HOW:  Higher value = shorter latency
 *
 * @param encoder Latency encoder
 * @param values Input values [n_inputs]
 * @param latencies_out Output latencies [n_outputs]
 * @return SNN_SUCCESS or error code
 */
int snn_encode_latency(snn_encoder_t* encoder,
                       const float* values,
                       float* latencies_out);

/**
 * @brief Generic encode function
 *
 * WHAT: Encode using encoder's configured method
 * WHY:  Uniform interface for all encoding types
 * HOW:  Dispatch to appropriate encode function
 *
 * @param encoder Encoder of any type
 * @param values Input values [n_inputs]
 * @param dt Simulation timestep (ms)
 * @param output Output buffer (interpretation depends on method)
 * @return SNN_SUCCESS or error code
 */
int snn_encode(snn_encoder_t* encoder,
               const float* values,
               float dt,
               void* output);

//=============================================================================
// Decoding Functions
//=============================================================================

/**
 * @brief Decode spikes using rate decoding
 *
 * WHAT: Convert spike counts to continuous values
 * WHY:  Reconstruct output from spike activity
 * HOW:  Count spikes in window, normalize by max rate
 *
 * @param decoder Rate decoder
 * @param spike_counts Spike counts [n_inputs]
 * @param values_out Output values [n_outputs]
 * @return SNN_SUCCESS or error code
 */
int snn_decode_rate(snn_decoder_t* decoder,
                    const float* spike_counts,
                    float* values_out);

/**
 * @brief Decode using first-spike timing
 *
 * WHAT: Winner-take-all classification from spike times
 * WHY:  Rapid classification from first responses
 * HOW:  Neuron with earliest spike wins
 *
 * @param decoder First-spike decoder
 * @param spike_times First spike times [n_inputs]
 * @param class_out Output class (winner index)
 * @param confidence_out Optional output confidence
 * @return SNN_SUCCESS or error code
 */
int snn_decode_first_spike(snn_decoder_t* decoder,
                           const float* spike_times,
                           uint32_t* class_out,
                           float* confidence_out);

/**
 * @brief Decode using population vector
 *
 * WHAT: Reconstruct value from population activity
 * WHY:  Precise continuous output
 * HOW:  Weighted sum of preferred values
 *
 * @param decoder Population decoder
 * @param activities Neuron activities [n_inputs]
 * @param values_out Output values [n_outputs]
 * @return SNN_SUCCESS or error code
 */
int snn_decode_population(snn_decoder_t* decoder,
                          const float* activities,
                          float* values_out);

/**
 * @brief Generic decode function
 *
 * WHAT: Decode using decoder's configured method
 * WHY:  Uniform interface for all decoding types
 * HOW:  Dispatch to appropriate decode function
 *
 * @param decoder Decoder of any type
 * @param input Input buffer (interpretation depends on method)
 * @param output Output buffer (interpretation depends on method)
 * @return SNN_SUCCESS or error code
 */
int snn_decode(snn_decoder_t* decoder,
               const void* input,
               void* output);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Initialize rate encoder with default config
 *
 * WHAT: Set default rate encoder parameters
 * WHY:  Convenient initialization
 * HOW:  Fill config with sensible defaults
 *
 * @param config Config to initialize
 */
void snn_rate_encoder_config_default(snn_rate_encoder_config_t* config);

/**
 * @brief Initialize temporal encoder with default config
 *
 * WHAT: Set default temporal encoder parameters
 * WHY:  Convenient initialization
 * HOW:  Fill config with sensible defaults
 *
 * @param config Config to initialize
 */
void snn_temporal_encoder_config_default(snn_temporal_encoder_config_t* config);

/**
 * @brief Initialize population encoder with default config
 *
 * WHAT: Set default population encoder parameters
 * WHY:  Convenient initialization
 * HOW:  Fill config with sensible defaults
 *
 * @param config Config to initialize
 */
void snn_population_encoder_config_default(snn_population_encoder_config_t* config);

/**
 * @brief Initialize rate decoder with default config
 *
 * WHAT: Set default rate decoder parameters
 * WHY:  Convenient initialization
 * HOW:  Fill config with sensible defaults
 *
 * @param config Config to initialize
 */
void snn_rate_decoder_config_default(snn_rate_decoder_config_t* config);

/**
 * @brief Get encoder statistics
 *
 * WHAT: Retrieve encoding statistics
 * WHY:  Monitoring and debugging
 * HOW:  Return total spikes and call count
 *
 * @param encoder Encoder to query
 * @param total_spikes Optional output for total spikes
 * @param encode_count Optional output for encode count
 */
void snn_encoder_get_stats(const snn_encoder_t* encoder,
                           uint64_t* total_spikes,
                           uint64_t* encode_count);

/**
 * @brief Get decoder statistics
 *
 * WHAT: Retrieve decoding statistics
 * WHY:  Monitoring and debugging
 * HOW:  Return total outputs and call count
 *
 * @param decoder Decoder to query
 * @param total_outputs Optional output for total outputs
 * @param decode_count Optional output for decode count
 */
void snn_decoder_get_stats(const snn_decoder_t* decoder,
                           uint64_t* total_outputs,
                           uint64_t* decode_count);

/**
 * @brief Reset encoder statistics
 *
 * WHAT: Clear encoder statistics
 * WHY:  Fresh start for monitoring
 * HOW:  Zero all counters
 *
 * @param encoder Encoder to reset
 */
void snn_encoder_reset_stats(snn_encoder_t* encoder);

/**
 * @brief Reset decoder statistics
 *
 * WHAT: Clear decoder statistics
 * WHY:  Fresh start for monitoring
 * HOW:  Zero all counters
 *
 * @param decoder Decoder to reset
 */
void snn_decoder_reset_stats(snn_decoder_t* decoder);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_ENCODING_H */

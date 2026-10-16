/**
 * @file nimcp_snn_attention_bridge.h
 * @brief SNN-Attention integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and multihead attention system
 * WHY:  Enable spike-based attention modulation and gamma synchronization
 * HOW:  Convert spike patterns to attention signals, gamma oscillations for focus
 *
 * BIOLOGICAL BASIS:
 * - Gamma oscillations (30-80 Hz) synchronize attended features
 * - Spike rate modulates attention strength (higher rate = more attention)
 * - Thalamic pulvinar gates attention through spike bursts
 * - Phase-locking to gamma provides temporal attention windows
 *
 * INTEGRATION:
 * - SNN → Attention: Spike rate modulates attention gate signal
 * - SNN → Attention: Gamma oscillations provide temporal focus
 * - Attention → SNN: High attention increases input population firing
 * - Attention → SNN: Attention weights modulate synaptic efficacy
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_ATTENTION_BRIDGE_H
#define NIMCP_SNN_ATTENTION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "plasticity/attention/nimcp_attention.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN-Attention bridge configuration
 *
 * WHAT: Parameters for SNN-attention integration
 * WHY:  Control bidirectional modulation between systems
 * HOW:  Thresholds, scaling factors, oscillation parameters
 */
typedef struct snn_attention_config_s {
    /* Spike-to-attention conversion */
    float spike_rate_min;           /**< Min spike rate for attention (Hz) */
    float spike_rate_max;           /**< Max spike rate for full attention (Hz) */
    float gate_scaling_factor;      /**< Scale spike rate to gate signal */

    /* Gamma oscillation parameters */
    bool enable_gamma_sync;         /**< Enable gamma synchronization */
    float gamma_frequency;          /**< Center frequency (Hz, default: 40) */
    float gamma_bandwidth;          /**< Bandwidth (Hz, default: 10) */
    float gamma_phase_threshold;    /**< Phase coherence threshold */

    /* Attention-to-spike modulation */
    float attention_boost_factor;   /**< Boost input population by attention */
    float salience_spike_scaling;   /**< Scale salience to spike input */
    bool modulate_synaptic_weights; /**< Allow attention to modulate weights */
    float weight_modulation_gain;   /**< Gain for weight modulation */

    /* Population mapping */
    uint32_t attention_population_id; /**< SNN population for attention control */
    uint32_t input_population_id;     /**< SNN population receiving attention */

    /* Update timing */
    float update_interval_ms;       /**< How often to sync (default: 25ms) */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_attention_config_t;

/**
 * @brief Gamma oscillation state
 *
 * WHAT: State of gamma oscillations for attention synchronization
 * WHY:  Track oscillation phase for temporal attention windows
 * HOW:  Phase, amplitude, coherence metrics
 */
typedef struct snn_gamma_state_s {
    float phase;                    /**< Current phase [0, 2π] */
    float amplitude;                /**< Current amplitude [0, 1] */
    float frequency;                /**< Instantaneous frequency (Hz) */
    float coherence;                /**< Phase coherence across neurons [0, 1] */
    bool is_synchronized;           /**< Whether population is synchronized */
    uint32_t burst_count;           /**< Number of gamma bursts detected */
} snn_gamma_state_t;

/**
 * @brief SNN-Attention bridge state
 *
 * WHAT: Current state of bidirectional integration
 * WHY:  Track computed signals and metrics
 * HOW:  Cached values updated on sync
 */
typedef struct snn_attention_state_s {
    /* Spike-derived signals */
    float current_spike_rate;       /**< Current attention population rate (Hz) */
    float attention_gate_signal;    /**< Computed gate signal [0, 1] */
    snn_gamma_state_t gamma;        /**< Gamma oscillation state */

    /* Attention-derived signals */
    float attention_strength;       /**< Attention system strength [0, 1] */
    float input_boost_current;      /**< Current boost to input population */

    /* Statistics */
    uint32_t sync_count;            /**< Number of syncs performed */
    float avg_gate_signal;          /**< Average gate signal */
    float avg_spike_rate;           /**< Average spike rate */
} snn_attention_state_t;

/**
 * @brief SNN-Attention bridge structure
 *
 * WHAT: Context for SNN-attention integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and cached state
 */
typedef struct snn_attention_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;                 /**< SNN network */
    multihead_attention_t* attention;   /**< Multihead attention system */
    snn_attention_config_t config;      /**< Bridge configuration */
    snn_attention_state_t state;        /**< Current state */

    /* Populations */
    snn_population_t* attention_pop;    /**< Population controlling attention */
    snn_population_t* input_pop;        /**< Population receiving attention */

    /* Timing */
    float last_update_time;             /**< Last update timestamp (ms) */

    /* Bio-async */
    bool bio_async_enabled;             /**< Bio-async connected */
    bio_module_context_t bio_ctx;       /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_attention_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize attention bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from attention neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_attention_config_default(snn_attention_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-attention bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable SNN-attention integration
 * HOW:  Allocate context, set up connections
 *
 * @param config Bridge configuration
 * @param snn SNN network
 * @param attention Multihead attention system
 * @return Bridge instance or NULL on failure
 */
snn_attention_bridge_t* snn_attention_bridge_create(
    const snn_attention_config_t* config,
    snn_network_t* snn,
    multihead_attention_t* attention
);

/**
 * @brief Destroy SNN-attention bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_attention_bridge_destroy(snn_attention_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed attention coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_attention_bridge_connect_bio_async(snn_attention_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_attention_bridge_disconnect_bio_async(snn_attention_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_attention_bridge_is_bio_async_connected(const snn_attention_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Process spike input through attention
 *
 * WHAT: Convert spike patterns to attention modulation
 * WHY:  SNN drives attention based on spike activity
 * HOW:  Compute spike rate, detect gamma, update gate
 *
 * @param bridge Bridge instance
 * @param input Input spike pattern (can be NULL for current state)
 * @param output Output attention-modulated signal
 * @return 0 on success, error code on failure
 */
int snn_attention_bridge_process(
    snn_attention_bridge_t* bridge,
    const float* input,
    float* output
);

/**
 * @brief Update bridge state
 *
 * WHAT: Synchronize SNN and attention systems
 * WHY:  Keep bidirectional signals current
 * HOW:  Update spike rate, gamma, attention signals
 *
 * @param bridge Bridge to update
 * @param dt Time step in milliseconds
 * @return 0 on success, error code on failure
 */
int snn_attention_bridge_update(snn_attention_bridge_t* bridge, float dt);

//=============================================================================
// Spike-to-Attention Functions
//=============================================================================

/**
 * @brief Compute attention gate from spike rate
 *
 * WHAT: Convert spike rate to thalamic gate signal
 * WHY:  Spike rate drives attention strength
 * HOW:  Scale and saturate to [0, 1]
 *
 * @param bridge Bridge with configuration
 * @param spike_rate Current spike rate (Hz)
 * @return Gate signal [0, 1]
 */
float snn_attention_compute_gate_signal(
    const snn_attention_bridge_t* bridge,
    float spike_rate
);

/**
 * @brief Detect gamma oscillation in spike train
 *
 * WHAT: Analyze spike patterns for gamma synchrony
 * WHY:  Gamma indicates focused attention
 * HOW:  FFT or autocorrelation of spike times
 *
 * @param bridge Bridge instance
 * @param population Population to analyze
 * @param gamma_state Output: gamma state
 * @return 0 on success, error code on failure
 */
int snn_attention_detect_gamma(
    snn_attention_bridge_t* bridge,
    snn_population_t* population,
    snn_gamma_state_t* gamma_state
);

/**
 * @brief Apply gamma synchronization to attention
 *
 * WHAT: Modulate attention based on gamma phase
 * WHY:  Gamma provides temporal attention windows
 * HOW:  Boost attention at gamma peaks
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int snn_attention_apply_gamma_sync(snn_attention_bridge_t* bridge);

//=============================================================================
// Attention-to-Spike Functions
//=============================================================================

/**
 * @brief Boost input population based on attention
 *
 * WHAT: Increase input population firing with attention
 * WHY:  Attention enhances neural responses
 * HOW:  Scale input current by attention strength
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int snn_attention_boost_input_population(snn_attention_bridge_t* bridge);

/**
 * @brief Modulate synaptic weights by attention
 *
 * WHAT: Adjust weights based on attention pattern
 * WHY:  Attention-gated plasticity
 * HOW:  Scale synaptic efficacy by attention weights
 *
 * @param bridge Bridge instance
 * @param attention_weights Attention weight matrix
 * @param seq_length Sequence length
 * @return 0 on success, error code on failure
 */
int snn_attention_modulate_weights(
    snn_attention_bridge_t* bridge,
    const float* attention_weights,
    uint32_t seq_length
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current bridge state
 *
 * @param bridge Bridge to query
 * @param state Output state (copied)
 * @return 0 on success
 */
int snn_attention_bridge_get_state(
    const snn_attention_bridge_t* bridge,
    snn_attention_state_t* state
);

/**
 * @brief Get gamma oscillation state
 *
 * @param bridge Bridge to query
 * @param gamma Output gamma state (copied)
 * @return 0 on success
 */
int snn_attention_get_gamma_state(
    const snn_attention_bridge_t* bridge,
    snn_gamma_state_t* gamma
);

/**
 * @brief Get current attention gate signal
 *
 * @param bridge Bridge to query
 * @return Gate signal [0, 1]
 */
float snn_attention_get_gate_signal(const snn_attention_bridge_t* bridge);

/**
 * @brief Check if gamma synchronized
 *
 * @param bridge Bridge to check
 * @return true if synchronized
 */
bool snn_attention_is_gamma_synchronized(const snn_attention_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param sync_count Output: total syncs
 * @param avg_gate Output: average gate signal
 * @param avg_spike_rate Output: average spike rate
 * @return 0 on success
 */
int snn_attention_get_stats(
    const snn_attention_bridge_t* bridge,
    uint32_t* sync_count,
    float* avg_gate,
    float* avg_spike_rate
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_attention_reset_stats(snn_attention_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_ATTENTION_BRIDGE_H */

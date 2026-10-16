/**
 * @file nimcp_attention_snn_bridge.h
 * @brief Attention System - SNN Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between multihead attention and SNN module
 * WHY:  Enable spike-based attention computation and bio-plausible gating
 * HOW:  Convert attention patterns to spikes, decode SNN output to attention weights
 *
 * THEORETICAL FOUNDATIONS:
 * - Desimone & Duncan (1995): Biased competition model of attention
 * - Reynolds & Heeger (2009): Normalization model of attention
 * - Itti & Koch (2001): Salience-based attention
 *
 * BIOLOGICAL BASIS:
 * - Attention modulates neural firing rates in visual cortex
 * - Pulvinar nucleus provides spike-timing-based gating
 * - Winner-take-all dynamics in frontal eye fields
 * - Spike synchrony encodes attentional focus
 *
 * INTEGRATION FLOWS:
 *
 * Attention System --> SNN:
 *   1. Attention weights encoded as input spike rates
 *   2. Salience values converted to population activity
 *   3. Gate signals modulate SNN excitability
 *   4. Top-k attention drives winner-take-all competition
 *
 * SNN --> Attention System:
 *   1. Population firing rates decoded to attention weights
 *   2. Spike synchrony indicates attentional focus strength
 *   3. Competition dynamics produce sparse attention
 *   4. Temporal dynamics track attention shifts
 *
 * BIO-ASYNC MESSAGES:
 * - ATTENTION_SNN_MSG_SPIKE_EVENT: Spike events from attention processing
 * - ATTENTION_SNN_MSG_FOCUS_UPDATE: Attention focus changes
 * - ATTENTION_SNN_MSG_GATE_UPDATE: Thalamic gate state changes
 * - ATTENTION_SNN_MSG_SALIENCE_CHANGE: Salience map updates
 *
 * @see nimcp_snn.h
 * @see nimcp_attention.h
 * @see nimcp_attention_plasticity_bridge.h
 */

#ifndef NIMCP_ATTENTION_SNN_BRIDGE_H
#define NIMCP_ATTENTION_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "snn/nimcp_snn.h"
#include "plasticity/attention/nimcp_attention.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum attention heads in SNN encoding */
#define ATTENTION_SNN_MAX_HEADS             16

/** @brief Default neurons per attention head population */
#define ATTENTION_SNN_NEURONS_PER_HEAD      32

/** @brief Default salience encoding dimension */
#define ATTENTION_SNN_SALIENCE_DIM          64

/** @brief Bio-async module ID for attention-SNN bridge */
#define BIO_MODULE_ATTENTION_SNN_BRIDGE     0x0C00

/** @brief Default simulation timestep (ms) */
#define ATTENTION_SNN_DEFAULT_DT            1.0f

/** @brief Default spike encoding window (ms) */
#define ATTENTION_SNN_ENCODING_WINDOW       50.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spike encoding method for attention patterns
 */
typedef enum {
    ATTENTION_SNN_ENCODE_RATE = 0,       /**< Rate coding (attention = rate) */
    ATTENTION_SNN_ENCODE_TEMPORAL,       /**< Temporal/latency coding */
    ATTENTION_SNN_ENCODE_POPULATION,     /**< Population vector coding */
    ATTENTION_SNN_ENCODE_WINNER_TAKE_ALL /**< Winner-take-all competition */
} attention_snn_encoding_t;

/**
 * @brief Decoding method for SNN output to attention weights
 */
typedef enum {
    ATTENTION_SNN_DECODE_RATE = 0,       /**< Rate-based decoding */
    ATTENTION_SNN_DECODE_SOFTMAX,        /**< Softmax normalization */
    ATTENTION_SNN_DECODE_COMPETITION,    /**< Competition-based (sparse) */
    ATTENTION_SNN_DECODE_SYNCHRONY       /**< Synchrony-based focus */
} attention_snn_decoding_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    ATTENTION_SNN_STATE_IDLE = 0,        /**< Ready for input */
    ATTENTION_SNN_STATE_ENCODING,        /**< Encoding attention input */
    ATTENTION_SNN_STATE_SIMULATING,      /**< Running SNN simulation */
    ATTENTION_SNN_STATE_DECODING,        /**< Decoding SNN output */
    ATTENTION_SNN_STATE_COMPETING,       /**< Competition phase */
    ATTENTION_SNN_STATE_DISABLED         /**< Bridge disabled */
} attention_snn_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Attention-SNN bridge configuration
 */
typedef struct {
    /* Network dimensions */
    uint32_t num_heads;                  /**< Number of attention heads */
    uint32_t neurons_per_head;           /**< Neurons per head population */
    uint32_t salience_dim;               /**< Salience encoding dimension */
    uint32_t sequence_length;            /**< Maximum sequence length */

    /* Encoding parameters */
    attention_snn_encoding_t encoding;   /**< Spike encoding method */
    float encoding_gain;                 /**< Attention-to-spike gain */
    float baseline_rate_hz;              /**< Baseline firing rate */
    float max_rate_hz;                   /**< Maximum firing rate */
    float salience_gain;                 /**< Salience modulation gain */

    /* Decoding parameters */
    attention_snn_decoding_t decoding;   /**< Output decoding method */
    float decoding_threshold;            /**< Competition threshold */
    float softmax_temperature;           /**< Softmax temperature */
    float temporal_smoothing;            /**< Temporal averaging alpha */

    /* Competition parameters */
    bool enable_competition;             /**< Enable winner-take-all */
    float inhibition_strength;           /**< Lateral inhibition strength */
    float competition_tau_ms;            /**< Competition time constant */
    uint32_t top_k;                      /**< Top-k items to attend */

    /* Gate integration */
    bool enable_gate_integration;        /**< Enable thalamic gate SNN */
    float gate_modulation_gain;          /**< Gate effect on attention */

    /* Simulation parameters */
    float dt_ms;                         /**< Simulation timestep */
    float simulation_window_ms;          /**< Simulation window per update */

    /* Integration */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    bool enable_plasticity_integration;  /**< Enable plasticity bridge */
    bool enable_immune_modulation;       /**< Enable immune system effects */
} attention_snn_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Current attention state from SNN
 */
typedef struct {
    float* attention_weights;            /**< Per-head attention weights [num_heads] */
    float* salience_map;                 /**< Decoded salience [sequence_length] */
    float focus_strength;                /**< Overall focus strength [0, 1] */
    float sparsity;                      /**< Attention sparsity [0, 1] */
    int32_t* top_k_indices;              /**< Top-k attended indices */
    float gate_activation;               /**< Current gate activation [0, 1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} attention_snn_attention_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    attention_snn_state_t state;         /**< Current operational state */
    attention_snn_attention_state_t attention; /**< Current attention state */
    uint32_t active_populations;         /**< Number of active populations */
    float avg_firing_rate;               /**< Average network firing rate */
    float competition_energy;            /**< Current competition energy */
    bool bio_async_connected;            /**< Bio-async connection status */
} attention_snn_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_forward_passes;       /**< Total forward passes */
    uint64_t total_spikes_generated;     /**< Total spikes generated */
    uint64_t total_decodings;            /**< Total decoding operations */
    uint64_t attention_shifts;           /**< Number of attention focus shifts */
    float avg_focus_strength;            /**< Average focus strength */
    float avg_sparsity;                  /**< Average attention sparsity */
    float avg_processing_time_ms;        /**< Average processing time */
    float competition_convergence_rate;  /**< Competition convergence rate */
} attention_snn_stats_t;

//=============================================================================
// Main Bridge Structure
//=============================================================================

/** @brief Forward declaration */
typedef struct attention_snn_bridge attention_snn_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default configuration values
 */
attention_snn_config_t attention_snn_config_default(void);

/**
 * @brief Create attention-SNN bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
attention_snn_bridge_t* attention_snn_create(const attention_snn_config_t* config);

/**
 * @brief Destroy attention-SNN bridge
 * @param bridge Bridge to destroy
 */
void attention_snn_destroy(attention_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int attention_snn_reset(attention_snn_bridge_t* bridge);

//=============================================================================
// Encoding Functions (Attention --> SNN)
//=============================================================================

/**
 * @brief Encode attention weights as spike patterns
 * @param bridge Bridge instance
 * @param attention_weights Per-head attention weights [num_heads]
 * @param num_heads Number of attention heads
 * @return Number of spikes generated, or -1 on failure
 */
int attention_snn_encode_weights(
    attention_snn_bridge_t* bridge,
    const float* attention_weights,
    uint32_t num_heads
);

/**
 * @brief Encode salience map as spike patterns
 * @param bridge Bridge instance
 * @param salience Salience values [sequence_length]
 * @param sequence_length Sequence length
 * @return Number of spikes generated, or -1 on failure
 */
int attention_snn_encode_salience(
    attention_snn_bridge_t* bridge,
    const float* salience,
    uint32_t sequence_length
);

/**
 * @brief Encode full attention state
 * @param bridge Bridge instance
 * @param mha Multihead attention system
 * @param input Input sequence for attention computation
 * @param sequence_length Sequence length
 * @return Number of spikes generated, or -1 on failure
 */
int attention_snn_encode_multihead(
    attention_snn_bridge_t* bridge,
    multihead_attention_t mha,
    const float* input,
    uint32_t sequence_length
);

/**
 * @brief Encode thalamic gate signal
 * @param bridge Bridge instance
 * @param gate_signal Gate activation [0, 1]
 * @return 0 on success, -1 on failure
 */
int attention_snn_encode_gate(
    attention_snn_bridge_t* bridge,
    float gate_signal
);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Run SNN simulation step
 * @param bridge Bridge instance
 * @param duration_ms Simulation duration in milliseconds
 * @return 0 on success, -1 on failure
 */
int attention_snn_simulate(attention_snn_bridge_t* bridge, float duration_ms);

/**
 * @brief Process single timestep
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int attention_snn_step(attention_snn_bridge_t* bridge);

/**
 * @brief Run competition phase for winner-take-all
 * @param bridge Bridge instance
 * @param duration_ms Competition duration
 * @return 0 on success, -1 on failure
 */
int attention_snn_compete(attention_snn_bridge_t* bridge, float duration_ms);

//=============================================================================
// Decoding Functions (SNN --> Attention)
//=============================================================================

/**
 * @brief Get decoded attention weights from SNN output
 * @param bridge Bridge instance
 * @param weights Output array for weights [num_heads]
 * @param num_heads Number of heads
 * @return 0 on success, -1 on failure
 */
int attention_snn_get_weights(
    attention_snn_bridge_t* bridge,
    float* weights,
    uint32_t num_heads
);

/**
 * @brief Get decoded salience map from SNN output
 * @param bridge Bridge instance
 * @param salience Output salience array [sequence_length]
 * @param sequence_length Sequence length
 * @return 0 on success, -1 on failure
 */
int attention_snn_get_salience(
    attention_snn_bridge_t* bridge,
    float* salience,
    uint32_t sequence_length
);

/**
 * @brief Get top-k attended indices
 * @param bridge Bridge instance
 * @param indices Output indices array [k]
 * @param k Number of top items
 * @return Actual number of items returned, or -1 on failure
 */
int attention_snn_get_top_k(
    attention_snn_bridge_t* bridge,
    int32_t* indices,
    uint32_t k
);

/**
 * @brief Get overall focus strength
 * @param bridge Bridge instance
 * @return Focus strength [0, 1], or -1.0f on error
 */
float attention_snn_get_focus_strength(attention_snn_bridge_t* bridge);

/**
 * @brief Get attention sparsity
 * @param bridge Bridge instance
 * @return Sparsity [0, 1], or -1.0f on error
 */
float attention_snn_get_sparsity(attention_snn_bridge_t* bridge);

/**
 * @brief Get complete decoded attention state
 * @param bridge Bridge instance
 * @param attention_state Output attention state
 * @return 0 on success, -1 on failure
 */
int attention_snn_get_attention_state(
    attention_snn_bridge_t* bridge,
    attention_snn_attention_state_t* attention_state
);

//=============================================================================
// State and Statistics
//=============================================================================

/**
 * @brief Get bridge state
 * @param bridge Bridge instance
 * @param state Output state
 * @return 0 on success, -1 on failure
 */
int attention_snn_get_state(
    const attention_snn_bridge_t* bridge,
    attention_snn_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int attention_snn_get_stats(
    const attention_snn_bridge_t* bridge,
    attention_snn_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge instance
 */
void attention_snn_reset_stats(attention_snn_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int attention_snn_connect_bio_async(attention_snn_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge instance
 * @return 0 on success, -1 on failure
 */
int attention_snn_disconnect_bio_async(attention_snn_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge instance
 * @return true if connected
 */
bool attention_snn_is_bio_async_connected(const attention_snn_bridge_t* bridge);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Modulate encoding gain based on arousal
 * @param bridge Bridge instance
 * @param arousal_level Current arousal [0, 1]
 * @return 0 on success, -1 on failure
 */
int attention_snn_modulate_by_arousal(
    attention_snn_bridge_t* bridge,
    float arousal_level
);

/**
 * @brief Set competition strength
 * @param bridge Bridge instance
 * @param strength Competition/inhibition strength [0, 1]
 * @return 0 on success, -1 on failure
 */
int attention_snn_set_competition_strength(
    attention_snn_bridge_t* bridge,
    float strength
);

/**
 * @brief Set gate modulation
 * @param bridge Bridge instance
 * @param gate_level Gate activation [0, 1]
 * @return 0 on success, -1 on failure
 */
int attention_snn_set_gate_modulation(
    attention_snn_bridge_t* bridge,
    float gate_level
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_SNN_BRIDGE_H */

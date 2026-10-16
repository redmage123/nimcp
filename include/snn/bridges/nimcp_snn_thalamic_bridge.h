/**
 * @file nimcp_snn_thalamic_bridge.h
 * @brief SNN-Thalamic bridge for attention-gated spike transmission
 *
 * WHAT: Bidirectional integration between SNN and thalamic routing system
 * WHY:  Enable thalamic gating of SNN spike transmission with attention control
 * HOW:  Route spikes through thalamus with burst/tonic mode switching
 *
 * BIOLOGICAL BASIS:
 * - Thalamus is "gateway to cortex" (Sherman & Guillery, 2001)
 * - Thalamic reticular nucleus (TRN) provides attention-based gating
 * - Burst mode signals wake-up/salient events (Ca²⁺ spike)
 * - Tonic mode for steady-state information relay
 * - Pulvinar coordinates attention across cortical areas
 *
 * INTEGRATION:
 * - Connects to snn_network_t for spike generation
 * - Connects to thalamic_router_t for attention-gated routing
 * - Implements cortical-thalamic loops
 * - Uses bio-async for distributed coordination
 *
 * @author NIMCP Team
 * @date 2024-12-20
 *
 * TODO (2026-04-22): This bridge is pre-integration scaffolding for
 * SNN↔thalamic routing. Currently has no production callers — the new
 * thalamic_channel_t pattern (include/core/thalamic/nimcp_thalamic_channel.h)
 * is preferred for LNN/CNN. This file is retained because it has features
 * the shared channel lacks (TRN lateral inhibition, burst/tonic mode
 * detection, cortical-thalamic feedback buffer). When SNN needs attention-
 * gated routing in production, either: (a) wire this bridge into brain
 * init alongside substrate/thalamic_router, or (b) extend thalamic_channel_t
 * to match and delete this file. See docs/design/substrate_thalamic_integration.md.
 */

#ifndef NIMCP_SNN_THALAMIC_BRIDGE_H
#define NIMCP_SNN_THALAMIC_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "middleware/routing/nimcp_thalamic_router.h"
#include "async/nimcp_bio_async.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Thalamic relay mode
 */
typedef enum {
    THALAMIC_MODE_BURST = 0,     /**< Burst mode (salient events) */
    THALAMIC_MODE_TONIC,         /**< Tonic mode (steady state) */
    THALAMIC_MODE_ADAPTIVE       /**< Switch based on context */
} thalamic_relay_mode_t;

/**
 * @brief SNN thalamic bridge configuration
 *
 * WHAT: Parameters for SNN-thalamic integration
 * WHY:  Control attention gating and relay modes
 * HOW:  Configure burst detection, attention modulation, loops
 */
typedef struct snn_thalamic_config_s {
    /* Relay mode configuration */
    thalamic_relay_mode_t default_mode;   /**< Default relay mode */
    bool enable_mode_switching;           /**< Allow burst/tonic switching */
    float burst_threshold_ms;             /**< ISI for burst detection (ms) */
    float tonic_min_isi_ms;               /**< Min ISI for tonic (ms) */

    /* Attention gating */
    bool enable_attention_gating;         /**< Apply attention modulation */
    float attention_threshold;            /**< Min attention for relay [0, 1] */
    float attention_boost_burst;          /**< Attention boost for bursts */

    /* Cortical-thalamic loops */
    bool enable_ct_loop;                  /**< Enable corticothalamic feedback */
    float ct_delay_ms;                    /**< Corticothalamic delay */
    float ct_gain;                        /**< Feedback loop gain */

    /* Thalamic reticular nucleus (TRN) */
    bool enable_trn_inhibition;           /**< TRN lateral inhibition */
    float trn_inhibition_radius;          /**< Spatial radius for TRN */
    float trn_inhibition_strength;        /**< Inhibition strength */

    /* First-order vs higher-order relay */
    bool enable_first_order;              /**< First-order relay (sensory) */
    bool enable_higher_order;             /**< Higher-order (cortico-cortical) */

    /* Integration flags */
    bool enable_bio_async;                /**< Enable bio-async messaging */
    float update_interval_ms;             /**< How often to update */
} snn_thalamic_config_t;

/**
 * @brief Thalamic routing statistics
 *
 * WHAT: Metrics for thalamic routing performance
 * WHY:  Monitor relay modes, attention effects, loops
 * HOW:  Track burst/tonic counts, gating, feedback
 */
typedef struct snn_thalamic_stats_s {
    uint64_t spikes_relayed;              /**< Total spikes relayed */
    uint64_t spikes_blocked;              /**< Spikes blocked by attention */
    uint64_t bursts_detected;             /**< Burst sequences detected */
    uint64_t tonic_spikes;                /**< Tonic mode spikes */
    uint64_t ct_loop_activations;         /**< CT loop feedback events */
    float avg_attention;                  /**< Average attention weight */
    float burst_ratio;                    /**< Burst / total spikes */
} snn_thalamic_stats_t;

/**
 * @brief SNN-Thalamic bridge structure
 *
 * WHAT: Context for SNN-thalamic integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and relay state
 */
typedef struct snn_thalamic_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;               /**< SNN network being relayed */
    thalamic_router_t* router;            /**< Thalamic router */
    snn_thalamic_config_t config;         /**< Bridge configuration */
    snn_thalamic_stats_t stats;           /**< Routing statistics */

    /* Relay state */
    bool connected;                       /**< Bridge active */
    float last_update_time;               /**< Last update timestamp (ms) */
    thalamic_relay_mode_t* neuron_modes;  /**< Current mode per neuron */
    uint64_t* last_spike_time_us;         /**< Last spike time per neuron */

    /* Attention state */
    float* attention_weights;             /**< Per-population attention */
    float* trn_inhibition;                /**< TRN lateral inhibition */

    /* Cortical-thalamic loop state */
    snn_spike_t* ct_feedback_buffer;      /**< CT loop spike buffer */
    uint32_t ct_buffer_size;              /**< Buffer size */
    uint32_t ct_buffer_count;             /**< Current buffered spikes */

    /* Bio-async */
    bool bio_async_enabled;               /**< Bio-async connected */
    bio_module_context_t bio_ctx;         /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_thalamic_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize thalamic config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from thalamic physiology literature
 *
 * @param config Config to initialize
 */
void snn_thalamic_config_default(snn_thalamic_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-thalamic bridge
 *
 * WHAT: Initialize bidirectional bridge between SNN and thalamus
 * WHY:  Enable attention-gated spike relay
 * HOW:  Allocate context, set up router, configure loops
 *
 * @param config Bridge configuration
 * @param network SNN network to relay
 * @param router Thalamic router
 * @return Bridge instance or NULL on failure
 */
snn_thalamic_bridge_t* snn_thalamic_bridge_create(
    const snn_thalamic_config_t* config,
    snn_network_t* network,
    thalamic_router_t* router
);

/**
 * @brief Destroy SNN-thalamic bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_thalamic_bridge_destroy(snn_thalamic_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed thalamic coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_connect_bio_async(snn_thalamic_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_thalamic_bridge_disconnect_bio_async(snn_thalamic_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_thalamic_bridge_is_bio_async_connected(const snn_thalamic_bridge_t* bridge);

//=============================================================================
// Relay Functions
//=============================================================================

/**
 * @brief Process thalamic spike relay
 *
 * WHAT: Relay spikes through thalamus with attention gating
 * WHY:  Filter and modulate spike transmission
 * HOW:  Detect bursts, apply attention, route via thalamic router
 *
 * ALGORITHM:
 * 1. For each spike:
 *    a. Detect relay mode (burst vs tonic)
 *    b. Apply attention gating
 *    c. Apply TRN lateral inhibition if enabled
 *    d. Route through thalamic router
 *    e. Trigger CT feedback loop if enabled
 * 2. Update statistics
 *
 * @param bridge Bridge for relay
 * @param spikes_in Input spike array [n_spikes]
 * @param n_spikes Number of input spikes
 * @param spikes_out Output spike array (relayed) [n_out capacity]
 * @param n_out_capacity Maximum output spikes
 * @param n_out_actual Actual number of output spikes
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_process(
    snn_thalamic_bridge_t* bridge,
    const snn_spike_t* spikes_in,
    uint32_t n_spikes,
    snn_spike_t* spikes_out,
    uint32_t n_out_capacity,
    uint32_t* n_out_actual
);

/**
 * @brief Update thalamic state
 *
 * WHAT: Update relay modes, attention, CT loops
 * WHY:  Adapt thalamic function to network state
 * HOW:  Process CT feedback, update TRN, adapt modes
 *
 * @param bridge Bridge to update
 * @param dt Timestep (ms)
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_update(snn_thalamic_bridge_t* bridge, float dt);

//=============================================================================
// Mode Control
//=============================================================================

/**
 * @brief Set relay mode for neuron
 *
 * WHAT: Configure burst vs tonic relay mode
 * WHY:  Control information transmission type
 * HOW:  Set mode flag
 *
 * @param bridge Bridge to configure
 * @param neuron_id Global neuron ID
 * @param mode Relay mode to set
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_set_mode(
    snn_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    thalamic_relay_mode_t mode
);

/**
 * @brief Get relay mode for neuron
 *
 * @param bridge Bridge to query
 * @param neuron_id Global neuron ID
 * @param mode Output: current mode
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_get_mode(
    const snn_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    thalamic_relay_mode_t* mode
);

/**
 * @brief Detect relay mode from spike timing
 *
 * WHAT: Infer burst vs tonic from ISI
 * WHY:  Automatic mode adaptation
 * HOW:  Short ISI → burst, long ISI → tonic
 *
 * BIOLOGICAL BASIS:
 * - Burst: ISI < 4ms (Ca²⁺ spike triggered)
 * - Tonic: ISI > 10ms (steady depolarization)
 *
 * @param bridge Bridge with timing state
 * @param neuron_id Global neuron ID
 * @param spike_time_us Current spike time
 * @return Detected relay mode
 */
thalamic_relay_mode_t snn_thalamic_bridge_detect_mode(
    snn_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    uint64_t spike_time_us
);

//=============================================================================
// Attention Control
//=============================================================================

/**
 * @brief Set attention weight for population
 *
 * WHAT: Modulate thalamic transmission for population
 * WHY:  Top-down attention control
 * HOW:  Store weight, apply during relay
 *
 * @param bridge Bridge to configure
 * @param pop_id Population ID
 * @param attention Attention weight [0, 1]
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_set_attention(
    snn_thalamic_bridge_t* bridge,
    uint32_t pop_id,
    float attention
);

/**
 * @brief Get attention weight for population
 *
 * @param bridge Bridge to query
 * @param pop_id Population ID
 * @param attention Output: attention weight
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_get_attention(
    const snn_thalamic_bridge_t* bridge,
    uint32_t pop_id,
    float* attention
);

//=============================================================================
// Cortical-Thalamic Loop
//=============================================================================

/**
 * @brief Trigger cortical-thalamic feedback
 *
 * WHAT: Send cortical spike as thalamic feedback
 * WHY:  Implement CT loop modulation
 * HOW:  Buffer spike for delayed feedback
 *
 * BIOLOGICAL BASIS:
 * - Layer 6 cortical neurons project to thalamus
 * - Modulate thalamic relay gain
 * - Implement attention and context effects
 *
 * @param bridge Bridge with CT loop
 * @param spike Cortical spike for feedback
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_ct_feedback(
    snn_thalamic_bridge_t* bridge,
    const snn_spike_t* spike
);

/**
 * @brief Process CT feedback loop
 *
 * WHAT: Apply cortical feedback to thalamic relay
 * WHY:  Modulate relay based on cortical state
 * HOW:  Deliver delayed feedback, adjust attention
 *
 * @param bridge Bridge with CT state
 * @param current_time_us Current time
 * @return 0 on success, error code on failure
 */
int snn_thalamic_bridge_process_ct_loop(
    snn_thalamic_bridge_t* bridge,
    uint64_t current_time_us
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get thalamic routing statistics
 *
 * @param bridge Bridge to query
 * @param stats Output: statistics (copied)
 * @return 0 on success
 */
int snn_thalamic_bridge_get_stats(
    const snn_thalamic_bridge_t* bridge,
    snn_thalamic_stats_t* stats
);

/**
 * @brief Reset thalamic statistics
 *
 * @param bridge Bridge to reset
 */
void snn_thalamic_bridge_reset_stats(snn_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_THALAMIC_BRIDGE_H */

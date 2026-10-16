/**
 * @file nimcp_snn_immune.h
 * @brief Brain immune system integration for Spiking Neural Networks
 *
 * WHAT: Bidirectional integration between SNN and brain immune system
 * WHY:  Enable immune-modulated learning and instability detection
 * HOW:  Cytokine effects on plasticity, spike-based threat detection
 *
 * BIOLOGICAL BASIS:
 * - Neuroinflammation affects synaptic plasticity (IL-1, IL-6)
 * - Fever suppresses learning (energy conservation)
 * - Cytokines modulate excitability and STDP
 * - Aberrant spiking triggers immune response
 *
 * INTEGRATION:
 * - Connects to brain_immune_system_t
 * - Modulates STDP parameters based on inflammation
 * - Reports network instabilities as threats
 * - Uses bio-async for immune messaging
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_IMMUNE_H
#define NIMCP_SNN_IMMUNE_H

// Includes MUST be before extern "C" to avoid CUDA header conflicts
#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief SNN immune bridge configuration
 *
 * WHAT: Parameters for SNN-immune integration
 * WHY:  Control how immune affects SNN and vice versa
 * HOW:  Thresholds and modulation factors
 */
typedef struct snn_immune_config_s {
    /* Inflammation effects on STDP */
    float stdp_il1_factor;          /**< IL-1 reduces LTP (fever effect) */
    float stdp_il6_factor;          /**< IL-6 reduces plasticity */
    float stdp_tnf_factor;          /**< TNF-alpha modulates STDP */
    float stdp_il10_factor;         /**< IL-10 is anti-inflammatory */

    /* Inflammation effects on excitability */
    float threshold_shift_per_level; /**< mV shift per inflammation level */
    float rate_suppression_factor;   /**< Rate reduction per level */

    /* Network instability thresholds */
    float max_spike_rate;           /**< Max rate before immune alert (Hz) */
    float min_spike_rate;           /**< Min rate (silent network) (Hz) */
    float burst_threshold;          /**< Burst ratio for epileptiform detect */
    float sync_threshold;           /**< Hypersynchrony threshold */

    /* Response configuration */
    bool auto_report_instabilities; /**< Auto-report to immune system */
    bool enable_learning_modulation; /**< Allow immune to modulate learning */
    float update_interval_ms;       /**< How often to check/update */

    /* Bio-async */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} snn_immune_config_t;

/**
 * @brief Cytokine effects on SNN
 *
 * WHAT: Computed effects of current cytokine levels
 * WHY:  Cache computed modulations for efficiency
 * HOW:  Updated on cytokine level changes
 */
typedef struct snn_cytokine_effects_s {
    float stdp_amplitude_factor;    /**< Multiply STDP amplitude [0, 1] */
    float learning_rate_factor;     /**< Multiply learning rate [0, 1] */
    float threshold_shift;          /**< mV shift in spike threshold */
    float excitability_factor;      /**< Input gain factor [0, 1] */
    float refractory_extension;     /**< ms added to refractory */
    brain_inflammation_level_t current_level; /**< Current inflammation level */
} snn_cytokine_effects_t;

/**
 * @brief Network health metrics
 *
 * WHAT: Metrics used for instability detection
 * WHY:  Track network health for immune reporting
 * HOW:  Computed from spike activity
 */
typedef struct snn_health_metrics_s {
    float mean_rate;                /**< Mean firing rate (Hz) */
    float max_rate;                 /**< Maximum neuron rate (Hz) */
    float min_rate;                 /**< Minimum neuron rate (Hz) */
    float burst_ratio;              /**< Fraction of spikes in bursts */
    float sync_index;               /**< Population synchrony [0, 1] */
    float cv_isi;                   /**< Coefficient of variation of ISI */
    uint32_t silent_neurons;        /**< Number of silent neurons */
    uint32_t saturated_neurons;     /**< Neurons at max rate */
    bool has_instability;           /**< Any instability detected */
    snn_state_health_t health;      /**< Overall health status */
} snn_health_metrics_t;

/**
 * @brief SNN-immune bridge structure
 *
 * WHAT: Context for SNN-immune integration
 * WHY:  Maintain state of bidirectional bridge
 * HOW:  Store references and cached effects
 */
typedef struct snn_immune_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* network;             /**< SNN network being monitored */
    brain_immune_system_t* immune;      /**< Brain immune system */
    snn_immune_config_t config;         /**< Bridge configuration */
    snn_cytokine_effects_t effects;     /**< Current cytokine effects */
    snn_health_metrics_t health;        /**< Current health metrics */

    /* State */
    bool connected;                     /**< Bridge active */
    float last_update_time;             /**< Last update timestamp (ms) */
    uint32_t instability_count;         /**< Number of instabilities */
    uint32_t immune_reports;            /**< Reports sent to immune */
} snn_immune_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize immune config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from neuroinflammation literature
 *
 * @param config Config to initialize
 */
void snn_immune_config_default(snn_immune_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-immune bridge
 *
 * WHAT: Initialize bidirectional bridge
 * WHY:  Enable immune-SNN integration
 * HOW:  Allocate context, set up connections
 *
 * @param config Bridge configuration
 * @param network SNN network to monitor
 * @param immune Brain immune system
 * @return Bridge instance or NULL on failure
 */
snn_immune_bridge_t* snn_immune_bridge_create(
    const snn_immune_config_t* config,
    snn_network_t* network,
    brain_immune_system_t* immune
);

/**
 * @brief Destroy SNN-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_immune_bridge_destroy(snn_immune_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed immune coordination
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_immune_bridge_connect_bio_async(snn_immune_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_immune_bridge_disconnect_bio_async(snn_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_immune_bridge_is_bio_async_connected(const snn_immune_bridge_t* bridge);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update cytokine effects from immune system
 *
 * WHAT: Get current cytokine levels and compute effects
 * WHY:  Keep SNN modulation current
 * HOW:  Query immune, compute factors
 *
 * @param bridge Bridge to update
 * @return 0 on success, error code on failure
 */
int snn_immune_update_effects(snn_immune_bridge_t* bridge);

/**
 * @brief Compute network health metrics
 *
 * WHAT: Analyze current network activity
 * WHY:  Detect instabilities
 * HOW:  Compute rates, synchrony, patterns
 *
 * @param bridge Bridge with network to analyze
 * @return 0 on success, error code on failure
 */
int snn_immune_compute_health(snn_immune_bridge_t* bridge);

/**
 * @brief Full update cycle
 *
 * WHAT: Update effects and check health
 * WHY:  Single call for regular updates
 * HOW:  Combines effect update and health check
 *
 * @param bridge Bridge to update
 * @param t Current simulation time (ms)
 * @return 0 on success, error code on failure
 */
int snn_immune_update(snn_immune_bridge_t* bridge, float t);

//=============================================================================
// Modulation Functions
//=============================================================================

/**
 * @brief Apply cytokine modulation to STDP
 *
 * WHAT: Modify STDP parameters based on inflammation
 * WHY:  Model fever-induced learning suppression
 * HOW:  Scale amplitude and time constants
 *
 * @param bridge Bridge with current effects
 * @param a_plus Original LTP amplitude
 * @param a_minus Original LTD amplitude
 * @return Modulated amplitudes in output parameters
 */
void snn_immune_modulate_stdp(
    const snn_immune_bridge_t* bridge,
    float* a_plus,
    float* a_minus
);

/**
 * @brief Get modulated spike threshold
 *
 * WHAT: Adjust threshold for inflammation
 * WHY:  Model reduced excitability during inflammation
 * HOW:  Add threshold shift
 *
 * @param bridge Bridge with current effects
 * @param base_threshold Original threshold (mV)
 * @return Modulated threshold (mV)
 */
float snn_immune_modulate_threshold(
    const snn_immune_bridge_t* bridge,
    float base_threshold
);

/**
 * @brief Get modulated learning rate
 *
 * WHAT: Scale learning rate for inflammation
 * WHY:  Reduce learning during immune response
 * HOW:  Multiply by factor
 *
 * @param bridge Bridge with current effects
 * @param base_lr Original learning rate
 * @return Modulated learning rate
 */
float snn_immune_modulate_learning_rate(
    const snn_immune_bridge_t* bridge,
    float base_lr
);

//=============================================================================
// Instability Reporting
//=============================================================================

/**
 * @brief Report instability to immune system
 *
 * WHAT: Send instability as threat to immune
 * WHY:  Trigger immune response to network problem
 * HOW:  Present as antigen
 *
 * @param bridge Bridge for reporting
 * @param instability_type Type of instability detected
 * @param severity Severity (0-10)
 * @return 0 on success, error code on failure
 */
int snn_immune_report_instability(
    snn_immune_bridge_t* bridge,
    snn_state_health_t instability_type,
    uint8_t severity
);

/**
 * @brief Check network and auto-report instabilities
 *
 * WHAT: Analyze network and report any problems
 * WHY:  Automatic immune integration
 * HOW:  Compute health, report if unstable
 *
 * @param bridge Bridge for checking
 * @return Number of instabilities reported
 */
uint32_t snn_immune_check_and_report(snn_immune_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current cytokine effects
 *
 * @param bridge Bridge to query
 * @param effects Output for effects (copied)
 * @return 0 on success
 */
int snn_immune_get_effects(
    const snn_immune_bridge_t* bridge,
    snn_cytokine_effects_t* effects
);

/**
 * @brief Get current health metrics
 *
 * @param bridge Bridge to query
 * @param health Output for metrics (copied)
 * @return 0 on success
 */
int snn_immune_get_health(
    const snn_immune_bridge_t* bridge,
    snn_health_metrics_t* health
);

/**
 * @brief Get current inflammation level
 *
 * @param bridge Bridge to query
 * @return Current inflammation level
 */
brain_inflammation_level_t snn_immune_get_inflammation(
    const snn_immune_bridge_t* bridge
);

/**
 * @brief Check if network is healthy
 *
 * @param bridge Bridge to check
 * @return true if network is healthy
 */
bool snn_immune_is_network_healthy(const snn_immune_bridge_t* bridge);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge to query
 * @param instability_count Output: total instabilities detected
 * @param reports_sent Output: reports sent to immune
 * @param updates Output: update cycles completed
 * @return 0 on success
 */
int snn_immune_get_stats(
    const snn_immune_bridge_t* bridge,
    uint32_t* instability_count,
    uint32_t* reports_sent,
    uint32_t* updates
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge to reset
 */
void snn_immune_reset_stats(snn_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_IMMUNE_H */

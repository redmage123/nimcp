//=============================================================================
// nimcp_physics_lnn_bridge.h - Physics Layer to Liquid Neural Network Bridge
//=============================================================================
/**
 * @file nimcp_physics_lnn_bridge.h
 * @brief Bridge connecting HH biophysics with Liquid Neural Networks
 *
 * WHAT: Provides bidirectional integration between Hodgkin-Huxley biophysics
 *       and Liquid Neural Networks (LNN), enabling spike-driven LNN computation
 *       and LNN-driven neuromodulation of biophysical parameters.
 *
 * WHY:  Bridges the gap between:
 *       - Biophysical realism (HH action potentials, ion channels)
 *       - Continuous-time machine learning (LNN adaptive dynamics)
 *       This enables biologically-grounded computation with learnable dynamics.
 *
 * HOW:  Two-way integration:
 *       1. HH → LNN: Spike trains encoded as continuous input currents
 *       2. LNN → HH: Network output modulates conductances/thresholds
 *       Temperature coupling ensures consistent temporal dynamics.
 *
 * BIOLOGICAL BASIS:
 * ```
 * HH BIOPHYSICS                         LNN COMPUTATION
 * ─────────────────────────────────────────────────────────────────
 * Action potentials (spikes)         → Input current to LNN neurons
 * Membrane time constants            ↔ LNN liquid time constants (τ)
 * Ion channel conductances           ← LNN output modulation
 * Temperature (Q10 effects)          → LNN time constant scaling
 * Population firing rate             → LNN input magnitude
 * Ephaptic field coherence           → LNN recurrent connectivity strength
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ┌──────────────────────────────────────────────────────────────────┐
 * │                    PHYSICS-LNN BRIDGE                            │
 * │                                                                  │
 * │  ┌─────────────────────┐        ┌─────────────────────┐        │
 * │  │   HH POPULATION     │        │    LNN NETWORK      │        │
 * │  │                     │        │                     │        │
 * │  │  ┌─────┐ ┌─────┐   │        │  ┌─────┐ ┌─────┐   │        │
 * │  │  │Spike│ │Spike│   │ ──────▶│  │ LTC │ │ LTC │   │        │
 * │  │  │Train│ │Train│   │ encode │  │Neuron│ │Neuron│   │        │
 * │  │  └─────┘ └─────┘   │        │  └─────┘ └─────┘   │        │
 * │  │                     │        │         │         │        │
 * │  │  Ion Channels       │ ◀──────│    Output        │        │
 * │  │  (g_Na, g_K)        │modulate│    Layer         │        │
 * │  └─────────────────────┘        └─────────────────────┘        │
 * │                                                                  │
 * │  ┌─────────────────────┐        ┌─────────────────────┐        │
 * │  │  THERMODYNAMICS     │        │   STATE SYNC        │        │
 * │  │  Temperature → τ    │ ──────▶│   LNN τ scaling     │        │
 * │  └─────────────────────┘        └─────────────────────┘        │
 * └──────────────────────────────────────────────────────────────────┘
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_LNN_BRIDGE_H
#define NIMCP_PHYSICS_LNN_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "lnn/nimcp_lnn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_LNN_MODULE_NAME    "physics_lnn_bridge"

/** Default spike encoding time constant (ms) */
#define PHYSICS_LNN_SPIKE_TAU      5.0f

/** Default spike encoding amplitude */
#define PHYSICS_LNN_SPIKE_AMPLITUDE 1.0f

/** Default LNN time constant at reference temperature */
#define PHYSICS_LNN_TAU_REF        10.0f

/** Reference temperature for Q10 scaling (Celsius) */
#define PHYSICS_LNN_TEMP_REF       25.0f

/** Q10 coefficient for LNN time constants */
#define PHYSICS_LNN_Q10            2.3f

/** Maximum modulation factor for conductances */
#define PHYSICS_LNN_MAX_MOD        2.0f

/** Minimum modulation factor for conductances */
#define PHYSICS_LNN_MIN_MOD        0.5f

/** Maximum number of spike history entries */
#define PHYSICS_LNN_MAX_SPIKE_HISTORY 1024

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Spike encoding methods for HH → LNN
 */
typedef enum {
    /** Exponential decay after each spike */
    PHYSICS_LNN_ENCODE_EXPONENTIAL = 0,

    /** Alpha function (t * exp(-t/τ)) */
    PHYSICS_LNN_ENCODE_ALPHA,

    /** Double exponential (rise and decay) */
    PHYSICS_LNN_ENCODE_DOUBLE_EXP,

    /** Rate coding (instantaneous firing rate) */
    PHYSICS_LNN_ENCODE_RATE,

    /** Phase coding (spike timing relative to oscillation) */
    PHYSICS_LNN_ENCODE_PHASE,

    PHYSICS_LNN_ENCODE_COUNT
} physics_lnn_encode_t;

/**
 * @brief LNN output interpretation for modulation
 */
typedef enum {
    /** Direct conductance scaling */
    PHYSICS_LNN_OUTPUT_CONDUCTANCE = 0,

    /** Threshold modulation */
    PHYSICS_LNN_OUTPUT_THRESHOLD,

    /** Time constant scaling */
    PHYSICS_LNN_OUTPUT_TAU,

    /** Current injection */
    PHYSICS_LNN_OUTPUT_CURRENT,

    PHYSICS_LNN_OUTPUT_COUNT
} physics_lnn_output_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** Spike encoding method */
    physics_lnn_encode_t encode_method;

    /** Spike encoding time constant (ms) */
    float spike_tau;

    /** Spike encoding amplitude */
    float spike_amplitude;

    /** Enable temperature coupling */
    bool enable_temp_coupling;

    /** Q10 coefficient for temperature scaling */
    float q10;

    /** Reference temperature (Celsius) */
    float temp_ref;

    /** LNN output interpretation */
    physics_lnn_output_t output_mode;

    /** Modulation strength (0.0-1.0) */
    float modulation_strength;

    /** Enable bidirectional coupling */
    bool bidirectional;

    /** Update interval (ms) - how often to sync state */
    float update_interval_ms;

    /** Number of HH neurons to encode */
    uint32_t num_encode_neurons;

    /** Number of LNN outputs for modulation */
    uint32_t num_output_channels;
} physics_lnn_config_t;

/**
 * @brief Spike event for encoding
 */
typedef struct {
    /** Neuron index that spiked */
    uint32_t neuron_id;

    /** Spike time (ms) */
    float spike_time;

    /** Spike amplitude (for graded responses) */
    float amplitude;
} physics_lnn_spike_t;

/**
 * @brief Encoded input to LNN
 */
typedef struct {
    /** Encoded current values [num_encode_neurons] */
    float* currents;

    /** Number of channels */
    uint32_t num_channels;

    /** Current timestamp (ms) */
    float timestamp;

    /** Total spikes encoded since last reset */
    uint64_t total_spikes;
} physics_lnn_encoded_t;

/**
 * @brief LNN modulation output
 */
typedef struct {
    /** Modulation values [num_output_channels] */
    float* values;

    /** Number of channels */
    uint32_t num_channels;

    /** Output interpretation */
    physics_lnn_output_t mode;

    /** Timestamp (ms) */
    float timestamp;
} physics_lnn_modulation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /** Total spikes encoded */
    uint64_t spikes_encoded;

    /** Total LNN forward passes */
    uint64_t lnn_forward_count;

    /** Total modulations applied */
    uint64_t modulations_applied;

    /** Average encoding latency (ms) */
    float avg_encode_latency;

    /** Average LNN latency (ms) */
    float avg_lnn_latency;

    /** Current temperature (Celsius) */
    float current_temperature;

    /** Current tau scaling factor */
    float tau_scale_factor;

    /** Last update timestamp */
    float last_update_ms;
} physics_lnn_stats_t;

/**
 * @brief Opaque bridge structure
 */
typedef struct physics_lnn_bridge_struct physics_lnn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_default_config(physics_lnn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create physics-LNN bridge
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
NIMCP_EXPORT physics_lnn_bridge_t* physics_lnn_bridge_create(
    const physics_lnn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void physics_lnn_bridge_destroy(physics_lnn_bridge_t* bridge);

/**
 * @brief Connect bridge to HH population
 *
 * @param bridge Bridge instance
 * @param hh_pop HH population
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_connect_hh(
    physics_lnn_bridge_t* bridge,
    nimcp_hh_population_t* hh_pop
);

/**
 * @brief Connect bridge to LNN network
 *
 * @param bridge Bridge instance
 * @param network LNN network
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_connect_lnn(
    physics_lnn_bridge_t* bridge,
    lnn_network_t* network
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_reset(physics_lnn_bridge_t* bridge);

//=============================================================================
// HH → LNN API (Spike Encoding)
//=============================================================================

/**
 * @brief Register a spike event
 *
 * WHAT: Records spike from HH neuron for encoding
 * WHY:  Spikes drive LNN input
 * HOW:  Adds to spike history buffer
 *
 * @param bridge Bridge instance
 * @param neuron_id HH neuron that spiked
 * @param spike_time Time of spike (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_register_spike(
    physics_lnn_bridge_t* bridge,
    uint32_t neuron_id,
    float spike_time
);

/**
 * @brief Encode spikes to LNN input
 *
 * WHAT: Convert spike history to continuous LNN input
 * WHY:  LNN needs continuous input, not discrete spikes
 * HOW:  Apply encoding kernel (exponential, alpha, etc.)
 *
 * @param bridge Bridge instance
 * @param current_time Current simulation time (ms)
 * @param encoded Output encoded input
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_encode_spikes(
    physics_lnn_bridge_t* bridge,
    float current_time,
    physics_lnn_encoded_t* encoded
);

/**
 * @brief Auto-detect spikes from HH population
 *
 * WHAT: Scan HH population for threshold crossings
 * WHY:  Automatic spike detection from biophysics
 * HOW:  Check membrane voltage against threshold
 *
 * @param bridge Bridge instance
 * @param current_time Current simulation time (ms)
 * @param threshold Spike threshold (mV, typically 0 or -20)
 * @return Number of spikes detected, -1 on error
 */
NIMCP_EXPORT int physics_lnn_detect_spikes(
    physics_lnn_bridge_t* bridge,
    float current_time,
    float threshold
);

//=============================================================================
// LNN → HH API (Modulation)
//=============================================================================

/**
 * @brief Get LNN output for modulation
 *
 * WHAT: Extract LNN network output as modulation signal
 * WHY:  LNN output drives HH parameter changes
 * HOW:  Read output layer, scale to modulation range
 *
 * @param bridge Bridge instance
 * @param modulation Output modulation values
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_get_modulation(
    physics_lnn_bridge_t* bridge,
    physics_lnn_modulation_t* modulation
);

/**
 * @brief Apply modulation to HH population
 *
 * WHAT: Modify HH parameters based on LNN output
 * WHY:  Enables LNN-driven neuromodulation
 * HOW:  Scale conductances, thresholds, or inject current
 *
 * @param bridge Bridge instance
 * @param modulation Modulation to apply
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_apply_modulation(
    physics_lnn_bridge_t* bridge,
    const physics_lnn_modulation_t* modulation
);

//=============================================================================
// Temperature Coupling API
//=============================================================================

/**
 * @brief Set temperature for Q10 scaling
 *
 * WHAT: Update temperature for LNN time constant scaling
 * WHY:  Temperature affects both HH and LNN dynamics
 * HOW:  Apply Q10 factor to LNN tau values
 *
 * @param bridge Bridge instance
 * @param temperature Temperature (Celsius)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_set_temperature(
    physics_lnn_bridge_t* bridge,
    float temperature
);

/**
 * @brief Get current tau scaling factor
 *
 * @param bridge Bridge instance
 * @return Tau scale factor (1.0 at reference temperature)
 */
NIMCP_EXPORT float physics_lnn_get_tau_scale(
    const physics_lnn_bridge_t* bridge
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Full bridge update cycle
 *
 * WHAT: Perform complete HH ↔ LNN synchronization
 * WHY:  Single call for bidirectional integration
 * HOW:  Detect spikes → Encode → LNN forward → Get modulation → Apply
 *
 * @param bridge Bridge instance
 * @param dt Time step (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_update(
    physics_lnn_bridge_t* bridge,
    float dt
);

/**
 * @brief Forward LNN step with current encoded input
 *
 * @param bridge Bridge instance
 * @param dt Time step (ms)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_forward_step(
    physics_lnn_bridge_t* bridge,
    float dt
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_get_stats(
    const physics_lnn_bridge_t* bridge,
    physics_lnn_stats_t* stats
);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge instance
 * @return true if HH and LNN are both connected
 */
NIMCP_EXPORT bool physics_lnn_is_connected(
    const physics_lnn_bridge_t* bridge
);

/**
 * @brief Get current encoded input (for debugging)
 *
 * @param bridge Bridge instance
 * @param encoded Output encoded state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_lnn_get_encoded(
    const physics_lnn_bridge_t* bridge,
    physics_lnn_encoded_t* encoded
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_LNN_BRIDGE_H */

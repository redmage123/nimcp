//=============================================================================
// nimcp_ephaptic.h - Ephaptic Coupling Module (AC-1)
//=============================================================================
/**
 * @file nimcp_ephaptic.h
 * @brief Ephaptic coupling for non-synaptic neural communication
 *
 * BIOLOGICAL: Ephaptic coupling refers to the influence of extracellular
 * electric fields on neural activity. Unlike synaptic transmission, ephaptic
 * effects arise from voltage gradients in the extracellular space that can
 * modulate nearby neurons' membrane potentials.
 *
 * WHAT: Models local field potentials (LFP), electric field effects, and
 *       phase synchronization via ephaptic mechanisms
 *
 * WHY:  Ephaptic coupling is increasingly recognized as an important mechanism
 *       for neural synchronization, particularly in densely packed cortical
 *       columns where extracellular fields can reach 1-5 mV/mm. This enables
 *       rapid coordination without synaptic delays.
 *
 * HOW:  - LFP computation from population activity
 *       - Electric field calculation from voltage gradients
 *       - Kuramoto-like phase coupling for synchronization
 *       - Field-induced membrane polarization
 *
 * REFERENCES:
 * - Anastassiou et al. (2011) "Ephaptic coupling of cortical neurons"
 * - Fröhlich & McCormick (2010) "Endogenous electric fields may guide neocortical network activity"
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_H
#define NIMCP_EPHAPTIC_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Error Codes
//=============================================================================

/* Include canonical error type definition */
#ifndef NIMCP_ERROR_TYPE_DEFINED
#include "utils/error/nimcp_error_codes.h"
#endif

/**
 * @brief Ephaptic module error codes (0x1500xx range)
 */
typedef enum {
    EPHAPTIC_OK = 0,
    EPHAPTIC_ERROR_NULL_POINTER       = 0x150001,
    EPHAPTIC_ERROR_INVALID_CONFIG     = 0x150002,
    EPHAPTIC_ERROR_NOT_INITIALIZED    = 0x150003,
    EPHAPTIC_ERROR_ALREADY_INITIALIZED = 0x150004,
    EPHAPTIC_ERROR_INVALID_PARAMETER  = 0x150005,
    EPHAPTIC_ERROR_OUT_OF_MEMORY      = 0x150006,
    EPHAPTIC_ERROR_NO_NEURONS         = 0x150007,
    EPHAPTIC_ERROR_FIELD_OVERFLOW     = 0x150008,
    EPHAPTIC_ERROR_SYNC_FAILED        = 0x150009
} ephaptic_error_t;

//=============================================================================
// Constants
//=============================================================================

/** Default coupling strength (unitless, [0,1]) */
#define EPHAPTIC_DEFAULT_COUPLING_STRENGTH    0.1f

/** Default field decay constant (mm^-1) - spatial decay rate */
#define EPHAPTIC_DEFAULT_FIELD_DECAY          2.0f

/** Default synchronization threshold (phase coherence [0,1]) */
#define EPHAPTIC_DEFAULT_SYNC_THRESHOLD       0.7f

/** Maximum field strength (V/m) - prevent numerical instability */
#define EPHAPTIC_MAX_FIELD_STRENGTH           100.0f

/** Maximum magnetic field (Tesla) */
#define EPHAPTIC_MAX_MAGNETIC_FIELD           1e-9f

/** Typical extracellular resistivity (Ohm-cm) */
#define EPHAPTIC_EXTRACELLULAR_RESISTIVITY    300.0f

/** Default LFP decay time constant (ms) */
#define EPHAPTIC_DEFAULT_LFP_TAU              10.0f

/** Maximum neurons for phase synchronization tracking */
#define EPHAPTIC_MAX_TRACKED_NEURONS          10000

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Ephaptic system configuration
 *
 * BIOLOGICAL: Parameters controlling the strength and spatial extent of
 * ephaptic effects in neural tissue.
 */
typedef struct {
    /** Coupling strength - scales field effects [0, 1] */
    float coupling_strength;

    /** Field decay constant (mm^-1) - rate of spatial decay */
    float field_decay_constant;

    /** Synchronization threshold - minimum coherence for sync [0, 1] */
    float sync_threshold;

    /** Enable magnetic field computation (computationally expensive) */
    bool enable_magnetic_field;

    /** LFP decay time constant (ms) */
    float lfp_tau;

    /** Extracellular resistivity (Ohm-cm) */
    float extracellular_resistivity;

    /** Kuramoto coupling constant for phase synchronization */
    float kuramoto_coupling;

    /** Enable adaptive coupling based on local activity */
    bool enable_adaptive_coupling;
} nimcp_ephaptic_config_t;

/**
 * @brief Neuron state for ephaptic modulation
 *
 * BIOLOGICAL: Represents a neuron's membrane state relevant to ephaptic
 * coupling - its position, voltage, phase, and susceptibility to fields.
 */
typedef struct {
    /** Neuron identifier */
    uint32_t id;

    /** Position in 3D space (mm) */
    float position[3];

    /** Membrane potential (mV) */
    float membrane_potential;

    /** Oscillation phase [0, 2*pi) for synchronization */
    float phase;

    /** Natural frequency (Hz) for Kuramoto model */
    float natural_frequency;

    /** Field susceptibility - how strongly this neuron responds to fields */
    float field_susceptibility;

    /** Is neuron currently spiking */
    bool is_spiking;
} nimcp_ephaptic_neuron_t;

/**
 * @brief Ephaptic system state
 *
 * BIOLOGICAL: Complete state of the ephaptic field model including
 * electric and magnetic fields, local field potential, and synchronization
 * metrics for a neural population.
 *
 * WHAT: Core state structure for ephaptic coupling simulation
 * WHY:  Tracks all field components and synchronization state
 * HOW:  Updated via compute and synchronize functions
 */
typedef struct {
    /** Electric field strength components (V/m) in x, y, z */
    float field_strength[3];

    /** Local field potential (mV) */
    float field_potential;

    /** Magnetic field components (Tesla) in x, y, z */
    float magnetic_field[3];

    /** Field-induced current (pA) - current induced by field gradient */
    float field_induced_current;

    /** Membrane polarization change from fields (mV) */
    float membrane_polarization;

    /** Phase locking strength (0 = no sync, 1 = perfect sync) */
    float phase_locking_strength;

    /** Number of neurons currently synchronized */
    uint32_t synchronized_neurons;

    /** Module identifier for bio-async integration */
    uint32_t module_id;

    /** Initialization flag */
    bool initialized;

    /** Current configuration */
    nimcp_ephaptic_config_t config;

    /** Internal: array of tracked neuron states */
    nimcp_ephaptic_neuron_t* neurons;

    /** Internal: number of tracked neurons */
    uint32_t neuron_count;

    /** Internal: allocated capacity for neurons */
    uint32_t neuron_capacity;

    /** Internal: accumulated LFP for decay computation */
    float lfp_accumulated;

    /** Internal: mean field direction (normalized) */
    float field_direction[3];

    /** Internal: complex order parameter for phase coherence */
    float order_parameter_real;
    float order_parameter_imag;

    /** Simulation time (ms) */
    float time;

    /** Last update timestep (ms) */
    float dt;
} nimcp_ephaptic_system_t;

/**
 * @brief LFP computation result
 *
 * BIOLOGICAL: Local field potential arises from the summed extracellular
 * currents of many neurons. This structure captures the LFP magnitude
 * and its dominant frequency components.
 */
typedef struct {
    /** LFP amplitude (mV) */
    float amplitude;

    /** Dominant frequency (Hz) */
    float dominant_frequency;

    /** Power in different frequency bands (delta, theta, alpha, beta, gamma) */
    float band_power[5];

    /** Phase of the LFP signal (radians) */
    float phase;
} nimcp_lfp_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default ephaptic configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide biological plausible starting parameters
 * HOW:  Static defaults based on experimental literature
 *
 * @return Default configuration structure
 */
NIMCP_EXPORT nimcp_ephaptic_config_t nimcp_ephaptic_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize ephaptic coupling system
 *
 * WHAT: Initializes the ephaptic system with given configuration
 * WHY:  Set up field computation state before simulation
 * HOW:  Allocates internal buffers, initializes field state
 *
 * BIOLOGICAL: Prepares the ephaptic coupling model for a neural population,
 * setting up data structures to track extracellular fields and synchronization.
 *
 * @param system    System to initialize
 * @param config    Configuration (NULL for defaults)
 * @return NIMCP_OK on success, error code on failure
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_init(
    nimcp_ephaptic_system_t* system,
    const nimcp_ephaptic_config_t* config
);

/**
 * @brief Destroy ephaptic system and free resources
 *
 * WHAT: Cleans up all allocated resources
 * WHY:  Proper memory management
 * HOW:  Frees internal arrays, resets state
 *
 * @param system    System to destroy
 */
NIMCP_EXPORT void nimcp_ephaptic_destroy(nimcp_ephaptic_system_t* system);

/**
 * @brief Reset ephaptic system to initial state
 *
 * WHAT: Resets fields and synchronization state without reallocating
 * WHY:  Allow reuse of system for multiple simulations
 * HOW:  Zeros out field values, resets synchronization metrics
 *
 * @param system    System to reset
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_reset(nimcp_ephaptic_system_t* system);

//=============================================================================
// Neuron Management API
//=============================================================================

/**
 * @brief Add a neuron to the ephaptic system
 *
 * WHAT: Registers a neuron for ephaptic field computation
 * WHY:  Track neurons contributing to and affected by fields
 * HOW:  Stores neuron state in internal array
 *
 * @param system    Ephaptic system
 * @param neuron    Neuron state to add
 * @return NIMCP_OK on success, error code on failure
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_add_neuron(
    nimcp_ephaptic_system_t* system,
    const nimcp_ephaptic_neuron_t* neuron
);

/**
 * @brief Update a neuron's state
 *
 * WHAT: Updates voltage and phase for an existing neuron
 * WHY:  Keep ephaptic computation in sync with neural simulation
 * HOW:  Finds neuron by ID and updates state
 *
 * @param system              Ephaptic system
 * @param neuron_id           ID of neuron to update
 * @param membrane_potential  New membrane potential (mV)
 * @param phase               New oscillation phase (radians)
 * @param is_spiking          Whether neuron is currently spiking
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_update_neuron(
    nimcp_ephaptic_system_t* system,
    uint32_t neuron_id,
    float membrane_potential,
    float phase,
    bool is_spiking
);

/**
 * @brief Clear all neurons from system
 *
 * @param system    Ephaptic system
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_clear_neurons(
    nimcp_ephaptic_system_t* system
);

//=============================================================================
// Core Computation API
//=============================================================================

/**
 * @brief Compute local field potential from population activity
 *
 * WHAT: Calculates LFP from summed neural currents
 * WHY:  LFP represents collective extracellular activity measurable by electrodes
 * HOW:  Sum weighted contributions from all neurons, apply distance decay
 *
 * BIOLOGICAL: The LFP reflects the summed transmembrane currents of neurons
 * within the recording volume. It is dominated by synaptic currents due to
 * their longer time constants compared to action potentials.
 *
 * LFP = Sigma_i [ (V_i - V_rest) * w_i * exp(-d_i / lambda) ]
 *
 * @param system    Ephaptic system (must have neurons registered)
 * @param position  Position to compute LFP at (mm, xyz)
 * @param result    Output LFP result
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_compute_lfp(
    nimcp_ephaptic_system_t* system,
    const float position[3],
    nimcp_lfp_result_t* result
);

/**
 * @brief Update electric field from current activity
 *
 * WHAT: Computes electric field from voltage gradients
 * WHY:  Electric field mediates ephaptic effects on nearby neurons
 * HOW:  E = -grad(V), computed from neuron potentials
 *
 * BIOLOGICAL: Extracellular electric fields arise from spatial gradients
 * in transmembrane currents. Fields of 1-5 mV/mm can modulate neural
 * excitability by shifting spike threshold.
 *
 * @param system    Ephaptic system
 * @param dt        Timestep (ms)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_update_field(
    nimcp_ephaptic_system_t* system,
    float dt
);

/**
 * @brief Apply phase synchronization via Kuramoto-like coupling
 *
 * WHAT: Updates neuron phases based on field-mediated coupling
 * WHY:  Ephaptic fields can synchronize neural oscillations
 * HOW:  Kuramoto model: d(theta)/dt = omega + K * sum(sin(theta_j - theta_i))
 *
 * BIOLOGICAL: Ephaptic coupling provides a mechanism for rapid phase
 * synchronization without synaptic delays, potentially underlying
 * gamma oscillations in densely packed cortical columns.
 *
 * @param system    Ephaptic system
 * @param dt        Timestep (ms)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_synchronize(
    nimcp_ephaptic_system_t* system,
    float dt
);

/**
 * @brief Get current phase coherence of the population
 *
 * WHAT: Computes the Kuramoto order parameter r
 * WHY:  Quantifies degree of phase synchronization
 * HOW:  r = |1/N * sum(exp(i*theta_j))| in [0, 1]
 *
 * @param system    Ephaptic system
 * @param coherence Output: phase coherence [0, 1]
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_get_phase_coherence(
    const nimcp_ephaptic_system_t* system,
    float* coherence
);

/**
 * @brief Modulate a neuron's membrane potential via ephaptic field
 *
 * WHAT: Computes field-induced polarization for a specific neuron
 * WHY:  Ephaptic fields shift membrane potential toward/away from threshold
 * HOW:  delta_V = E_field * susceptibility * coupling_strength
 *
 * BIOLOGICAL: The extracellular field acts on the neuron's membrane,
 * causing depolarization or hyperpolarization depending on field
 * orientation relative to the cell's axis.
 *
 * @param system          Ephaptic system
 * @param neuron_id       ID of neuron to modulate
 * @param polarization    Output: membrane potential change (mV)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_modulate_neuron(
    const nimcp_ephaptic_system_t* system,
    uint32_t neuron_id,
    float* polarization
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current electric field strength
 *
 * @param system    Ephaptic system
 * @param field     Output: field strength (V/m), 3 components
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_get_field(
    const nimcp_ephaptic_system_t* system,
    float field[3]
);

/**
 * @brief Get current field potential (LFP)
 *
 * @param system    Ephaptic system
 * @param potential Output: field potential (mV)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_get_potential(
    const nimcp_ephaptic_system_t* system,
    float* potential
);

/**
 * @brief Get number of synchronized neurons
 *
 * @param system    Ephaptic system
 * @param count     Output: number of synchronized neurons
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t nimcp_ephaptic_get_sync_count(
    const nimcp_ephaptic_system_t* system,
    uint32_t* count
);

/**
 * @brief Check if system is initialized
 *
 * @param system    Ephaptic system
 * @return true if initialized
 */
NIMCP_EXPORT bool nimcp_ephaptic_is_initialized(
    const nimcp_ephaptic_system_t* system
);

//=============================================================================
// Wave W15: Runtime aggregated field-coupling events
//
// Never emit from the per-neuron field update loop — call from the
// aggregator (LFP compute complete, synchronize complete, or phase
// coherence threshold crossing).
//=============================================================================

#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

struct brain_kg;

/**
 * @brief Register brain handle for ephaptic runtime KG emission.
 */
NIMCP_EXPORT void nimcp_ephaptic_kg_register_brain(brain_t brain);

/**
 * @brief Emit an aggregated ephaptic field-coupling event.
 *
 * @param brain                 Brain handle (falls back to registered one)
 * @param kind                  "field_sync", "field_desync", "lfp_spike",
 *                              "phase_lock"
 * @param lfp_amplitude_mv      Recent aggregate LFP amplitude (mV)
 * @param phase_coherence       Current phase coherence [0,1]
 * @param ts_us                 Timestamp in microseconds
 */
NIMCP_EXPORT void nimcp_ephaptic_kg_trigger_field_event(
    brain_t brain,
    const char* kind,
    float lfp_amplitude_mv,
    float phase_coherence,
    uint64_t ts_us
);

/**
 * @brief Read-path: count ephaptic events matching kind.
 */
NIMCP_EXPORT uint32_t nimcp_ephaptic_kg_count_events(
    struct brain_kg* kg,
    const char* kind_substr
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_H */

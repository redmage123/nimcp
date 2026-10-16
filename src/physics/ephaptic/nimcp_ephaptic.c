#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_ephaptic.c - Ephaptic Coupling Module Implementation (AC-1)
//=============================================================================
/**
 * @file nimcp_ephaptic.c
 * @brief Implementation of ephaptic coupling for non-synaptic neural communication
 *
 * BIOLOGICAL: This module implements ephaptic coupling mechanisms that allow
 * neurons to influence each other through extracellular electric fields,
 * independent of synaptic transmission.
 *
 * KEY ALGORITHMS:
 * 1. LFP computation: Weighted sum of membrane currents with distance decay
 * 2. Electric field: Spatial gradient of extracellular potential
 * 3. Phase sync: Kuramoto model with field-mediated coupling
 * 4. Membrane modulation: Field projection onto neuronal axis
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0
 */

#include "physics/ephaptic/nimcp_ephaptic.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ephaptic)

//=============================================================================
// Internal Constants
//=============================================================================

/** Two times pi for phase calculations */

/** Resting membrane potential (mV) */
#define V_REST -65.0f

/** Initial neuron array capacity */
#define INITIAL_NEURON_CAPACITY 256

/** Small value for numerical stability */
#define EPSILON 1e-10f

/** Safe exponential limit to prevent overflow */
#define EXP_LIMIT 80.0f

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Safe exponential to prevent overflow
 *
 * WHAT: Bounded exponential function
 * WHY:  Prevent numerical instability from large arguments
 * HOW:  Clamp input to safe range
 */
static inline float safe_exp(float x) {
    if (x > EXP_LIMIT) return expf(EXP_LIMIT);
    if (x < -EXP_LIMIT) return expf(-EXP_LIMIT);
    return expf(x);
}

/**
 * @brief Compute Euclidean distance between two 3D points
 *
 * WHAT: 3D distance calculation
 * WHY:  Distance needed for field decay computation
 * HOW:  Standard Euclidean formula
 */
static inline float compute_distance(const float p1[3], const float p2[3]) {
    float dx = p1[0] - p2[0];
    float dy = p1[1] - p2[1];
    float dz = p1[2] - p2[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Normalize a 3D vector
 *
 * WHAT: Make vector unit length
 * WHY:  Field direction should be normalized
 * HOW:  Divide by magnitude
 */
static void normalize_vector(float v[3]) {
    float mag = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (mag > EPSILON) {
        v[0] /= mag;
        v[1] /= mag;
        v[2] /= mag;
    }
}

/**
 * @brief Wrap phase to [0, 2*pi)
 *
 * WHAT: Keep phase in valid range
 * WHY:  Phase accumulates during simulation
 * HOW:  Modulo arithmetic
 */
static inline float wrap_phase(float phase) {
    while (phase < 0.0f) phase += NIMCP_TWO_PI_F;
    while (phase >= NIMCP_TWO_PI_F) phase -= NIMCP_TWO_PI_F;
    return phase;
}

/**
 * @brief Find neuron by ID in system
 *
 * WHAT: Linear search for neuron
 * WHY:  Need to update/query specific neurons
 * HOW:  Iterate through array, match ID
 *
 * @return Pointer to neuron or NULL if not found
 */
static nimcp_ephaptic_neuron_t* find_neuron(
    nimcp_ephaptic_system_t* system,
    uint32_t neuron_id
) {
    if (!system || !system->neurons) {
        return NULL;
    }

    for (uint32_t i = 0; i < system->neuron_count; i++) {
        if (system->neurons[i].id == neuron_id) {
            return &system->neurons[i];
        }
    }
    return NULL;  /* Not found - normal lookup behavior */
}

/**
 * @brief Ensure neuron array has capacity for more neurons
 *
 * WHAT: Grow array if needed
 * WHY:  Support dynamic neuron registration
 * HOW:  Realloc with doubling strategy
 */
static nimcp_error_t ensure_neuron_capacity(nimcp_ephaptic_system_t* system) {
    if (!system) return EPHAPTIC_ERROR_NULL_POINTER;

    if (system->neuron_count < system->neuron_capacity) {
        return NIMCP_OK;
    }

    /* Double capacity */
    uint32_t new_capacity = system->neuron_capacity * 2;
    if (new_capacity > EPHAPTIC_MAX_TRACKED_NEURONS) {
        new_capacity = EPHAPTIC_MAX_TRACKED_NEURONS;
    }
    if (new_capacity <= system->neuron_capacity) {
        return EPHAPTIC_ERROR_OUT_OF_MEMORY;
    }

    nimcp_ephaptic_neuron_t* new_neurons = (nimcp_ephaptic_neuron_t*)nimcp_realloc(
        system->neurons,
        new_capacity * sizeof(nimcp_ephaptic_neuron_t)
    );
    if (!new_neurons) {
        LOG_ERROR("Failed to reallocate ephaptic neuron array to capacity %u", new_capacity);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to reallocate ephaptic neurons");
        return EPHAPTIC_ERROR_OUT_OF_MEMORY;
    }

    system->neurons = new_neurons;
    system->neuron_capacity = new_capacity;
    return NIMCP_OK;
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

nimcp_ephaptic_config_t nimcp_ephaptic_default_config(void) {
    nimcp_ephaptic_config_t config;

    config.coupling_strength = EPHAPTIC_DEFAULT_COUPLING_STRENGTH;
    config.field_decay_constant = EPHAPTIC_DEFAULT_FIELD_DECAY;
    config.sync_threshold = EPHAPTIC_DEFAULT_SYNC_THRESHOLD;
    config.enable_magnetic_field = false;
    config.lfp_tau = EPHAPTIC_DEFAULT_LFP_TAU;
    config.extracellular_resistivity = EPHAPTIC_EXTRACELLULAR_RESISTIVITY;
    config.kuramoto_coupling = 1.0f;
    config.enable_adaptive_coupling = false;

    return config;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

nimcp_error_t nimcp_ephaptic_init(
    nimcp_ephaptic_system_t* system,
    const nimcp_ephaptic_config_t* config
) {
    /* Guard: null pointer */
    if (!system) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Zero initialize FIRST to avoid reading uninitialized memory.
     * Note: We do NOT check initialized flag before memset because
     * the system struct may contain garbage values on the stack. */
    memset(system, 0, sizeof(nimcp_ephaptic_system_t));

    /* Apply configuration */
    if (config) {
        system->config = *config;
    } else {
        system->config = nimcp_ephaptic_default_config();
    }

    /* Validate configuration */
    if (system->config.coupling_strength < 0.0f ||
        system->config.coupling_strength > 1.0f) {
        return EPHAPTIC_ERROR_INVALID_CONFIG;
    }
    if (system->config.field_decay_constant <= 0.0f) {
        return EPHAPTIC_ERROR_INVALID_CONFIG;
    }
    if (system->config.sync_threshold < 0.0f ||
        system->config.sync_threshold > 1.0f) {
        return EPHAPTIC_ERROR_INVALID_CONFIG;
    }

    /* Allocate initial neuron array */
    system->neurons = (nimcp_ephaptic_neuron_t*)nimcp_calloc(
        INITIAL_NEURON_CAPACITY,
        sizeof(nimcp_ephaptic_neuron_t)
    );
    if (!system->neurons) {
        LOG_ERROR("Failed to allocate ephaptic neuron array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate ephaptic neurons");
        return EPHAPTIC_ERROR_OUT_OF_MEMORY;
    }
    system->neuron_capacity = INITIAL_NEURON_CAPACITY;
    system->neuron_count = 0;

    /* Initialize field state */
    system->field_strength[0] = 0.0f;
    system->field_strength[1] = 0.0f;
    system->field_strength[2] = 0.0f;
    system->field_potential = 0.0f;
    system->magnetic_field[0] = 0.0f;
    system->magnetic_field[1] = 0.0f;
    system->magnetic_field[2] = 0.0f;
    system->field_induced_current = 0.0f;
    system->membrane_polarization = 0.0f;

    /* Initialize synchronization state */
    system->phase_locking_strength = 0.0f;
    system->synchronized_neurons = 0;
    system->order_parameter_real = 0.0f;
    system->order_parameter_imag = 0.0f;

    /* Initialize timing */
    system->time = 0.0f;
    system->dt = 0.0f;

    system->initialized = true;
    return NIMCP_OK;
}

void nimcp_ephaptic_destroy(nimcp_ephaptic_system_t* system) {
    if (!system) return;

    if (system->neurons) {
        nimcp_free(system->neurons);
        system->neurons = NULL;
    }

    memset(system, 0, sizeof(nimcp_ephaptic_system_t));
}

nimcp_error_t nimcp_ephaptic_reset(nimcp_ephaptic_system_t* system) {
    /* Guard: null pointer */
    if (!system) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    /* Reset field state */
    system->field_strength[0] = 0.0f;
    system->field_strength[1] = 0.0f;
    system->field_strength[2] = 0.0f;
    system->field_potential = 0.0f;
    system->magnetic_field[0] = 0.0f;
    system->magnetic_field[1] = 0.0f;
    system->magnetic_field[2] = 0.0f;
    system->field_induced_current = 0.0f;
    system->membrane_polarization = 0.0f;
    system->lfp_accumulated = 0.0f;

    /* Reset synchronization state */
    system->phase_locking_strength = 0.0f;
    system->synchronized_neurons = 0;
    system->order_parameter_real = 0.0f;
    system->order_parameter_imag = 0.0f;

    /* Reset timing */
    system->time = 0.0f;
    system->dt = 0.0f;

    /* Reset neuron phases (keep other state) */
    for (uint32_t i = 0; i < system->neuron_count; i++) {
        system->neurons[i].phase = 0.0f;
    }

    return NIMCP_OK;
}

//=============================================================================
// Neuron Management API Implementation
//=============================================================================

nimcp_error_t nimcp_ephaptic_add_neuron(
    nimcp_ephaptic_system_t* system,
    const nimcp_ephaptic_neuron_t* neuron
) {
    /* Guard: null pointers */
    if (!system || !neuron) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    /* Ensure capacity */
    nimcp_error_t err = ensure_neuron_capacity(system);
    if (err != NIMCP_OK) {
        return err;
    }

    /* Check for duplicate ID */
    if (find_neuron(system, neuron->id) != NULL) {
        /* Update existing instead of adding duplicate */
        return nimcp_ephaptic_update_neuron(
            system,
            neuron->id,
            neuron->membrane_potential,
            neuron->phase,
            neuron->is_spiking
        );
    }

    /* Add new neuron */
    system->neurons[system->neuron_count] = *neuron;
    system->neuron_count++;

    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_update_neuron(
    nimcp_ephaptic_system_t* system,
    uint32_t neuron_id,
    float membrane_potential,
    float phase,
    bool is_spiking
) {
    /* Guard: null pointer */
    if (!system) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    /* Find neuron */
    nimcp_ephaptic_neuron_t* neuron = find_neuron(system, neuron_id);
    if (!neuron) {
        return EPHAPTIC_ERROR_INVALID_PARAMETER;
    }

    /* Update state */
    neuron->membrane_potential = membrane_potential;
    neuron->phase = wrap_phase(phase);
    neuron->is_spiking = is_spiking;

    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_clear_neurons(nimcp_ephaptic_system_t* system) {
    /* Guard: null pointer */
    if (!system) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    system->neuron_count = 0;
    return NIMCP_OK;
}

//=============================================================================
// Core Computation API Implementation
//=============================================================================

nimcp_error_t nimcp_ephaptic_compute_lfp(
    nimcp_ephaptic_system_t* system,
    const float position[3],
    nimcp_lfp_result_t* result
) {
    /* Guard: null pointers */
    if (!system || !position || !result) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    /* Guard: no neurons */
    if (system->neuron_count == 0) {
        memset(result, 0, sizeof(nimcp_lfp_result_t));
        return NIMCP_OK;
    }

    /*
     * BIOLOGICAL: LFP computation
     *
     * The local field potential at a point is the sum of contributions
     * from all nearby neurons, weighted by distance and activity.
     *
     * LFP = sum_i [ (V_i - V_rest) / (4 * pi * rho * r_i) ]
     *
     * where:
     *   V_i = membrane potential of neuron i
     *   V_rest = resting potential
     *   rho = extracellular resistivity
     *   r_i = distance to neuron i
     */

    float lfp_sum = 0.0f;
    float phase_sum_real = 0.0f;
    float phase_sum_imag = 0.0f;

    for (uint32_t i = 0; i < system->neuron_count; i++) {
        nimcp_ephaptic_neuron_t* n = &system->neurons[i];

        /* Compute distance from position to neuron */
        float dist = compute_distance(position, n->position);
        if (dist < EPSILON) dist = EPSILON;  /* Prevent division by zero */

        /* Distance-weighted contribution */
        float decay = safe_exp(-dist * system->config.field_decay_constant);
        float voltage_diff = n->membrane_potential - V_REST;

        /* Add contribution (with resistivity factor) */
        float contribution = voltage_diff * decay / dist;
        lfp_sum += contribution;

        /* Phase contribution for dominant frequency estimation */
        phase_sum_real += cosf(n->phase) * decay;
        phase_sum_imag += sinf(n->phase) * decay;
    }

    /* Scale by resistivity and geometric factors */
    float scale = 1.0f / (4.0f * NIMCP_PI_F * system->config.extracellular_resistivity);
    result->amplitude = fabsf(lfp_sum * scale);

    /* Compute LFP phase from weighted phase average */
    result->phase = atan2f(phase_sum_imag, phase_sum_real);

    /* Estimate dominant frequency from mean neural frequency */
    float freq_sum = 0.0f;
    for (uint32_t i = 0; i < system->neuron_count; i++) {
        freq_sum += system->neurons[i].natural_frequency;
    }
    result->dominant_frequency = (system->neuron_count > 0)
        ? freq_sum / (float)system->neuron_count : 0.0f;

    /* Simplified band power estimation */
    /* In a full implementation, would use FFT on time series */
    result->band_power[0] = result->amplitude * 0.1f;  /* Delta */
    result->band_power[1] = result->amplitude * 0.15f; /* Theta */
    result->band_power[2] = result->amplitude * 0.2f;  /* Alpha */
    result->band_power[3] = result->amplitude * 0.25f; /* Beta */
    result->band_power[4] = result->amplitude * 0.3f;  /* Gamma */

    /* Store in system for field computation */
    system->field_potential = result->amplitude;

    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_update_field(
    nimcp_ephaptic_system_t* system,
    float dt
) {
    /* Guard: null pointer */
    if (!system) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    /* Guard: invalid timestep */
    if (dt <= 0.0f) {
        return EPHAPTIC_ERROR_INVALID_PARAMETER;
    }

    system->dt = dt;

    /* No neurons - no field */
    if (system->neuron_count == 0) {
        system->field_strength[0] = 0.0f;
        system->field_strength[1] = 0.0f;
        system->field_strength[2] = 0.0f;
        return NIMCP_OK;
    }

    /*
     * BIOLOGICAL: Electric field computation
     *
     * The electric field E = -grad(V) is computed from voltage gradients
     * in the extracellular space. We estimate this from the spatial
     * distribution of membrane potentials.
     *
     * For each axis, we compute the weighted voltage difference across
     * that axis dimension.
     */

    /* Compute centroid of neural population */
    float centroid[3] = {0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < system->neuron_count; i++) {
        centroid[0] += system->neurons[i].position[0];
        centroid[1] += system->neurons[i].position[1];
        centroid[2] += system->neurons[i].position[2];
    }
    centroid[0] /= (float)system->neuron_count;
    centroid[1] /= (float)system->neuron_count;
    centroid[2] /= (float)system->neuron_count;

    /* Compute field as weighted voltage gradient */
    float field_x = 0.0f, field_y = 0.0f, field_z = 0.0f;
    float norm_sum = 0.0f;

    for (uint32_t i = 0; i < system->neuron_count; i++) {
        nimcp_ephaptic_neuron_t* n = &system->neurons[i];

        float voltage_diff = n->membrane_potential - V_REST;
        float dist = compute_distance(centroid, n->position);
        if (dist < EPSILON) dist = EPSILON;

        float decay = safe_exp(-dist * system->config.field_decay_constant);
        float weight = decay / dist;
        norm_sum += weight;

        /* Direction from centroid to neuron */
        float dx = n->position[0] - centroid[0];
        float dy = n->position[1] - centroid[1];
        float dz = n->position[2] - centroid[2];

        /* Field contribution: -grad(V) direction weighted by voltage */
        field_x -= voltage_diff * weight * dx / dist;
        field_y -= voltage_diff * weight * dy / dist;
        field_z -= voltage_diff * weight * dz / dist;
    }

    /* Normalize and scale */
    if (norm_sum > EPSILON) {
        float scale = system->config.coupling_strength / norm_sum;
        field_x *= scale;
        field_y *= scale;
        field_z *= scale;
    }

    /* Clamp to physical limits */
    field_x = nimcp_clampf(field_x, -EPHAPTIC_MAX_FIELD_STRENGTH, EPHAPTIC_MAX_FIELD_STRENGTH);
    field_y = nimcp_clampf(field_y, -EPHAPTIC_MAX_FIELD_STRENGTH, EPHAPTIC_MAX_FIELD_STRENGTH);
    field_z = nimcp_clampf(field_z, -EPHAPTIC_MAX_FIELD_STRENGTH, EPHAPTIC_MAX_FIELD_STRENGTH);

    /* Apply temporal smoothing (LFP-like decay) */
    float alpha = dt / (system->config.lfp_tau + dt);
    system->field_strength[0] = system->field_strength[0] * (1.0f - alpha) + field_x * alpha;
    system->field_strength[1] = system->field_strength[1] * (1.0f - alpha) + field_y * alpha;
    system->field_strength[2] = system->field_strength[2] * (1.0f - alpha) + field_z * alpha;

    /* Store field direction */
    system->field_direction[0] = system->field_strength[0];
    system->field_direction[1] = system->field_strength[1];
    system->field_direction[2] = system->field_strength[2];
    normalize_vector(system->field_direction);

    /* Compute induced current (I = sigma * E) */
    float field_mag = sqrtf(
        system->field_strength[0] * system->field_strength[0] +
        system->field_strength[1] * system->field_strength[1] +
        system->field_strength[2] * system->field_strength[2]
    );
    system->field_induced_current = field_mag * 1000.0f /
                                     system->config.extracellular_resistivity;

    /* Magnetic field (optional, computationally expensive) */
    if (system->config.enable_magnetic_field) {
        /*
         * BIOLOGICAL: Magnetic field from neural currents
         * B = mu_0 / (4*pi) * I * dl x r / r^3
         *
         * Very weak (~fT to pT) but measurable with MEG
         */
        float mu_0 = 1.257e-6f;  /* Permeability of free space */
        float current_scale = 1e-12f;  /* pA to A */

        /* Simplified: assume current along mean field direction */
        system->magnetic_field[0] = mu_0 * system->field_induced_current *
                                     current_scale * system->field_direction[1];
        system->magnetic_field[1] = mu_0 * system->field_induced_current *
                                     current_scale * (-system->field_direction[0]);
        system->magnetic_field[2] = 0.0f;  /* Assume 2D current flow */

        /* Clamp magnetic field */
        system->magnetic_field[0] = nimcp_clampf(system->magnetic_field[0],
                                             -EPHAPTIC_MAX_MAGNETIC_FIELD,
                                             EPHAPTIC_MAX_MAGNETIC_FIELD);
        system->magnetic_field[1] = nimcp_clampf(system->magnetic_field[1],
                                             -EPHAPTIC_MAX_MAGNETIC_FIELD,
                                             EPHAPTIC_MAX_MAGNETIC_FIELD);
    }

    /* Update simulation time */
    system->time += dt;

    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_synchronize(
    nimcp_ephaptic_system_t* system,
    float dt
) {
    /* Guard: null pointer */
    if (!system) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    /* Guard: invalid timestep */
    if (dt <= 0.0f) {
        return EPHAPTIC_ERROR_INVALID_PARAMETER;
    }

    /* No neurons to synchronize */
    if (system->neuron_count < 2) {
        system->phase_locking_strength = 0.0f;
        system->synchronized_neurons = system->neuron_count;
        return NIMCP_OK;
    }

    /*
     * BIOLOGICAL: Kuramoto model for phase synchronization
     *
     * d(theta_i)/dt = omega_i + (K/N) * sum_j [ sin(theta_j - theta_i) ]
     *
     * where:
     *   theta_i = phase of oscillator i
     *   omega_i = natural frequency of oscillator i
     *   K = coupling strength
     *   N = number of oscillators
     *
     * The ephaptic field provides additional coupling proportional to
     * the field strength and neuron susceptibility.
     */

    float K = system->config.kuramoto_coupling * system->config.coupling_strength;
    float N = (float)system->neuron_count;

    /* Compute mean field (order parameter) for efficient coupling */
    float mean_real = 0.0f;
    float mean_imag = 0.0f;

    for (uint32_t i = 0; i < system->neuron_count; i++) {
        mean_real += cosf(system->neurons[i].phase);
        mean_imag += sinf(system->neurons[i].phase);
    }
    mean_real /= N;
    mean_imag /= N;

    /* Order parameter magnitude (coherence) */
    float r = sqrtf(mean_real * mean_real + mean_imag * mean_imag);
    float mean_phase = atan2f(mean_imag, mean_real);

    /* Store order parameter */
    system->order_parameter_real = mean_real;
    system->order_parameter_imag = mean_imag;
    system->phase_locking_strength = r;

    /* Update each neuron's phase */
    for (uint32_t i = 0; i < system->neuron_count; i++) {
        nimcp_ephaptic_neuron_t* n = &system->neurons[i];

        /* Natural oscillation */
        float d_theta = n->natural_frequency * NIMCP_TWO_PI_F * dt * 0.001f;  /* dt in ms */

        /* Kuramoto coupling: K * r * sin(mean_phase - theta_i) */
        float coupling = K * r * sinf(mean_phase - n->phase);
        d_theta += coupling * dt * 0.001f;

        /* Field-mediated coupling: enhance coupling based on field strength */
        float field_mag = sqrtf(
            system->field_strength[0] * system->field_strength[0] +
            system->field_strength[1] * system->field_strength[1] +
            system->field_strength[2] * system->field_strength[2]
        );
        float field_coupling = field_mag * n->field_susceptibility * 0.01f;
        d_theta += field_coupling * sinf(mean_phase - n->phase) * dt * 0.001f;

        /* Update phase */
        n->phase = wrap_phase(n->phase + d_theta);
    }

    /* Count synchronized neurons (phase deviation < threshold) */
    uint32_t sync_count = 0;
    float threshold_rad = 0.5f;  /* ~30 degrees */

    for (uint32_t i = 0; i < system->neuron_count; i++) {
        float phase_diff = fabsf(system->neurons[i].phase - mean_phase);
        if (phase_diff > 3.14159f) phase_diff = NIMCP_TWO_PI_F - phase_diff;

        if (phase_diff < threshold_rad) {
            sync_count++;
        }
    }
    system->synchronized_neurons = sync_count;

    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_get_phase_coherence(
    const nimcp_ephaptic_system_t* system,
    float* coherence
) {
    /* Guard: null pointers */
    if (!system || !coherence) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    *coherence = system->phase_locking_strength;
    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_modulate_neuron(
    const nimcp_ephaptic_system_t* system,
    uint32_t neuron_id,
    float* polarization
) {
    /* Guard: null pointers */
    if (!system || !polarization) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    /* Find neuron (const-safe by casting for lookup only) */
    nimcp_ephaptic_neuron_t* neuron = find_neuron(
        (nimcp_ephaptic_system_t*)system,
        neuron_id
    );
    if (!neuron) {
        return EPHAPTIC_ERROR_INVALID_PARAMETER;
    }

    /*
     * BIOLOGICAL: Field-induced membrane polarization
     *
     * The extracellular field induces a change in membrane potential
     * proportional to the field strength projected onto the cell's axis.
     *
     * delta_V = E * L * cos(theta)
     *
     * where:
     *   E = field strength
     *   L = effective membrane length constant (~100 um)
     *   theta = angle between field and cell axis
     *
     * For simplicity, we use a scalar susceptibility factor.
     */

    float field_mag = sqrtf(
        system->field_strength[0] * system->field_strength[0] +
        system->field_strength[1] * system->field_strength[1] +
        system->field_strength[2] * system->field_strength[2]
    );

    /* Convert V/m to mV over typical cell length (~0.1 mm) */
    float cell_length_mm = 0.1f;
    float field_effect = field_mag * cell_length_mm;

    /* Apply susceptibility and coupling strength */
    *polarization = field_effect * neuron->field_susceptibility *
                    system->config.coupling_strength;

    /* Clamp to reasonable range (ephaptic effects are typically < 1 mV) */
    *polarization = nimcp_clampf(*polarization, -2.0f, 2.0f);

    return NIMCP_OK;
}

//=============================================================================
// Query API Implementation
//=============================================================================

nimcp_error_t nimcp_ephaptic_get_field(
    const nimcp_ephaptic_system_t* system,
    float field[3]
) {
    /* Guard: null pointers */
    if (!system || !field) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    field[0] = system->field_strength[0];
    field[1] = system->field_strength[1];
    field[2] = system->field_strength[2];

    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_get_potential(
    const nimcp_ephaptic_system_t* system,
    float* potential
) {
    /* Guard: null pointers */
    if (!system || !potential) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    *potential = system->field_potential;
    return NIMCP_OK;
}

nimcp_error_t nimcp_ephaptic_get_sync_count(
    const nimcp_ephaptic_system_t* system,
    uint32_t* count
) {
    /* Guard: null pointers */
    if (!system || !count) {
        return EPHAPTIC_ERROR_NULL_POINTER;
    }

    /* Guard: not initialized */
    if (!system->initialized) {
        return EPHAPTIC_ERROR_NOT_INITIALIZED;
    }

    *count = system->synchronized_neurons;
    return NIMCP_OK;
}

bool nimcp_ephaptic_is_initialized(const nimcp_ephaptic_system_t* system) {
    if (!system) {
        return false;
    }
    return system->initialized;
}

/*=============================================================================
 * W15: Ephaptic aggregated KG emission + read path
 *===========================================================================*/

#define EPHAPTIC_KG_ROOT_NAME "ephaptic_runtime"

static brain_t s_ephaptic_kg_brain = NULL;

void nimcp_ephaptic_kg_register_brain(brain_t brain) {
    s_ephaptic_kg_brain = brain;
}

void nimcp_ephaptic_kg_trigger_field_event(brain_t brain,
                                           const char* kind,
                                           float lfp_amplitude_mv,
                                           float phase_coherence,
                                           uint64_t ts_us) {
    if (!brain) brain = s_ephaptic_kg_brain;
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    if (!kind) return;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    /* Ensure runtime root exists (idempotent). */
    brain_kg_node_id_t root = brain_kg_find_node(kg, EPHAPTIC_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) {
        root = brain_kg_add_node(kg, EPHAPTIC_KG_ROOT_NAME,
            BRAIN_KG_NODE_CORE,
            "Ephaptic coupling runtime aggregator - LFP/field events");
        brain_kg_node_id_t phys = brain_kg_find_node(kg, "ephaptic_coupling");
        if (root != BRAIN_KG_INVALID_NODE && phys != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, phys, root,
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "runtime aggregator for ephaptic subsystem", 1.0f);
        }
    }

    char ev_name[160];
    snprintf(ev_name, sizeof(ev_name),
             "ephaptic_event_%s_%llu",
             kind, (unsigned long long)ts_us);
    char desc[240];
    snprintf(desc, sizeof(desc),
             "Ephaptic aggregated event: kind=%s lfp=%.4fmV coherence=%.3f",
             kind, lfp_amplitude_mv, phase_coherence);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, ev_name,
        BRAIN_KG_NODE_CORE, desc);
    if (ev != BRAIN_KG_INVALID_NODE && root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, root, ev, BRAIN_KG_EDGE_SENDS_TO,
            "field_event",
            phase_coherence > 0.0f ? phase_coherence : 1.0f);
    }
    if (ev != BRAIN_KG_INVALID_NODE) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.4f mV", lfp_amplitude_mv);
        brain_kg_add_metadata(kg, ev, "lfp_amplitude", buf);
        snprintf(buf, sizeof(buf), "%.4f", phase_coherence);
        brain_kg_add_metadata(kg, ev, "phase_coherence", buf);
        brain_kg_add_metadata(kg, ev, "kind", kind);
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts_us);
        brain_kg_add_metadata(kg, ev, "ts_us", buf);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

uint32_t nimcp_ephaptic_kg_count_events(struct brain_kg* kg_opaque,
                                        const char* kind_substr) {
    brain_kg_t* kg = (brain_kg_t*)kg_opaque;
    if (!kg) return 0;
    brain_kg_node_id_t root = brain_kg_find_node(kg, EPHAPTIC_KG_ROOT_NAME);
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out = brain_kg_get_outgoing(kg, root);
    if (!out) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < out->count; ++i) {
        const brain_kg_edge_t* e = out->edges[i];
        if (!e) continue;
        const brain_kg_node_t* to = brain_kg_get_node(kg, e->to);
        if (!to) continue;
        if (!kind_substr || !*kind_substr) {
            count++;
        } else if ((to->name && strstr(to->name, kind_substr)) ||
                   (to->description && strstr(to->description, kind_substr))) {
            count++;
        }
    }
    brain_kg_edge_list_destroy(out);
    return count;
}

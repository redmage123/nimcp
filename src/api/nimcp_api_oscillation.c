/**
 * @file nimcp_api_oscillation.c
 * @brief Brain oscillation and complex number phasor API implementation
 *
 * This module handles complex oscillation features including:
 * - Oscillation enable/disable and status
 * - Phasor extraction for neurons
 * - Phase coherence computation
 * - Phase-amplitude coupling (PAC) analysis
 *
 * Responsibilities:
 * - Complex oscillation management
 * - Neural phasor operations
 * - Coherence and PAC metrics
 */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "API_OSCILLATION"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(api_oscillation)

/* API Exception Integration (Phase 7) */
extern void set_error(const char* fmt, ...);
#define NIMCP_API_SET_ERROR(fmt, ...) set_error(fmt, ##__VA_ARGS__)
#include "api/nimcp_api_exception.h"

#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/oscillations/nimcp_brain_complex_oscillations.h"
#include "utils/memory/nimcp_memory.h"
#include <stdio.h>
#include <string.h>

//=============================================================================
// External References (from nimcp.c)
//=============================================================================

// These functions are defined in nimcp.c and shared across modules
extern void set_error(const char* fmt, ...);
extern const char* nimcp_get_error(void);

//=============================================================================
// Complex Number & Oscillation API Implementation
//=============================================================================

bool nimcp_enable_complex_oscillations(nimcp_brain_t brain, bool enable) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain handle is NULL in enable_complex_oscillations");
        return false;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain has NULL internal_brain in enable_complex_oscillations");
        return false;
    }

    // Get brain's internal structure to access complex oscillation state
    // Note: This requires brain_internal_t access, which would need to be exposed
    // For now, we'll use a hypothetical brain configuration function
    // This would typically be: brain_config_set_complex_oscillations(brain->internal_brain, enable)

    // Since we may not have direct config access, we'll check if it's already enabled
    // and return true if enabling and already enabled, or if disabling and already disabled
    bool currently_enabled = brain_complex_oscillation_is_enabled(brain->internal_brain);

    if (enable == currently_enabled) {
        return true;
    }

    // For actual enable/disable, we would need brain_config_t access
    // This is a placeholder implementation that at least validates the state
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_enable_complex_oscillations: validation failed");
    return false;
}

bool nimcp_is_complex_oscillations_enabled(nimcp_brain_t brain) {
    if (!brain || !brain->internal_brain) {
        return false;
    }

    return brain_complex_oscillation_is_enabled(brain->internal_brain);
}

nimcp_oscillation_phasor_t nimcp_get_oscillation_phasor(
    nimcp_brain_t brain,
    uint32_t neuron_id)
{
    nimcp_oscillation_phasor_t result = {0.0f, 0.0f};

    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL brain provided to get_oscillation_phasor");
        return result;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain has NULL internal_brain in get_oscillation_phasor");
        return result;
    }

    // Check if complex oscillations are enabled
    if (!brain_complex_oscillation_is_enabled(brain->internal_brain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Complex oscillations not enabled");
        return result;
    }

    // Get brain's oscillation analyzer
    brain_oscillation_analyzer_t* analyzer = brain_get_oscillations(brain->internal_brain);
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Brain oscillation analyzer not available");
        return result;
    }

    // Access complex oscillation state from analyzer
    // Note: This assumes brain_oscillation_analyzer_t has a complex_state field
    // The actual implementation would need to extract this from the analyzer structure
    // For now, we'll use a hypothetical accessor function
    brain_complex_oscillation_state_t* complex_state =
        (brain_complex_oscillation_state_t*)analyzer;  // Placeholder cast

    // Get neuron phasor
    neural_phasor_t phasor;
    if (!brain_complex_oscillation_get_phasor(complex_state, neuron_id, &phasor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid neuron ID %u or failed to get phasor", neuron_id);
        return result;
    }

    // Convert neural_phasor_t to nimcp_oscillation_phasor_t
    result.amplitude = phasor_amplitude(phasor);
    result.phase = phasor_phase(phasor);

    return result;
}

float nimcp_get_phase_coherence(
    nimcp_brain_t brain,
    const uint32_t* neuron_ids,
    uint32_t count)
{
    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL brain provided to get_phase_coherence");
        return 0.0f;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain has NULL internal_brain in get_phase_coherence");
        return 0.0f;
    }

    if (!neuron_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL neuron_ids provided to get_phase_coherence");
        return 0.0f;
    }

    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Invalid count (0) provided to get_phase_coherence");
        return 0.0f;
    }

    // Check if complex oscillations are enabled
    if (!brain_complex_oscillation_is_enabled(brain->internal_brain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Complex oscillations not enabled in get_phase_coherence");
        return 0.0f;
    }

    // Get brain's oscillation analyzer
    brain_oscillation_analyzer_t* analyzer = brain_get_oscillations(brain->internal_brain);
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Brain oscillation analyzer not available");
        return 0.0f;
    }

    // Access complex oscillation state
    brain_complex_oscillation_state_t* complex_state =
        (brain_complex_oscillation_state_t*)analyzer;

    // Compute phase coherence for neuron subset
    phase_coherence_result_t result;
    if (!brain_complex_oscillation_compute_coherence_subset(
            complex_state, neuron_ids, count, &result)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to compute phase coherence");
        return 0.0f;
    }

    return result.coherence;
}

float nimcp_get_pac_modulation(
    nimcp_brain_t brain,
    float theta_freq,
    float gamma_freq)
{
    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL brain provided to get_pac_modulation");
        return 0.0f;
    }

    if (!brain->internal_brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Brain has NULL internal_brain in get_pac_modulation");
        return 0.0f;
    }

    // Validate frequency ranges
    if (theta_freq < 4.0f || theta_freq > 8.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Theta frequency %.2f out of range 4-8 Hz", theta_freq);
        return 0.0f;
    }

    if (gamma_freq < 30.0f || gamma_freq > 100.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Gamma frequency %.2f out of range 30-100 Hz", gamma_freq);
        return 0.0f;
    }

    // Check if complex oscillations are enabled
    if (!brain_complex_oscillation_is_enabled(brain->internal_brain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Complex oscillations not enabled in get_pac_modulation");
        return 0.0f;
    }

    // Get brain's oscillation analyzer
    brain_oscillation_analyzer_t* analyzer = brain_get_oscillations(brain->internal_brain);
    if (!analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Brain oscillation analyzer not available");
        return 0.0f;
    }

    // Access complex oscillation state
    brain_complex_oscillation_state_t* complex_state =
        (brain_complex_oscillation_state_t*)analyzer;

    // For PAC computation, we need to:
    // 1. Extract theta-band phase neurons
    // 2. Extract gamma-band amplitude values
    // 3. Compute PAC modulation index

    // This requires access to brain's neural network to filter by frequency bands
    // For now, we'll provide a simplified implementation that uses all neurons
    uint32_t num_neurons = brain_complex_oscillation_get_num_neurons(complex_state);

    if (num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "No neurons in complex oscillation state");
        return 0.0f;
    }

    // Allocate arrays for phase and amplitude
    uint32_t* phase_indices = (uint32_t*)nimcp_malloc(num_neurons * sizeof(uint32_t));
    float* amplitude_values = (float*)nimcp_malloc(num_neurons * sizeof(float));

    if (!phase_indices || !amplitude_values) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_neurons * sizeof(uint32_t),
            "Failed to allocate memory for PAC computation");
        if (phase_indices) nimcp_free(phase_indices);
        if (amplitude_values) nimcp_free(amplitude_values);
        return 0.0f;
    }

    // Populate indices and extract amplitude values
    for (uint32_t i = 0; i < num_neurons; i++) {
        phase_indices[i] = i;

        neural_phasor_t phasor;
        if (brain_complex_oscillation_get_phasor(complex_state, i, &phasor)) {
            amplitude_values[i] = phasor_amplitude(phasor);
        } else {
            amplitude_values[i] = 0.0f;
        }
    }

    // Compute PAC
    pac_result_t pac_result;
    bool success = brain_complex_oscillation_compute_pac(
        complex_state,
        phase_indices,
        num_neurons,
        amplitude_values,
        num_neurons,
        &pac_result
    );

    // Cleanup
    nimcp_free(phase_indices);
    nimcp_free(amplitude_values);

    if (!success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to compute PAC modulation index");
        return 0.0f;
    }

    return pac_result.modulation_index;
}

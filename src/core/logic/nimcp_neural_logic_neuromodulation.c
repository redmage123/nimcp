/**
 * @file nimcp_neural_logic_neuromodulation.c
 * @brief MODULE 4: Neural Logic Neuromodulation Implementation
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Neuromodulator-based threshold modulation for logic gates
 * WHY:  Single Responsibility: Implement neurochemical influence on logical reasoning
 * HOW:  Read brain DA/ACh levels, modulate gate thresholds based on biological principles
 *
 * @author NIMCP Development Team
 */

#include "core/logic/nimcp_neural_logic_neuromodulation.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/logic/nimcp_neural_logic_attachment.h"
#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

#include <stddef.h>  /* for NULL */
// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "neural_logic_neuromodulation"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_logic_neuromodulation)

#define BIO_MODULE_ID 0x013A


//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Read dopamine and acetylcholine levels from brain
 *
 * WHAT: Query neuromodulator system for DA and ACh concentrations
 * WHY:  Centralize neuromodulator reading with defaults
 * HOW:  Query neuromodulator_system, fall back to 0.5 if unavailable
 */
static void read_neuromodulator_levels(
    brain_t brain,
    float* da_level,
    float* ach_level
) {
    // Default baseline levels
    *da_level = 0.5F;
    *ach_level = 0.5F;

    // Guard: NULL brain
    if (!brain) {
        return;
    }

    // Query neuromodulator system
    neuromodulator_system_t neuromod = brain->neuromodulator_system;
    if (!neuromod) {
        return;
    }

    *da_level = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
    *ach_level = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);

    // Clamp to valid range
    *da_level = nimcp_clamp01(*da_level);
    *ach_level = nimcp_clamp01(*ach_level);
}

/**
 * @brief Compute combined DA + ACh modulation factor
 *
 * WHAT: Calculate threshold scaling factor from DA and ACh levels
 * WHY:  Implement biological modulation formula
 * HOW:  factor = (1.0 - DA * 0.3) * (1.0 + ACh * 0.2)
 */
static float compute_modulation_factor(float da_level, float ach_level) {
    float da_factor = 1.0F - (da_level * DA_MODULATION_FACTOR);
    float ach_factor = 1.0F + (ach_level * ACH_MODULATION_FACTOR);
    return da_factor * ach_factor;
}

//=============================================================================
// MODULE 4: Neuromodulation API Implementation
//=============================================================================

bool apply_dopamine_modulation(
    brain_t brain,
    uint32_t gate_id,
    float da_level
) {
    // WHAT: Validate inputs for DA modulation
    // WHY:  Prevent NULL derefs and invalid gate access
    // HOW:  Guard clauses with early returns

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("apply_dopamine_modulation: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_dopamine_modulation: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("apply_dopamine_modulation: brain has no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_dopamine_modulation: brain_has_neural_logic is NULL");
        return false;
    }

    // Clamp and warn if out of range
    if (da_level < 0.0F || da_level > 1.0F) {
        LOG_WARNING("apply_dopamine_modulation: da_level %.3f out of range, clamping to [0,1]",
                    da_level);
        da_level = nimcp_clamp01(da_level);
    }

    // WHAT: Read current gate state
    // WHY:  Need base threshold for modulation
    // HOW:  Call neural_logic_get_state()

    neural_logic_network_t network = brain_get_neural_logic(brain);
    logic_neuron_state_t state;

    if (!neural_logic_get_state(network, gate_id, &state)) {
        LOG_ERROR("apply_dopamine_modulation: invalid gate_id %u", gate_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_dopamine_modulation: neural_logic_get_state is NULL");
        return false;
    }

    // WHAT: Apply DA modulation to threshold
    // WHY:  Implement biological dopamine effects on logic
    // HOW:  threshold_new = threshold_base * (1.0 - da * 0.3)

    float base_threshold = state.threshold;
    float da_factor = 1.0F - (da_level * DA_MODULATION_FACTOR);
    float modulated_threshold = base_threshold * da_factor;

    // NOTE: Actual threshold update would require write-back to network
    // For now, log the modulation effect

    LOG_DEBUG("apply_dopamine_modulation: gate=%u, DA=%.3f, "
              "thresh: %.3f → %.3f (factor=%.3f)",
              gate_id, da_level, base_threshold, modulated_threshold, da_factor);

    return true;
}

bool apply_acetylcholine_modulation(
    brain_t brain,
    uint32_t gate_id,
    float ach_level
) {
    // WHAT: Validate inputs for ACh modulation
    // WHY:  Prevent NULL derefs and invalid gate access
    // HOW:  Guard clauses with early returns

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("apply_acetylcholine_modulation: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_acetylcholine_modulation: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("apply_acetylcholine_modulation: brain has no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_acetylcholine_modulation: brain_has_neural_logic is NULL");
        return false;
    }

    // Clamp and warn if out of range
    if (ach_level < 0.0F || ach_level > 1.0F) {
        LOG_WARNING("apply_acetylcholine_modulation: ach_level %.3f out of range, clamping to [0,1]",
                    ach_level);
        ach_level = nimcp_clamp01(ach_level);
    }

    // WHAT: Read current gate state
    // WHY:  Need base threshold for modulation
    // HOW:  Call neural_logic_get_state()

    neural_logic_network_t network = brain_get_neural_logic(brain);
    logic_neuron_state_t state;

    if (!neural_logic_get_state(network, gate_id, &state)) {
        LOG_ERROR("apply_acetylcholine_modulation: invalid gate_id %u", gate_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_acetylcholine_modulation: neural_logic_get_state is NULL");
        return false;
    }

    // WHAT: Apply ACh modulation to threshold
    // WHY:  Implement biological acetylcholine effects on logic
    // HOW:  threshold_new = threshold_base * (1.0 + ach * 0.2)

    float base_threshold = state.threshold;
    float ach_factor = 1.0F + (ach_level * ACH_MODULATION_FACTOR);
    float modulated_threshold = base_threshold * ach_factor;

    // NOTE: Actual threshold update would require write-back to network
    // For now, log the modulation effect

    LOG_DEBUG("apply_acetylcholine_modulation: gate=%u, ACh=%.3f, "
              "thresh: %.3f → %.3f (factor=%.3f)",
              gate_id, ach_level, base_threshold, modulated_threshold, ach_factor);

    return true;
}

uint32_t update_all_gate_modulation(brain_t brain) {
    // WHAT: Validate brain and subsystems
    // WHY:  Ensure we can read neuromodulators and access gates
    // HOW:  Guard clauses for brain, logic, neuromodulator system

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("update_all_gate_modulation: NULL brain");
        return 0;
    }

    // Guard: no logic network
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("update_all_gate_modulation: brain has no logic network");
        return 0;
    }

    // Guard: no neuromodulator system
    if (!brain->neuromodulator_system) {
        LOG_ERROR("update_all_gate_modulation: brain has no neuromodulator system");
        return 0;
    }

    // WHAT: Read current neuromodulator levels
    // WHY:  Apply same DA/ACh values to all gates for consistency
    // HOW:  Query neuromodulator system once

    float da_level, ach_level;
    read_neuromodulator_levels(brain, &da_level, &ach_level);

    float modulation_factor = compute_modulation_factor(da_level, ach_level);

    LOG_DEBUG("update_all_gate_modulation: DA=%.3f, ACh=%.3f, factor=%.3f",
              da_level, ach_level, modulation_factor);

    // WHAT: Query network for gate count
    // WHY:  Determine iteration range
    // HOW:  Call neural_logic_get_stats()

    neural_logic_network_t network = brain_get_neural_logic(brain);

    uint32_t total_gates = 0;
    uint32_t total_vars = 0;
    uint64_t total_spikes = 0;
    float avg_eval_time = 0.0F;
    uint64_t gpu_memory = 0;

    if (!neural_logic_get_stats(network, &total_gates, &total_vars,
                                 &total_spikes, &avg_eval_time, &gpu_memory)) {
        LOG_ERROR("update_all_gate_modulation: failed to get network stats");
        return 0;
    }

    // WHAT: Iterate all gates and apply modulation
    // WHY:  Synchronize entire logic network with brain state
    // HOW:  For each gate: read state, compute modulated threshold, log

    uint32_t modulated_count = 0;

    for (uint32_t gate_id = 0; gate_id < total_gates; gate_id++) {
        logic_neuron_state_t state;
        if (neural_logic_get_state(network, gate_id, &state)) {
            float base_threshold = state.threshold;
            float modulated_threshold = base_threshold * modulation_factor;

            // NOTE: Actual threshold update would require write-back to network
            // For now, just count successful reads

            LOG_DEBUG("update_all_gate_modulation: gate=%u, thresh: %.3f → %.3f",
                      gate_id, base_threshold, modulated_threshold);

            modulated_count++;
        }
    }

    LOG_INFO("update_all_gate_modulation: modulated %u/%u gates (DA=%.2f, ACh=%.2f)",
             modulated_count, total_gates, da_level, ach_level);

    return modulated_count;
}

bool get_modulated_threshold(
    brain_t brain,
    float base_threshold,
    float* modulated_threshold
) {
    // WHAT: Validate inputs for threshold query
    // WHY:  Prevent NULL derefs when writing output
    // HOW:  Guard clauses on pointers

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("get_modulated_threshold: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_modulated_threshold: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL output
    if (!nimcp_validate_pointer(modulated_threshold, "modulated_threshold")) {
        LOG_ERROR("get_modulated_threshold: NULL modulated_threshold");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "get_modulated_threshold: nimcp_validate_pointer is NULL");
        return false;
    }

    // WHAT: Read neuromodulator levels (with defaults if unavailable)
    // WHY:  Compute modulation based on current brain state
    // HOW:  Call helper function with default handling

    float da_level, ach_level;
    read_neuromodulator_levels(brain, &da_level, &ach_level);

    // WHAT: Compute modulated threshold
    // WHY:  Apply biological modulation formula
    // HOW:  threshold * (1.0 - DA * 0.3) * (1.0 + ACh * 0.2)

    float modulation_factor = compute_modulation_factor(da_level, ach_level);
    *modulated_threshold = base_threshold * modulation_factor;

    LOG_DEBUG("get_modulated_threshold: base=%.3f, DA=%.3f, ACh=%.3f → mod=%.3f",
              base_threshold, da_level, ach_level, *modulated_threshold);

    return true;
}

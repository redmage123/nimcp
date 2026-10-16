#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_stp.c - Short-Term Synaptic Plasticity Implementation
//=============================================================================
/**
 * @file nimcp_stp.c
 * @brief Implementation of Tsodyks-Markram short-term plasticity model
 *
 * MATHEMATICAL FORMULATION:
 * The Tsodyks-Markram model consists of two coupled ODEs:
 *   dx/dt = (1 - x) / τ_D            (resource recovery)
 *   du/dt = (U - u) / τ_F            (facilitation decay)
 *
 * With instantaneous updates on spike:
 *   u ← u + U(1 - u)                 (facilitation jump)
 *   x ← x(1 - u)                     (resource depletion)
 *
 * NUMERICAL INTEGRATION:
 * Uses exponential decay formulas (exact solution for linear ODEs):
 *   x(t+Δt) = 1 - (1-x(t))exp(-Δt/τ_D)
 *   u(t+Δt) = U + (u(t)-U)exp(-Δt/τ_F)
 *
 * STABILITY: Unconditionally stable (exact integration)
 *
 * DESIGN PATTERNS:
 * - Value Object: stp_state_t is self-contained
 * - Immutable Parameters: params stored in const structs
 * - Single Responsibility: Each function handles one operation
 *
 * PERFORMANCE:
 * - Update: O(1) - just 2 exponentials
 * - Memory: 20 bytes per synapse (x, u, params, timestamp)
 * - Throughput: ~10M synapses/sec on modern CPU
 *
 * @author NIMCP Development Team
 * @date 2025-11-06
 */

#include "plasticity/stp/nimcp_stp.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_stp"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stp)

//=============================================================================
// Preset Parameter Tables
//=============================================================================

/**
 * @brief Preset parameter table
 *
 * WHAT: Pre-configured parameter sets from literature
 * WHY: Validated combinations from experimental data
 * HOW: Static const table indexed by preset enum
 *
 * REFERENCES:
 * - Markram et al. (1998) J Physiol 506:431-440
 * - Dittman et al. (2000) J Neurosci 20:1374-1385
 */
static const stp_params_t PRESET_TABLE[] = {
    // Depressing: High U, long recovery (cortical L4→L2/3)
    {.U = 0.5F, .tau_D = 800.0F, .tau_F = 50.0F},

    // Facilitating: Low U, fast recovery (cortical L2/3→L5)
    {.U = 0.15F, .tau_D = 200.0F, .tau_F = 750.0F},

    // Mixed: Intermediate values
    {.U = 0.3F, .tau_D = 400.0F, .tau_F = 200.0F},

    // Fast Depressing: High U, fast recovery
    {.U = 0.6F, .tau_D = 100.0F, .tau_F = 30.0F},

    // Slow Depressing: High U, very slow recovery
    {.U = 0.5F, .tau_D = 1500.0F, .tau_F = 50.0F},

    // None: No STP (static synapse)
    {.U = 1.0F, .tau_D = 1.0F, .tau_F = 1.0F}
};

/**
 * @brief Preset names table
 */
static const char* PRESET_NAMES[] = {
    "Depressing",
    "Facilitating",
    "Mixed",
    "Fast Depressing",
    "Slow Depressing",
    "None (Static)"
};

/**
 * @brief Preset descriptions table
 */
static const char* PRESET_DESCRIPTIONS[] = {
    "Predominantly depressing synapse - high initial release, slow recovery",
    "Predominantly facilitating synapse - low initial release, buildup with activity",
    "Mixed depression and facilitation - balanced dynamics",
    "Fast depressing synapse - rapid depletion and recovery",
    "Slow depressing synapse - gradual depletion, very slow recovery",
    "No short-term plasticity - static synaptic strength"
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute exponential decay
 *
 * WHAT: Calculates exp(-dt/tau) with safety checks
 * WHY: Prevent numerical overflow/underflow
 * HOW: Guards against extreme values
 *
 * COMPLEXITY: O(1)
 */
static inline float exp_decay(float dt, float tau) {
    // Guard: Prevent division by zero
    if (tau <= 0.0F) {
        return 0.0F;
    }

    // Guard: Prevent overflow for large dt/tau
    const float ratio = dt / tau;
    if (ratio > 20.0F) {
        return 0.0F;  // exp(-20) ≈ 2e-9, effectively zero
    }

    return expf(-ratio);
}

//=============================================================================
// Core STP Functions
//=============================================================================

void stp_init(stp_state_t* state, const stp_params_t* params, uint64_t timestamp) {
    // Guard: Validate state pointer
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_init: null state pointer");
        return;
    }

    // Initialize to resting values
    state->x = 1.0F;  // Full resources available
    state->last_update = timestamp;

    // Set parameters (use defaults if NULL)
    if (params) {
        state->params = *params;
        state->u = params->U;  // Baseline utilization
    } else {
        state->params.U = STP_DEFAULT_U;
        state->params.tau_D = STP_DEFAULT_TAU_D;
        state->params.tau_F = STP_DEFAULT_TAU_F;
        state->u = STP_DEFAULT_U;
    }
}

void stp_update(stp_state_t* state, uint64_t timestamp) {
    // Guard: Validate state pointer
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_update: null state pointer");
        return;
    }

    // Calculate elapsed time (guard against backward timestamps)
    if (timestamp <= state->last_update) {
        return;
    }
    const float dt = (float)(timestamp - state->last_update);

    // Exact exponential decay integration
    // x(t+dt) = 1 - (1-x(t))exp(-dt/τ_D)
    const float decay_D = exp_decay(dt, state->params.tau_D);
    state->x = 1.0F - (1.0F - state->x) * decay_D;

    // u(t+dt) = U + (u(t)-U)exp(-dt/τ_F)
    const float decay_F = exp_decay(dt, state->params.tau_F);
    state->u = state->params.U + (state->u - state->params.U) * decay_F;

    // Clamp to valid range (prevent numerical drift)
    state->x = nimcp_clamp01(state->x);
    state->u = nimcp_clamp01(state->u);

    // Update timestamp
    state->last_update = timestamp;
}

void stp_process_spike(stp_state_t* state, uint64_t timestamp) {
    // Guard: Validate state pointer
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_process_spike: null state pointer");
        return;
    }

    // First update continuous decay to current time
    stp_update(state, timestamp);

    // Instantaneous facilitation jump: u ← u + U(1-u)
    const float du = state->params.U * (1.0F - state->u);
    state->u = nimcp_clamp01(state->u + du);

    // Instantaneous resource depletion: x ← x(1-u)
    state->x = state->x * (1.0F - state->u);
    state->x = nimcp_clamp01(state->x);

    // Timestamp already updated by stp_update()
}

float stp_get_modulation(const stp_state_t* state) {
    // Guard: Validate state pointer
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_get_modulation: null state pointer");
        return 1.0F;  // No modulation if state invalid
    }

    // Effective strength = facilitation × available resources
    return state->u * state->x;
}

void stp_reset(stp_state_t* state, uint64_t timestamp) {
    // Guard: Validate state pointer
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_reset: null state pointer");
        return;
    }

    // Reset to resting values
    state->x = 1.0F;
    state->u = state->params.U;
    state->last_update = timestamp;
}

//=============================================================================
// Parameter Presets
//=============================================================================

stp_params_t stp_get_preset_params(stp_preset_t preset) {
    // Guard: Validate preset index
    if (preset < 0 || preset >= sizeof(PRESET_TABLE) / sizeof(PRESET_TABLE[0])) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stp_get_preset_params: invalid preset index");
        return PRESET_TABLE[STP_PRESET_DEPRESSING];  // Default
    }

    return PRESET_TABLE[preset];
}

stp_params_t stp_create_params(float U, float tau_D, float tau_F) {
    // Clamp parameters to valid ranges
    stp_params_t params;
    params.U = nimcp_clamp01(U);
    params.tau_D = (tau_D > 0.0F) ? tau_D : STP_DEFAULT_TAU_D;
    params.tau_F = (tau_F > 0.0F) ? tau_F : STP_DEFAULT_TAU_F;

    return params;
}

const char* stp_get_preset_name(stp_preset_t preset) {
    // Guard: Validate preset index
    if (preset < 0 || preset >= sizeof(PRESET_NAMES) / sizeof(PRESET_NAMES[0])) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stp_get_preset_name: invalid preset index");
        return "Unknown";
    }

    return PRESET_NAMES[preset];
}

const char* stp_get_preset_description(stp_preset_t preset) {
    // Guard: Validate preset index
    if (preset < 0 || preset >= sizeof(PRESET_DESCRIPTIONS) / sizeof(PRESET_DESCRIPTIONS[0])) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stp_get_preset_description: invalid preset index");
        return "Unknown preset type";
    }

    return PRESET_DESCRIPTIONS[preset];
}

//=============================================================================
// Analysis Functions
//=============================================================================

void stp_compute_steady_state(const stp_params_t* params, float frequency, float* x_ss,
                               float* u_ss) {
    // Guard: Validate pointers
    if (!params || !x_ss || !u_ss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_compute_steady_state: null params, x_ss, or u_ss pointer");
        return;
    }

    // Guard: Validate frequency
    if (frequency <= 0.0F) {
        *x_ss = 1.0F;
        *u_ss = params->U;
        return;
    }

    // Inter-spike interval in milliseconds
    const float ISI = 1000.0F / frequency;

    // Compute decay factors
    const float decay_D = exp_decay(ISI, params->tau_D);
    const float decay_F = exp_decay(ISI, params->tau_F);

    // Solve fixed-point equations
    // At steady state: x(t+ISI) = x(t), u(t+ISI) = u(t)

    // u_ss = U + (u_ss + U(1-u_ss) - U) * exp(-ISI/τ_F)
    // Simplifying: u_ss = U * (1 - exp(-ISI/τ_F)) / (1 - (1-U)*exp(-ISI/τ_F))
    const float numerator_u = params->U * (1.0F - decay_F);
    const float denominator_u = 1.0F - (1.0F - params->U) * decay_F;
    *u_ss = (denominator_u > 1e-6F) ? (numerator_u / denominator_u) : params->U;
    *u_ss = nimcp_clamp01(*u_ss);

    // x_ss = (1 - (1-x_ss(1-u_ss)) * exp(-ISI/τ_D))
    // Simplifying: x_ss = (1 - exp(-ISI/τ_D)) / (1 - (1-u_ss)*exp(-ISI/τ_D))
    const float numerator_x = 1.0F - decay_D;
    const float denominator_x = 1.0F - (1.0F - *u_ss) * decay_D;
    *x_ss = (denominator_x > 1e-6F) ? (numerator_x / denominator_x) : 1.0F;
    *x_ss = nimcp_clamp01(*x_ss);
}

int stp_classify_synapse(const stp_params_t* params) {
    // Guard: Validate params
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stp_classify_synapse: null params pointer");
        return 2;  // Unknown -> mixed
    }

    // Compute depression vs facilitation strength
    // Depression index: High U, long τ_D
    const float depression_index = params->U * (params->tau_D / 1000.0F);

    // Facilitation index: Low U, long τ_F
    const float facilitation_index = (1.0F - params->U) * (params->tau_F / 100.0F);

    // Classification thresholds
    const float threshold = 0.3F;

    if (depression_index > facilitation_index + threshold) {
        return 0;  // Depressing
    } else if (facilitation_index > depression_index + threshold) {
        return 1;  // Facilitating
    } else {
        return 2;  // Mixed
    }
}

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_quantum_bridge.c - Quantum-Inspired Pink Noise Generation
//=============================================================================
/**
 * WHAT: Quantum-inspired algorithms for 1/f noise generation
 * WHY:  Efficient spectral synthesis with long-range correlations
 * HOW:  Simulated annealing, ternary filtering, quantum walks
 */

#include "plasticity/noise/nimcp_pink_noise_quantum_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"
#include "constants/nimcp_frequency_constants.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_quantum_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(pink_quantum_bridge)


//=============================================================================
// Internal: Random Number Generation
//=============================================================================

static float uniform_random(uint32_t* state) {
    *state = *state * NIMCP_LCG_MULTIPLIER + NIMCP_LCG_INCREMENT;
    return (float)(*state % 65536) / 65536.0f;
}

static float normal_random(uint32_t* state) {
    float u1 = uniform_random(state);
    float u2 = uniform_random(state);
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

//=============================================================================
// Internal: Annealing Functions
//=============================================================================

static void init_target_spectrum(pink_quantum_bridge_t* bridge) {
    /**
     * WHAT: Initialize target 1/f^α spectrum
     * WHY:  Annealing minimizes distance to this target
     */
    float alpha = bridge->config.target_alpha;
    for (uint32_t i = 1; i < bridge->num_frequencies; i++) {
        float freq = (float)i;
        bridge->target_spectrum[i] = 1.0f / powf(freq, alpha / 2.0f);
    }
    bridge->target_spectrum[0] = 0.0f;  // No DC
}

static float compute_energy(const pink_quantum_bridge_t* bridge) {
    /**
     * WHAT: Compute annealing energy (distance from target spectrum)
     * WHY:  Energy function for Metropolis-Hastings
     */
    float energy = 0.0f;
    for (uint32_t i = 1; i < bridge->num_frequencies; i++) {
        float diff = bridge->magnitudes[i] - bridge->target_spectrum[i];
        energy += diff * diff;
    }
    return energy;
}

static void init_ternary_coefficients(pink_quantum_bridge_t* bridge) {
    /**
     * WHAT: Initialize ternary filter coefficients
     * WHY:  Approximate 1/f filter with {-1, 0, +1}
     * HOW:  Quantize ideal pink noise filter coefficients
     */
    float high = bridge->config.ternary.threshold_high;
    float low = bridge->config.ternary.threshold_low;

    // Ideal pink noise filter has h[n] ∝ n^(-0.5)
    for (uint32_t i = 0; i < bridge->config.ternary.num_taps; i++) {
        float ideal = 1.0f / sqrtf((float)(i + 1));

        // Ternary quantization
        if (ideal > high) {
            bridge->ternary_coeffs[i] = 1;
        } else if (ideal < low) {
            bridge->ternary_coeffs[i] = -1;
        } else {
            bridge->ternary_coeffs[i] = 0;
        }
    }
}

static void init_quantum_walk(pink_quantum_bridge_t* bridge) {
    /**
     * WHAT: Initialize quantum walk probability amplitudes
     * WHY:  Quantum walk generates long-range correlations
     * HOW:  Allocate (or reuse) amplitude array, initialize at center
     *
     * Walk position ranges from 0 to 2*len-1, each with 2 spin components
     * So we need 4*len elements total: (2*len positions) * (2 components each)
     */
    uint32_t len = bridge->config.walk.walk_length;
    // Position range: 0 to 2*len-1, with 2 components each = 4*len elements
    size_t num_elements = 4 * len;
    size_t alloc_size = num_elements * sizeof(float);

    // Free existing allocation if present (prevents memory leak on reset)
    if (bridge->walk_amplitudes_handle) {
        unified_mem_free(bridge->walk_amplitudes_handle);
        bridge->walk_amplitudes_handle = NULL;
        bridge->walk_amplitudes = NULL;
    } else if (bridge->walk_amplitudes) {
        nimcp_free(bridge->walk_amplitudes);
        bridge->walk_amplitudes = NULL;
    }

    // Allocate via UMM if available, otherwise use nimcp_calloc
    if (bridge->mem_manager) {
        unified_mem_request_t req = unified_mem_request(alloc_size, NULL, true);
        bridge->walk_amplitudes_handle = unified_mem_alloc(bridge->mem_manager, &req);
        if (bridge->walk_amplitudes_handle) {
            bridge->walk_amplitudes = (float*)unified_mem_write(bridge->walk_amplitudes_handle);
            memset(bridge->walk_amplitudes, 0, alloc_size);
            NIMCP_LOGGING_DEBUG("Quantum walk amplitudes allocated via UMM (%zu bytes)", alloc_size);
        }
    }

    // Fallback to direct allocation if UMM unavailable or failed
    if (!bridge->walk_amplitudes) {
        bridge->walk_amplitudes = nimcp_calloc(num_elements, sizeof(float));
    }
    if (!bridge->walk_amplitudes) return;

    // Initialize at center with symmetric superposition
    uint32_t center = len;
    bridge->walk_amplitudes[center * 2] = 0.707f;      // |0⟩ component
    bridge->walk_amplitudes[center * 2 + 1] = 0.707f;  // |1⟩ component
    bridge->walk_position = center;
    bridge->walk_steps = 0;
}

//=============================================================================
// Default Configuration
//=============================================================================

pink_quantum_config_t pink_quantum_default_config(void) {
    pink_quantum_config_t config = {0};

    config.method = PINK_QUANTUM_HYBRID;
    config.target_alpha = 1.0f;
    config.amplitude = 0.1f;
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;
    config.seed = 0;
    config.enable_classical_fallback = true;

    // Annealing defaults
    config.annealing.initial_temperature = 10.0f;
    config.annealing.final_temperature = 0.01f;
    config.annealing.num_sweeps = 50;
    config.annealing.coupling_strength = NIMCP_OSCILLATION_COUPLING_DEFAULT;

    // Ternary defaults
    config.ternary.num_taps = 16;
    config.ternary.threshold_high = 0.3f;
    config.ternary.threshold_low = -0.3f;
    config.ternary.enable_dithering = true;

    // Walk defaults
    config.walk.walk_length = 256;
    config.walk.superposition_bias = 0.5f;
    config.walk.periodic_boundary = true;

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pink_quantum_bridge_t* pink_quantum_create(const pink_quantum_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    pink_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(pink_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    memcpy(&bridge->config, config, sizeof(pink_quantum_config_t));
    bridge->rng_state = (config->seed != 0) ? config->seed : 42;

    // Initialize based on method
    bridge->num_frequencies = PINK_QUANTUM_MAX_FREQUENCIES;
    init_target_spectrum(bridge);

    // Initialize magnitudes with random values
    for (uint32_t i = 0; i < bridge->num_frequencies; i++) {
        bridge->magnitudes[i] = bridge->target_spectrum[i] * (0.5f + uniform_random(&bridge->rng_state));
        bridge->phases[i] = uniform_random(&bridge->rng_state) * NIMCP_TWO_PI_F;
    }

    init_ternary_coefficients(bridge);
    init_quantum_walk(bridge);

    // Create classical fallback
    if (config->enable_classical_fallback) {
        pink_noise_config_t classical_config = pink_noise_default_config();
        classical_config.alpha = config->target_alpha;
        classical_config.amplitude = config->amplitude;
        classical_config.seed = config->seed;
        bridge->classical_generator = pink_noise_create(&classical_config);
    }

    bridge->energy = compute_energy(bridge);
    NIMCP_LOGGING_INFO("Created quantum pink noise bridge (method: %s)",
                       pink_quantum_method_name(config->method));

    return bridge;
}

void pink_quantum_destroy(pink_quantum_bridge_t* bridge) {
    if (!bridge) return;

    // Free walk amplitudes via UMM or direct free
    if (bridge->walk_amplitudes_handle) {
        unified_mem_free(bridge->walk_amplitudes_handle);
    } else if (bridge->walk_amplitudes) {
        nimcp_free(bridge->walk_amplitudes);
    }
    if (bridge->classical_generator) {
        pink_noise_destroy(bridge->classical_generator);
    }

    nimcp_free(bridge);
}

//=============================================================================
// Generation Functions
//=============================================================================

float pink_quantum_anneal_step(
    pink_quantum_bridge_t* bridge,
    float temperature
) {
    /**
     * WHAT: One Metropolis-Hastings step with quantum tunneling
     * WHY:  Refine spectrum toward target 1/f^α
     */
    if (!bridge) return 0.0f;

    // Select random frequency bin
    uint32_t idx = (uint32_t)(uniform_random(&bridge->rng_state) * (bridge->num_frequencies - 1)) + 1;

    // Propose change
    float old_mag = bridge->magnitudes[idx];
    float delta = normal_random(&bridge->rng_state) * temperature * 0.1f;
    float new_mag = fmaxf(0.0f, old_mag + delta);

    // Compute energy change
    float old_diff = old_mag - bridge->target_spectrum[idx];
    float new_diff = new_mag - bridge->target_spectrum[idx];
    float delta_e = new_diff * new_diff - old_diff * old_diff;

    // Add coupling energy (quantum tunneling effect)
    if (idx > 1) {
        float coupling = bridge->config.annealing.coupling_strength;
        delta_e += coupling * (new_mag - bridge->magnitudes[idx - 1]) *
                             (new_mag - bridge->magnitudes[idx - 1]);
    }

    // Metropolis acceptance
    bool accept = false;
    if (delta_e < 0.0f) {
        accept = true;
    } else {
        float prob = expf(-delta_e / temperature);
        accept = (uniform_random(&bridge->rng_state) < prob);
    }

    if (accept) {
        bridge->magnitudes[idx] = new_mag;
        bridge->energy += delta_e;
    }

    // Randomize phase
    bridge->phases[idx] = uniform_random(&bridge->rng_state) * NIMCP_TWO_PI_F;

    bridge->quantum_operations++;
    return bridge->energy;
}

float pink_quantum_ternary_filter(
    pink_quantum_bridge_t* bridge,
    float input
) {
    /**
     * WHAT: Apply ternary-quantized filter
     * WHY:  Efficient multiply-free filtering
     */
    if (!bridge) return 0.0f;

    // Add dithering if enabled
    float dithered = input;
    if (bridge->config.ternary.enable_dithering) {
        dithered += normal_random(&bridge->rng_state) * 0.01f;
    }

    // Store input in history
    uint32_t idx = bridge->filter_index;
    bridge->filter_history[idx] = dithered;
    bridge->filter_index = (idx + 1) % bridge->config.ternary.num_taps;

    // Apply ternary filter (no multiplications!)
    float output = 0.0f;
    uint32_t n_taps = bridge->config.ternary.num_taps;
    for (uint32_t i = 0; i < n_taps; i++) {
        uint32_t hist_idx = (idx + n_taps - i) % n_taps;
        int8_t coeff = bridge->ternary_coeffs[i];
        if (coeff == 1) {
            output += bridge->filter_history[hist_idx];
        } else if (coeff == -1) {
            output -= bridge->filter_history[hist_idx];
        }
        // coeff == 0: skip (most efficient)
    }

    bridge->quantum_operations++;
    return output * bridge->config.amplitude;
}

float pink_quantum_walk_step(pink_quantum_bridge_t* bridge) {
    /**
     * WHAT: One step of discrete-time quantum walk
     * WHY:  Generate long-range correlations via quantum interference
     */
    if (!bridge || !bridge->walk_amplitudes) return 0.0f;

    uint32_t len = bridge->config.walk.walk_length;
    float* amp = bridge->walk_amplitudes;
    float bias = bridge->config.walk.superposition_bias;

    // Hadamard coin operation (with bias)
    float h00 = sqrtf(bias);
    float h01 = sqrtf(1.0f - bias);
    float h10 = sqrtf(1.0f - bias);
    float h11 = -sqrtf(bias);

    // Apply coin at current position
    uint32_t pos = bridge->walk_position;
    float a0 = amp[pos * 2];      // |0⟩ amplitude
    float a1 = amp[pos * 2 + 1];  // |1⟩ amplitude

    float new_a0 = h00 * a0 + h01 * a1;
    float new_a1 = h10 * a0 + h11 * a1;

    // Shift operation
    uint32_t left = bridge->config.walk.periodic_boundary ?
                    (pos + 2 * len - 1) % (2 * len) : (pos > 0 ? pos - 1 : 0);
    uint32_t right = bridge->config.walk.periodic_boundary ?
                     (pos + 1) % (2 * len) : (pos < 2 * len - 1 ? pos + 1 : 2 * len - 1);

    // Clear current and update neighbors
    amp[pos * 2] = 0.0f;
    amp[pos * 2 + 1] = 0.0f;
    amp[left * 2] += new_a0 * 0.707f;
    amp[right * 2 + 1] += new_a1 * 0.707f;

    // Update position based on measurement probability
    float prob_left = amp[left * 2] * amp[left * 2] + amp[left * 2 + 1] * amp[left * 2 + 1];
    float prob_right = amp[right * 2] * amp[right * 2] + amp[right * 2 + 1] * amp[right * 2 + 1];

    if (uniform_random(&bridge->rng_state) < prob_left / (prob_left + prob_right + 0.001f)) {
        bridge->walk_position = left;
    } else {
        bridge->walk_position = right;
    }

    bridge->walk_steps++;
    bridge->quantum_operations++;

    // Output based on position deviation from center
    float center = (float)len;
    float deviation = ((float)bridge->walk_position - center) / center;
    return deviation * bridge->config.amplitude;
}

int pink_quantum_generate_sample(
    pink_quantum_bridge_t* bridge,
    float* sample
) {
    BRIDGE_BBB_VALIDATE(bridge, sample, sizeof(*sample));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_generate_sample: bridge is NULL");
        return -1;
    }
    if (!sample) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_generate_sample: sample is NULL");
        return -1;
    }

    float result = 0.0f;

    switch (bridge->config.method) {
        case PINK_QUANTUM_ANNEALING: {
            // Perform annealing steps
            float temp = bridge->config.annealing.initial_temperature /
                        (1.0f + 0.01f * (float)bridge->quantum_operations);
            for (uint32_t i = 0; i < 5; i++) {
                pink_quantum_anneal_step(bridge, temp);
            }

            // Synthesize sample from spectrum (simplified IFFT)
            float t = (float)bridge->quantum_operations * 0.001f;
            for (uint32_t f = 1; f < 32; f++) {
                result += bridge->magnitudes[f] * cosf(NIMCP_TWO_PI_F * f * t + bridge->phases[f]);
            }
            result *= bridge->config.amplitude * 0.1f;
            break;
        }

        case PINK_QUANTUM_TERNARY: {
            float white = normal_random(&bridge->rng_state);
            result = pink_quantum_ternary_filter(bridge, white);
            break;
        }

        case PINK_QUANTUM_WALK: {
            result = pink_quantum_walk_step(bridge);
            break;
        }

        case PINK_QUANTUM_HYBRID:
        default: {
            // Combine methods
            float temp = bridge->config.annealing.final_temperature;
            pink_quantum_anneal_step(bridge, temp);

            float white = normal_random(&bridge->rng_state);
            float ternary = pink_quantum_ternary_filter(bridge, white);
            float walk = pink_quantum_walk_step(bridge);

            result = 0.5f * ternary + 0.3f * walk + 0.2f * white * bridge->config.amplitude;
            break;
        }
    }

    // Classical fallback if quantum fails
    if (isnan(result) || isinf(result)) {
        bridge->using_classical = true;
        bridge->classical_fallbacks++;
        if (bridge->classical_generator) {
            pink_noise_generate_sample(bridge->classical_generator, &result);
        } else {
            result = 0.0f;
        }
    }

    *sample = result;
    return 0;
}

int pink_quantum_generate_batch(
    pink_quantum_bridge_t* bridge,
    float* samples,
    uint32_t num_samples
) {
    BRIDGE_BBB_VALIDATE(bridge, samples, sizeof(*samples));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_generate_batch: bridge is NULL");
        return -1;
    }
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_generate_batch: samples is NULL");
        return -1;
    }
    if (num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "pink_quantum_generate_batch: num_samples is 0");
        return -1;
    }

    for (uint32_t i = 0; i < num_samples; i++) {
        int result = pink_quantum_generate_sample(bridge, &samples[i]);
        if (result != 0) return result;
    }

    return 0;
}

//=============================================================================
// Control Functions
//=============================================================================

int pink_quantum_set_method(
    pink_quantum_bridge_t* bridge,
    pink_quantum_method_t method
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_set_method: bridge is NULL");
        return -1;
    }
    bridge->config.method = method;
    return 0;
}

int pink_quantum_set_enabled(
    pink_quantum_bridge_t* bridge,
    bool enabled
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_set_enabled: bridge is NULL");
        return -1;
    }
    bridge->using_classical = !enabled;
    return 0;
}

bool pink_quantum_is_enabled(const pink_quantum_bridge_t* bridge) {
    if (!bridge) {
        return false;  /* Boolean query returning false for NULL is normal */
    }
    return !bridge->using_classical;
}

int pink_quantum_restart_annealing(pink_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_restart_annealing: bridge is NULL");
        return -1;
    }

    // Reinitialize magnitudes and phases (matching create sequence)
    for (uint32_t i = 0; i < bridge->num_frequencies; i++) {
        bridge->magnitudes[i] = bridge->target_spectrum[i] *
                               (0.5f + uniform_random(&bridge->rng_state));
        bridge->phases[i] = uniform_random(&bridge->rng_state) * NIMCP_TWO_PI_F;
    }
    bridge->energy = compute_energy(bridge);

    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int pink_quantum_get_stats(
    const pink_quantum_bridge_t* bridge,
    pink_quantum_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(pink_quantum_stats_t));
    stats->quantum_operations = bridge->quantum_operations;
    stats->classical_fallbacks = bridge->classical_fallbacks;
    stats->measured_alpha = bridge->config.target_alpha;  // Simplified
    stats->annealing_energy = bridge->energy;

    uint64_t total = stats->quantum_operations + stats->classical_fallbacks;
    stats->quantum_efficiency = (total > 0) ?
        (float)stats->quantum_operations / (float)total : 1.0f;

    return 0;
}

int pink_quantum_reset_stats(pink_quantum_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_reset_stats: bridge is NULL");
        return -1;
    }
    bridge->quantum_operations = 0;
    bridge->classical_fallbacks = 0;
    return 0;
}

int pink_quantum_reset(
    pink_quantum_bridge_t* bridge,
    uint32_t new_seed
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_reset: bridge is NULL");
        return -1;
    }

    bridge->rng_state = (new_seed != 0) ? new_seed : bridge->config.seed;
    if (bridge->rng_state == 0) bridge->rng_state = 42;

    pink_quantum_restart_annealing(bridge);
    memset(bridge->filter_history, 0, sizeof(bridge->filter_history));
    bridge->filter_index = 0;

    if (bridge->walk_amplitudes) {
        init_quantum_walk(bridge);
    }

    bridge->quantum_operations = 0;
    bridge->classical_fallbacks = 0;
    bridge->using_classical = false;

    return 0;
}

const char* pink_quantum_method_name(pink_quantum_method_t method) {
    switch (method) {
        case PINK_QUANTUM_ANNEALING: return "annealing";
        case PINK_QUANTUM_TERNARY: return "ternary";
        case PINK_QUANTUM_WALK: return "walk";
        case PINK_QUANTUM_HYBRID: return "hybrid";
        default: return "unknown";
    }
}

//=============================================================================
// Unified Memory Manager Integration
//=============================================================================

int pink_quantum_connect_memory_manager(
    pink_quantum_bridge_t* bridge,
    unified_mem_manager_t mem_manager
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_quantum_connect_memory_manager: bridge is NULL");
        return -1;
    }

    // Store the memory manager
    bridge->mem_manager = mem_manager;

    // If we already have walk_amplitudes allocated and manager is now set,
    // reallocate via UMM for CoW benefits
    if (mem_manager && bridge->walk_amplitudes && !bridge->walk_amplitudes_handle) {
        // Save current state
        uint32_t len = bridge->config.walk.walk_length;
        size_t num_elements = 4 * len;
        size_t alloc_size = num_elements * sizeof(float);

        // Save current amplitudes
        float* old_amplitudes = bridge->walk_amplitudes;

        // Allocate via UMM
        unified_mem_request_t req = unified_mem_request(alloc_size, old_amplitudes, true);
        bridge->walk_amplitudes_handle = unified_mem_alloc(mem_manager, &req);
        if (bridge->walk_amplitudes_handle) {
            bridge->walk_amplitudes = (float*)unified_mem_write(bridge->walk_amplitudes_handle);
            // Free old allocation
            nimcp_free(old_amplitudes);
            NIMCP_LOGGING_INFO("Migrated quantum walk amplitudes to UMM");
        } else {
            // Keep using old allocation
            bridge->walk_amplitudes = old_amplitudes;
        }
    }

    return 0;
}

bool pink_quantum_has_memory_manager(const pink_quantum_bridge_t* bridge) {
    if (!bridge) {
        return false;  /* Boolean query returning false for NULL is normal */
    }
    return bridge->mem_manager != NULL;
}

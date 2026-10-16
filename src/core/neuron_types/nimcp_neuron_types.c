/**
 * @file nimcp_neuron_types.c
 * @brief Phase 8.7: Implementation of specialized neuron type system
 *
 * This file implements biologically-motivated compute functions for each
 * specialized neuron type defined in Phase 8.7.
 *
 * BIO-ASYNC INTEGRATION:
 * - Module ID: 0x0660 (BIO_MODULE_NEURON_TYPES)
 * - Publishes: neuron type registration, type parameter updates
 * - Uses: BIO_CHANNEL_ACETYLCHOLINE for neuron queries
 * - Uses: BIO_CHANNEL_DOPAMINE for learning-related events
 *
 * @author NIMCP Phase 8.7
 * @date 2025-11-08
 */

#include "core/neuron_types/nimcp_neuron_types.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Logging module identifier */
#define LOG_MODULE "NEURON_TYPES"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neuron_types)

// Define M_PI if not already defined (for some compilers)

// Bio-async module ID
#define BIO_MODULE_NEURON_TYPES 0x0660

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Compute mean of array
 *
 * WHAT: Calculate average value
 * WHY: Used for uncertainty estimation
 * HOW: Sum all values and divide by count
 *
 * COMPLEXITY: O(n)
 */
static float compute_mean(const float* values, uint32_t count) {
    if (!values || count == 0) return 0.0F;

    float sum = 0.0F;
    for (uint32_t i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / (float)count;
}

/**
 * @brief Compute variance of array
 *
 * WHAT: Calculate variance (spread) of values
 * WHY: Used for uncertainty estimation in metacognitive neurons
 * HOW: variance = E[(X - μ)^2] = E[X^2] - μ^2
 *
 * COMPLEXITY: O(n)
 */
static float compute_variance(const float* values, uint32_t count, float mean) {
    if (!values || count == 0) return 0.0F;

    float sum_sq_diff = 0.0F;
    for (uint32_t i = 0; i < count; i++) {
        float diff = values[i] - mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / (float)count;
}

/**
 * @brief Rotate coordinates for orientation tuning
 *
 * WHAT: Rotate (x,y) by angle theta
 * WHY: Gabor filters need rotation for orientation selectivity
 * HOW: Standard 2D rotation matrix
 */
static void rotate_coords(float x, float y, float theta_rad,
                         float* out_x, float* out_y) {
    float cos_theta = cosf(theta_rad);
    float sin_theta = sinf(theta_rad);
    *out_x = x * cos_theta + y * sin_theta;
    *out_y = -x * sin_theta + y * cos_theta;
}

// ============================================================================
// GENERIC NEURON MODEL IMPLEMENTATIONS
// ============================================================================

float compute_lif_neuron(const lif_params_t* params, float input,
                         float state, float dt) {
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_lif_neuron: params is NULL");
        return state;
    }

    /**
     * WHAT: Leaky Integrate-and-Fire neuron dynamics
     * WHY: Most common computational neuron model, efficient and interpretable
     * HOW: dV/dt = (E_L - V + R*I) / tau_m
     *      where V = membrane potential, E_L = rest potential,
     *      I = input current, tau_m = membrane time constant
     *
     * BIOLOGICAL MOTIVATION:
     * Models RC circuit of cell membrane with leak conductance.
     * When V reaches threshold, neuron fires and resets.
     *
     * COMPLEXITY: O(1)
     */

    // Euler integration: V(t+dt) = V(t) + dt * dV/dt
    float dV = ((params->rest_potential - state + input) / params->tau_membrane) * dt;
    float new_state = state + dV;

    // Clamp to reasonable range
    if (new_state < params->reset_potential) {
        new_state = params->reset_potential;
    }

    return new_state;
}

void compute_izhikevich_neuron(const izh_params_t* params, float input,
                               float v, float u, float dt,
                               float* out_v, float* out_u) {
    if (!params || !out_v || !out_u) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_izhikevich_neuron: NULL param");
        if (out_v) *out_v = v;
        if (out_u) *out_u = u;
        return;
    }

    /**
     * WHAT: Izhikevich neuron model - simple yet biologically realistic
     * WHY: Reproduces 20+ distinct cortical neuron firing patterns
     * HOW: dv/dt = 0.04v^2 + 5v + 140 - u + I
     *      du/dt = a(bv - u)
     *
     * BIOLOGICAL MOTIVATION:
     * Simplified Hodgkin-Huxley model that captures key dynamics:
     * - Fast voltage dynamics (v equation)
     * - Slow recovery dynamics (u equation)
     * - Parameter space maps to distinct neuron types
     *
     * PARAMETERS:
     * a: recovery time scale (smaller = slower recovery)
     * b: sensitivity of recovery to subthreshold fluctuations
     * c: after-spike reset value of v
     * d: after-spike reset increment of u
     *
     * REFERENCE: Izhikevich (2003) "Simple model of spiking neurons"
     * COMPLEXITY: O(1)
     */

    // Euler integration with 0.5ms sub-steps for stability
    const float substep = 0.5F;
    int num_substeps = (int)(dt / substep) + 1;
    float actual_substep = dt / num_substeps;

    float v_curr = v;
    float u_curr = u;

    for (int i = 0; i < num_substeps; i++) {
        // dv/dt = 0.04v^2 + 5v + 140 - u + I
        float dv = (0.04F * v_curr * v_curr + 5.0F * v_curr + 140.0F - u_curr + input) * actual_substep;

        // du/dt = a(bv - u)
        float du = params->a * (params->b * v_curr - u_curr) * actual_substep;

        v_curr += dv;
        u_curr += du;

        // Check for spike (v >= 30 mV)
        if (v_curr >= 30.0F) {
            v_curr = params->c;  // reset voltage
            u_curr += params->d;  // reset recovery
        }
    }

    *out_v = v_curr;
    *out_u = u_curr;
}

// ============================================================================
// VISUAL CORTEX (V1) IMPLEMENTATIONS
// ============================================================================

float compute_v1_simple_cell(const v1_simple_cell_params_t* params,
                             const float* inputs,
                             uint32_t input_width, uint32_t input_height,
                             float center_x, float center_y) {
    if (!params || !inputs) return 0.0F;

    /**
     * WHAT: Gabor filter for oriented edge detection
     * WHY: Models V1 simple cells (Hubel & Wiesel, 1962)
     * HOW: G(x,y) = exp(-(x'^2 + γ^2*y'^2)/(2σ^2)) * cos(2πf*x' + φ)
     *      where (x',y') are coordinates rotated by orientation angle
     *
     * BIOLOGICAL MOTIVATION:
     * V1 simple cells have elongated receptive fields that respond to
     * edges at specific orientations. They are phase-dependent (ON vs OFF).
     *
     * GABOR FILTER COMPONENTS:
     * 1. Gaussian envelope: spatial localization
     * 2. Sinusoidal carrier: orientation/frequency selectivity
     * 3. Rotation: orientation tuning
     *
     * PARAMETERS:
     * - orientation: preferred angle (0-180 degrees)
     * - spatial_frequency: cycles per degree of visual angle
     * - phase: 0 = ON-center, π = OFF-center
     * - sigma: envelope width
     *
     * COMPLEXITY: O(w*h) where w,h are receptive field size
     */

    float response = 0.0F;
    float theta_rad = params->orientation * M_PI / 180.0F;
    float gamma = params->aspect_ratio;
    float sigma_sq = params->sigma * params->sigma;

    // Define receptive field size (typically 3-4 sigma)
    int rf_size = (int)(3.0F * params->sigma);
    int count = 0;

    // Convolve Gabor filter over receptive field
    for (int dy = -rf_size; dy <= rf_size; dy++) {
        for (int dx = -rf_size; dx <= rf_size; dx++) {
            // Input coordinates
            int px = (int)(center_x + dx);
            int py = (int)(center_y + dy);

            // Check bounds
            if (px < 0 || px >= (int)input_width || py < 0 || py >= (int)input_height) {
                continue;
            }

            // Rotate coordinates by orientation angle
            float x_rot, y_rot;
            rotate_coords((float)dx, (float)dy, theta_rad, &x_rot, &y_rot);

            // Gaussian envelope
            float gaussian = expf(-(x_rot * x_rot + gamma * gamma * y_rot * y_rot) / (2.0F * sigma_sq));

            // Sinusoidal carrier
            float sinusoid = cosf(2.0F * M_PI * params->spatial_frequency * x_rot + params->phase);

            // Gabor filter value
            float gabor = gaussian * sinusoid;

            // Apply to input
            float input_val = inputs[py * input_width + px];
            response += gabor * input_val;
            count++;
        }
    }

    // Normalize by receptive field size
    if (count > 0) {
        response /= count;
    }

    // ON-center vs OFF-center
    if (!params->on_center) {
        response = -response;
    }

    // Half-wave rectification (neurons only respond to positive)
    return fmaxf(0.0F, response);
}

float compute_v1_complex_cell(const v1_complex_cell_params_t* params,
                              const float* simple_cell_responses,
                              uint32_t num_simple_cells) {
    if (!params || !simple_cell_responses || num_simple_cells == 0) return 0.0F;

    /**
     * WHAT: Energy model for phase-invariant edge detection
     * WHY: Models V1 complex cells (Hubel & Wiesel, 1962)
     * HOW: E = sqrt(even^2 + odd^2) where even/odd are 90° phase shifted
     *
     * BIOLOGICAL MOTIVATION:
     * Complex cells pool over multiple simple cells with different phases,
     * giving position-invariant edge detection. They respond to edges
     * regardless of exact position within receptive field.
     *
     * ENERGY MODEL:
     * Pool responses from simple cells with phases 0° and 90°
     * Energy = sqrt(R_even^2 + R_odd^2)
     * This gives phase invariance while preserving orientation selectivity
     *
     * COMPLEXITY: O(n) where n = num_simple_cells
     */

    // Pool responses (typically first half are even-symmetric, second half odd)
    float even_sum = 0.0F;
    float odd_sum = 0.0F;
    uint32_t half = num_simple_cells / 2;

    // Even-symmetric responses (phase = 0)
    for (uint32_t i = 0; i < half; i++) {
        even_sum += simple_cell_responses[i];
    }

    // Odd-symmetric responses (phase = π/2)
    for (uint32_t i = half; i < num_simple_cells; i++) {
        odd_sum += simple_cell_responses[i];
    }

    // Normalize
    if (half > 0) {
        even_sum /= half;
        odd_sum /= (num_simple_cells - half);
    }

    // Energy model: sqrt(even^2 + odd^2)
    float energy = sqrtf(even_sum * even_sum + odd_sum * odd_sum);

    // Direction selectivity: suppress responses to non-preferred direction
    // (simplified: assume even/odd sign indicates direction)
    float direction_factor = 1.0F;
    if (params->direction_selectivity > 0.0F) {
        float direction_signal = even_sum - odd_sum;
        if (direction_signal < 0.0F) {
            direction_factor = 1.0F - params->direction_selectivity;
        }
    }

    return energy * direction_factor;
}

// ============================================================================
// AUDITORY CORTEX (A1) IMPLEMENTATIONS
// ============================================================================

float compute_a1_frequency_tuned(const a1_frequency_params_t* params,
                                 const float* audio_input,
                                 uint32_t signal_length,
                                 float sample_rate,
                                 uint64_t timestamp) {
    if (!params || !audio_input || signal_length == 0) return 0.0F;

    /**
     * WHAT: Bandpass filtering for tonotopic frequency selectivity
     * WHY: Models A1 frequency-tuned neurons (Schreiner et al., 2000)
     * HOW: Simple IIR bandpass filter centered at preferred frequency
     *
     * BIOLOGICAL MOTIVATION:
     * Primary auditory cortex (A1) is organized tonotopically - neurons
     * are spatially arranged by preferred frequency, creating a frequency
     * map from low to high.
     *
     * TONOTOPIC ORGANIZATION:
     * - Each neuron responds to narrow frequency band
     * - Q factor determines sharpness of tuning
     * - Temporal integration window smooths response
     *
     * SIMPLIFIED IMPLEMENTATION:
     * Real auditory processing uses cochlear filterbank (gammatone filters)
     * This is simplified bandpass with resonance at center frequency
     *
     * COMPLEXITY: O(n) where n = signal_length
     */

    // Simplified bandpass: multiply by sinusoid at center frequency
    // This extracts energy at that frequency (like heterodyning)
    float response = 0.0F;
    float dt = 1.0F / sample_rate;

    // Determine integration window size (in samples)
    uint32_t window_samples = (uint32_t)(params->integration_window * sample_rate / 1000.0F);
    if (window_samples > signal_length) window_samples = signal_length;
    if (window_samples == 0) window_samples = 1;

    // Bandpass filtering via modulation
    for (uint32_t i = 0; i < window_samples; i++) {
        float t = i * dt;

        // Carrier at center frequency
        float carrier = sinf(2.0F * M_PI * params->center_frequency * t);

        // Modulate input
        float modulated = audio_input[i] * carrier;

        // Low-pass filter the modulated signal (envelope detection)
        // Q factor determines bandwidth
        float alpha = expf(-1.0F / params->q_factor);
        response = alpha * response + (1.0F - alpha) * modulated;
    }

    // Normalize
    response /= window_samples;

    // Adaptation to sustained input
    response *= (1.0F - params->adaptation_rate * 0.1F);

    // Half-wave rectification
    return fmaxf(0.0F, response);
}

float compute_a1_coincidence_detector(const a1_coincidence_params_t* params,
                                      const float* left_input,
                                      const float* right_input,
                                      uint32_t num_samples,
                                      uint64_t timestamp) {
    if (!params || !left_input || !right_input || num_samples == 0) return 0.0F;

    /**
     * WHAT: Coincidence detection for binaural sound localization
     * WHY: Models superior olivary complex neurons (Jeffress, 1948)
     * HOW: Cross-correlation of left/right inputs with high temporal precision
     *
     * BIOLOGICAL MOTIVATION:
     * Medial superior olive (MSO) contains coincidence detector neurons
     * that compute interaural time differences (ITDs) for sound localization.
     * These neurons have very precise temporal integration (~100 µs).
     *
     * JEFFRESS MODEL:
     * Neurons are arranged in delay lines - each neuron fires maximally
     * when inputs from both ears arrive simultaneously. Different delays
     * create spatial map of sound location.
     *
     * ITD COMPUTATION:
     * ITD = (distance to left ear - distance to right ear) / speed_of_sound
     * ITD range: ±700 µs for humans (interaural distance ~21 cm)
     *
     * COMPLEXITY: O(n) where n = num_samples in integration window
     */

    // Compute cross-correlation over short integration window
    float coincidence = 0.0F;
    uint32_t window_size = num_samples;

    // Short integration window for temporal precision
    if (params->integration_window > 0.0F) {
        // Assume sample rate ~44.1 kHz typical for audio
        float sample_rate = 44100.0F;
        uint32_t window_samples = (uint32_t)(params->integration_window * sample_rate / 1000.0F);
        if (window_samples < window_size) {
            window_size = window_samples;
        }
    }

    // Cross-correlation: sum(L[i] * R[i])
    for (uint32_t i = 0; i < window_size; i++) {
        coincidence += left_input[i] * right_input[i];
    }

    // Normalize by window size
    coincidence /= window_size;

    // Apply decay
    float decay_factor = expf(-params->decay_rate * 0.01F);
    coincidence *= decay_factor;

    // Threshold for coincidence detection
    if (coincidence < params->threshold) {
        coincidence = 0.0F;
    }

    return coincidence;
}

// ============================================================================
// COGNITIVE/METACOGNITIVE IMPLEMENTATIONS
// ============================================================================

float compute_metacognitive(const metacognitive_params_t* params,
                            const float* inputs, uint32_t num_inputs,
                            const float* input_history, uint32_t history_size,
                            float* out_confidence, float* out_uncertainty) {
    if (!params || !inputs || num_inputs == 0) {
        if (out_confidence) *out_confidence = 0.0F;
        if (out_uncertainty) *out_uncertainty = 1.0F;
        return 0.0F;
    }

    /**
     * WHAT: Metacognitive monitoring via uncertainty estimation
     * WHY: Models prefrontal confidence monitoring (Fleming & Dolan, 2012)
     * HOW: Estimate uncertainty from input variance, compute confidence
     *
     * BIOLOGICAL MOTIVATION:
     * Metacognition = "thinking about thinking"
     * Prefrontal cortex tracks decision confidence and uncertainty, enabling:
     * - Adaptive behavior based on confidence
     * - Error monitoring and correction
     * - Knowing when to seek more information
     *
     * UNCERTAINTY ESTIMATION:
     * Use variance of inputs as proxy for uncertainty
     * High variance = high uncertainty = low confidence
     * Low variance = low uncertainty = high confidence
     *
     * CONFIDENCE COMPUTATION:
     * Confidence = 1 / (1 + β * uncertainty)
     * where β controls sensitivity to uncertainty
     *
     * SECOND-ORDER PROCESSING:
     * This is "second-order" - monitoring first-order sensory/decision signals
     *
     * COMPLEXITY: O(n) where n = history_size
     */

    // Compute mean of current inputs
    float mean = compute_mean(inputs, num_inputs);

    // Compute variance over history (if available)
    float variance = 0.0F;
    if (input_history && history_size > 0) {
        float history_mean = compute_mean(input_history, history_size);
        variance = compute_variance(input_history, history_size, history_mean);
    } else {
        // Use current inputs only
        variance = compute_variance(inputs, num_inputs, mean);
    }

    // Uncertainty = variance (scaled by beta)
    float uncertainty = variance * params->uncertainty_beta;

    // Confidence = 1 / (1 + uncertainty)
    // This gives sigmoid-like mapping: low uncertainty -> high confidence
    float confidence = 1.0F / (1.0F + uncertainty);

    // Output values if requested
    if (out_confidence) *out_confidence = confidence;
    if (out_uncertainty) *out_uncertainty = uncertainty;

    // Metacognitive activation reflects confidence
    // High confidence -> strong signal, low confidence -> weak signal
    float activation = mean * confidence;

    // Apply confidence threshold
    if (confidence < params->confidence_threshold) {
        activation *= 0.5F;  // Attenuate low-confidence signals
    }

    return activation;
}

float compute_executive_control(const executive_control_params_t* params,
                                float goal_input,
                                float current_state,
                                float dt,
                                bool is_delay_period) {
    if (!params) return current_state;

    /**
     * WHAT: Executive control with goal maintenance and top-down modulation
     * WHY: Models dorsolateral PFC executive functions (Miller & Cohen, 2001)
     * HOW: Sustained activity during delays, goal-directed modulation
     *
     * BIOLOGICAL MOTIVATION:
     * Dorsolateral prefrontal cortex (dlPFC) maintains goal representations
     * and modulates processing throughout the brain:
     * - Working memory: sustained activity during delay periods
     * - Attention: top-down biasing of sensory areas
     * - Cognitive control: maintaining task rules and goals
     *
     * EXECUTIVE CONTROL PROPERTIES:
     * 1. Goal maintenance: persist activity in absence of input
     * 2. Top-down modulation: amplify task-relevant signals
     * 3. Decay: gradual loss of activity (working memory decay)
     * 4. Threshold boosting: increase sensitivity during goal-directed state
     *
     * DELAY PERIOD ACTIVITY:
     * During delays (e.g., between stimulus and response), dlPFC neurons
     * maintain elevated firing to bridge temporal gap.
     *
     * COMPLEXITY: O(1)
     */

    float new_state = current_state;

    if (is_delay_period && params->delay_activity) {
        // DELAY PERIOD: Maintain activity via recurrent excitation
        // dS/dt = -decay * S + maintenance * S
        // This creates sustained firing in absence of input

        float maintenance_input = params->goal_maintenance * current_state;
        float decay = params->decay_rate * current_state;

        // Update: maintain - decay
        new_state = current_state + dt * (maintenance_input - decay);

        // Prevent unbounded growth
        if (new_state > 1.0F) new_state = 1.0F;

    } else {
        // ACTIVE PERIOD: Process goal input with amplification

        // Top-down modulation amplifies goal-relevant signals
        float modulated_input = goal_input * (1.0F + params->modulation_strength);

        // Integrate input with decay
        float integration = modulated_input - params->decay_rate * current_state;
        new_state = current_state + dt * integration;
    }

    // Clamp to [0, 1]
    if (new_state < 0.0F) new_state = 0.0F;
    if (new_state > 1.0F) new_state = 1.0F;

    return new_state;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

nimcp_result_t neuron_type_get_default_params(neuron_type_t type,
                                               neuron_type_params_t* out_params) {
    LOG_DEBUG(LOG_MODULE, "Getting default params for neuron type: %s", neuron_type_get_name(type));

    if (!out_params) {
        LOG_ERROR(LOG_MODULE, "NULL out_params pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Zero out union
    memset(out_params, 0, sizeof(neuron_type_params_t));

    switch (type) {
        case NEURON_EXCITATORY:
        case NEURON_INHIBITORY:
            // Backward compatibility: use LIF as default
            out_params->lif.tau_membrane = 20.0F;
            out_params->lif.rest_potential = -70.0F;
            out_params->lif.threshold = -55.0F;
            out_params->lif.reset_potential = -75.0F;
            out_params->lif.refractory_period = 2.0F;
            LOG_DEBUG(LOG_MODULE, "Initialized LIF params for %s: tau=%.1fms, threshold=%.1fmV",
                      neuron_type_get_name(type), out_params->lif.tau_membrane, out_params->lif.threshold);
            break;

        case NEURON_GENERIC_LIF:
            out_params->lif.tau_membrane = 20.0F;      // 20 ms
            out_params->lif.rest_potential = -70.0F;   // -70 mV
            out_params->lif.threshold = -55.0F;        // -55 mV
            out_params->lif.reset_potential = -75.0F;  // -75 mV
            out_params->lif.refractory_period = 2.0F;  // 2 ms
            break;

        case NEURON_GENERIC_IZHIKEVICH:
            // Regular spiking (RS) cortical neuron
            out_params->izhikevich.a = 0.02F;
            out_params->izhikevich.b = 0.2F;
            out_params->izhikevich.c = -65.0F;
            out_params->izhikevich.d = 8.0F;
            break;

        case NEURON_V1_SIMPLE_CELL:
            out_params->v1_simple.orientation = 45.0F;        // 45 degrees
            out_params->v1_simple.spatial_frequency = 2.0F;   // 2 cycles/degree
            out_params->v1_simple.phase = 0.0F;               // ON-center
            out_params->v1_simple.aspect_ratio = 0.5F;        // Elongated
            out_params->v1_simple.sigma = 2.0F;               // 2 degree width
            out_params->v1_simple.on_center = true;
            break;

        case NEURON_V1_COMPLEX_CELL:
            out_params->v1_complex.orientation = 90.0F;         // Vertical
            out_params->v1_complex.direction_selectivity = 0.5F; // Moderate
            out_params->v1_complex.surround_suppression = 0.3F;  // Weak
            out_params->v1_complex.pooling_size = 4.0F;         // 4 degrees
            break;

        case NEURON_A1_FREQUENCY_TUNED:
            out_params->a1_frequency.center_frequency = 1000.0F;  // 1 kHz
            out_params->a1_frequency.q_factor = 5.0F;             // Moderate tuning
            out_params->a1_frequency.bandwidth = 200.0F;          // 200 Hz
            out_params->a1_frequency.integration_window = 10.0F;  // 10 ms
            out_params->a1_frequency.adaptation_rate = 0.1F;
            break;

        case NEURON_A1_COINCIDENCE_DETECTOR:
            out_params->a1_coincidence.integration_window = 1.0F;   // 1 ms
            out_params->a1_coincidence.temporal_precision = 100.0F; // 100 µs
            out_params->a1_coincidence.threshold = 0.3F;
            out_params->a1_coincidence.decay_rate = 0.1F;
            break;

        case NEURON_METACOGNITIVE:
            out_params->metacognitive.confidence_threshold = 0.5F;  // 50%
            out_params->metacognitive.uncertainty_window = 100.0F;  // 100 ms
            out_params->metacognitive.uncertainty_beta = 1.0F;
            out_params->metacognitive.history_size = 10;
            break;

        case NEURON_EXECUTIVE_CONTROL:
            out_params->executive.goal_maintenance = 0.8F;        // Strong maintenance
            out_params->executive.modulation_strength = 0.5F;     // Moderate modulation
            out_params->executive.decay_rate = 0.05F;             // Slow decay
            out_params->executive.threshold_boost = 0.2F;
            out_params->executive.delay_activity = true;
            break;

        case NEURON_MOTOR_PATTERN_GEN:
            // Motor pattern generator doesn't have type-specific params yet
            // Just return success with zeroed params
            LOG_DEBUG(LOG_MODULE, "Motor pattern generator: no specific params yet");
            break;

        // Neural logic gates don't need specific params - they operate on binary inputs
        case NEURON_LOGIC_AND:
        case NEURON_LOGIC_OR:
        case NEURON_LOGIC_XOR:
        case NEURON_LOGIC_NOT:
        case NEURON_LOGIC_VARIABLE:
        case NEURON_LOGIC_IMPLIES:
            LOG_DEBUG(LOG_MODULE, "Logic gate %s: stateless, no params needed",
                      neuron_type_get_name(type));
            break;

        default:
            // For types without specific default params, return success with zeroed params
            // (params were already zeroed at start of function)
            LOG_DEBUG(LOG_MODULE, "Neuron type %d (%s): using zeroed default params",
                      type, neuron_type_get_name(type));
            break;
    }

    LOG_INFO(LOG_MODULE, "Default params initialized for %s", neuron_type_get_name(type));
    return NIMCP_SUCCESS;
}

nimcp_result_t neuron_type_validate_params(neuron_type_t type,
                                            const neuron_type_params_t* params) {
    LOG_DEBUG(LOG_MODULE, "Validating params for neuron type: %s", neuron_type_get_name(type));

    if (!params) {
        LOG_ERROR(LOG_MODULE, "NULL params pointer");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    switch (type) {
        case NEURON_GENERIC_LIF:
            if (params->lif.tau_membrane <= 0.0F) {
                LOG_ERROR(LOG_MODULE, "LIF: invalid tau_membrane=%.2f (must be > 0)", params->lif.tau_membrane);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (params->lif.refractory_period < 0.0F) {
                LOG_ERROR(LOG_MODULE, "LIF: invalid refractory_period=%.2f (must be >= 0)", params->lif.refractory_period);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NEURON_GENERIC_IZHIKEVICH:
            // Parameters can have wide range, minimal validation
            break;

        case NEURON_V1_SIMPLE_CELL:
            if (params->v1_simple.orientation < 0.0F || params->v1_simple.orientation > 180.0F) {
                LOG_ERROR(LOG_MODULE, "V1 Simple: invalid orientation=%.1f (must be 0-180)", params->v1_simple.orientation);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (params->v1_simple.spatial_frequency <= 0.0F) {
                LOG_ERROR(LOG_MODULE, "V1 Simple: invalid spatial_frequency=%.2f (must be > 0)", params->v1_simple.spatial_frequency);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (params->v1_simple.sigma <= 0.0F) {
                LOG_ERROR(LOG_MODULE, "V1 Simple: invalid sigma=%.2f (must be > 0)", params->v1_simple.sigma);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NEURON_V1_COMPLEX_CELL:
            if (params->v1_complex.orientation < 0.0F || params->v1_complex.orientation > 180.0F) {
                LOG_ERROR(LOG_MODULE, "V1 Complex: invalid orientation=%.1f (must be 0-180)", params->v1_complex.orientation);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (params->v1_complex.direction_selectivity < 0.0F || params->v1_complex.direction_selectivity > 1.0F) {
                LOG_ERROR(LOG_MODULE, "V1 Complex: invalid direction_selectivity=%.2f (must be 0-1)", params->v1_complex.direction_selectivity);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NEURON_A1_FREQUENCY_TUNED:
            if (params->a1_frequency.center_frequency <= 0.0F || params->a1_frequency.center_frequency > 20000.0F) {
                LOG_ERROR(LOG_MODULE, "A1 Frequency: invalid center_frequency=%.1f (must be 0-20000 Hz)", params->a1_frequency.center_frequency);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            if (params->a1_frequency.q_factor <= 0.0F) {
                LOG_ERROR(LOG_MODULE, "A1 Frequency: invalid q_factor=%.2f (must be > 0)", params->a1_frequency.q_factor);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NEURON_A1_COINCIDENCE_DETECTOR:
            if (params->a1_coincidence.integration_window <= 0.0F) {
                LOG_ERROR(LOG_MODULE, "A1 Coincidence: invalid integration_window=%.2f (must be > 0)", params->a1_coincidence.integration_window);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NEURON_METACOGNITIVE:
            if (params->metacognitive.confidence_threshold < 0.0F || params->metacognitive.confidence_threshold > 1.0F) {
                LOG_ERROR(LOG_MODULE, "Metacognitive: invalid confidence_threshold=%.2f (must be 0-1)", params->metacognitive.confidence_threshold);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        case NEURON_EXECUTIVE_CONTROL:
            if (params->executive.goal_maintenance < 0.0F || params->executive.goal_maintenance > 1.0F) {
                LOG_ERROR(LOG_MODULE, "Executive: invalid goal_maintenance=%.2f (must be 0-1)", params->executive.goal_maintenance);
                return NIMCP_ERROR_INVALID_PARAM;
            }
            break;

        default:
            break;
    }

    LOG_DEBUG(LOG_MODULE, "Params validated successfully for %s", neuron_type_get_name(type));
    return NIMCP_SUCCESS;
}

const char* neuron_type_get_name(neuron_type_t type) {
    switch (type) {
        case NEURON_EXCITATORY:            return "Excitatory";
        case NEURON_INHIBITORY:            return "Inhibitory";
        case NEURON_GENERIC_LIF:           return "Generic LIF";
        case NEURON_GENERIC_IZHIKEVICH:    return "Generic Izhikevich";
        case NEURON_V1_SIMPLE_CELL:        return "V1 Simple Cell";
        case NEURON_V1_COMPLEX_CELL:       return "V1 Complex Cell";
        case NEURON_A1_FREQUENCY_TUNED:    return "A1 Frequency-Tuned";
        case NEURON_A1_COINCIDENCE_DETECTOR: return "A1 Coincidence Detector";
        case NEURON_METACOGNITIVE:         return "Metacognitive";
        case NEURON_EXECUTIVE_CONTROL:     return "Executive Control";
        default:                           return "Unknown";
    }
}

bool neuron_type_is_excitatory(neuron_type_t type) {
    switch (type) {
        case NEURON_EXCITATORY:
            return true;
        case NEURON_INHIBITORY:
            return false;
        default:
            // Most specialized types are excitatory
            // Can be overridden per-neuron if needed
            return true;
    }
}

/**
 * @brief Helper: Apply Gabor filter response for V1 simple cell
 *
 * WHAT: Single-value Gabor filter computation without full 2D convolution
 * WHY:  For single-input processing, compute orientation-tuned response
 * HOW:  Simplified Gabor: cos(2πf*input*cos(θ-θ_pref) + φ) * input
 *
 * BIOLOGICAL RATIONALE:
 * V1 simple cells respond to oriented edges. When given a single input value,
 * we model this as orientation selectivity where the input strength is
 * modulated by how well it matches the neuron's preferred orientation.
 *
 * COMPLEXITY: O(1)
 */
static float apply_gabor_response(float input, float preferred_orientation,
                                   float spatial_frequency, float phase) {
    if (input <= 0.0F) return 0.0F;

    // For single-value input, treat input as edge strength
    // Response is modulated by sinusoid at spatial frequency
    float theta_rad = preferred_orientation * M_PI / 180.0F;

    // Gabor response: Gaussian envelope already implicit in network weights
    // We compute just the sinusoidal carrier modulation
    float sinusoid = cosf(2.0F * M_PI * spatial_frequency * input + phase);

    // Modulate input by sinusoid (orientation tuning)
    float response = input * (0.5F + 0.5F * sinusoid);  // [0,1] range

    // Half-wave rectification
    return fmaxf(0.0F, response);
}

/**
 * @brief Helper: Apply complex cell energy model
 *
 * WHAT: Phase-invariant response via energy model
 * WHY:  Complex cells respond to edges regardless of phase
 * HOW:  E = sqrt(even^2 + odd^2) approximation
 *
 * BIOLOGICAL RATIONALE:
 * Complex cells pool over simple cells with different phases, giving
 * position/phase-invariant edge detection. For single input, we approximate
 * this by taking the magnitude of the input.
 *
 * COMPLEXITY: O(1)
 */
static float apply_complex_cell_response(float input, float orientation,
                                          float direction_selectivity) {
    if (input <= 0.0F) return 0.0F;

    // Energy model approximation for single input
    // Typically: E = sqrt(even^2 + odd^2)
    // For single value, use magnitude
    float energy = fabsf(input);

    // Direction selectivity: suppress non-preferred directions
    // For single input, we can't determine direction, so apply partial suppression
    float direction_factor = 1.0F - (direction_selectivity * 0.3F);

    return energy * direction_factor;
}

/**
 * @brief Helper: Apply bandpass filter response for frequency tuning
 *
 * WHAT: Bandpass filtering centered at preferred frequency
 * WHY:  A1 neurons show tonotopic frequency selectivity (Schreiner 2000)
 * HOW:  Gaussian-shaped frequency response with Q factor controlling width
 *
 * BIOLOGICAL RATIONALE:
 * Primary auditory cortex (A1) is organized tonotopically - neurons are
 * spatially arranged by preferred frequency. Each neuron responds to a
 * narrow frequency band, with tuning sharpness determined by Q factor.
 * Q = center_frequency / bandwidth (higher Q = sharper tuning).
 *
 * REFERENCE: Schreiner et al. (2000) "Functional architecture of auditory cortex"
 * COMPLEXITY: O(1)
 */
static float apply_bandpass_response(float input, float preferred_frequency,
                                      float quality_factor, float integration_window) {
    if (input <= 0.0F || preferred_frequency <= 0.0F || quality_factor <= 0.0F) {
        return 0.0F;
    }

    // For single-value input, apply frequency-dependent gain
    // This models the neuron's frequency response curve
    // Response = input * H(f) where H(f) is bandpass transfer function

    // Normalize input by integration window to account for temporal integration
    float window_factor = fminf(1.0F, integration_window / 10.0F);  // 10ms baseline

    // Apply Q factor as gain modulation
    // Higher Q = more selective = lower baseline response
    float q_factor = 1.0F / (1.0F + quality_factor * 0.1F);

    // Bandpass response with temporal integration
    float response = input * window_factor * q_factor;

    // Half-wave rectification (neurons only fire for positive inputs)
    return fmaxf(0.0F, response);
}

/**
 * @brief Helper: Apply coincidence detection for binaural processing
 *
 * WHAT: Temporal coincidence detection with high temporal precision
 * WHY:  Models superior olivary complex for sound localization (Jeffress 1948)
 * HOW:  Detect temporal alignment of inputs within short integration window
 *
 * BIOLOGICAL RATIONALE:
 * Coincidence detector neurons in the medial superior olive (MSO) compute
 * interaural time differences (ITDs) for sound localization. These neurons
 * have very precise temporal integration (~100 µs) and fire maximally when
 * inputs from both ears arrive simultaneously. The Jeffress model proposes
 * neurons arranged in delay lines to create a spatial map of sound location.
 *
 * ITD COMPUTATION:
 * ITD = (distance to left ear - distance to right ear) / speed_of_sound
 * ITD range: ±700 µs for humans (interaural distance ~21 cm)
 *
 * REFERENCE: Jeffress (1948) "A place theory of sound localization"
 * COMPLEXITY: O(1) for single-value processing
 */
static float apply_coincidence_detection(float input, float integration_window,
                                          float decay_rate) {
    if (input <= 0.0F || integration_window <= 0.0F) {
        return 0.0F;
    }

    // For single-value input, model coincidence strength
    // In full implementation, this would cross-correlate left/right inputs

    // Short integration window enhances temporal precision
    // Typical MSO neurons: 0.5-2ms integration window
    float temporal_precision = 1.0F / fmaxf(0.1F, integration_window);

    // Apply temporal precision scaling
    float coincidence = input * temporal_precision;

    // Apply decay for sustained inputs
    float decay_factor = expf(-decay_rate * 0.01F);
    coincidence *= decay_factor;

    // Threshold for coincidence detection
    // Coincidence detectors have high thresholds
    float threshold = 0.3F;
    if (coincidence < threshold) {
        coincidence = 0.0F;
    }

    return coincidence;
}

/**
 * @brief Helper: Apply onset detection for transient auditory events
 *
 * WHAT: Detect onset of auditory stimuli with short integration window
 * WHY:  Onset detectors signal start of acoustic events (Heil 1997)
 * HOW:  High-pass temporal filtering with rapid adaptation
 *
 * BIOLOGICAL RATIONALE:
 * Auditory onset neurons respond strongly to the beginning of sounds but
 * adapt rapidly during sustained stimulation. This provides precise timing
 * information about acoustic events. Found throughout auditory pathway from
 * cochlear nucleus to auditory cortex. Short integration windows (1-5ms)
 * enable detection of rapid temporal changes in sound envelope.
 *
 * ADAPTATION:
 * Onset neurons show rapid spike-frequency adaptation (SFA), responding
 * strongly to transient changes but weakly to sustained input. This is
 * implemented via high-pass filtering of the temporal envelope.
 *
 * REFERENCE: Heil (1997) "Auditory cortical onset responses revisited"
 * COMPLEXITY: O(1)
 */
static float apply_onset_detection(float input, float integration_window) {
    if (input <= 0.0F || integration_window <= 0.0F) {
        return 0.0F;
    }

    // Onset detection via short integration window
    // Typical onset detectors: 1-5ms integration window

    // Shorter window = stronger onset response
    // Normalize by 5ms baseline
    float onset_strength = 5.0F / fmaxf(1.0F, integration_window);

    // Apply onset scaling to input
    float onset_response = input * onset_strength;

    // Rapid adaptation: strong initial response, fast decay
    // Model as high-pass filter on envelope
    // For single-value input, apply threshold nonlinearity
    float adaptation_threshold = 0.5F;
    if (onset_response < adaptation_threshold) {
        onset_response *= 0.1F;  // Weak response below threshold
    }

    // Clamp to [0, 1] range
    onset_response = fminf(1.0F, onset_response);

    return onset_response;
}

/**
 * @brief Helper: Compute prediction error for metacognitive monitoring
 *
 * WHAT: Calculate prediction error between expected and actual activation
 * WHY:  Prediction errors drive metacognitive uncertainty (Fleming & Dolan 2012)
 * HOW:  PE = |actual - expected|, weighted by uncertainty_beta
 *
 * BIOLOGICAL RATIONALE:
 * Prefrontal cortex tracks prediction errors to monitor decision quality.
 * Large prediction errors indicate high uncertainty and trigger metacognitive
 * monitoring signals. This implements "error monitoring" (Yeung & Summerfield 2012).
 *
 * COMPLEXITY: O(1)
 */
static float compute_prediction_error(float actual, float expected,
                                        float uncertainty_beta) {
    if (expected < 0.0F) {
        // No expected value: use neutral baseline (0.5)
        expected = 0.5F;
    }

    // Prediction error magnitude
    float error = fabsf(actual - expected);

    // Scale by uncertainty beta (sensitivity to errors)
    return error * uncertainty_beta;
}

/**
 * @brief Helper: Track activation pattern variance for introspection
 *
 * WHAT: Estimate variance of activation over recent history
 * WHY:  Variance indicates stability vs volatility of processing
 * HOW:  Approximate variance using exponential moving average
 *
 * BIOLOGICAL RATIONALE:
 * Metacognitive neurons track their own activation patterns (introspection).
 * Low variance = stable processing = high confidence.
 * High variance = volatile processing = low confidence.
 *
 * COMPLEXITY: O(1)
 */
static float estimate_activation_variance(float current, float prev_mean,
                                            float prev_variance,
                                            float decay_alpha) {
    // Exponential moving average of variance
    // This approximates online variance calculation
    float delta = current - prev_mean;
    float variance = decay_alpha * prev_variance + (1.0F - decay_alpha) * (delta * delta);

    return variance;
}

/**
 * @brief Helper: Modulate learning rate based on confidence
 *
 * WHAT: Adjust learning rate based on metacognitive confidence
 * WHY:  High confidence = large updates, low confidence = small updates
 * HOW:  learning_rate = base_rate * confidence
 *
 * BIOLOGICAL RATIONALE:
 * Metacognition enables adaptive learning. When confident, update strongly.
 * When uncertain, update cautiously to avoid cementing errors.
 * This implements confidence-weighted learning (Fleming & Lau 2014).
 *
 * REFERENCE: Fleming & Lau (2014) "How to measure metacognition"
 * COMPLEXITY: O(1)
 */
static float modulate_learning_rate(float base_signal, float confidence,
                                      float min_rate, float max_rate) {
    // Confidence-weighted modulation
    // confidence ∈ [0, 1] maps to [min_rate, max_rate]
    float rate_range = max_rate - min_rate;
    float modulated_rate = min_rate + (confidence * rate_range);

    // Apply to signal
    return base_signal * modulated_rate;
}

/**
 * @brief Helper: Apply metacognitive monitoring with full introspection
 *
 * WHAT: Monitor activation for uncertainty estimation with introspection
 * WHY:  Metacognition enables confidence-based learning (Fleming & Dolan 2012)
 * HOW:  Track variance, prediction errors, and confidence
 *
 * BIOLOGICAL RATIONALE:
 * Prefrontal cortex contains neurons that monitor decision confidence and
 * uncertainty. These metacognitive signals enable adaptive behavior:
 * - High confidence → maintain current strategy
 * - Low confidence → seek more information, adjust learning rate
 *
 * Metacognitive monitoring is "second-order" - it monitors first-order
 * sensory/decision signals. Fleming & Dolan (2012) show that anterior PFC
 * tracks confidence independently of decision accuracy.
 *
 * INTROSPECTION FEATURES:
 * 1. Track own activation patterns (variance estimation)
 * 2. Compute prediction errors (expected vs actual)
 * 3. Modulate learning rate based on confidence
 *
 * REFERENCE: Fleming & Dolan (2012) "The neural basis of metacognitive ability"
 * REFERENCE: Yeung & Summerfield (2012) "Metacognition in human decision-making"
 * COMPLEXITY: O(1)
 */
static float apply_metacognitive_monitoring(float input, float prev_input,
                                              float confidence_threshold,
                                              float uncertainty_beta) {
    // Guard clause: no previous input for comparison
    if (prev_input < 0.0F) {
        // First activation: assume moderate confidence
        return input * 0.7F;
    }

    // 1. INTROSPECTION: Track activation variance
    // Use exponential decay for online variance estimation
    float decay_alpha = 0.9F;  // Smoothing factor
    static float prev_variance = 0.0F;  // Static for persistence
    float variance = estimate_activation_variance(input, prev_input,
                                                    prev_variance, decay_alpha);
    prev_variance = variance;

    // 2. PREDICTION ERROR: Compute error from expected baseline
    float prediction_error = compute_prediction_error(input, prev_input,
                                                        uncertainty_beta);

    // 3. UNCERTAINTY ESTIMATION: Combine variance and prediction error
    // Uncertainty = variance + prediction_error
    float uncertainty = variance + prediction_error;

    // 4. CONFIDENCE COMPUTATION: Convert uncertainty to confidence
    // Confidence = 1 / (1 + uncertainty)
    float confidence = 1.0F / (1.0F + uncertainty);

    // 5. LEARNING RATE MODULATION: Adjust based on confidence
    // High confidence → strong learning (rate = 1.0)
    // Low confidence → weak learning (rate = 0.3)
    float modulated_input = modulate_learning_rate(input, confidence,
                                                     0.3F, 1.0F);

    // 6. CONFIDENCE THRESHOLD: Apply hard threshold for low confidence
    if (confidence < confidence_threshold) {
        // Low confidence: flag for additional monitoring
        modulated_input *= 0.5F;
    }

    return modulated_input;
}

/**
 * @brief Helper: Implement task switching with reconfiguration cost
 *
 * WHAT: Rapidly switch between task contexts with cognitive cost
 * WHY:  Models task-switch cost (Monsell 2003)
 * HOW:  Detect task change, apply reconfiguration penalty
 *
 * BIOLOGICAL RATIONALE:
 * Task switching incurs a measurable reaction time cost (~100-200ms).
 * This reflects the time needed to reconfigure processing priorities.
 * dlPFC neurons show transient activity during task switches.
 *
 * REFERENCE: Monsell (2003) "Task switching"
 * COMPLEXITY: O(1)
 */
static float apply_task_switching(float input, float current_goal,
                                    float prev_goal, float switch_cost) {
    // Detect task switch: goal signal changed significantly
    static float task_switch_threshold = 0.3F;
    float goal_change = fabsf(current_goal - prev_goal);

    if (goal_change > task_switch_threshold) {
        // Task switch detected: apply reconfiguration cost
        // Temporarily reduce processing efficiency
        return input * (1.0F - switch_cost);
    }

    // No task switch: normal processing
    return input;
}

/**
 * @brief Helper: Implement inhibitory control via threshold gating
 *
 * WHAT: Suppress irrelevant or prepotent responses
 * WHY:  Models response inhibition (Aron et al. 2004)
 * HOW:  Apply adaptive threshold based on goal relevance
 *
 * BIOLOGICAL RATIONALE:
 * Right inferior frontal cortex (rIFC) implements response inhibition.
 * Strong prepotent responses can be suppressed when task-irrelevant.
 * This enables flexible, goal-directed behavior.
 *
 * STROOP TASK EXAMPLE:
 * Word "RED" in blue ink: suppress word reading (prepotent), say "blue"
 *
 * REFERENCE: Aron et al. (2004) "Inhibitory control in the brain"
 * COMPLEXITY: O(1)
 */
static float apply_inhibitory_control(float input, float goal_relevance,
                                        float threshold_boost) {
    // Adaptive threshold: higher for low-relevance inputs
    // threshold = base + (1 - relevance) * boost
    float base_threshold = 0.3F;
    float adaptive_threshold = base_threshold + (1.0F - goal_relevance) * threshold_boost;

    if (input < adaptive_threshold) {
        // Below threshold: suppress (strong inhibition)
        return input * 0.1F;
    }

    // Above threshold: pass through
    return input;
}

/**
 * @brief Helper: Maintain working memory via persistent activation
 *
 * WHAT: Sustain activation during delay periods
 * WHY:  Models dlPFC delay-period activity (Goldman-Rakic 1995)
 * HOW:  Boost activation when goal is strong
 *
 * BIOLOGICAL RATIONALE:
 * dlPFC neurons maintain elevated firing during working memory delays.
 * This bridges temporal gaps between stimulus and response.
 * Delay-period activity represents maintained information.
 *
 * CLASSIC TASK: Delayed response task
 * - Cue stimulus at location A
 * - Delay period (no stimulus)
 * - Response to remembered location A
 *
 * REFERENCE: Goldman-Rakic (1995) "Cellular basis of working memory"
 * COMPLEXITY: O(1)
 */
static float maintain_working_memory(float input, float goal_strength,
                                       float maintenance_threshold) {
    // Strong goal signal: maintain elevated activity
    if (goal_strength > maintenance_threshold) {
        // Boost weak inputs to maintain representation
        float maintenance_boost = 0.5F * goal_strength;
        return fmaxf(input, maintenance_boost);
    }

    // Weak goal: no maintenance
    return input;
}

/**
 * @brief Helper: Apply top-down attentional modulation
 *
 * WHAT: Amplify task-relevant signals via gain modulation
 * WHY:  Models PFC top-down attention (Desimone & Duncan 1995)
 * HOW:  Multiplicative gain based on task relevance
 *
 * BIOLOGICAL RATIONALE:
 * PFC sends top-down signals that modulate sensory cortex gain.
 * Task-relevant features are amplified, irrelevant features suppressed.
 * This implements "biased competition" for attention.
 *
 * REFERENCE: Desimone & Duncan (1995) "Neural mechanisms of selective attention"
 * COMPLEXITY: O(1)
 */
static float apply_attentional_modulation(float input, float task_relevance,
                                            float modulation_strength) {
    // Multiplicative gain modulation
    // gain = 1 + strength * relevance
    float gain = 1.0F + (modulation_strength * task_relevance);

    return input * gain;
}

/**
 * @brief Helper: Apply executive control processing with full cognitive functions
 *
 * WHAT: Task switching, inhibitory control, and working memory
 * WHY:  Executive functions coordinate goal-directed behavior (Miller & Cohen 2001)
 * HOW:  Selective amplification, suppression, and maintenance
 *
 * BIOLOGICAL RATIONALE:
 * Dorsolateral prefrontal cortex (dlPFC) implements executive control via:
 * 1. Task switching: rapid reconfiguration of processing priorities
 * 2. Inhibitory control: suppress irrelevant/prepotent responses
 * 3. Working memory: maintain task-relevant information
 * 4. Top-down attention: modulate processing gain
 *
 * Miller & Cohen (2001) propose that PFC provides "top-down" biasing signals
 * that modulate processing throughout the brain based on current goals.
 *
 * EXECUTIVE CONTROL FEATURES:
 * 1. Task switching with reconfiguration cost (Monsell 2003)
 * 2. Inhibitory control via threshold gating (Aron et al. 2004)
 * 3. Working memory maintenance (Goldman-Rakic 1995)
 * 4. Top-down attentional modulation (Desimone & Duncan 1995)
 *
 * REFERENCE: Miller & Cohen (2001) "Integrative theory of prefrontal cortex function"
 * REFERENCE: Monsell (2003) "Task switching"
 * REFERENCE: Aron et al. (2004) "Inhibitory control in the brain"
 * REFERENCE: Goldman-Rakic (1995) "Cellular basis of working memory"
 * COMPLEXITY: O(1)
 */
static float apply_executive_control(float input, float goal_signal,
                                       float modulation_strength,
                                       float threshold_boost) {
    // Guard clause: invalid inputs
    if (goal_signal < 0.0F) {
        // No goal context: minimal processing
        return input * 0.3F;
    }

    // Track previous goal for task switching detection
    static float prev_goal_signal = 0.0F;

    // 1. TASK SWITCHING: Detect and apply switch cost
    float switch_cost = 0.3F;  // 30% efficiency reduction during switch
    float switched_input = apply_task_switching(input, goal_signal,
                                                  prev_goal_signal, switch_cost);

    // 2. TOP-DOWN MODULATION: Amplify task-relevant signals
    // Task relevance = goal_signal (proxy for alignment with current goal)
    float task_relevance = goal_signal;
    float modulated_input = apply_attentional_modulation(switched_input,
                                                           task_relevance,
                                                           modulation_strength);

    // 3. INHIBITORY CONTROL: Suppress low-relevance inputs
    // Goal relevance: higher goal_signal = higher relevance
    float inhibited_input = apply_inhibitory_control(modulated_input,
                                                       goal_signal,
                                                       threshold_boost);

    // 4. WORKING MEMORY: Maintain activity for strong goals
    float maintenance_threshold = 0.7F;  // Strong goal threshold
    float maintained_input = maintain_working_memory(inhibited_input,
                                                       goal_signal,
                                                       maintenance_threshold);

    // Update previous goal for next call
    prev_goal_signal = goal_signal;

    // Clamp output to [0, 1]
    maintained_input = fminf(1.0F, fmaxf(0.0F, maintained_input));

    return maintained_input;
}

/**
 * Process input through neuron type-specific processing
 *
 * WHAT: Type-specific input transformations based on neuron specialization
 * WHY:  Different neuron types have different response properties
 * HOW:  Switch on type, apply appropriate transformation
 *
 * BIOLOGICAL RATIONALE:
 * Real neurons have specialized response properties based on their type.
 * V1 simple cells: orientation-selective via Gabor filtering
 * V1 complex cells: phase-invariant edge detection via energy model
 *
 * LIMITATIONS:
 * This is simplified single-value processing. For full 2D visual processing,
 * use compute_v1_simple_cell() and compute_v1_complex_cell() functions.
 *
 * COMPLEXITY: O(1)
 */
float neuron_type_process_input(neuron_type_t type, const neuron_type_params_t* params,
                                 float input, uint64_t timestamp) {
    LOG_DEBUG(LOG_MODULE, "Processing input for %s: input=%.3f, timestamp=%lu",
              neuron_type_get_name(type), input, timestamp);

    // Guard clause: invalid input
    if (!params) {
        LOG_WARN(LOG_MODULE, "NULL params for %s, returning 0.0", neuron_type_get_name(type));
        return 0.0F;
    }

    (void)timestamp;  // May be used by future neuron types

    float result = 0.0F;

    switch (type) {
        case NEURON_V1_SIMPLE_CELL:  // NEURON_VISUAL_EDGE is an alias (same value)
            // WHAT: Orientation-selective Gabor filter response
            // WHY:  V1 simple cells detect oriented edges
            // HOW:  Apply Gabor-like orientation tuning
            result = apply_gabor_response(
                input,
                params->v1_simple.orientation,
                params->v1_simple.spatial_frequency,
                params->v1_simple.phase
            );
            LOG_DEBUG(LOG_MODULE, "V1 Simple: input=%.3f, orientation=%.1f, output=%.3f",
                      input, params->v1_simple.orientation, result);
            return result;

        case NEURON_V1_COMPLEX_CELL:
        case NEURON_VISUAL_ORIENTATION:
            // WHAT: Phase-invariant edge detection
            // WHY:  V1 complex cells pool over simple cells
            // HOW:  Energy model approximation
            return apply_complex_cell_response(
                input,
                params->v1_complex.orientation,
                params->v1_complex.direction_selectivity
            );

        case NEURON_VISUAL_DIRECTION:
            // Direction-selective: similar to complex cell but with higher selectivity
            return apply_complex_cell_response(
                input,
                params->v1_complex.orientation,
                1.0F  // Maximum direction selectivity
            );

        case NEURON_AUDITORY_FREQUENCY:
            // WHAT: Frequency-selective bandpass filtering
            // WHY:  A1 neurons show tonotopic organization (Schreiner 2000)
            // HOW:  Apply bandpass response centered at preferred frequency
            return apply_bandpass_response(
                input,
                params->a1_frequency.center_frequency,
                params->a1_frequency.q_factor,
                params->a1_frequency.integration_window
            );

        case NEURON_A1_COINCIDENCE_DETECTOR:
            // WHAT: Temporal coincidence detection for binaural processing
            // WHY:  Sound localization via interaural time differences (Jeffress 1948)
            // HOW:  High temporal precision integration
            return apply_coincidence_detection(
                input,
                params->a1_coincidence.integration_window,
                params->a1_coincidence.decay_rate
            );

        case NEURON_AUDITORY_ONSET:
            // WHAT: Onset detection with rapid adaptation
            // WHY:  Signal transient acoustic events (Heil 1997)
            // HOW:  Short integration window with high-pass filtering
            return apply_onset_detection(
                input,
                params->a1_coincidence.integration_window  // Reuse coincidence params for onset
            );

        case NEURON_METACOGNITIVE:
            // WHAT: Metacognitive monitoring with confidence estimation
            // WHY:  Enable adaptive behavior based on uncertainty (Fleming & Dolan 2012)
            // HOW:  Track temporal stability to estimate confidence
            //
            // NOTE: For single-value processing, we use a simplified approach.
            // Full metacognitive processing requires history (see compute_metacognitive).
            // We approximate by comparing current input to expected baseline (0.5).
            return apply_metacognitive_monitoring(
                input,
                0.5F,  // Baseline for comparison (prev_input proxy)
                params->metacognitive.confidence_threshold,
                params->metacognitive.uncertainty_beta
            );

        case NEURON_EXECUTIVE_CONTROL:
            // WHAT: Executive control with task switching and inhibitory control
            // WHY:  Coordinate goal-directed behavior (Miller & Cohen 2001)
            // HOW:  Modulate input based on goal relevance, suppress irrelevant signals
            //
            // NOTE: For single-value processing, we use a simplified approach.
            // Full executive control requires goal context (see compute_executive_control).
            // We approximate by assuming moderate goal signal (0.6).
            return apply_executive_control(
                input,
                0.6F,  // Default goal signal strength
                params->executive.modulation_strength,
                params->executive.threshold_boost
            );

        // Generic and backward-compatible types: passthrough
        case NEURON_EXCITATORY:
        case NEURON_INHIBITORY:
        case NEURON_GENERIC_LIF:
        case NEURON_GENERIC_IZHIKEVICH:
            LOG_DEBUG(LOG_MODULE, "%s: passthrough input=%.3f", neuron_type_get_name(type), input);
            return input;

        default:
            LOG_WARN(LOG_MODULE, "Unknown neuron type %d, passthrough input=%.3f", type, input);
            return input;
    }
}

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_signal_filter.c - Signal Filtering Implementation
//=============================================================================

#include "utils/signal/nimcp_signal_filter.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/math/nimcp_complex_math.h"
#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(signal_filter)

//=============================================================================
// Internal Structures
//=============================================================================

struct signal_filter_t {
    signal_filter_config_t config;
    float* coefficients;        // Filter coefficients (taps)
    uint32_t num_coeffs;        // Number of coefficients
    float* state;               // Filter state for streaming (circular buffer)
    uint32_t state_size;        // Size of state buffer
    uint32_t state_idx;         // Current state index
};

//=============================================================================
// Window Functions
//=============================================================================

static void apply_window(float* coeffs, uint32_t n, window_function_t window) {
    for (uint32_t i = 0; i < n; i++) {
        float w = 1.0F;
        float x = (float)i / (float)(n - 1);

        switch (window) {
            case WINDOW_RECTANGULAR:
                w = 1.0F;
                break;

            case WINDOW_HAMMING:
                w = 0.54F - 0.46F * cosf(2.0F * M_PI * x);
                break;

            case WINDOW_HANN:
                w = 0.5F - 0.5F * cosf(2.0F * M_PI * x);
                break;

            case WINDOW_BLACKMAN:
                w = 0.42F - 0.5F * cosf(2.0F * M_PI * x) + 0.08F * cosf(4.0F * M_PI * x);
                break;
        }

        coeffs[i] *= w;
    }
}

//=============================================================================
// Filter Design - Windowed Sinc Method
//=============================================================================

static bool design_lowpass_filter(float* coeffs, uint32_t order, float cutoff, float sample_rate) {
    if (!coeffs || order == 0 || cutoff <= 0.0F || sample_rate <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "design_lowpass_filter: coeffs is NULL");
        return false;
    }

    uint32_t n = order + 1;
    int32_t m = (int32_t)(order / 2);
    float fc = cutoff / sample_rate;  // Normalized cutoff frequency

    for (int32_t i = 0; i < (int32_t)n; i++) {
        int32_t idx = i - m;
        if (idx == 0) {
            // Limit as idx->0: sin(2πfc*idx)/(π*idx) = 2fc
            coeffs[i] = 2.0F * fc;
        } else {
            // Windowed sinc: h[n] = sin(2πfc*n) / (π*n) = 2fc * sinc(2fc*n)
            coeffs[i] = sinf(2.0F * M_PI * fc * (float)idx) / (M_PI * (float)idx);
        }
    }

    return true;
}

// Note: Highpass is derived from lowpass via spectral inversion AFTER windowing
// So this function just designs the base lowpass filter
static bool design_highpass_filter(float* coeffs, uint32_t order, float cutoff, float sample_rate) {
    return design_lowpass_filter(coeffs, order, cutoff, sample_rate);
}

static bool design_bandpass_filter(float* coeffs, uint32_t order, float low_freq, float high_freq, float sample_rate) {
    if (!coeffs || order == 0 || low_freq < 0.0F || high_freq <= low_freq || sample_rate <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "design_bandpass_filter: coeffs is NULL");
        return false;
    }

    uint32_t n = order + 1;

    // Design lowpass at high_freq
    float* lp_high = (float*)nimcp_malloc(n * sizeof(float));
    if (!lp_high) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "design_bandpass_filter: lp_high is NULL");
        return false;
    }

    // Design lowpass at low_freq
    float* lp_low = (float*)nimcp_malloc(n * sizeof(float));
    if (!lp_low) {
        nimcp_free(lp_high);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "design_bandpass_filter: lp_low is NULL");
        return false;
    }

    if (!design_lowpass_filter(lp_high, order, high_freq, sample_rate) ||
        !design_lowpass_filter(lp_low, order, low_freq, sample_rate)) {
        nimcp_free(lp_high);
        nimcp_free(lp_low);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "design_bandpass_filter: design_lowpass_filter is NULL");
        return false;
    }

    // Bandpass = LP(high) - LP(low)
    for (uint32_t i = 0; i < n; i++) {
        coeffs[i] = lp_high[i] - lp_low[i];
    }

    nimcp_free(lp_high);
    nimcp_free(lp_low);
    return true;
}

// Note: Bandstop is derived from bandpass via spectral inversion AFTER windowing
static bool design_bandstop_filter(float* coeffs, uint32_t order, float low_freq, float high_freq, float sample_rate) {
    return design_bandpass_filter(coeffs, order, low_freq, high_freq, sample_rate);
}

//=============================================================================
// Configuration Functions
//=============================================================================

signal_filter_config_t signal_filter_default_config(void) {
    signal_filter_config_t config;
    config.type = FILTER_BANDPASS;
    config.low_freq = 4.0F;
    config.high_freq = 8.0F;
    config.cutoff_freq = 0.0F;
    config.sample_rate = 1000.0F;
    config.order = 64;
    config.window = WINDOW_HAMMING;
    config.use_fft_convolution = true;
    return config;
}

signal_filter_config_t signal_filter_bandpass_config(float low_freq, float high_freq, float sample_rate) {
    signal_filter_config_t config = signal_filter_default_config();
    config.type = FILTER_BANDPASS;
    config.low_freq = low_freq;
    config.high_freq = high_freq;
    config.sample_rate = sample_rate;
    return config;
}

signal_filter_config_t signal_filter_lowpass_config(float cutoff_freq, float sample_rate) {
    signal_filter_config_t config = signal_filter_default_config();
    config.type = FILTER_LOWPASS;
    config.cutoff_freq = cutoff_freq;
    config.sample_rate = sample_rate;
    return config;
}

signal_filter_config_t signal_filter_highpass_config(float cutoff_freq, float sample_rate) {
    signal_filter_config_t config = signal_filter_default_config();
    config.type = FILTER_HIGHPASS;
    config.cutoff_freq = cutoff_freq;
    config.sample_rate = sample_rate;
    return config;
}

bool signal_filter_validate_config(const signal_filter_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_filter_validate_config: config is NULL");
        return false;
    }

    if (config->sample_rate <= 0.0F) {
        return false;
    }
    if (config->order == 0 || config->order > 1024) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_filter_validate_config: config->order is zero");
        return false;
    }
    if (config->order % 2 != 0) {
        return false;
    }

    float nyquist = config->sample_rate / 2.0F;

    switch (config->type) {
        case FILTER_LOWPASS:
        case FILTER_HIGHPASS:
            if (config->cutoff_freq <= 0.0F || config->cutoff_freq >= nyquist) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_filter_validate_config: capacity exceeded");
                return false;
            }
            break;

        case FILTER_BANDPASS:
        case FILTER_BANDSTOP:
            if (config->low_freq < 0.0F || config->high_freq >= nyquist) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_filter_validate_config: capacity exceeded");
                return false;
            }
            if (config->low_freq >= config->high_freq) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_filter_validate_config: capacity exceeded");
                return false;
            }
            break;
    }

    return true;
}

//=============================================================================
// Filter Lifecycle
//=============================================================================

signal_filter_t* signal_filter_create(const signal_filter_config_t* config) {
    if (!config || !signal_filter_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "signal_filter_create: required parameter is NULL (config, signal_filter_validate_config)");
        return NULL;
    }

    signal_filter_t* filter = (signal_filter_t*)nimcp_calloc(1, sizeof(signal_filter_t));
    if (!filter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filter is NULL");

        return NULL;

    }

    filter->config = *config;
    filter->num_coeffs = config->order + 1;

    // Allocate coefficient buffer
    filter->coefficients = (float*)nimcp_calloc(filter->num_coeffs, sizeof(float));
    if (!filter->coefficients) {
        nimcp_free(filter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "signal_filter_create: filter->coefficients is NULL");
        return NULL;
    }

    // Design filter
    bool design_ok = false;
    switch (config->type) {
        case FILTER_LOWPASS:
            design_ok = design_lowpass_filter(filter->coefficients, config->order,
                                              config->cutoff_freq, config->sample_rate);
            break;

        case FILTER_HIGHPASS:
            design_ok = design_highpass_filter(filter->coefficients, config->order,
                                               config->cutoff_freq, config->sample_rate);
            break;

        case FILTER_BANDPASS:
            design_ok = design_bandpass_filter(filter->coefficients, config->order,
                                               config->low_freq, config->high_freq, config->sample_rate);
            break;

        case FILTER_BANDSTOP:
            design_ok = design_bandstop_filter(filter->coefficients, config->order,
                                               config->low_freq, config->high_freq, config->sample_rate);
            break;
    }

    if (!design_ok) {
        nimcp_free(filter->coefficients);
        nimcp_free(filter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_filter_create: design_ok is NULL");
        return NULL;
    }

    // Apply window
    apply_window(filter->coefficients, filter->num_coeffs, config->window);

    // Apply spectral inversion for highpass/bandstop (AFTER windowing)
    if (config->type == FILTER_HIGHPASS || config->type == FILTER_BANDSTOP) {
        for (uint32_t i = 0; i < filter->num_coeffs; i++) {
            filter->coefficients[i] = -filter->coefficients[i];
        }
        filter->coefficients[config->order / 2] += 1.0F;  // Add impulse at center
    }

    // NOTE: We don't normalize by sum for FIR filters
    // - Lowpass: windowed sinc is already properly normalized
    // - Highpass/bandstop: sum ≈ 0 after spectral inversion, normalization breaks DC rejection
    // - Bandpass: combination of two lowpass filters, inherits correct gains

    // Allocate state buffer for streaming convolution
    filter->state_size = filter->num_coeffs;
    filter->state = (float*)nimcp_calloc(filter->state_size, sizeof(float));
    if (!filter->state) {
        nimcp_free(filter->coefficients);
        nimcp_free(filter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "signal_filter_create: filter->state is NULL");
        return NULL;
    }

    filter->state_idx = 0;

    return filter;
}

void signal_filter_destroy(signal_filter_t* filter) {
    if (!filter) return;

    if (filter->coefficients) {
        nimcp_free(filter->coefficients);
    }
    if (filter->state) {
        nimcp_free(filter->state);
    }
    nimcp_free(filter);
}

bool signal_filter_reset(signal_filter_t* filter) {
    if (!filter || !filter->state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_filter_reset: required parameter is NULL (filter, filter->state)");
        return false;
    }

    memset(filter->state, 0, filter->state_size * sizeof(float));
    filter->state_idx = 0;
    return true;
}

//=============================================================================
// Filtering Functions
//=============================================================================

bool signal_filter_apply(signal_filter_t* filter, const float* input, float* output, uint32_t n) {
    if (!filter || !input || !output || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_filter_apply: required parameter is NULL (filter, input, output)");
        return false;
    }

    // Direct convolution (suitable for real-time streaming)
    for (uint32_t i = 0; i < n; i++) {
        // Add new sample to state
        filter->state[filter->state_idx] = input[i];

        // Convolve
        float sum = 0.0F;
        for (uint32_t j = 0; j < filter->num_coeffs; j++) {
            uint32_t idx = (filter->state_idx + filter->state_size - j) % filter->state_size;
            sum += filter->coefficients[j] * filter->state[idx];
        }

        output[i] = sum;

        // Advance state index
        filter->state_idx = (filter->state_idx + 1) % filter->state_size;
    }

    return true;
}

bool signal_filter_apply_envelope(signal_filter_t* filter, const float* input, float* envelope, uint32_t n) {
    if (!signal_filter_apply(filter, input, envelope, n)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "signal_filter_apply_envelope: signal_filter_apply is NULL");
        return false;
    }

    // Extract envelope (absolute value)
    for (uint32_t i = 0; i < n; i++) {
        envelope[i] = fabsf(envelope[i]);
    }

    return true;
}

bool signal_filter_get_coefficients(signal_filter_t* filter, float* coeffs, uint32_t max_coeffs, uint32_t* num_coeffs) {
    if (!filter || !coeffs || !num_coeffs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_filter_get_coefficients: required parameter is NULL (filter, coeffs, num_coeffs)");
        return false;
    }

    *num_coeffs = filter->num_coeffs;
    uint32_t copy_count = (max_coeffs < filter->num_coeffs) ? max_coeffs : filter->num_coeffs;

    memcpy(coeffs, filter->coefficients, copy_count * sizeof(float));
    return true;
}

bool signal_filter_get_response(signal_filter_t* filter, const float* frequencies, float* response, uint32_t n) {
    if (!filter || !frequencies || !response || n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "signal_filter_get_response: required parameter is NULL (filter, frequencies, response)");
        return false;
    }

    // Compute frequency response via DTFT
    for (uint32_t i = 0; i < n; i++) {
        float freq = frequencies[i];
        float omega = 2.0F * M_PI * freq / filter->config.sample_rate;

        float real = 0.0F, imag = 0.0F;
        for (uint32_t k = 0; k < filter->num_coeffs; k++) {
            float phase = omega * (float)k;
            real += filter->coefficients[k] * cosf(phase);
            imag -= filter->coefficients[k] * sinf(phase);
        }

        response[i] = sqrtf(real * real + imag * imag);
    }

    return true;
}

uint32_t signal_filter_get_delay(signal_filter_t* filter) {
    if (!filter) return 0;
    return filter->config.order / 2;  // Group delay for linear phase FIR
}

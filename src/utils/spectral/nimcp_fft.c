/**
 * @file nimcp_fft.c
 * @brief Implementation of Fast Fourier Transform
 *
 * WHAT: Cooley-Tukey FFT algorithm with neuroscience-oriented analysis
 * WHY:  Enable frequency-domain analysis of neural signals
 * HOW:  Radix-2 decimation-in-time FFT with bit-reversal and butterfly ops
 *
 * ALGORITHM OVERVIEW:
 * 1. Bit-reversal permutation of input
 * 2. Iterative butterfly operations (log2(N) stages)
 * 3. Each stage combines pairs with twiddle factors
 * 4. In-place computation for memory efficiency
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6 (GPU P2P + Spectral Analysis)
 */

#include "utils/spectral/nimcp_fft.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fft)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief FFT plan implementation
 */
struct fft_plan_struct {
    uint32_t size;               /**< FFT size (power of 2) */
    fft_type_t type;             /**< FFT type */
    fft_window_t window;         /**< Window function */

    // Pre-computed coefficients
    fft_complex_t* twiddle;      /**< Twiddle factors e^(-2πik/N) */
    uint32_t* bit_reverse;       /**< Bit-reversal permutation table */
    float* window_coeffs;        /**< Window function coefficients */

    // Metadata
    uint32_t log2_size;          /**< log2(size) for algorithm */
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Round up to next power of 2
 */
uint32_t fft_next_power_of_2(uint32_t n)
{
    if (n == 0) {
        return 1;
    }

    // Already power of 2?
    if ((n & (n - 1)) == 0) {
        return n;
    }

    // Round up
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;

    return n;
}

/**
 * @brief Check if number is power of 2
 */
bool fft_is_power_of_2(uint32_t n)
{
    return (n > 0) && ((n & (n - 1)) == 0);
}

/**
 * @brief Compute log2 of integer
 */
static uint32_t log2_int(uint32_t n)
{
    uint32_t log = 0;
    while (n >>= 1) {
        log++;
    }
    return log;
}

/**
 * @brief Reverse bits of integer
 */
static uint32_t reverse_bits(uint32_t x, uint32_t bits)
{
    uint32_t result = 0;
    for (uint32_t i = 0; i < bits; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

//=============================================================================
// Window Functions
//=============================================================================

/**
 * @brief Compute Hann window coefficients
 */
static void compute_hann_window(float* window, uint32_t size)
{
    for (uint32_t n = 0; n < size; n++) {
        window[n] = 0.5F * (1.0F - cosf(2.0F * M_PI * n / (size - 1)));
    }
}

/**
 * @brief Compute Hamming window coefficients
 */
static void compute_hamming_window(float* window, uint32_t size)
{
    for (uint32_t n = 0; n < size; n++) {
        window[n] = 0.54F - 0.46F * cosf(2.0F * M_PI * n / (size - 1));
    }
}

/**
 * @brief Compute Blackman window coefficients
 */
static void compute_blackman_window(float* window, uint32_t size)
{
    const float a0 = 0.42F;
    const float a1 = 0.5F;
    const float a2 = 0.08F;

    for (uint32_t n = 0; n < size; n++) {
        float cos1 = cosf(2.0F * M_PI * n / (size - 1));
        float cos2 = cosf(4.0F * M_PI * n / (size - 1));
        window[n] = a0 - a1 * cos1 + a2 * cos2;
    }
}

/**
 * @brief Apply window function to signal
 */
bool fft_apply_window(float* signal, uint32_t size, fft_window_t window)
{
    // Guard: Validate inputs
    if (!signal || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fft_apply_window: signal is NULL");
        return false;
    }

    if (window == FFT_WINDOW_NONE) {
        return true;  // No windowing
    }

    // Allocate temporary window
    float* win_coeffs = (float*)nimcp_malloc(size * sizeof(float));
    if (!win_coeffs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_apply_window: win_coeffs is NULL");
        return false;
    }

    // Compute window coefficients
    switch (window) {
        case FFT_WINDOW_HANN:
            compute_hann_window(win_coeffs, size);
            break;
        case FFT_WINDOW_HAMMING:
            compute_hamming_window(win_coeffs, size);
            break;
        case FFT_WINDOW_BLACKMAN:
            compute_blackman_window(win_coeffs, size);
            break;
        default:
            nimcp_free(win_coeffs);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fft_apply_window: operation failed");
            return false;
    }

    // Apply window
    for (uint32_t i = 0; i < size; i++) {
        signal[i] *= win_coeffs[i];
    }

    nimcp_free(win_coeffs);
    return true;
}

//=============================================================================
// FFT Plan Management
//=============================================================================

/**
 * @brief Create FFT plan
 */
fft_plan_t* fft_plan_create(uint32_t size, fft_type_t type)
{
    // Guard: Validate size is power of 2
    if (!fft_is_power_of_2(size) || size < 2 || size > 65536) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_plan_create: fft_is_power_of_2 is NULL");
        return NULL;
    }

    // Allocate plan
    fft_plan_t* plan = (fft_plan_t*)nimcp_calloc(1, sizeof(fft_plan_t));
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plan is NULL");

        return NULL;
    }

    plan->size = size;
    plan->type = type;
    plan->window = FFT_WINDOW_NONE;
    plan->log2_size = log2_int(size);

    // Allocate twiddle factors
    plan->twiddle = (fft_complex_t*)nimcp_malloc(size * sizeof(fft_complex_t));
    if (!plan->twiddle) {
        fft_plan_destroy(plan);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_plan_create: plan->twiddle is NULL");
        return NULL;
    }

    // Compute twiddle factors: W_N^k = e^(-2πik/N)
    for (uint32_t k = 0; k < size; k++) {
        float angle = -2.0F * M_PI * k / size;
        plan->twiddle[k].real = cosf(angle);
        plan->twiddle[k].imag = sinf(angle);
    }

    // Allocate bit-reversal table
    plan->bit_reverse = (uint32_t*)nimcp_malloc(size * sizeof(uint32_t));
    if (!plan->bit_reverse) {
        fft_plan_destroy(plan);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_plan_create: plan->bit_reverse is NULL");
        return NULL;
    }

    // Compute bit-reversal permutation
    for (uint32_t i = 0; i < size; i++) {
        plan->bit_reverse[i] = reverse_bits(i, plan->log2_size);
    }

    plan->window_coeffs = NULL;  // Allocated on demand

    return plan;
}

/**
 * @brief Destroy FFT plan
 */
void fft_plan_destroy(fft_plan_t* plan)
{
    if (!plan) {
        return;
    }

    if (plan->twiddle) {
        nimcp_free(plan->twiddle);
    }

    if (plan->bit_reverse) {
        nimcp_free(plan->bit_reverse);
    }

    if (plan->window_coeffs) {
        nimcp_free(plan->window_coeffs);
    }

    nimcp_free(plan);
}

/**
 * @brief Set window function
 */
bool fft_plan_set_window(fft_plan_t* plan, fft_window_t window)
{
    // Guard: Validate plan
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_plan_set_window: plan is NULL");
        return false;
    }

    plan->window = window;

    // Free old window coefficients
    if (plan->window_coeffs) {
        nimcp_free(plan->window_coeffs);
        plan->window_coeffs = NULL;
    }

    // Compute new window coefficients if needed
    if (window != FFT_WINDOW_NONE) {
        plan->window_coeffs = (float*)nimcp_malloc(plan->size * sizeof(float));
        if (!plan->window_coeffs) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_plan_set_window: plan->window_coeffs is NULL");
            return false;
        }

        switch (window) {
            case FFT_WINDOW_HANN:
                compute_hann_window(plan->window_coeffs, plan->size);
                break;
            case FFT_WINDOW_HAMMING:
                compute_hamming_window(plan->window_coeffs, plan->size);
                break;
            case FFT_WINDOW_BLACKMAN:
                compute_blackman_window(plan->window_coeffs, plan->size);
                break;
            default:
                nimcp_free(plan->window_coeffs);
                plan->window_coeffs = NULL;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fft_plan_set_window: operation failed");
                return false;
        }
    }

    return true;
}

/**
 * @brief Get FFT size
 */
uint32_t fft_plan_get_size(const fft_plan_t* plan)
{
    return plan ? plan->size : 0;
}

//=============================================================================
// Core FFT Algorithm (Cooley-Tukey)
//=============================================================================

/**
 * @brief Complex multiplication: out = a * b
 */
static inline void complex_mul(const fft_complex_t* a, const fft_complex_t* b,
                               fft_complex_t* out)
{
    out->real = a->real * b->real - a->imag * b->imag;
    out->imag = a->real * b->imag + a->imag * b->real;
}

/**
 * @brief In-place Cooley-Tukey FFT
 *
 * ALGORITHM:
 * 1. Bit-reverse input
 * 2. Iterative butterfly operations
 * 3. Each stage combines pairs with increasing distance
 */
static void fft_cooley_tukey(fft_complex_t* data, fft_plan_t* plan)
{
    uint32_t n = plan->size;

    // Bit-reversal permutation
    for (uint32_t i = 0; i < n; i++) {
        uint32_t j = plan->bit_reverse[i];
        if (i < j) {
            // Swap data[i] and data[j]
            fft_complex_t temp = data[i];
            data[i] = data[j];
            data[j] = temp;
        }
    }

    // Iterative FFT (log2(N) stages)
    for (uint32_t stage = 1; stage <= plan->log2_size; stage++) {
        uint32_t m = 1 << stage;        // 2^stage
        uint32_t m2 = m >> 1;            // m/2

        // Twiddle factor stride
        uint32_t twiddle_stride = n / m;

        // Process each group
        for (uint32_t k = 0; k < n; k += m) {
            // Butterfly operations within group
            for (uint32_t j = 0; j < m2; j++) {
                uint32_t idx_even = k + j;
                uint32_t idx_odd = k + j + m2;

                // Get twiddle factor W_N^(j * twiddle_stride)
                fft_complex_t twiddle = plan->twiddle[j * twiddle_stride];

                // Butterfly: t = W * data[odd]
                fft_complex_t t;
                complex_mul(&twiddle, &data[idx_odd], &t);

                // data[odd] = data[even] - t
                fft_complex_t u = data[idx_even];
                data[idx_odd].real = u.real - t.real;
                data[idx_odd].imag = u.imag - t.imag;

                // data[even] = data[even] + t
                data[idx_even].real = u.real + t.real;
                data[idx_even].imag = u.imag + t.imag;
            }
        }
    }
}

//=============================================================================
// FFT Execution
//=============================================================================

/**
 * @brief Execute real-to-complex FFT
 */
bool fft_execute_real(fft_plan_t* plan, const float* input, fft_complex_t* output)
{
    // Guard: Validate inputs
    if (!plan || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_execute_real: required parameter is NULL (plan, input, output)");
        return false;
    }

    if (plan->type != FFT_REAL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fft_execute_real: validation failed");
        return false;
    }

    uint32_t n = plan->size;

    // Allocate temporary complex buffer
    fft_complex_t* temp = (fft_complex_t*)nimcp_calloc(n, sizeof(fft_complex_t));
    if (!temp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_execute_real: temp is NULL");
        return false;
    }

    // Convert real input to complex (with windowing if enabled)
    for (uint32_t i = 0; i < n; i++) {
        temp[i].real = input[i];
        if (plan->window_coeffs) {
            temp[i].real *= plan->window_coeffs[i];
        }
        temp[i].imag = 0.0F;
    }

    // Execute FFT
    fft_cooley_tukey(temp, plan);

    // Copy output (only first N/2 + 1 bins for real FFT)
    uint32_t output_size = n / 2 + 1;
    for (uint32_t i = 0; i < output_size; i++) {
        output[i] = temp[i];
    }

    nimcp_free(temp);
    return true;
}

/**
 * @brief Execute complex-to-complex FFT
 */
bool fft_execute_complex(fft_plan_t* plan, const fft_complex_t* input,
                         fft_complex_t* output)
{
    // Guard: Validate inputs
    if (!plan || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_execute_complex: required parameter is NULL (plan, input, output)");
        return false;
    }

    uint32_t n = plan->size;

    // Copy input to output (for in-place operation)
    if (input != output) {
        memcpy(output, input, n * sizeof(fft_complex_t));
    }

    // Execute FFT
    fft_cooley_tukey(output, plan);

    return true;
}

/**
 * @brief Execute inverse real FFT
 */
bool fft_execute_inverse_real(fft_plan_t* plan, const fft_complex_t* input,
                               float* output)
{
    // Guard: Validate inputs
    if (!plan || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_execute_inverse_real: required parameter is NULL (plan, input, output)");
        return false;
    }

    if (plan->type != FFT_INVERSE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fft_execute_inverse_real: validation failed");
        return false;
    }

    uint32_t n = plan->size;

    // Allocate temporary buffer
    fft_complex_t* temp = (fft_complex_t*)nimcp_calloc(n, sizeof(fft_complex_t));
    if (!temp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_execute_inverse_real: temp is NULL");
        return false;
    }

    // Copy input and mirror for Hermitian symmetry
    uint32_t n2 = n / 2;
    temp[0] = input[0];
    for (uint32_t i = 1; i < n2; i++) {
        temp[i] = input[i];
        // Mirror: X[N-k] = conj(X[k])
        temp[n - i].real = input[i].real;
        temp[n - i].imag = -input[i].imag;
    }
    temp[n2] = input[n2];

    // Conjugate
    for (uint32_t i = 0; i < n; i++) {
        temp[i].imag = -temp[i].imag;
    }

    // Execute FFT
    fft_cooley_tukey(temp, plan);

    // Conjugate and scale by 1/N
    float scale = 1.0F / n;
    for (uint32_t i = 0; i < n; i++) {
        output[i] = temp[i].real * scale;
    }

    nimcp_free(temp);
    return true;
}

/**
 * @brief Execute inverse complex-to-complex FFT
 *
 * WHAT: Transform complex frequency spectrum back to complex time domain
 * WHY:  Needed for Hilbert transform and complex signal reconstruction
 * HOW:  Conjugate input, FFT, conjugate and scale output
 *
 * @param plan FFT plan (must be FFT_INVERSE type)
 * @param input Complex frequency spectrum [size]
 * @param output Complex time-domain signal [size]
 * @return true on success, false on failure
 *
 * ALGORITHM:
 * 1. Conjugate input spectrum
 * 2. Execute forward FFT
 * 3. Conjugate output and scale by 1/N
 *
 * COMPLEXITY: O(N log N)
 * USE CASES: Hilbert transform, complex filtering
 */
bool fft_execute_inverse_complex(
    fft_plan_t* plan,
    const fft_complex_t* input,
    fft_complex_t* output)
{
    // Guard: Validate inputs
    if (!plan || !input || !output || plan->type != FFT_INVERSE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_execute_inverse_complex: required parameter is NULL (plan, input, output)");
        return false;
    }

    uint32_t n = plan->size;

    // Allocate temporary buffer for conjugated input
    fft_complex_t* temp = (fft_complex_t*)nimcp_calloc(n, sizeof(fft_complex_t));
    if (!temp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "fft_execute_inverse_complex: temp is NULL");
        return false;
    }

    // Conjugate input
    for (uint32_t i = 0; i < n; i++) {
        temp[i].real = input[i].real;
        temp[i].imag = -input[i].imag;
    }

    // Execute FFT (forward FFT on conjugated input)
    fft_cooley_tukey(temp, plan);

    // Conjugate and scale output by 1/N
    float scale = 1.0F / n;
    for (uint32_t i = 0; i < n; i++) {
        output[i].real = temp[i].real * scale;
        output[i].imag = -temp[i].imag * scale;
    }

    nimcp_free(temp);
    return true;
}

//=============================================================================
// Power Spectral Density
//=============================================================================

/**
 * @brief Compute power spectrum
 */
bool fft_power_spectrum(const fft_complex_t* spectrum, float* power, uint32_t size)
{
    // Guard: Validate inputs
    if (!spectrum || !power || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_power_spectrum: required parameter is NULL (spectrum, power)");
        return false;
    }

    for (uint32_t i = 0; i < size; i++) {
        float re = spectrum[i].real;
        float im = spectrum[i].imag;
        power[i] = re * re + im * im;
    }

    return true;
}

/**
 * @brief Compute power spectrum in dB
 */
bool fft_power_spectrum_db(const fft_complex_t* spectrum, float* psd_db,
                           uint32_t size)
{
    // Guard: Validate inputs
    if (!spectrum || !psd_db || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_power_spectrum_db: required parameter is NULL (spectrum, psd_db)");
        return false;
    }

    for (uint32_t i = 0; i < size; i++) {
        float re = spectrum[i].real;
        float im = spectrum[i].imag;
        float power = re * re + im * im;

        // Convert to dB (with floor to avoid log(0))
        psd_db[i] = 10.0F * log10f(fmaxf(power, 1e-10F));
    }

    return true;
}

/**
 * @brief Compute magnitude spectrum
 */
bool fft_magnitude_spectrum(const fft_complex_t* spectrum, float* magnitude,
                            uint32_t size)
{
    // Guard: Validate inputs
    if (!spectrum || !magnitude || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_magnitude_spectrum: required parameter is NULL (spectrum, magnitude)");
        return false;
    }

    for (uint32_t i = 0; i < size; i++) {
        float re = spectrum[i].real;
        float im = spectrum[i].imag;
        magnitude[i] = sqrtf(re * re + im * im);
    }

    return true;
}

/**
 * @brief Compute phase spectrum
 */
bool fft_phase_spectrum(const fft_complex_t* spectrum, float* phase, uint32_t size)
{
    // Guard: Validate inputs
    if (!spectrum || !phase || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fft_phase_spectrum: required parameter is NULL (spectrum, phase)");
        return false;
    }

    for (uint32_t i = 0; i < size; i++) {
        phase[i] = atan2f(spectrum[i].imag, spectrum[i].real);
    }

    return true;
}

//=============================================================================
// Spectral Analysis Utilities
//=============================================================================

/**
 * @brief Find dominant frequency
 */
float fft_dominant_frequency(const float* power, uint32_t size, float sampling_rate)
{
    // Guard: Validate inputs
    if (!power || size == 0 || sampling_rate <= 0.0F) {
        return 0.0F;
    }

    // Find peak
    uint32_t peak_bin = 0;
    float peak_power = power[0];

    for (uint32_t i = 1; i < size; i++) {
        if (power[i] > peak_power) {
            peak_power = power[i];
            peak_bin = i;
        }
    }

    // Convert bin to frequency
    // For power spectrum with 'size' bins, the FFT size is size * 2
    // (since power spectrum typically has N/2+1 bins from N-point FFT,
    //  but here 'size' refers to the number of bins in the power array)
    uint32_t fft_size = size * 2;
    return fft_bin_to_frequency(peak_bin, fft_size, sampling_rate);
}

/**
 * @brief Compute band power
 */
float fft_band_power(const float* power, uint32_t size, float sampling_rate,
                     float freq_low, float freq_high)
{
    // Guard: Validate inputs
    if (!power || size == 0 || sampling_rate <= 0.0F) {
        return 0.0F;
    }

    if (freq_low < 0.0F || freq_high > sampling_rate / 2.0F || freq_low >= freq_high) {
        return 0.0F;
    }

    // Convert frequencies to bins
    // For real FFT power spectrum with 'size' bins, the FFT size is (size-1)*2
    // because spectrum_size = fft_size/2 + 1, so fft_size = (spectrum_size-1)*2
    uint32_t fft_size = (size - 1) * 2;  // Real FFT size
    int32_t bin_low = fft_frequency_to_bin(freq_low, fft_size, sampling_rate);
    int32_t bin_high = fft_frequency_to_bin(freq_high, fft_size, sampling_rate);

    if (bin_low < 0 || bin_high < 0 || (uint32_t)bin_high >= size) {
        return 0.0F;
    }

    // Sum power in band
    float total_power = 0.0F;
    for (int32_t i = bin_low; i <= bin_high; i++) {
        total_power += power[i];
    }

    return total_power;
}

/**
 * @brief Convert frequency to bin
 */
int32_t fft_frequency_to_bin(float frequency, uint32_t fft_size, float sampling_rate)
{
    if (frequency < 0.0F || frequency > sampling_rate / 2.0F || sampling_rate <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "fft_frequency_to_bin: validation failed");
        return -1;
    }

    float bin_f = frequency * fft_size / sampling_rate;
    int32_t bin = (int32_t)(bin_f + 0.5F);  // Round to nearest

    // Clamp to valid range
    if (bin >= (int32_t)(fft_size / 2 + 1)) {
        bin = fft_size / 2;
    }

    return bin;
}

/**
 * @brief Convert bin to frequency
 */
float fft_bin_to_frequency(uint32_t bin, uint32_t fft_size, float sampling_rate)
{
    return (float)bin * sampling_rate / fft_size;
}

//=============================================================================
// Brain Wave Analysis
//=============================================================================

/**
 * @brief Compute brain wave band power
 */
float fft_brain_wave_power(const float* power, uint32_t size, float sampling_rate,
                           brain_wave_band_t band)
{
    float freq_low, freq_high;

    switch (band) {
        case BRAIN_WAVE_DELTA:
            freq_low = 1.0F;
            freq_high = 4.0F;
            break;
        case BRAIN_WAVE_THETA:
            freq_low = 4.0F;
            freq_high = 8.0F;
            break;
        case BRAIN_WAVE_ALPHA:
            freq_low = 8.0F;
            freq_high = 13.0F;
            break;
        case BRAIN_WAVE_BETA:
            freq_low = 13.0F;
            freq_high = 30.0F;
            break;
        case BRAIN_WAVE_GAMMA:
            freq_low = 30.0F;
            freq_high = 100.0F;
            break;
        default:
            return 0.0F;
    }

    return fft_band_power(power, size, sampling_rate, freq_low, freq_high);
}

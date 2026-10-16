//=============================================================================
// nimcp_hilbert.h - Hilbert Transform for Analytic Signal Computation
//=============================================================================
/**
 * @file nimcp_hilbert.h
 * @brief Hilbert transform for instantaneous amplitude and phase extraction
 *
 * WHAT: FFT-based Hilbert transform to compute analytic signals
 * WHY:  PAC detection and oscillation analysis require amplitude envelopes and phases
 * HOW:  Zero negative frequencies in FFT domain → analytic signal = A(t)·e^(iφ(t))
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Analytic Signal Representation:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  Real Signal: x(t) = A(t)·cos(φ(t))                         │
 *   │    ╱╲      ╱╲      ╱╲                                       │
 *   │   ╱  ╲    ╱  ╲    ╱  ╲                                      │
 *   │  ╱    ╲  ╱    ╲  ╱    ╲                                     │
 *   │ ╱      ╲╱      ╲╱      ╲                                    │
 *   │                                                              │
 *   │  Analytic Signal: z(t) = x(t) + i·Hilbert[x(t)]           │
 *   │                         = A(t)·e^(i·φ(t))                  │
 *   │                                                              │
 *   │  Instantaneous Amplitude: A(t) = |z(t)|                    │
 *   │  Instantaneous Phase: φ(t) = arg(z(t))                     │
 *   └──────────────────────────────────────────────────────────────┘
 *
 *   Applications in Neuroscience:
 *   - Phase-Amplitude Coupling (PAC): Theta phase modulates gamma amplitude
 *   - Oscillation Detection: Track amplitude envelopes across frequency bands
 *   - Phase Synchrony: Measure phase locking between brain regions
 *   - Instantaneous Frequency: ω(t) = dφ/dt for time-varying oscillations
 *
 * DESIGN PRINCIPLES:
 * - SRP: Single Responsibility - Hilbert transform and analytic signal only
 * - Modular: Separate transform, envelope, phase extraction
 * - Performance: FFT-based O(n log n), supports non-power-of-2 via padding
 * - Memory-safe: All allocations via nimcp_memory API
 *
 * USAGE:
 *   hilbert_config_t config = hilbert_default_config();
 *   hilbert_transform_t* ht = hilbert_create(&config);
 *
 *   // Compute analytic signal
 *   hilbert_apply(ht, signal, analytic, n);
 *
 *   // Or extract envelope and phase directly
 *   hilbert_extract_envelope(ht, signal, envelope, n);
 *   hilbert_extract_phase(ht, signal, phase, n);
 *
 *   hilbert_destroy(ht);
 *
 * PERFORMANCE:
 * - Transform (n=1024): ~80µs (includes 2x FFT)
 * - Envelope extraction (n=1024): ~85µs (transform + magnitude)
 * - Phase extraction (n=1024): ~90µs (transform + atan2)
 * - Batch transform (4 channels, n=1024): ~250µs (parallelized)
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 * @version 1.0.0
 */

#ifndef NIMCP_HILBERT_H
#define NIMCP_HILBERT_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/math/nimcp_complex_math.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Hilbert transform configuration
 */
typedef struct {
    bool auto_pad_power_of_2;     ///< Automatically pad to next power of 2
    bool enable_simd;             ///< Use SIMD optimization for envelope/phase
    uint32_t max_signal_length;   ///< Maximum expected signal length (for pre-allocation)
} hilbert_config_t;

/**
 * @brief Hilbert transform handle (opaque)
 */
typedef struct hilbert_transform_t hilbert_transform_t;

/**
 * @brief Analytic signal result
 */
typedef struct {
    neural_phasor_t* analytic;    ///< Complex analytic signal z(t)
    float* amplitude;             ///< Instantaneous amplitude |z(t)|
    float* phase;                 ///< Instantaneous phase arg(z(t))
    uint32_t length;              ///< Signal length
    bool owns_memory;             ///< True if struct owns the buffers
} hilbert_result_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default Hilbert transform configuration
 * @return Default configuration
 */
hilbert_config_t hilbert_default_config(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create Hilbert transform processor
 * @param config Configuration (NULL for defaults)
 * @return Transform handle or NULL on failure
 */
hilbert_transform_t* hilbert_create(const hilbert_config_t* config);

/**
 * @brief Destroy Hilbert transform processor
 * @param ht Transform to destroy
 */
void hilbert_destroy(hilbert_transform_t* ht);

//=============================================================================
// Core Transform Functions
//=============================================================================

/**
 * @brief Compute analytic signal via Hilbert transform
 *
 * Computes z(t) = x(t) + i·H[x(t)] where H is the Hilbert transform.
 * The result is a complex signal with instantaneous amplitude and phase.
 *
 * @param ht Hilbert transform handle
 * @param signal Input real signal
 * @param analytic Output analytic signal (must have space for n samples)
 * @param n Signal length
 * @return True on success, false on error
 *
 * Performance: ~80µs for n=1024
 *
 * Example:
 *   float signal[1024];
 *   neural_phasor_t analytic[1024];
 *   hilbert_apply(ht, signal, analytic, 1024);
 *   // Now: amplitude = phasor_amplitude(analytic[i])
 *   //      phase = phasor_phase(analytic[i])
 */
bool hilbert_apply(hilbert_transform_t* ht,
                   const float* signal,
                   neural_phasor_t* analytic,
                   uint32_t n);

/**
 * @brief Extract instantaneous amplitude envelope
 *
 * Computes A(t) = |z(t)| = sqrt(x²(t) + H[x(t)]²) where z is analytic signal.
 * This is the amplitude envelope of the oscillation.
 *
 * @param ht Hilbert transform handle
 * @param signal Input real signal
 * @param amplitude Output amplitude envelope
 * @param n Signal length
 * @return True on success, false on error
 *
 * Performance: ~85µs for n=1024
 *
 * Neuroscience application:
 * - Gamma amplitude for PAC detection
 * - Oscillation strength tracking
 * - Event-related power changes
 */
bool hilbert_extract_amplitude(hilbert_transform_t* ht,
                                const float* signal,
                                float* amplitude,
                                uint32_t n);

/**
 * @brief Extract instantaneous phase
 *
 * Computes φ(t) = arg(z(t)) = atan2(H[x(t)], x(t)) where z is analytic signal.
 * Phase is returned in radians [-π, π].
 *
 * @param ht Hilbert transform handle
 * @param signal Input real signal
 * @param phase Output instantaneous phase (radians)
 * @param n Signal length
 * @return True on success, false on error
 *
 * Performance: ~90µs for n=1024
 *
 * Neuroscience application:
 * - Theta phase for PAC detection
 * - Phase-locking analysis
 * - Phase precession in place cells
 */
bool hilbert_extract_phase(hilbert_transform_t* ht,
                            const float* signal,
                            float* phase,
                            uint32_t n);

/**
 * @brief Extract both amplitude and phase simultaneously
 *
 * More efficient than calling extract_amplitude and extract_phase separately.
 * Computes analytic signal once and extracts both components.
 *
 * @param ht Hilbert transform handle
 * @param signal Input real signal
 * @param amplitude Output amplitude envelope
 * @param phase Output instantaneous phase (radians)
 * @param n Signal length
 * @return True on success, false on error
 *
 * Performance: ~95µs for n=1024 (vs ~175µs for separate calls)
 */
bool hilbert_extract_amplitude_phase(hilbert_transform_t* ht,
                                      const float* signal,
                                      float* amplitude,
                                      float* phase,
                                      uint32_t n);

//=============================================================================
// Batch Processing (Multi-Channel)
//=============================================================================

/**
 * @brief Compute analytic signals for multiple channels
 *
 * Processes multiple channels in parallel (if SIMD available).
 * All channels must have the same length.
 *
 * @param ht Hilbert transform handle
 * @param signals Array of input signals (num_channels pointers)
 * @param analytic Array of output analytic signals (num_channels pointers)
 * @param n Signal length (same for all channels)
 * @param num_channels Number of channels to process
 * @return True on success, false on error
 *
 * Performance: ~250µs for 4 channels × 1024 samples
 *
 * Example:
 *   float* signals[4] = {channel1, channel2, channel3, channel4};
 *   neural_phasor_t* analytic[4] = {out1, out2, out3, out4};
 *   hilbert_apply_batch(ht, signals, analytic, 1024, 4);
 */
bool hilbert_apply_batch(hilbert_transform_t* ht,
                          const float** signals,
                          neural_phasor_t** analytic,
                          uint32_t n,
                          uint32_t num_channels);

/**
 * @brief Extract amplitude envelopes for multiple channels
 *
 * @param ht Hilbert transform handle
 * @param signals Array of input signals (num_channels pointers)
 * @param amplitudes Array of output amplitudes (num_channels pointers)
 * @param n Signal length (same for all channels)
 * @param num_channels Number of channels to process
 * @return True on success, false on error
 */
bool hilbert_extract_amplitude_batch(hilbert_transform_t* ht,
                                      const float** signals,
                                      float** amplitudes,
                                      uint32_t n,
                                      uint32_t num_channels);

/**
 * @brief Extract instantaneous phases for multiple channels
 *
 * @param ht Hilbert transform handle
 * @param signals Array of input signals (num_channels pointers)
 * @param phases Array of output phases (num_channels pointers)
 * @param n Signal length (same for all channels)
 * @param num_channels Number of channels to process
 * @return True on success, false on error
 */
bool hilbert_extract_phase_batch(hilbert_transform_t* ht,
                                  const float** signals,
                                  float** phases,
                                  uint32_t n,
                                  uint32_t num_channels);

//=============================================================================
// Result Management
//=============================================================================

/**
 * @brief Create result structure (allocates buffers)
 * @param n Signal length
 * @return Allocated result structure or NULL on failure
 */
hilbert_result_t* hilbert_result_create(uint32_t n);

/**
 * @brief Destroy result structure (frees buffers)
 * @param result Result to destroy
 */
void hilbert_result_destroy(hilbert_result_t* result);

/**
 * @brief Compute full analytic signal with all components
 *
 * Convenience function that allocates result and computes everything.
 * Caller must call hilbert_result_destroy() when done.
 *
 * @param ht Hilbert transform handle
 * @param signal Input real signal
 * @param n Signal length
 * @return Allocated result with analytic signal, amplitude, phase, or NULL on error
 */
hilbert_result_t* hilbert_compute_full(hilbert_transform_t* ht,
                                        const float* signal,
                                        uint32_t n);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute instantaneous frequency from phase
 *
 * ω(t) = dφ/dt computed via central differences.
 * Returns frequency in radians/sample (multiply by sample_rate/(2π) for Hz).
 *
 * @param phase Instantaneous phase (radians)
 * @param frequency Output instantaneous frequency (rad/sample)
 * @param n Signal length
 * @param sample_rate Sampling rate (Hz) - for Hz output
 * @return True on success, false on error
 *
 * Example:
 *   float phase[1024], freq_hz[1024];
 *   hilbert_extract_phase(ht, signal, phase, 1024);
 *   hilbert_instantaneous_frequency(phase, freq_hz, 1024, 1000.0f);
 */
bool hilbert_instantaneous_frequency(const float* phase,
                                      float* frequency,
                                      uint32_t n,
                                      float sample_rate);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return True if valid, false otherwise
 */
bool hilbert_validate_config(const hilbert_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HILBERT_H

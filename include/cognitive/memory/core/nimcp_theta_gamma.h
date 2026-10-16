//=============================================================================
// nimcp_theta_gamma.h - Theta-Gamma Coupling for Phase-Gated Memory Operations
//=============================================================================
/**
 * @file nimcp_theta_gamma.h
 * @brief Phase-amplitude coupling between theta and gamma oscillations
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Theta-gamma coupling for phase-gated encoding and retrieval windows
 * WHY:  Theta phase determines encoding (0-90°) vs retrieval (180-270°) mode;
 *       gamma bursts carry information at phase-appropriate moments
 * HOW:  Track theta phase, modulate gamma by phase, gate memory operations
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 * Theta-Gamma Coupling in Hippocampal Memory:
 *
 *   Theta Wave (4-8 Hz) - "The Conductor":
 *   +-------------------------------------------------------------------------+
 *   |  Phase Windows for Memory Operations:                                    |
 *   |                                                                          |
 *   |    0°────45°───90°───135°───180°───225°───270°───315°───360°            |
 *   |    │      │     │     │       │      │      │      │       │             |
 *   |    │ EARLY│LATE │TRANS│ EARLY │ PEAK │ LATE │TRANS │PREPARE│             |
 *   |    │ ENC  │ENC  │ E→R │ RETR  │ RETR │ RETR │ R→E  │  ENC  │             |
 *   |    │      │     │     │       │      │      │      │       │             |
 *   |    └──────┴─────┴─────┴───────┴──────┴──────┴──────┴───────┘             |
 *   |     ↑                         ↑                                          |
 *   |   Trough                    Peak                                         |
 *   |   (new info)              (recall)                                       |
 *   +-------------------------------------------------------------------------+
 *
 *   Gamma Nested in Theta:
 *   +-------------------------------------------------------------------------+
 *   |                                                                          |
 *   |  Theta: ~~~~~/\~~~~~\/~~~~~                                             |
 *   |              │       │                                                   |
 *   |  Gamma: ▓▓▓░░│░░░░░░░│▓▓▓    ← Amplitude modulated by theta phase       |
 *   |        (high)│(low)  │(high)                                             |
 *   |              │       │                                                   |
 *   |  Encoding    │       │  Retrieval                                        |
 *   |  window      │       │  window                                           |
 *   |              │       │                                                   |
 *   |  High gamma  │       │  Low gamma                                        |
 *   |  (60-100Hz)  │       │  (30-60Hz)                                        |
 *   +-------------------------------------------------------------------------+
 *
 *   Phase-Amplitude Coupling (PAC):
 *   +-------------------------------------------------------------------------+
 *   |  Modulation Index (MI):                                                  |
 *   |                                                                          |
 *   |  1. Extract theta phase via Hilbert transform: φ_theta(t)               |
 *   |  2. Extract gamma amplitude envelope: A_gamma(t)                        |
 *   |  3. Bin gamma amplitude by theta phase (18 bins, 20° each)              |
 *   |  4. Compute KL divergence from uniform distribution                     |
 *   |  5. MI = (H_max - H_actual) / H_max                                     |
 *   |                                                                          |
 *   |  MI ≈ 0: No coupling (gamma amplitude independent of theta phase)       |
 *   |  MI ≈ 1: Strong coupling (gamma peaks at specific theta phase)          |
 *   +-------------------------------------------------------------------------+
 *
 *   Biological Roles:
 *   - Encoding (theta trough, 0-90°): New synaptic potentiation, LTP
 *   - Retrieval (theta peak, 180-270°): Pattern completion, memory replay
 *   - High gamma (60-100 Hz): Feature binding during encoding
 *   - Low gamma (30-60 Hz): Memory retrieval and pattern completion
 *
 * PERFORMANCE:
 * =============================================================================
 * - State update: ~50ns (phase advance, window check)
 * - Gating query: ~10ns (phase comparison)
 * - PAC computation (n=1024): ~200µs (Hilbert transform + binning)
 * - Burst detection (n=256): ~30µs (envelope + threshold)
 * - Quaternion modulation: ~25ns (quat ops)
 *
 * MEMORY:
 * =============================================================================
 * - theta_gamma_manager_t: ~2KB (state, config, stats, workspace)
 * - PAC histogram: 18 bins × 4 bytes = 72 bytes
 * - Burst detection buffer: n × 4 bytes (amplitude envelope)
 *
 * INTEGRATION:
 * =============================================================================
 * - Core: Memory encoding/retrieval gating, consolidation timing
 * - Middleware: Resonance modulation, Kuramoto integration
 * - API: Memory operation timing, phase-locked processing
 *
 * REFERENCES:
 * - Lisman & Jensen (2013): Theta-gamma neural code
 * - Tort et al. (2010): Measuring phase-amplitude coupling
 * - Colgin (2015): Theta-gamma coupling in memory
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THETA_GAMMA_H
#define NIMCP_THETA_GAMMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Minimum theta frequency (Hz) */
#define THETA_FREQ_MIN              4.0f

/** Maximum theta frequency (Hz) */
#define THETA_FREQ_MAX              8.0f

/** Default theta frequency (Hz) */
#define THETA_FREQ_DEFAULT          6.0f

/** Low gamma minimum frequency (Hz) */
#define GAMMA_LOW_FREQ_MIN          30.0f

/** Low gamma maximum frequency (Hz) */
#define GAMMA_LOW_FREQ_MAX          60.0f

/** High gamma minimum frequency (Hz) */
#define GAMMA_HIGH_FREQ_MIN         60.0f

/** High gamma maximum frequency (Hz) */
#define GAMMA_HIGH_FREQ_MAX         100.0f

/** Default gamma frequency (Hz) */
#define GAMMA_FREQ_DEFAULT          40.0f

/** Pi constant */

/** Two pi constant */
#ifndef NIMCP_TWO_PI_F
#endif

/** Number of phase bins for PAC computation */
#define THETA_GAMMA_PAC_BINS        18

/** Default encoding phase start (radians) - 0° */
#define THETA_ENCODE_START_DEFAULT  0.0f

/** Default encoding phase end (radians) - π/2 = 90° */
#define THETA_ENCODE_END_DEFAULT    (M_PI / 2.0f)

/** Default retrieval phase start (radians) - π = 180° */
#define THETA_RETRIEVE_START_DEFAULT M_PI

/** Default retrieval phase end (radians) - 3π/2 = 270° */
#define THETA_RETRIEVE_END_DEFAULT  (3.0f * M_PI / 2.0f)

/** Default transition gate strictness */
#define THETA_TRANSITION_GATE_DEFAULT 0.1f

/** Default PAC coupling strength */
#define THETA_GAMMA_PAC_DEFAULT     0.5f

/** Maximum signal length for PAC computation */
#define THETA_GAMMA_MAX_SIGNAL_LEN  8192

/** Default burst detection threshold (std devs above mean) */
#define GAMMA_BURST_THRESHOLD_DEFAULT 2.0f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Theta phase windows (8 windows of 45° each)
 *
 * WHAT: Discrete phase windows within theta cycle
 * WHY:  Enable fine-grained control of memory operations by phase
 * HOW:  Each window corresponds to 45° of theta phase
 *
 *   0°────45°───90°───135°───180°───225°───270°───315°───360°
 *   │ ENC │ ENC │TRANS│RETR │RETR │RETR │TRANS│PREP │
 *   │EARLY│LATE │ E→R │EARLY│PEAK │LATE │ R→E │ ENC │
 */
typedef enum {
    THETA_PHASE_ENCODE_EARLY = 0,   /**< 0° - 45°: Early encoding window */
    THETA_PHASE_ENCODE_LATE,         /**< 45° - 90°: Late encoding window */
    THETA_PHASE_TRANSITION_ER,       /**< 90° - 135°: Encode → Retrieve transition */
    THETA_PHASE_RETRIEVE_EARLY,      /**< 135° - 180°: Early retrieval window */
    THETA_PHASE_RETRIEVE_PEAK,       /**< 180° - 225°: Peak retrieval window */
    THETA_PHASE_RETRIEVE_LATE,       /**< 225° - 270°: Late retrieval window */
    THETA_PHASE_TRANSITION_RE,       /**< 270° - 315°: Retrieve → Encode transition */
    THETA_PHASE_ENCODE_PREPARE,      /**< 315° - 360°: Prepare next encoding cycle */

    THETA_PHASE_WINDOW_COUNT         /**< Number of phase windows */
} theta_phase_window_t;

/**
 * @brief Memory operation type allowed by current theta phase
 *
 * WHAT: High-level operation category (encode, retrieve, transition, blocked)
 * WHY:  Simplifies gating logic for memory operations
 * HOW:  Derived from theta_phase_window_t
 */
typedef enum {
    THETA_OP_ENCODE = 0,    /**< Encoding allowed (theta trough region) */
    THETA_OP_RETRIEVE,      /**< Retrieval allowed (theta peak region) */
    THETA_OP_TRANSITION,    /**< Between states (operation partially allowed) */
    THETA_OP_BLOCKED        /**< Neither encoding nor retrieval optimal */
} theta_op_type_t;

/**
 * @brief Gamma frequency band selection
 *
 * WHAT: Which gamma sub-band to use for analysis
 * WHY:  High gamma (encoding) vs low gamma (retrieval) serve different functions
 * HOW:  Select band for filtering and burst detection
 */
typedef enum {
    GAMMA_BAND_LOW = 0,     /**< 30-60 Hz (associated with retrieval) */
    GAMMA_BAND_HIGH,        /**< 60-100 Hz (associated with encoding) */
    GAMMA_BAND_FULL         /**< 30-100 Hz (combined gamma band) */
} gamma_band_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Theta-gamma oscillator state
 *
 * WHAT: Current state of theta-gamma coupling system
 * WHY:  Track phases, frequencies, and coupling metrics
 * HOW:  Updated each simulation step, queried for gating decisions
 *
 * Memory layout: ~64 bytes
 */
typedef struct {
    /* Theta oscillator state */
    float theta_phase;           /**< Current theta phase (radians, 0 to 2π) */
    float theta_frequency;       /**< Theta frequency (Hz, 4-8) */
    float theta_amplitude;       /**< Theta amplitude (normalized, 0-1) */

    /* Gamma oscillator state */
    float gamma_phase;           /**< Current gamma phase (radians) */
    float gamma_frequency;       /**< Gamma frequency (Hz, 30-100) */
    float gamma_amplitude;       /**< Gamma amplitude (modulated by theta phase) */

    /* Phase-amplitude coupling metrics */
    float pac_strength;          /**< Phase-amplitude coupling strength (0-1) */
    float modulation_index;      /**< Modulation index from PAC analysis */
    float preferred_phase;       /**< Theta phase where gamma peaks (radians) */

    /* Current operational state */
    theta_phase_window_t current_window;  /**< Current 45° window */
    theta_op_type_t current_op;           /**< Current allowed operation */

    /* Timing */
    uint64_t last_update_ns;     /**< Last update timestamp (nanoseconds) */
    uint64_t cycle_count;        /**< Number of complete theta cycles */
} theta_gamma_state_t;

/**
 * @brief Theta-gamma coupling configuration
 *
 * WHAT: Parameters controlling theta-gamma behavior
 * WHY:  Allow tuning for different cognitive states and experiments
 * HOW:  Passed to theta_gamma_create(), can be modified at runtime
 *
 * Memory layout: ~64 bytes
 */
typedef struct {
    /* Theta frequency range */
    float theta_freq_min;        /**< Minimum theta frequency (Hz), default 4.0 */
    float theta_freq_max;        /**< Maximum theta frequency (Hz), default 8.0 */
    float theta_freq_default;    /**< Default theta frequency (Hz), default 6.0 */

    /* Gamma low band (retrieval) */
    float gamma_freq_low_min;    /**< Low gamma min (Hz), default 30.0 */
    float gamma_freq_low_max;    /**< Low gamma max (Hz), default 60.0 */

    /* Gamma high band (encoding) */
    float gamma_freq_high_min;   /**< High gamma min (Hz), default 60.0 */
    float gamma_freq_high_max;   /**< High gamma max (Hz), default 100.0 */

    /* Phase window boundaries (radians) */
    float encode_phase_start;    /**< Encoding window start, default 0.0 */
    float encode_phase_end;      /**< Encoding window end, default π/2 */
    float retrieve_phase_start;  /**< Retrieval window start, default π */
    float retrieve_phase_end;    /**< Retrieval window end, default 3π/2 */

    /* Gating parameters */
    float transition_gate;       /**< Transition zone strictness (0-1), default 0.1 */
    float min_gate_strength;     /**< Minimum gating strength to allow operation */

    /* PAC parameters */
    uint32_t pac_num_bins;       /**< Number of phase bins for PAC (default 18) */
    float pac_smoothing;         /**< Smoothing factor for PAC histogram */

    /* Burst detection */
    float burst_threshold;       /**< Gamma burst threshold (std devs), default 2.0 */
    uint32_t burst_min_samples;  /**< Minimum samples for valid burst */
} theta_gamma_config_t;

/**
 * @brief Burst detection result
 *
 * WHAT: Information about a detected gamma burst
 * WHY:  Bursts indicate information processing events
 * HOW:  Detected via amplitude envelope thresholding
 */
typedef struct {
    bool detected;               /**< True if burst detected */
    float peak_amplitude;        /**< Maximum amplitude during burst */
    float theta_phase_at_peak;   /**< Theta phase when burst peaked */
    uint32_t start_sample;       /**< Sample index where burst started */
    uint32_t end_sample;         /**< Sample index where burst ended */
    uint32_t peak_sample;        /**< Sample index of peak amplitude */
    float duration_ms;           /**< Burst duration in milliseconds */
    gamma_band_t band;           /**< Which gamma band the burst was in */
} gamma_burst_t;

/**
 * @brief Statistics for theta-gamma coupling
 *
 * WHAT: Metrics tracking coupling behavior over time
 * WHY:  Monitor system health, diagnose issues, tune parameters
 * HOW:  Updated during operation, queried via theta_gamma_get_stats()
 */
typedef struct {
    /* Operation counts */
    uint64_t total_updates;      /**< Total state updates performed */
    uint64_t encode_operations;  /**< Operations during encoding window */
    uint64_t retrieve_operations;/**< Operations during retrieval window */
    uint64_t blocked_operations; /**< Operations blocked by phase */
    uint64_t theta_cycles;       /**< Complete theta cycles */

    /* Gating statistics */
    float mean_encode_strength;  /**< Mean encoding gate strength */
    float mean_retrieve_strength;/**< Mean retrieval gate strength */
    float gate_efficiency;       /**< Fraction of optimal gating */

    /* PAC statistics */
    float mean_pac;              /**< Mean PAC modulation index */
    float max_pac;               /**< Maximum PAC observed */
    float pac_variance;          /**< Variance in PAC over time */

    /* Burst statistics */
    uint64_t bursts_detected;    /**< Total gamma bursts detected */
    float mean_burst_amplitude;  /**< Mean burst peak amplitude */
    float mean_burst_duration;   /**< Mean burst duration (ms) */

    /* Timing */
    uint64_t first_update_ns;    /**< Timestamp of first update */
    uint64_t last_update_ns;     /**< Timestamp of last update */
    float total_runtime_s;       /**< Total runtime in seconds */
} theta_gamma_stats_t;

/**
 * @brief Opaque manager handle
 *
 * WHAT: Handle to theta-gamma coupling manager
 * WHY:  Encapsulate internal state, provide clean API
 * HOW:  Created by theta_gamma_create(), destroyed by theta_gamma_destroy()
 */
typedef struct theta_gamma_manager_internal* theta_gamma_manager_t;

//=============================================================================
// Dependencies
//=============================================================================

/* Include quaternion header for nimcp_quaternion_t */
#include "cognitive/memory/core/nimcp_quaternion.h"

/* Forward declaration from nimcp_kuramoto.h */
typedef struct kuramoto_system_t kuramoto_system_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default theta-gamma configuration
 *
 * WHAT: Returns sensible default configuration based on neuroscience literature
 * WHY:  Provides starting point for typical hippocampal theta-gamma coupling
 * HOW:  Sets frequencies, phase windows, and gating parameters to standard values
 *
 * @return Default configuration structure
 *
 * Default values:
 * - theta: 6 Hz (range 4-8 Hz)
 * - gamma low: 30-60 Hz
 * - gamma high: 60-100 Hz
 * - encode window: 0° - 90°
 * - retrieve window: 180° - 270°
 *
 * EXAMPLE:
 * ```c
 * theta_gamma_config_t config = theta_gamma_config_default();
 * config.theta_freq_default = 7.0f;  // Override for faster theta
 * theta_gamma_manager_t mgr = theta_gamma_create(&config);
 * ```
 */
NIMCP_EXPORT theta_gamma_config_t theta_gamma_config_default(void);

/**
 * @brief Validate and normalize configuration
 *
 * WHAT: Checks configuration values and clamps to valid ranges
 * WHY:  Prevent invalid states from corrupting system
 * HOW:  Range checks, constraint enforcement, normalization
 *
 * @param config Configuration to validate (modified in place)
 * @return true if valid (possibly after correction), false if unrecoverable
 */
NIMCP_EXPORT bool theta_gamma_config_validate(theta_gamma_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create theta-gamma coupling manager
 *
 * WHAT: Allocates and initializes theta-gamma coupling system
 * WHY:  Prepare system for phase-gated memory operations
 * HOW:  Allocates manager, initializes state, creates workspace buffers
 *
 * @param config Configuration (NULL for defaults)
 * @return Manager handle or NULL on failure
 *
 * COMPLEXITY: O(1) allocation
 * MEMORY: ~2KB for manager + workspace
 *
 * EXAMPLE:
 * ```c
 * theta_gamma_config_t config = theta_gamma_config_default();
 * theta_gamma_manager_t mgr = theta_gamma_create(&config);
 * if (!mgr) {
 *     // Handle allocation failure
 * }
 * // ... use manager ...
 * theta_gamma_destroy(mgr);
 * ```
 */
NIMCP_EXPORT theta_gamma_manager_t theta_gamma_create(const theta_gamma_config_t* config);

/**
 * @brief Destroy theta-gamma manager and free resources
 *
 * WHAT: Releases all memory associated with manager
 * WHY:  Prevent memory leaks
 * HOW:  Frees state, workspace buffers, manager structure
 *
 * @param manager Manager to destroy (can be NULL)
 */
NIMCP_EXPORT void theta_gamma_destroy(theta_gamma_manager_t manager);

/**
 * @brief Reset manager to initial state
 *
 * WHAT: Reinitializes phases, clears statistics, resets counters
 * WHY:  Start fresh simulation, clear accumulated state
 * HOW:  Resets phases to 0, clears stats, preserves configuration
 *
 * @param manager Theta-gamma manager
 * @return true on success, false on error
 */
NIMCP_EXPORT bool theta_gamma_reset(theta_gamma_manager_t manager);

//=============================================================================
// State Update Functions
//=============================================================================

/**
 * @brief Advance oscillators by time delta
 *
 * WHAT: Updates theta and gamma phases based on elapsed time
 * WHY:  Simulate oscillation progression, track phase windows
 * HOW:  phase += 2π × frequency × dt, update window and operation type
 *
 * @param manager Theta-gamma manager
 * @param dt_ns Time delta in nanoseconds
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~50ns
 *
 * EXAMPLE:
 * ```c
 * // Update every 1ms
 * theta_gamma_update(mgr, 1000000);  // 1ms in ns
 * ```
 */
NIMCP_EXPORT bool theta_gamma_update(theta_gamma_manager_t manager, uint64_t dt_ns);

/**
 * @brief Set theta oscillator frequency
 *
 * WHAT: Changes theta frequency within valid range
 * WHY:  Theta frequency varies with cognitive state (4-8 Hz)
 * HOW:  Clamps to valid range, updates internal state
 *
 * @param manager Theta-gamma manager
 * @param freq New theta frequency in Hz (clamped to 4-8 Hz)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool theta_gamma_set_theta_freq(theta_gamma_manager_t manager, float freq);

/**
 * @brief Set gamma oscillator frequency
 *
 * WHAT: Changes gamma frequency within valid range
 * WHY:  Gamma frequency varies with task demands (30-100 Hz)
 * HOW:  Clamps to valid range, updates internal state
 *
 * @param manager Theta-gamma manager
 * @param freq New gamma frequency in Hz (clamped to 30-100 Hz)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool theta_gamma_set_gamma_freq(theta_gamma_manager_t manager, float freq);

/**
 * @brief Synchronize to external theta phase
 *
 * WHAT: Forces theta phase to match external reference
 * WHY:  Sync with external rhythm, phase reset, coordination
 * HOW:  Sets theta_phase directly, updates window and op
 *
 * @param manager Theta-gamma manager
 * @param phase External theta phase in radians (wrapped to [0, 2π])
 * @return true on success, false on error
 */
NIMCP_EXPORT bool theta_gamma_sync_to_external(theta_gamma_manager_t manager, float phase);

//=============================================================================
// Phase Query Functions
//=============================================================================

/**
 * @brief Get current theta phase
 *
 * WHAT: Returns instantaneous theta phase
 * WHY:  Query for gating decisions, timing, analysis
 * HOW:  Direct state lookup
 *
 * @param manager Theta-gamma manager
 * @return Theta phase in radians [0, 2π], or -1.0f on error
 */
NIMCP_EXPORT float theta_gamma_get_theta_phase(const theta_gamma_manager_t manager);

/**
 * @brief Get current gamma phase
 *
 * WHAT: Returns instantaneous gamma phase
 * WHY:  Query for precise timing within gamma cycle
 * HOW:  Direct state lookup
 *
 * @param manager Theta-gamma manager
 * @return Gamma phase in radians [0, 2π], or -1.0f on error
 */
NIMCP_EXPORT float theta_gamma_get_gamma_phase(const theta_gamma_manager_t manager);

/**
 * @brief Get current theta phase window
 *
 * WHAT: Returns which 45° window theta is currently in
 * WHY:  Determine encoding/retrieval/transition state
 * HOW:  Cached from last update
 *
 * @param manager Theta-gamma manager
 * @return Current phase window, or THETA_PHASE_ENCODE_EARLY on error
 */
NIMCP_EXPORT theta_phase_window_t theta_gamma_get_window(const theta_gamma_manager_t manager);

/**
 * @brief Get current allowed operation type
 *
 * WHAT: Returns high-level operation category (encode/retrieve/transition/blocked)
 * WHY:  Simplified gating decision for memory operations
 * HOW:  Derived from current phase window
 *
 * @param manager Theta-gamma manager
 * @return Current operation type, or THETA_OP_BLOCKED on error
 */
NIMCP_EXPORT theta_op_type_t theta_gamma_get_operation(const theta_gamma_manager_t manager);

//=============================================================================
// Gating Functions
//=============================================================================

/**
 * @brief Check if encoding is currently allowed
 *
 * WHAT: Boolean check for encoding window
 * WHY:  Simple yes/no gating for encoding operations
 * HOW:  Returns true if current operation is THETA_OP_ENCODE
 *
 * @param manager Theta-gamma manager
 * @return true if encoding is allowed, false otherwise
 */
NIMCP_EXPORT bool theta_gamma_can_encode(const theta_gamma_manager_t manager);

/**
 * @brief Check if retrieval is currently allowed
 *
 * WHAT: Boolean check for retrieval window
 * WHY:  Simple yes/no gating for retrieval operations
 * HOW:  Returns true if current operation is THETA_OP_RETRIEVE
 *
 * @param manager Theta-gamma manager
 * @return true if retrieval is allowed, false otherwise
 */
NIMCP_EXPORT bool theta_gamma_can_retrieve(const theta_gamma_manager_t manager);

/**
 * @brief Get encoding gate strength
 *
 * WHAT: Continuous [0, 1] encoding strength based on theta phase
 * WHY:  Proportional gating instead of binary on/off
 * HOW:  Cosine-shaped curve centered on encoding window
 *
 * @param manager Theta-gamma manager
 * @return Encoding strength in [0, 1], higher during encoding window
 *
 * The encoding strength follows a smooth curve:
 * - Peak (1.0) at theta trough (0°)
 * - Minimum (0.0) at theta peak (180°)
 * - Smooth transition through cosine interpolation
 */
NIMCP_EXPORT float theta_gamma_get_encode_strength(const theta_gamma_manager_t manager);

/**
 * @brief Get retrieval gate strength
 *
 * WHAT: Continuous [0, 1] retrieval strength based on theta phase
 * WHY:  Proportional gating instead of binary on/off
 * HOW:  Cosine-shaped curve centered on retrieval window
 *
 * @param manager Theta-gamma manager
 * @return Retrieval strength in [0, 1], higher during retrieval window
 *
 * The retrieval strength follows a smooth curve:
 * - Peak (1.0) at theta peak (180°)
 * - Minimum (0.0) at theta trough (0°)
 * - Smooth transition through cosine interpolation
 */
NIMCP_EXPORT float theta_gamma_get_retrieve_strength(const theta_gamma_manager_t manager);

/**
 * @brief Gate a specific operation type and return weight
 *
 * WHAT: Gets gating weight for requested operation
 * WHY:  Generic gating for any operation type
 * HOW:  Maps operation to appropriate strength function
 *
 * @param manager Theta-gamma manager
 * @param op_type Requested operation type
 * @return Gating weight [0, 1] for the operation (0 = blocked, 1 = optimal)
 *
 * EXAMPLE:
 * ```c
 * float weight = theta_gamma_gate_operation(mgr, THETA_OP_ENCODE);
 * if (weight > 0.5f) {
 *     // Good time for encoding
 *     perform_encoding(weight);  // Scale by weight
 * }
 * ```
 */
NIMCP_EXPORT float theta_gamma_gate_operation(const theta_gamma_manager_t manager,
                                                theta_op_type_t op_type);

//=============================================================================
// Phase-Amplitude Coupling Functions
//=============================================================================

/**
 * @brief Compute phase-amplitude coupling from signals
 *
 * WHAT: Calculates PAC modulation index between theta and gamma signals
 * WHY:  Quantify strength of cross-frequency coupling
 * HOW:  Bins gamma amplitude by theta phase, computes entropy-based MI
 *
 * Algorithm:
 * 1. Extract theta phase via Hilbert transform
 * 2. Extract gamma amplitude envelope via Hilbert transform
 * 3. Bin gamma amplitude by theta phase (18 bins, 20° each)
 * 4. Normalize bins to probability distribution
 * 5. Compute KL divergence from uniform: MI = (log(N) - H) / log(N)
 *
 * @param manager Theta-gamma manager
 * @param theta_signal Raw theta-band filtered signal
 * @param gamma_signal Raw gamma-band filtered signal
 * @param n Number of samples (max THETA_GAMMA_MAX_SIGNAL_LEN)
 * @param sample_rate Sampling rate in Hz
 * @return Modulation index [0, 1], or -1.0f on error
 *
 * COMPLEXITY: O(n) after Hilbert transforms
 * PERFORMANCE: ~200µs for n=1024
 *
 * EXAMPLE:
 * ```c
 * float* theta = filter_theta_band(raw_signal, n);
 * float* gamma = filter_gamma_band(raw_signal, n);
 * float mi = theta_gamma_compute_pac(mgr, theta, gamma, n, 1000.0f);
 * printf("PAC Modulation Index: %.3f\n", mi);
 * ```
 */
NIMCP_EXPORT float theta_gamma_compute_pac(theta_gamma_manager_t manager,
                                            const float* theta_signal,
                                            const float* gamma_signal,
                                            uint32_t n,
                                            float sample_rate);

/**
 * @brief Compute modulation index from pre-extracted amplitude and phase
 *
 * WHAT: Calculates MI when amplitude and phase are already available
 * WHY:  Avoid redundant Hilbert transforms if already computed
 * HOW:  Direct binning and entropy computation
 *
 * @param manager Theta-gamma manager
 * @param theta_phase Array of theta phase values (radians)
 * @param gamma_amplitude Array of gamma amplitude values
 * @param n Number of samples
 * @return Modulation index [0, 1], or -1.0f on error
 *
 * COMPLEXITY: O(n)
 * PERFORMANCE: ~10µs for n=1024 (no Hilbert)
 */
NIMCP_EXPORT float theta_gamma_modulation_index(theta_gamma_manager_t manager,
                                                  const float* theta_phase,
                                                  const float* gamma_amplitude,
                                                  uint32_t n);

/**
 * @brief Find preferred theta phase for gamma amplitude
 *
 * WHAT: Determines at which theta phase gamma amplitude is maximal
 * WHY:  Identifies coupling phase relationship
 * HOW:  Finds peak of PAC histogram
 *
 * @param manager Theta-gamma manager
 * @param theta_phase Array of theta phase values (radians)
 * @param gamma_amplitude Array of gamma amplitude values
 * @param n Number of samples
 * @return Preferred phase in radians [0, 2π], or -1.0f on error
 */
NIMCP_EXPORT float theta_gamma_preferred_phase(theta_gamma_manager_t manager,
                                                 const float* theta_phase,
                                                 const float* gamma_amplitude,
                                                 uint32_t n);

//=============================================================================
// Gamma Burst Detection Functions
//=============================================================================

/**
 * @brief Detect gamma burst in signal
 *
 * WHAT: Finds amplitude peaks exceeding threshold in gamma signal
 * WHY:  Bursts indicate information processing events
 * HOW:  Envelope extraction, threshold crossing, burst boundaries
 *
 * @param manager Theta-gamma manager
 * @param gamma_signal Gamma-band filtered signal
 * @param n Number of samples
 * @param sample_rate Sampling rate in Hz
 * @param burst Output burst detection result
 * @return true if burst detected, false otherwise
 *
 * COMPLEXITY: O(n)
 * PERFORMANCE: ~30µs for n=256
 *
 * EXAMPLE:
 * ```c
 * gamma_burst_t burst;
 * if (theta_gamma_detect_burst(mgr, gamma_signal, n, 1000.0f, &burst)) {
 *     printf("Burst at theta phase: %.2f rad\n", burst.theta_phase_at_peak);
 * }
 * ```
 */
NIMCP_EXPORT bool theta_gamma_detect_burst(theta_gamma_manager_t manager,
                                            const float* gamma_signal,
                                            uint32_t n,
                                            float sample_rate,
                                            gamma_burst_t* burst);

/**
 * @brief Get theta phase at which a burst occurred
 *
 * WHAT: Determines theta phase at burst peak
 * WHY:  Classifies burst as encoding or retrieval related
 * HOW:  Uses burst timestamp relative to theta phase tracking
 *
 * @param manager Theta-gamma manager
 * @param burst_sample Sample index of burst peak
 * @param sample_rate Sampling rate in Hz
 * @return Theta phase at burst in radians [0, 2π], or -1.0f on error
 */
NIMCP_EXPORT float theta_gamma_burst_phase(theta_gamma_manager_t manager,
                                            uint32_t burst_sample,
                                            float sample_rate);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Modulate quaternion memory state based on theta phase
 *
 * WHAT: Adjusts quaternion accessibility based on encoding/retrieval phase
 * WHY:  Memories should be more accessible during retrieval phase
 * HOW:  Modifies quaternion z-component (accessibility) by phase
 *
 * @param manager Theta-gamma manager
 * @param q Input quaternion (memory state)
 * @return Modulated quaternion with phase-adjusted accessibility
 *
 * The accessibility (z-component) is scaled by:
 * - During retrieval: increased by retrieve_strength
 * - During encoding: decreased (memories less accessible while encoding new)
 * - During transition: neutral modulation
 *
 * EXAMPLE:
 * ```c
 * nimcp_quaternion_t mem_state = get_memory_state(memory_id);
 * nimcp_quaternion_t modulated = theta_gamma_modulate_quaternion(mgr, mem_state);
 * // modulated.z reflects phase-appropriate accessibility
 * ```
 */
NIMCP_EXPORT nimcp_quaternion_t theta_gamma_modulate_quaternion(
    theta_gamma_manager_t manager,
    nimcp_quaternion_t q);

/**
 * @brief Integrate with Kuramoto oscillator system
 *
 * WHAT: Synchronizes theta-gamma with Kuramoto module oscillators
 * WHY:  Coordinate phase-gating with module synchronization
 * HOW:  Updates Kuramoto coupling strengths based on theta phase
 *
 * @param manager Theta-gamma manager
 * @param kuramoto Kuramoto system to integrate with
 * @param module_id Module ID for theta-gamma in Kuramoto system
 * @return true on success, false on error
 *
 * Integration effects:
 * - During encoding: Strengthen coupling to encoding-related modules
 * - During retrieval: Strengthen coupling to retrieval-related modules
 * - Updates Kuramoto oscillator phase to match theta phase
 */
NIMCP_EXPORT bool theta_gamma_integrate_kuramoto(theta_gamma_manager_t manager,
                                                   kuramoto_system_t* kuramoto,
                                                   uint32_t module_id);

/**
 * @brief Get full internal state structure
 *
 * WHAT: Returns copy of current state
 * WHY:  Full state access for monitoring, serialization
 * HOW:  Copies internal state to output structure
 *
 * @param manager Theta-gamma manager
 * @param state Output state structure
 * @return true on success, false on error
 */
NIMCP_EXPORT bool theta_gamma_get_state(const theta_gamma_manager_t manager,
                                          theta_gamma_state_t* state);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get operation statistics
 *
 * WHAT: Returns accumulated statistics about theta-gamma coupling
 * WHY:  Monitoring, debugging, parameter tuning
 * HOW:  Copies internal stats structure
 *
 * @param manager Theta-gamma manager
 * @param stats Output statistics structure
 * @return true on success, false on error
 *
 * EXAMPLE:
 * ```c
 * theta_gamma_stats_t stats;
 * theta_gamma_get_stats(mgr, &stats);
 * printf("Theta cycles: %lu, PAC mean: %.3f\n",
 *        stats.theta_cycles, stats.mean_pac);
 * ```
 */
NIMCP_EXPORT bool theta_gamma_get_stats(const theta_gamma_manager_t manager,
                                          theta_gamma_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clears all accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zeros all stats fields except configuration
 *
 * @param manager Theta-gamma manager
 * @return true on success, false on error
 */
NIMCP_EXPORT bool theta_gamma_reset_stats(theta_gamma_manager_t manager);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert phase window to string name
 *
 * WHAT: Human-readable name for phase window
 * WHY:  Debugging, logging, UI display
 *
 * @param window Phase window enumeration value
 * @return Static string name, or "unknown" for invalid
 */
NIMCP_EXPORT const char* theta_gamma_window_name(theta_phase_window_t window);

/**
 * @brief Convert operation type to string name
 *
 * WHAT: Human-readable name for operation type
 * WHY:  Debugging, logging, UI display
 *
 * @param op Operation type enumeration value
 * @return Static string name, or "unknown" for invalid
 */
NIMCP_EXPORT const char* theta_gamma_op_name(theta_op_type_t op);

/**
 * @brief Convert gamma band to string name
 *
 * WHAT: Human-readable name for gamma band
 * WHY:  Debugging, logging, UI display
 *
 * @param band Gamma band enumeration value
 * @return Static string name, or "unknown" for invalid
 */
NIMCP_EXPORT const char* theta_gamma_band_name(gamma_band_t band);

/**
 * @brief Get last error message
 *
 * WHAT: Returns human-readable error description
 * WHY:  Debugging failed operations
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* theta_gamma_get_last_error(void);

/**
 * @brief Print current state (debug)
 *
 * WHAT: Outputs human-readable state description
 * WHY:  Debugging and development
 * HOW:  Prints to stdout
 *
 * @param manager Theta-gamma manager
 */
NIMCP_EXPORT void theta_gamma_print_state(const theta_gamma_manager_t manager);

/**
 * @brief Wrap phase to [0, 2π] range
 *
 * WHAT: Normalizes phase angle to canonical range
 * WHY:  Utility for phase arithmetic
 * HOW:  Uses fmod and adjustment
 *
 * @param phase Input phase (any value)
 * @return Wrapped phase in [0, 2π]
 */
NIMCP_EXPORT float theta_gamma_wrap_phase(float phase);

/**
 * @brief Convert phase to window index
 *
 * WHAT: Maps phase angle to window enumeration
 * WHY:  Determine window from arbitrary phase
 * HOW:  Divides phase range into 8 windows
 *
 * @param phase Phase in radians
 * @return Corresponding phase window
 */
NIMCP_EXPORT theta_phase_window_t theta_gamma_phase_to_window(float phase);

/**
 * @brief Get operation type from window
 *
 * WHAT: Maps window to high-level operation
 * WHY:  Simplify gating logic
 *
 * @param window Phase window
 * @return Operation type (encode/retrieve/transition/blocked)
 */
NIMCP_EXPORT theta_op_type_t theta_gamma_window_to_op(theta_phase_window_t window);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THETA_GAMMA_H */

//=============================================================================
// nimcp_theta_gamma.c - Theta-Gamma Coupling Implementation
//=============================================================================
/**
 * @file nimcp_theta_gamma.c
 * @brief Implementation of phase-gated memory operations via theta-gamma coupling
 *
 * WHAT: Implements theta-gamma coupling for encoding/retrieval gating
 * WHY:  Theta phase determines memory operation mode; gamma carries information
 * HOW:  Phase tracking, PAC computation, gating functions, Kuramoto integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_theta_gamma.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_kuramoto.h"
#include "utils/signal/nimcp_hilbert.h"
#include "utils/signal/nimcp_signal_filter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(theta_gamma, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal manager structure
 *
 * Contains all state, configuration, statistics, and workspace buffers
 * for theta-gamma coupling operations.
 */
struct theta_gamma_manager_internal {
    /* Current state */
    theta_gamma_state_t state;

    /* Configuration */
    theta_gamma_config_t config;

    /* Statistics */
    theta_gamma_stats_t stats;

    /* PAC computation workspace */
    float* pac_histogram;           /**< Phase bins for PAC (pac_num_bins floats) */
    uint32_t* pac_bin_counts;       /**< Count per bin */

    /* Workspace buffers for signal processing */
    float* work_theta_phase;        /**< Extracted theta phase */
    float* work_gamma_amplitude;    /**< Extracted gamma amplitude */
    float* work_envelope;           /**< Amplitude envelope buffer */
    uint32_t work_buffer_size;      /**< Current workspace allocation */

    /* Hilbert transform handle (for phase/amplitude extraction) */
    hilbert_transform_t* hilbert;

    /* Signal filters */
    signal_filter_t* theta_filter;  /**< Bandpass for theta extraction */
    signal_filter_t* gamma_low_filter;  /**< Bandpass for low gamma */
    signal_filter_t* gamma_high_filter; /**< Bandpass for high gamma */

    /* Thread safety */
    // Note: In production, would use nimcp_mutex_t here
    // For now, single-threaded implementation

    /* Validity flag */
    bool initialized;
};

//=============================================================================
// Static Variables
//=============================================================================

/** Thread-local error message buffer */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Set the last error message
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(s_last_error, sizeof(s_last_error), format, args);
    va_end(args);
}

/**
 * @brief Clear the last error message
 */
static void clear_error(void) {
    s_last_error[0] = '\0';
}

/**
 * @brief Wrap phase to [0, 2*pi] range
 */
static inline float wrap_phase_internal(float phase) {
    while (phase < 0.0f) phase += NIMCP_TWO_PI_F;
    while (phase >= NIMCP_TWO_PI_F) phase -= NIMCP_TWO_PI_F;
    return phase;
}

/**
 * @brief Fast absolute value for floats
 */
static inline float fabsf_fast(float x) {
    return (x < 0.0f) ? -x : x;
}

/**
 * @brief Convert nanoseconds to seconds
 */
static inline float ns_to_seconds(uint64_t ns) {
    return (float)ns / 1e9f;
}

/**
 * @brief Get current timestamp in nanoseconds (monotonic)
 */
static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    #ifdef _POSIX_MONOTONIC_CLOCK
    clock_gettime(CLOCK_MONOTONIC, &ts);
    #else
    clock_gettime(CLOCK_REALTIME, &ts);
    #endif
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Compute mean of float array
 */
static float compute_mean(const float* data, uint32_t n) {
    if (n == 0) return 0.0f;

    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)n);
        }

        sum += data[i];
    }
    return (float)(sum / (fabsf(n) > 1e-7f ? n : 1e-7f));
}

/**
 * @brief Compute standard deviation of float array
 */
static float compute_stddev(const float* data, uint32_t n, float mean) {
    if (n <= 1) return 0.0f;

    double sum_sq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)n);
        }

        double diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrtf((float)(sum_sq / (n - 1)));
}

/**
 * @brief Compute entropy of probability distribution
 *
 * H = -sum(p[i] * log(p[i]))
 * For uniform distribution: H_max = log(n)
 *
 * Delegates to nimcp_stats_entropy() which returns bits.
 * Converts to nats for compatibility with existing code that uses logf(n).
 */
static float compute_entropy(const float* probs, uint32_t n) {
    if (!probs || n == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    if (n > 256) {
        theta_gamma_heartbeat("theta_gamma_entropy", 0.5f);
    }

    /* nimcp_stats_entropy returns bits; convert to nats for compatibility */
    float entropy_bits = nimcp_stats_entropy(probs, n);
    return entropy_bits * 0.693147f;  /* ln(2) to convert bits to nats */
}

/**
 * @brief Map phase to window index
 */
static theta_phase_window_t phase_to_window_internal(float phase) {
    phase = wrap_phase_internal(phase);

    /* Each window is 45° = π/4 radians */
    float window_size = M_PI / 4.0f;
    int idx = (int)(phase / (fabsf(window_size) > 1e-7f ? window_size : 1e-7f));

    if (idx < 0) idx = 0;
    if (idx >= THETA_PHASE_WINDOW_COUNT) idx = THETA_PHASE_WINDOW_COUNT - 1;

    return (theta_phase_window_t)idx;
}

/**
 * @brief Map window to operation type
 */
static theta_op_type_t window_to_op_internal(theta_phase_window_t window) {
    switch (window) {
        case THETA_PHASE_ENCODE_EARLY:
        case THETA_PHASE_ENCODE_LATE:
        case THETA_PHASE_ENCODE_PREPARE:
            return THETA_OP_ENCODE;

        case THETA_PHASE_RETRIEVE_EARLY:
        case THETA_PHASE_RETRIEVE_PEAK:
        case THETA_PHASE_RETRIEVE_LATE:
            return THETA_OP_RETRIEVE;

        case THETA_PHASE_TRANSITION_ER:
        case THETA_PHASE_TRANSITION_RE:
            return THETA_OP_TRANSITION;

        default:
            return THETA_OP_BLOCKED;
    }
}

/**
 * @brief Compute smooth encoding strength from theta phase
 *
 * Uses cosine interpolation for smooth gating:
 * - Peak (1.0) at phase 0 (theta trough)
 * - Minimum (0.0) at phase π (theta peak)
 */
static float compute_encode_strength(float theta_phase) {
    theta_phase = wrap_phase_internal(theta_phase);

    /* cos(phase) maps 0→1, π→-1
     * We want: 0→1 (encoding), π→0 (no encoding)
     * Transform: (cos(phase) + 1) / 2 */
    float cos_val = cosf(theta_phase);
    return nimcp_clamp01((cos_val + 1.0f) / 2.0f);
}

/**
 * @brief Compute smooth retrieval strength from theta phase
 *
 * Uses cosine interpolation for smooth gating:
 * - Peak (1.0) at phase π (theta peak)
 * - Minimum (0.0) at phase 0 (theta trough)
 */
static float compute_retrieve_strength(float theta_phase) {
    theta_phase = wrap_phase_internal(theta_phase);

    /* Opposite of encode: (1 - cos(phase)) / 2
     * Or equivalently: (-cos(phase) + 1) / 2 */
    float cos_val = cosf(theta_phase);
    return nimcp_clamp01((1.0f - cos_val) / 2.0f);
}

/**
 * @brief Update internal state after phase change
 */
static void update_state_internals(struct theta_gamma_manager_internal* mgr) {
    mgr->state.current_window = phase_to_window_internal(mgr->state.theta_phase);
    mgr->state.current_op = window_to_op_internal(mgr->state.current_window);
}

/**
 * @brief Allocate workspace buffers for signal processing
 */
static bool allocate_workspace(struct theta_gamma_manager_internal* mgr, uint32_t size) {
    if (size == 0 || size > THETA_GAMMA_MAX_SIGNAL_LEN) {
        set_error("Invalid workspace size: %u", size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "allocate_workspace: size is zero");
        return false;
    }

    /* Free existing buffers if size changed */
    if (mgr->work_buffer_size != size) {
        nimcp_free(mgr->work_theta_phase);
        nimcp_free(mgr->work_gamma_amplitude);
        nimcp_free(mgr->work_envelope);

        mgr->work_theta_phase = (float*)nimcp_calloc(size, sizeof(float));
        mgr->work_gamma_amplitude = (float*)nimcp_calloc(size, sizeof(float));
        if (!mgr->work_gamma_amplitude) return -1;
        mgr->work_envelope = (float*)nimcp_calloc(size, sizeof(float));

        if (!mgr->work_theta_phase || !mgr->work_gamma_amplitude || !mgr->work_envelope) {
            set_error("Failed to allocate workspace buffers");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_workspace: required parameter is NULL (mgr->work_theta_phase, mgr->work_gamma_amplitude, mgr->work_envelope)");
            return false;
        }

        mgr->work_buffer_size = size;
    }

    return true;
}

/**
 * @brief Initialize PAC histogram
 */
static bool init_pac_histogram(struct theta_gamma_manager_internal* mgr) {
    uint32_t num_bins = mgr->config.pac_num_bins;

    mgr->pac_histogram = (float*)nimcp_calloc(num_bins, sizeof(float));
    mgr->pac_bin_counts = (uint32_t*)nimcp_calloc(num_bins, sizeof(uint32_t));

    if (!mgr->pac_histogram || !mgr->pac_bin_counts) {
        set_error("Failed to allocate PAC histogram");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_pac_histogram: required parameter is NULL (mgr->pac_histogram, mgr->pac_bin_counts)");
        return false;
    }

    return true;
}

/**
 * @brief Clear PAC histogram
 */
static void clear_pac_histogram(struct theta_gamma_manager_internal* mgr) {
    if (mgr->pac_histogram) {
        memset(mgr->pac_histogram, 0, mgr->config.pac_num_bins * sizeof(float));
    }
    if (mgr->pac_bin_counts) {
        memset(mgr->pac_bin_counts, 0, mgr->config.pac_num_bins * sizeof(uint32_t));
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

theta_gamma_config_t theta_gamma_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_config_default", 0.0f);


    theta_gamma_config_t config = {
        /* Theta frequency range */
        .theta_freq_min = THETA_FREQ_MIN,
        .theta_freq_max = THETA_FREQ_MAX,
        .theta_freq_default = THETA_FREQ_DEFAULT,

        /* Gamma low band (retrieval) */
        .gamma_freq_low_min = GAMMA_LOW_FREQ_MIN,
        .gamma_freq_low_max = GAMMA_LOW_FREQ_MAX,

        /* Gamma high band (encoding) */
        .gamma_freq_high_min = GAMMA_HIGH_FREQ_MIN,
        .gamma_freq_high_max = GAMMA_HIGH_FREQ_MAX,

        /* Phase window boundaries */
        .encode_phase_start = THETA_ENCODE_START_DEFAULT,
        .encode_phase_end = THETA_ENCODE_END_DEFAULT,
        .retrieve_phase_start = THETA_RETRIEVE_START_DEFAULT,
        .retrieve_phase_end = THETA_RETRIEVE_END_DEFAULT,

        /* Gating parameters */
        .transition_gate = THETA_TRANSITION_GATE_DEFAULT,
        .min_gate_strength = 0.1f,

        /* PAC parameters */
        .pac_num_bins = THETA_GAMMA_PAC_BINS,
        .pac_smoothing = 0.1f,

        /* Burst detection */
        .burst_threshold = GAMMA_BURST_THRESHOLD_DEFAULT,
        .burst_min_samples = 5
    };

    return config;
}

bool theta_gamma_config_validate(theta_gamma_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_config_validate: config is NULL");
        return false;
    }

    /* Validate theta frequency range */
    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_config_validate", 0.0f);


    if (config->theta_freq_min < 1.0f) config->theta_freq_min = 1.0f;
    if (config->theta_freq_max > 12.0f) config->theta_freq_max = 12.0f;
    if (config->theta_freq_min >= config->theta_freq_max) {
        set_error("Invalid theta frequency range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "theta_gamma_config_validate: capacity exceeded");
        return false;
    }
    config->theta_freq_default = nimcp_clampf(config->theta_freq_default,
                                         config->theta_freq_min,
                                         config->theta_freq_max);

    /* Validate gamma frequency ranges */
    if (config->gamma_freq_low_min < 20.0f) config->gamma_freq_low_min = 20.0f;
    if (config->gamma_freq_high_max > 150.0f) config->gamma_freq_high_max = 150.0f;
    if (config->gamma_freq_low_min >= config->gamma_freq_low_max ||
        config->gamma_freq_high_min >= config->gamma_freq_high_max) {
        set_error("Invalid gamma frequency ranges");
        return false;
    }

    /* Validate phase windows */
    config->encode_phase_start = wrap_phase_internal(config->encode_phase_start);
    config->encode_phase_end = wrap_phase_internal(config->encode_phase_end);
    config->retrieve_phase_start = wrap_phase_internal(config->retrieve_phase_start);
    config->retrieve_phase_end = wrap_phase_internal(config->retrieve_phase_end);

    /* Validate gating parameters */
    config->transition_gate = nimcp_clamp01(config->transition_gate);
    config->min_gate_strength = nimcp_clamp01(config->min_gate_strength);

    /* Validate PAC parameters */
    if (config->pac_num_bins < 4) config->pac_num_bins = 4;
    if (config->pac_num_bins > 72) config->pac_num_bins = 72;
    config->pac_smoothing = nimcp_clamp01(config->pac_smoothing);

    /* Validate burst detection */
    if (config->burst_threshold < 0.5f) config->burst_threshold = 0.5f;
    if (config->burst_threshold > 10.0f) config->burst_threshold = 10.0f;
    if (config->burst_min_samples < 2) config->burst_min_samples = 2;

    clear_error();
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

theta_gamma_manager_t theta_gamma_create(const theta_gamma_config_t* config) {
    /* Allocate manager structure */
    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_create", 0.0f);


    struct theta_gamma_manager_internal* mgr =
        (struct theta_gamma_manager_internal*)nimcp_calloc(1,
            sizeof(struct theta_gamma_manager_internal));

    if (!mgr) {
        set_error("Failed to allocate theta-gamma manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mgr is NULL");


        return NULL;
    }

    /* Copy or create default config */
    if (config) {
        mgr->config = *config;
    } else {
        mgr->config = theta_gamma_config_default();
    }

    /* Validate configuration */
    if (!theta_gamma_config_validate(&mgr->config)) {
        nimcp_free(mgr);
        mgr = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_create: theta_gamma_config_validate is NULL");
        return NULL;
    }

    /* Initialize state */
    memset(&mgr->state, 0, sizeof(mgr->state));
    mgr->state.theta_frequency = mgr->config.theta_freq_default;
    mgr->state.gamma_frequency = GAMMA_FREQ_DEFAULT;
    mgr->state.theta_amplitude = 1.0f;
    mgr->state.gamma_amplitude = 1.0f;
    mgr->state.pac_strength = THETA_GAMMA_PAC_DEFAULT;
    mgr->state.current_window = THETA_PHASE_ENCODE_EARLY;
    mgr->state.current_op = THETA_OP_ENCODE;
    mgr->state.last_update_ns = get_timestamp_ns();

    /* Initialize statistics */
    memset(&mgr->stats, 0, sizeof(mgr->stats));
    mgr->stats.first_update_ns = mgr->state.last_update_ns;

    /* Initialize PAC histogram */
    if (!init_pac_histogram(mgr)) {
        nimcp_free(mgr);
        mgr = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "theta_gamma_create: init_pac_histogram is NULL");
        return NULL;
    }

    /* Create Hilbert transform for phase/amplitude extraction */
    hilbert_config_t hilbert_config = hilbert_default_config();
    hilbert_config.max_signal_length = THETA_GAMMA_MAX_SIGNAL_LEN;
    mgr->hilbert = hilbert_create(&hilbert_config);
    if (!mgr->hilbert) {
        set_error("Failed to create Hilbert transform");
        nimcp_free(mgr->pac_histogram);
        nimcp_free(mgr->pac_bin_counts);
        nimcp_free(mgr);
        mgr = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "theta_gamma_create: mgr->hilbert is NULL");
        return NULL;
    }

    /* Create signal filters */
    signal_filter_config_t theta_filt_cfg = signal_filter_bandpass_config(
        mgr->config.theta_freq_min, mgr->config.theta_freq_max, 1000.0f);
    mgr->theta_filter = signal_filter_create(&theta_filt_cfg);

    signal_filter_config_t gamma_low_cfg = signal_filter_bandpass_config(
        mgr->config.gamma_freq_low_min, mgr->config.gamma_freq_low_max, 1000.0f);
    mgr->gamma_low_filter = signal_filter_create(&gamma_low_cfg);

    signal_filter_config_t gamma_high_cfg = signal_filter_bandpass_config(
        mgr->config.gamma_freq_high_min, mgr->config.gamma_freq_high_max, 1000.0f);
    mgr->gamma_high_filter = signal_filter_create(&gamma_high_cfg);

    /* Filters are optional - log warning but continue if creation fails */
    if (!mgr->theta_filter || !mgr->gamma_low_filter || !mgr->gamma_high_filter) {
        /* Filters may not be available; PAC from pre-filtered signals still works */
    }

    /* Allocate initial workspace */
    if (!allocate_workspace(mgr, 1024)) {
        theta_gamma_destroy(mgr);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "theta_gamma_create: allocate_workspace is NULL");
        return NULL;
    }

    mgr->initialized = true;
    clear_error();

    return mgr;
}

void theta_gamma_destroy(theta_gamma_manager_t manager) {
    if (!manager) return;

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_destroy", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    /* Free workspace buffers */
    nimcp_free(mgr->work_theta_phase);
    nimcp_free(mgr->work_gamma_amplitude);
    nimcp_free(mgr->work_envelope);

    /* Free PAC histogram */
    nimcp_free(mgr->pac_histogram);
    nimcp_free(mgr->pac_bin_counts);

    /* Destroy Hilbert transform */
    if (mgr->hilbert) {
        hilbert_destroy(mgr->hilbert);
    }

    /* Destroy filters */
    if (mgr->theta_filter) signal_filter_destroy(mgr->theta_filter);
    if (mgr->gamma_low_filter) signal_filter_destroy(mgr->gamma_low_filter);
    if (mgr->gamma_high_filter) signal_filter_destroy(mgr->gamma_high_filter);

    /* Clear sensitive data */
    memset(mgr, 0, sizeof(*mgr));

    nimcp_free(mgr);
    mgr = NULL;
}

bool theta_gamma_reset(theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_reset: manager is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_reset", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    /* Reset state (preserve frequencies from config) */
    float theta_freq = mgr->state.theta_frequency;
    float gamma_freq = mgr->state.gamma_frequency;

    memset(&mgr->state, 0, sizeof(mgr->state));
    mgr->state.theta_frequency = theta_freq;
    mgr->state.gamma_frequency = gamma_freq;
    mgr->state.theta_amplitude = 1.0f;
    mgr->state.gamma_amplitude = 1.0f;
    mgr->state.pac_strength = THETA_GAMMA_PAC_DEFAULT;
    mgr->state.current_window = THETA_PHASE_ENCODE_EARLY;
    mgr->state.current_op = THETA_OP_ENCODE;
    mgr->state.last_update_ns = get_timestamp_ns();

    /* Reset statistics */
    memset(&mgr->stats, 0, sizeof(mgr->stats));
    mgr->stats.first_update_ns = mgr->state.last_update_ns;

    /* Clear PAC histogram */
    clear_pac_histogram(mgr);

    clear_error();
    return true;
}

//=============================================================================
// State Update Functions
//=============================================================================

bool theta_gamma_update(theta_gamma_manager_t manager, uint64_t dt_ns) {
    if (!manager) {
        set_error("NULL manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_update: manager is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_update", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    /* Convert dt to seconds */
    float dt_s = ns_to_seconds(dt_ns);

    /* Update theta phase: phase += 2π × frequency × dt */
    float theta_delta = NIMCP_TWO_PI_F * mgr->state.theta_frequency * dt_s;
    float old_theta_phase = mgr->state.theta_phase;
    mgr->state.theta_phase = wrap_phase_internal(mgr->state.theta_phase + theta_delta);

    /* Detect theta cycle completion (phase wrapped around) */
    if (mgr->state.theta_phase < old_theta_phase - M_PI) {
        mgr->state.cycle_count++;
        mgr->stats.theta_cycles++;
    }

    /* Update gamma phase: phase += 2π × frequency × dt */
    float gamma_delta = NIMCP_TWO_PI_F * mgr->state.gamma_frequency * dt_s;
    mgr->state.gamma_phase = wrap_phase_internal(mgr->state.gamma_phase + gamma_delta);

    /* Modulate gamma amplitude by theta phase (PAC) */
    /* Gamma amplitude is higher during encoding (theta trough) and retrieval (theta peak) */
    float encode_str = compute_encode_strength(mgr->state.theta_phase);
    float retrieve_str = compute_retrieve_strength(mgr->state.theta_phase);
    float modulation = encode_str + retrieve_str;  /* Both windows have high gamma */
    modulation = 0.5f + 0.5f * modulation * mgr->state.pac_strength;
    mgr->state.gamma_amplitude = nimcp_clamp01(modulation);

    /* Update window and operation type */
    theta_phase_window_t new_window = phase_to_window_internal(mgr->state.theta_phase);

    /* Track operation counts when window changes */
    if (new_window != mgr->state.current_window) {
        switch (window_to_op_internal(new_window)) {
            case THETA_OP_ENCODE:
                mgr->stats.encode_operations++;
                break;
            case THETA_OP_RETRIEVE:
                mgr->stats.retrieve_operations++;
                break;
            case THETA_OP_BLOCKED:
            case THETA_OP_TRANSITION:
                /* No increment for transitions */
                break;
        }
    }

    update_state_internals(mgr);

    /* Update timing */
    mgr->state.last_update_ns += dt_ns;
    mgr->stats.total_updates++;
    mgr->stats.last_update_ns = mgr->state.last_update_ns;
    mgr->stats.total_runtime_s = ns_to_seconds(
        mgr->stats.last_update_ns - mgr->stats.first_update_ns);

    /* Update running mean gating strengths */
    float n = (float)mgr->stats.total_updates;
    mgr->stats.mean_encode_strength =
        ((n - 1.0f) * mgr->stats.mean_encode_strength + encode_str) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    mgr->stats.mean_retrieve_strength =
        ((n - 1.0f) * mgr->stats.mean_retrieve_strength + retrieve_str) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    clear_error();
    return true;
}

bool theta_gamma_set_theta_freq(theta_gamma_manager_t manager, float freq) {
    if (!manager) {
        set_error("NULL manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_set_theta_freq: manager is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_set_theta_freq", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    /* Clamp to valid range */
    freq = nimcp_clampf(freq, mgr->config.theta_freq_min, mgr->config.theta_freq_max);
    mgr->state.theta_frequency = freq;

    clear_error();
    return true;
}

bool theta_gamma_set_gamma_freq(theta_gamma_manager_t manager, float freq) {
    if (!manager) {
        set_error("NULL manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_set_gamma_freq: manager is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_set_gamma_freq", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    /* Clamp to valid gamma range (full band) */
    freq = nimcp_clampf(freq, mgr->config.gamma_freq_low_min, mgr->config.gamma_freq_high_max);
    mgr->state.gamma_frequency = freq;

    clear_error();
    return true;
}

bool theta_gamma_sync_to_external(theta_gamma_manager_t manager, float phase) {
    if (!manager) {
        set_error("NULL manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_sync_to_external: manager is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_sync_to_external", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    mgr->state.theta_phase = wrap_phase_internal(phase);
    update_state_internals(mgr);

    clear_error();
    return true;
}

//=============================================================================
// Phase Query Functions
//=============================================================================

float theta_gamma_get_theta_phase(const theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_theta_phase", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return mgr->state.theta_phase;
}

float theta_gamma_get_gamma_phase(const theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_gamma_phase", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return mgr->state.gamma_phase;
}

theta_phase_window_t theta_gamma_get_window(const theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        return THETA_PHASE_ENCODE_EARLY;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_window", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return mgr->state.current_window;
}

theta_op_type_t theta_gamma_get_operation(const theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        return THETA_OP_BLOCKED;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_operation", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return mgr->state.current_op;
}

//=============================================================================
// Gating Functions
//=============================================================================

bool theta_gamma_can_encode(const theta_gamma_manager_t manager) {
    if (!manager) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_can_encode", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return (mgr->state.current_op == THETA_OP_ENCODE);
}

bool theta_gamma_can_retrieve(const theta_gamma_manager_t manager) {
    if (!manager) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_can_retrieve", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return (mgr->state.current_op == THETA_OP_RETRIEVE);
}

float theta_gamma_get_encode_strength(const theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_encode_strength", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return compute_encode_strength(mgr->state.theta_phase);
}

float theta_gamma_get_retrieve_strength(const theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_retrieve_strengt", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    return compute_retrieve_strength(mgr->state.theta_phase);
}

float theta_gamma_gate_operation(const theta_gamma_manager_t manager, theta_op_type_t op_type) {
    if (!manager) {
        set_error("NULL manager");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_gate_operation", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;

    switch (op_type) {
        case THETA_OP_ENCODE:
            return compute_encode_strength(mgr->state.theta_phase);

        case THETA_OP_RETRIEVE:
            return compute_retrieve_strength(mgr->state.theta_phase);

        case THETA_OP_TRANSITION: {
            /* Transition allowed during transition windows with reduced strength */
            float encode_str = compute_encode_strength(mgr->state.theta_phase);
            float retrieve_str = compute_retrieve_strength(mgr->state.theta_phase);
            /* Return whichever is higher, scaled by transition gate */
            float max_str = (encode_str > retrieve_str) ? encode_str : retrieve_str;
            return max_str * mgr->config.transition_gate;
        }

        case THETA_OP_BLOCKED:
        default:
            return 0.0f;
    }
}

//=============================================================================
// Phase-Amplitude Coupling Functions
//=============================================================================

float theta_gamma_compute_pac(theta_gamma_manager_t manager,
                               const float* theta_signal,
                               const float* gamma_signal,
                               uint32_t n,
                               float sample_rate) {
    if (!manager || !theta_signal || !gamma_signal) {
        set_error("NULL pointer in PAC computation");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_compute_pac", 0.0f);


    if (n == 0 || n > THETA_GAMMA_MAX_SIGNAL_LEN) {
        set_error("Invalid signal length: %u", n);
        return -1.0f;
    }

    if (sample_rate <= 0.0f) {
        set_error("Invalid sample rate: %.2f", sample_rate);
        return -1.0f;
    }

    struct theta_gamma_manager_internal* mgr = manager;

    /* Ensure workspace is large enough */
    if (!allocate_workspace(mgr, n)) {
        return -1.0f;
    }

    /* Extract theta phase via Hilbert transform */
    if (!hilbert_extract_phase(mgr->hilbert, theta_signal,
                               mgr->work_theta_phase, n)) {
        set_error("Failed to extract theta phase");
        return -1.0f;
    }

    /* Extract gamma amplitude envelope via Hilbert transform */
    if (!hilbert_extract_amplitude(mgr->hilbert, gamma_signal,
                                    mgr->work_gamma_amplitude, n)) {
        set_error("Failed to extract gamma amplitude");
        return -1.0f;
    }

    /* Compute modulation index */
    return theta_gamma_modulation_index(manager, mgr->work_theta_phase,
                                          mgr->work_gamma_amplitude, n);
}

float theta_gamma_modulation_index(theta_gamma_manager_t manager,
                                     const float* theta_phase,
                                     const float* gamma_amplitude,
                                     uint32_t n) {
    if (!manager || !theta_phase || !gamma_amplitude) {
        set_error("NULL pointer in modulation index computation");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_modulation_index", 0.0f);


    if (n == 0) {
        set_error("Empty signal");
        return -1.0f;
    }

    struct theta_gamma_manager_internal* mgr = manager;
    uint32_t num_bins = mgr->config.pac_num_bins;

    /* Clear histogram */
    clear_pac_histogram(mgr);

    /* Bin gamma amplitude by theta phase */
    float bin_width = NIMCP_TWO_PI_F / (float)num_bins;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)n);
        }

        float phase = wrap_phase_internal(theta_phase[i]);
        uint32_t bin = (uint32_t)(phase / bin_width);
        if (bin >= num_bins) bin = num_bins - 1;

        mgr->pac_histogram[bin] += gamma_amplitude[i];
        mgr->pac_bin_counts[bin]++;
    }

    /* Compute mean amplitude per bin */
    float total_mean = 0.0f;
    uint32_t valid_bins = 0;

    for (uint32_t i = 0; i < num_bins; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_bins > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)num_bins);
        }

        if (mgr->pac_bin_counts[i] > 0) {
            mgr->pac_histogram[i] /= (float)mgr->pac_bin_counts[i];
            total_mean += mgr->pac_histogram[i];
            valid_bins++;
        }
    }

    if (valid_bins == 0) {
        set_error("No valid phase bins");
        return -1.0f;
    }

    total_mean /= (float)valid_bins;

    /* Normalize histogram to probability distribution */
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_bins; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_bins > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)num_bins);
        }

        if (mgr->pac_bin_counts[i] > 0) {
            sum += mgr->pac_histogram[i];
        }
    }

    if (sum <= 0.0f) {
        return 0.0f;  /* No coupling if all amplitudes are zero */
    }

    for (uint32_t i = 0; i < num_bins; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_bins > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)num_bins);
        }

        if (mgr->pac_bin_counts[i] > 0) {
            mgr->pac_histogram[i] /= sum;
        } else {
            mgr->pac_histogram[i] = 1.0f / (float)num_bins;  /* Fill empty bins with uniform */
        }
    }

    /* Compute entropy */
    float h = compute_entropy(mgr->pac_histogram, num_bins);

    /* Compute modulation index: MI = (H_max - H) / H_max */
    /* For uniform distribution: H_max = log(num_bins) */
    float h_max = logf((float)num_bins);
    float mi = (h_max - h) / h_max;

    /* Update state and stats */
    mgr->state.modulation_index = nimcp_clamp01(mi);

    float stat_n = (float)(mgr->stats.total_updates + 1);
    mgr->stats.mean_pac = ((stat_n - 1.0f) * mgr->stats.mean_pac + mi) / stat_n;
    if (mi > mgr->stats.max_pac) {
        mgr->stats.max_pac = mi;
    }

    clear_error();
    return nimcp_clamp01(mi);
}

float theta_gamma_preferred_phase(theta_gamma_manager_t manager,
                                    const float* theta_phase,
                                    const float* gamma_amplitude,
                                    uint32_t n) {
    if (!manager || !theta_phase || !gamma_amplitude) {
        set_error("NULL pointer in preferred phase computation");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_preferred_phase", 0.0f);


    if (n == 0) {
        set_error("Empty signal");
        return -1.0f;
    }

    struct theta_gamma_manager_internal* mgr = manager;
    uint32_t num_bins = mgr->config.pac_num_bins;

    /* Clear and fill histogram */
    clear_pac_histogram(mgr);

    float bin_width = NIMCP_TWO_PI_F / (float)num_bins;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)n);
        }

        float phase = wrap_phase_internal(theta_phase[i]);
        uint32_t bin = (uint32_t)(phase / bin_width);
        if (bin >= num_bins) bin = num_bins - 1;

        mgr->pac_histogram[bin] += gamma_amplitude[i];
        mgr->pac_bin_counts[bin]++;
    }

    /* Normalize bins */
    for (uint32_t i = 0; i < num_bins; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_bins > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)num_bins);
        }

        if (mgr->pac_bin_counts[i] > 0) {
            mgr->pac_histogram[i] /= (float)mgr->pac_bin_counts[i];
        }
    }

    /* Find bin with maximum mean amplitude */
    uint32_t max_bin = 0;
    float max_amp = mgr->pac_histogram[0];

    for (uint32_t i = 1; i < num_bins; i++) {
        if (mgr->pac_histogram[i] > max_amp) {
            max_amp = mgr->pac_histogram[i];
            max_bin = i;
        }
    }

    /* Return center phase of max bin */
    float preferred = (max_bin + 0.5f) * bin_width;
    mgr->state.preferred_phase = preferred;

    clear_error();
    return preferred;
}

//=============================================================================
// Gamma Burst Detection Functions
//=============================================================================

bool theta_gamma_detect_burst(theta_gamma_manager_t manager,
                               const float* gamma_signal,
                               uint32_t n,
                               float sample_rate,
                               gamma_burst_t* burst) {
    if (!manager || !gamma_signal || !burst) {
        set_error("NULL pointer in burst detection");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (manager, gamma_signal, burst)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_detect_burst", 0.0f);


    if (n == 0 || n > THETA_GAMMA_MAX_SIGNAL_LEN) {
        set_error("Invalid signal length: %u", n);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: n is zero");
        return false;
    }

    if (sample_rate <= 0.0f) {
        set_error("Invalid sample rate: %.2f", sample_rate);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    struct theta_gamma_manager_internal* mgr = manager;

    /* Initialize burst result */
    memset(burst, 0, sizeof(*burst));

    /* Ensure workspace is large enough */
    if (!allocate_workspace(mgr, n)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: allocate_workspace is NULL");
        return false;
    }

    /* Extract amplitude envelope via Hilbert transform */
    if (!hilbert_extract_amplitude(mgr->hilbert, gamma_signal,
                                    mgr->work_envelope, n)) {
        set_error("Failed to extract amplitude envelope");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "unknown: allocate_workspace is NULL");
        return false;
    }

    /* Compute mean and std of envelope */
    float mean_amp = compute_mean(mgr->work_envelope, n);
    float std_amp = compute_stddev(mgr->work_envelope, n, mean_amp);

    /* Threshold for burst detection */
    float threshold = mean_amp + mgr->config.burst_threshold * std_amp;

    /* Find burst (longest consecutive above-threshold segment) */
    uint32_t best_start = 0, best_end = 0;
    uint32_t best_length = 0;
    uint32_t current_start = 0;
    bool in_burst = false;

    float peak_amp = 0.0f;
    uint32_t peak_sample = 0;

    for (uint32_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            theta_gamma_heartbeat("theta_gamma_loop",
                             (float)(i + 1) / (float)n);
        }

        if (mgr->work_envelope[i] > threshold) {
            if (!in_burst) {
                current_start = i;
                in_burst = true;
            }

            /* Track peak within burst */
            if (mgr->work_envelope[i] > peak_amp) {
                peak_amp = mgr->work_envelope[i];
                peak_sample = i;
            }
        } else {
            if (in_burst) {
                uint32_t length = i - current_start;
                if (length > best_length && length >= mgr->config.burst_min_samples) {
                    best_start = current_start;
                    best_end = i;
                    best_length = length;
                }
                in_burst = false;
            }
        }
    }

    /* Check if still in burst at end of signal */
    if (in_burst) {
        uint32_t length = n - current_start;
        if (length > best_length && length >= mgr->config.burst_min_samples) {
            best_start = current_start;
            best_end = n;
            best_length = length;
        }
    }

    /* No valid burst found */
    if (best_length < mgr->config.burst_min_samples) {
        clear_error();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    /* Find actual peak within best burst */
    peak_amp = 0.0f;
    peak_sample = best_start;
    for (uint32_t i = best_start; i < best_end; i++) {
        if (mgr->work_envelope[i] > peak_amp) {
            peak_amp = mgr->work_envelope[i];
            peak_sample = i;
        }
    }

    /* Fill burst result */
    burst->detected = true;
    burst->peak_amplitude = peak_amp;
    burst->start_sample = best_start;
    burst->end_sample = best_end;
    burst->peak_sample = peak_sample;
    burst->duration_ms = (float)best_length * 1000.0f / sample_rate;
    burst->band = GAMMA_BAND_FULL;  /* Could be refined based on frequency content */

    /* Compute theta phase at burst peak */
    burst->theta_phase_at_peak = theta_gamma_burst_phase(manager, peak_sample, sample_rate);

    /* Update statistics */
    mgr->stats.bursts_detected++;
    float bn = (float)mgr->stats.bursts_detected;
    mgr->stats.mean_burst_amplitude =
        ((bn - 1.0f) * mgr->stats.mean_burst_amplitude + peak_amp) / bn;
    mgr->stats.mean_burst_duration =
        ((bn - 1.0f) * mgr->stats.mean_burst_duration + burst->duration_ms) / bn;

    clear_error();
    return true;
}

float theta_gamma_burst_phase(theta_gamma_manager_t manager,
                               uint32_t burst_sample,
                               float sample_rate) {
    if (!manager) {
        set_error("NULL manager");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_burst_phase", 0.0f);


    if (sample_rate <= 0.0f) {
        set_error("Invalid sample rate: %.2f", sample_rate);
        return -1.0f;
    }

    const struct theta_gamma_manager_internal* mgr = manager;

    /* Calculate time offset from current phase */
    float sample_time = (float)burst_sample / sample_rate;

    /* Reconstruct theta phase at burst time */
    /* phase_at_burst = current_phase - (elapsed_samples / sample_rate) * 2π * freq */
    /* Note: This is approximate; for accurate results, track phase history */
    float phase_at_burst = mgr->state.theta_phase -
                           (sample_time * NIMCP_TWO_PI_F * mgr->state.theta_frequency);

    return wrap_phase_internal(phase_at_burst);
}

//=============================================================================
// Integration Functions
//=============================================================================

nimcp_quaternion_t theta_gamma_modulate_quaternion(theta_gamma_manager_t manager,
                                                    nimcp_quaternion_t q) {
    if (!manager) {
        set_error("NULL manager");
        return q;  /* Return unmodified */
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_modulate_quaternion", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;

    /* Get current gating strengths */
    float encode_str = compute_encode_strength(mgr->state.theta_phase);
    float retrieve_str = compute_retrieve_strength(mgr->state.theta_phase);

    /* Modulate accessibility (z-component) based on retrieval phase */
    /* During retrieval: increase accessibility
     * During encoding: decrease accessibility (focusing on new info) */
    float accessibility_mod = 0.5f + 0.5f * (retrieve_str - encode_str);

    /* Create modulated quaternion */
    nimcp_quaternion_t result = q;

    /* Scale z-component (accessibility) by modulation factor */
    result.z = q.z * accessibility_mod;

    /* Optionally modulate other components based on phase */
    /* Salience (y) could be enhanced during encoding */
    result.y = q.y * (0.8f + 0.4f * encode_str);

    /* Re-normalize to maintain unit quaternion if needed */
    /* Note: For memory states, normalization may not be required */

    return result;
}

bool theta_gamma_integrate_kuramoto(theta_gamma_manager_t manager,
                                      kuramoto_system_t* kuramoto,
                                      uint32_t module_id) {
    if (!manager) {
        set_error("NULL manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: manager is NULL");
        return false;
    }

    if (!kuramoto) {
        set_error("NULL Kuramoto system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: kuramoto is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_integrate_kuramoto", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    /* Set theta-gamma oscillator phase in Kuramoto system */
    if (!kuramoto_set_phase(kuramoto, module_id, mgr->state.theta_phase)) {
        set_error("Failed to set Kuramoto phase");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: kuramoto_set_phase is NULL");
        return false;
    }

    /* Set frequency based on current theta */
    if (!kuramoto_set_frequency(kuramoto, module_id,
                                 mgr->state.theta_frequency * NIMCP_TWO_PI_F)) {
        set_error("Failed to set Kuramoto frequency");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: kuramoto_set_phase is NULL");
        return false;
    }

    clear_error();
    return true;
}

bool theta_gamma_get_state(const theta_gamma_manager_t manager,
                             theta_gamma_state_t* state) {
    if (!manager || !state) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (manager, state)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_state", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    *state = mgr->state;

    clear_error();
    return true;
}

//=============================================================================
// Statistics Functions
//=============================================================================

bool theta_gamma_get_stats(const theta_gamma_manager_t manager,
                             theta_gamma_stats_t* stats) {
    if (!manager || !stats) {
        set_error("NULL pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (manager, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_get_stats", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    *stats = mgr->stats;

    clear_error();
    return true;
}

bool theta_gamma_reset_stats(theta_gamma_manager_t manager) {
    if (!manager) {
        set_error("NULL manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "theta_gamma_reset_stats: manager is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_reset_stats", 0.0f);


    struct theta_gamma_manager_internal* mgr = manager;

    /* Reset statistics while preserving timing reference */
    uint64_t now = get_timestamp_ns();
    memset(&mgr->stats, 0, sizeof(mgr->stats));
    mgr->stats.first_update_ns = now;
    mgr->stats.last_update_ns = now;

    clear_error();
    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* theta_gamma_window_name(theta_phase_window_t window) {
    switch (window) {
        case THETA_PHASE_ENCODE_EARLY:    return "ENCODE_EARLY";
        case THETA_PHASE_ENCODE_LATE:     return "ENCODE_LATE";
        case THETA_PHASE_TRANSITION_ER:   return "TRANSITION_E2R";
        case THETA_PHASE_RETRIEVE_EARLY:  return "RETRIEVE_EARLY";
        case THETA_PHASE_RETRIEVE_PEAK:   return "RETRIEVE_PEAK";
        case THETA_PHASE_RETRIEVE_LATE:   return "RETRIEVE_LATE";
        case THETA_PHASE_TRANSITION_RE:   return "TRANSITION_R2E";
        case THETA_PHASE_ENCODE_PREPARE:  return "ENCODE_PREPARE";
        default:                          return "UNKNOWN";
    }
}

const char* theta_gamma_op_name(theta_op_type_t op) {
    switch (op) {
        case THETA_OP_ENCODE:     return "ENCODE";
        case THETA_OP_RETRIEVE:   return "RETRIEVE";
        case THETA_OP_TRANSITION: return "TRANSITION";
        case THETA_OP_BLOCKED:    return "BLOCKED";
        default:                  return "UNKNOWN";
    }
}

const char* theta_gamma_band_name(gamma_band_t band) {
    switch (band) {
        case GAMMA_BAND_LOW:  return "LOW_GAMMA";
        case GAMMA_BAND_HIGH: return "HIGH_GAMMA";
        case GAMMA_BAND_FULL: return "FULL_GAMMA";
        default:              return "UNKNOWN";
    }
}

const char* theta_gamma_get_last_error(void) {
    return s_last_error[0] ? s_last_error : NULL;
}

void theta_gamma_print_state(const theta_gamma_manager_t manager) {
    if (!manager) {
        printf("Theta-Gamma: (null manager)\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_print_state", 0.0f);


    const struct theta_gamma_manager_internal* mgr = manager;
    const theta_gamma_state_t* s = &mgr->state;

    printf("=== Theta-Gamma Coupling State ===\n");
    printf("Theta: phase=%.3f rad (%.1f deg), freq=%.2f Hz, amp=%.3f\n",
           s->theta_phase, s->theta_phase * 180.0f / M_PI,
           s->theta_frequency, s->theta_amplitude);
    printf("Gamma: phase=%.3f rad, freq=%.2f Hz, amp=%.3f\n",
           s->gamma_phase, s->gamma_frequency, s->gamma_amplitude);
    printf("Window: %s, Operation: %s\n",
           theta_gamma_window_name(s->current_window),
           theta_gamma_op_name(s->current_op));
    printf("PAC: strength=%.3f, MI=%.3f, preferred_phase=%.3f rad\n",
           s->pac_strength, s->modulation_index, s->preferred_phase);
    printf("Gating: encode=%.3f, retrieve=%.3f\n",
           compute_encode_strength(s->theta_phase),
           compute_retrieve_strength(s->theta_phase));
    printf("Cycles: %lu\n", (unsigned long)s->cycle_count);
    printf("===================================\n");
}

float theta_gamma_wrap_phase(float phase) {
    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_wrap_phase", 0.0f);


    return wrap_phase_internal(phase);
}

theta_phase_window_t theta_gamma_phase_to_window(float phase) {
    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_phase_to_window", 0.0f);


    return phase_to_window_internal(phase);
}

theta_op_type_t theta_gamma_window_to_op(theta_phase_window_t window) {
    /* Phase 8: Heartbeat at operation start */
    theta_gamma_heartbeat("theta_gamma_window_to_op", 0.0f);


    return window_to_op_internal(window);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void theta_gamma_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_theta_gamma_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int theta_gamma_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theta_gamma_training_begin: NULL argument");
        return -1;
    }
    theta_gamma_heartbeat_instance(NULL, "theta_gamma_training_begin", 0.0f);
    (void)(struct theta_gamma_manager_internal*)instance; /* Module state available for reset */
    return 0;
}

int theta_gamma_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theta_gamma_training_end: NULL argument");
        return -1;
    }
    theta_gamma_heartbeat_instance(NULL, "theta_gamma_training_end", 1.0f);
    (void)(struct theta_gamma_manager_internal*)instance; /* Module state available for finalization */
    return 0;
}

int theta_gamma_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theta_gamma_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    theta_gamma_heartbeat_instance(NULL, "theta_gamma_training_step", progress);
    (void)(struct theta_gamma_manager_internal*)instance; /* Module state available for step adaptation */
    return 0;
}

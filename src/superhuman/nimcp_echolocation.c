/**
 * @file nimcp_echolocation.c
 * @brief 3D Spatial Mapping from Echoes - Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bio-inspired echolocation for 3D environment reconstruction
 * WHY:  Enable spatial perception without visual input
 * HOW:  Echo processing, delay analysis, 3D reconstruction from reflections
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * Bat echolocation achieves microsecond temporal resolution through specialized
 * neural circuits in the inferior colliculus. Delay-tuned neurons create
 * topographic maps of distance. This implementation models matched-filter echo
 * detection, Doppler velocity estimation, and spatial reconstruction.
 *
 * REFERENCES:
 * - Moss & Surlykke (2010) "Probing the natural scene by echolocation"
 * - Suga (1990) "Biosonar and neural computation"
 * - Simmons (2012) "Target image representation in bat sonar"
 */

#include "superhuman/nimcp_echolocation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_neural_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(echolocation)

/* ============================================================================
 * Constants
 * ============================================================================ */


/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Pulse template for matched filtering
 */
typedef struct {
    float* samples;             /**< Template waveform */
    uint32_t num_samples;       /**< Template length */
    echo_pulse_params_t params; /**< Pulse parameters */
} pulse_template_t;

/**
 * @brief Object tracking entry
 */
typedef struct {
    echo_object_t object;       /**< Object state */
    echo_point3d_t predicted;   /**< Predicted position */
    float velocity_history[3][8]; /**< Recent velocities (x,y,z) */
    uint32_t history_idx;       /**< Circular buffer index */
    bool active;                /**< Tracking active */
} tracked_object_t;

/**
 * @brief Internal echolocation system
 */
struct echolocation_system {
    /* Configuration */
    echolocation_config_t config;

    /* State */
    echolocation_state_t state;
    echolocation_stats_t stats;

    /* Pulse generation */
    pulse_template_t pulse_template;
    float last_pulse_time_ms;
    uint64_t pulse_sequence;

    /* Echo detection */
    float* correlation_buffer;      /**< Matched filter output */
    uint32_t correlation_size;      /**< Buffer size */
    float* window_function;         /**< FFT window */
    uint32_t fft_size;              /**< FFT size */

    /* Object tracking */
    tracked_object_t* tracked;      /**< Tracked objects array */
    uint32_t max_tracked;           /**< Maximum tracked objects */
    uint32_t next_object_id;        /**< Next object ID */

    /* Environment map */
    echo_environment_map_t* map;    /**< Environment map */

    /* Binaural processing */
    float ear_separation;           /**< Distance between ears */
    float* left_buffer;             /**< Left channel buffer */
    float* right_buffer;            /**< Right channel buffer */
    uint32_t binaural_size;         /**< Binaural buffer size */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t last_process_time_ms;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Track temporal dynamics
 * HOW:  Use CLOCK_MONOTONIC for stable timing
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * WHAT: Convert time delay to range
 * WHY:  Fundamental echolocation calculation
 * HOW:  range = (speed_of_sound * delay) / 2
 */
float echolocation_delay_to_range(float delay_us) {
    return (ECHOLOCATION_SPEED_OF_SOUND * delay_us * 1e-6f) / 2.0f;
}

/**
 * WHAT: Convert range to time delay
 * WHY:  Inverse of delay_to_range
 * HOW:  delay = (2 * range) / speed_of_sound
 */
float echolocation_range_to_delay(float range) {
    return (2.0f * range / ECHOLOCATION_SPEED_OF_SOUND) * 1e6f;
}

/**
 * WHAT: Convert Doppler shift to velocity
 * WHY:  Moving target indication
 * HOW:  v = (doppler / carrier) * (speed_of_sound / 2)
 */
float echolocation_doppler_to_velocity(float doppler_hz, float carrier_hz) {
    if (carrier_hz < 1.0f) return 0.0f;
    return (doppler_hz / carrier_hz) * (ECHOLOCATION_SPEED_OF_SOUND / 2.0f);
}

/**
 * WHAT: Generate click pulse waveform
 * WHY:  Broadband pulse for high resolution
 * HOW:  Gaussian-modulated sinusoid
 */
static int generate_click_pulse(pulse_template_t* tmpl,
                                float frequency,
                                float duration_us,
                                uint32_t sample_rate) {
    uint32_t num_samples = (uint32_t)(duration_us * sample_rate / 1e6f);
    if (num_samples < 2) num_samples = 2;

    tmpl->samples = nimcp_malloc(num_samples * sizeof(float));
    if (!tmpl->samples) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_samples * sizeof(float),
                           "generate_click_pulse: Failed to allocate pulse samples");
        return ECHOLOCATION_ERROR_NO_MEMORY;
    }

    /* Gaussian envelope modulated sinusoid */
    float center = num_samples / 2.0f;
    float sigma = num_samples / 6.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        float env = expf(-0.5f * powf((i - center) / sigma, 2.0f));
        tmpl->samples[i] = env * sinf(2.0f * M_PI * frequency * t);
    }

    tmpl->num_samples = num_samples;
    return ECHOLOCATION_SUCCESS;
}

/**
 * WHAT: Generate chirp pulse waveform
 * WHY:  Frequency-modulated sweep for pulse compression
 * HOW:  Linear frequency sweep
 */
static int generate_chirp_pulse(pulse_template_t* tmpl,
                                float freq_start,
                                float freq_end,
                                float duration_us,
                                uint32_t sample_rate) {
    uint32_t num_samples = (uint32_t)(duration_us * sample_rate / 1e6f);
    if (num_samples < 2) num_samples = 2;

    tmpl->samples = nimcp_malloc(num_samples * sizeof(float));
    if (!tmpl->samples) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_samples * sizeof(float),
                           "generate_chirp_pulse: Failed to allocate pulse samples");
        return ECHOLOCATION_ERROR_NO_MEMORY;
    }

    float chirp_rate = (freq_end - freq_start) / (duration_us * 1e-6f);

    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        float freq = freq_start + chirp_rate * t;
        float phase = 2.0f * M_PI * (freq_start * t + 0.5f * chirp_rate * t * t);

        /* Apply Hann window */
        float window = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (num_samples - 1)));
        tmpl->samples[i] = window * sinf(phase);
    }

    tmpl->num_samples = num_samples;
    return ECHOLOCATION_SUCCESS;
}

/**
 * WHAT: Perform matched filter correlation
 * WHY:  Optimal echo detection in noise
 * HOW:  Cross-correlation with pulse template
 */
static void matched_filter(const float* signal, uint32_t signal_len,
                           const float* template_sig, uint32_t template_len,
                           float* output) {
    for (uint32_t i = 0; i < signal_len - template_len; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < template_len; j++) {
            sum += signal[i + j] * template_sig[j];
        }
        output[i] = sum;
    }
}

/**
 * WHAT: Find peaks in correlation output
 * WHY:  Detect echo locations
 * HOW:  Local maxima above threshold
 */
static uint32_t find_peaks(const float* data, uint32_t len,
                           float threshold, uint32_t min_distance,
                           uint32_t* peak_indices, uint32_t max_peaks) {
    uint32_t num_peaks = 0;

    for (uint32_t i = 1; i < len - 1 && num_peaks < max_peaks; i++) {
        /* Check if local maximum */
        if (data[i] > threshold &&
            data[i] > data[i-1] &&
            data[i] > data[i+1]) {

            /* Check minimum distance from previous peak */
            bool far_enough = true;
            for (uint32_t j = 0; j < num_peaks; j++) {
                if (i - peak_indices[j] < min_distance) {
                    far_enough = false;
                    /* Keep the larger peak */
                    if (data[i] > data[peak_indices[j]]) {
                        peak_indices[j] = i;
                    }
                    break;
                }
            }

            if (far_enough) {
                peak_indices[num_peaks++] = i;
            }
        }
    }

    return num_peaks;
}

/**
 * WHAT: Compute interaural time difference
 * WHY:  Binaural azimuth estimation
 * HOW:  Cross-correlation peak of left/right channels
 */
static float compute_itd(const float* left, const float* right,
                         uint32_t len, uint32_t max_lag) {
    float max_corr = -1e30f;
    int32_t best_lag = 0;

    for (int32_t lag = -(int32_t)max_lag; lag <= (int32_t)max_lag; lag++) {
        float corr = 0.0f;
        uint32_t start = (lag > 0) ? (uint32_t)lag : 0;
        uint32_t end = (lag < 0) ? len + lag : len;

        for (uint32_t i = start; i < end; i++) {
            corr += left[i] * right[i - lag];
        }

        if (corr > max_corr) {
            max_corr = corr;
            best_lag = lag;
        }
    }

    /* Convert lag to time difference */
    return (float)best_lag / ECHOLOCATION_SAMPLE_RATE;
}

/**
 * WHAT: Compute azimuth from ITD
 * WHY:  Convert time difference to angle
 * HOW:  Geometric relationship with ear separation
 */
static float itd_to_azimuth(float itd, float ear_separation) {
    if (ear_separation < 0.001f) return 0.0f;

    /* ITD = (d * sin(theta)) / c */
    float sin_theta = (itd * ECHOLOCATION_SPEED_OF_SOUND) / ear_separation;
    sin_theta = nimcp_clampf(sin_theta, -1.0f, 1.0f);

    return asinf(sin_theta);
}

/**
 * WHAT: Allocate environment map
 * WHY:  Store spatial occupancy information
 * HOW:  Grid of map cells
 */
static echo_environment_map_t* alloc_map(float resolution, uint32_t max_cells) {
    echo_environment_map_t* map = nimcp_calloc(1, sizeof(echo_environment_map_t));
    if (!map) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(echo_environment_map_t),
                           "alloc_map: Failed to allocate environment map");
        return NULL;
    }

    map->cells = nimcp_calloc(max_cells, sizeof(echo_map_cell_t));
    if (!map->cells) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_cells * sizeof(echo_map_cell_t),
                           "alloc_map: Failed to allocate map cells");
        nimcp_free(map);
        return NULL;
    }

    map->max_cells = max_cells;
    map->num_cells = 0;
    map->resolution = resolution;

    return map;
}

/**
 * WHAT: Free environment map
 * WHY:  Clean resource release
 * HOW:  Free cells then map structure
 */
static void free_map(echo_environment_map_t* map) {
    if (!map) return;
    if (map->cells) nimcp_free(map->cells);
    nimcp_free(map);
}

/**
 * WHAT: Find or create map cell at position
 * WHY:  Update occupancy at detected position
 * HOW:  Hash position to cell index
 */
static echo_map_cell_t* get_or_create_cell(echo_environment_map_t* map,
                                           echo_point3d_t position) {
    if (!map || !map->cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "free_map: required parameter is NULL (map, map->cells)");
        return NULL;
    }

    /* Quantize position to grid */
    int32_t gx = (int32_t)(position.x / map->resolution);
    int32_t gy = (int32_t)(position.y / map->resolution);
    int32_t gz = (int32_t)(position.z / map->resolution);

    /* Search existing cells */
    for (uint32_t i = 0; i < map->num_cells; i++) {
        echo_map_cell_t* cell = &map->cells[i];
        int32_t cx = (int32_t)(cell->center.x / map->resolution);
        int32_t cy = (int32_t)(cell->center.y / map->resolution);
        int32_t cz = (int32_t)(cell->center.z / map->resolution);

        if (cx == gx && cy == gy && cz == gz) {
            return cell;
        }
    }

    /* Create new cell if space available */
    if (map->num_cells < map->max_cells) {
        echo_map_cell_t* cell = &map->cells[map->num_cells++];
        cell->center.x = gx * map->resolution + map->resolution / 2.0f;
        cell->center.y = gy * map->resolution + map->resolution / 2.0f;
        cell->center.z = gz * map->resolution + map->resolution / 2.0f;
        cell->occupancy = 0.0f;
        cell->material = ECHO_MATERIAL_UNKNOWN;
        cell->observation_count = 0;
        return cell;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_map: operation failed");
    return NULL;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int echolocation_default_config(echolocation_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_default_config: config is NULL");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    /* Pulse settings - bat-like chirp */
    config->pulse_type = ECHO_PULSE_CHIRP;
    config->pulse_frequency = 40000.0f;     /* 40 kHz start */
    config->pulse_bandwidth = 20000.0f;     /* 20 kHz sweep */
    config->pulse_duration_us = 2000.0f;    /* 2 ms */
    config->pulse_interval_ms = 50.0f;      /* 50 ms between pulses */

    /* Detection settings */
    config->detection_threshold = 0.1f;
    config->max_range = ECHOLOCATION_MAX_RANGE;
    config->min_range = ECHOLOCATION_MIN_RANGE;
    config->range_resolution = 0.01f;       /* 1 cm */
    config->angular_resolution = 5.0f;      /* 5 degrees */

    /* Processing settings */
    config->mode = ECHO_MODE_IMAGING;
    config->enable_doppler = true;
    config->enable_spectral_analysis = true;
    config->enable_material_classification = true;
    config->enable_object_tracking = true;
    config->enable_mapping = true;

    /* Binaural settings */
    config->binaural_mode = true;
    config->ear_separation = 0.17f;         /* Human head width */

    /* Performance */
    config->fft_size = 1024;
    config->max_echoes = ECHOLOCATION_MAX_ECHOES;
    config->max_objects = ECHOLOCATION_MAX_OBJECTS;

    return ECHOLOCATION_SUCCESS;
}

echolocation_system_t* echolocation_create(const echolocation_config_t* config) {
    echolocation_system_t* sys = nimcp_calloc(1, sizeof(echolocation_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate echolocation system");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(echolocation_system_t),
                           "echolocation_create: Failed to allocate system");
        return NULL;
    }

    /* Apply configuration */
    echolocation_config_t default_cfg;
    if (!config) {
        echolocation_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Generate pulse template */
    int result;
    if (config->pulse_type == ECHO_PULSE_CHIRP) {
        result = generate_chirp_pulse(&sys->pulse_template,
                                      config->pulse_frequency,
                                      config->pulse_frequency + config->pulse_bandwidth,
                                      config->pulse_duration_us,
                                      ECHOLOCATION_SAMPLE_RATE);
    } else {
        result = generate_click_pulse(&sys->pulse_template,
                                      config->pulse_frequency,
                                      config->pulse_duration_us,
                                      ECHOLOCATION_SAMPLE_RATE);
    }

    if (result != ECHOLOCATION_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to generate pulse template");
        /* Exception already thrown in generate_*_pulse */
        echolocation_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "echolocation_create: validation failed");
        return NULL;
    }

    /* Allocate correlation buffer */
    sys->correlation_size = ECHOLOCATION_SAMPLE_RATE / 10;  /* 100ms worth */
    sys->correlation_buffer = nimcp_calloc(sys->correlation_size, sizeof(float));
    if (!sys->correlation_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate correlation buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->correlation_size * sizeof(float),
                           "echolocation_create: Failed to allocate correlation buffer");
        echolocation_destroy(sys);
        return NULL;
    }

    /* Allocate FFT window */
    sys->fft_size = config->fft_size;
    sys->window_function = nimcp_malloc(sys->fft_size * sizeof(float));
    if (!sys->window_function) {
        NIMCP_LOGGING_ERROR("Failed to allocate FFT window");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->fft_size * sizeof(float),
                           "echolocation_create: Failed to allocate FFT window");
        echolocation_destroy(sys);
        return NULL;
    }

    /* Initialize Hann window */
    for (uint32_t i = 0; i < sys->fft_size; i++) {
        sys->window_function[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (sys->fft_size - 1)));
    }

    /* Allocate tracking array */
    sys->max_tracked = config->max_objects;
    sys->tracked = nimcp_calloc(sys->max_tracked, sizeof(tracked_object_t));
    if (!sys->tracked) {
        NIMCP_LOGGING_ERROR("Failed to allocate tracking array");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->max_tracked * sizeof(tracked_object_t),
                           "echolocation_create: Failed to allocate tracking array");
        echolocation_destroy(sys);
        return NULL;
    }
    sys->next_object_id = 1;

    /* Allocate environment map */
    if (config->enable_mapping) {
        sys->map = alloc_map(ECHOLOCATION_MAP_RESOLUTION, ECHOLOCATION_MAX_MAP_CELLS);
        if (!sys->map) {
            NIMCP_LOGGING_ERROR("Failed to allocate environment map");
            /* Exception already thrown in alloc_map */
            echolocation_destroy(sys);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "echolocation_create: sys->map is NULL");
            return NULL;
        }
    }

    /* Allocate binaural buffers */
    if (config->binaural_mode) {
        sys->ear_separation = config->ear_separation;
        sys->binaural_size = sys->correlation_size;
        sys->left_buffer = nimcp_calloc(sys->binaural_size, sizeof(float));
        sys->right_buffer = nimcp_calloc(sys->binaural_size, sizeof(float));

        if (!sys->left_buffer || !sys->right_buffer) {
            NIMCP_LOGGING_ERROR("Failed to allocate binaural buffers");
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                               sys->binaural_size * sizeof(float),
                               "echolocation_create: Failed to allocate binaural buffers");
            echolocation_destroy(sys);
            return NULL;
        }
    }

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_THREADING(NIMCP_ERROR_THREAD_CREATE, 0,
                              "echolocation_create: Failed to create mutex%s", "");
        echolocation_destroy(sys);
        return NULL;
    }

    /* Initialize state */
    sys->state.is_initialized = true;
    sys->last_process_time_ms = get_time_ms();

    NIMCP_LOGGING_INFO("Echolocation system created: %s pulse, %.0f Hz, %.0f us",
                       config->pulse_type == ECHO_PULSE_CHIRP ? "chirp" : "click",
                       config->pulse_frequency, config->pulse_duration_us);

    return sys;
}

void echolocation_destroy(echolocation_system_t* system) {
    if (!system) return;

    /* Free pulse template */
    if (system->pulse_template.samples) {
        nimcp_free(system->pulse_template.samples);
    }

    /* Free buffers */
    if (system->correlation_buffer) nimcp_free(system->correlation_buffer);
    if (system->window_function) nimcp_free(system->window_function);
    if (system->tracked) nimcp_free(system->tracked);
    if (system->left_buffer) nimcp_free(system->left_buffer);
    if (system->right_buffer) nimcp_free(system->right_buffer);

    /* Free map */
    if (system->map) free_map(system->map);

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
    NIMCP_LOGGING_INFO("Echolocation system destroyed");
}

int echolocation_reset(echolocation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_reset: system is NULL");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Reset state */
    system->state.pulse_count = 0;
    system->state.last_pulse_time_ms = 0.0f;
    system->state.pulse_in_flight = false;
    system->state.echoes_detected = 0;
    system->state.objects_detected = 0;
    system->state.nearest_range = ECHOLOCATION_MAX_RANGE;
    system->state.farthest_range = 0.0f;
    system->state.ambient_noise_level = 0.0f;
    system->state.signal_to_noise = 0.0f;
    system->state.map_cells_occupied = 0;
    system->state.processing_load = 0.0f;

    /* Clear tracking */
    for (uint32_t i = 0; i < system->max_tracked; i++) {
        system->tracked[i].active = false;
    }

    /* Clear map */
    if (system->map) {
        system->map->num_cells = 0;
    }

    /* Reset sequence */
    system->pulse_sequence = 0;

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Echolocation system reset");
    return ECHOLOCATION_SUCCESS;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int echolocation_set_config(echolocation_system_t* system,
                            const echolocation_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_set_config: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->config = *config;
    system->ear_separation = config->ear_separation;
    nimcp_platform_mutex_unlock(system->mutex);

    return ECHOLOCATION_SUCCESS;
}

int echolocation_get_config(const echolocation_system_t* system,
                            echolocation_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_get_config: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    *config = system->config;
    return ECHOLOCATION_SUCCESS;
}

int echolocation_set_pulse_params(echolocation_system_t* system,
                                  const echo_pulse_params_t* params) {
    if (!system || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_set_pulse_params: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Free old template */
    if (system->pulse_template.samples) {
        nimcp_free(system->pulse_template.samples);
        system->pulse_template.samples = NULL;
    }

    /* Generate new template */
    int result;
    if (params->type == ECHO_PULSE_CHIRP) {
        result = generate_chirp_pulse(&system->pulse_template,
                                      params->frequency_start,
                                      params->frequency_end,
                                      params->duration_us,
                                      ECHOLOCATION_SAMPLE_RATE);
    } else {
        result = generate_click_pulse(&system->pulse_template,
                                      params->frequency_start,
                                      params->duration_us,
                                      ECHOLOCATION_SAMPLE_RATE);
    }

    system->pulse_template.params = *params;

    nimcp_platform_mutex_unlock(system->mutex);
    return result;
}

int echolocation_set_mode(echolocation_system_t* system,
                          echo_processing_mode_t mode) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_set_mode: system is NULL");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->config.mode = mode;
    nimcp_platform_mutex_unlock(system->mutex);

    return ECHOLOCATION_SUCCESS;
}

/* ============================================================================
 * Processing Implementation
 * ============================================================================ */

int echolocation_generate_pulse(echolocation_system_t* system,
                                float* output_samples,
                                uint32_t buffer_size,
                                uint32_t* num_samples) {
    if (!system || !output_samples || !num_samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_generate_pulse: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    if (system->pulse_template.num_samples > buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW,
                              "echolocation_generate_pulse: buffer too small");
        nimcp_platform_mutex_unlock(system->mutex);
        return ECHOLOCATION_ERROR_BUFFER_TOO_SMALL;
    }

    /* Copy pulse template to output */
    memcpy(output_samples, system->pulse_template.samples,
           system->pulse_template.num_samples * sizeof(float));
    *num_samples = system->pulse_template.num_samples;

    /* Update state */
    system->state.pulse_count++;
    system->state.last_pulse_time_ms = (float)get_time_ms();
    system->state.pulse_in_flight = true;
    system->pulse_sequence++;
    system->stats.total_pulses++;

    nimcp_platform_mutex_unlock(system->mutex);
    return ECHOLOCATION_SUCCESS;
}

int echolocation_process_audio(echolocation_system_t* system,
                               const echo_audio_buffer_t* audio,
                               echolocation_output_t* output) {
    if (!system || !audio || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_process_audio: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }
    if (!audio->samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "echolocation_process_audio: audio->samples is NULL");
        return ECHOLOCATION_ERROR_NO_SIGNAL;
    }

    uint64_t start_time = get_time_ms();

    /* Detect echoes */
    int result = echolocation_detect_echoes(system, audio,
                                            output->echoes, output->max_echoes,
                                            &output->num_echoes);
    if (result != ECHOLOCATION_SUCCESS) return result;

    /* Analyze Doppler if enabled */
    if (system->config.enable_doppler) {
        for (uint32_t i = 0; i < output->num_echoes; i++) {
            echolocation_analyze_doppler(system, &output->echoes[i], NULL);
        }
    }

    /* Classify materials if enabled */
    if (system->config.enable_material_classification) {
        for (uint32_t i = 0; i < output->num_echoes; i++) {
            echo_material_t material;
            float confidence;
            if (echolocation_classify_material(system, &output->echoes[i],
                                               &material, &confidence) == ECHOLOCATION_SUCCESS) {
                system->stats.classification_count++;
            }
        }
    }

    /* Cluster into objects */
    result = echolocation_cluster_objects(system, output->echoes, output->num_echoes,
                                          output->objects, output->max_objects,
                                          &output->num_objects);
    if (result != ECHOLOCATION_SUCCESS) return result;

    /* Track objects */
    if (system->config.enable_object_tracking) {
        echolocation_track_objects(system, output->objects, output->num_objects);
    }

    /* Update map */
    if (system->config.enable_mapping) {
        echolocation_update_map(system, output->objects, output->num_objects);
    }

    /* Update output metadata */
    uint64_t end_time = get_time_ms();
    output->processing_time_ms = (float)(end_time - start_time);
    output->pulse_number = system->pulse_sequence;

    /* Compute SNR */
    float signal_power = 0.0f, noise_power = 0.0f;
    for (uint32_t i = 0; i < output->num_echoes; i++) {
        signal_power += output->echoes[i].amplitude * output->echoes[i].amplitude;
    }
    noise_power = system->state.ambient_noise_level * system->state.ambient_noise_level + 0.001f;
    output->signal_to_noise_db = 10.0f * log10f(signal_power / noise_power + 0.001f);

    /* Update statistics */
    nimcp_platform_mutex_lock(system->mutex);

    system->state.pulse_in_flight = false;
    system->stats.total_echoes += output->num_echoes;
    system->stats.avg_echoes_per_pulse = (system->stats.avg_echoes_per_pulse * 0.99f) +
                                         (output->num_echoes * 0.01f);
    system->stats.avg_processing_time_ms = (system->stats.avg_processing_time_ms * 0.99f) +
                                           (output->processing_time_ms * 0.01f);
    if (output->processing_time_ms > system->stats.max_processing_time_ms) {
        system->stats.max_processing_time_ms = output->processing_time_ms;
    }

    system->last_process_time_ms = end_time;

    nimcp_platform_mutex_unlock(system->mutex);

    return ECHOLOCATION_SUCCESS;
}

int echolocation_detect_echoes(echolocation_system_t* system,
                               const echo_audio_buffer_t* audio,
                               echo_detection_t* echoes,
                               uint32_t max_echoes,
                               uint32_t* num_echoes) {
    if (!system || !audio || !echoes || !num_echoes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_detect_echoes: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    *num_echoes = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* Perform matched filtering */
    uint32_t output_len = audio->num_samples - system->pulse_template.num_samples;
    if (output_len > system->correlation_size) {
        output_len = system->correlation_size;
    }

    /* Use mono or left channel for primary detection */
    const float* signal = audio->samples;
    matched_filter(signal, audio->num_samples,
                   system->pulse_template.samples, system->pulse_template.num_samples,
                   system->correlation_buffer);

    /* Normalize correlation output */
    float max_corr = 0.001f;
    for (uint32_t i = 0; i < output_len; i++) {
        system->correlation_buffer[i] = fabsf(system->correlation_buffer[i]);
        if (system->correlation_buffer[i] > max_corr) {
            max_corr = system->correlation_buffer[i];
        }
    }
    for (uint32_t i = 0; i < output_len; i++) {
        system->correlation_buffer[i] /= max_corr;
    }

    /* Estimate noise level */
    float noise_sum = 0.0f;
    uint32_t noise_count = 0;
    for (uint32_t i = 0; i < output_len; i++) {
        if (system->correlation_buffer[i] < system->config.detection_threshold) {
            noise_sum += system->correlation_buffer[i];
            noise_count++;
        }
    }
    system->state.ambient_noise_level = noise_count > 0 ? noise_sum / noise_count : 0.0f;

    /* Find peaks */
    uint32_t* peak_indices = nimcp_malloc(max_echoes * sizeof(uint32_t));
    if (!peak_indices) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, max_echoes * sizeof(uint32_t),
                           "echolocation_detect_echoes: Failed to allocate peak indices");
        nimcp_platform_mutex_unlock(system->mutex);
        return ECHOLOCATION_ERROR_NO_MEMORY;
    }

    uint32_t min_peak_distance = (uint32_t)(system->config.range_resolution * 2.0f *
                                             audio->sample_rate / ECHOLOCATION_SPEED_OF_SOUND);
    uint32_t num_peaks = find_peaks(system->correlation_buffer, output_len,
                                    system->config.detection_threshold, min_peak_distance,
                                    peak_indices, max_echoes);

    /* Process binaural if available */
    bool have_binaural = audio->num_channels >= 2 && system->config.binaural_mode;
    if (have_binaural) {
        /* Extract left and right channels */
        for (uint32_t i = 0; i < audio->num_samples && i < system->binaural_size; i++) {
            system->left_buffer[i] = audio->samples[i * audio->num_channels];
            system->right_buffer[i] = audio->samples[i * audio->num_channels + 1];
        }
    }

    /* Convert peaks to echo detections */
    float min_range = ECHOLOCATION_MAX_RANGE;
    float max_range = 0.0f;

    for (uint32_t i = 0; i < num_peaks && *num_echoes < max_echoes; i++) {
        uint32_t idx = peak_indices[i];
        echo_detection_t* echo = &echoes[*num_echoes];

        /* Time delay from sample index */
        float time_delay_us = (float)idx / audio->sample_rate * 1e6f;

        /* Skip if outside range limits */
        float range = echolocation_delay_to_range(time_delay_us);
        if (range < system->config.min_range || range > system->config.max_range) {
            continue;
        }

        /* Fill echo detection */
        echo->echo_id = *num_echoes + 1;
        echo->time_delay_us = time_delay_us;
        echo->range = range;
        echo->amplitude = system->correlation_buffer[idx];
        echo->confidence = nimcp_clampf(echo->amplitude / (system->state.ambient_noise_level + 0.001f) / 10.0f,
                                   0.0f, 1.0f);

        /* Direction from binaural processing */
        if (have_binaural) {
            uint32_t window_start = idx > 50 ? idx - 50 : 0;
            uint32_t window_end = idx + 50 < system->binaural_size ? idx + 50 : system->binaural_size;
            uint32_t window_len = window_end - window_start;

            float itd = compute_itd(&system->left_buffer[window_start],
                                    &system->right_buffer[window_start],
                                    window_len, 50);
            echo->direction.azimuth = itd_to_azimuth(itd, system->ear_separation);
        } else {
            echo->direction.azimuth = 0.0f;
        }

        echo->direction.range = range;
        echo->direction.elevation = 0.0f;  /* Would require HRTF analysis */

        /* Initialize Doppler (will be computed later if enabled) */
        echo->doppler_shift_hz = 0.0f;
        echo->radial_velocity = 0.0f;
        echo->spectral_centroid = system->config.pulse_frequency;
        echo->spectral_spread = system->config.pulse_bandwidth;

        /* Track range bounds */
        if (range < min_range) min_range = range;
        if (range > max_range) max_range = range;

        (*num_echoes)++;
    }

    nimcp_free(peak_indices);

    /* Update state */
    system->state.echoes_detected = *num_echoes;
    system->state.nearest_range = min_range;
    system->state.farthest_range = max_range;

    /* Update statistics */
    if (*num_echoes > 0) {
        float total_range = 0.0f;
        float total_conf = 0.0f;
        for (uint32_t i = 0; i < *num_echoes; i++) {
            total_range += echoes[i].range;
            total_conf += echoes[i].confidence;
        }
        system->stats.avg_detection_range = (system->stats.avg_detection_range * 0.99f) +
                                            (total_range / *num_echoes * 0.01f);
        system->stats.avg_detection_confidence = (system->stats.avg_detection_confidence * 0.99f) +
                                                 (total_conf / *num_echoes * 0.01f);
        if (max_range > system->stats.max_detection_range) {
            system->stats.max_detection_range = max_range;
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return ECHOLOCATION_SUCCESS;
}

int echolocation_analyze_doppler(echolocation_system_t* system,
                                 echo_detection_t* echo,
                                 float* velocity) {
    if (!system || !echo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_analyze_doppler: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    /* Simple Doppler estimation from spectral shift */
    /* Real implementation would use short-time Fourier transform */

    float freq_shift = (echo->spectral_centroid - system->config.pulse_frequency);
    echo->doppler_shift_hz = freq_shift;
    echo->radial_velocity = echolocation_doppler_to_velocity(freq_shift,
                                                              system->config.pulse_frequency);

    if (velocity) *velocity = echo->radial_velocity;

    return ECHOLOCATION_SUCCESS;
}

int echolocation_classify_material(echolocation_system_t* system,
                                   const echo_detection_t* echo,
                                   echo_material_t* material,
                                   float* confidence) {
    if (!system || !echo || !material) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_classify_material: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    /* Simple material classification based on echo characteristics */
    /* Real implementation would use learned spectral signatures */

    float amplitude = echo->amplitude;
    float spread = echo->spectral_spread;
    float conf = 0.5f;

    if (amplitude > 0.8f) {
        /* Strong reflection - hard surface */
        if (spread < system->config.pulse_bandwidth * 0.5f) {
            *material = ECHO_MATERIAL_METAL;
            conf = 0.7f;
        } else {
            *material = ECHO_MATERIAL_HARD;
            conf = 0.6f;
        }
    } else if (amplitude > 0.5f) {
        *material = ECHO_MATERIAL_GLASS;
        conf = 0.5f;
    } else if (amplitude > 0.2f) {
        *material = ECHO_MATERIAL_ORGANIC;
        conf = 0.4f;
    } else {
        *material = ECHO_MATERIAL_SOFT;
        conf = 0.3f;
    }

    if (confidence) *confidence = conf;

    return ECHOLOCATION_SUCCESS;
}

int echolocation_cluster_objects(echolocation_system_t* system,
                                 const echo_detection_t* echoes,
                                 uint32_t num_echoes,
                                 echo_object_t* objects,
                                 uint32_t max_objects,
                                 uint32_t* num_objects) {
    if (!system || !echoes || !objects || !num_objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_cluster_objects: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    *num_objects = 0;

    if (num_echoes == 0) return ECHOLOCATION_SUCCESS;

    nimcp_platform_mutex_lock(system->mutex);

    /* Simple clustering: group echoes by proximity */
    float cluster_distance = system->config.range_resolution * 5.0f;  /* 5x range resolution */
    bool* clustered = nimcp_calloc(num_echoes, sizeof(bool));
    if (!clustered) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, num_echoes * sizeof(bool),
                           "echolocation_cluster_objects: Failed to allocate clustered array");
        nimcp_platform_mutex_unlock(system->mutex);
        return ECHOLOCATION_ERROR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < num_echoes && *num_objects < max_objects; i++) {
        if (clustered[i]) continue;

        /* Start new cluster with this echo */
        echo_object_t* obj = &objects[*num_objects];
        memset(obj, 0, sizeof(echo_object_t));

        obj->object_id = system->next_object_id++;

        /* Convert first echo to position */
        echo_point3d_t pos;
        echolocation_echo_to_position(system, &echoes[i], &pos);
        obj->position = pos;
        obj->confidence = echoes[i].confidence;
        obj->echo_count = 1;
        obj->reflectivity = echoes[i].amplitude;

        clustered[i] = true;

        /* Find nearby echoes to add to cluster */
        for (uint32_t j = i + 1; j < num_echoes; j++) {
            if (clustered[j]) continue;

            echo_point3d_t other_pos;
            echolocation_echo_to_position(system, &echoes[j], &other_pos);

            /* Check distance */
            float dx = other_pos.x - pos.x;
            float dy = other_pos.y - pos.y;
            float dz = other_pos.z - pos.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            if (dist < cluster_distance) {
                /* Add to cluster - update centroid */
                float w = 1.0f / (obj->echo_count + 1);
                obj->position.x = obj->position.x * (1.0f - w) + other_pos.x * w;
                obj->position.y = obj->position.y * (1.0f - w) + other_pos.y * w;
                obj->position.z = obj->position.z * (1.0f - w) + other_pos.z * w;
                obj->confidence = (obj->confidence + echoes[j].confidence) / 2.0f;
                obj->reflectivity = (obj->reflectivity + echoes[j].amplitude) / 2.0f;
                obj->echo_count++;
                clustered[j] = true;
            }
        }

        /* Estimate size from cluster spread */
        if (obj->echo_count > 1) {
            obj->size_estimate = cluster_distance * 0.5f;
            obj->shape = ECHO_SHAPE_COMPLEX;
        } else {
            obj->size_estimate = 0.1f;
            obj->shape = ECHO_SHAPE_POINT;
        }

        /* Classify material from dominant echo */
        echolocation_classify_material(system, &echoes[i], &obj->material, NULL);

        (*num_objects)++;
    }

    nimcp_free(clustered);

    /* Update state */
    system->state.objects_detected = *num_objects;
    system->stats.total_objects_detected += *num_objects;

    nimcp_platform_mutex_unlock(system->mutex);
    return ECHOLOCATION_SUCCESS;
}

int echolocation_track_objects(echolocation_system_t* system,
                               echo_object_t* objects,
                               uint32_t num_objects) {
    if (!system || !objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_track_objects: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Age existing tracks */
    for (uint32_t i = 0; i < system->max_tracked; i++) {
        if (!system->tracked[i].active) continue;

        system->tracked[i].object.confidence *= 0.9f;
        if (system->tracked[i].object.confidence < 0.1f) {
            system->tracked[i].active = false;
        }
    }

    /* Match new objects to existing tracks */
    for (uint32_t i = 0; i < num_objects; i++) {
        echo_object_t* obj = &objects[i];

        /* Find closest track */
        tracked_object_t* best_track = NULL;
        float best_dist = 2.0f;  /* Maximum association distance */

        for (uint32_t j = 0; j < system->max_tracked; j++) {
            if (!system->tracked[j].active) continue;

            float dx = obj->position.x - system->tracked[j].predicted.x;
            float dy = obj->position.y - system->tracked[j].predicted.y;
            float dz = obj->position.z - system->tracked[j].predicted.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            if (dist < best_dist) {
                best_dist = dist;
                best_track = &system->tracked[j];
            }
        }

        if (best_track) {
            /* Update existing track */
            float dt = NIMCP_SIMULATION_DT_MS;  /* Normalized time step */

            /* Compute velocity */
            best_track->object.velocity.x = (obj->position.x - best_track->object.position.x) / dt;
            best_track->object.velocity.y = (obj->position.y - best_track->object.position.y) / dt;
            best_track->object.velocity.z = (obj->position.z - best_track->object.position.z) / dt;
            best_track->object.is_moving = sqrtf(
                best_track->object.velocity.x * best_track->object.velocity.x +
                best_track->object.velocity.y * best_track->object.velocity.y +
                best_track->object.velocity.z * best_track->object.velocity.z) > 0.1f;

            /* Update position */
            best_track->object.position = obj->position;
            best_track->object.confidence = nimcp_clampf(best_track->object.confidence + 0.2f, 0.0f, 1.0f);
            best_track->object.frames_tracked++;

            /* Predict next position */
            best_track->predicted.x = obj->position.x + best_track->object.velocity.x;
            best_track->predicted.y = obj->position.y + best_track->object.velocity.y;
            best_track->predicted.z = obj->position.z + best_track->object.velocity.z;

            /* Copy track info back to object */
            obj->velocity = best_track->object.velocity;
            obj->is_moving = best_track->object.is_moving;
            obj->frames_tracked = best_track->object.frames_tracked;
        } else {
            /* Create new track */
            for (uint32_t j = 0; j < system->max_tracked; j++) {
                if (!system->tracked[j].active) {
                    system->tracked[j].object = *obj;
                    system->tracked[j].object.frames_tracked = 1;
                    system->tracked[j].predicted = obj->position;
                    system->tracked[j].active = true;
                    break;
                }
            }
        }
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return ECHOLOCATION_SUCCESS;
}

/* ============================================================================
 * 3D Reconstruction Implementation
 * ============================================================================ */

int echolocation_echo_to_position(echolocation_system_t* system,
                                  const echo_detection_t* echo,
                                  echo_point3d_t* position) {
    if (!system || !echo || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_echo_to_position: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    /* Spherical to Cartesian conversion */
    float r = echo->range;
    float az = echo->direction.azimuth;
    float el = echo->direction.elevation;

    position->x = r * cosf(el) * sinf(az);
    position->y = r * cosf(el) * cosf(az);
    position->z = r * sinf(el);

    return ECHOLOCATION_SUCCESS;
}

int echolocation_update_map(echolocation_system_t* system,
                            const echo_object_t* objects,
                            uint32_t num_objects) {
    if (!system || !objects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_update_map: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }
    if (!system->map) return ECHOLOCATION_SUCCESS;

    nimcp_platform_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < num_objects; i++) {
        const echo_object_t* obj = &objects[i];

        echo_map_cell_t* cell = get_or_create_cell(system->map, obj->position);
        if (cell) {
            /* Update occupancy using Bayesian update */
            float prior = cell->occupancy;
            float likelihood = obj->confidence;
            float posterior = (likelihood * prior) /
                              ((likelihood * prior) + (1.0f - likelihood) * (1.0f - prior) + 0.001f);
            cell->occupancy = nimcp_clampf(posterior, 0.0f, 1.0f);
            cell->material = obj->material;
            cell->observation_count++;
            cell->last_update_time = (float)get_time_ms();
        }
    }

    /* Update bounds */
    if (system->map->num_cells > 0) {
        system->map->bounds_min.x = 1e30f;
        system->map->bounds_min.y = 1e30f;
        system->map->bounds_min.z = 1e30f;
        system->map->bounds_max.x = -1e30f;
        system->map->bounds_max.y = -1e30f;
        system->map->bounds_max.z = -1e30f;

        for (uint32_t i = 0; i < system->map->num_cells; i++) {
            echo_map_cell_t* cell = &system->map->cells[i];
            if (cell->center.x < system->map->bounds_min.x) system->map->bounds_min.x = cell->center.x;
            if (cell->center.y < system->map->bounds_min.y) system->map->bounds_min.y = cell->center.y;
            if (cell->center.z < system->map->bounds_min.z) system->map->bounds_min.z = cell->center.z;
            if (cell->center.x > system->map->bounds_max.x) system->map->bounds_max.x = cell->center.x;
            if (cell->center.y > system->map->bounds_max.y) system->map->bounds_max.y = cell->center.y;
            if (cell->center.z > system->map->bounds_max.z) system->map->bounds_max.z = cell->center.z;
        }
    }

    system->state.map_cells_occupied = system->map->num_cells;
    system->map->last_update = get_time_ms();

    nimcp_platform_mutex_unlock(system->mutex);
    return ECHOLOCATION_SUCCESS;
}

int echolocation_get_map(const echolocation_system_t* system,
                         echo_environment_map_t** map) {
    if (!system || !map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_get_map: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    *map = system->map;
    return ECHOLOCATION_SUCCESS;
}

int echolocation_clear_map(echolocation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_clear_map: system is NULL");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }
    if (!system->map) return ECHOLOCATION_SUCCESS;

    nimcp_platform_mutex_lock(system->mutex);
    system->map->num_cells = 0;
    system->state.map_cells_occupied = 0;
    nimcp_platform_mutex_unlock(system->mutex);

    return ECHOLOCATION_SUCCESS;
}

int echolocation_query_map(const echolocation_system_t* system,
                           echo_point3d_t position,
                           echo_map_cell_t* cell) {
    if (!system || !cell) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_query_map: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }
    if (!system->map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,
                              "echolocation_query_map: map not initialized");
        return ECHOLOCATION_ERROR_NOT_INITIALIZED;
    }

    /* Quantize position */
    int32_t gx = (int32_t)(position.x / system->map->resolution);
    int32_t gy = (int32_t)(position.y / system->map->resolution);
    int32_t gz = (int32_t)(position.z / system->map->resolution);

    /* Search for cell */
    for (uint32_t i = 0; i < system->map->num_cells; i++) {
        echo_map_cell_t* c = &system->map->cells[i];
        int32_t cx = (int32_t)(c->center.x / system->map->resolution);
        int32_t cy = (int32_t)(c->center.y / system->map->resolution);
        int32_t cz = (int32_t)(c->center.z / system->map->resolution);

        if (cx == gx && cy == gy && cz == gz) {
            *cell = *c;
            return ECHOLOCATION_SUCCESS;
        }
    }

    /* Cell not found - return empty */
    memset(cell, 0, sizeof(echo_map_cell_t));
    return ECHOLOCATION_SUCCESS;
}

/* ============================================================================
 * State and Statistics Implementation
 * ============================================================================ */

int echolocation_get_state(const echolocation_system_t* system,
                           echolocation_state_t* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_get_state: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    *state = system->state;
    return ECHOLOCATION_SUCCESS;
}

int echolocation_get_stats(const echolocation_system_t* system,
                           echolocation_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_get_stats: NULL parameter");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    *stats = system->stats;
    return ECHOLOCATION_SUCCESS;
}

int echolocation_reset_stats(echolocation_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "echolocation_reset_stats: system is NULL");
        return ECHOLOCATION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    memset(&system->stats, 0, sizeof(echolocation_stats_t));
    nimcp_platform_mutex_unlock(system->mutex);

    return ECHOLOCATION_SUCCESS;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

echolocation_output_t* echolocation_output_create(uint32_t max_echoes,
                                                  uint32_t max_objects) {
    echolocation_output_t* output = nimcp_calloc(1, sizeof(echolocation_output_t));
    if (!output) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(echolocation_output_t),
                           "echolocation_output_create: Failed to allocate output");
        return NULL;
    }

    output->echoes = nimcp_calloc(max_echoes, sizeof(echo_detection_t));
    output->objects = nimcp_calloc(max_objects, sizeof(echo_object_t));

    if (!output->echoes || !output->objects) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                           "echolocation_output_create: Failed to allocate buffers");
        echolocation_output_destroy(output);
        return NULL;
    }

    output->max_echoes = max_echoes;
    output->max_objects = max_objects;

    return output;
}

void echolocation_output_destroy(echolocation_output_t* output) {
    if (!output) return;

    if (output->echoes) nimcp_free(output->echoes);
    if (output->objects) nimcp_free(output->objects);
    nimcp_free(output);
}

const char* echolocation_error_string(echolocation_error_t error) {
    switch (error) {
        case ECHOLOCATION_SUCCESS:               return "Success";
        case ECHOLOCATION_ERROR_NULL_POINTER:    return "Null pointer";
        case ECHOLOCATION_ERROR_INVALID_PARAM:   return "Invalid parameter";
        case ECHOLOCATION_ERROR_NO_MEMORY:       return "Memory allocation failed";
        case ECHOLOCATION_ERROR_NOT_INITIALIZED: return "System not initialized";
        case ECHOLOCATION_ERROR_INVALID_STATE:   return "Invalid state";
        case ECHOLOCATION_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case ECHOLOCATION_ERROR_NO_SIGNAL:       return "No signal provided";
        case ECHOLOCATION_ERROR_PROCESSING_FAILED: return "Processing failed";
        case ECHOLOCATION_ERROR_TIMEOUT:         return "Operation timeout";
        default:                                  return "Unknown error";
    }
}

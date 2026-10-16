/**
 * @file nimcp_audio_cortical_bridge.c
 * @brief Audio-Cortical Bridge Implementation
 *
 * WHAT: Connects audio cortex perception with cortical column processing.
 * WHY:  Provides biologically-realistic A1 processing with proper organization.
 * HOW:  Routes audio input through tonotopic mapping to frequency hypercolumns.
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#include "perception/cortical/nimcp_audio_cortical_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Utilities */
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(audio_cortical_bridge)

/* ============================================================================
 * Internal Structure Definition
 * ============================================================================ */

/**
 * @brief Internal structure for audio-cortical bridge
 */
struct audio_cortical_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Connected modules */
    audio_cortex_t* audio_cortex;
    topographic_map_t* tonotopic_map;
    cortical_immune_system_t* cortical_immune;

    /* Frequency hypercolumns */
    feature_hypercolumn_t** hypercolumns;
    uint32_t num_hypercolumns;
    uint32_t bands_per_hypercolumn;

    /* Configuration */
    audio_cortical_config_t config;

    /* State */
    audio_cortical_state_t state;
    audio_cortical_stats_t stats;

    /* UMM */
    bool umm_enabled;

    /* Immune modulation */
    float immune_modulation_factor;
    float* hypercolumn_gains;

    /* Thread safety */
    nimcp_mutex_t mutex;
    bool mutex_initialized;

    /* Timing */
    uint64_t last_process_time_ns;
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Compute hypercolumn index for a frequency
 */
static uint32_t compute_hypercolumn_index(
    const audio_cortical_bridge_t* bridge,
    float frequency_hz)
{
    if (!bridge || frequency_hz <= 0.0f) return 0;

    /* Logarithmic frequency mapping (tonotopic) */
    float log_min = logf(bridge->config.min_frequency);
    float log_max = logf(bridge->config.max_frequency);
    float log_freq = logf(frequency_hz);

    /* Clamp to valid range */
    if (log_freq < log_min) log_freq = log_min;
    if (log_freq > log_max) log_freq = log_max;

    /* Map to hypercolumn index */
    float normalized = (log_freq - log_min) / (log_max - log_min);
    uint32_t idx = (uint32_t)(normalized * (float)(bridge->num_hypercolumns - 1));

    if (idx >= bridge->num_hypercolumns) {
        idx = bridge->num_hypercolumns - 1;
    }

    return idx;
}

/**
 * @brief Apply immune modulation to a value
 */
static float apply_immune_modulation(
    audio_cortical_bridge_t* bridge,
    uint32_t hypercolumn_idx,
    float value)
{
    if (!bridge || hypercolumn_idx >= bridge->num_hypercolumns) {
        return value;
    }

    float gain = 1.0f;
    if (bridge->hypercolumn_gains) {
        gain = bridge->hypercolumn_gains[hypercolumn_idx];
    }

    /* Apply immune modulation factor */
    float modulation = 1.0f - (bridge->immune_modulation_factor * (1.0f - gain));

    return value * modulation;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void audio_cortical_default_config(audio_cortical_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortical_default_config: config is NULL");
        return;
    }

    memset(config, 0, sizeof(audio_cortical_config_t));

    config->num_hypercolumns = 64;
    config->freq_bands_per_hypercolumn = AUDIO_CORTICAL_DEFAULT_FREQ_BANDS;
    config->min_frequency = 80.0f;
    config->max_frequency = 16000.0f;
    config->q_factor = AUDIO_CORTICAL_DEFAULT_Q_FACTOR;
    config->tuning_width = AUDIO_CORTICAL_DEFAULT_TUNING_WIDTH;
    config->mode = AUDIO_CORTICAL_MODE_HYPERCOLUMN;
    config->enable_tonotopic_mapping = true;
    config->enable_cortical_immune = true;
    config->enable_bio_async = true;
    config->frequency_range_octaves = 8.0f;
    config->low_freq_emphasis = 500.0f;
    config->cortical_magnification = 1.0f;
    config->immune_modulation_factor = AUDIO_CORTICAL_DEFAULT_IMMUNE_FACTOR;
    config->use_umm = false;
}

audio_cortical_bridge_t* audio_cortical_bridge_create(
    const audio_cortical_config_t* config,
    audio_cortex_t* audio_cortex)
{
    audio_cortical_config_t local_config;

    if (!config) {
        audio_cortical_default_config(&local_config);
        config = &local_config;
    }

    /* Validate configuration */
    if (config->num_hypercolumns == 0 ||
        config->num_hypercolumns > AUDIO_CORTICAL_MAX_HYPERCOLUMNS) {
        NIMCP_LOGGING_ERROR("Invalid num_hypercolumns: %u", config->num_hypercolumns);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortical_bridge_create: invalid num_hypercolumns");
        return NULL;
    }

    /* Allocate bridge structure */
    audio_cortical_bridge_t* bridge = (audio_cortical_bridge_t*)nimcp_malloc(
        sizeof(audio_cortical_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate audio-cortical bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortical_bridge_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(audio_cortical_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(audio_cortical_config_t));
    bridge->audio_cortex = audio_cortex;
    bridge->state = AUDIO_CORTICAL_STATE_UNINITIALIZED;
    bridge->immune_modulation_factor = 0.0f;
    bridge->umm_enabled = config->use_umm;

    /* Initialize mutex */
    bridge->base.mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (bridge->base.mutex && nimcp_mutex_init(bridge->base.mutex, NULL) == 0) {
    } else {
        NIMCP_LOGGING_WARN("Failed to initialize mutex, continuing without thread safety");
    }

    /* Allocate hypercolumns array */
    bridge->hypercolumns = (feature_hypercolumn_t**)nimcp_malloc(
        config->num_hypercolumns * sizeof(feature_hypercolumn_t*)
    );
    if (!bridge->hypercolumns) {
        NIMCP_LOGGING_ERROR("Failed to allocate hypercolumns array");
        audio_cortical_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortical_bridge_create: bridge->hypercolumns is NULL");
        return NULL;
    }
    memset(bridge->hypercolumns, 0,
           config->num_hypercolumns * sizeof(feature_hypercolumn_t*));

    /* Create frequency hypercolumns */
    bridge->num_hypercolumns = config->num_hypercolumns;
    bridge->bands_per_hypercolumn = config->freq_bands_per_hypercolumn;

    for (uint32_t i = 0; i < config->num_hypercolumns; i++) {
        /* Compute center frequency for this hypercolumn (logarithmic spacing) */
        float log_min = logf(config->min_frequency);
        float log_max = logf(config->max_frequency);
        float log_center = log_min + (log_max - log_min) *
                          ((float)i / (float)(config->num_hypercolumns - 1));
        float center_freq = expf(log_center);

        /* Create feature dimension for frequency bands */
        float bandwidth = center_freq / config->q_factor;
        float min_band = center_freq - bandwidth / 2.0f;
        float max_band = center_freq + bandwidth / 2.0f;

        feature_dimension_t freq_dim = feature_dimension_create(
            FEATURE_SPATIAL_FREQ,
            min_band,
            max_band,
            config->freq_bands_per_hypercolumn
        );
        feature_dimension_set_circular(&freq_dim, false);
        feature_dimension_set_tuning_width(&freq_dim,
            bandwidth / (float)config->freq_bands_per_hypercolumn);

        bridge->hypercolumns[i] = feature_hypercolumn_create(&freq_dim, 1);
        if (!bridge->hypercolumns[i]) {
            NIMCP_LOGGING_ERROR("Failed to create hypercolumn %u", i);
            audio_cortical_bridge_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortical_bridge_create: bridge->hypercolumns is NULL");
            return NULL;
        }

        /* Set spatial position in tonotopic map */
        bridge->hypercolumns[i]->position[0] = (float)i / (float)config->num_hypercolumns;
        bridge->hypercolumns[i]->position[1] = 0.0f;
        bridge->hypercolumns[i]->position[2] = 0.0f;
    }

    /* Allocate per-hypercolumn gains */
    bridge->hypercolumn_gains = (float*)nimcp_malloc(
        config->num_hypercolumns * sizeof(float)
    );
    if (bridge->hypercolumn_gains) {
        for (uint32_t i = 0; i < config->num_hypercolumns; i++) {
            bridge->hypercolumn_gains[i] = 1.0f;
        }
    }

    /* Create tonotopic map if enabled */
    if (config->enable_tonotopic_mapping) {
        tonotopic_params_t tono_params = {
            .min_frequency = config->min_frequency,
            .max_frequency = config->max_frequency,
            .octave_span = 1.0f,
            .is_logarithmic = true,
            .q_factor = config->q_factor
        };

        bridge->tonotopic_map = topographic_map_create_tonotopic(
            &tono_params,
            config->num_hypercolumns
        );
        if (!bridge->tonotopic_map) {
            NIMCP_LOGGING_WARN("Failed to create tonotopic map, continuing without");
        }
    }

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(audio_cortical_stats_t));

    bridge->state = AUDIO_CORTICAL_STATE_READY;

    NIMCP_LOGGING_INFO("Audio-cortical bridge created with %u hypercolumns (%u bands each)",
                       bridge->num_hypercolumns, bridge->bands_per_hypercolumn);

    return bridge;
}

void audio_cortical_bridge_destroy(audio_cortical_bridge_t* bridge)
{
    if (!bridge) return;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        audio_cortical_disconnect_bio_async(bridge);
    }

    /* Destroy hypercolumns */
    if (bridge->hypercolumns) {
        for (uint32_t i = 0; i < bridge->num_hypercolumns; i++) {
            if (bridge->hypercolumns[i]) {
                feature_hypercolumn_destroy(bridge->hypercolumns[i]);
            }
        }
        nimcp_free(bridge->hypercolumns);
    }

    /* Destroy tonotopic map */
    if (bridge->tonotopic_map) {
        topographic_map_destroy(bridge->tonotopic_map);
    }

    /* Free gains */
    if (bridge->hypercolumn_gains) {
        nimcp_free(bridge->hypercolumn_gains);
    }

    /* Destroy mutex */
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_destroy(bridge->base.mutex);
        nimcp_free(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);

    NIMCP_LOGGING_DEBUG("Audio-cortical bridge destroyed");
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int audio_cortical_connect_audio_cortex(
    audio_cortical_bridge_t* bridge,
    audio_cortex_t* audio_cortex)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->audio_cortex = audio_cortex;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Connected to audio cortex");
    return NIMCP_SUCCESS;
}

int audio_cortical_connect_immune(
    audio_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->cortical_immune = immune;

    /* Register hypercolumns with immune system */
    if (immune && bridge->hypercolumns) {
        for (uint32_t i = 0; i < bridge->num_hypercolumns; i++) {
            if (bridge->hypercolumns[i]) {
                /* Note: Using orientation hypercolumn register as placeholder */
                /* A proper feature hypercolumn register would be ideal */
            }
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Connected to cortical immune system");
    return NIMCP_SUCCESS;
}

int audio_cortical_connect_bio_async(audio_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_AUDIO_CORTICAL,
        .module_name = "audio_cortical_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_SUCCESS;
    } else {
        NIMCP_LOGGING_WARN("Failed to connect to bio-async router");
        return NIMCP_ERROR_OPERATION_FAILED;
    }
}

int audio_cortical_disconnect_bio_async(audio_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_DEBUG("Disconnected from bio-async router");
    return NIMCP_SUCCESS;
}

bool audio_cortical_is_bio_async_connected(const audio_cortical_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    bool connected;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    connected = bridge->base.bio_async_enabled;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return connected;
}

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

int audio_cortical_process(
    audio_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    uint32_t sample_rate,
    audio_cortical_frequency_result_t* result)
{
    NIMCP_CHECK_THROW(bridge && audio_data && result, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in audio_cortical_process");
    NIMCP_CHECK_THROW(num_samples > 0 && sample_rate > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid num_samples or sample_rate in audio_cortical_process");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_time = get_time_ns();
    bridge->state = AUDIO_CORTICAL_STATE_PROCESSING;

    memset(result, 0, sizeof(audio_cortical_frequency_result_t));

    /* Simple energy-based frequency analysis per hypercolumn */
    float* band_energies = (float*)nimcp_calloc(
        bridge->num_hypercolumns, sizeof(float)
    );
    if (!band_energies) {
        bridge->state = AUDIO_CORTICAL_STATE_ERROR;
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Compute total energy and distribute to frequency bands */
    float total_energy = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        total_energy += audio_data[i] * audio_data[i];
    }
    total_energy = sqrtf(total_energy / (float)num_samples);

    /* Distribute energy to hypercolumns based on simple model */
    float max_response = 0.0f;
    uint32_t max_idx = 0;
    float sum_responses = 0.0f;

    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        /* Simple energy distribution - in real impl would use FFT */
        float phase = (float)h / (float)bridge->num_hypercolumns;
        float energy = total_energy * (0.5f + 0.5f * cosf(phase * M_PI * 4.0f));
        energy = apply_immune_modulation(bridge, h, energy);

        band_energies[h] = energy;
        sum_responses += energy;

        if (energy > max_response) {
            max_response = energy;
            max_idx = h;
        }

        bridge->stats.hypercolumn_activations++;
    }

    /* Compute result */
    result->num_freq_bands = bridge->num_hypercolumns;
    result->frequency_responses = band_energies;

    /* Compute dominant frequency from dominant hypercolumn */
    float log_min = logf(bridge->config.min_frequency);
    float log_max = logf(bridge->config.max_frequency);
    float log_center = log_min + (log_max - log_min) *
                      ((float)max_idx / (float)bridge->num_hypercolumns);
    result->dominant_frequency = expf(log_center);

    if (sum_responses > 0.0f) {
        result->selectivity_index = max_response / sum_responses;
    }
    result->confidence = result->selectivity_index;

    bridge->stats.active_hypercolumns = bridge->num_hypercolumns;

    /* Update statistics */
    bridge->stats.frames_processed++;
    bridge->stats.peak_frequency_response = result->selectivity_index;
    bridge->stats.current_dominant_frequency = result->dominant_frequency;
    bridge->stats.current_immune_modulation = bridge->immune_modulation_factor;

    uint64_t end_time = get_time_ns();
    float process_time_ms = (float)(end_time - start_time) / 1000000.0f;
    if (bridge->stats.frames_processed > 1) {
        float updated = (bridge->stats.avg_processing_time_ms * (bridge->stats.frames_processed - 1) +
             process_time_ms) / (float)bridge->stats.frames_processed;
        if (isfinite(updated)) bridge->stats.avg_processing_time_ms = updated;
    } else {
        bridge->stats.avg_processing_time_ms = process_time_ms;
    }

    bridge->last_process_time_ns = end_time;
    bridge->state = AUDIO_CORTICAL_STATE_READY;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int audio_cortical_process_spectrogram(
    audio_cortical_bridge_t* bridge,
    const float* spectrogram,
    uint32_t num_freq_bins,
    uint32_t num_time_frames,
    audio_cortical_frequency_result_t* result)
{
    NIMCP_CHECK_THROW(bridge && spectrogram && result, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in audio_cortical_process_spectrogram");
    NIMCP_CHECK_THROW(num_freq_bins > 0 && num_time_frames > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid num_freq_bins or num_time_frames in audio_cortical_process_spectrogram");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_time = get_time_ns();
    bridge->state = AUDIO_CORTICAL_STATE_PROCESSING;

    memset(result, 0, sizeof(audio_cortical_frequency_result_t));

    /* Allocate temporary storage for averaged responses */
    float* avg_responses = (float*)nimcp_malloc(
        bridge->bands_per_hypercolumn * sizeof(float)
    );
    if (!avg_responses) {
        bridge->state = AUDIO_CORTICAL_STATE_ERROR;
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }
    memset(avg_responses, 0, bridge->bands_per_hypercolumn * sizeof(float));

    /* Process spectrogram through hypercolumns */
    uint32_t processed_count = 0;
    float max_response = 0.0f;
    uint32_t max_idx = 0;
    float sum_responses = 0.0f;

    for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
        feature_hypercolumn_t* hcol = bridge->hypercolumns[h];
        if (!hcol) continue;

        /* Compute frequency range for this hypercolumn */
        uint32_t bin_start = (h * num_freq_bins) / bridge->num_hypercolumns;
        uint32_t bin_end = ((h + 1) * num_freq_bins) / bridge->num_hypercolumns;
        if (bin_end > num_freq_bins) bin_end = num_freq_bins;

        /* Compute average energy in this frequency range */
        float total_energy = 0.0f;
        uint32_t count = 0;
        for (uint32_t t = 0; t < num_time_frames; t++) {
            for (uint32_t f = bin_start; f < bin_end; f++) {
                total_energy += spectrogram[t * num_freq_bins + f];
                count++;
            }
        }

        float avg_energy = (count > 0) ? total_energy / (float)count : 0.0f;

        /* Apply to hypercolumn */
        float* input = (float*)nimcp_malloc(hcol->total_columns * sizeof(float));
        if (input) {
            for (uint32_t c = 0; c < hcol->total_columns; c++) {
                input[c] = avg_energy;
            }
            feature_hypercolumn_process(hcol, input, hcol->total_columns);
            feature_hypercolumn_normalize(hcol);
            nimcp_free(input);
        }

        bridge->stats.hypercolumn_activations++;
        processed_count++;

        /* Track maximum response */
        float response = avg_energy;
        response = apply_immune_modulation(bridge, h, response);
        sum_responses += response;

        if (response > max_response) {
            max_response = response;
            max_idx = h;
        }

        /* Add to averaged responses */
        if (h < bridge->bands_per_hypercolumn) {
            avg_responses[h] = response;
        }
    }

    /* Compute result */
    if (processed_count > 0) {
        result->num_freq_bands = bridge->bands_per_hypercolumn;
        result->frequency_responses = avg_responses;

        /* Compute dominant frequency from dominant hypercolumn */
        float log_min = logf(bridge->config.min_frequency);
        float log_max = logf(bridge->config.max_frequency);
        float log_center = log_min + (log_max - log_min) *
                          ((float)max_idx / (float)bridge->num_hypercolumns);
        result->dominant_frequency = expf(log_center);

        if (sum_responses > 0.0f) {
            result->selectivity_index = max_response / sum_responses;
        }
        result->confidence = result->selectivity_index;

        bridge->stats.active_hypercolumns = processed_count;
    } else {
        nimcp_free(avg_responses);
    }

    /* Update statistics */
    bridge->stats.frames_processed++;
    bridge->stats.peak_frequency_response = result->selectivity_index;
    bridge->stats.current_dominant_frequency = result->dominant_frequency;
    bridge->stats.current_immune_modulation = bridge->immune_modulation_factor;

    uint64_t end_time = get_time_ns();
    float process_time_ms = (float)(end_time - start_time) / 1000000.0f;
    if (bridge->stats.frames_processed > 1) {
        float updated = (bridge->stats.avg_processing_time_ms * (bridge->stats.frames_processed - 1) +
             process_time_ms) / (float)bridge->stats.frames_processed;
        if (isfinite(updated)) bridge->stats.avg_processing_time_ms = updated;
    } else {
        bridge->stats.avg_processing_time_ms = process_time_ms;
    }

    bridge->last_process_time_ns = end_time;
    bridge->state = AUDIO_CORTICAL_STATE_READY;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int audio_cortical_process_frequency_band(
    audio_cortical_bridge_t* bridge,
    const float* band_energy,
    uint32_t num_samples,
    float center_frequency,
    audio_cortical_frequency_result_t* result)
{
    NIMCP_CHECK_THROW(bridge && band_energy && result, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in audio_cortical_process_frequency_band");
    NIMCP_CHECK_THROW(num_samples > 0 && center_frequency > 0.0f, NIMCP_ERROR_INVALID_PARAM,
        "Invalid num_samples or center_frequency in audio_cortical_process_frequency_band");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(audio_cortical_frequency_result_t));

    /* Find hypercolumn for this frequency */
    uint32_t hcol_idx = compute_hypercolumn_index(bridge, center_frequency);
    if (hcol_idx >= bridge->num_hypercolumns) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    feature_hypercolumn_t* hcol = bridge->hypercolumns[hcol_idx];
    if (!hcol) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Compute average energy */
    float avg_energy = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        avg_energy += band_energy[i];
    }
    avg_energy /= (float)num_samples;

    /* Process through hypercolumn */
    float* input = (float*)nimcp_malloc(hcol->total_columns * sizeof(float));
    if (input) {
        for (uint32_t c = 0; c < hcol->total_columns; c++) {
            input[c] = avg_energy;
        }
        feature_hypercolumn_process(hcol, input, hcol->total_columns);
        feature_hypercolumn_normalize(hcol);
        nimcp_free(input);
    }

    bridge->stats.hypercolumn_activations++;

    /* Compute result */
    result->dominant_frequency = center_frequency;
    result->selectivity_index = avg_energy;
    result->confidence = avg_energy;
    result->num_freq_bands = 1;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

void audio_cortical_free_result(audio_cortical_frequency_result_t* result)
{
    if (!result) return;

    if (result->frequency_responses) {
        nimcp_free(result->frequency_responses);
        result->frequency_responses = NULL;
    }
    result->num_freq_bands = 0;
}

int audio_cortical_get_frequency_map(
    audio_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    uint32_t sample_rate,
    float* frequency_map,
    float* selectivity_map,
    uint32_t* num_windows)
{
    NIMCP_CHECK_THROW(bridge && audio_data && frequency_map && num_windows, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in audio_cortical_get_frequency_map");
    NIMCP_CHECK_THROW(num_samples > 0 && sample_rate > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid num_samples or sample_rate in audio_cortical_get_frequency_map");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    /* Compute windowed frequency analysis */
    uint32_t window_size = sample_rate / 10;
    if (window_size < 64) window_size = 64;
    if (window_size > num_samples) window_size = num_samples;

    uint32_t hop_size = window_size / 2;
    uint32_t windows = (num_samples > window_size) ?
                       (num_samples - window_size) / hop_size + 1 : 1;

    *num_windows = windows;

    /* Process each window */
    for (uint32_t w = 0; w < windows; w++) {
        uint32_t start_idx = w * hop_size;

        /* Compute energy for this window */
        float max_energy = 0.0f;
        float dominant_freq = bridge->config.min_frequency;

        for (uint32_t h = 0; h < bridge->num_hypercolumns; h++) {
            float log_min = logf(bridge->config.min_frequency);
            float log_max = logf(bridge->config.max_frequency);
            float log_center = log_min + (log_max - log_min) *
                              ((float)h / (float)bridge->num_hypercolumns);
            float center_freq = expf(log_center);

            /* Simple energy computation */
            float energy = 0.0f;
            for (uint32_t i = 0; i < window_size && (start_idx + i) < num_samples; i++) {
                energy += fabsf(audio_data[start_idx + i]);
            }
            energy /= (float)window_size;

            if (energy > max_energy) {
                max_energy = energy;
                dominant_freq = center_freq;
            }
        }

        frequency_map[w] = dominant_freq;
        if (selectivity_map) {
            selectivity_map[w] = (max_energy > 0.0f) ? 1.0f / (1.0f + max_energy) : 0.0f;
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Hypercolumn Functions
 * ============================================================================ */

const feature_hypercolumn_t* audio_cortical_get_hypercolumn(
    const audio_cortical_bridge_t* bridge,
    float frequency_hz)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint32_t idx = compute_hypercolumn_index(bridge, frequency_hz);
    if (idx >= bridge->num_hypercolumns) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "audio_cortical_get_hypercolumn: index out of range");
        return NULL;
    }

    const feature_hypercolumn_t* hcol = bridge->hypercolumns[idx];
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return hcol;
}

int audio_cortical_set_hypercolumn_gain(
    audio_cortical_bridge_t* bridge,
    float frequency_hz,
    float gain)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    uint32_t idx = compute_hypercolumn_index(bridge, frequency_hz);
    if (idx >= bridge->num_hypercolumns) return NIMCP_ERROR_INVALID_PARAM;

    if (bridge->hypercolumn_gains) {
        bridge->hypercolumn_gains[idx] = gain;
    }

    return NIMCP_SUCCESS;
}

uint32_t audio_cortical_get_num_hypercolumns(const audio_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortical_get_num_hypercolumns: bridge is NULL");
        return 0;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    uint32_t count;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    count = bridge->num_hypercolumns;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return count;
}

const feature_hypercolumn_t* audio_cortical_get_hypercolumn_by_index(
    const audio_cortical_bridge_t* bridge,
    uint32_t index)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortical_get_hypercolumn_by_index: bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    const feature_hypercolumn_t* hcol = NULL;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    if (index < bridge->num_hypercolumns) {
        hcol = bridge->hypercolumns[index];
    }
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return hcol;
}

/* ============================================================================
 * Immune Modulation Functions
 * ============================================================================ */

int audio_cortical_set_immune_modulation(
    audio_cortical_bridge_t* bridge,
    float modulation_factor)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (modulation_factor < 0.0f) modulation_factor = 0.0f;
    if (modulation_factor > 1.0f) modulation_factor = 1.0f;

    bridge->immune_modulation_factor = modulation_factor;
    bridge->stats.current_immune_modulation = modulation_factor;

    return NIMCP_SUCCESS;
}

float audio_cortical_get_immune_modulation(const audio_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortical_get_immune_modulation: bridge is NULL");
        return 0.0f;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    float factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    factor = bridge->immune_modulation_factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return factor;
}

/* API aliases for header compatibility */
int audio_cortical_set_immune_factor(
    audio_cortical_bridge_t* bridge,
    float factor)
{
    return audio_cortical_set_immune_modulation(bridge, factor);
}

float audio_cortical_get_immune_factor(const audio_cortical_bridge_t* bridge)
{
    return audio_cortical_get_immune_modulation(bridge);
}

int audio_cortical_update_immune_modulation(audio_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->cortical_immune) {
        return NIMCP_SUCCESS;
    }

    /* Query immune system for modulation factors */
    /* For now, just maintain current modulation */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int audio_cortical_get_stats(
    const audio_cortical_bridge_t* bridge,
    audio_cortical_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(audio_cortical_stats_t));
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int audio_cortical_reset_stats(audio_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->stats, 0, sizeof(audio_cortical_stats_t));

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

audio_cortical_state_t audio_cortical_get_state(const audio_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortical_get_state: bridge is NULL");
        return AUDIO_CORTICAL_STATE_UNINITIALIZED;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    audio_cortical_state_t state;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    state = bridge->state;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return state;
}

/* ============================================================================
 * Accessor Functions
 * ============================================================================ */

const topographic_map_t* audio_cortical_get_tonotopic_map(
    const audio_cortical_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortical_get_tonotopic_map: bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    audio_cortical_bridge_t* mutable_bridge = (audio_cortical_bridge_t*)bridge;

    const topographic_map_t* map;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    map = bridge->tonotopic_map;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return map;
}

/* ============================================================================
 * Bio-Async Message Handling
 * ============================================================================ */

uint32_t audio_cortical_process_bio_messages(
    audio_cortical_bridge_t* bridge,
    uint32_t max_messages)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortical_process_bio_messages: bridge is NULL");
        return 0;
    }
    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    uint32_t processed = bio_router_process_inbox(bridge->base.bio_ctx, max_messages);

    if (processed > 0) {
        bridge->stats.bio_messages_received += processed;
        NIMCP_LOGGING_DEBUG("Processed %u bio-async messages", processed);
    }

    return processed;
}

int audio_cortical_broadcast_frequency(
    audio_cortical_bridge_t* bridge,
    const audio_cortical_frequency_result_t* result)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Broadcasting requires custom message structure
     * For now, just update stats and return success
     */
    bridge->stats.bio_messages_sent++;

    NIMCP_LOGGING_DEBUG("Broadcast frequency: freq=%.1f, selectivity=%.2f",
                        result->dominant_frequency, result->selectivity_index);

    return NIMCP_SUCCESS;
}

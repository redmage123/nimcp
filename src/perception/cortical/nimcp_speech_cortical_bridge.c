/**
 * @file nimcp_speech_cortical_bridge.c
 * @brief Speech-Cortical Bridge Implementation
 *
 * WHAT: Connects speech cortex perception with cortical column processing.
 * WHY:  Provides biologically-realistic speech processing with proper columnar organization.
 * HOW:  Routes phoneme input through tonotopic mapping to feature hypercolumns.
 *
 * @version 1.0.0
 * @date 2025-12-19
 * @author NIMCP Development Team
 */

#include "perception/cortical/nimcp_speech_cortical_bridge.h"
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
#include "api/nimcp_api_exception.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(speech_cortical_bridge)

/* ============================================================================
 * Internal Structure Definition
 * ============================================================================ */

/**
 * @brief Internal structure for speech-cortical bridge
 */
struct speech_cortical_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Connected modules */
    speech_cortex_t* speech_cortex;
    topographic_map_t* tonotopic_map;
    cortical_immune_system_t* cortical_immune;

    /* Feature hypercolumns for phoneme classes */
    feature_hypercolumn_t** hypercolumns;
    uint32_t num_hypercolumns;
    uint32_t phonemes_per_hypercolumn;

    /* Configuration */
    speech_cortical_config_t config;

    /* State */
    speech_cortical_state_t state;
    speech_cortical_stats_t stats;

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
 * @brief Compute hypercolumn grid position from tonotopic coordinates
 */
static uint32_t compute_hypercolumn_index(
    const speech_cortical_bridge_t* bridge,
    float tono_x,
    float tono_y)
{
    if (!bridge || bridge->num_hypercolumns == 0) {
        return UINT32_MAX;
    }

    /* Convert frequency to normalized position using logarithmic mapping */
    float min_freq = bridge->config.min_formant_freq;
    float max_freq = bridge->config.max_formant_freq;

    /* Handle invalid frequency range */
    if (max_freq <= min_freq || min_freq <= 0) {
        min_freq = SPEECH_CORTICAL_DEFAULT_FORMANT_MIN;
        max_freq = SPEECH_CORTICAL_DEFAULT_FORMANT_MAX;
    }

    /* Logarithmic frequency mapping (cochlear-like) */
    float log_min = logf(min_freq);
    float log_max = logf(max_freq);
    float log_x = logf(fmaxf(tono_x, min_freq));

    float norm_x = (log_x - log_min) / (log_max - log_min);
    float norm_y = (tono_y + 1.0f) / 2.0f;  /* Assume tono_y is in [-1, 1] */

    /* Clamp to [0, 1] */
    norm_x = fminf(1.0f, fmaxf(0.0f, norm_x));
    norm_y = fminf(1.0f, fmaxf(0.0f, norm_y));

    /* Compute grid dimensions (assume square grid) */
    uint32_t grid_size = (uint32_t)sqrtf((float)bridge->num_hypercolumns);
    if (grid_size == 0) grid_size = 1;

    uint32_t grid_x = (uint32_t)(norm_x * (grid_size - 1));
    uint32_t grid_y = (uint32_t)(norm_y * (grid_size - 1));

    uint32_t idx = grid_y * grid_size + grid_x;
    if (idx >= bridge->num_hypercolumns) {
        idx = bridge->num_hypercolumns - 1;
    }

    return idx;
}

/**
 * @brief Apply immune modulation to hypercolumn gain
 */
static float apply_immune_modulation(
    const speech_cortical_bridge_t* bridge,
    uint32_t hcol_idx,
    float response)
{
    if (!bridge || hcol_idx >= bridge->num_hypercolumns) {
        return response;
    }

    /* Base modulation from global immune factor */
    float modulation = 1.0f - (bridge->immune_modulation_factor *
                               bridge->config.immune_modulation_factor);

    /* Per-hypercolumn gain if available */
    if (bridge->hypercolumn_gains) {
        modulation *= bridge->hypercolumn_gains[hcol_idx];
    }

    return response * modulation;
}

/**
 * @brief Compute phoneme result from hypercolumn responses
 */
static void compute_phoneme_result(
    feature_hypercolumn_t* hcol,
    speech_cortical_phoneme_result_t* result,
    float tono_x,
    float tono_y)
{
    if (!hcol || !result) return;

    /* Get hypercolumn statistics */
    feature_hypercolumn_stats_t stats;
    feature_hypercolumn_get_stats(hcol, &stats);

    result->dominant_phoneme = (phoneme_t)stats.winner_index;
    result->selectivity_index = stats.selectivity;
    result->tono_x = tono_x;
    result->tono_y = tono_y;

    /* Allocate and copy phoneme responses */
    if (hcol->total_columns > 0) {
        result->phoneme_responses = (float*)nimcp_malloc(
            hcol->total_columns * sizeof(float)
        );
        if (result->phoneme_responses) {
            feature_hypercolumn_get_all_activations(hcol, result->phoneme_responses);
            result->num_phonemes = hcol->total_columns;
        }
    }

    /* Compute confidence from selectivity */
    result->confidence = stats.selectivity;
}

/**
 * @brief Extract formants from audio using simple peak detection
 */
static void extract_formants(
    const float* audio_data,
    uint32_t num_samples,
    float* formants)
{
    /* Initialize formants to typical values */
    formants[0] = 500.0f;   /* F1 - typically 200-900 Hz */
    formants[1] = 1500.0f;  /* F2 - typically 800-2500 Hz */
    formants[2] = 2500.0f;  /* F3 - typically 1500-3500 Hz */
    formants[3] = 3500.0f;  /* F4 - typically 3000-4500 Hz */

    if (!audio_data || num_samples == 0) {
        return;
    }

    /* Simple energy-based formant estimation */
    /* In a real implementation, this would use LPC or similar */
    float energy = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        energy += audio_data[i] * audio_data[i];
    }
    energy /= (float)num_samples;

    /* Adjust formants based on energy (simple heuristic) */
    float energy_factor = sqrtf(energy) * 2.0f;
    if (energy_factor > 0.1f) {
        formants[0] *= (1.0f + energy_factor * 0.1f);
        formants[1] *= (1.0f + energy_factor * 0.05f);
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void speech_cortical_default_config(speech_cortical_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(speech_cortical_config_t));

    config->num_hypercolumns = 64;  /* 8x8 grid */
    config->phonemes_per_hypercolumn = SPEECH_CORTICAL_DEFAULT_PHONEMES;
    config->min_formant_freq = SPEECH_CORTICAL_DEFAULT_FORMANT_MIN;
    config->max_formant_freq = SPEECH_CORTICAL_DEFAULT_FORMANT_MAX;
    config->tuning_width = SPEECH_CORTICAL_DEFAULT_TUNING_WIDTH;
    config->mode = SPEECH_CORTICAL_MODE_HYPERCOLUMN;
    config->enable_tonotopic_mapping = true;
    config->enable_cortical_immune = true;
    config->enable_bio_async = true;
    config->auditory_field_octaves = 4.0f;  /* ~4 octaves for speech */
    config->cochlear_magnification = 10.0f;
    config->immune_modulation_factor = SPEECH_CORTICAL_DEFAULT_IMMUNE_FACTOR;
    config->use_umm = false;
}

speech_cortical_bridge_t* speech_cortical_bridge_create(
    const speech_cortical_config_t* config,
    speech_cortex_t* speech_cortex)
{
    speech_cortical_config_t local_config;

    /* Use default config if none provided */
    if (!config) {
        speech_cortical_default_config(&local_config);
        config = &local_config;
    }

    /* Validate configuration */
    if (config->num_hypercolumns == 0 ||
        config->num_hypercolumns > SPEECH_CORTICAL_MAX_HYPERCOLUMNS) {
        NIMCP_LOGGING_ERROR("Invalid num_hypercolumns: %u", config->num_hypercolumns);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "speech_cortical_bridge_create: invalid num_hypercolumns");
        return NULL;
    }

    /* Allocate bridge */
    speech_cortical_bridge_t* bridge = (speech_cortical_bridge_t*)nimcp_malloc(
        sizeof(speech_cortical_bridge_t)
    );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate speech-cortical bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_cortical_bridge_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(speech_cortical_bridge_t));

    /* Copy configuration */
    memcpy(&bridge->config, config, sizeof(speech_cortical_config_t));
    bridge->speech_cortex = speech_cortex;
    bridge->state = SPEECH_CORTICAL_STATE_UNINITIALIZED;
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
        speech_cortical_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_cortical_bridge_create: bridge->hypercolumns is NULL");
        return NULL;
    }
    memset(bridge->hypercolumns, 0,
           config->num_hypercolumns * sizeof(feature_hypercolumn_t*));

    /* Create hypercolumns for phoneme features */
    bridge->num_hypercolumns = config->num_hypercolumns;
    bridge->phonemes_per_hypercolumn = config->phonemes_per_hypercolumn;

    for (uint32_t i = 0; i < config->num_hypercolumns; i++) {
        /* Create feature dimension for phoneme classes */
        feature_dimension_t dim = feature_dimension_create(
            FEATURE_CUSTOM,  /* Phoneme features are custom */
            0.0f,
            (float)config->phonemes_per_hypercolumn,
            config->phonemes_per_hypercolumn
        );
        feature_dimension_set_circular(&dim, false);
        feature_dimension_set_tuning_width(&dim, config->tuning_width);

        bridge->hypercolumns[i] = feature_hypercolumn_create(&dim, 1);
        if (!bridge->hypercolumns[i]) {
            NIMCP_LOGGING_ERROR("Failed to create hypercolumn %u", i);
            speech_cortical_bridge_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "speech_cortical_bridge_create: bridge->hypercolumns is NULL");
            return NULL;
        }

        /* Set spatial position based on grid */
        uint32_t grid_size = (uint32_t)sqrtf((float)config->num_hypercolumns);
        if (grid_size == 0) grid_size = 1;
        float pos_x = (float)(i % grid_size) / (float)grid_size;
        float pos_y = (float)(i / grid_size) / (float)grid_size;
        bridge->hypercolumns[i]->position[0] = pos_x;
        bridge->hypercolumns[i]->position[1] = pos_y;
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
            .min_frequency = config->min_formant_freq,
            .max_frequency = config->max_formant_freq,
            .octave_span = config->auditory_field_octaves,
            .is_logarithmic = true,  /* Logarithmic frequency mapping */
            .q_factor = 10.0f  /* Default Q factor for speech */
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
    memset(&bridge->stats, 0, sizeof(speech_cortical_stats_t));

    bridge->state = SPEECH_CORTICAL_STATE_READY;

    NIMCP_LOGGING_INFO("Speech-cortical bridge created with %u hypercolumns (%u phonemes each)",
                       bridge->num_hypercolumns, bridge->phonemes_per_hypercolumn);

    return bridge;
}

void speech_cortical_bridge_destroy(speech_cortical_bridge_t* bridge)
{
    if (!bridge) return;

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        speech_cortical_disconnect_bio_async(bridge);
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

    NIMCP_LOGGING_DEBUG("Speech-cortical bridge destroyed");
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int speech_cortical_connect_speech_cortex(
    speech_cortical_bridge_t* bridge,
    speech_cortex_t* speech_cortex)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->speech_cortex = speech_cortex;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Connected to speech cortex");
    return NIMCP_SUCCESS;
}

int speech_cortical_connect_immune(
    speech_cortical_bridge_t* bridge,
    cortical_immune_system_t* immune)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->cortical_immune = immune;

    /* Register hypercolumns with immune system */
    /* Note: Feature hypercolumns don't have a direct registration function */
    /* The cortical immune system will be used for inflammation modulation */
    (void)immune; /* Suppress unused warning when registration is skipped */

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Connected to cortical immune system");
    return NIMCP_SUCCESS;
}

int speech_cortical_connect_bio_async(speech_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_SPEECH_CORTICAL,
        .module_name = "speech_cortical_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Speech-cortical bridge connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("Failed to connect to bio-async router");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int speech_cortical_disconnect_bio_async(speech_cortical_bridge_t* bridge)
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

bool speech_cortical_is_bio_async_connected(const speech_cortical_bridge_t* bridge)
{
    return bridge && bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Processing Functions
 * ============================================================================ */

int speech_cortical_process(
    speech_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    speech_cortical_phoneme_result_t* result)
{
    NIMCP_CHECK_THROW(bridge && audio_data && result, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in speech_cortical_process");
    NIMCP_CHECK_THROW(num_samples > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid num_samples in speech_cortical_process");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    bridge->state = SPEECH_CORTICAL_STATE_PROCESSING;
    uint64_t start_time = get_time_ns();

    memset(result, 0, sizeof(speech_cortical_phoneme_result_t));

    /* Extract formants from audio */
    extract_formants(audio_data, num_samples, result->formants);

    /* Convert audio energy to phoneme features */
    /* Simple energy-based feature extraction per hypercolumn */
    float* phoneme_features = (float*)nimcp_calloc(
        bridge->phonemes_per_hypercolumn, sizeof(float)
    );
    if (!phoneme_features) {
        bridge->state = SPEECH_CORTICAL_STATE_ERROR;
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Compute basic audio features */
    float total_energy = 0.0f;
    float zero_crossings = 0.0f;

    for (uint32_t i = 0; i < num_samples; i++) {
        total_energy += audio_data[i] * audio_data[i];
        if (i > 0 && ((audio_data[i] >= 0) != (audio_data[i-1] >= 0))) {
            zero_crossings += 1.0f;
        }
    }
    total_energy = sqrtf(total_energy / (float)num_samples);
    zero_crossings /= (float)num_samples;

    /* Map features to phoneme classes based on energy and ZCR patterns */
    /* This is a simplified model - real phoneme detection would use spectral analysis */
    for (uint32_t p = 0; p < bridge->phonemes_per_hypercolumn; p++) {
        /* Different phoneme classes respond to different feature combinations */
        float phase = (float)p / (float)bridge->phonemes_per_hypercolumn;
        phoneme_features[p] = total_energy * cosf(phase * M_PI * 2.0f + zero_crossings * 10.0f);
        phoneme_features[p] = fmaxf(0.0f, phoneme_features[p]);
    }

    /* Process through hypercolumns */
    float* avg_responses = (float*)nimcp_calloc(
        bridge->phonemes_per_hypercolumn, sizeof(float)
    );
    uint32_t processed_count = 0;

    if (!avg_responses) {
        nimcp_free(phoneme_features);
        bridge->state = SPEECH_CORTICAL_STATE_ERROR;
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Process a subset of hypercolumns (center and corners) */
    uint32_t grid_size = (uint32_t)sqrtf((float)bridge->num_hypercolumns);
    if (grid_size == 0) grid_size = 1;

    /* Sample hypercolumn positions */
    uint32_t center = bridge->num_hypercolumns / 2;
    uint32_t sample_indices[] = {
        0, grid_size - 1, center,
        bridge->num_hypercolumns - grid_size, bridge->num_hypercolumns - 1
    };
    uint32_t num_sample_indices = sizeof(sample_indices) / sizeof(sample_indices[0]);

    for (uint32_t s = 0; s < num_sample_indices && s < bridge->num_hypercolumns; s++) {
        uint32_t idx = sample_indices[s];
        if (idx >= bridge->num_hypercolumns) continue;

        feature_hypercolumn_t* hcol = bridge->hypercolumns[idx];
        if (!hcol) continue;

        /* Process phoneme features through hypercolumn */
        feature_hypercolumn_process(hcol, phoneme_features, bridge->phonemes_per_hypercolumn);
        feature_hypercolumn_normalize(hcol);

        /* Accumulate responses */
        float* hcol_activations = (float*)nimcp_malloc(
            hcol->total_columns * sizeof(float)
        );
        if (hcol_activations) {
            feature_hypercolumn_get_all_activations(hcol, hcol_activations);

            for (uint32_t f = 0; f < hcol->total_columns &&
                 f < bridge->phonemes_per_hypercolumn; f++) {
                float resp = hcol_activations[f];
                resp = apply_immune_modulation(bridge, idx, resp);
                avg_responses[f] += resp;
            }
            processed_count++;
            nimcp_free(hcol_activations);
        }

        bridge->stats.hypercolumn_activations++;
    }

    nimcp_free(phoneme_features);

    /* Compute average and find dominant */
    if (processed_count > 0) {
        float max_response = 0.0f;
        uint32_t max_idx = 0;
        float sum_responses = 0.0f;

        for (uint32_t i = 0; i < bridge->phonemes_per_hypercolumn; i++) {
            avg_responses[i] /= (float)processed_count;
            sum_responses += avg_responses[i];
            if (avg_responses[i] > max_response) {
                max_response = avg_responses[i];
                max_idx = i;
            }
        }

        result->dominant_phoneme = (phoneme_t)max_idx;
        result->num_phonemes = bridge->phonemes_per_hypercolumn;
        result->phoneme_responses = avg_responses;  /* Transfer ownership */

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
    bridge->stats.peak_phoneme_response = result->selectivity_index;
    bridge->stats.current_dominant_phoneme = result->dominant_phoneme;
    bridge->stats.current_immune_modulation = bridge->immune_modulation_factor;

    uint64_t end_time = get_time_ns();
    float process_time_ms = (float)(end_time - start_time) / 1000000.0f;
    float updated = (bridge->stats.avg_processing_time_ms * (bridge->stats.frames_processed - 1) +
         process_time_ms) / (float)bridge->stats.frames_processed;
    if (isfinite(updated)) bridge->stats.avg_processing_time_ms = updated;

    bridge->last_process_time_ns = end_time;
    bridge->state = SPEECH_CORTICAL_STATE_READY;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int speech_cortical_process_segment(
    speech_cortical_bridge_t* bridge,
    const float* audio_segment,
    uint32_t num_samples,
    float tono_x,
    float tono_y,
    speech_cortical_phoneme_result_t* result)
{
    NIMCP_CHECK_THROW(bridge && audio_segment && result, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in speech_cortical_process_segment");
    NIMCP_CHECK_THROW(num_samples > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid num_samples in speech_cortical_process_segment");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    memset(result, 0, sizeof(speech_cortical_phoneme_result_t));

    /* Find hypercolumn for this position */
    uint32_t hcol_idx = compute_hypercolumn_index(bridge, tono_x, tono_y);
    if (hcol_idx >= bridge->num_hypercolumns) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    feature_hypercolumn_t* hcol = bridge->hypercolumns[hcol_idx];
    if (!hcol) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Extract formants from audio segment */
    extract_formants(audio_segment, num_samples, result->formants);

    /* Convert audio to phoneme features */
    float* phoneme_features = (float*)nimcp_calloc(
        bridge->phonemes_per_hypercolumn, sizeof(float)
    );
    if (!phoneme_features) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Compute audio energy */
    float energy = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        energy += audio_segment[i] * audio_segment[i];
    }
    energy = sqrtf(energy / (float)num_samples);

    /* Map to phoneme features */
    for (uint32_t p = 0; p < bridge->phonemes_per_hypercolumn; p++) {
        float phase = (float)p / (float)bridge->phonemes_per_hypercolumn;
        phoneme_features[p] = energy * cosf(phase * M_PI * 2.0f);
        phoneme_features[p] = fmaxf(0.0f, phoneme_features[p]);
    }

    /* Process through hypercolumn */
    feature_hypercolumn_process(hcol, phoneme_features, bridge->phonemes_per_hypercolumn);
    feature_hypercolumn_normalize(hcol);
    bridge->stats.hypercolumn_activations++;

    nimcp_free(phoneme_features);

    /* Compute result */
    compute_phoneme_result(hcol, result, tono_x, tono_y);

    /* Apply immune modulation */
    if (result->phoneme_responses) {
        for (uint32_t i = 0; i < result->num_phonemes; i++) {
            result->phoneme_responses[i] = apply_immune_modulation(
                bridge, hcol_idx, result->phoneme_responses[i]
            );
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

void speech_cortical_free_result(speech_cortical_phoneme_result_t* result)
{
    if (!result) return;

    if (result->phoneme_responses) {
        nimcp_free(result->phoneme_responses);
        result->phoneme_responses = NULL;
    }
    result->num_phonemes = 0;
}

int speech_cortical_get_phoneme_map(
    speech_cortical_bridge_t* bridge,
    const float* audio_data,
    uint32_t num_samples,
    phoneme_t* phoneme_map,
    float* selectivity_map)
{
    NIMCP_CHECK_THROW(bridge && audio_data && phoneme_map, NIMCP_ERROR_NULL_POINTER,
        "NULL parameter in speech_cortical_get_phoneme_map");
    NIMCP_CHECK_THROW(num_samples > 0, NIMCP_ERROR_INVALID_PARAM,
        "Invalid num_samples in speech_cortical_get_phoneme_map");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    uint32_t grid_size = (uint32_t)sqrtf((float)bridge->num_hypercolumns);
    if (grid_size == 0) grid_size = 1;

    /* Convert audio to phoneme features once */
    float* phoneme_features = (float*)nimcp_calloc(
        bridge->phonemes_per_hypercolumn, sizeof(float)
    );
    if (!phoneme_features) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Compute audio features */
    float energy = 0.0f;
    float zero_crossings = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        energy += audio_data[i] * audio_data[i];
        if (i > 0 && ((audio_data[i] >= 0) != (audio_data[i-1] >= 0))) {
            zero_crossings += 1.0f;
        }
    }
    energy = sqrtf(energy / (float)num_samples);
    zero_crossings /= (float)num_samples;

    /* Map to phoneme features */
    for (uint32_t p = 0; p < bridge->phonemes_per_hypercolumn; p++) {
        float phase = (float)p / (float)bridge->phonemes_per_hypercolumn;
        phoneme_features[p] = energy * cosf(phase * M_PI * 2.0f + zero_crossings * 10.0f);
        phoneme_features[p] = fmaxf(0.0f, phoneme_features[p]);
    }

    /* Process each hypercolumn and fill phoneme map */
    for (uint32_t hy = 0; hy < grid_size; hy++) {
        for (uint32_t hx = 0; hx < grid_size; hx++) {
            uint32_t hcol_idx = hy * grid_size + hx;
            if (hcol_idx >= bridge->num_hypercolumns) continue;

            feature_hypercolumn_t* hcol = bridge->hypercolumns[hcol_idx];
            if (!hcol) continue;

            /* Process */
            feature_hypercolumn_process(hcol, phoneme_features, bridge->phonemes_per_hypercolumn);
            feature_hypercolumn_normalize(hcol);

            feature_hypercolumn_stats_t stats;
            feature_hypercolumn_get_stats(hcol, &stats);

            phoneme_map[hcol_idx] = (phoneme_t)stats.winner_index;
            if (selectivity_map) {
                selectivity_map[hcol_idx] = stats.selectivity;
            }
        }
    }

    nimcp_free(phoneme_features);

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Hypercolumn Functions
 * ============================================================================ */

const feature_hypercolumn_t* speech_cortical_get_hypercolumn(
    const speech_cortical_bridge_t* bridge,
    float tono_x,
    float tono_y)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    speech_cortical_bridge_t* mutable_bridge = (speech_cortical_bridge_t*)bridge;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);

    uint32_t idx = compute_hypercolumn_index(bridge, tono_x, tono_y);
    if (idx >= bridge->num_hypercolumns) {
        if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "speech_cortical_get_hypercolumn: index out of range");
        return NULL;
    }

    const feature_hypercolumn_t* hcol = bridge->hypercolumns[idx];
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return hcol;
}

const feature_hypercolumn_t* speech_cortical_get_hypercolumn_by_index(
    const speech_cortical_bridge_t* bridge,
    uint32_t index)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "speech_cortical_get_hypercolumn_by_index: bridge is NULL");
        return NULL;
    }

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    speech_cortical_bridge_t* mutable_bridge = (speech_cortical_bridge_t*)bridge;

    const feature_hypercolumn_t* hcol = NULL;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    if (index < bridge->num_hypercolumns) {
        hcol = bridge->hypercolumns[index];
    }
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return hcol;
}

uint32_t speech_cortical_get_num_hypercolumns(const speech_cortical_bridge_t* bridge)
{
    if (!bridge) return 0;

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    speech_cortical_bridge_t* mutable_bridge = (speech_cortical_bridge_t*)bridge;

    uint32_t count;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    count = bridge->num_hypercolumns;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return count;
}

/* ============================================================================
 * Immune Modulation Functions
 * ============================================================================ */

int speech_cortical_update_immune_modulation(speech_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (!bridge->cortical_immune) {
        return NIMCP_SUCCESS;  /* No immune system connected */
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);

    /* Get cortical immune statistics */
    cortical_immune_stats_t stats;
    if (cortical_immune_get_stats(bridge->cortical_immune, &stats) == 0) {
        bridge->immune_modulation_factor = stats.mean_inflammation_level;
        bridge->stats.current_immune_modulation = bridge->immune_modulation_factor;

        /* Update per-hypercolumn gains based on local inflammation */
        for (uint32_t i = 0; i < bridge->num_hypercolumns; i++) {
            cortical_column_immune_t col_status;
            if (cortical_immune_get_column_status(bridge->cortical_immune,
                                                  i, &col_status) == 0) {
                if (bridge->hypercolumn_gains) {
                    bridge->hypercolumn_gains[i] = col_status.gain_modulation;
                }
            }
        }
    }

    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

int speech_cortical_set_immune_factor(
    speech_cortical_bridge_t* bridge,
    float factor)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune_modulation_factor = factor;
    bridge->stats.current_immune_modulation = factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

float speech_cortical_get_immune_factor(const speech_cortical_bridge_t* bridge)
{
    if (!bridge) return 0.0f;

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    speech_cortical_bridge_t* mutable_bridge = (speech_cortical_bridge_t*)bridge;

    float factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    factor = bridge->immune_modulation_factor;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return factor;
}

/* ============================================================================
 * Statistics and State Functions
 * ============================================================================ */

int speech_cortical_get_stats(
    const speech_cortical_bridge_t* bridge,
    speech_cortical_stats_t* stats)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_lock(bridge->base.mutex);
    }
    memcpy(stats, &bridge->stats, sizeof(speech_cortical_stats_t));
    if ((bridge->base.mutex != NULL)) {
        nimcp_mutex_unlock(bridge->base.mutex);
    }

    return NIMCP_SUCCESS;
}

int speech_cortical_reset_stats(speech_cortical_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(speech_cortical_stats_t));
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

speech_cortical_state_t speech_cortical_get_state(
    const speech_cortical_bridge_t* bridge)
{
    if (!bridge) return SPEECH_CORTICAL_STATE_UNINITIALIZED;

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    speech_cortical_bridge_t* mutable_bridge = (speech_cortical_bridge_t*)bridge;

    speech_cortical_state_t state;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    state = bridge->state;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return state;
}

const topographic_map_t* speech_cortical_get_tonotopic_map(
    const speech_cortical_bridge_t* bridge)
{
    if (!bridge) return NULL;

    /* Cast away const for mutex operations - mutex is logically const but physically modified */
    speech_cortical_bridge_t* mutable_bridge = (speech_cortical_bridge_t*)bridge;

    const topographic_map_t* map;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_lock(mutable_bridge->base.mutex);
    map = bridge->tonotopic_map;
    if ((bridge->base.mutex != NULL)) nimcp_mutex_unlock(mutable_bridge->base.mutex);

    return map;
}

/* ============================================================================
 * Bio-Async Message Handling
 * ============================================================================ */

uint32_t speech_cortical_process_bio_messages(
    speech_cortical_bridge_t* bridge,
    uint32_t max_messages)
{
    if (!bridge || !bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    /* Process messages through bio router */
    uint32_t processed = bio_router_process_inbox(bridge->base.bio_ctx, max_messages);

    if (processed > 0) {
        bridge->stats.bio_messages_received += processed;
        NIMCP_LOGGING_DEBUG("Processed %u bio-async messages", processed);
    }

    return processed;
}

int speech_cortical_broadcast_phoneme(
    speech_cortical_bridge_t* bridge,
    const speech_cortical_phoneme_result_t* result)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_NULL_POINTER, "result is NULL");

    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Broadcasting requires custom message structure definition
     * For now, just update stats and return success
     * Full implementation would use bio_router_broadcast with custom message type
     */
    bridge->stats.bio_messages_sent++;

    NIMCP_LOGGING_DEBUG("Broadcast phoneme: phoneme=%u, selectivity=%.2f",
                        (uint32_t)result->dominant_phoneme, result->selectivity_index);

    return NIMCP_SUCCESS;
}

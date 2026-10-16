#include <stddef.h>  /* for NULL */
//=============================================================================
// brain_integration.c - Brain Integration Implementation
//=============================================================================

#include "middleware/brain_integration.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>


#define LOG_MODULE "brain_integration"
#define LOG_MODULE_ID 0x0510
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_integration)

//=============================================================================
// BUFFER SIZE MAPPING
//=============================================================================

static void get_buffer_sizes(
    brain_buffer_size_t preset,
    size_t* fast_size,
    size_t* medium_size,
    size_t* slow_size,
    size_t* window_size
) {
    switch (preset) {
        case BUFFER_SIZE_10MS:
            *window_size = 100;
            *fast_size = 100;
            *medium_size = 50;
            *slow_size = 25;
            break;

        case BUFFER_SIZE_100MS:
            *window_size = 1000;
            *fast_size = 1000;
            *medium_size = 500;
            *slow_size = 250;
            break;

        case BUFFER_SIZE_1S:
            *window_size = 10000;
            *fast_size = 10000;
            *medium_size = 5000;
            *slow_size = 2500;
            break;

        default:  // BUFFER_SIZE_CUSTOM
            *window_size = 500;
            *fast_size = 500;
            *medium_size = 250;
            *slow_size = 125;
            break;
    }
}

//=============================================================================
// TEMPORAL BUFFERING
//=============================================================================

brain_temporal_buffer_t* brain_create_temporal_buffer(
    size_t num_channels,
    brain_buffer_size_t size_preset
) {
    // Guard: validate inputs
    if (num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_temporal_buffer: num_channels is zero");
        return NULL;
    }

    // Allocate structure
    brain_temporal_buffer_t* buffer = (brain_temporal_buffer_t*)nimcp_calloc(1, sizeof(brain_temporal_buffer_t));
    if (!buffer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer is NULL");

        return NULL;

    }

    // Get buffer sizes
    size_t window_size, fast_size, medium_size, slow_size;
    get_buffer_sizes(size_preset, &fast_size, &medium_size, &slow_size, &window_size);

    // Create components
    buffer->window = sliding_window_create(window_size, 0);
    buffer->multiscale = integration_buffer_create(
        fast_size, medium_size, slow_size, num_channels
    );
    buffer->accumulator = temporal_accumulator_create(
        num_channels, 0.1F, INTEGRATION_LEAKY
    );

    // Check allocation
    if (!buffer->window || !buffer->multiscale || !buffer->accumulator) {
        brain_destroy_temporal_buffer(buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_temporal_buffer: required parameter is NULL (buffer->window, buffer->multiscale, buffer->accumulator)");
        return NULL;
    }

    buffer->num_channels = num_channels;
    buffer->size_preset = size_preset;

    return buffer;
}

void brain_destroy_temporal_buffer(brain_temporal_buffer_t* buffer) {
    if (!buffer) return;

    sliding_window_destroy(buffer->window);
    integration_buffer_destroy(buffer->multiscale);
    temporal_accumulator_destroy(buffer->accumulator);
    nimcp_free(buffer);
}

bool brain_buffer_activity(
    brain_temporal_buffer_t* buffer,
    const float* activity,
    size_t num_channels,
    uint64_t timestamp
) {
    // Guard: validate inputs
    if (!buffer || !activity || num_channels != buffer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_buffer_activity: required parameter is NULL (buffer, activity)");
        return false;
    }

    // Add to each buffer component
    for (size_t ch = 0; ch < num_channels; ch++) {
        // Add to sliding window
        sliding_window_add(buffer->window, activity[ch]);

        // Add to multi-scale buffer
        integration_buffer_add(buffer->multiscale, ch, activity[ch], timestamp);

        // Update accumulator
        temporal_accumulator_update(buffer->accumulator, ch, activity[ch], 0.01F);
    }

    return true;
}

size_t brain_extract_windowed_features(
    const brain_temporal_buffer_t* buffer,
    float* features,
    size_t max_features
) {
    // Guard: validate inputs
    if (!buffer || !features || max_features == 0) return 0;

    size_t feature_idx = 0;

    // Extract features per channel (limit to available space)
    size_t channels_to_process = buffer->num_channels;
    if (channels_to_process * 5 > max_features) {
        channels_to_process = max_features / 5;
    }

    for (size_t ch = 0; ch < channels_to_process && feature_idx < max_features; ch++) {
        // Feature 1: Fast timescale mean
        if (feature_idx < max_features) {
            features[feature_idx++] = integration_buffer_mean(
                buffer->multiscale, TIMESCALE_FAST, ch
            );
        }

        // Feature 2: Medium timescale mean
        if (feature_idx < max_features) {
            features[feature_idx++] = integration_buffer_mean(
                buffer->multiscale, TIMESCALE_MEDIUM, ch
            );
        }

        // Feature 3: Slow timescale mean
        if (feature_idx < max_features) {
            features[feature_idx++] = integration_buffer_mean(
                buffer->multiscale, TIMESCALE_SLOW, ch
            );
        }

        // Feature 4: Temporal trend
        if (feature_idx < max_features) {
            features[feature_idx++] = integration_buffer_trend(
                buffer->multiscale, ch
            );
        }

        // Feature 5: Accumulated value
        if (feature_idx < max_features) {
            features[feature_idx++] = temporal_accumulator_get_value(
                buffer->accumulator, ch
            );
        }
    }

    return feature_idx;
}

//=============================================================================
// FEATURE NORMALIZATION
//=============================================================================

brain_feature_normalizer_t* brain_create_feature_normalizer(
    size_t num_features,
    brain_normalize_type_t type
) {
    // Guard: validate inputs
    if (num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_feature_normalizer: num_features is zero");
        return NULL;
    }

    // Allocate structure
    brain_feature_normalizer_t* normalizer = (brain_feature_normalizer_t*)nimcp_calloc(1, sizeof(brain_feature_normalizer_t));
    if (!normalizer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "normalizer is NULL");

        return NULL;

    }

    // Create normalizers based on type
    switch (type) {
        case NORMALIZE_ZSCORE:
            normalizer->zscore = zscore_normalizer_create(num_features, 0, 3.0F);
            if (!normalizer->zscore) {
                nimcp_free(normalizer);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_feature_normalizer: normalizer->zscore is NULL");
                return NULL;
            }
            break;

        case NORMALIZE_MINMAX:
            normalizer->minmax = minmax_normalizer_create(num_features, 0.0F, 1.0F, false);
            if (!normalizer->minmax) {
                nimcp_free(normalizer);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_feature_normalizer: normalizer->minmax is NULL");
                return NULL;
            }
            break;

        case NORMALIZE_ADAPTIVE:
            normalizer->adaptive = adaptive_normalizer_create(num_features, 0.01F, 0.001F);
            if (!normalizer->adaptive) {
                nimcp_free(normalizer);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_feature_normalizer: normalizer->adaptive is NULL");
                return NULL;
            }
            break;

        case NORMALIZE_HOMEOSTATIC:
            normalizer->homeo = homeostatic_normalizer_create(num_features, 0.5F, 10.0F);
            if (!normalizer->homeo) {
                nimcp_free(normalizer);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_feature_normalizer: normalizer->homeo is NULL");
                return NULL;
            }
            break;

        case NORMALIZE_NONE:
            // No normalization - just allocate structure
            break;

        default:
            nimcp_free(normalizer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_feature_normalizer: operation failed");
            return NULL;
    }

    normalizer->type = type;
    normalizer->num_features = num_features;

    return normalizer;
}

void brain_destroy_feature_normalizer(brain_feature_normalizer_t* normalizer) {
    if (!normalizer) return;

    zscore_normalizer_destroy(normalizer->zscore);
    minmax_normalizer_destroy(normalizer->minmax);
    adaptive_normalizer_destroy(normalizer->adaptive);
    homeostatic_normalizer_destroy(normalizer->homeo);
    nimcp_free(normalizer);
}

bool brain_normalize_features(
    brain_feature_normalizer_t* normalizer,
    float* features,
    size_t num_features
) {
    // Guard: validate inputs
    if (!normalizer || !features || num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_normalize_features: required parameter is NULL (normalizer, features)");
        return false;
    }
    if (num_features > normalizer->num_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_normalize_features: validation failed");
        return false;
    }

    // Apply normalization based on type
    switch (normalizer->type) {
        case NORMALIZE_ZSCORE:
            for (size_t i = 0; i < num_features; i++) {
                features[i] = zscore_normalizer_fit_transform(
                    normalizer->zscore, i, features[i]
                );
            }
            break;

        case NORMALIZE_MINMAX:
            for (size_t i = 0; i < num_features; i++) {
                features[i] = minmax_normalizer_fit_transform(
                    normalizer->minmax, i, features[i]
                );
            }
            break;

        case NORMALIZE_ADAPTIVE:
            for (size_t i = 0; i < num_features; i++) {
                features[i] = adaptive_normalizer_fit_transform(
                    normalizer->adaptive, i, features[i]
                );
            }
            break;

        case NORMALIZE_HOMEOSTATIC:
            for (size_t i = 0; i < num_features; i++) {
                homeostatic_normalizer_update(
                    normalizer->homeo, i, features[i], 0.1F
                );
                features[i] = homeostatic_normalizer_apply(
                    normalizer->homeo, i, features[i]
                );
            }
            break;

        case NORMALIZE_NONE:
            // No normalization
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_normalize_features: operation failed");
            return false;
    }

    return true;
}

//=============================================================================
// COMBINED OPERATIONS
//=============================================================================

size_t brain_extract_and_normalize(
    const brain_temporal_buffer_t* buffer,
    brain_feature_normalizer_t* normalizer,
    float* features,
    size_t max_features
) {
    // Guard: validate inputs
    if (!buffer || !normalizer || !features) return 0;

    // Extract features
    size_t num_extracted = brain_extract_windowed_features(
        buffer, features, max_features
    );

    if (num_extracted == 0) return 0;

    // Normalize features
    if (!brain_normalize_features(normalizer, features, num_extracted)) {
        return 0;
    }

    return num_extracted;
}

//=============================================================================
// PHASE 2: POPULATION CODING & FEATURE EXTRACTION
//=============================================================================

#include "middleware/features/nimcp_feature_extractor.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"


/**
 * @brief Internal structure for spike feature extractor
 */
struct brain_spike_feature_extractor_struct {
    feature_extractor_t extractor;
    uint32_t max_neurons;
};

/**
 * @brief Internal structure for population analyzer
 */
struct brain_population_analyzer_struct {
    population_coding_encoder_t encoder;
};

//-----------------------------------------------------------------------------
// Spike Feature Extraction
//-----------------------------------------------------------------------------

brain_spike_feature_extractor_t brain_create_spike_feature_extractor(
    uint32_t max_neurons,
    bool compute_oscillations,
    bool compute_synchrony
) {
    // Guard clauses
    if (max_neurons == 0 || max_neurons > FEATURE_EXTRACTOR_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_spike_feature_extractor: max_neurons is zero");
        return NULL;
    }

    // Allocate structure
    struct brain_spike_feature_extractor_struct* extractor =
        nimcp_calloc(1, sizeof(struct brain_spike_feature_extractor_struct));
    if (!extractor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extractor is NULL");

        return NULL;

    }

    // Configure feature extractor for brain use
    feature_extractor_config_t config = {
        .window_ms = 100.0F,
        .synchrony_window_ms = 5.0F,
        .burst_isi_threshold_ms = 10.0F,
        .min_burst_spikes = 3,
        .entropy_bins = 20,
        .compute_oscillations = compute_oscillations,
        .compute_entropy = true,
        .compute_synchrony = compute_synchrony
    };

    extractor->extractor = feature_extractor_create(&config);
    if (!extractor->extractor) {
        nimcp_free(extractor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_spike_feature_extractor: extractor->extractor is NULL");
        return NULL;
    }

    extractor->max_neurons = max_neurons;
    return extractor;
}

void brain_destroy_spike_feature_extractor(brain_spike_feature_extractor_t extractor) {
    if (!extractor) return;
    feature_extractor_destroy(extractor->extractor);
    nimcp_free(extractor);
}

bool brain_extract_spike_features(
    brain_spike_feature_extractor_t extractor,
    const spike_data_t* spike_data,
    middleware_features_t* features_out
) {
    // Guard clauses
    if (!extractor || !spike_data || !features_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_extract_spike_features: required parameter is NULL (extractor, spike_data, features_out)");
        return false;
    }
    if (spike_data->num_neurons > extractor->max_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_extract_spike_features: validation failed");
        return false;
    }

    // Extract features using middleware
    return feature_extractor_update(extractor->extractor, spike_data, features_out);
}

//-----------------------------------------------------------------------------
// Population Coding Analysis
//-----------------------------------------------------------------------------

brain_population_analyzer_t brain_create_population_analyzer(void) {
    // Allocate structure
    struct brain_population_analyzer_struct* analyzer =
        nimcp_calloc(1, sizeof(struct brain_population_analyzer_struct));
    if (!analyzer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analyzer is NULL");

        return NULL;

    }

    // Configure for brain use with sensible defaults
    population_coding_config_t config = {
        .n_pca_components = 3,
        .correlation_window_ms = 5.0F,
        .synchrony_threshold = 0.5F,
        .sparsity_target = 0.1F,
        .enable_pca = true,
        .enable_synchrony = true
    };

    analyzer->encoder = population_coding_create(&config);
    if (!analyzer->encoder) {
        nimcp_free(analyzer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_create_population_analyzer: analyzer->encoder is NULL");
        return NULL;
    }

    return analyzer;
}

void brain_destroy_population_analyzer(brain_population_analyzer_t analyzer) {
    if (!analyzer) return;
    population_coding_destroy(analyzer->encoder);
    nimcp_free(analyzer);
}

bool brain_compute_population_vector(
    brain_population_analyzer_t analyzer,
    const float* rates,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    vector3d_t* vector_out
) {
    // Guard clauses
    if (!analyzer || !rates || !tuning_curves || !vector_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_compute_population_vector: required parameter is NULL (analyzer, rates, tuning_curves, vector_out)");
        return false;
    }
    if (num_neurons == 0 || num_neurons > POPULATION_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_compute_population_vector: num_neurons is zero");
        return false;
    }

    // Use population vector encoding
    return population_coding_encode_vector_sum(
        analyzer->encoder,
        rates,
        tuning_curves,
        num_neurons,
        vector_out
    );
}

bool brain_compute_population_synchrony(
    brain_population_analyzer_t analyzer,
    spike_train_t* const * spike_trains,
    uint32_t num_neurons,
    synchrony_result_t* synchrony_out
) {
    // Guard clauses
    if (!analyzer || !spike_trains || !synchrony_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_compute_population_synchrony: required parameter is NULL (analyzer, spike_trains, synchrony_out)");
        return false;
    }
    if (num_neurons < 2 || num_neurons > POPULATION_MAX_NEURONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_compute_population_synchrony: validation failed");
        return false;
    }

    // Compute synchrony using population coding
    return population_coding_compute_synchrony(
        analyzer->encoder,
        spike_trains,
        num_neurons,
        synchrony_out
    );
}

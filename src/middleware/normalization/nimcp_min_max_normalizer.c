#include "middleware/normalization/nimcp_min_max_normalizer.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <float.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"



#define LOG_MODULE "nimcp_min_max_normalizer"
#define LOG_MODULE_ID 0x0522
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(min_max_normalizer)

typedef struct {
    float min_value;
    float max_value;
    float range;
    size_t count;
} channel_stats_t;

struct min_max_normalizer {
    size_t num_channels;
    float target_min;
    float target_max;
    bool use_percentiles;
    channel_stats_t* channels;
    nimcp_mutex_t* mutex;
};

min_max_normalizer_t* minmax_normalizer_create(
    size_t num_channels,
    float target_min,
    float target_max,
    bool use_percentiles
) {
    if (num_channels == 0 || target_min >= target_max) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "minmax_normalizer_create: invalid num_channels or target range");
        return NULL;
    }

    min_max_normalizer_t* norm = nimcp_calloc(1, sizeof(min_max_normalizer_t));
    if (!norm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "minmax_normalizer_create: failed to allocate normalizer");
        return NULL;
    }

    norm->num_channels = num_channels;
    norm->target_min = target_min;
    norm->target_max = target_max;
    norm->use_percentiles = use_percentiles;

    norm->channels = nimcp_calloc(num_channels, sizeof(channel_stats_t));
    if (!norm->channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "minmax_normalizer_create: failed to allocate channels");
        nimcp_free(norm);
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].min_value = FLT_MAX;
        norm->channels[i].max_value = -FLT_MAX;
        norm->channels[i].range = 1.0F;
        norm->channels[i].count = 0;
    }

    norm->mutex = nimcp_mutex_create(NULL);
    if (!norm->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "minmax_normalizer_create: failed to create mutex");
        nimcp_free(norm->channels);
        nimcp_free(norm);
        return NULL;
    }

    return norm;
}

void minmax_normalizer_destroy(min_max_normalizer_t* normalizer) {
    if (!normalizer) return;
    if (normalizer->mutex) nimcp_mutex_free(normalizer->mutex);
    nimcp_free(normalizer->channels);
    nimcp_free(normalizer);
}

bool minmax_normalizer_fit(
    min_max_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "minmax_normalizer_fit: normalizer is NULL");
        return false;
    }

    nimcp_mutex_lock(normalizer->mutex);

    channel_stats_t* stats = &normalizer->channels[channel];

    if (value < stats->min_value) stats->min_value = value;
    if (value > stats->max_value) stats->max_value = value;

    stats->range = stats->max_value - stats->min_value;
    if (stats->range < 1e-6F) stats->range = 1.0F;

    stats->count++;

    nimcp_mutex_unlock(normalizer->mutex);
    return true;
}

float minmax_normalizer_transform(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;

    const channel_stats_t* stats = &normalizer->channels[channel];
    if (stats->count == 0) return normalizer->target_min;

    float normalized = (value - stats->min_value) / stats->range;
    return normalized * (normalizer->target_max - normalizer->target_min) + normalizer->target_min;
}

float minmax_normalizer_fit_transform(
    min_max_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    minmax_normalizer_fit(normalizer, channel, value);
    return minmax_normalizer_transform(normalizer, channel, value);
}

float minmax_normalizer_inverse_transform(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    float normalized_value
) {
    if (!normalizer || channel >= normalizer->num_channels) return normalized_value;

    const channel_stats_t* stats = &normalizer->channels[channel];
    float unit_value = (normalized_value - normalizer->target_min) /
                       (normalizer->target_max - normalizer->target_min);
    return unit_value * stats->range + stats->min_value;
}

bool minmax_normalizer_get_stats(
    const min_max_normalizer_t* normalizer,
    size_t channel,
    minmax_stats_t* stats
) {
    if (!normalizer || channel >= normalizer->num_channels || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "minmax_normalizer_get_stats: required parameter is NULL (normalizer, stats)");
        return false;
    }

    const channel_stats_t* ch = &normalizer->channels[channel];
    stats->min_value = (ch->min_value == FLT_MAX) ? 0.0F : ch->min_value;
    stats->max_value = (ch->max_value == -FLT_MAX) ? 1.0F : ch->max_value;
    stats->range = ch->range;
    stats->sample_count = ch->count;

    return true;
}

bool minmax_normalizer_reset_channel(
    min_max_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "minmax_normalizer_reset_channel: normalizer is NULL");
        return false;
    }

    nimcp_mutex_lock(normalizer->mutex);

    channel_stats_t* stats = &normalizer->channels[channel];
    stats->min_value = FLT_MAX;
    stats->max_value = -FLT_MAX;
    stats->range = 1.0F;
    stats->count = 0;

    nimcp_mutex_unlock(normalizer->mutex);

    return true;
}

static void minmax_normalizer_reset_channel_unlocked(
    min_max_normalizer_t* normalizer,
    size_t channel
) {
    channel_stats_t* stats = &normalizer->channels[channel];
    stats->min_value = FLT_MAX;
    stats->max_value = -FLT_MAX;
    stats->range = 1.0F;
    stats->count = 0;
}

void minmax_normalizer_reset_all(min_max_normalizer_t* normalizer) {
    if (!normalizer) return;
    nimcp_mutex_lock(normalizer->mutex);
    for (size_t i = 0; i < normalizer->num_channels; i++) {
        minmax_normalizer_reset_channel_unlocked(normalizer, i);
    }
    nimcp_mutex_unlock(normalizer->mutex);
}

size_t minmax_normalizer_num_channels(const min_max_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}

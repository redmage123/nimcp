#include "middleware/normalization/nimcp_adaptive_normalizer.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "utils/memory/nimcp_memory.h"
#include <math.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"



#define LOG_MODULE "nimcp_adaptive_normalizer"
#define LOG_MODULE_ID 0x0520
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(adaptive_normalizer)

typedef struct {
    float mean;
    float variance;
    float learning_rate;
    size_t count;
} adaptive_channel_t;

struct adaptive_normalizer {
    size_t num_channels;
    float initial_learning_rate;
    float adaptation_rate;
    adaptive_channel_t* channels;

    /* Thread safety: mutex protects channels state.
     * Added to fix thread-safety issue - concurrent calls to adaptive_normalizer
     * functions could corrupt internal state. */
    nimcp_mutex_t* mutex;
};

adaptive_normalizer_t* adaptive_normalizer_create(
    size_t num_channels,
    float initial_learning_rate,
    float adaptation_rate
) {
    if (num_channels == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_normalizer_create: num_channels is 0");
        return NULL;
    }

    adaptive_normalizer_t* norm = nimcp_calloc(1, sizeof(adaptive_normalizer_t));
    if (!norm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_normalizer_create: failed to allocate normalizer");
        return NULL;
    }

    norm->num_channels = num_channels;
    norm->initial_learning_rate = initial_learning_rate;
    norm->adaptation_rate = adaptation_rate;

    /* Thread safety: Create mutex to protect normalizer state.
     * This fixes thread-safety issue where concurrent calls could corrupt state. */
    norm->mutex = nimcp_mutex_create(NULL);
    if (!norm->mutex) {
        adaptive_normalizer_destroy(norm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_normalizer_create: norm->mutex is NULL");
        return NULL;
    }

    norm->channels = nimcp_calloc(num_channels, sizeof(adaptive_channel_t));
    if (!norm->channels) {
        adaptive_normalizer_destroy(norm);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_normalizer_create: norm->channels is NULL");
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].mean = 0.0F;
        norm->channels[i].variance = 1.0F;
        norm->channels[i].learning_rate = initial_learning_rate;
        norm->channels[i].count = 0;
    }

    return norm;
}

void adaptive_normalizer_destroy(adaptive_normalizer_t* normalizer) {
    if (!normalizer) return;

    nimcp_free(normalizer->channels);

    /* Thread safety: Clean up mutex */
    if (normalizer->mutex) {
        nimcp_mutex_free(normalizer->mutex);
    }

    nimcp_free(normalizer);
}

bool adaptive_normalizer_fit(
    adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_normalizer_fit: normalizer is NULL");
        return false;
    }

    /* Thread safety: Lock mutex to protect channel state */
    nimcp_mutex_lock(normalizer->mutex);

    adaptive_channel_t* ch = &normalizer->channels[channel];

    float lr = ch->learning_rate;
    ch->mean = (1.0F - lr) * ch->mean + lr * value;

    float diff = value - ch->mean;
    ch->variance = (1.0F - lr) * ch->variance + lr * diff * diff;

    // Adapt learning rate based on variance change
    ch->learning_rate *= (1.0F + normalizer->adaptation_rate * fabsf(diff));
    if (ch->learning_rate > 0.5F) ch->learning_rate = 0.5F;
    if (ch->learning_rate < 0.001F) ch->learning_rate = 0.001F;

    ch->count++;

    nimcp_mutex_unlock(normalizer->mutex);
    return true;
}

float adaptive_normalizer_transform(
    const adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;

    /* Thread safety: Lock mutex to protect channel state access */
    nimcp_mutex_lock(normalizer->mutex);

    const adaptive_channel_t* ch = &normalizer->channels[channel];
    if (ch->count == 0 || ch->variance < 0.0001F) {
        nimcp_mutex_unlock(normalizer->mutex);
        return 0.0F;
    }

    float result = (value - ch->mean) / sqrtf(ch->variance);

    nimcp_mutex_unlock(normalizer->mutex);
    return result;
}

float adaptive_normalizer_fit_transform(
    adaptive_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    adaptive_normalizer_fit(normalizer, channel, value);
    return adaptive_normalizer_transform(normalizer, channel, value);
}

bool adaptive_normalizer_reset_channel(
    adaptive_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_normalizer_reset_channel: normalizer is NULL");
        return false;
    }

    /* Thread safety: Lock mutex to protect channel state */
    nimcp_mutex_lock(normalizer->mutex);

    adaptive_channel_t* ch = &normalizer->channels[channel];
    ch->mean = 0.0F;
    ch->variance = 1.0F;
    ch->learning_rate = normalizer->initial_learning_rate;
    ch->count = 0;

    nimcp_mutex_unlock(normalizer->mutex);
    return true;
}

/* Internal helper - resets channel without locking (caller holds mutex) */
static void adaptive_normalizer_reset_channel_unlocked(
    adaptive_normalizer_t* normalizer,
    size_t channel
) {
    adaptive_channel_t* ch = &normalizer->channels[channel];
    ch->mean = 0.0F;
    ch->variance = 1.0F;
    ch->learning_rate = normalizer->initial_learning_rate;
    ch->count = 0;
}

void adaptive_normalizer_reset_all(adaptive_normalizer_t* normalizer) {
    if (!normalizer) return;

    /* Thread safety: Lock mutex to protect all channels */
    nimcp_mutex_lock(normalizer->mutex);

    for (size_t i = 0; i < normalizer->num_channels; i++) {
        adaptive_normalizer_reset_channel_unlocked(normalizer, i);
    }

    nimcp_mutex_unlock(normalizer->mutex);
}

size_t adaptive_normalizer_num_channels(const adaptive_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}

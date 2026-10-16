#include "middleware/normalization/nimcp_homeostatic_normalizer.h"
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
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"



#define LOG_MODULE "nimcp_homeostatic_normalizer"
#define LOG_MODULE_ID 0x0521
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(homeostatic_normalizer)

typedef struct {
    float current_activity;
    float scaling_factor;
    float accumulated_error;
} homeostatic_channel_t;

struct homeostatic_normalizer {
    size_t num_channels;
    float target_activity;
    float time_constant;
    homeostatic_channel_t* channels;
    nimcp_mutex_t* mutex;
};

homeostatic_normalizer_t* homeostatic_normalizer_create(
    size_t num_channels,
    float target_activity,
    float time_constant
) {
    if (num_channels == 0 || time_constant <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "homeostatic_normalizer_create: invalid num_channels or time_constant");
        return NULL;
    }

    homeostatic_normalizer_t* norm = nimcp_calloc(1, sizeof(homeostatic_normalizer_t));
    if (!norm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_normalizer_create: failed to allocate normalizer");
        return NULL;
    }

    norm->num_channels = num_channels;
    norm->target_activity = target_activity;
    norm->time_constant = time_constant;

    norm->channels = nimcp_calloc(num_channels, sizeof(homeostatic_channel_t));
    if (!norm->channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_normalizer_create: failed to allocate channels");
        nimcp_free(norm);
        return NULL;
    }

    for (size_t i = 0; i < num_channels; i++) {
        norm->channels[i].current_activity = 0.0F;
        norm->channels[i].scaling_factor = 1.0F;
        norm->channels[i].accumulated_error = 0.0F;
    }

    norm->mutex = nimcp_mutex_create(NULL);
    if (!norm->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_normalizer_create: failed to create mutex");
        nimcp_free(norm->channels);
        nimcp_free(norm);
        return NULL;
    }

    return norm;
}

void homeostatic_normalizer_destroy(homeostatic_normalizer_t* normalizer) {
    if (!normalizer) return;
    if (normalizer->mutex) nimcp_mutex_free(normalizer->mutex);
    nimcp_free(normalizer->channels);
    nimcp_free(normalizer);
}

bool homeostatic_normalizer_update(
    homeostatic_normalizer_t* normalizer,
    size_t channel,
    float activity,
    float dt
) {
    if (!normalizer || channel >= normalizer->num_channels || dt <= 0.0F) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "homeostatic_normalizer_update: normalizer is NULL");
        return false;
    }

    nimcp_mutex_lock(normalizer->mutex);

    homeostatic_channel_t* ch = &normalizer->channels[channel];

    // Update activity with exponential smoothing
    float alpha = dt / normalizer->time_constant;
    ch->current_activity = (1.0F - alpha) * ch->current_activity + alpha * activity;

    // Calculate error
    float error = normalizer->target_activity - ch->current_activity;
    ch->accumulated_error += error * dt;

    // Update scaling factor (homeostatic plasticity)
    ch->scaling_factor = 1.0F + 0.1F * ch->accumulated_error;

    // Clamp scaling factor
    if (ch->scaling_factor < 0.1F) ch->scaling_factor = 0.1F;
    if (ch->scaling_factor > 10.0F) ch->scaling_factor = 10.0F;

    nimcp_mutex_unlock(normalizer->mutex);

    return true;
}

float homeostatic_normalizer_get_scaling(
    const homeostatic_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) return 1.0F;
    return normalizer->channels[channel].scaling_factor;
}

float homeostatic_normalizer_apply(
    const homeostatic_normalizer_t* normalizer,
    size_t channel,
    float value
) {
    if (!normalizer || channel >= normalizer->num_channels) return value;
    return value * normalizer->channels[channel].scaling_factor;
}

bool homeostatic_normalizer_reset_channel(
    homeostatic_normalizer_t* normalizer,
    size_t channel
) {
    if (!normalizer || channel >= normalizer->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "homeostatic_normalizer_reset_channel: normalizer is NULL");
        return false;
    }

    nimcp_mutex_lock(normalizer->mutex);

    homeostatic_channel_t* ch = &normalizer->channels[channel];
    ch->current_activity = 0.0F;
    ch->scaling_factor = 1.0F;
    ch->accumulated_error = 0.0F;

    nimcp_mutex_unlock(normalizer->mutex);

    return true;
}

static bool homeostatic_normalizer_reset_channel_unlocked(
    homeostatic_normalizer_t* normalizer,
    size_t channel
) {
    homeostatic_channel_t* ch = &normalizer->channels[channel];
    ch->current_activity = 0.0F;
    ch->scaling_factor = 1.0F;
    ch->accumulated_error = 0.0F;
    return true;
}

void homeostatic_normalizer_reset_all(homeostatic_normalizer_t* normalizer) {
    if (!normalizer) return;
    nimcp_mutex_lock(normalizer->mutex);
    for (size_t i = 0; i < normalizer->num_channels; i++) {
        homeostatic_normalizer_reset_channel_unlocked(normalizer, i);
    }
    nimcp_mutex_unlock(normalizer->mutex);
}

size_t homeostatic_normalizer_num_channels(const homeostatic_normalizer_t* normalizer) {
    if (!normalizer) return 0;
    return normalizer->num_channels;
}

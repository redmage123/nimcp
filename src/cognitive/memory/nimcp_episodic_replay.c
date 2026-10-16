/**
 * @file nimcp_episodic_replay.c
 * @brief Episodic replay during sleep/consolidation
 *
 * WHAT: Replay important experiences at accelerated speed during sleep
 * WHY:  Hippocampal replay consolidates labile memories into stable traces
 * HOW:  Circular buffer stores (features, target, label, loss, reward).
 *       During consolidation, top-N by importance replayed via learn_vector.
 *
 * BIOLOGICAL BASIS:
 * - Wilson & McNaughton (1994): Hippocampal place cell replay
 * - Diekelmann & Born (2010): Active systems consolidation during SWS
 */

#include "cognitive/memory/nimcp_episodic_replay.h"
#include "cognitive/memory/nimcp_memory_kg_events.h"  /* W6: KG event emitters */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "EPISODIC_REPLAY"

/* Forward declaration for brain learn_vector */
typedef int nimcp_status_t;
extern nimcp_status_t nimcp_brain_learn_vector(
    nimcp_brain_t brain,
    const float* features, uint32_t num_features,
    const float* target, uint32_t target_size,
    const char* label, float confidence);

/* Internal brain_learn_vector: used by the sleep cycle, which has only the
 * internal brain_t (void* here to keep this TU header-free of the internal
 * brain header). The real signature is declared in
 * include/core/brain/learning/nimcp_brain_learning.h:
 *   float brain_learn_vector(brain_t, const float*, uint32_t,
 *                            const float*, uint32_t, const char*, float);
 * brain_t is itself a pointer typedef, so void* is ABI-compatible. */
extern float brain_learn_vector(
    void* brain_internal,
    const float* features, uint32_t num_features,
    const float* target, uint32_t target_size,
    const char* label, float confidence);

/* ---- internal types ---- */

typedef struct {
    float* features;
    uint32_t num_features;
    float* target;
    uint32_t target_size;
    char label[EPISODIC_REPLAY_MAX_LABEL];
    float loss;
    float reward;
    uint64_t timestamp;       /* Monotonic counter for recency */
} episodic_experience_t;

struct nimcp_episodic_replay {
    nimcp_episodic_replay_config_t config;

    episodic_experience_t* buffer;
    uint32_t buffer_capacity;
    uint32_t buffer_count;
    uint32_t write_head;      /* Next write position (circular) */
    uint64_t total_recorded;  /* Monotonic counter */

    float* importance_scores; /* Scratch buffer for consolidation sort */
    uint32_t* sort_indices;   /* Scratch buffer for sorted indices */

    /* Set true while consolidate is running through brain_learn_vector so
     * the record hook can skip re-encoding replayed experiences as new
     * buffer entries. Without this, each replay cycle adds replay_count
     * fresh slots overwriting the oldest real training experiences — the
     * buffer would be dominated by recent replays within ~20 cycles. */
    bool replay_in_progress;
};

/* ---- helpers ---- */

static void free_experience(episodic_experience_t* exp) {
    if (!exp) return;
    if (exp->features) nimcp_free(exp->features);
    if (exp->target) nimcp_free(exp->target);
    /* Zero-fill entire struct to clear stale data (labels, loss, reward) */
    memset(exp, 0, sizeof(episodic_experience_t));
}

static float compute_importance(const episodic_experience_t* exp,
    uint64_t total_recorded, const nimcp_episodic_replay_config_t* config)
{
    /* importance = loss * 0.5 + reward * 0.3 + recency * 0.2 */
    float loss_score = 0.0f;
    float reward_score = 0.0f;
    float recency_score = 0.0f;

    if (config->prioritize_high_loss) {
        loss_score = exp->loss;
        if (loss_score > 10.0f) loss_score = 10.0f;  /* Cap */
        loss_score /= 10.0f;  /* Normalize to [0, 1] */
    }

    if (config->prioritize_high_reward) {
        reward_score = exp->reward;
        if (reward_score < 0.0f) reward_score = 0.0f;
        if (reward_score > 1.0f) reward_score = 1.0f;
    }

    /* Recency: newer experiences get higher score */
    if (total_recorded > 0 && exp->timestamp > 0) {
        float age = (float)(total_recorded - exp->timestamp);
        float max_age = (float)total_recorded;
        recency_score = 1.0f - (age / (max_age + 1.0f));
        if (recency_score < 0.0f) recency_score = 0.0f;
    }

    return loss_score * 0.5f + reward_score * 0.3f + recency_score * 0.2f;
}

/* Simple insertion sort for top-N selection (buffer_capacity is bounded) */
static void sort_by_importance_desc(const float* scores, uint32_t* indices, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        indices[i] = i;
    }

    for (uint32_t i = 1; i < n; i++) {
        uint32_t key_idx = indices[i];
        float key_val = scores[key_idx];
        int j = (int)i - 1;

        while (j >= 0 && scores[indices[j]] < key_val) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key_idx;
    }
}

/* ---- public API ---- */

nimcp_episodic_replay_config_t nimcp_episodic_replay_config_default(void) {
    nimcp_episodic_replay_config_t cfg = {0};
    cfg.replay_count = 50;
    cfg.replay_speed_multiplier = 10.0f;
    cfg.importance_threshold = 0.5f;
    cfg.replay_lr_scale = 0.3f;
    cfg.prioritize_high_loss = true;
    cfg.prioritize_high_reward = true;
    cfg.buffer_capacity = EPISODIC_REPLAY_DEFAULT_CAPACITY;
    return cfg;
}

nimcp_episodic_replay_t* nimcp_episodic_replay_create(const nimcp_episodic_replay_config_t* config) {
    if (!config) {
        LOG_ERROR("[%s] NULL config", LOG_MODULE);
        return NULL;
    }

    nimcp_episodic_replay_t* handle = nimcp_calloc(1, sizeof(nimcp_episodic_replay_t));
    if (!handle) {
        LOG_ERROR("[%s] Failed to allocate replay handle", LOG_MODULE);
        return NULL;
    }

    handle->config = *config;

    /* Clamp capacity */
    if (handle->config.buffer_capacity == 0) {
        handle->config.buffer_capacity = EPISODIC_REPLAY_DEFAULT_CAPACITY;
    }

    handle->buffer_capacity = handle->config.buffer_capacity;
    handle->buffer = nimcp_calloc(handle->buffer_capacity, sizeof(episodic_experience_t));
    if (!handle->buffer) {
        LOG_ERROR("[%s] Failed to allocate experience buffer (%u entries)",
                  LOG_MODULE, handle->buffer_capacity);
        nimcp_free(handle);
        return NULL;
    }

    handle->importance_scores = nimcp_calloc(handle->buffer_capacity, sizeof(float));
    handle->sort_indices = nimcp_calloc(handle->buffer_capacity, sizeof(uint32_t));
    if (!handle->importance_scores || !handle->sort_indices) {
        LOG_ERROR("[%s] Failed to allocate scratch buffers", LOG_MODULE);
        nimcp_episodic_replay_destroy(handle);
        return NULL;
    }

    handle->buffer_count = 0;
    handle->write_head = 0;
    handle->total_recorded = 0;

    LOG_INFO("[%s] Created: capacity=%u, replay_count=%u, lr_scale=%.3f, threshold=%.3f",
             LOG_MODULE, handle->buffer_capacity, handle->config.replay_count,
             handle->config.replay_lr_scale, handle->config.importance_threshold);

    return handle;
}

void nimcp_episodic_replay_destroy(nimcp_episodic_replay_t* handle) {
    if (!handle) return;

    if (handle->buffer) {
        for (uint32_t i = 0; i < handle->buffer_capacity; i++) {
            free_experience(&handle->buffer[i]);
        }
        nimcp_free(handle->buffer);
    }

    if (handle->importance_scores) nimcp_free(handle->importance_scores);
    if (handle->sort_indices) nimcp_free(handle->sort_indices);

    nimcp_free(handle);
    LOG_DEBUG("[%s] Destroyed", LOG_MODULE);
}

int nimcp_episodic_replay_record(nimcp_episodic_replay_t* handle,
    const float* features, uint32_t num_features,
    const float* target, uint32_t target_size,
    const char* label, float loss, float reward)
{
    if (!handle || !features || num_features == 0) {
        LOG_ERROR("[%s] Invalid arguments to record", LOG_MODULE);
        return -1;
    }

    /* Skip re-recording during consolidation — prevents the buffer from
     * becoming dominated by recent replays. */
    if (handle->replay_in_progress) {
        return 0;
    }

    if (num_features > EPISODIC_REPLAY_MAX_FEATURES) {
        LOG_WARN("[%s] Clamping num_features from %u to %u",
                 LOG_MODULE, num_features, EPISODIC_REPLAY_MAX_FEATURES);
        num_features = EPISODIC_REPLAY_MAX_FEATURES;
    }

    /* Free old experience at write position (circular overwrite) */
    episodic_experience_t* slot = &handle->buffer[handle->write_head];
    free_experience(slot);

    /* Copy features */
    slot->features = nimcp_calloc(num_features, sizeof(float));
    if (!slot->features) {
        LOG_ERROR("[%s] Failed to allocate features (%u floats)", LOG_MODULE, num_features);
        return -1;
    }
    memcpy(slot->features, features, num_features * sizeof(float));
    slot->num_features = num_features;

    /* Copy target (if provided) */
    if (target && target_size > 0) {
        uint32_t ts = target_size > EPISODIC_REPLAY_MAX_TARGET ? EPISODIC_REPLAY_MAX_TARGET : target_size;
        slot->target = nimcp_calloc(ts, sizeof(float));
        if (!slot->target) {
            LOG_ERROR("[%s] Failed to allocate target (%u floats)", LOG_MODULE, ts);
            nimcp_free(slot->features);
            slot->features = NULL;
            return -1;
        }
        memcpy(slot->target, target, ts * sizeof(float));
        slot->target_size = ts;
    } else {
        slot->target = NULL;
        slot->target_size = 0;
    }

    /* Copy label */
    if (label) {
        strncpy(slot->label, label, EPISODIC_REPLAY_MAX_LABEL - 1);
        slot->label[EPISODIC_REPLAY_MAX_LABEL - 1] = '\0';
    } else {
        slot->label[0] = '\0';
    }

    slot->loss = loss;
    slot->reward = reward;
    handle->total_recorded++;
    slot->timestamp = handle->total_recorded;

    /* Advance circular write head */
    handle->write_head = (handle->write_head + 1) % handle->buffer_capacity;
    if (handle->buffer_count < handle->buffer_capacity) {
        handle->buffer_count++;
    }

    return 0;
}

int nimcp_episodic_replay_consolidate(nimcp_episodic_replay_t* handle,
    nimcp_brain_t brain, float learning_rate)
{
    if (!handle || !brain) {
        LOG_ERROR("[%s] NULL argument to consolidate", LOG_MODULE);
        return -1;
    }

    if (handle->buffer_count == 0) {
        LOG_DEBUG("[%s] No experiences to replay", LOG_MODULE);
        return 0;
    }

    handle->replay_in_progress = true;

    /* Compute importance for all stored experiences */
    for (uint32_t i = 0; i < handle->buffer_count; i++) {
        handle->importance_scores[i] = compute_importance(
            &handle->buffer[i], handle->total_recorded, &handle->config);
    }

    /* Sort indices by importance (descending) */
    sort_by_importance_desc(handle->importance_scores,
                            handle->sort_indices, handle->buffer_count);

    /* Replay top-N experiences */
    float replay_lr = learning_rate * handle->config.replay_lr_scale;
    uint32_t replay_target = handle->config.replay_count;
    if (replay_target > handle->buffer_count) {
        replay_target = handle->buffer_count;
    }

    uint32_t replayed = 0;

    for (uint32_t r = 0; r < replay_target; r++) {
        uint32_t idx = handle->sort_indices[r];
        float importance = handle->importance_scores[idx];

        /* Skip below threshold */
        if (importance < handle->config.importance_threshold) {
            LOG_DEBUG("[%s] Stopping replay at rank %u (importance=%.4f < threshold=%.4f)",
                      LOG_MODULE, r, importance, handle->config.importance_threshold);
            break;
        }

        episodic_experience_t* exp = &handle->buffer[idx];
        if (!exp->features || exp->num_features == 0) continue;

        /* Replay through brain learn_vector at reduced LR */
        float confidence = replay_lr;  /* Use replay LR as confidence signal */
        const char* replay_label = exp->label[0] ? exp->label : "replay";

        nimcp_status_t status = nimcp_brain_learn_vector(
            brain,
            exp->features, exp->num_features,
            exp->target, exp->target_size,
            replay_label, confidence);

        if (status == 0) {
            replayed++;
        } else {
            LOG_WARN("[%s] learn_vector failed for experience idx=%u", LOG_MODULE, idx);
        }
    }

    LOG_INFO("[%s] Consolidated: replayed %u/%u experiences (lr=%.6f, buffer=%u)",
             LOG_MODULE, replayed, replay_target, replay_lr, handle->buffer_count);

    handle->replay_in_progress = false;

    /* W6: emit KG event for replay cycle */
    memory_kg_emit_replay_cycle(
        memory_kg_events_get_registered_brain(),
        replayed, handle->buffer_count);

    return (int)replayed;
}

int nimcp_episodic_replay_consolidate_internal(nimcp_episodic_replay_t* handle,
    void* brain_internal, float learning_rate)
{
    if (!handle || !brain_internal) {
        LOG_ERROR("[%s] NULL argument to consolidate_internal", LOG_MODULE);
        return -1;
    }

    if (handle->buffer_count == 0) {
        LOG_DEBUG("[%s] No experiences to replay (internal)", LOG_MODULE);
        return 0;
    }

    handle->replay_in_progress = true;

    for (uint32_t i = 0; i < handle->buffer_count; i++) {
        handle->importance_scores[i] = compute_importance(
            &handle->buffer[i], handle->total_recorded, &handle->config);
    }

    sort_by_importance_desc(handle->importance_scores,
                            handle->sort_indices, handle->buffer_count);

    float replay_lr = learning_rate * handle->config.replay_lr_scale;
    uint32_t replay_target = handle->config.replay_count;
    if (replay_target > handle->buffer_count) {
        replay_target = handle->buffer_count;
    }

    uint32_t replayed = 0;

    for (uint32_t r = 0; r < replay_target; r++) {
        uint32_t idx = handle->sort_indices[r];
        float importance = handle->importance_scores[idx];

        if (importance < handle->config.importance_threshold) {
            LOG_DEBUG("[%s] Stopping replay at rank %u (importance=%.4f < threshold=%.4f)",
                      LOG_MODULE, r, importance, handle->config.importance_threshold);
            break;
        }

        episodic_experience_t* exp = &handle->buffer[idx];
        if (!exp->features || exp->num_features == 0) continue;

        float confidence = replay_lr;
        const char* replay_label = exp->label[0] ? exp->label : "replay";

        float loss = brain_learn_vector(
            brain_internal,
            exp->features, exp->num_features,
            exp->target, exp->target_size,
            replay_label, confidence);

        if (loss >= 0.0f) {
            replayed++;
        } else {
            LOG_WARN("[%s] brain_learn_vector failed for experience idx=%u (loss=%.2f)",
                     LOG_MODULE, idx, loss);
        }
    }

    LOG_INFO("[%s] Consolidated (internal): replayed %u/%u experiences (lr=%.6f, buffer=%u)",
             LOG_MODULE, replayed, replay_target, replay_lr, handle->buffer_count);

    handle->replay_in_progress = false;

    /* W6: emit KG event for replay cycle (internal variant) */
    memory_kg_emit_replay_cycle(
        memory_kg_events_get_registered_brain(),
        replayed, handle->buffer_count);

    return (int)replayed;
}

int nimcp_episodic_replay_rem_recombine_internal(
    nimcp_episodic_replay_t* handle,
    void* brain_internal, float learning_rate, uint32_t num_pairs)
{
    if (!handle || !brain_internal) {
        return -1;
    }
    if (handle->buffer_count < 2 || num_pairs == 0) {
        return 0;
    }

    handle->replay_in_progress = true;

    /* Very low LR — REM should not overwrite consolidated memory, only
     * seed novel associations. */
    float rem_lr = learning_rate * handle->config.replay_lr_scale * 0.3f;
    uint32_t replayed = 0;

    /* Scratch buffer for mixed features. Size by the largest source. */
    uint32_t max_features = 0;
    uint32_t max_target = 0;
    for (uint32_t i = 0; i < handle->buffer_count; i++) {
        if (handle->buffer[i].num_features > max_features) {
            max_features = handle->buffer[i].num_features;
        }
        if (handle->buffer[i].target_size > max_target) {
            max_target = handle->buffer[i].target_size;
        }
    }
    if (max_features == 0) {
        handle->replay_in_progress = false;
        return 0;
    }

    float* mix_features = nimcp_calloc(max_features, sizeof(float));
    if (!mix_features) {
        handle->replay_in_progress = false;
        return -1;
    }
    float* mix_target = NULL;
    if (max_target > 0) {
        mix_target = nimcp_calloc(max_target, sizeof(float));
        if (!mix_target) {
            nimcp_free(mix_features);
            handle->replay_in_progress = false;
            return -1;
        }
    }

    for (uint32_t p = 0; p < num_pairs; p++) {
        /* Pick two distinct random indices. */
        uint32_t a = (uint32_t)(rand() % handle->buffer_count);
        uint32_t b = (uint32_t)(rand() % handle->buffer_count);
        if (a == b) {
            b = (a + 1) % handle->buffer_count;
        }
        episodic_experience_t* ea = &handle->buffer[a];
        episodic_experience_t* eb = &handle->buffer[b];
        if (!ea->features || !eb->features ||
            ea->num_features == 0 || eb->num_features == 0) {
            continue;
        }

        /* Random blend ratio in [0.3, 0.7] — keeps both traces visible. */
        float alpha = 0.3f + 0.4f * ((float)rand() / (float)RAND_MAX);
        uint32_t nf = ea->num_features < eb->num_features
                        ? ea->num_features : eb->num_features;
        for (uint32_t i = 0; i < nf; i++) {
            mix_features[i] = alpha * ea->features[i] +
                              (1.0f - alpha) * eb->features[i];
        }

        /* Blend target analogously if both present. */
        uint32_t nt = 0;
        if (ea->target && eb->target &&
            ea->target_size > 0 && eb->target_size > 0) {
            nt = ea->target_size < eb->target_size
                    ? ea->target_size : eb->target_size;
            if (mix_target) {
                for (uint32_t i = 0; i < nt; i++) {
                    mix_target[i] = alpha * ea->target[i] +
                                    (1.0f - alpha) * eb->target[i];
                }
            }
        }

        float loss = brain_learn_vector(
            brain_internal,
            mix_features, nf,
            (nt > 0 ? mix_target : NULL), nt,
            "rem_recombine", rem_lr);
        if (loss >= 0.0f) {
            replayed++;
        }
    }

    if (mix_target) nimcp_free(mix_target);
    nimcp_free(mix_features);

    LOG_INFO("[%s] REM recombine: %u/%u pairs blended (lr=%.6f)",
             LOG_MODULE, replayed, num_pairs, rem_lr);

    handle->replay_in_progress = false;
    return (int)replayed;
}

uint32_t nimcp_episodic_replay_get_buffer_size(const nimcp_episodic_replay_t* handle) {
    if (!handle) return 0;
    return handle->buffer_count;
}

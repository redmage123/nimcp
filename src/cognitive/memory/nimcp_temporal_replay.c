/**
 * @file nimcp_temporal_replay.c
 * @brief Temporal Replay Implementation
 * @version 1.0.0
 * @date 2025-01-04
 */

#include "cognitive/memory/nimcp_temporal_replay.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE(temporal_replay, MESH_ADAPTER_CATEGORY_MEMORY)



/* Logging macros - wrap LOG_* for consistent usage */
#define NIMCP_LOG_INFO(...)  LOG_INFO(__VA_ARGS__)
#define NIMCP_LOG_WARN(...)  LOG_WARN(__VA_ARGS__)
#define NIMCP_LOG_ERROR(...) LOG_ERROR(__VA_ARGS__)

/* ============================================================================
 * Private Helpers
 * ============================================================================ */

/**
 * @brief Get transition at index
 */
static inline replay_transition_t* get_transition(temporal_replay_t* replay, uint32_t index) {
    return &replay->transitions[index];
}

/**
 * @brief Get transition state
 */
static inline float* get_state(temporal_replay_t* replay, uint32_t index) {
    replay_transition_t* t = get_transition(replay, index);
    return t->state;
}

/**
 * @brief Update priority tree (sum tree)
 */
static void update_priority_tree(temporal_replay_t* replay, uint32_t index, float priority) {
    if (!replay->priority_tree) {
        return;
    }

    uint32_t capacity = replay->config.capacity;
    uint32_t tree_idx = index + capacity;

    float delta = priority - replay->priority_tree[tree_idx];
    replay->priority_tree[tree_idx] = priority;
    replay->total_priority += delta;

    while (tree_idx > 1) {
        tree_idx /= 2;
        replay->priority_tree[tree_idx] = replay->priority_tree[2 * tree_idx] +
                                           replay->priority_tree[2 * tree_idx + 1];
    }
}

/**
 * @brief Sample from priority tree
 */
static uint32_t sample_from_priority_tree(temporal_replay_t* replay, float value) {
    if (!replay->priority_tree || replay->total_priority <= 0.0f) {
        return (uint32_t)(((float)nimcp_tl_rand() / RAND_MAX) * replay->count);
    }

    uint32_t capacity = replay->config.capacity;
    uint32_t idx = 1;

    while (idx < capacity) {
        uint32_t left = 2 * idx;
        uint32_t right = left + 1;

        if (value <= replay->priority_tree[left]) {
            idx = left;
        } else {
            value -= replay->priority_tree[left];
            idx = right;
        }
    }

    return idx - capacity;
}

/**
 * @brief Get max priority
 */
static float get_max_priority(temporal_replay_t* replay) {
    if (!replay || replay->count == 0) {
        return 1.0f;
    }

    float max_p = REPLAY_MIN_PRIORITY;
    for (uint32_t i = 0; i < replay->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && replay->count > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)replay->count);
        }

        float p = replay->transitions[i].td_error;
        if (p > max_p) {
            max_p = p;
        }
    }
    return max_p;
}

/**
 * @brief Should use GPU?
 */
static inline bool should_use_gpu(const temporal_replay_t* replay, uint32_t batch_size) {
#ifdef NIMCP_ENABLE_CUDA
    if (!replay || !replay->gpu_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "should_use_gpu: required parameter is NULL (replay, replay->gpu_initialized)");
        return false;
    }
    if (replay->config.gpu_mode == REPLAY_GPU_DISABLED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "should_use_gpu: validation failed");
        return false;
    }
    if (replay->config.gpu_mode == REPLAY_GPU_REQUIRED ||
        replay->config.gpu_mode == REPLAY_GPU_PREFERRED) {
        return true;
    }
    if (replay->config.gpu_mode == REPLAY_GPU_AUTO) {
        return batch_size >= replay->config.min_batch_for_gpu;
    }
    return false;
#else
    (void)replay;
    (void)batch_size;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "should_use_gpu: validation failed");
    return false;
#endif
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int replay_default_config(replay_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_replay_default_confi", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(*config));

    config->capacity = REPLAY_DEFAULT_CAPACITY;
    config->state_dim = 256;
    config->action_dim = 0;
    config->max_sequence_length = REPLAY_DEFAULT_SEQUENCE_LENGTH;

    config->default_mode = REPLAY_MODE_PRIORITY;
    config->priority_alpha = REPLAY_DEFAULT_PRIORITY_ALPHA;
    config->is_beta = REPLAY_DEFAULT_IS_BETA;
    config->compression_ratio = REPLAY_DEFAULT_COMPRESSION;

    config->use_priority_tree = true;
    config->priority_decay = 0.0f;

    config->gpu_mode = REPLAY_GPU_AUTO;
    config->min_batch_for_gpu = 32;

    config->enable_bio_async = false;

    config->store_next_states = false;
    config->store_actions = false;

    return NIMCP_SUCCESS;
}

int replay_validate_config(const replay_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_replay_validate_conf", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(config->capacity > 0 && config->capacity <= REPLAY_MAX_CAPACITY,
                      NIMCP_ERROR_INVALID_PARAM, "capacity must be in range (0, REPLAY_MAX_CAPACITY]");
    NIMCP_CHECK_THROW(config->state_dim > 0 && config->state_dim <= 65536,
                      NIMCP_ERROR_INVALID_PARAM, "state_dim must be in range (0, 65536]");
    NIMCP_CHECK_THROW(config->priority_alpha >= 0.0f && config->priority_alpha <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM, "priority_alpha must be in range [0.0, 1.0]");
    NIMCP_CHECK_THROW(config->is_beta >= 0.0f && config->is_beta <= 1.0f,
                      NIMCP_ERROR_INVALID_PARAM, "is_beta must be in range [0.0, 1.0]");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

temporal_replay_t* temporal_replay_create(const replay_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_create", 0.0f);


    replay_config_t default_config;
    if (!config) {
        replay_default_config(&default_config);
        config = &default_config;
    }

    if (replay_validate_config(config) != NIMCP_SUCCESS) {
        NIMCP_LOG_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_replay_create: validation failed");
        return NULL;
    }

    temporal_replay_t* replay = nimcp_calloc(1, sizeof(temporal_replay_t));
    if (!replay) {
        NIMCP_LOG_ERROR("Failed to allocate replay buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_replay_create: replay is NULL");
        return NULL;
    }

    memcpy(&replay->config, config, sizeof(replay_config_t));

    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    replay->mutex = nimcp_mutex_create(&attr);
    if (!replay->mutex) {
        NIMCP_LOG_ERROR("Failed to create mutex");
        nimcp_free(replay);
        replay = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_replay_create: replay->mutex is NULL");
        return NULL;
    }

    replay->transitions = nimcp_calloc(config->capacity, sizeof(replay_transition_t));
    if (!replay->transitions) {
        NIMCP_LOG_ERROR("Failed to allocate transitions");
        goto error;
    }

    for (uint32_t i = 0; i < config->capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && config->capacity > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)config->capacity);
        }

        replay->transitions[i].state = nimcp_calloc(config->state_dim, sizeof(float));
        if (!replay->transitions[i].state) {
            NIMCP_LOG_ERROR("Failed to allocate state buffer");
            goto error;
        }

        if (config->store_actions && config->action_dim > 0) {
            replay->transitions[i].action = nimcp_calloc(config->action_dim, sizeof(float));
            if (!replay->transitions[i].action) {
                goto error;
            }
        }

        if (config->store_next_states) {
            replay->transitions[i].next_state = nimcp_calloc(config->state_dim, sizeof(float));
            if (!replay->transitions[i].next_state) {
                goto error;
            }
        }
    }

    if (config->use_priority_tree) {
        replay->priority_tree = nimcp_calloc(2 * config->capacity, sizeof(float));
        if (!replay->priority_tree) {
            NIMCP_LOG_ERROR("Failed to allocate priority tree");
            goto error;
        }
    }

    uint32_t max_sequences = config->capacity / config->max_sequence_length + 1;
    replay->sequences = nimcp_calloc(max_sequences, sizeof(replay_sequence_t));
    if (!replay->sequences) {
        NIMCP_LOG_ERROR("Failed to allocate sequences");
        goto error;
    }
    replay->max_sequences = max_sequences;

    replay->head = 0;
    replay->count = 0;
    replay->next_transition_id = 1;
    replay->num_sequences = 0;
    replay->current_sequence_id = 0;
    replay->recording_sequence = false;
    replay->total_priority = 0.0f;

    replay->seq_state = REPLAY_SEQ_IDLE;
    replay->replay_position = 0;
    replay->replay_sequence_idx = 0;

#ifdef NIMCP_ENABLE_CUDA
    replay->gpu_ctx = NULL;
    replay->states_device = NULL;
    replay->priorities_device = NULL;
    replay->gpu_initialized = false;

    if (config->gpu_mode != REPLAY_GPU_DISABLED) {
        if (temporal_replay_init_gpu(replay, NULL) != NIMCP_SUCCESS) {
            if (config->gpu_mode == REPLAY_GPU_REQUIRED) {
                NIMCP_LOG_ERROR("GPU required but init failed");
                goto error;
            }
            NIMCP_LOG_WARN("GPU init failed, using CPU");
        }
    }
#endif

    NIMCP_LOG_INFO("Created temporal replay: capacity=%u, state_dim=%u",
                   config->capacity, config->state_dim);
    return replay;

error:
    temporal_replay_destroy(replay);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "temporal_replay_create: operation failed");
    return NULL;
}

void temporal_replay_destroy(temporal_replay_t* replay) {
    if (!replay) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_destroy", 0.0f);


    if (replay->transitions) {
        for (uint32_t i = 0; i < replay->config.capacity; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && replay->config.capacity > 256) {
                temporal_replay_heartbeat("temporal_rep_loop",
                                 (float)(i + 1) / (float)replay->config.capacity);
            }

            if (replay->transitions[i].state) {
                nimcp_free(replay->transitions[i].state);
            }
            if (replay->transitions[i].action) {
                nimcp_free(replay->transitions[i].action);
            }
            if (replay->transitions[i].next_state) {
                nimcp_free(replay->transitions[i].next_state);
            }
        }
        nimcp_free(replay->transitions);
    }

    if (replay->priority_tree) {
        nimcp_free(replay->priority_tree);
    }

    if (replay->sequences) {
        for (uint32_t i = 0; i < replay->num_sequences; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && replay->num_sequences > 256) {
                temporal_replay_heartbeat("temporal_rep_loop",
                                 (float)(i + 1) / (float)replay->num_sequences);
            }

            if (replay->sequences[i].transition_indices) {
                nimcp_free(replay->sequences[i].transition_indices);
            }
        }
        nimcp_free(replay->sequences);
    }

#ifdef NIMCP_ENABLE_CUDA
    if (replay->states_device) {
        cudaFree(replay->states_device);
    }
    if (replay->priorities_device) {
        cudaFree(replay->priorities_device);
    }
    if (replay->gpu_ctx) {
        nimcp_gpu_context_destroy(replay->gpu_ctx);
    }
#endif

    if (replay->mutex) {
        nimcp_mutex_destroy(replay->mutex);
    }

    nimcp_free(replay);
    replay = NULL;
}

int temporal_replay_clear(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_clear", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    nimcp_mutex_lock(replay->mutex);

    replay->head = 0;
    replay->count = 0;
    replay->next_transition_id = 1;
    replay->total_priority = 0.0f;

    for (uint32_t i = 0; i < replay->num_sequences; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && replay->num_sequences > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)replay->num_sequences);
        }

        if (replay->sequences[i].transition_indices) {
            nimcp_free(replay->sequences[i].transition_indices);
            replay->sequences[i].transition_indices = NULL;
        }
    }
    replay->num_sequences = 0;
    replay->recording_sequence = false;

    if (replay->priority_tree) {
        memset(replay->priority_tree, 0, 2 * replay->config.capacity * sizeof(float));
    }

    replay->seq_state = REPLAY_SEQ_IDLE;

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Storage API
 * ============================================================================ */

int temporal_replay_store(temporal_replay_t* replay,
                           const float* state,
                           const float* action,
                           const float* next_state,
                           float reward,
                           bool terminal,
                           float priority) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_store", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state is NULL");

    nimcp_mutex_lock(replay->mutex);

    uint32_t idx = replay->head;
    replay_transition_t* t = get_transition(replay, idx);

    memcpy(t->state, state, replay->config.state_dim * sizeof(float));

    if (action && t->action && replay->config.action_dim > 0) {
        memcpy(t->action, action, replay->config.action_dim * sizeof(float));
    }

    if (next_state && t->next_state) {
        memcpy(t->next_state, next_state, replay->config.state_dim * sizeof(float));
    }

    t->reward = reward;
    t->terminal = terminal;
    t->timestamp = 0;

    if (priority <= 0.0f) {
        priority = get_max_priority(replay);
    }
    t->td_error = priority;

    if (replay->config.use_priority_tree) {
        float tree_priority = powf(priority, replay->config.priority_alpha);
        update_priority_tree(replay, idx, tree_priority);
    }

    replay->head = (replay->head + 1) % replay->config.capacity;
    if (replay->count < replay->config.capacity) {
        replay->count++;
    }

    replay->stats.transitions_stored = replay->count;
    replay->stats.capacity_used = (float)replay->count / (float)replay->config.capacity;

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

uint32_t temporal_replay_start_sequence(temporal_replay_t* replay) {
    if (!replay) {
        return UINT32_MAX;
    }

    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_start_sequence", 0.0f);


    nimcp_mutex_lock(replay->mutex);

    if (replay->num_sequences >= replay->max_sequences) {
        nimcp_mutex_unlock(replay->mutex);
        return UINT32_MAX;
    }

    uint32_t seq_id = replay->num_sequences;
    replay_sequence_t* seq = &replay->sequences[seq_id];

    seq->sequence_id = seq_id;
    seq->length = 0;
    seq->start_timestamp = 0;
    seq->total_reward = 0.0f;
    seq->transition_indices = nimcp_calloc(replay->config.max_sequence_length, sizeof(uint32_t));

    if (!seq->transition_indices) {
        nimcp_mutex_unlock(replay->mutex);
        return UINT32_MAX;
    }

    replay->current_sequence_id = seq_id;
    replay->recording_sequence = true;
    replay->num_sequences++;

    replay->stats.sequences_stored = replay->num_sequences;

    nimcp_mutex_unlock(replay->mutex);
    return seq_id;
}

int temporal_replay_end_sequence(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_end_sequence", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    nimcp_mutex_lock(replay->mutex);

    if (!replay->recording_sequence) {
        nimcp_mutex_unlock(replay->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "not currently recording a sequence");
    }

    replay->recording_sequence = false;

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_update_priority(temporal_replay_t* replay,
                                     uint32_t index,
                                     float priority) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_update_priority", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(index < replay->count, NIMCP_ERROR_INVALID_PARAM, "index out of range");

    nimcp_mutex_lock(replay->mutex);

    replay->transitions[index].td_error = priority;

    if (replay->config.use_priority_tree) {
        float tree_priority = powf(priority, replay->config.priority_alpha);
        update_priority_tree(replay, index, tree_priority);
    }

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_update_priorities(temporal_replay_t* replay,
                                       const uint32_t* indices,
                                       const float* priorities,
                                       uint32_t batch_size) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_update_priorities", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(indices, NIMCP_ERROR_INVALID_PARAM, "indices is NULL");
    NIMCP_CHECK_THROW(priorities, NIMCP_ERROR_INVALID_PARAM, "priorities is NULL");
    NIMCP_CHECK_THROW(batch_size > 0, NIMCP_ERROR_INVALID_PARAM, "batch_size must be > 0");

    for (uint32_t i = 0; i < batch_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && batch_size > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)batch_size);
        }

        int ret = temporal_replay_update_priority(replay, indices[i], priorities[i]);
        if (ret != NIMCP_SUCCESS) {
            return ret;
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Sampling API
 * ============================================================================ */

int temporal_replay_sample(temporal_replay_t* replay,
                            replay_mode_t mode,
                            uint32_t batch_size,
                            replay_batch_t* batch) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_sample", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(batch, NIMCP_ERROR_INVALID_PARAM, "batch output is NULL");
    NIMCP_CHECK_THROW(batch_size > 0, NIMCP_ERROR_INVALID_PARAM, "batch_size must be > 0");
    NIMCP_CHECK_THROW(replay->count > 0, NIMCP_ERROR_INVALID_PARAM, "replay buffer is empty");

    nimcp_mutex_lock(replay->mutex);

    uint32_t actual_batch = (batch_size > replay->count) ? replay->count : batch_size;
    float total_p = replay->total_priority;
    float min_p = REPLAY_MIN_PRIORITY;

    if (replay->config.use_priority_tree && total_p > 0.0f) {
        min_p = replay->priority_tree[replay->config.capacity];
        for (uint32_t i = 1; i < replay->count; i++) {
            float p = replay->priority_tree[replay->config.capacity + i];
            if (p > 0.0f && p < min_p) {
                min_p = p;
            }
        }
    }

    for (uint32_t i = 0; i < actual_batch; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_batch > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)actual_batch);
        }

        uint32_t idx = 0;

        if (mode == REPLAY_MODE_PRIORITY && replay->config.use_priority_tree && total_p > 0.0f) {
            float segment = total_p / actual_batch;
            float value = (i + ((float)nimcp_tl_rand() / RAND_MAX)) * segment;
            idx = sample_from_priority_tree(replay, value);
        } else if (mode == REPLAY_MODE_RANDOM) {
            idx = (uint32_t)(((float)nimcp_tl_rand() / RAND_MAX) * replay->count);
        } else {
            idx = i % replay->count;
        }

        if (idx >= replay->count) {
            idx = replay->count - 1;
        }

        replay_transition_t* t = get_transition(replay, idx);

        float* dest_state = batch->states + (size_t)i * replay->config.state_dim;
        memcpy(dest_state, t->state, replay->config.state_dim * sizeof(float));

        if (batch->actions && t->action && replay->config.action_dim > 0) {
            float* dest_action = batch->actions + (size_t)i * replay->config.action_dim;
            memcpy(dest_action, t->action, replay->config.action_dim * sizeof(float));
        }

        if (batch->next_states && t->next_state) {
            float* dest_next = batch->next_states + (size_t)i * replay->config.state_dim;
            memcpy(dest_next, t->next_state, replay->config.state_dim * sizeof(float));
        }

        if (batch->rewards) {
            batch->rewards[i] = t->reward;
        }

        if (batch->indices) {
            batch->indices[i] = idx;
        }

        if (batch->is_weights && mode == REPLAY_MODE_PRIORITY && total_p > 0.0f) {
            float p = replay->priority_tree[replay->config.capacity + idx];
            float prob = p / total_p;
            float weight = powf(replay->count * prob, -replay->config.is_beta);
            float max_weight = powf(replay->count * (min_p / total_p), -replay->config.is_beta);
            batch->is_weights[i] = weight / (fabsf(max_weight) > 1e-7f ? max_weight : 1e-7f);
        } else if (batch->is_weights) {
            batch->is_weights[i] = 1.0f;
        }
    }

    batch->batch_size = actual_batch;
    batch->is_sequence = false;

    replay->stats.total_samples += actual_batch;
    if (should_use_gpu(replay, actual_batch)) {
        replay->stats.gpu_samples += actual_batch;
    } else {
        replay->stats.cpu_samples += actual_batch;
    }

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_sample_sequence(temporal_replay_t* replay,
                                     uint32_t sequence_length,
                                     replay_batch_t* batch) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_sample_sequence", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(batch, NIMCP_ERROR_INVALID_PARAM, "batch output is NULL");
    NIMCP_CHECK_THROW(sequence_length > 0, NIMCP_ERROR_INVALID_PARAM, "sequence_length must be > 0");
    NIMCP_CHECK_THROW(replay->count > 0, NIMCP_ERROR_INVALID_PARAM, "replay buffer is empty");

    nimcp_mutex_lock(replay->mutex);

    uint32_t actual_length = (sequence_length > replay->count) ? replay->count : sequence_length;
    uint32_t max_start = replay->count - actual_length;
    uint32_t start_idx = (uint32_t)(((float)nimcp_tl_rand() / RAND_MAX) * max_start);

    for (uint32_t i = 0; i < actual_length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_length > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)actual_length);
        }

        uint32_t idx = (start_idx + i) % replay->config.capacity;
        replay_transition_t* t = get_transition(replay, idx);

        float* dest_state = batch->states + (size_t)i * replay->config.state_dim;
        memcpy(dest_state, t->state, replay->config.state_dim * sizeof(float));

        if (batch->rewards) {
            batch->rewards[i] = t->reward;
        }
        if (batch->indices) {
            batch->indices[i] = idx;
        }
        if (batch->is_weights) {
            batch->is_weights[i] = 1.0f;
        }
    }

    batch->batch_size = actual_length;
    batch->is_sequence = true;

    replay->stats.total_samples += actual_length;

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Replay Sweep API
 * ============================================================================ */

int temporal_replay_forward_sweep(temporal_replay_t* replay,
                                   uint32_t start_idx,
                                   uint32_t length,
                                   replay_sweep_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_forward_sweep", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result output is NULL");
    NIMCP_CHECK_THROW(length > 0, NIMCP_ERROR_INVALID_PARAM, "length must be > 0");

    nimcp_mutex_lock(replay->mutex);

    if (start_idx >= replay->count) {
        nimcp_mutex_unlock(replay->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "start_idx out of range");
    }

    uint32_t actual_length = length;
    if (start_idx + length > replay->count) {
        actual_length = replay->count - start_idx;
    }

    for (uint32_t i = 0; i < actual_length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_length > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)actual_length);
        }

        uint32_t idx = (start_idx + i) % replay->config.capacity;
        replay_transition_t* t = get_transition(replay, idx);

        memcpy(result->states[i], t->state, replay->config.state_dim * sizeof(float));
        result->timestamps[i] = t->timestamp;
        result->rewards[i] = t->reward;
    }

    result->length = actual_length;
    result->mode = REPLAY_MODE_FORWARD;
    result->compression_ratio = replay->config.compression_ratio;

    replay->stats.forward_sweeps++;

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_backward_sweep(temporal_replay_t* replay,
                                    uint32_t end_idx,
                                    uint32_t length,
                                    replay_sweep_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_backward_sweep", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result output is NULL");
    NIMCP_CHECK_THROW(length > 0, NIMCP_ERROR_INVALID_PARAM, "length must be > 0");

    nimcp_mutex_lock(replay->mutex);

    if (end_idx >= replay->count) {
        nimcp_mutex_unlock(replay->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "end_idx out of range");
    }

    uint32_t actual_length = length;
    if (length > end_idx + 1) {
        actual_length = end_idx + 1;
    }

    for (uint32_t i = 0; i < actual_length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && actual_length > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)actual_length);
        }

        uint32_t idx = end_idx - i;
        replay_transition_t* t = get_transition(replay, idx);

        memcpy(result->states[i], t->state, replay->config.state_dim * sizeof(float));
        result->timestamps[i] = t->timestamp;
        result->rewards[i] = t->reward;
    }

    result->length = actual_length;
    result->mode = REPLAY_MODE_BACKWARD;
    result->compression_ratio = replay->config.compression_ratio;

    replay->stats.backward_sweeps++;

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_replay_sequence(temporal_replay_t* replay,
                                     uint32_t sequence_id,
                                     replay_mode_t mode,
                                     replay_sweep_result_t* result) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_replay_sequence", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(result, NIMCP_ERROR_INVALID_PARAM, "result output is NULL");
    NIMCP_CHECK_THROW(sequence_id < replay->num_sequences, NIMCP_ERROR_INVALID_PARAM, "sequence_id out of range");

    replay_sequence_t* seq = &replay->sequences[sequence_id];
    NIMCP_CHECK_THROW(seq->length > 0 && seq->transition_indices, NIMCP_ERROR_NOT_FOUND, "sequence is empty or invalid");

    if (mode == REPLAY_MODE_FORWARD) {
        return temporal_replay_forward_sweep(replay, seq->transition_indices[0],
                                              seq->length, result);
    } else if (mode == REPLAY_MODE_BACKWARD) {
        return temporal_replay_backward_sweep(replay,
                                               seq->transition_indices[seq->length - 1],
                                               seq->length, result);
    }

    NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_PARAM, "invalid replay mode");
}

int temporal_replay_next(temporal_replay_t* replay,
                          float* state,
                          uint64_t* timestamp) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_next", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_INVALID_PARAM, "state output is NULL");

    nimcp_mutex_lock(replay->mutex);

    if (replay->seq_state == REPLAY_SEQ_IDLE) {
        nimcp_mutex_unlock(replay->mutex);
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "replay is idle");
    }

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_pause(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_pause", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    nimcp_mutex_lock(replay->mutex);

    if (replay->seq_state == REPLAY_SEQ_FORWARD ||
        replay->seq_state == REPLAY_SEQ_BACKWARD) {
        replay->seq_state = REPLAY_SEQ_PAUSED;
    }

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_resume(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_resume", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    nimcp_mutex_lock(replay->mutex);

    if (replay->seq_state == REPLAY_SEQ_PAUSED) {
        replay->seq_state = REPLAY_SEQ_FORWARD;
    }

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

int temporal_replay_stop(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_stop", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    nimcp_mutex_lock(replay->mutex);
    replay->seq_state = REPLAY_SEQ_IDLE;
    replay->replay_position = 0;
    nimcp_mutex_unlock(replay->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * GPU API
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA
int temporal_replay_init_gpu(temporal_replay_t* replay,
                              struct nimcp_gpu_context_s* gpu_ctx) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_init_gpu", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    if (gpu_ctx) {
        replay->gpu_ctx = gpu_ctx;
    } else {
        replay->gpu_ctx = nimcp_gpu_context_create_auto();
        NIMCP_CHECK_THROW(replay->gpu_ctx, NIMCP_ERROR_GPU_NOT_AVAILABLE, "failed to create GPU context");
    }

    replay->gpu_initialized = true;
    NIMCP_LOG_INFO("GPU initialized for temporal replay");
    return NIMCP_SUCCESS;
}

int temporal_replay_sync_to_gpu(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_sync_to_gpu", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(replay->gpu_initialized, NIMCP_ERROR_INVALID_PARAM, "GPU not initialized");
    return NIMCP_SUCCESS;
}

bool temporal_replay_has_gpu(const temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_has_gpu", 0.0f);


    return replay && replay->gpu_initialized;
}
#endif

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int temporal_replay_get_stats(const temporal_replay_t* replay,
                               replay_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_get_stats", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats output is NULL");
    memcpy(stats, &replay->stats, sizeof(replay_stats_t));
    return NIMCP_SUCCESS;
}

int temporal_replay_reset_stats(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");

    nimcp_mutex_lock(replay->mutex);

    uint32_t stored = replay->stats.transitions_stored;
    uint32_t sequences = replay->stats.sequences_stored;
    float used = replay->stats.capacity_used;

    memset(&replay->stats, 0, sizeof(replay_stats_t));

    replay->stats.transitions_stored = stored;
    replay->stats.sequences_stored = sequences;
    replay->stats.capacity_used = used;

    nimcp_mutex_unlock(replay->mutex);
    return NIMCP_SUCCESS;
}

uint32_t temporal_replay_count(const temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_count", 0.0f);


    return replay ? replay->count : 0;
}

uint32_t temporal_replay_capacity(const temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_capacity", 0.0f);


    return replay ? replay->config.capacity : 0;
}

bool temporal_replay_is_full(const temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_is_full", 0.0f);


    return replay && replay->count >= replay->config.capacity;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int temporal_replay_connect_bio_async(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    NIMCP_LOG_INFO("Bio-async connection (stub)");
    return NIMCP_SUCCESS;
}

int temporal_replay_disconnect_bio_async(temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(replay, NIMCP_ERROR_INVALID_PARAM, "replay buffer is NULL");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Result Management API
 * ============================================================================ */

replay_batch_t* replay_batch_create(uint32_t batch_size,
                                     uint32_t state_dim,
                                     uint32_t action_dim) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_replay_batch_create", 0.0f);


    if (batch_size == 0 || state_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_replay_disconnect_bio_async: batch_size is zero");
        return NULL;
    }

    replay_batch_t* batch = nimcp_calloc(1, sizeof(replay_batch_t));
    if (!batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate batch");

        return NULL;
    }

    batch->states = nimcp_calloc(batch_size * state_dim, sizeof(float));
    if (!batch->states) return NULL;
    batch->rewards = nimcp_calloc(batch_size, sizeof(float));
    if (!batch->rewards) return NULL;
    batch->is_weights = nimcp_calloc(batch_size, sizeof(float));
    if (!batch->is_weights) return NULL;
    batch->indices = nimcp_calloc(batch_size, sizeof(uint32_t));

    if (!batch->states || !batch->rewards || !batch->is_weights || !batch->indices) {
        replay_batch_destroy(batch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_replay_disconnect_bio_async: required parameter is NULL (batch->states, batch->rewards, batch->is_weights, batch->indices)");
        return NULL;
    }

    if (action_dim > 0) {
        batch->actions = nimcp_calloc(batch_size * action_dim, sizeof(float));
        if (!batch->actions) return NULL;
        batch->next_states = nimcp_calloc(batch_size * state_dim, sizeof(float));
    }

    batch->batch_size = batch_size;
    return batch;
}

void replay_batch_destroy(replay_batch_t* batch) {
    if (!batch) {
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_replay_batch_destroy", 0.0f);


    if (batch->states) nimcp_free(batch->states);
    if (batch->actions) nimcp_free(batch->actions);
    if (batch->next_states) nimcp_free(batch->next_states);
    if (batch->rewards) nimcp_free(batch->rewards);
    if (batch->is_weights) nimcp_free(batch->is_weights);
    if (batch->indices) nimcp_free(batch->indices);
    nimcp_free(batch);
    batch = NULL;
}

replay_sweep_result_t* replay_sweep_result_create(uint32_t max_length,
                                                   uint32_t state_dim) {
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_replay_sweep_result_", 0.0f);


    if (max_length == 0 || state_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "replay_batch_destroy: max_length is zero");
        return NULL;
    }

    replay_sweep_result_t* result = nimcp_calloc(1, sizeof(replay_sweep_result_t));
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate result");

        return NULL;
    }

    result->states = nimcp_calloc(max_length, sizeof(float*));
    result->timestamps = nimcp_calloc(max_length, sizeof(uint64_t));
    if (!result->timestamps) return NULL;
    result->rewards = nimcp_calloc(max_length, sizeof(float));

    if (!result->states || !result->timestamps || !result->rewards) {
        replay_sweep_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "replay_batch_destroy: required parameter is NULL (result->states, result->timestamps, result->rewards)");
        return NULL;
    }

    for (uint32_t i = 0; i < max_length; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && max_length > 256) {
            temporal_replay_heartbeat("temporal_rep_loop",
                             (float)(i + 1) / (float)max_length);
        }

        result->states[i] = nimcp_calloc(state_dim, sizeof(float));
        if (!result->states[i]) {
            result->length = i;
            replay_sweep_result_destroy(result);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "replay_batch_destroy: result->states is NULL");
            return NULL;
        }
    }

    result->length = max_length;
    return result;
}

void replay_sweep_result_destroy(replay_sweep_result_t* result) {
    if (!result) {
        return;
    }
    /* Phase 8: Heartbeat at operation start */
    temporal_replay_heartbeat("temporal_rep_replay_sweep_result_", 0.0f);


    if (result->states) {
        for (uint32_t i = 0; i < result->length; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->length > 256) {
                temporal_replay_heartbeat("temporal_rep_loop",
                                 (float)(i + 1) / (float)result->length);
            }

            if (result->states[i]) {
                nimcp_free(result->states[i]);
            }
        }
        nimcp_free(result->states);
    }
    if (result->timestamps) nimcp_free(result->timestamps);
    if (result->rewards) nimcp_free(result->rewards);
    nimcp_free(result);
    result = NULL;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* replay_mode_to_string(replay_mode_t mode) {
    switch (mode) {
        case REPLAY_MODE_FORWARD: return "FORWARD";
        case REPLAY_MODE_BACKWARD: return "BACKWARD";
        case REPLAY_MODE_RANDOM: return "RANDOM";
        case REPLAY_MODE_PRIORITY: return "PRIORITY";
        case REPLAY_MODE_INTERLEAVED: return "INTERLEAVED";
        default: return "UNKNOWN";
    }
}

const char* replay_seq_state_to_string(replay_seq_state_t state) {
    switch (state) {
        case REPLAY_SEQ_IDLE: return "IDLE";
        case REPLAY_SEQ_FORWARD: return "FORWARD";
        case REPLAY_SEQ_BACKWARD: return "BACKWARD";
        case REPLAY_SEQ_PAUSED: return "PAUSED";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void temporal_replay_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_temporal_replay_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int temporal_replay_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "temporal_replay_training_begin: NULL argument");
        return -1;
    }
    temporal_replay_heartbeat_instance(NULL, "temporal_replay_training_begin", 0.0f);
    return 0;
}

int temporal_replay_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "temporal_replay_training_end: NULL argument");
        return -1;
    }
    temporal_replay_heartbeat_instance(NULL, "temporal_replay_training_end", 1.0f);
    return 0;
}

int temporal_replay_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "temporal_replay_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    temporal_replay_heartbeat_instance(NULL, "temporal_replay_training_step", progress);
    return 0;
}

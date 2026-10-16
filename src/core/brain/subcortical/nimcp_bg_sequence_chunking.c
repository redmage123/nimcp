//=============================================================================
// nimcp_bg_sequence_chunking.c - Sequence Chunking Implementation
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_sequence_chunking.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bgsc, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


//=============================================================================
// Internal Structure
//=============================================================================

struct bgsc_system {
    /* Chunks */
    bgsc_chunk_t* chunks;
    uint32_t num_chunks;
    uint32_t max_chunks;

    /* Execution state */
    uint32_t executing_chunk_id;        /**< Currently executing chunk */
    bool is_executing;

    /* Bidirectional state */
    float dopamine_level;
    float cortical_feedback;
    float current_reward_prediction;

    /* Configuration */
    bgsc_config_t config;

    /* Statistics */
    bgsc_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

void bgsc_default_config(bgsc_config_t* config) {
    if (!config) return;

    config->max_chunks = BGSC_MAX_CHUNKS;
    config->max_sequence_length = BGSC_MAX_SEQUENCE_LENGTH;
    config->learning_rate = 0.1f;
    config->decay_rate = 0.01f;
    config->automaticity_threshold = BGSC_AUTOMATICITY_THRESHOLD;
    config->min_reps_to_chunk = BGSC_MIN_REPETITIONS;
    config->enable_chunking = true;
    config->enable_early_termination = true;
}

bgsc_system_t* bgsc_create(const bgsc_config_t* config) {
    bgsc_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        bgsc_default_config(&cfg);
    }

    bgsc_system_t* system = nimcp_calloc(1, sizeof(bgsc_system_t));
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bgsc_system");
        return NULL;
    }

    system->config = cfg;
    system->max_chunks = cfg.max_chunks;
    system->dopamine_level = 0.5f;
    system->executing_chunk_id = UINT32_MAX;

    /* Allocate chunks */
    system->chunks = nimcp_calloc(cfg.max_chunks, sizeof(bgsc_chunk_t));
    if (!system->chunks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate chunks");
        nimcp_free(system);
        return NULL;
    }

    /* Initialize chunks */
    for (uint32_t i = 0; i < cfg.max_chunks; i++) {
        system->chunks[i].chunk_id = UINT32_MAX;  /* Invalid = unused */
        system->chunks[i].max_length = cfg.max_sequence_length;
    }

    /* Create mutex */
    system->mutex = nimcp_mutex_create(NULL);
    if (!system->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex");
        nimcp_free(system->chunks);
        nimcp_free(system);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created sequence chunking system: max %u chunks, max %u actions/chunk",
                       cfg.max_chunks, cfg.max_sequence_length);

    return system;
}

void bgsc_destroy(bgsc_system_t* system) {
    if (!system) return;

    /* Free action sequences */
    for (uint32_t i = 0; i < system->max_chunks; i++) {
        if (system->chunks[i].actions) {
            nimcp_free(system->chunks[i].actions);
        }
    }

    if (system->mutex) {
        nimcp_mutex_free(system->mutex);
    }
    nimcp_free(system->chunks);
    nimcp_free(system);

    NIMCP_LOGGING_DEBUG("Destroyed sequence chunking system");
}

int bgsc_reset(bgsc_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);

    /* Reset all chunks */
    for (uint32_t i = 0; i < system->max_chunks; i++) {
        if (system->chunks[i].chunk_id != UINT32_MAX) {
            system->chunks[i].exec_state = BGSC_EXEC_IDLE;
            system->chunks[i].current_step = 0;
            system->chunks[i].current_initiation = 0.0f;
        }
    }

    system->is_executing = false;
    system->executing_chunk_id = UINT32_MAX;
    system->cortical_feedback = 0.0f;
    system->current_reward_prediction = 0.0f;
    memset(&system->stats, 0, sizeof(bgsc_stats_t));

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

//=============================================================================
// Chunk Registration Functions
//=============================================================================

int bgsc_register_chunk(bgsc_system_t* system,
                         const char* name,
                         uint32_t trigger_context,
                         uint32_t* chunk_id) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!chunk_id, NIMCP_ERROR_NULL_POINTER, "chunk_id is NULL");
    if (!system || !chunk_id) return -1;

    nimcp_mutex_lock(system->mutex);

    /* Find empty slot */
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < system->max_chunks; i++) {
        if (system->chunks[i].chunk_id == UINT32_MAX) {
            slot = i;
            break;
        }
    }

    if (slot == UINT32_MAX) {
        NIMCP_LOGGING_WARN("No free chunk slots");
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgsc_reset: validation failed");
        return -1;
    }

    bgsc_chunk_t* chunk = &system->chunks[slot];
    chunk->chunk_id = slot;
    if (name) {
        strncpy(chunk->name, name, sizeof(chunk->name) - 1);
        chunk->name[sizeof(chunk->name) - 1] = '\0';
    }
    chunk->trigger_context = trigger_context;
    chunk->stage = BGSC_STAGE_NAIVE;
    chunk->automaticity = 0.0f;
    chunk->initiation_threshold = 0.5f;
    chunk->termination_threshold = 0.9f;
    chunk->exec_state = BGSC_EXEC_IDLE;
    chunk->sequence_length = 0;

    /* Allocate action sequence */
    chunk->actions = nimcp_calloc(system->config.max_sequence_length,
                                   sizeof(bgsc_sequence_action_t));
    if (!chunk->actions) {
        chunk->chunk_id = UINT32_MAX;
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgsc_reset: chunk->actions is NULL");
        return -1;
    }

    system->num_chunks++;
    system->stats.total_chunks++;
    *chunk_id = slot;

    NIMCP_LOGGING_DEBUG("Registered chunk %u: '%s' (context=%u)",
                        slot, name ? name : "unnamed", trigger_context);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_add_action(bgsc_system_t* system,
                     uint32_t chunk_id,
                     uint32_t action_id,
                     float expected_duration_ms) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || chunk_id >= system->max_chunks) return -1;

    nimcp_mutex_lock(system->mutex);

    bgsc_chunk_t* chunk = &system->chunks[chunk_id];
    if (chunk->chunk_id == UINT32_MAX) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgsc_reset: validation failed");
        return -1;
    }

    if (chunk->sequence_length >= chunk->max_length) {
        NIMCP_LOGGING_WARN("Chunk %u sequence full", chunk_id);
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgsc_reset: capacity exceeded");
        return -1;
    }

    uint32_t idx = chunk->sequence_length;
    chunk->actions[idx].action_id = action_id;
    chunk->actions[idx].expected_duration_ms = expected_duration_ms;
    chunk->actions[idx].transition_strength = 0.5f;  /* Initial */
    chunk->sequence_length++;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_set_initiation_threshold(bgsc_system_t* system,
                                   uint32_t chunk_id,
                                   float threshold) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || chunk_id >= system->max_chunks) return -1;

    nimcp_mutex_lock(system->mutex);
    system->chunks[chunk_id].initiation_threshold = fmaxf(0.0f, fminf(1.0f, threshold));
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

int bgsc_unregister_chunk(bgsc_system_t* system, uint32_t chunk_id) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || chunk_id >= system->max_chunks) return -1;

    nimcp_mutex_lock(system->mutex);

    bgsc_chunk_t* chunk = &system->chunks[chunk_id];
    if (chunk->actions) {
        nimcp_free(chunk->actions);
        chunk->actions = NULL;
    }
    chunk->chunk_id = UINT32_MAX;
    system->num_chunks--;

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

//=============================================================================
// Execution Functions
//=============================================================================

bool bgsc_check_trigger(bgsc_system_t* system,
                         uint32_t context,
                         uint32_t* chunk_id) {
    if (!system || !chunk_id) {
        return false;
    }

    nimcp_mutex_lock(system->mutex);

    for (uint32_t i = 0; i < system->max_chunks; i++) {
        bgsc_chunk_t* chunk = &system->chunks[i];
        if (chunk->chunk_id != UINT32_MAX &&
            chunk->trigger_context == context &&
            chunk->automaticity >= chunk->initiation_threshold) {
            *chunk_id = i;
            nimcp_mutex_unlock(system->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return false;
}

int bgsc_initiate(bgsc_system_t* system, uint32_t chunk_id) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || chunk_id >= system->max_chunks) return -1;

    nimcp_mutex_lock(system->mutex);
    bgsc_heartbeat("chunk_initiate", 0.0f);

    bgsc_chunk_t* chunk = &system->chunks[chunk_id];
    if (chunk->chunk_id == UINT32_MAX || chunk->sequence_length == 0) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgsc_initiate: chunk->sequence_length is zero");
        return -1;
    }

    /* Abort any currently executing chunk */
    if (system->is_executing && system->executing_chunk_id != UINT32_MAX) {
        system->chunks[system->executing_chunk_id].exec_state = BGSC_EXEC_ABORTED;
    }

    chunk->exec_state = BGSC_EXEC_INITIATED;
    chunk->current_step = 0;
    chunk->exec_start_time_ms = nimcp_time_get_ms();
    chunk->step_start_time_ms = chunk->exec_start_time_ms;
    chunk->progress = 0.0f;

    /* Calculate reward prediction based on history */
    chunk->reward_prediction = chunk->success_rate * chunk->automaticity;

    system->is_executing = true;
    system->executing_chunk_id = chunk_id;
    system->current_reward_prediction = chunk->reward_prediction;

    /* Provide feedback to cortex */
    system->cortical_feedback = chunk->automaticity;

    NIMCP_LOGGING_DEBUG("Initiated chunk %u '%s' (auto=%.2f)",
                        chunk_id, chunk->name, chunk->automaticity);

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_get_current_action(bgsc_system_t* system,
                             uint32_t* action_id,
                             float* urgency) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || !action_id) return -1;

    nimcp_mutex_lock(system->mutex);

    if (!system->is_executing || system->executing_chunk_id == UINT32_MAX) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgsc_initiate: system->is_executing is NULL");
        return -1;
    }

    bgsc_chunk_t* chunk = &system->chunks[system->executing_chunk_id];
    if (chunk->current_step >= chunk->sequence_length) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgsc_initiate: capacity exceeded");
        return -1;
    }

    *action_id = chunk->actions[chunk->current_step].action_id;
    if (urgency) {
        /* Urgency increases with automaticity */
        *urgency = 0.5f + chunk->automaticity * 0.5f;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_action_completed(bgsc_system_t* system,
                           uint32_t action_id,
                           bool success,
                           float duration_ms) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);

    if (!system->is_executing || system->executing_chunk_id == UINT32_MAX) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgsc_initiate: system->is_executing is NULL");
        return -1;
    }

    bgsc_chunk_t* chunk = &system->chunks[system->executing_chunk_id];
    uint32_t step = chunk->current_step;

    /* Verify this is the expected action */
    if (step < chunk->sequence_length &&
        chunk->actions[step].action_id == action_id) {

        /* Update action statistics */
        bgsc_sequence_action_t* act = &chunk->actions[step];
        act->times_executed++;
        float n = (float)act->times_executed;
        act->avg_duration_ms = (act->avg_duration_ms * (n - 1) + duration_ms) / n;

        if (success) {
            /* Strengthen transition */
            act->transition_strength = fminf(1.0f, act->transition_strength + 0.05f);
        } else {
            /* Weaken transition */
            act->transition_strength = fmaxf(0.0f, act->transition_strength - 0.1f);
        }

        /* Advance to next step */
        chunk->current_step++;
        chunk->step_start_time_ms = nimcp_time_get_ms();
        chunk->progress = (float)chunk->current_step / (float)chunk->sequence_length;

        /* Update execution state */
        if (chunk->current_step >= chunk->sequence_length) {
            /* Sequence completed */
            chunk->exec_state = BGSC_EXEC_TERMINATED;
            chunk->last_termination = BGSC_TERM_SEQUENCE_COMPLETE;
            chunk->total_executions++;
            chunk->successful_executions++;
            system->is_executing = false;
            system->executing_chunk_id = UINT32_MAX;
            system->stats.total_executions++;
            system->stats.successful_executions++;
        } else if (chunk->current_step == chunk->sequence_length - 1) {
            chunk->exec_state = BGSC_EXEC_COMPLETING;
        } else {
            chunk->exec_state = BGSC_EXEC_RUNNING;
        }

        /* Bidirectional feedback to cortex */
        system->cortical_feedback = chunk->progress;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_pause(bgsc_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || !system->is_executing) return -1;

    nimcp_mutex_lock(system->mutex);
    if (system->executing_chunk_id != UINT32_MAX) {
        system->chunks[system->executing_chunk_id].exec_state = BGSC_EXEC_PAUSED;
    }
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_resume(bgsc_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || !system->is_executing) return -1;

    nimcp_mutex_lock(system->mutex);
    if (system->executing_chunk_id != UINT32_MAX) {
        bgsc_chunk_t* chunk = &system->chunks[system->executing_chunk_id];
        if (chunk->exec_state == BGSC_EXEC_PAUSED) {
            chunk->exec_state = BGSC_EXEC_RUNNING;
            chunk->step_start_time_ms = nimcp_time_get_ms();
        }
    }
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_abort(bgsc_system_t* system) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);

    if (system->is_executing && system->executing_chunk_id != UINT32_MAX) {
        bgsc_chunk_t* chunk = &system->chunks[system->executing_chunk_id];
        chunk->exec_state = BGSC_EXEC_ABORTED;
        chunk->last_termination = BGSC_TERM_EXTERNAL_STOP;

        system->is_executing = false;
        system->executing_chunk_id = UINT32_MAX;
        system->cortical_feedback = -1.0f;  /* Negative feedback for abort */
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

bool bgsc_is_executing(const bgsc_system_t* system) {
    if (!system) {
        return false;
    }
    return system->is_executing;
}

float bgsc_get_progress(const bgsc_system_t* system) {
    if (!system || !system->is_executing) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float progress = 0.0f;
    if (system->executing_chunk_id != UINT32_MAX) {
        progress = system->chunks[system->executing_chunk_id].progress;
    }
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return progress;
}

//=============================================================================
// Bidirectional Data Flow Functions
//=============================================================================

int bgsc_process_bidir(bgsc_system_t* system, bgsc_bidir_data_t* data) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!data, NIMCP_ERROR_NULL_POINTER, "data is NULL");
    if (!system || !data) return -1;

    nimcp_mutex_lock(system->mutex);
    bgsc_heartbeat("process_bidir", 0.0f);

    /* Process inputs */
    system->dopamine_level = data->dopamine_level;

    if (data->action_completed && system->is_executing) {
        /* Handle completion inside lock - use internal logic */
        uint32_t chunk_id = system->executing_chunk_id;
        if (chunk_id != UINT32_MAX) {
            bgsc_chunk_t* chunk = &system->chunks[chunk_id];
            uint32_t step = chunk->current_step;

            if (step < chunk->sequence_length &&
                chunk->actions[step].action_id == data->completed_action_id) {
                chunk->actions[step].times_executed++;
                chunk->current_step++;
                chunk->progress = (float)chunk->current_step / (float)chunk->sequence_length;
                system->cortical_feedback = chunk->progress;  /* Update feedback */

                if (chunk->current_step >= chunk->sequence_length) {
                    chunk->exec_state = BGSC_EXEC_TERMINATED;
                    chunk->last_termination = BGSC_TERM_SEQUENCE_COMPLETE;
                    chunk->total_executions++;
                    chunk->successful_executions++;

                    /* Update automaticity with dopamine modulation */
                    float da_boost = (system->dopamine_level - 0.5f) * 0.2f;
                    chunk->automaticity = fminf(1.0f,
                        chunk->automaticity + system->config.learning_rate + da_boost);

                    /* Update success rate */
                    float n = (float)chunk->total_executions;
                    chunk->success_rate = (float)chunk->successful_executions / n;

                    /* Progress stage if needed */
                    if (chunk->stage == BGSC_STAGE_NAIVE && chunk->total_executions >= 3) {
                        chunk->stage = BGSC_STAGE_LEARNING;
                    } else if (chunk->stage == BGSC_STAGE_LEARNING &&
                               chunk->automaticity >= system->config.automaticity_threshold) {
                        chunk->stage = BGSC_STAGE_CHUNKED;
                        system->stats.automatized_chunks++;
                    }

                    system->is_executing = false;
                    system->executing_chunk_id = UINT32_MAX;
                    system->stats.total_executions++;
                    system->stats.successful_executions++;
                }
            }
        }
    }

    if (data->external_stop && system->is_executing) {
        if (system->executing_chunk_id != UINT32_MAX) {
            system->chunks[system->executing_chunk_id].exec_state = BGSC_EXEC_ABORTED;
            system->chunks[system->executing_chunk_id].last_termination = BGSC_TERM_EXTERNAL_STOP;
        }
        system->is_executing = false;
        system->executing_chunk_id = UINT32_MAX;
    }

    /* Generate outputs */
    data->chunk_active = system->is_executing;
    data->progress_feedback = system->cortical_feedback;
    data->reward_prediction = system->current_reward_prediction;

    if (system->is_executing && system->executing_chunk_id != UINT32_MAX) {
        bgsc_chunk_t* chunk = &system->chunks[system->executing_chunk_id];
        if (chunk->current_step < chunk->sequence_length) {
            data->requested_action = chunk->actions[chunk->current_step].action_id;
            data->action_urgency = 0.5f + chunk->automaticity * 0.5f;
        }
    } else {
        data->requested_action = UINT32_MAX;
        data->action_urgency = 0.0f;
    }

    bgsc_heartbeat("process_bidir", 1.0f);
    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_set_dopamine(bgsc_system_t* system, float dopamine) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);
    system->dopamine_level = fmaxf(0.0f, fminf(1.0f, dopamine));
    nimcp_mutex_unlock(system->mutex);

    return 0;
}

float bgsc_get_cortical_feedback(const bgsc_system_t* system) {
    if (!system) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float fb = system->cortical_feedback;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return fb;
}

float bgsc_get_reward_prediction(const bgsc_system_t* system) {
    if (!system) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float rp = system->current_reward_prediction;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return rp;
}

//=============================================================================
// Learning Functions
//=============================================================================

int bgsc_learn_sequence(bgsc_system_t* system,
                         const uint32_t* actions,
                         uint32_t num_actions,
                         float reward) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!actions, NIMCP_ERROR_NULL_POINTER, "actions is NULL");
    if (!system || !actions || num_actions == 0) return -1;
    if (!system->config.enable_chunking) return 0;

    nimcp_mutex_lock(system->mutex);

    /* Check if sequence matches existing chunk */
    for (uint32_t i = 0; i < system->max_chunks; i++) {
        bgsc_chunk_t* chunk = &system->chunks[i];
        if (chunk->chunk_id == UINT32_MAX) continue;
        if (chunk->sequence_length != num_actions) continue;

        bool match = true;
        for (uint32_t j = 0; j < num_actions && match; j++) {
            if (chunk->actions[j].action_id != actions[j]) {
                match = false;
            }
        }

        if (match) {
            /* Strengthen existing chunk */
            chunk->total_executions++;
            if (reward > 0) {
                chunk->successful_executions++;
                float da_boost = (system->dopamine_level - 0.5f) * 0.2f;
                chunk->automaticity = fminf(1.0f,
                    chunk->automaticity + system->config.learning_rate * reward + da_boost);
            }
            chunk->success_rate = (float)chunk->successful_executions /
                                   (float)chunk->total_executions;

            nimcp_mutex_unlock(system->mutex);
            return 0;
        }
    }

    /* Create new chunk if reward is positive and sequence is long enough */
    if (reward > 0.5f && num_actions >= 2) {
        uint32_t new_id;
        nimcp_mutex_unlock(system->mutex);

        if (bgsc_register_chunk(system, "learned", 0, &new_id) == 0) {
            for (uint32_t i = 0; i < num_actions; i++) {
                bgsc_add_action(system, new_id, actions[i], 100.0f);
            }
            bgsc_strengthen_chunk(system, new_id, reward);
        }
        return 0;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_strengthen_chunk(bgsc_system_t* system, uint32_t chunk_id, float reward) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || chunk_id >= system->max_chunks) return -1;

    nimcp_mutex_lock(system->mutex);

    bgsc_chunk_t* chunk = &system->chunks[chunk_id];
    if (chunk->chunk_id == UINT32_MAX) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgsc_strengthen_chunk: validation failed");
        return -1;
    }

    /* Dopamine-modulated strengthening */
    float da_mod = 1.0f + (system->dopamine_level - 0.5f) * 2.0f;
    float delta = system->config.learning_rate * reward * da_mod;
    chunk->automaticity = fminf(1.0f, chunk->automaticity + delta);

    /* Strengthen all transitions */
    for (uint32_t i = 0; i < chunk->sequence_length; i++) {
        chunk->actions[i].transition_strength = fminf(1.0f,
            chunk->actions[i].transition_strength + delta * 0.5f);
    }

    /* Progress stage */
    if (chunk->stage == BGSC_STAGE_NAIVE) {
        chunk->stage = BGSC_STAGE_LEARNING;
    } else if (chunk->stage == BGSC_STAGE_LEARNING &&
               chunk->automaticity >= system->config.automaticity_threshold) {
        chunk->stage = BGSC_STAGE_CHUNKED;
        system->stats.automatized_chunks++;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

int bgsc_weaken_chunk(bgsc_system_t* system, uint32_t chunk_id) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system || chunk_id >= system->max_chunks) return -1;

    nimcp_mutex_lock(system->mutex);

    bgsc_chunk_t* chunk = &system->chunks[chunk_id];
    if (chunk->chunk_id == UINT32_MAX) {
        nimcp_mutex_unlock(system->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgsc_weaken_chunk: validation failed");
        return -1;
    }

    chunk->automaticity = fmaxf(0.0f, chunk->automaticity - system->config.decay_rate);

    /* Regress stage if needed */
    if (chunk->stage == BGSC_STAGE_CHUNKED &&
        chunk->automaticity < system->config.automaticity_threshold) {
        chunk->stage = BGSC_STAGE_DEGRADED;
        system->stats.automatized_chunks--;
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

float bgsc_get_automaticity(const bgsc_system_t* system, uint32_t chunk_id) {
    if (!system || chunk_id >= system->max_chunks) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    float auto_level = system->chunks[chunk_id].automaticity;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return auto_level;
}

bool bgsc_is_automatized(const bgsc_system_t* system, uint32_t chunk_id) {
    if (!system || chunk_id >= system->max_chunks) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);
    bool is_auto = system->chunks[chunk_id].stage == BGSC_STAGE_CHUNKED;
    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);

    return is_auto;
}

//=============================================================================
// Query Functions
//=============================================================================

const bgsc_chunk_t* bgsc_get_chunk(const bgsc_system_t* system, uint32_t chunk_id) {
    if (!system || chunk_id >= system->max_chunks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgsc_get_chunk: system is NULL");
        return NULL;
    }
    if (system->chunks[chunk_id].chunk_id == UINT32_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgsc_get_chunk: validation failed");
        return NULL;
    }
    return &system->chunks[chunk_id];
}

uint32_t bgsc_get_executing_chunk(const bgsc_system_t* system) {
    if (!system) return UINT32_MAX;
    return system->executing_chunk_id;
}

bgsc_exec_state_t bgsc_get_exec_state(const bgsc_system_t* system, uint32_t chunk_id) {
    if (!system || chunk_id >= system->max_chunks) return BGSC_EXEC_IDLE;
    return system->chunks[chunk_id].exec_state;
}

int bgsc_get_stats(const bgsc_system_t* system, bgsc_stats_t* stats) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    NIMCP_THROW_IF(!stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    if (!system || !stats) return -1;

    nimcp_mutex_lock((nimcp_mutex_t*)system->mutex);

    stats->total_chunks = system->num_chunks;
    stats->active_chunks = system->is_executing ? 1 : 0;
    stats->automatized_chunks = system->stats.automatized_chunks;
    stats->total_executions = system->stats.total_executions;
    stats->successful_executions = system->stats.successful_executions;

    /* Calculate averages */
    float total_len = 0.0f, total_auto = 0.0f;
    uint32_t count = 0;
    for (uint32_t i = 0; i < system->max_chunks; i++) {
        if (system->chunks[i].chunk_id != UINT32_MAX) {
            total_len += system->chunks[i].sequence_length;
            total_auto += system->chunks[i].automaticity;
            count++;
        }
    }
    stats->avg_sequence_length = count > 0 ? total_len / count : 0.0f;
    stats->avg_automaticity = count > 0 ? total_auto / count : 0.0f;

    nimcp_mutex_unlock((nimcp_mutex_t*)system->mutex);
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int bgsc_step(bgsc_system_t* system, float dt_ms) {
    NIMCP_THROW_IF(!system, NIMCP_ERROR_NULL_POINTER, "system is NULL");
    if (!system) return -1;

    nimcp_mutex_lock(system->mutex);

    /* Check for timeout on executing chunk */
    if (system->is_executing && system->executing_chunk_id != UINT32_MAX) {
        bgsc_chunk_t* chunk = &system->chunks[system->executing_chunk_id];
        uint64_t now = nimcp_time_get_ms();
        uint64_t elapsed = now - chunk->step_start_time_ms;

        if (chunk->current_step < chunk->sequence_length) {
            float expected = chunk->actions[chunk->current_step].expected_duration_ms;
            if (elapsed > expected * 3.0f) {
                /* Timeout - action taking too long */
                if (system->config.enable_early_termination) {
                    chunk->exec_state = BGSC_EXEC_TERMINATED;
                    chunk->last_termination = BGSC_TERM_TIMEOUT;
                    chunk->total_executions++;
                    /* Don't count as successful */
                    system->is_executing = false;
                    system->executing_chunk_id = UINT32_MAX;
                    system->stats.total_executions++;
                    NIMCP_LOGGING_DEBUG("Chunk %u timed out at step %u",
                                        chunk->chunk_id, chunk->current_step);
                }
            }
        }
    }

    /* Decay unused chunks */
    for (uint32_t i = 0; i < system->max_chunks; i++) {
        bgsc_chunk_t* chunk = &system->chunks[i];
        if (chunk->chunk_id != UINT32_MAX && chunk->exec_state == BGSC_EXEC_IDLE) {
            chunk->automaticity = fmaxf(0.0f,
                chunk->automaticity - system->config.decay_rate * dt_ms / 1000.0f);
        }
    }

    nimcp_mutex_unlock(system->mutex);
    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* bgsc_stage_name(bgsc_stage_t stage) {
    switch (stage) {
        case BGSC_STAGE_NAIVE: return "Naive";
        case BGSC_STAGE_LEARNING: return "Learning";
        case BGSC_STAGE_CONSOLIDATING: return "Consolidating";
        case BGSC_STAGE_CHUNKED: return "Chunked";
        case BGSC_STAGE_DEGRADED: return "Degraded";
        default: return "Unknown";
    }
}

const char* bgsc_exec_state_name(bgsc_exec_state_t state) {
    switch (state) {
        case BGSC_EXEC_IDLE: return "Idle";
        case BGSC_EXEC_INITIATED: return "Initiated";
        case BGSC_EXEC_RUNNING: return "Running";
        case BGSC_EXEC_PAUSED: return "Paused";
        case BGSC_EXEC_COMPLETING: return "Completing";
        case BGSC_EXEC_TERMINATED: return "Terminated";
        case BGSC_EXEC_ABORTED: return "Aborted";
        default: return "Unknown";
    }
}

const char* bgsc_termination_name(bgsc_termination_t term) {
    switch (term) {
        case BGSC_TERM_SEQUENCE_COMPLETE: return "Sequence Complete";
        case BGSC_TERM_GOAL_ACHIEVED: return "Goal Achieved";
        case BGSC_TERM_TIMEOUT: return "Timeout";
        case BGSC_TERM_EXTERNAL_STOP: return "External Stop";
        case BGSC_TERM_ERROR: return "Error";
        default: return "Unknown";
    }
}

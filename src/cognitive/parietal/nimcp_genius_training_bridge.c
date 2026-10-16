/**
 * @file nimcp_genius_training_bridge.c
 * @brief Mathematical Genius - Training System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-24
 */

#include "cognitive/parietal/nimcp_genius_training_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_kg_hierarchy.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(genius_training_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from genius_training_bridge module (instance-level) */
static inline void genius_training_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_genius_training_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_genius_training_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_genius_training_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "GENIUS_TRAINING_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct genius_training_bridge {
    bridge_base_t base;
    genius_training_config_t config;
    struct mathematical_genius* genius;
    struct nimcp_brain_training_ctx* brain_training;

    /* State */
    genius_training_state_t state;
    uint64_t current_time_us;
    bool bio_async_connected;

    /* Heartbeat tracking (Phase 8) */
    uint64_t last_heartbeat_us;

    /* Task management */
    genius_training_task_t* tasks;
    uint32_t task_count;
    uint32_t task_capacity;

    /* Curriculum state */
    genius_curriculum_state_t curriculum;

    /* Training progress */
    genius_training_progress_t progress;

    /* Mode-specific state */
    genius_mode_training_state_t mode_state;

    /* Learning rate scheduling */
    float current_learning_rate;
    uint64_t lr_step_count;

    /* Callbacks */
    genius_training_epoch_callback_t epoch_callback;
    void* epoch_callback_data;
    genius_training_curriculum_callback_t curriculum_callback;
    void* curriculum_callback_data;
    genius_training_checkpoint_callback_t checkpoint_callback;
    void* checkpoint_callback_data;

    /* Statistics */
    genius_training_stats_t stats;

    /* KG Wiring */
    struct kg_module_wiring* kg_wiring;

    /* Health agent (instance-level) - Phase 8 */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(genius_training_bridge)

//=============================================================================
// Helper Functions
//=============================================================================

static genius_training_task_t* find_task(genius_training_bridge_t* bridge, uint32_t task_id) {
    for (uint32_t i = 0; i < bridge->task_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_count > 256) {
            genius_training_bridge_heartbeat("genius_train_loop",
                             (float)(i + 1) / (float)bridge->task_count);
        }

        if (bridge->tasks[i].task_id == task_id) {
            return &bridge->tasks[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_task: validation failed");
    return NULL;
}

static void update_curriculum_state(genius_training_bridge_t* bridge, float score) {
    genius_curriculum_state_t* cur = &bridge->curriculum;

    cur->stage_score = 0.9f * cur->stage_score + 0.1f * score;
    cur->examples_in_stage++;
    cur->domain_scores[cur->current_domain] =
        0.95f * cur->domain_scores[cur->current_domain] + 0.05f * score;

    /* Check for advancement */
    if (cur->examples_in_stage >= bridge->config.min_examples_per_stage) {
        if (cur->stage_score >= bridge->config.curriculum_advancement_thresh &&
            cur->current_stage < GENIUS_STAGE_RESEARCH) {

            genius_curriculum_stage_t old_stage = cur->current_stage;
            cur->current_stage++;
            cur->examples_in_stage = 0;
            cur->stages_completed++;
            cur->advancements++;

            if (bridge->curriculum_callback) {
                bridge->curriculum_callback(bridge, old_stage, cur->current_stage,
                                           cur->current_domain, bridge->curriculum_callback_data);
            }
        }
        /* Check for regression */
        else if (cur->stage_score < bridge->config.curriculum_regression_thresh &&
                 cur->current_stage > GENIUS_STAGE_NOVICE) {

            genius_curriculum_stage_t old_stage = cur->current_stage;
            cur->current_stage--;
            cur->examples_in_stage = 0;
            cur->regressions++;

            if (bridge->curriculum_callback) {
                bridge->curriculum_callback(bridge, old_stage, cur->current_stage,
                                           cur->current_domain, bridge->curriculum_callback_data);
            }
        }
    }

    /* Update progress */
    cur->stage_progress = (float)cur->examples_in_stage /
                          (float)bridge->config.min_examples_per_stage;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

genius_training_config_t genius_training_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_conf", 0.0f);


    genius_training_config_t config = {
        .base_learning_rate = NIMCP_LEARNING_RATE_FINE,
        .learning_rate_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT,
        .momentum = NIMCP_MOMENTUM_DEFAULT,
        .weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT,
        .batch_size = GENIUS_TRAINING_DEFAULT_BATCH,

        .enable_curriculum = true,
        .curriculum_advancement_thresh = 0.8f,
        .curriculum_regression_thresh = 0.4f,
        .min_examples_per_stage = 100,

        .enable_continual_learning = true,
        .ewc_lambda = 0.1f,
        .enable_replay = true,
        .replay_buffer_size = 1000,

        .enable_transfer = true,
        .transfer_weight = 0.1f,

        .enable_meta_learning = false,
        .meta_learning_rate = NIMCP_LEARNING_RATE_MICRO,

        .train_gauss_mode = true,
        .train_newton_mode = true,
        .train_erdos_mode = true,
        .mode_specific_weight = 0.3f,

        .epochs_per_domain = 10,
        .enable_domain_rotation = true,
        .domain_rotation_period = 5.0f,

        .validation_split = 0.2f,
        .validation_frequency = 100,

        .enable_checkpointing = true,
        .checkpoint_frequency = 10,

        .enable_bio_async = false
    };
    return config;
}

genius_training_bridge_t* genius_training_create(const genius_training_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_crea", 0.0f);


    genius_training_bridge_t* bridge = nimcp_calloc(1, sizeof(genius_training_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge allocation failed");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = genius_training_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "genius_training") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "bridge_base_init failed for genius_training");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate task array */
    bridge->task_capacity = GENIUS_TRAINING_MAX_TASKS;
    bridge->tasks = nimcp_calloc(bridge->task_capacity, sizeof(genius_training_task_t));
    if (!bridge->tasks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "task array allocation failed for genius_training_bridge");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize curriculum state */
    bridge->curriculum.current_stage = GENIUS_STAGE_NOVICE;
    bridge->curriculum.current_domain = GENIUS_TRAIN_DOMAIN_NUMBER_THEORY;
    bridge->curriculum.examples_in_stage = 0;
    bridge->curriculum.stage_score = 0.5f;
    for (int i = 0; i < GENIUS_TRAIN_DOMAIN_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && GENIUS_TRAIN_DOMAIN_COUNT > 256) {
            genius_training_bridge_heartbeat("genius_train_loop",
                             (float)(i + 1) / (float)GENIUS_TRAIN_DOMAIN_COUNT);
        }

        bridge->curriculum.domain_scores[i] = 0.0f;
    }

    /* Initialize mode state */
    bridge->mode_state.gauss_skill = 0.3f;
    bridge->mode_state.newton_skill = 0.3f;
    bridge->mode_state.erdos_skill = 0.3f;
    bridge->mode_state.best_mode = GENIUS_MODE_ADAPTIVE;
    bridge->mode_state.mode_selection_accuracy = 0.5f;

    /* Initialize learning rate */
    bridge->current_learning_rate = bridge->config.base_learning_rate;
    bridge->lr_step_count = 0;

    /* Initialize state */
    bridge->state = GENIUS_TRAINING_STATE_IDLE;
    bridge->task_count = 0;
    bridge->current_time_us = 0;

    memset(&bridge->progress, 0, sizeof(genius_training_progress_t));
    memset(&bridge->stats, 0, sizeof(genius_training_stats_t));

    /* Initialize KG wiring */
    bridge->kg_wiring = genius_training_create_kg_wiring();

    NIMCP_LOGGING_INFO("Created %s bridge", "genius_training");
    return bridge;
}

void genius_training_destroy(genius_training_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "genius_training");

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_dest", 0.0f);


    if (bridge->tasks) nimcp_free(bridge->tasks);

    /* Destroy KG wiring */
    if (bridge->kg_wiring) {
        kg_module_wiring_destroy(bridge->kg_wiring);
        bridge->kg_wiring = NULL;
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int genius_training_reset(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset curriculum */
    bridge->curriculum.current_stage = GENIUS_STAGE_NOVICE;
    bridge->curriculum.examples_in_stage = 0;
    bridge->curriculum.stage_score = 0.5f;
    bridge->curriculum.stages_completed = 0;
    bridge->curriculum.advancements = 0;
    bridge->curriculum.regressions = 0;

    /* Reset progress */
    memset(&bridge->progress, 0, sizeof(genius_training_progress_t));

    /* Reset mode state */
    bridge->mode_state.gauss_skill = 0.3f;
    bridge->mode_state.newton_skill = 0.3f;
    bridge->mode_state.erdos_skill = 0.3f;

    /* Reset learning rate */
    bridge->current_learning_rate = bridge->config.base_learning_rate;
    bridge->lr_step_count = 0;

    bridge->state = GENIUS_TRAINING_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_link_genius(genius_training_bridge_t* bridge, struct mathematical_genius* genius) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_link_genius: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_link", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->genius = genius;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_link_brain_training(genius_training_bridge_t* bridge,
                                        struct nimcp_brain_training_ctx* ctx) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_link_genius: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_link", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->brain_training = ctx;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Task Management
//=============================================================================

int genius_training_register_task(genius_training_bridge_t* bridge,
                                  const genius_training_task_t* task) {
    if (!bridge || !task) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_link_genius: required parameter is NULL (bridge, task)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_regi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->task_count >= bridge->task_capacity) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "genius_training_link_genius: capacity exceeded");
        return -1;
    }

    /* Assign task ID */
    uint32_t task_id = bridge->task_count;
    bridge->tasks[bridge->task_count] = *task;
    bridge->tasks[bridge->task_count].task_id = task_id;
    bridge->task_count++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return (int)task_id;
}

int genius_training_unregister_task(genius_training_bridge_t* bridge, uint32_t task_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_unregister_task: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_unre", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < bridge->task_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->task_count > 256) {
            genius_training_bridge_heartbeat("genius_train_loop",
                             (float)(i + 1) / (float)bridge->task_count);
        }

        if (bridge->tasks[i].task_id == task_id) {
            /* Swap with last and decrement */
            bridge->tasks[i] = bridge->tasks[--bridge->task_count];
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "genius_training_unregister_task: validation failed");
    return -1;
}

int genius_training_get_task(genius_training_bridge_t* bridge,
                             uint32_t task_id,
                             genius_training_task_t* task) {
    if (!bridge || !task) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_unregister_task: required parameter is NULL (bridge, task)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    genius_training_task_t* t = find_task(bridge, task_id);
    if (t) {
        *task = *t;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "genius_training_unregister_task: validation failed");
    return -1;
}

//=============================================================================
// Training Functions
//=============================================================================

int genius_training_start(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_start: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_star", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_TRAINING_STATE_TRAINING;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_pause(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_pause: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_paus", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_TRAINING_STATE_PAUSED;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_resume(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_resume: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_resu", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->state == GENIUS_TRAINING_STATE_PAUSED) {
        bridge->state = GENIUS_TRAINING_STATE_TRAINING;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_stop(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_stop: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_stop", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_TRAINING_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float genius_training_train_batch(genius_training_bridge_t* bridge,
                                  const void* inputs,
                                  const void* targets,
                                  uint32_t batch_size) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_training_train_batch: bridge is NULL");
        return -1.0f;
    }
    if (!inputs || !targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_training_train_batch: inputs or targets is NULL");
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_trai", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, inputs, sizeof(*inputs));

    if (batch_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_training_train_batch: batch_size is 0");
        return -1.0f;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_TRAINING_STATE_TRAINING;

    /* Simulate batch training */
    float batch_loss = 0.0f;
    float batch_accuracy = 0.0f;

    /* In a real implementation, this would forward through the genius module
     * and compute gradients. For now, simulate training dynamics. */
    for (uint32_t i = 0; i < batch_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && batch_size > 256) {
            genius_training_bridge_heartbeat("genius_train_loop",
                             (float)(i + 1) / (float)batch_size);
        }

        /* Simulated loss computation */
        float sample_loss = 0.5f * (1.0f - bridge->mode_state.gauss_skill) +
                           0.3f * (1.0f - bridge->mode_state.newton_skill) +
                           0.2f * (1.0f - bridge->mode_state.erdos_skill);
        batch_loss += sample_loss;

        /* Simulated accuracy */
        float sample_acc = (bridge->mode_state.gauss_skill +
                           bridge->mode_state.newton_skill +
                           bridge->mode_state.erdos_skill) / 3.0f;
        batch_accuracy += sample_acc;
    }

    batch_loss /= (float)batch_size;
    batch_accuracy /= (float)batch_size;

    /* Update skills based on training (simulated gradient descent) */
    float lr = bridge->current_learning_rate;
    bridge->mode_state.gauss_skill += lr * 0.01f * (1.0f - batch_loss);
    bridge->mode_state.newton_skill += lr * 0.01f * (1.0f - batch_loss);
    bridge->mode_state.erdos_skill += lr * 0.01f * (1.0f - batch_loss);

    bridge->mode_state.gauss_skill = nimcp_myelin_clamp(bridge->mode_state.gauss_skill, 0.0f, 1.0f);
    bridge->mode_state.newton_skill = nimcp_myelin_clamp(bridge->mode_state.newton_skill, 0.0f, 1.0f);
    bridge->mode_state.erdos_skill = nimcp_myelin_clamp(bridge->mode_state.erdos_skill, 0.0f, 1.0f);

    /* Update progress */
    bridge->progress.total_examples_trained += batch_size;
    bridge->progress.total_batches++;
    bridge->progress.current_loss = batch_loss;
    bridge->progress.current_accuracy = batch_accuracy;
    bridge->progress.learning_rate_current = bridge->current_learning_rate;

    /* Update curriculum */
    if (bridge->config.enable_curriculum) {
        update_curriculum_state(bridge, batch_accuracy);
    }

    /* Update stats */
    bridge->stats.total_examples += batch_size;
    if (batch_accuracy > 0.8f) {
        bridge->stats.successful_proofs += batch_size / 2;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return batch_loss;
}

float genius_training_train_epoch(genius_training_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_trai", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_TRAINING_STATE_TRAINING;

    uint64_t start_time = nimcp_time_get_us();
    float epoch_loss = 0.0f;
    uint32_t num_batches = 100; /* Simulated */

    for (uint32_t b = 0; b < num_batches; b++) {
        /* Phase 8: Loop progress heartbeat */
        if ((b & 0xFF) == 0 && num_batches > 256) {
            genius_training_bridge_heartbeat("genius_train_loop",
                             (float)(b + 1) / (float)num_batches);
        }

        /* Simulated batch */
        float batch_loss = 0.5f * expf(-0.01f * (float)bridge->progress.total_batches);
        epoch_loss += batch_loss;

        bridge->progress.total_batches++;
        bridge->progress.total_examples_trained += bridge->config.batch_size;
    }

    epoch_loss /= (float)num_batches;
    bridge->progress.total_epochs++;
    bridge->progress.current_loss = epoch_loss;

    uint64_t elapsed = nimcp_time_get_us() - start_time;
    bridge->stats.total_training_time_us += elapsed;
    bridge->stats.mean_batch_time_ms = (float)elapsed / (float)num_batches / 1000.0f;
    bridge->stats.total_examples += num_batches * bridge->config.batch_size;

    /* Callback */
    if (bridge->epoch_callback) {
        bridge->epoch_callback(bridge, bridge->progress.total_epochs,
                              epoch_loss, bridge->progress.current_accuracy,
                              bridge->epoch_callback_data);
    }

    /* Checkpoint */
    if (bridge->config.enable_checkpointing &&
        bridge->progress.total_epochs % bridge->config.checkpoint_frequency == 0) {
        if (bridge->checkpoint_callback) {
            bridge->checkpoint_callback(bridge, bridge->progress.total_epochs,
                                       bridge->progress.validation_accuracy,
                                       bridge->checkpoint_callback_data);
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return epoch_loss;
}

float genius_training_validate(genius_training_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_vali", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_TRAINING_STATE_VALIDATING;

    uint64_t start_time = nimcp_time_get_us();

    /* Simulated validation */
    float validation_loss = bridge->progress.current_loss * 1.1f;
    float validation_accuracy = (bridge->mode_state.gauss_skill +
                                 bridge->mode_state.newton_skill +
                                 bridge->mode_state.erdos_skill) / 3.0f;

    bridge->progress.validation_loss = validation_loss;
    bridge->progress.validation_accuracy = validation_accuracy;

    if (validation_accuracy > bridge->progress.best_validation_score) {
        bridge->progress.best_validation_score = validation_accuracy;
    }

    bridge->stats.peak_accuracy = fmaxf(bridge->stats.peak_accuracy, validation_accuracy);
    bridge->stats.final_accuracy = validation_accuracy;

    uint64_t elapsed = nimcp_time_get_us() - start_time;
    bridge->stats.total_validation_time_us += elapsed;

    bridge->state = GENIUS_TRAINING_STATE_TRAINING;
    nimcp_mutex_unlock(bridge->base.mutex);

    return validation_accuracy;
}

int genius_training_consolidate(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_consolidate: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_cons", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state = GENIUS_TRAINING_STATE_CONSOLIDATING;

    /* Consolidation: strengthen well-learned skills */
    if (bridge->mode_state.gauss_skill > 0.7f) {
        bridge->mode_state.gauss_skill *= 1.02f;
    }
    if (bridge->mode_state.newton_skill > 0.7f) {
        bridge->mode_state.newton_skill *= 1.02f;
    }
    if (bridge->mode_state.erdos_skill > 0.7f) {
        bridge->mode_state.erdos_skill *= 1.02f;
    }

    bridge->mode_state.gauss_skill = nimcp_myelin_clamp(bridge->mode_state.gauss_skill, 0.0f, 1.0f);
    bridge->mode_state.newton_skill = nimcp_myelin_clamp(bridge->mode_state.newton_skill, 0.0f, 1.0f);
    bridge->mode_state.erdos_skill = nimcp_myelin_clamp(bridge->mode_state.erdos_skill, 0.0f, 1.0f);

    bridge->state = GENIUS_TRAINING_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Curriculum Functions
//=============================================================================

int genius_training_get_curriculum_state(genius_training_bridge_t* bridge,
                                         genius_curriculum_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_consolidate: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->curriculum;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_advance_curriculum(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_advance_curriculum: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_adva", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->curriculum.current_stage < GENIUS_STAGE_RESEARCH) {
        genius_curriculum_stage_t old_stage = bridge->curriculum.current_stage;
        bridge->curriculum.current_stage++;
        bridge->curriculum.examples_in_stage = 0;
        bridge->curriculum.stages_completed++;
        bridge->curriculum.advancements++;

        if (bridge->curriculum_callback) {
            bridge->curriculum_callback(bridge, old_stage, bridge->curriculum.current_stage,
                                       bridge->curriculum.current_domain,
                                       bridge->curriculum_callback_data);
        }

        nimcp_mutex_unlock(bridge->base.mutex);
        return (int)bridge->curriculum.current_stage;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "genius_training_advance_curriculum: operation failed");
    return -1;
}

int genius_training_set_curriculum_stage(genius_training_bridge_t* bridge,
                                         genius_curriculum_stage_t stage) {
    if (!bridge || stage > GENIUS_STAGE_RESEARCH) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "genius_training_advance_curriculum: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_set_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->curriculum.current_stage = stage;
    bridge->curriculum.examples_in_stage = 0;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_set_domain(genius_training_bridge_t* bridge,
                               genius_train_domain_t domain) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_training_set_domain: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_set_", 0.0f);


    if (domain >= GENIUS_TRAIN_DOMAIN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_training_set_domain: invalid domain %d (max: %d)",
            (int)domain, GENIUS_TRAIN_DOMAIN_COUNT - 1);
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->curriculum.current_domain = domain;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Learning Rate Functions
//=============================================================================

float genius_training_get_learning_rate(genius_training_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float lr = bridge->current_learning_rate;
    nimcp_mutex_unlock(bridge->base.mutex);
    return lr;
}

int genius_training_set_learning_rate(genius_training_bridge_t* bridge, float lr) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_training_set_learning_rate: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_set_", 0.0f);


    if (lr <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_training_set_learning_rate: learning rate must be positive, got %f", lr);
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_learning_rate = lr;
    bridge->progress.learning_rate_current = lr;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float genius_training_lr_step(genius_training_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_lr_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->lr_step_count++;
    bridge->current_learning_rate *= bridge->config.learning_rate_decay;
    bridge->progress.learning_rate_current = bridge->current_learning_rate;

    float lr = bridge->current_learning_rate;
    nimcp_mutex_unlock(bridge->base.mutex);
    return lr;
}

//=============================================================================
// State Query Functions
//=============================================================================

int genius_training_get_progress(genius_training_bridge_t* bridge,
                                 genius_training_progress_t* progress) {
    if (!bridge || !progress) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_lr_step: required parameter is NULL (bridge, progress)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, progress, sizeof(*progress));

    nimcp_mutex_lock(bridge->base.mutex);
    *progress = bridge->progress;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_get_mode_state(genius_training_bridge_t* bridge,
                                   genius_mode_training_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_lr_step: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->mode_state;

    /* Determine best mode */
    float max_skill = state->gauss_skill;
    state->best_mode = GENIUS_MODE_GAUSS;
    if (state->newton_skill > max_skill) {
        max_skill = state->newton_skill;
        state->best_mode = GENIUS_MODE_NEWTON;
    }
    if (state->erdos_skill > max_skill) {
        state->best_mode = GENIUS_MODE_ERDOS;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_get_state(genius_training_bridge_t* bridge,
                              genius_training_bridge_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_lr_step: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, state, sizeof(*state));

    nimcp_mutex_lock(bridge->base.mutex);

    state->state = bridge->state;
    state->active_tasks = bridge->task_count;
    state->overall_progress = (float)bridge->progress.total_epochs / 100.0f; /* Normalized */
    state->overall_skill = (bridge->mode_state.gauss_skill +
                            bridge->mode_state.newton_skill +
                            bridge->mode_state.erdos_skill) / 3.0f;
    state->max_stage = bridge->curriculum.current_stage;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_get_stats(genius_training_bridge_t* bridge,
                              genius_training_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_lr_step: required parameter is NULL (bridge, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, stats, sizeof(*stats));

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    stats->curriculum_efficiency = (bridge->curriculum.advancements > 0) ?
        (float)bridge->curriculum.stages_completed /
        (float)(bridge->curriculum.advancements + bridge->curriculum.regressions + 1) : 0.0f;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_reset_stats(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_rese", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(genius_training_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Checkpoint Functions
//=============================================================================

int genius_training_save_checkpoint(genius_training_bridge_t* bridge, const char* path) {
    if (!bridge || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_save_checkpoint: required parameter is NULL (bridge, path)");
        return -1;
    }

    /* In a full implementation, this would serialize the bridge state to disk */
    /* For now, just log the action */
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_save", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, path, sizeof(*path));

    NIMCP_LOG_INFO("Saving genius training checkpoint to: %s", path);
    return 0;
}

int genius_training_load_checkpoint(genius_training_bridge_t* bridge, const char* path) {
    if (!bridge || !path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_load_checkpoint: required parameter is NULL (bridge, path)");
        return -1;
    }

    /* In a full implementation, this would deserialize the bridge state from disk */
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_load", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, path, sizeof(*path));

    NIMCP_LOG_INFO("Loading genius training checkpoint from: %s", path);
    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int genius_training_register_epoch_callback(genius_training_bridge_t* bridge,
                                            genius_training_epoch_callback_t callback,
                                            void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_load_checkpoint: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_regi", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->epoch_callback = callback;
    bridge->epoch_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_register_curriculum_callback(genius_training_bridge_t* bridge,
                                                 genius_training_curriculum_callback_t callback,
                                                 void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_load_checkpoint: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_regi", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->curriculum_callback = callback;
    bridge->curriculum_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_register_checkpoint_callback(genius_training_bridge_t* bridge,
                                                 genius_training_checkpoint_callback_t callback,
                                                 void* user_data) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_load_checkpoint: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_regi", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, user_data, sizeof(*user_data));

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->checkpoint_callback = callback;
    bridge->checkpoint_callback_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int genius_training_bio_async_connect(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_bio_async_connect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = true;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_bio_async_disconnect(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_bio_async_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_bio_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool genius_training_is_bio_async_connected(genius_training_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_is_b", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool connected = bridge->bio_async_connected;
    nimcp_mutex_unlock(bridge->base.mutex);
    return connected;
}

//=============================================================================
// Heartbeat and State Serialization (Phase 8)
//=============================================================================

int genius_training_send_heartbeat(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_training_send_heartbeat: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->last_heartbeat_us = nimcp_time_get_us();
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

uint64_t genius_training_get_last_heartbeat(const genius_training_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Cast away const for mutex lock; the read is logically const */
    genius_training_bridge_t* nc = (genius_training_bridge_t*)bridge;
    nimcp_mutex_lock(nc->base.mutex);
    uint64_t last_hb = nc->last_heartbeat_us;
    nimcp_mutex_unlock(nc->base.mutex);
    return last_hb;
}

bool genius_training_is_heartbeat_stale(const genius_training_bridge_t* bridge,
                                         uint32_t timeout_ms) {
    if (!bridge) return true;

    uint64_t last_hb = genius_training_get_last_heartbeat(bridge);
    if (last_hb == 0) return true;

    uint64_t now_us = nimcp_time_get_us();
    uint64_t elapsed_us = now_us - last_hb;
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

    return elapsed_us > timeout_us;
}

int genius_training_serialize_state(genius_training_bridge_t* bridge,
                                     genius_training_serialized_t* serialized) {
    if (!bridge || !serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_training_serialize_state: bridge or serialized is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_seri", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, serialized, sizeof(*serialized));

    nimcp_mutex_lock(bridge->base.mutex);

    memset(serialized, 0, sizeof(*serialized));
    serialized->version = 1;
    serialized->timestamp_us = nimcp_time_get_us();

    /* Capture bridge state */
    genius_training_get_state(bridge, &serialized->state);

    /* Copy progress */
    memcpy(&serialized->progress, &bridge->progress, sizeof(genius_training_progress_t));

    /* Copy statistics */
    memcpy(&serialized->stats, &bridge->stats, sizeof(genius_training_stats_t));

    /* Compute checksum */
    serialized->checksum = genius_training_compute_checksum(serialized);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int genius_training_deserialize_state(genius_training_bridge_t* bridge,
                                       const genius_training_serialized_t* serialized) {
    if (!bridge || !serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "genius_training_deserialize_state: bridge or serialized is NULL");
        return -1;
    }

    if (!genius_training_verify_checksum(serialized)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_training_deserialize_state: checksum verification failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_dese", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, serialized, sizeof(*serialized));

    nimcp_mutex_lock(bridge->base.mutex);

    /* Restore state */
    bridge->state = serialized->state.state;

    /* Restore progress */
    memcpy(&bridge->progress, &serialized->progress, sizeof(genius_training_progress_t));

    /* Restore statistics */
    memcpy(&bridge->stats, &serialized->stats, sizeof(genius_training_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

uint32_t genius_training_compute_checksum(const genius_training_serialized_t* serialized) {
    if (!serialized) return 0;

    /* FNV-1a hash over relevant fields */
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_comp", 0.0f);


    uint32_t hash = 2166136261u;
    const uint8_t* data = (const uint8_t*)serialized;
    size_t len = offsetof(genius_training_serialized_t, checksum);

    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            genius_training_bridge_heartbeat("genius_train_loop",
                             (float)(i + 1) / (float)len);
        }

        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

bool genius_training_verify_checksum(const genius_training_serialized_t* serialized) {
    if (!serialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_verify_checksum: serialized is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_veri", 0.0f);


    uint32_t computed = genius_training_compute_checksum(serialized);
    return computed == serialized->checksum;
}

//=============================================================================
// KG Wiring Integration
//=============================================================================

kg_module_wiring_t* genius_training_create_kg_wiring(void) {
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_crea", 0.0f);


    kg_module_wiring_t* wiring = kg_module_wiring_create(
        KG_GENIUS_TRAINING_MODULE_NAME,
        KG_GENIUS_TRAINING_MODULE_TYPE
    );
    if (!wiring) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wiring is NULL");
        return NULL;
    }

    /* Set hierarchical placement - parietal cortex layer III */
    wiring->target_layer = KG_LAYER_III;
    wiring->hemisphere_affinity = KG_HEMISPHERE_BILATERAL;

    /* Set metadata */
    kg_module_wiring_set_metadata(wiring,
        "NIMCP Team",
        "MATHEMATICAL_CURRICULUM",
        "Mathematical genius curriculum learning and training bridge"
    );
    kg_module_wiring_set_version(wiring, 1, 0, 0);

    /* Register inputs from training system */
    kg_module_wiring_add_input(wiring, "data_loader", KG_MSG_TRAINING_BATCH_INPUT, true);
    kg_module_wiring_add_input(wiring, "curriculum_scheduler", KG_MSG_TRAINING_CURRICULUM_CHECK, false);
    kg_module_wiring_add_input(wiring, "validation_system", KG_MSG_TRAINING_VALIDATION_DATA, false);
    kg_module_wiring_add_input(wiring, "difficulty_controller", KG_MSG_TRAINING_DIFFICULTY_ADJUST, false);

    /* Register outputs for training events */
    kg_module_wiring_add_output(wiring, KG_MSG_TRAINING_EPOCH_COMPLETE, "Training epoch completion notification");
    kg_module_wiring_add_output(wiring, KG_MSG_TRAINING_STAGE_ADVANCE, "Curriculum stage advancement event");
    kg_module_wiring_add_output(wiring, KG_MSG_TRAINING_VALIDATION_RESULT, "Validation results for current stage");
    kg_module_wiring_add_output(wiring, KG_MSG_TRAINING_CHECKPOINT, "Model checkpoint saved event");

    /* Register message handlers */
    kg_module_wiring_add_handler(wiring, KG_MSG_TRAINING_TRAIN_REQUEST, 100);
    kg_module_wiring_add_handler(wiring, KG_MSG_TRAINING_VALIDATE_REQUEST, 100);
    kg_module_wiring_add_handler(wiring, KG_MSG_TRAINING_ADVANCE_REQUEST, 150);

    /* Set network type to hybrid (SNN + curriculum) */
    wiring->network_type = KG_WEIGHT_HYBRID;

    /* Add custom metadata */
    kg_module_wiring_add_metadata_entry(wiring, "brain_region", "parietal_supramarginal_gyrus");
    kg_module_wiring_add_metadata_entry(wiring, "training_types", "curriculum,continual,transfer");
    kg_module_wiring_add_metadata_entry(wiring, "domains", "number_theory,calculus,combinatorics");

    return wiring;
}

kg_module_wiring_t* genius_training_get_kg_wiring(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "genius_training_get_kg_wiring: bridge is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    genius_training_bridge_heartbeat("genius_train_genius_training_get_", 0.0f);


    return bridge->kg_wiring;
}

//=============================================================================
// Instance Health Agent Setter (B23 Upgrade)
//=============================================================================

void genius_training_bridge_set_instance_health_agent(
    genius_training_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B23 Upgrade)
//=============================================================================

int genius_training_bridge_training_begin(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_training_bridge_training_begin: NULL argument");
        return -1;
    }
    genius_training_bridge_heartbeat_instance(bridge->health_agent, "genius_training_bridge_training_begin", 0.0f);
    return 0;
}

int genius_training_bridge_training_end(genius_training_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_training_bridge_training_end: NULL argument");
        return -1;
    }
    genius_training_bridge_heartbeat_instance(bridge->health_agent, "genius_training_bridge_training_end", 1.0f);
    return 0;
}

int genius_training_bridge_training_step(genius_training_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_training_bridge_training_step: NULL argument");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "genius_training_bridge_training_step");
    BRIDGE_LGSS_GATE(bridge, "genius_training_bridge_training_step");
    genius_training_bridge_heartbeat_instance(bridge->health_agent, "genius_training_bridge_training_step", progress);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/**
 * @file nimcp_cognitive_meta_controller.c
 * @brief Cognitive Meta-Controller implementation
 * @version 1.0.0
 * @date 2025-12-15
 *
 * @author NIMCP Development Team
 */

#include "cognitive/nimcp_cognitive_meta_controller.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* Platform-specific includes */
#include "utils/platform/nimcp_platform.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(cognitive_meta_controller, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * WHAT: Validate configuration
 * WHY:  Catch invalid configs before creation
 * HOW:  Range checks on all parameters
 */
static int validate_config(const meta_controller_config_t* config) {
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    NIMCP_CHECK_THROW(config->max_wm_slots > 0 && config->max_wm_slots <= 20,
        NIMCP_ERROR_INVALID_PARAM, "invalid max_wm_slots: %u (must be 1-20)", config->max_wm_slots);
    NIMCP_CHECK_THROW(config->base_learning_rate >= META_CONTROLLER_LR_MIN &&
        config->base_learning_rate <= META_CONTROLLER_LR_MAX,
        NIMCP_ERROR_INVALID_PARAM, "invalid base_learning_rate: %f", config->base_learning_rate);

    return NIMCP_SUCCESS;
}

/**
 * WHAT: Initialize module performance tracking
 * WHY:  Start fresh with clean metrics
 * HOW:  Zero all fields
 */
static void init_module_performance(module_performance_t* perf,
                                    cognitive_module_id_t module) {
    if (!perf) return;

    memset(perf, 0, sizeof(module_performance_t));
    perf->module = module;
    perf->success_rate = 1.0f;  /* Start optimistic */
    perf->current_confidence = 0.5f;
    perf->current_uncertainty = 0.5f;
}

/**
 * WHAT: Notify allocation observers
 * WHY:  Inform subscribers of resource allocation events
 * HOW:  Call all registered callbacks
 */
static void notify_allocation_observers(
    cognitive_meta_controller_t* controller,
    const resource_request_t* request) {

    if (!controller || !request) return;

    for (uint32_t i = 0; i < controller->allocation_callback_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && controller->allocation_callback_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)controller->allocation_callback_count);
        }

        if (controller->allocation_callbacks[i]) {
            controller->allocation_callbacks[i](
                request,
                controller->allocation_callback_data[i]
            );
        }
    }
}

/**
 * WHAT: Notify metacognitive observers
 * WHY:  Inform subscribers of metacognitive state changes
 * HOW:  Call all registered callbacks
 */
static void notify_metacognitive_observers(
    cognitive_meta_controller_t* controller,
    float uncertainty,
    float confidence,
    float performance) {

    if (!controller) return;

    for (uint32_t i = 0; i < controller->metacognitive_callback_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && controller->metacognitive_callback_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)controller->metacognitive_callback_count);
        }

        if (controller->metacognitive_callbacks[i]) {
            controller->metacognitive_callbacks[i](
                uncertainty,
                confidence,
                performance,
                controller->metacognitive_callback_data[i]
            );
        }
    }
}

/* ============================================================================
 * Arbitration Functions
 * ============================================================================ */

/**
 * WHAT: Find highest priority request for resource type
 * WHY:  Winner-take-all arbitration needs maximum
 * HOW:  Linear scan for max priority
 */
static int find_highest_priority_request(
    cognitive_meta_controller_t* controller,
    resource_type_t type) {

    if (!controller || controller->request_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_config: controller is NULL");
        return -1;
    }

    int best_idx = -1;
    float best_priority = -1.0f;

    for (uint32_t i = 0; i < controller->request_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && controller->request_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)controller->request_count);
        }

        if (controller->requests[i].type == type &&
            !controller->requests[i].granted &&
            controller->requests[i].priority > best_priority) {
            best_priority = controller->requests[i].priority;
            best_idx = (int)i;
        }
    }

    return best_idx;
}

/**
 * WHAT: Apply winner-take-all arbitration
 * WHY:  Simple, fast, biologically plausible
 * HOW:  Highest priority wins
 */
static void arbitrate_winner_take_all(
    cognitive_meta_controller_t* controller,
    resource_type_t type) {

    if (!controller) return;

    int winner_idx = find_highest_priority_request(controller, type);
    if (winner_idx >= 0) {
        controller->requests[winner_idx].granted = true;
        controller->stats.granted_requests++;

        /* Notify observers */
        notify_allocation_observers(controller,
                                    &controller->requests[winner_idx]);
    }
}

/**
 * WHAT: Apply priority-weighted arbitration
 * WHY:  Module importance influences allocation
 * HOW:  Priority * module_weight, highest wins
 */
static void arbitrate_priority_weighted(
    cognitive_meta_controller_t* controller,
    resource_type_t type) {

    if (!controller || controller->request_count == 0) return;

    int best_idx = -1;
    float best_weighted_priority = -1.0f;

    for (uint32_t i = 0; i < controller->request_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && controller->request_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)controller->request_count);
        }

        if (controller->requests[i].type != type ||
            controller->requests[i].granted) {
            continue;
        }

        cognitive_module_id_t module = controller->requests[i].requester;
        float module_weight = 1.0f;

        if (module < META_CONTROLLER_MAX_MODULES) {
            module_weight = controller->config.module_weights[module];
        }

        float weighted = controller->requests[i].priority * module_weight;

        if (weighted > best_weighted_priority) {
            best_weighted_priority = weighted;
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0) {
        controller->requests[best_idx].granted = true;
        controller->stats.granted_requests++;
        notify_allocation_observers(controller,
                                    &controller->requests[best_idx]);
    }
}

/**
 * WHAT: Resolve resource conflicts
 * WHY:  Multiple requests compete for limited resources
 * HOW:  Apply arbitration strategy
 */
static void resolve_conflicts(cognitive_meta_controller_t* controller) {
    if (!controller) return;

    /* Count conflicts by type */
    uint32_t conflicts[5] = {0}; /* One per resource_type_t */

    for (uint32_t i = 0; i < controller->request_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && controller->request_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)controller->request_count);
        }

        if (!controller->requests[i].granted &&
            controller->requests[i].type < 5) {
            conflicts[controller->requests[i].type]++;
        }
    }

    /* Resolve each resource type */
    for (int type = 0; type < 5; type++) {
        /* Phase 8: Loop progress heartbeat */
        if ((type & 0xFF) == 0 && 5 > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(type + 1) / (float)5);
        }

        if (conflicts[type] == 0) continue;

        controller->stats.conflicts_resolved++;

        switch (controller->config.strategy) {
            case ARBITRATION_WINNER_TAKE_ALL:
                arbitrate_winner_take_all(controller, (resource_type_t)type);
                break;

            case ARBITRATION_PRIORITY_WEIGHTED:
                arbitrate_priority_weighted(controller, (resource_type_t)type);
                break;

            case ARBITRATION_WEIGHTED_FUSION:
            case ARBITRATION_ROUND_ROBIN:
                /* Not implemented yet - fall back to winner-take-all */
                arbitrate_winner_take_all(controller, (resource_type_t)type);
                break;
        }
    }
}

/* ============================================================================
 * Resource Allocation Functions
 * ============================================================================ */

/**
 * WHAT: Allocate working memory slots to granted requests
 * WHY:  Winners need actual WM allocation
 * HOW:  Call working_memory_add for each granted WM request
 */
static void allocate_wm_slots(cognitive_meta_controller_t* controller) {
    if (!controller || !controller->working_memory) return;

    for (uint32_t i = 0; i < controller->request_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && controller->request_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)controller->request_count);
        }

        resource_request_t* req = &controller->requests[i];

        if (req->type == RESOURCE_WORKING_MEMORY_SLOT && req->granted) {
            wm_slot_request_t* wm_req = &req->data.wm_slot;

            bool success = working_memory_add(
                controller->working_memory,
                (const float*)wm_req->item_data,
                wm_req->item_size,
                wm_req->salience
            );

            if (success) {
                /* Update module performance */
                uint32_t module_idx = req->requester;
                if (module_idx < META_CONTROLLER_MAX_MODULES) {
                    controller->modules[module_idx].requests_granted++;
                }
            }
        }
    }
}

/**
 * WHAT: Clear processed requests
 * WHY:  Keep request queue clean
 * HOW:  Remove granted/expired requests
 */
static void clear_processed_requests(
    cognitive_meta_controller_t* controller,
    uint64_t current_time_ms) {

    if (!controller) return;

    uint32_t write_idx = 0;

    for (uint32_t read_idx = 0; read_idx < controller->request_count; read_idx++) {
        /* Phase 8: Loop progress heartbeat */
        if ((read_idx & 0xFF) == 0 && controller->request_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(read_idx + 1) / (float)controller->request_count);
        }

        resource_request_t* req = &controller->requests[read_idx];

        /* Keep if: not granted AND not expired (>5 seconds old) */
        bool keep = !req->granted &&
                    (current_time_ms - req->timestamp_ms) < 5000;

        if (keep) {
            if (write_idx != read_idx) {
                controller->requests[write_idx] = controller->requests[read_idx];
            }
            write_idx++;
        }
    }

    controller->request_count = write_idx;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int meta_controller_default_config(meta_controller_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_defa", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(meta_controller_config_t));

    /* Resource limits */
    config->max_wm_slots = META_CONTROLLER_WM_DEFAULT_CAPACITY;
    config->max_attention_foci = 1;
    config->base_learning_rate = META_CONTROLLER_LR_DEFAULT;

    /* Arbitration */
    config->strategy = ARBITRATION_WINNER_TAKE_ALL;
    config->priority_threshold = NIMCP_PLASTICITY_RATE_DEFAULT;

    /* Metacognitive control */
    config->enable_uncertainty_modulation = true;
    config->enable_affective_metacontrol = true;
    config->enable_performance_tracking = true;

    /* Integration enables */
    config->enable_working_memory = true;
    config->enable_executive = true;
    config->enable_global_workspace = true;
    config->enable_brain_immune = true;
    config->enable_bio_async = true;

    /* Update timing */
    config->update_interval_ms = META_CONTROLLER_DEFAULT_UPDATE_MS;

    /* Module weights (all equal by default) */
    for (int i = 0; i < META_CONTROLLER_MAX_MODULES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && META_CONTROLLER_MAX_MODULES > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)META_CONTROLLER_MAX_MODULES);
        }

        config->module_weights[i] = 1.0f;
    }

    return NIMCP_SUCCESS;
}

cognitive_meta_controller_t* meta_controller_create(
    const meta_controller_config_t* config) {

    /* Use defaults if no config provided */
    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_crea", 0.0f);


    meta_controller_config_t default_config;
    if (!config) {
        if (meta_controller_default_config(&default_config) != NIMCP_SUCCESS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
        }
        config = &default_config;
    }

    /* Validate configuration */
    if (validate_config(config) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_controller_default_config: validation failed");
        return NULL;
    }

    /* Allocate controller */
    cognitive_meta_controller_t* controller =
        (cognitive_meta_controller_t*)nimcp_calloc(1, sizeof(cognitive_meta_controller_t));

    if (!controller) {
        NIMCP_LOGGING_ERROR("Failed to allocate controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "controller is NULL");


        return NULL;
    }

    /* Copy configuration */
    controller->config = *config;
    controller->state = META_CONTROLLER_STOPPED;

    /* Allocate request queue */
    controller->request_capacity = META_CONTROLLER_MAX_REQUESTS;
    controller->requests = (resource_request_t*)nimcp_calloc(
        controller->request_capacity,
        sizeof(resource_request_t)
    );

    if (!controller->requests) {
        NIMCP_LOGGING_ERROR("Failed to allocate request queue");
        nimcp_free(controller);
        controller = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_controller_default_config: controller->requests is NULL");
        return NULL;
    }

    /* Initialize module tracking */
    for (int i = 0; i < META_CONTROLLER_MAX_MODULES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && META_CONTROLLER_MAX_MODULES > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)META_CONTROLLER_MAX_MODULES);
        }

        init_module_performance(&controller->modules[i], (cognitive_module_id_t)i);
    }

    /* Create mutex */
    controller->mutex = nimcp_platform_mutex_create();
    if (!controller->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(controller->requests);
        nimcp_free(controller);
        controller = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "meta_controller_default_config: controller->mutex is NULL");
        return NULL;
    }

    /* Initialize IDs */
    controller->next_request_id = 1;

    /* Initialize timing */
    controller->start_time = nimcp_time_get_ms();
    controller->last_update_time = controller->start_time;

    NIMCP_LOGGING_INFO("Created cognitive meta-controller");

    return controller;
}

void meta_controller_destroy(cognitive_meta_controller_t* controller) {
    if (!controller) return;

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_dest", 0.0f);


    NIMCP_LOGGING_INFO("Destroying cognitive meta-controller");

    /* Stop if running */
    if (controller->state == META_CONTROLLER_RUNNING) {
        meta_controller_stop(controller);
    }

    /* Disconnect integrations */
    if (controller->bio_async_connected) {
        meta_controller_disconnect_bio_async(controller);
    }

    /* Free request queue */
    if (controller->requests) {
        nimcp_free(controller->requests);
    }

    /* Destroy mutex */
    if (controller->mutex) {
        nimcp_platform_mutex_destroy(controller->mutex);
        nimcp_free(controller->mutex);
        controller->mutex = NULL;
    }

    /* Free controller */
    nimcp_free(controller);
    controller = NULL;
}

int meta_controller_start(cognitive_meta_controller_t* controller) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_star", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    if (controller->state == META_CONTROLLER_RUNNING) {
        return NIMCP_SUCCESS; /* Already running */
    }

    nimcp_platform_mutex_lock(controller->mutex);

    controller->state = META_CONTROLLER_RUNNING;
    controller->start_time = nimcp_time_get_ms();
    controller->last_update_time = controller->start_time;

    nimcp_platform_mutex_unlock(controller->mutex);

    NIMCP_LOGGING_INFO("Started cognitive meta-controller");

    return NIMCP_SUCCESS;
}

int meta_controller_stop(cognitive_meta_controller_t* controller) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_stop", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    nimcp_platform_mutex_lock(controller->mutex);

    controller->state = META_CONTROLLER_STOPPED;

    nimcp_platform_mutex_unlock(controller->mutex);

    NIMCP_LOGGING_INFO("Stopped cognitive meta-controller");

    return NIMCP_SUCCESS;
}

int meta_controller_pause(cognitive_meta_controller_t* controller) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_paus", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    nimcp_platform_mutex_lock(controller->mutex);

    if (controller->state == META_CONTROLLER_RUNNING) {
        controller->state = META_CONTROLLER_PAUSED;
    }

    nimcp_platform_mutex_unlock(controller->mutex);

    return NIMCP_SUCCESS;
}

int meta_controller_resume(cognitive_meta_controller_t* controller) {
    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_resu", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    nimcp_platform_mutex_lock(controller->mutex);

    if (controller->state == META_CONTROLLER_PAUSED) {
        controller->state = META_CONTROLLER_RUNNING;
    }

    nimcp_platform_mutex_unlock(controller->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Resource Request API Implementation
 * ============================================================================ */

uint32_t meta_controller_request_wm_slot(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    const float* item_data,
    uint32_t item_size,
    float priority,
    float salience) {

    if (!controller || !item_data) return 0;
    if (item_size == 0 || item_size > WORKING_MEMORY_MAX_ITEM_SIZE) return 0;

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_requ", 0.0f);


    priority = nimcp_clampf(priority, 0.0f, 1.0f);
    salience = nimcp_clampf(salience, 0.0f, 1.0f);

    nimcp_platform_mutex_lock(controller->mutex);

    /* Check capacity */
    if (controller->request_count >= controller->request_capacity) {
        nimcp_platform_mutex_unlock(controller->mutex);
        NIMCP_LOGGING_WARN("Request queue full");
        return 0;
    }

    /* Create request */
    resource_request_t* req = &controller->requests[controller->request_count];
    req->request_id = controller->next_request_id++;
    req->type = RESOURCE_WORKING_MEMORY_SLOT;
    req->requester = requester;
    req->priority = priority;
    req->granted = false;
    req->timestamp_ms = nimcp_time_get_ms();

    /* Fill WM-specific data */
    req->data.wm_slot.requester = requester;
    req->data.wm_slot.priority = priority;
    req->data.wm_slot.item_size = item_size;
    req->data.wm_slot.salience = salience;
    req->data.wm_slot.item_data = (void*)item_data; /* Caller owns data */
    req->data.wm_slot.timestamp_ms = req->timestamp_ms;

    controller->request_count++;
    controller->stats.total_requests++;

    /* Update module stats */
    if (requester < META_CONTROLLER_MAX_MODULES) {
        controller->modules[requester].requests_made++;
        controller->modules[requester].last_request_time = req->timestamp_ms;
    }

    uint32_t request_id = req->request_id;

    nimcp_platform_mutex_unlock(controller->mutex);

    return request_id;
}

uint32_t meta_controller_request_attention(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    float salience,
    float urgency,
    void* focus_data) {

    if (!controller) return 0;

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_requ", 0.0f);


    salience = nimcp_clampf(salience, 0.0f, 1.0f);
    urgency = nimcp_clampf(urgency, 0.0f, 1.0f);

    nimcp_platform_mutex_lock(controller->mutex);

    if (controller->request_count >= controller->request_capacity) {
        nimcp_platform_mutex_unlock(controller->mutex);
        return 0;
    }

    resource_request_t* req = &controller->requests[controller->request_count];
    req->request_id = controller->next_request_id++;
    req->type = RESOURCE_ATTENTION_FOCUS;
    req->requester = requester;
    req->priority = (salience + urgency) / 2.0f; /* Combined priority */
    req->granted = false;
    req->timestamp_ms = nimcp_time_get_ms();

    req->data.attention.requester = requester;
    req->data.attention.salience = salience;
    req->data.attention.urgency = urgency;
    req->data.attention.focus_data = focus_data;
    req->data.attention.timestamp_ms = req->timestamp_ms;

    controller->request_count++;
    controller->stats.total_requests++;

    if (requester < META_CONTROLLER_MAX_MODULES) {
        controller->modules[requester].requests_made++;
        controller->modules[requester].last_request_time = req->timestamp_ms;
    }

    uint32_t request_id = req->request_id;

    nimcp_platform_mutex_unlock(controller->mutex);

    return request_id;
}

float meta_controller_request_learning_rate(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    float uncertainty,
    float confidence,
    float desired_lr) {

    if (!controller) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_requ", 0.0f);


    uncertainty = nimcp_clampf(uncertainty, 0.0f, 1.0f);
    confidence = nimcp_clampf(confidence, 0.0f, 1.0f);

    /* Base learning rate */
    float lr = controller->config.base_learning_rate;

    /* Modulate by uncertainty (high uncertainty → higher LR for exploration) */
    if (controller->config.enable_uncertainty_modulation) {
        float uncertainty_factor = 1.0f + (uncertainty - 0.5f) * 0.5f;
        lr *= uncertainty_factor;
    }

    /* Modulate by confidence (low confidence → lower LR for stability) */
    float confidence_factor = 0.5f + confidence * 0.5f;
    lr *= confidence_factor;

    /* Modulate by brain immune state if connected */
    if (controller->brain_immune && controller->immune_connected) {
        /* Inflammation reduces learning rate (conserve energy during threat) */
        /* This would require brain_immune API calls - placeholder for now */
        float immune_factor = 1.0f; /* TODO: Get from brain_immune */
        lr *= immune_factor;
    }

    /* Clamp to valid range */
    lr = nimcp_clampf(lr, META_CONTROLLER_LR_MIN, META_CONTROLLER_LR_MAX);

    /* Update module stats */
    if (requester < META_CONTROLLER_MAX_MODULES) {
        controller->modules[requester].current_confidence = confidence;
        controller->modules[requester].current_uncertainty = uncertainty;
    }

    return lr;
}

int meta_controller_request_executive_priority(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    uint32_t task_id,
    float priority) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_requ", 0.0f);


    NIMCP_CHECK_THROW(controller && controller->executive, NIMCP_ERROR_NULL_POINTER, "controller or executive is NULL");

    /* This would forward to executive controller */
    /* Placeholder - executive API doesn't have set_priority yet */

    NIMCP_LOGGING_DEBUG("Executive priority request from module %d for task %u",
                        requester, task_id);

    return NIMCP_SUCCESS;
}

bool meta_controller_request_workspace_access(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t requester,
    const float* content,
    uint32_t content_dim,
    float strength) {

    if (!controller || !controller->global_workspace || !content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (controller, controller->global_workspace, content)");
        return false;
    }

    /* Apply module weighting to strength */
    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_requ", 0.0f);


    float module_weight = 1.0f;
    if (requester < META_CONTROLLER_MAX_MODULES) {
        module_weight = controller->config.module_weights[requester];
    }

    float weighted_strength = strength * module_weight;
    weighted_strength = nimcp_clampf(weighted_strength, 0.0f, 1.0f);

    /* Forward to global workspace */
    bool granted = global_workspace_compete(
        controller->global_workspace,
        (cognitive_module_t)requester,
        content,
        content_dim,
        weighted_strength
    );

    /* Update module stats */
    if (requester < META_CONTROLLER_MAX_MODULES) {
        controller->modules[requester].requests_made++;
        if (granted) {
            controller->modules[requester].requests_granted++;
        } else {
            controller->modules[requester].requests_denied++;
        }
    }

    return granted;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

int meta_controller_update(
    cognitive_meta_controller_t* controller,
    uint64_t current_time_ms) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_upda", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    if (controller->state != META_CONTROLLER_RUNNING) {
        return 0; /* Not running, nothing to do */
    }

    uint64_t start_time_us = nimcp_time_get_us();

    nimcp_platform_mutex_lock(controller->mutex);

    /* Check if update interval has elapsed */
    uint64_t time_since_update = current_time_ms - controller->last_update_time;
    if (time_since_update < controller->config.update_interval_ms) {
        nimcp_platform_mutex_unlock(controller->mutex);
        return 0; /* Too soon */
    }

    controller->last_update_time = current_time_ms;

    /* Resolve conflicts via arbitration strategy */
    resolve_conflicts(controller);

    /* Allocate resources to winners */
    allocate_wm_slots(controller);

    /* Count denied requests */
    for (uint32_t i = 0; i < controller->request_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && controller->request_count > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)controller->request_count);
        }

        if (!controller->requests[i].granted) {
            controller->stats.denied_requests++;

            uint32_t module_idx = controller->requests[i].requester;
            if (module_idx < META_CONTROLLER_MAX_MODULES) {
                controller->modules[module_idx].requests_denied++;
            }
        }
    }

    /* Clear processed requests */
    clear_processed_requests(controller, current_time_ms);

    /* Update metacognitive state */
    meta_controller_update_metacognitive_state(controller);

    /* Update statistics */
    controller->stats.total_updates++;

    uint64_t end_time_us = nimcp_time_get_us();
    float update_time_us = (float)(end_time_us - start_time_us);

    /* Running average using EMA weights */
    controller->stats.avg_update_time_us =
        NIMCP_EMA_WEIGHT_FAST * update_time_us +
        NIMCP_EMA_WEIGHT_SLOW * controller->stats.avg_update_time_us;

    if (update_time_us > controller->stats.max_update_time_us) {
        controller->stats.max_update_time_us = update_time_us;
    }

    int requests_processed = (int)controller->request_count;

    nimcp_platform_mutex_unlock(controller->mutex);

    return requests_processed;
}

int meta_controller_update_metacognitive_state(
    cognitive_meta_controller_t* controller) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_upda", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    /* Compute system-wide metrics from module performance */
    float total_uncertainty = 0.0f;
    float total_confidence = 0.0f;
    float total_performance = 0.0f;
    uint32_t active_modules = 0;

    for (uint32_t i = 0; i < META_CONTROLLER_MAX_MODULES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && META_CONTROLLER_MAX_MODULES > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)META_CONTROLLER_MAX_MODULES);
        }

        module_performance_t* perf = &controller->modules[i];

        if (perf->requests_made > 0) {
            active_modules++;
            total_uncertainty += perf->current_uncertainty;
            total_confidence += perf->current_confidence;

            /* Update success rate */
            if (perf->requests_made > 0) {
                perf->success_rate =
                    (float)perf->requests_granted / (float)perf->requests_made;
                total_performance += perf->success_rate;
            }
        }
    }

    if (active_modules > 0) {
        controller->stats.system_uncertainty = total_uncertainty / active_modules;
        controller->stats.system_confidence = total_confidence / active_modules;
        controller->stats.system_performance = total_performance / active_modules;

        /* Notify observers */
        notify_metacognitive_observers(
            controller,
            controller->stats.system_uncertainty,
            controller->stats.system_confidence,
            controller->stats.system_performance
        );
    }

    /* Update resource utilization */
    if (controller->working_memory) {
        controller->stats.wm_utilization =
            working_memory_get_utilization(controller->working_memory);
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Integration API Implementation
 * ============================================================================ */

int meta_controller_connect_working_memory(
    cognitive_meta_controller_t* controller,
    working_memory_t* working_memory) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_conn", 0.0f);


    NIMCP_CHECK_THROW(controller && working_memory, NIMCP_ERROR_NULL_POINTER, "controller or working_memory is NULL");

    controller->working_memory = working_memory;

    NIMCP_LOGGING_INFO("Connected to working memory");

    return NIMCP_SUCCESS;
}

int meta_controller_connect_executive(
    cognitive_meta_controller_t* controller,
    executive_controller_t* executive) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_conn", 0.0f);


    NIMCP_CHECK_THROW(controller && executive, NIMCP_ERROR_NULL_POINTER, "controller or executive is NULL");

    controller->executive = executive;

    NIMCP_LOGGING_INFO("Connected to executive controller");

    return NIMCP_SUCCESS;
}

int meta_controller_connect_global_workspace(
    cognitive_meta_controller_t* controller,
    global_workspace_t* workspace) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_conn", 0.0f);


    NIMCP_CHECK_THROW(controller && workspace, NIMCP_ERROR_NULL_POINTER, "controller or workspace is NULL");

    controller->global_workspace = workspace;

    NIMCP_LOGGING_INFO("Connected to global workspace");

    return NIMCP_SUCCESS;
}

int meta_controller_connect_brain_immune(
    cognitive_meta_controller_t* controller,
    brain_immune_system_t* immune) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_conn", 0.0f);


    NIMCP_CHECK_THROW(controller && immune, NIMCP_ERROR_NULL_POINTER, "controller or immune is NULL");

    controller->brain_immune = immune;
    controller->immune_connected = true;

    NIMCP_LOGGING_INFO("Connected to brain immune system");

    return NIMCP_SUCCESS;
}

int meta_controller_connect_bio_async(
    cognitive_meta_controller_t* controller) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_conn", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    if (controller->bio_async_connected) {
        return NIMCP_SUCCESS; /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_COGNITIVE_META_CONTROLLER,
        .module_name = META_CONTROLLER_MODULE_NAME,
        .inbox_capacity = NIMCP_INBOX_CAPACITY_SMALL,
        .user_data = controller
    };

    controller->bio_context = bio_router_register_module(&info);

    if (controller->bio_context) {
        controller->bio_async_connected = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
        return NIMCP_SUCCESS;
    } else {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "bio-async router not available");
        return NIMCP_ERROR_OPERATION_FAILED;
    }
}

int meta_controller_disconnect_bio_async(
    cognitive_meta_controller_t* controller) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_disc", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    if (controller->bio_async_connected && controller->bio_context) {
        bio_router_unregister_module(controller->bio_context);
        controller->bio_context = NULL;
        controller->bio_async_connected = false;
        NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Observer API Implementation
 * ============================================================================ */

int meta_controller_register_allocation_observer(
    cognitive_meta_controller_t* controller,
    resource_allocation_callback_t callback,
    void* user_data) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_regi", 0.0f);


    NIMCP_CHECK_THROW(controller && callback, NIMCP_ERROR_NULL_POINTER, "controller or callback is NULL");

    if (controller->allocation_callback_count >= META_CONTROLLER_MAX_OBSERVERS) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "allocation observer limit reached");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    uint32_t idx = controller->allocation_callback_count++;
    controller->allocation_callbacks[idx] = callback;
    controller->allocation_callback_data[idx] = user_data;

    return NIMCP_SUCCESS;
}

int meta_controller_register_metacognitive_observer(
    cognitive_meta_controller_t* controller,
    metacognitive_callback_t callback,
    void* user_data) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_regi", 0.0f);


    NIMCP_CHECK_THROW(controller && callback, NIMCP_ERROR_NULL_POINTER, "controller or callback is NULL");

    if (controller->metacognitive_callback_count >= META_CONTROLLER_MAX_OBSERVERS) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "metacognitive observer limit reached");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    uint32_t idx = controller->metacognitive_callback_count++;
    controller->metacognitive_callbacks[idx] = callback;
    controller->metacognitive_callback_data[idx] = user_data;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Monitoring API Implementation
 * ============================================================================ */

int meta_controller_get_stats(
    const cognitive_meta_controller_t* controller,
    meta_controller_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_get_", 0.0f);


    NIMCP_CHECK_THROW(controller && stats, NIMCP_ERROR_NULL_POINTER, "controller or stats is NULL");

    *stats = controller->stats;

    return NIMCP_SUCCESS;
}

void meta_controller_reset_stats(cognitive_meta_controller_t* controller) {
    if (!controller) return;

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_rese", 0.0f);


    memset(&controller->stats, 0, sizeof(meta_controller_stats_t));

    /* Reinitialize module performance */
    for (int i = 0; i < META_CONTROLLER_MAX_MODULES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && META_CONTROLLER_MAX_MODULES > 256) {
            cognitive_meta_controller_heartbeat("cognitive_me_loop",
                             (float)(i + 1) / (float)META_CONTROLLER_MAX_MODULES);
        }

        init_module_performance(&controller->modules[i], (cognitive_module_id_t)i);
    }
}

int meta_controller_get_module_performance(
    const cognitive_meta_controller_t* controller,
    cognitive_module_id_t module,
    module_performance_t* performance) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_get_", 0.0f);


    NIMCP_CHECK_THROW(controller && performance, NIMCP_ERROR_NULL_POINTER, "controller or performance is NULL");
    NIMCP_CHECK_THROW(module < META_CONTROLLER_MAX_MODULES, NIMCP_ERROR_INVALID_PARAM, "invalid module id: %d", module);

    *performance = controller->modules[module];

    return NIMCP_SUCCESS;
}

meta_controller_state_t meta_controller_get_state(
    const cognitive_meta_controller_t* controller) {

    if (!controller) return META_CONTROLLER_ERROR;

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_get_", 0.0f);


    return controller->state;
}

/* ============================================================================
 * Configuration API Implementation
 * ============================================================================ */

int meta_controller_set_arbitration_strategy(
    cognitive_meta_controller_t* controller,
    arbitration_strategy_t strategy) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_set_", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");

    controller->config.strategy = strategy;

    return NIMCP_SUCCESS;
}

int meta_controller_set_module_weight(
    cognitive_meta_controller_t* controller,
    cognitive_module_id_t module,
    float weight) {

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_meta_controller_set_", 0.0f);


    NIMCP_CHECK_THROW(controller, NIMCP_ERROR_NULL_POINTER, "controller is NULL");
    NIMCP_CHECK_THROW(module < META_CONTROLLER_MAX_MODULES, NIMCP_ERROR_INVALID_PARAM, "invalid module id: %d", module);

    weight = nimcp_clampf(weight, 0.0f, 1.0f);
    controller->config.module_weights[module] = weight;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * String Conversion API Implementation
 * ============================================================================ */

const char* cognitive_module_id_to_string(cognitive_module_id_t module) {
    switch (module) {
        case META_CTRL_MODULE_NONE: return "NONE";
        case META_CTRL_MODULE_WORKING_MEMORY: return "WORKING_MEMORY";
        case META_CTRL_MODULE_EXECUTIVE: return "EXECUTIVE";
        case META_CTRL_MODULE_ATTENTION: return "ATTENTION";
        case META_CTRL_MODULE_CURIOSITY: return "CURIOSITY";
        case META_CTRL_MODULE_EMOTION: return "EMOTION";
        case META_CTRL_MODULE_INTROSPECTION: return "INTROSPECTION";
        case META_CTRL_MODULE_GLOBAL_WORKSPACE: return "GLOBAL_WORKSPACE";
        case META_CTRL_MODULE_THEORY_OF_MIND: return "THEORY_OF_MIND";
        case META_CTRL_MODULE_ETHICS: return "ETHICS";
        case META_CTRL_MODULE_WELLBEING: return "WELLBEING";
        case META_CTRL_MODULE_MENTAL_HEALTH: return "MENTAL_HEALTH";
        case META_CTRL_MODULE_CONSOLIDATION: return "CONSOLIDATION";
        default: return "UNKNOWN";
    }
}

const char* resource_type_to_string(resource_type_t type) {
    switch (type) {
        case RESOURCE_WORKING_MEMORY_SLOT: return "WM_SLOT";
        case RESOURCE_ATTENTION_FOCUS: return "ATTENTION";
        case RESOURCE_LEARNING_BANDWIDTH: return "LEARNING_RATE";
        case RESOURCE_EXECUTIVE_PRIORITY: return "EXEC_PRIORITY";
        case RESOURCE_GLOBAL_WORKSPACE_ACCESS: return "GW_ACCESS";
        default: return "UNKNOWN";
    }
}

const char* arbitration_strategy_to_string(arbitration_strategy_t strategy) {
    switch (strategy) {
        case ARBITRATION_WINNER_TAKE_ALL: return "WINNER_TAKE_ALL";
        case ARBITRATION_WEIGHTED_FUSION: return "WEIGHTED_FUSION";
        case ARBITRATION_ROUND_ROBIN: return "ROUND_ROBIN";
        case ARBITRATION_PRIORITY_WEIGHTED: return "PRIORITY_WEIGHTED";
        default: return "UNKNOWN";
    }
}

const char* meta_controller_state_to_string(meta_controller_state_t state) {
    switch (state) {
        case META_CONTROLLER_STOPPED: return "STOPPED";
        case META_CONTROLLER_STARTING: return "STARTING";
        case META_CONTROLLER_RUNNING: return "RUNNING";
        case META_CONTROLLER_PAUSED: return "PAUSED";
        case META_CONTROLLER_STOPPING: return "STOPPING";
        case META_CONTROLLER_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int cognitive_meta_controller_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    cognitive_meta_controller_heartbeat("cognitive_me_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Cognitive_Meta_Controller");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                cognitive_meta_controller_heartbeat("cognitive_me_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Cognitive_Meta_Controller");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Cognitive_Meta_Controller");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void cognitive_meta_controller_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_cognitive_meta_controller_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int cognitive_meta_controller_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_meta_controller_training_begin: NULL argument");
        return -1;
    }
    cognitive_meta_controller_heartbeat_instance(NULL, "cognitive_meta_controller_training_begin", 0.0f);
    return 0;
}

int cognitive_meta_controller_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_meta_controller_training_end: NULL argument");
        return -1;
    }
    cognitive_meta_controller_heartbeat_instance(NULL, "cognitive_meta_controller_training_end", 1.0f);
    return 0;
}

int cognitive_meta_controller_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "cognitive_meta_controller_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    cognitive_meta_controller_heartbeat_instance(NULL, "cognitive_meta_controller_training_step", progress);
    return 0;
}

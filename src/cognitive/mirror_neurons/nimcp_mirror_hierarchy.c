/**
 * @file nimcp_mirror_hierarchy.c
 * @brief Hierarchical Action Representation Implementation
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Implementation of goal-motor hierarchy for mirror neurons
 * WHY:  Enable dual-level action understanding (goals vs motor details)
 * HOW:  Separate IPL-level goals from F5-level motors with bindings
 *
 * @see nimcp_mirror_hierarchy.h for API documentation
 */

#include "cognitive/mirror_neurons/nimcp_mirror_hierarchy.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(mirror_hierarchy, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Constants
//=============================================================================

#define GOAL_ACTIVE_THRESHOLD   0.1f
#define MOTOR_ACTIVE_THRESHOLD  0.1f

//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal hierarchy system state
 */
struct mirror_hierarchy_system {
    // Configuration
    mirror_hierarchy_config_t config;

    // Goal storage (IPL level)
    goal_representation_t* goals;
    uint32_t max_goals;
    uint32_t num_goals;

    // Motor storage (F5 level)
    motor_representation_t* motors;
    uint32_t max_motors;
    uint32_t num_motors;

    // Selected goal
    int32_t selected_goal;

    // Statistics
    uint32_t goal_inferences;
    uint32_t motor_predictions;
    float sum_prediction_error;
    uint32_t prediction_count;
    uint32_t bindings_strengthened;
    uint32_t bindings_weakened;
    uint32_t bindings_created;
    uint32_t bindings_removed;
};

static inline float exp_decay(float dt_ms, float tau_ms) {
    return expf(-dt_ms / tau_ms);
}

/**
 * @brief Find binding between goal and motor
 */
static goal_motor_binding_t* find_binding(goal_representation_t* goal, uint32_t motor_id) {
    for (uint32_t i = 0; i < goal->num_bindings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && goal->num_bindings > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(i + 1) / (float)goal->num_bindings);
        }

        if (goal->bindings[i].motor_id == motor_id) {
            return &goal->bindings[i];
        }
    }
    return NULL;
}

/**
 * @brief Compute softmax for goal competition
 */
static void softmax_goals(mirror_hierarchy_t hierarchy) {
    if (!hierarchy || hierarchy->num_goals == 0) return;

    // Find max for numerical stability
    float max_act = -1e10F;
    for (uint32_t i = 0; i < hierarchy->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hierarchy->num_goals > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(i + 1) / (float)hierarchy->num_goals);
        }

        if (hierarchy->goals[i].activation > max_act) {
            max_act = hierarchy->goals[i].activation;
        }
    }

    // Compute exp sum
    float exp_sum = 0.0F;
    for (uint32_t i = 0; i < hierarchy->num_goals; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hierarchy->num_goals > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(i + 1) / (float)hierarchy->num_goals);
        }

        exp_sum += expf(hierarchy->goals[i].activation - max_act);
    }

    // Normalize
    if (exp_sum > 0.0F) {
        for (uint32_t i = 0; i < hierarchy->num_goals; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && hierarchy->num_goals > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(i + 1) / (float)hierarchy->num_goals);
            }

            hierarchy->goals[i].activation =
                expf(hierarchy->goals[i].activation - max_act) / exp_sum;
        }
    }
}

//=============================================================================
// Lifecycle Management
//=============================================================================

mirror_hierarchy_config_t mirror_hierarchy_get_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_get_default_config", 0.0f);


    mirror_hierarchy_config_t config = {
        // Capacity
        .max_goals = NIMCP_HIERARCHY_MAX_GOALS,
        .max_motors = NIMCP_HIERARCHY_MAX_MOTORS,
        .max_bindings_per_goal = NIMCP_HIERARCHY_MAX_BINDINGS,

        // Dynamics
        .tau_goal_decay = NIMCP_HIERARCHY_TAU_GOAL_DECAY,
        .tau_motor_decay = NIMCP_HIERARCHY_TAU_MOTOR_DECAY,
        .goal_inference_threshold = NIMCP_HIERARCHY_GOAL_THRESHOLD,

        // Learning
        .binding_learning_rate = 0.05F,
        .binding_decay_rate = 0.001F,
        .min_binding_strength = 0.01F,

        // Top-down modulation
        .goal_top_down_gain = 1.0F,
        .enable_goal_competition = true,

        // Bottom-up inference
        .motor_to_goal_gain = 0.5F,
        .enable_predictive_coding = true
    };
    return config;
}

mirror_hierarchy_t mirror_hierarchy_create(const mirror_hierarchy_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_create", 0.0f);


    LOG_DEBUG("Creating mirror hierarchy system");
    mirror_hierarchy_t hierarchy = (mirror_hierarchy_t)nimcp_calloc(1, sizeof(struct mirror_hierarchy_system));
    if (!hierarchy) return NULL;
    NIMCP_API_CHECK_ALLOC(hierarchy, "Failed to allocate mirror hierarchy system");

    // Copy configuration
    if (config) {
        hierarchy->config = *config;
    } else {
        hierarchy->config = mirror_hierarchy_get_default_config();
    }

    // Allocate goal storage
    hierarchy->max_goals = hierarchy->config.max_goals;
    hierarchy->goals = (goal_representation_t*)nimcp_calloc(hierarchy->max_goals,
                                                       sizeof(goal_representation_t));
    if (!hierarchy->goals) {
        LOG_ERROR("Failed to allocate goal storage");
        nimcp_free(hierarchy);
        hierarchy = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_hierarchy_create: hierarchy->goals is NULL");
        return NULL;
    }

    // Allocate motor storage
    hierarchy->max_motors = hierarchy->config.max_motors;
    hierarchy->motors = (motor_representation_t*)nimcp_calloc(hierarchy->max_motors,
                                                         sizeof(motor_representation_t));
    if (!hierarchy->motors) {
        LOG_ERROR("Failed to allocate motor storage");
        nimcp_free(hierarchy->goals);
        nimcp_free(hierarchy);
        hierarchy = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_hierarchy_create: hierarchy->motors is NULL");
        return NULL;
    }

    hierarchy->num_goals = 0;
    hierarchy->num_motors = 0;
    hierarchy->selected_goal = -1;

    LOG_INFO("Mirror hierarchy created with max_goals=%u, max_motors=%u",
             hierarchy->max_goals, hierarchy->max_motors);

    return hierarchy;
}

void mirror_hierarchy_destroy(mirror_hierarchy_t hierarchy) {
    if (!hierarchy) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_destroy", 0.0f);


    LOG_DEBUG("Destroying mirror hierarchy with %u goals, %u motors",
              hierarchy->num_goals, hierarchy->num_motors);

    if (hierarchy->motors) nimcp_free(hierarchy->motors);
    if (hierarchy->goals) nimcp_free(hierarchy->goals);
    nimcp_free(hierarchy);
    hierarchy = NULL;
}

//=============================================================================
// Goal Management
//=============================================================================

uint32_t mirror_hierarchy_create_goal(mirror_hierarchy_t hierarchy,
                                       const char* name,
                                       goal_category_t category) {
    if (!hierarchy || hierarchy->num_goals >= hierarchy->max_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_create_goal: invalid parameters");

            return UINT32_MAX;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_create_goal", 0.0f);


    uint32_t goal_id = hierarchy->num_goals++;
    goal_representation_t* goal = &hierarchy->goals[goal_id];

    // Initialize goal
    goal->goal_id = goal_id;
    goal->category = category;
    if (name) {
        strncpy(goal->name, name, sizeof(goal->name) - 1);
        goal->name[sizeof(goal->name) - 1] = '\0';
    }

    goal->activation = 0.0F;
    goal->peak_activation = 0.0F;
    goal->target_activation = 0.0F;
    goal->top_down_bias = 0.0F;
    goal->is_selected = false;
    goal->num_bindings = 0;
    goal->num_context_features = 0;
    goal->inference_count = 0;
    goal->selection_count = 0;
    goal->total_active_time = 0.0F;

    return goal_id;
}

bool mirror_hierarchy_get_goal(mirror_hierarchy_t hierarchy, uint32_t goal_id,
                                goal_representation_t* out_goal) {
    if (!hierarchy || !out_goal || goal_id >= hierarchy->num_goals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_get_goal: invalid parameters");

            return false;
    }

    *out_goal = hierarchy->goals[goal_id];
    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_get_goal", 0.0f);


    return true;
}

void mirror_hierarchy_activate_goal(mirror_hierarchy_t hierarchy,
                                     uint32_t goal_id, float activation) {
    if (!hierarchy || goal_id >= hierarchy->num_goals) {
        if (!hierarchy) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_activate_goal: hierarchy is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_activate_goal", 0.0f);


    goal_representation_t* goal = &hierarchy->goals[goal_id];

    // Apply top-down gain
    activation *= hierarchy->config.goal_top_down_gain;

    // Set target (will be smoothed in step())
    goal->target_activation = nimcp_clampf(activation, 0.0F, 1.0F);

    // Partial immediate update
    float instant_factor = 0.3F;
    goal->activation += (goal->target_activation - goal->activation) * instant_factor;
    goal->activation = nimcp_clampf(goal->activation, 0.0F, 1.0F);

    if (goal->activation > goal->peak_activation) {
        goal->peak_activation = goal->activation;
    }
}

void mirror_hierarchy_select_goal(mirror_hierarchy_t hierarchy, int32_t goal_id) {
    if (!hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_select_goal: hierarchy is NULL");
        return;
    }

    // Clear previous selection
    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_select_goal", 0.0f);


    if (hierarchy->selected_goal >= 0 &&
        (uint32_t)hierarchy->selected_goal < hierarchy->num_goals) {
        hierarchy->goals[hierarchy->selected_goal].is_selected = false;
    }

    // Set new selection
    hierarchy->selected_goal = goal_id;

    if (goal_id >= 0 && (uint32_t)goal_id < hierarchy->num_goals) {
        goal_representation_t* goal = &hierarchy->goals[goal_id];
        goal->is_selected = true;
        goal->selection_count++;

        // Boost activation
        goal->activation = fmaxf(goal->activation, 0.5F);
        goal->top_down_bias = 0.3F;
    }
}

uint32_t mirror_hierarchy_get_selected_goal(mirror_hierarchy_t hierarchy) {
    if (!hierarchy || hierarchy->selected_goal < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_get_selected_goal: invalid parameters");

            return UINT32_MAX;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_get_selected_goal", 0.0f);


    return (uint32_t)hierarchy->selected_goal;
}

//=============================================================================
// Motor Management
//=============================================================================

uint32_t mirror_hierarchy_create_motor(mirror_hierarchy_t hierarchy,
                                        const char* name,
                                        motor_type_t type) {
    if (!hierarchy || hierarchy->num_motors >= hierarchy->max_motors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_create_motor: invalid parameters");

            return UINT32_MAX;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_create_motor", 0.0f);


    uint32_t motor_id = hierarchy->num_motors++;
    motor_representation_t* motor = &hierarchy->motors[motor_id];

    // Initialize motor
    motor->motor_id = motor_id;
    motor->type = type;
    if (name) {
        strncpy(motor->name, name, sizeof(motor->name) - 1);
        motor->name[sizeof(motor->name) - 1] = '\0';
    }

    motor->activation = 0.0F;
    motor->peak_activation = 0.0F;
    motor->num_parameters = 0;
    motor->primary_goal = UINT32_MAX;
    motor->goal_evidence = 0.0F;
    motor->predicted_activation = 0.0F;
    motor->prediction_error = 0.0F;
    motor->execution_count = 0;
    motor->total_active_time = 0.0F;

    return motor_id;
}

bool mirror_hierarchy_get_motor(mirror_hierarchy_t hierarchy, uint32_t motor_id,
                                 motor_representation_t* out_motor) {
    if (!hierarchy || !out_motor || motor_id >= hierarchy->num_motors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_get_motor: invalid parameters");

            return false;
    }

    *out_motor = hierarchy->motors[motor_id];
    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_get_motor", 0.0f);


    return true;
}

void mirror_hierarchy_activate_motor(mirror_hierarchy_t hierarchy,
                                      uint32_t motor_id, float activation) {
    if (!hierarchy || motor_id >= hierarchy->num_motors) {
        if (!hierarchy) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_activate_motor: hierarchy is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_activate_motor", 0.0f);


    motor_representation_t* motor = &hierarchy->motors[motor_id];

    motor->activation = nimcp_clampf(activation, 0.0F, 1.0F);

    if (motor->activation > motor->peak_activation) {
        motor->peak_activation = motor->activation;
    }

    // Compute prediction error if predictive coding enabled
    if (hierarchy->config.enable_predictive_coding) {
        motor->prediction_error = motor->activation - motor->predicted_activation;
        hierarchy->sum_prediction_error += fabsf(motor->prediction_error);
        hierarchy->prediction_count++;
    }

    // Drive goal inference (bottom-up)
    if (motor->activation > MOTOR_ACTIVE_THRESHOLD) {
        // Find goals bound to this motor and boost them
        for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
            /* Phase 8: Loop progress heartbeat */
            if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(g + 1) / (float)hierarchy->num_goals);
            }

            goal_representation_t* goal = &hierarchy->goals[g];
            goal_motor_binding_t* binding = find_binding(goal, motor_id);

            if (binding) {
                float boost = motor->activation * binding->binding_strength *
                             hierarchy->config.motor_to_goal_gain;
                goal->activation += boost;
                goal->activation = nimcp_clampf(goal->activation, 0.0F, 1.0F);
            }
        }
    }
}

void mirror_hierarchy_set_motor_params(mirror_hierarchy_t hierarchy,
                                        uint32_t motor_id,
                                        const float* parameters,
                                        uint32_t num_params) {
    if (!hierarchy || motor_id >= hierarchy->num_motors ||
        !parameters || num_params == 0) return;

    motor_representation_t* motor = &hierarchy->motors[motor_id];

    motor->num_parameters = (num_params > 8) ? 8 : num_params;
    memcpy(motor->parameters, parameters, motor->num_parameters * sizeof(float));
}

//=============================================================================
// Goal-Motor Binding
//=============================================================================

bool mirror_hierarchy_create_binding(mirror_hierarchy_t hierarchy,
                                      uint32_t goal_id,
                                      uint32_t motor_id,
                                      float strength,
                                      binding_type_t type) {
    if (!hierarchy || goal_id >= hierarchy->num_goals ||
        motor_id >= hierarchy->num_motors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_create_binding: invalid parameters");

            return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_create_binding", 0.0f);

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_set_motor_params", 0.0f);

    goal_representation_t* goal = &hierarchy->goals[goal_id];

    // Check if binding already exists
    goal_motor_binding_t* existing = find_binding(goal, motor_id);
    if (existing) {
        // Update existing binding
        existing->binding_strength = nimcp_clampf(strength, 0.0F, 1.0F);
        existing->binding_type = type;
        return true;
    }

    // Create new binding
    if (goal->num_bindings >= hierarchy->config.max_bindings_per_goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_hierarchy_get_selected_goal: capacity exceeded");
        return false;  // No room for more bindings
    }

    goal_motor_binding_t* binding = &goal->bindings[goal->num_bindings++];
    binding->motor_id = motor_id;
    binding->binding_strength = nimcp_clampf(strength, 0.0F, 1.0F);
    binding->binding_type = type;
    binding->usage_count = 0.0F;
    binding->last_used_time = 0.0F;

    hierarchy->bindings_created++;

    return true;
}

void mirror_hierarchy_strengthen_binding(mirror_hierarchy_t hierarchy,
                                          uint32_t goal_id,
                                          uint32_t motor_id,
                                          float delta) {
    if (!hierarchy || goal_id >= hierarchy->num_goals) {
        if (!hierarchy) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_strengthen_binding: hierarchy is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_strengthen_binding", 0.0f);


    goal_representation_t* goal = &hierarchy->goals[goal_id];
    goal_motor_binding_t* binding = find_binding(goal, motor_id);

    if (binding) {
        float old_strength = binding->binding_strength;
        binding->binding_strength += delta * hierarchy->config.binding_learning_rate;
        binding->binding_strength = nimcp_clampf(binding->binding_strength, 0.0F, 1.0F);
        binding->usage_count += 1.0F;

        if (binding->binding_strength > old_strength) {
            hierarchy->bindings_strengthened++;
        } else if (binding->binding_strength < old_strength) {
            hierarchy->bindings_weakened++;
        }
    }
}

float mirror_hierarchy_get_binding(mirror_hierarchy_t hierarchy,
                                    uint32_t goal_id,
                                    uint32_t motor_id) {
    if (!hierarchy || goal_id >= hierarchy->num_goals) {
        if (!hierarchy) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_get_binding: hierarchy is NULL");
        return -1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_get_binding", 0.0f);


    goal_representation_t* goal = &hierarchy->goals[goal_id];
    goal_motor_binding_t* binding = find_binding(goal, motor_id);

    return binding ? binding->binding_strength : -1.0F;
}

//=============================================================================
// Inference
//=============================================================================

uint32_t mirror_hierarchy_infer_goal(mirror_hierarchy_t hierarchy,
                                      uint32_t motor_id,
                                      uint32_t* out_goal_ids,
                                      float* out_probs,
                                      uint32_t max_goals) {
    if (!hierarchy || motor_id >= hierarchy->num_motors ||
        !out_goal_ids || !out_probs || max_goals == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_infer_goal: invalid parameters");

            return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_infer_goal", 0.0f);

    // Compute likelihood for each goal given this motor
    float* likelihoods = (float*)nimcp_malloc(hierarchy->num_goals * sizeof(float));
    if (!likelihoods) {
        return 0;
    }
    float total_likelihood = 0.0F;

    for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
        /* Phase 8: Loop progress heartbeat */
        if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(g + 1) / (float)hierarchy->num_goals);
        }

        goal_representation_t* goal = &hierarchy->goals[g];
        goal_motor_binding_t* binding = find_binding(goal, motor_id);

        if (binding) {
            likelihoods[g] = binding->binding_strength;
            total_likelihood += likelihoods[g];
        } else {
            likelihoods[g] = 0.0F;
        }
    }

    // Normalize to probabilities
    if (total_likelihood > 0.0F) {
        for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
            /* Phase 8: Loop progress heartbeat */
            if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(g + 1) / (float)hierarchy->num_goals);
            }

            likelihoods[g] /= total_likelihood;
        }
    }

    // Sort by probability and return top goals
    uint32_t count = 0;
    for (uint32_t g = 0; g < hierarchy->num_goals && count < max_goals; g++) {
        if (likelihoods[g] > hierarchy->config.goal_inference_threshold) {
            // Find insertion point (sorted descending)
            uint32_t insert_pos = count;
            for (uint32_t i = 0; i < count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && count > 256) {
                    mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                     (float)(i + 1) / (float)count);
                }

                if (likelihoods[g] > out_probs[i]) {
                    insert_pos = i;
                    break;
                }
            }

            // Shift existing entries
            for (uint32_t i = count; i > insert_pos; i--) {
                out_goal_ids[i] = out_goal_ids[i-1];
                out_probs[i] = out_probs[i-1];
            }

            // Insert
            out_goal_ids[insert_pos] = g;
            out_probs[insert_pos] = likelihoods[g];
            count++;

            // Update goal statistics
            hierarchy->goals[g].inference_count++;
        }
    }

    hierarchy->goal_inferences++;

    nimcp_free(likelihoods);
    return count;
}

uint32_t mirror_hierarchy_predict_motor(mirror_hierarchy_t hierarchy,
                                         uint32_t goal_id,
                                         uint32_t* out_motor_ids,
                                         float* out_probs,
                                         uint32_t max_motors) {
    if (!hierarchy || goal_id >= hierarchy->num_goals ||
        !out_motor_ids || !out_probs || max_motors == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_predict_motor: invalid parameters");

            return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_predict_motor", 0.0f);

    goal_representation_t* goal = &hierarchy->goals[goal_id];
    uint32_t count = 0;

    // Normalize binding strengths to probabilities
    float total_strength = 0.0F;
    for (uint32_t i = 0; i < goal->num_bindings; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && goal->num_bindings > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(i + 1) / (float)goal->num_bindings);
        }

        total_strength += goal->bindings[i].binding_strength;
    }

    // Output bindings sorted by strength
    for (uint32_t b = 0; b < goal->num_bindings && count < max_motors; b++) {
        goal_motor_binding_t* binding = &goal->bindings[b];
        float prob = (total_strength > 0.0F) ?
                     binding->binding_strength / total_strength : 0.0F;

        // Find insertion point (sorted descending)
        uint32_t insert_pos = count;
        for (uint32_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(i + 1) / (float)count);
            }

            if (prob > out_probs[i]) {
                insert_pos = i;
                break;
            }
        }

        // Shift existing entries
        for (uint32_t i = count; i > insert_pos; i--) {
            out_motor_ids[i] = out_motor_ids[i-1];
            out_probs[i] = out_probs[i-1];
        }

        // Insert
        out_motor_ids[insert_pos] = binding->motor_id;
        out_probs[insert_pos] = prob;
        count++;

        // Update motor prediction
        if (hierarchy->config.enable_predictive_coding) {
            motor_representation_t* motor = &hierarchy->motors[binding->motor_id];
            motor->predicted_activation = goal->activation * binding->binding_strength;
        }
    }

    hierarchy->motor_predictions++;

    return count;
}

uint32_t mirror_hierarchy_infer_goal_contextual(mirror_hierarchy_t hierarchy,
                                                 uint32_t motor_id,
                                                 const float* context_features,
                                                 uint32_t num_features) {
    if (!hierarchy || motor_id >= hierarchy->num_motors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "mirror_hierarchy_infer_goal_contextual: invalid parameters");

            return UINT32_MAX;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_infer_goal_contextua", 0.0f);


    uint32_t best_goal = UINT32_MAX;
    float best_score = -1.0F;

    for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
        /* Phase 8: Loop progress heartbeat */
        if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(g + 1) / (float)hierarchy->num_goals);
        }

        goal_representation_t* goal = &hierarchy->goals[g];
        goal_motor_binding_t* binding = find_binding(goal, motor_id);

        if (!binding) continue;

        float score = binding->binding_strength;

        // Add context similarity bonus if context provided
        if (context_features && num_features > 0 && goal->num_context_features > 0) {
            // Compute dot product similarity
            float similarity = 0.0F;
            uint32_t min_features = (num_features < goal->num_context_features) ?
                                   num_features : goal->num_context_features;

            for (uint32_t f = 0; f < min_features; f++) {
                /* Phase 8: Loop progress heartbeat */
                if ((f & 0xFF) == 0 && min_features > 256) {
                    mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                     (float)(f + 1) / (float)min_features);
                }

                similarity += context_features[f] * goal->context_features[f];
            }

            // Normalize and add to score
            if (min_features > 0) {
                similarity /= min_features;
                score *= (1.0F + similarity);
            }
        }

        if (score > best_score) {
            best_score = score;
            best_goal = g;
        }
    }

    if (best_goal != UINT32_MAX) {
        hierarchy->goals[best_goal].inference_count++;
        hierarchy->goal_inferences++;
    }

    return best_goal;
}

//=============================================================================
// Time Update
//=============================================================================

void mirror_hierarchy_step(mirror_hierarchy_t hierarchy, float dt_ms) {
    if (!hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_step: hierarchy is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_step", 0.0f);


    const mirror_hierarchy_config_t* cfg = &hierarchy->config;

    // Decay factors
    float goal_decay = exp_decay(dt_ms, cfg->tau_goal_decay);
    float motor_decay = exp_decay(dt_ms, cfg->tau_motor_decay);

    // Update goals
    for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
        /* Phase 8: Loop progress heartbeat */
        if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(g + 1) / (float)hierarchy->num_goals);
        }

        goal_representation_t* goal = &hierarchy->goals[g];

        // Decay activation
        goal->activation *= goal_decay;

        // Add top-down bias if selected
        if (goal->is_selected) {
            goal->activation += goal->top_down_bias * (dt_ms / 1000.0F);
        }

        goal->activation = nimcp_clampf(goal->activation, 0.0F, 1.0F);

        // Decay top-down bias
        goal->top_down_bias *= 0.99F;

        // Track active time
        if (goal->activation > GOAL_ACTIVE_THRESHOLD) {
            goal->total_active_time += dt_ms;
        }

        // Decay peak
        goal->peak_activation *= 0.999F;

        // Decay unused bindings
        for (uint32_t b = 0; b < goal->num_bindings; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && goal->num_bindings > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(b + 1) / (float)goal->num_bindings);
            }

            goal_motor_binding_t* binding = &goal->bindings[b];
            binding->binding_strength -= cfg->binding_decay_rate * (dt_ms / 1000.0F);

            if (binding->binding_strength < cfg->min_binding_strength) {
                // Remove weak binding by shifting
                for (uint32_t j = b; j < goal->num_bindings - 1; j++) {
                    goal->bindings[j] = goal->bindings[j + 1];
                }
                goal->num_bindings--;
                hierarchy->bindings_removed++;
                b--;  // Re-check this slot
            }
        }
    }

    // Apply goal competition if enabled
    if (cfg->enable_goal_competition) {
        softmax_goals(hierarchy);
    }

    // Update motors
    for (uint32_t m = 0; m < hierarchy->num_motors; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && hierarchy->num_motors > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(m + 1) / (float)hierarchy->num_motors);
        }

        motor_representation_t* motor = &hierarchy->motors[m];

        // Decay activation
        motor->activation *= motor_decay;
        motor->activation = nimcp_clampf(motor->activation, 0.0F, 1.0F);

        // Decay prediction
        motor->predicted_activation *= motor_decay;

        // Track active time
        if (motor->activation > MOTOR_ACTIVE_THRESHOLD) {
            motor->total_active_time += dt_ms;
        }

        // Decay peak
        motor->peak_activation *= 0.999F;

        // Update primary goal association
        float max_evidence = 0.0F;
        for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
            /* Phase 8: Loop progress heartbeat */
            if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(g + 1) / (float)hierarchy->num_goals);
            }

            goal_representation_t* goal = &hierarchy->goals[g];
            goal_motor_binding_t* binding = find_binding(goal, m);

            if (binding && binding->binding_strength > max_evidence) {
                max_evidence = binding->binding_strength;
                motor->primary_goal = g;
            }
        }
        motor->goal_evidence = max_evidence;
    }

    // Generate top-down predictions for selected goal
    if (hierarchy->selected_goal >= 0 && cfg->enable_predictive_coding) {
        goal_representation_t* selected = &hierarchy->goals[hierarchy->selected_goal];

        for (uint32_t b = 0; b < selected->num_bindings; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && selected->num_bindings > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(b + 1) / (float)selected->num_bindings);
            }

            goal_motor_binding_t* binding = &selected->bindings[b];
            motor_representation_t* motor = &hierarchy->motors[binding->motor_id];

            // Predict motor from goal
            motor->predicted_activation = selected->activation * binding->binding_strength;
        }
    }
}

//=============================================================================
// Statistics
//=============================================================================

bool mirror_hierarchy_get_stats(mirror_hierarchy_t hierarchy, mirror_hierarchy_stats_t* stats) {
    if (!hierarchy || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_get_stats: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_get_stats", 0.0f);


    memset(stats, 0, sizeof(mirror_hierarchy_stats_t));

    stats->num_goals = hierarchy->num_goals;
    stats->num_motors = hierarchy->num_motors;

    // Count bindings and compute activation stats
    float sum_goal_act = 0.0F;
    float sum_motor_act = 0.0F;

    for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
        /* Phase 8: Loop progress heartbeat */
        if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(g + 1) / (float)hierarchy->num_goals);
        }

        goal_representation_t* goal = &hierarchy->goals[g];

        stats->num_bindings += goal->num_bindings;
        sum_goal_act += goal->activation;

        if (goal->activation > GOAL_ACTIVE_THRESHOLD) {
            stats->active_goals++;
        }
    }

    for (uint32_t m = 0; m < hierarchy->num_motors; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && hierarchy->num_motors > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(m + 1) / (float)hierarchy->num_motors);
        }

        motor_representation_t* motor = &hierarchy->motors[m];

        sum_motor_act += motor->activation;

        if (motor->activation > MOTOR_ACTIVE_THRESHOLD) {
            stats->active_motors++;
        }
    }

    if (hierarchy->num_goals > 0) {
        stats->mean_goal_activation = sum_goal_act / hierarchy->num_goals;
    }

    if (hierarchy->num_motors > 0) {
        stats->mean_motor_activation = sum_motor_act / hierarchy->num_motors;
    }

    // Inference stats
    stats->goal_inferences = hierarchy->goal_inferences;
    stats->motor_predictions = hierarchy->motor_predictions;

    if (hierarchy->prediction_count > 0) {
        stats->avg_prediction_error = hierarchy->sum_prediction_error / hierarchy->prediction_count;
    }

    // Learning stats
    stats->bindings_strengthened = hierarchy->bindings_strengthened;
    stats->bindings_weakened = hierarchy->bindings_weakened;
    stats->bindings_created = hierarchy->bindings_created;
    stats->bindings_removed = hierarchy->bindings_removed;

    return true;
}

void mirror_hierarchy_reset_stats(mirror_hierarchy_t hierarchy) {
    if (!hierarchy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_hierarchy_reset_stats: hierarchy is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_reset_stats", 0.0f);


    hierarchy->goal_inferences = 0;
    hierarchy->motor_predictions = 0;
    hierarchy->sum_prediction_error = 0.0F;
    hierarchy->prediction_count = 0;
    hierarchy->bindings_strengthened = 0;
    hierarchy->bindings_weakened = 0;
    hierarchy->bindings_created = 0;
    hierarchy->bindings_removed = 0;

    for (uint32_t g = 0; g < hierarchy->num_goals; g++) {
        /* Phase 8: Loop progress heartbeat */
        if ((g & 0xFF) == 0 && hierarchy->num_goals > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(g + 1) / (float)hierarchy->num_goals);
        }

        goal_representation_t* goal = &hierarchy->goals[g];
        goal->inference_count = 0;
        goal->selection_count = 0;
        goal->total_active_time = 0.0F;
        goal->peak_activation = 0.0F;

        for (uint32_t b = 0; b < goal->num_bindings; b++) {
            /* Phase 8: Loop progress heartbeat */
            if ((b & 0xFF) == 0 && goal->num_bindings > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(b + 1) / (float)goal->num_bindings);
            }

            goal->bindings[b].usage_count = 0.0F;
        }
    }

    for (uint32_t m = 0; m < hierarchy->num_motors; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && hierarchy->num_motors > 256) {
            mirror_hierarchy_heartbeat("mirror_hiera_loop",
                             (float)(m + 1) / (float)hierarchy->num_motors);
        }

        motor_representation_t* motor = &hierarchy->motors[m];
        motor->execution_count = 0;
        motor->total_active_time = 0.0F;
        motor->peak_activation = 0.0F;
    }
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int mirror_hierarchy_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    mirror_hierarchy_heartbeat("mirror_hiera_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mirror_Hierarchy");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mirror_hierarchy_heartbeat("mirror_hiera_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Mirror hierarchy self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mirror_Hierarchy");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mirror_Hierarchy");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void mirror_hierarchy_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_hierarchy_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int mirror_hierarchy_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hierarchy_training_begin: NULL argument");
        return -1;
    }
    mirror_hierarchy_heartbeat_instance(NULL, "mirror_hierarchy_training_begin", 0.0f);
    return 0;
}

int mirror_hierarchy_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hierarchy_training_end: NULL argument");
        return -1;
    }
    mirror_hierarchy_heartbeat_instance(NULL, "mirror_hierarchy_training_end", 1.0f);
    return 0;
}

int mirror_hierarchy_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_hierarchy_training_step: NULL argument");
        return -1;
    }
    mirror_hierarchy_heartbeat_instance(NULL, "mirror_hierarchy_training_step", progress);
    return 0;
}

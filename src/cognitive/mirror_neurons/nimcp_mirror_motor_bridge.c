/**
 * @file nimcp_mirror_motor_bridge.c
 * @brief Mirror Neuron - Motor Cortex Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-01-05
 *
 * WHAT: Implementation of mirror neuron to motor cortex integration
 * WHY:  Enable motor program extraction from observation and imitation execution
 * HOW:  F5 mirror neurons project to M1 motor cortex for imitation learning
 *
 * @see nimcp_mirror_motor_bridge.h for API documentation
 */

#include "cognitive/mirror_neurons/nimcp_mirror_motor_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_motor_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_motor_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_motor_bridge_mesh_registry = NULL;

nimcp_error_t mirror_motor_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_motor_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_motor_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_motor_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_motor_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_motor_bridge_mesh_registry = registry;
    return err;
}

void mirror_motor_bridge_mesh_unregister(void) {
    if (g_mirror_motor_bridge_mesh_registry && g_mirror_motor_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_motor_bridge_mesh_registry, g_mirror_motor_bridge_mesh_id);
        g_mirror_motor_bridge_mesh_id = 0;
        g_mirror_motor_bridge_mesh_registry = NULL;
    }
}


#define LOG_MODULE "MIRROR_MOTOR_BRIDGE"

/** @brief Send heartbeat from mirror_motor_bridge module (instance-level) */
static inline void mirror_motor_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_motor_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_motor_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_motor_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_motor_bridge)

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MIRROR_MOTOR_LOG_TAG "mirror_motor_bridge"

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline float vec3_distance(const motor_vec3_t* a, const motor_vec3_t* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Find program by action ID
 */
static int32_t find_program_by_action(const mirror_motor_bridge_t* bridge, uint32_t action_id) {
    for (uint32_t i = 0; i < bridge->num_programs; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_programs > 256) {
            mirror_motor_bridge_heartbeat("mirror_motor_loop",
                             (float)(i + 1) / (float)bridge->num_programs);
        }

        if (bridge->programs[i].action_id == action_id) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Find execution by program ID
 */
static int32_t find_execution_by_program(const mirror_motor_bridge_t* bridge, uint32_t program_id) {
    for (uint32_t i = 0; i < bridge->num_executions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_executions > 256) {
            mirror_motor_bridge_heartbeat("mirror_motor_loop",
                             (float)(i + 1) / (float)bridge->num_executions);
        }

        if (bridge->executions[i].program_id == program_id &&
            bridge->executions[i].is_executing) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Get next available program slot
 */
static int32_t get_free_program_slot(mirror_motor_bridge_t* bridge) {
    if (bridge->num_programs < bridge->max_programs) {
        return (int32_t)(bridge->num_programs++);
    }
    /* No free slots - could implement LRU eviction here */
    return -1;
}

/**
 * @brief Get next available execution slot
 */
static int32_t get_free_execution_slot(mirror_motor_bridge_t* bridge) {
    /* First look for inactive slot */
    for (uint32_t i = 0; i < bridge->num_executions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_executions > 256) {
            mirror_motor_bridge_heartbeat("mirror_motor_loop",
                             (float)(i + 1) / (float)bridge->num_executions);
        }

        if (!bridge->executions[i].is_executing) {
            return (int32_t)i;
        }
    }
    /* Allocate new if available */
    if (bridge->num_executions < bridge->max_executions) {
        return (int32_t)(bridge->num_executions++);
    }
    return -1;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int mirror_motor_bridge_default_config(mirror_motor_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Resonance-motor coupling */
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_default_config", 0.0f);


    config->resonance_gain = MIRROR_MOTOR_DEFAULT_GAIN;
    config->execution_threshold = MIRROR_MOTOR_EXEC_THRESHOLD;
    config->f5_m1_delay_ms = MIRROR_MOTOR_F5_M1_DELAY_MS;

    /* Program extraction */
    config->min_observation_strength = 0.3f;
    config->extraction_threshold = 0.5f;
    config->min_observations = 1;

    /* Execution control */
    config->enable_automatic_imitation = false;
    config->enable_learning_mode = true;
    config->learning_release_strength = 0.8f;

    /* Motor adaptation */
    config->enable_motor_adaptation = true;
    config->adaptation_rate = 0.1f;

    /* Feature enables */
    config->enable_program_extraction = true;
    config->enable_resonance_gating = true;
    config->enable_cerebellar_timing = false;

    return 0;
}

mirror_motor_bridge_t* mirror_motor_bridge_create(
    const mirror_motor_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_create", 0.0f);


    LOG_DEBUG("Creating mirror-motor bridge");

    /* Allocate bridge */
    mirror_motor_bridge_t* bridge = nimcp_calloc(1, sizeof(mirror_motor_bridge_t));
    if (!bridge) {
        LOG_ERROR("Failed to allocate mirror-motor bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_motor_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_MIRROR_MOTOR_BRIDGE,
                         "mirror_motor_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_bridge_create: bridge is NULL");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        mirror_motor_bridge_default_config(&bridge->config);
    }

    /* Allocate program storage */
    bridge->max_programs = MIRROR_MOTOR_MAX_PROGRAMS;
    bridge->programs = nimcp_calloc(bridge->max_programs, sizeof(extracted_motor_program_t));
    if (!bridge->programs) {
        LOG_ERROR("Failed to allocate program storage");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_motor_bridge_create: bridge->programs is NULL");
        return NULL;
    }
    bridge->num_programs = 0;

    /* Allocate execution tracking */
    bridge->max_executions = MIRROR_MOTOR_MAX_EFFECTORS;
    bridge->executions = nimcp_calloc(bridge->max_executions, sizeof(imitation_state_t));
    if (!bridge->executions) {
        LOG_ERROR("Failed to allocate execution tracking");
        nimcp_free(bridge->programs);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_motor_bridge_create: bridge->executions is NULL");
        return NULL;
    }
    bridge->num_executions = 0;

    /* Initialize state */
    bridge->learning_mode = false;
    bridge->current_suppression_release = 0.0f;
    bridge->last_update_ms = 0;

    LOG_INFO("Mirror-motor bridge created (max_programs=%u, max_executions=%u)",
             bridge->max_programs, bridge->max_executions);

    return bridge;
}

void mirror_motor_bridge_destroy(mirror_motor_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_motor");

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_destroy", 0.0f);


    LOG_DEBUG("Destroying mirror-motor bridge");

    /* Free storage */
    if (bridge->executions) {
        nimcp_free(bridge->executions);
    }
    if (bridge->programs) {
        nimcp_free(bridge->programs);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int mirror_motor_bridge_connect_resonance(
    mirror_motor_bridge_t* bridge,
    motor_resonance_t resonance
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_connect_resonance", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(resonance);

    bridge->resonance = resonance;
    bridge->base.system_a = resonance;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = bridge->base.system_b_connected;

    LOG_DEBUG("Connected motor resonance to mirror-motor bridge");
    return 0;
}

int mirror_motor_bridge_connect_motor(
    mirror_motor_bridge_t* bridge,
    motor_adapter_t* motor
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_connect_motor", 0.0f);


    BRIDGE_NULL_CHECK(bridge);
    BRIDGE_NULL_CHECK(motor);

    bridge->motor = motor;
    bridge->base.system_b = motor;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = bridge->base.system_a_connected;

    LOG_DEBUG("Connected motor cortex to mirror-motor bridge");
    return 0;
}

/* ============================================================================
 * Motor Program Extraction API Implementation
 * ============================================================================ */

uint32_t mirror_motor_extract_program(
    mirror_motor_bridge_t* bridge,
    uint32_t action_id,
    float observation_strength,
    motor_region_t region
) {
    if (!bridge || !bridge->config.enable_program_extraction) {
        return UINT32_MAX;
    }

    /* Check minimum observation strength */
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_extract", 0.0f);


    if (observation_strength < bridge->config.min_observation_strength) {
        return UINT32_MAX;
    }

    /* Check if program already exists for this action */
    int32_t existing = find_program_by_action(bridge, action_id);
    if (existing >= 0) {
        /* Update existing program */
        extracted_motor_program_t* prog = &bridge->programs[existing];
        prog->observation_count++;
        prog->observation_strength = fmaxf(prog->observation_strength, observation_strength);
        prog->extraction_confidence = nimcp_myelin_clamp(
            prog->extraction_confidence + 0.1f, 0.0f, 1.0f
        );
        return prog->program_id;
    }

    /* Get free slot */
    int32_t slot = get_free_program_slot(bridge);
    if (slot < 0) {
        LOG_WARN("No free program slots available");
        bridge->stats.failed_extractions++;
        return UINT32_MAX;
    }

    /* Create new program */
    extracted_motor_program_t* prog = &bridge->programs[slot];
    prog->program_id = (uint32_t)slot + 1; /* 1-indexed */
    prog->action_id = action_id;
    prog->primary_region = region;
    prog->type = MOVEMENT_TYPE_DISCRETE;

    /* Set default kinematic parameters */
    prog->start_position = (motor_vec3_t){0.0f, 0.0f, 0.0f};
    prog->end_position = (motor_vec3_t){0.0f, 0.0f, 0.0f};
    prog->peak_velocity = (motor_vec3_t){0.0f, 0.0f, 0.0f};
    prog->duration_ms = 500.0f; /* Default 500ms */
    prog->force_estimate = 0.5f;
    prog->precision_required = 0.5f;

    /* Set extraction metadata */
    prog->extraction_confidence = observation_strength;
    prog->observation_strength = observation_strength;
    prog->extraction_time_ms = bridge->last_update_ms;
    prog->observation_count = 1;

    /* Update statistics */
    bridge->stats.total_extractions++;
    bridge->stats.successful_extractions++;
    bridge->effects.programs_extracted++;

    LOG_DEBUG("Extracted motor program %u from action %u (confidence=%.2f)",
              prog->program_id, action_id, prog->extraction_confidence);

    return prog->program_id;
}

uint32_t mirror_motor_extract_program_full(
    mirror_motor_bridge_t* bridge,
    uint32_t action_id,
    const motor_vec3_t* start,
    const motor_vec3_t* end,
    float duration_ms,
    motor_region_t region,
    movement_type_t type
) {
    if (!bridge || !start || !end) {
        return UINT32_MAX;
    }
    BRIDGE_BBB_VALIDATE(bridge, start, sizeof(*start));
    BRIDGE_BBB_VALIDATE(bridge, end, sizeof(*end));

    /* First extract basic program */
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_extract", 0.0f);


    uint32_t program_id = mirror_motor_extract_program(
        bridge, action_id, 1.0f, region
    );
    if (program_id == UINT32_MAX) {
        return UINT32_MAX;
    }

    /* Update with full parameters */
    int32_t slot = find_program_by_action(bridge, action_id);
    if (slot < 0) {
        return UINT32_MAX;
    }

    extracted_motor_program_t* prog = &bridge->programs[slot];
    prog->start_position = *start;
    prog->end_position = *end;
    prog->duration_ms = duration_ms;
    prog->type = type;

    /* Estimate velocity from positions and duration */
    float dist = vec3_distance(start, end);
    float duration_sec = duration_ms / 1000.0f;
    float peak_vel = (duration_sec > 1e-6f) ? (2.0f * dist) / duration_sec : 0.0f;
    if (dist < 1e-6f) {
        prog->peak_velocity = (motor_vec3_t){ .x = 0.0f, .y = 0.0f, .z = 0.0f };
    } else {
        prog->peak_velocity = (motor_vec3_t){
            .x = (end->x - start->x) * peak_vel / dist,
            .y = (end->y - start->y) * peak_vel / dist,
            .z = (end->z - start->z) * peak_vel / dist
        };
    }

    prog->extraction_confidence = 1.0f; /* Full specification */

    return program_id;
}

int mirror_motor_get_program(
    const mirror_motor_bridge_t* bridge,
    uint32_t program_id,
    extracted_motor_program_t* program
) {
    if (!bridge || !program || program_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_get_program: required parameter is NULL (bridge, program)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_get_pro", 0.0f);


    uint32_t index = program_id - 1; /* 1-indexed */
    if (index >= bridge->num_programs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_motor_get_program: capacity exceeded");
        return -1;
    }

    *program = bridge->programs[index];
    return 0;
}

/* ============================================================================
 * Imitation Execution API Implementation
 * ============================================================================ */

int mirror_motor_execute_program(
    mirror_motor_bridge_t* bridge,
    uint32_t program_id
) {
    if (!bridge || program_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_motor_execute_program: bridge is NULL");
        return -1;
    }

    /* Check program exists */
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_execute", 0.0f);


    uint32_t prog_index = program_id - 1;
    if (prog_index >= bridge->num_programs) {
        LOG_WARN("Program %u not found", program_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "mirror_motor_execute_program: capacity exceeded");
        return -1;
    }

    extracted_motor_program_t* prog = &bridge->programs[prog_index];

    /* Check resonance gating */
    if (bridge->config.enable_resonance_gating && bridge->resonance) {
        uint32_t channel = motor_resonance_find_channel(bridge->resonance, prog->action_id);
        if (channel != UINT32_MAX) {
            float output = motor_resonance_get_output(bridge->resonance, channel);
            if (output < bridge->config.execution_threshold && !bridge->learning_mode) {
                /* Suppressed by resonance gating */
                LOG_DEBUG("Execution suppressed for program %u (output=%.2f)",
                          program_id, output);
                bridge->stats.suppressed_executions++;
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_execute_program: bridge->learning_mode is NULL");
                return -1;
            }
        }
    }

    /* Check motor cortex connection */
    if (!bridge->motor) {
        LOG_WARN("Motor cortex not connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_execute_program: bridge->motor is NULL");
        return -1;
    }

    /* Get execution slot */
    int32_t exec_slot = get_free_execution_slot(bridge);
    if (exec_slot < 0) {
        LOG_WARN("No free execution slots");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_motor_execute_program: validation failed");
        return -1;
    }

    /* Create motor goal */
    motor_goal_t goal = {
        .region = prog->primary_region,
        .target_position = prog->end_position,
        .target_velocity = {0.0f, 0.0f, 0.0f}, /* Stop at end */
        .max_duration_ms = prog->duration_ms * 1.5f,
        .precision_required = prog->precision_required,
        .type = prog->type,
        .urgency = 0.5f
    };

    /* Plan movement on motor cortex */
    if (!motor_plan_movement(bridge->motor, &goal)) {
        LOG_WARN("Failed to plan movement for program %u", program_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_motor_execute_program: motor_plan_movement is NULL");
        return -1;
    }

    /* Begin execution */
    if (!motor_begin_execution(bridge->motor)) {
        LOG_WARN("Failed to begin execution for program %u", program_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_motor_execute_program: motor_begin_execution is NULL");
        return -1;
    }

    /* Track execution */
    imitation_state_t* exec = &bridge->executions[exec_slot];
    exec->program_id = program_id;
    exec->action_id = prog->action_id;
    exec->is_executing = true;
    exec->execution_progress = 0.0f;
    exec->resonance_level = prog->observation_strength;
    exec->suppression_level = bridge->learning_mode ? 0.0f : 0.5f;
    exec->motor_output = exec->resonance_level - exec->suppression_level;
    exec->start_time_ms = bridge->last_update_ms;
    exec->elapsed_ms = 0;
    exec->planned_duration_ms = prog->duration_ms;
    exec->current_position = prog->start_position;
    exec->position_error = (motor_vec3_t){0.0f, 0.0f, 0.0f};
    exec->accuracy = 1.0f;

    /* Update statistics */
    bridge->stats.total_executions++;
    bridge->effects.executions_triggered++;
    bridge->state.executing_programs++;

    LOG_DEBUG("Started execution of program %u (duration=%.0fms)",
              program_id, prog->duration_ms);

    return 0;
}

uint32_t mirror_motor_execute_from_resonance(
    mirror_motor_bridge_t* bridge
) {
    if (!bridge || !bridge->resonance) {
        return UINT32_MAX;
    }

    /* Find channels above threshold */
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_execute", 0.0f);


    uint32_t active_channels[16];
    uint32_t num_active = motor_resonance_get_active_channels(
        bridge->resonance, active_channels, 16
    );

    if (num_active == 0) {
        return UINT32_MAX;
    }

    /* Find highest resonance channel with extracted program */
    float best_output = 0.0f;
    uint32_t best_program = UINT32_MAX;

    for (uint32_t i = 0; i < num_active; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_active > 256) {
            mirror_motor_bridge_heartbeat("mirror_motor_loop",
                             (float)(i + 1) / (float)num_active);
        }

        motor_channel_t channel;
        if (motor_resonance_get_channel(bridge->resonance, active_channels[i], &channel)) {
            if (channel.motor_output > best_output) {
                /* Check if we have a program for this action */
                int32_t prog_idx = find_program_by_action(bridge, channel.action_id);
                if (prog_idx >= 0) {
                    best_output = channel.motor_output;
                    best_program = bridge->programs[prog_idx].program_id;
                }
            }
        }
    }

    if (best_program == UINT32_MAX) {
        return UINT32_MAX;
    }

    /* Execute best program */
    if (mirror_motor_execute_program(bridge, best_program) == 0) {
        return best_program;
    }

    return UINT32_MAX;
}

int mirror_motor_get_execution_state(
    const mirror_motor_bridge_t* bridge,
    uint32_t program_id,
    imitation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_get_execution_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_get_exe", 0.0f);


    int32_t exec_idx = find_execution_by_program(bridge, program_id);
    if (exec_idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_motor_get_execution_state: validation failed");
        return -1;
    }

    *state = bridge->executions[exec_idx];
    return 0;
}

int mirror_motor_stop_execution(
    mirror_motor_bridge_t* bridge,
    uint32_t program_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_stop_ex", 0.0f);


    int32_t exec_idx = find_execution_by_program(bridge, program_id);
    if (exec_idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_motor_stop_execution: validation failed");
        return -1;
    }

    /* Stop motor execution */
    if (bridge->motor) {
        motor_stop_execution(bridge->motor);
    }

    /* Clear execution state */
    bridge->executions[exec_idx].is_executing = false;
    bridge->state.executing_programs--;

    LOG_DEBUG("Stopped execution of program %u", program_id);
    return 0;
}

/* ============================================================================
 * Learning Mode API Implementation
 * ============================================================================ */

int mirror_motor_enter_learning_mode(
    mirror_motor_bridge_t* bridge,
    float strength
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_enter_l", 0.0f);


    strength = nimcp_myelin_clamp(strength, 0.0f, 1.0f);

    bridge->learning_mode = true;
    bridge->current_suppression_release = strength * bridge->config.learning_release_strength;
    bridge->state.in_learning_mode = true;
    bridge->state.suppression_released = true;

    /* Release suppression on resonance system */
    if (bridge->resonance) {
        motor_resonance_release_for_learning(
            bridge->resonance, -1, /* all channels */
            bridge->current_suppression_release
        );
    }

    bridge->stats.learning_mode_activations++;

    LOG_DEBUG("Entered learning mode (strength=%.2f, release=%.2f)",
              strength, bridge->current_suppression_release);

    return 0;
}

int mirror_motor_exit_learning_mode(
    mirror_motor_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_exit_le", 0.0f);


    bridge->learning_mode = false;
    bridge->current_suppression_release = 0.0f;
    bridge->state.in_learning_mode = false;
    bridge->state.suppression_released = false;

    /* Restore suppression */
    if (bridge->resonance) {
        motor_resonance_set_bg_inhibition(bridge->resonance, 0.5f); /* Default tonic */
    }

    LOG_DEBUG("Exited learning mode");
    return 0;
}

bool mirror_motor_is_learning_mode(
    const mirror_motor_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_mirror_motor_is_lear", 0.0f);


    return bridge ? bridge->learning_mode : false;
}

/* ============================================================================
 * Update Cycle Implementation
 * ============================================================================ */

int mirror_motor_bridge_update(
    mirror_motor_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_update", 0.0f);


    bridge->last_update_ms += delta_ms;

    /* Reset per-cycle effects */
    bridge->effects.programs_extracted = 0;
    bridge->effects.executions_triggered = 0;

    /* Update resonance state */
    if (bridge->resonance) {
        motor_resonance_stats_t res_stats;
        if (motor_resonance_get_stats(bridge->resonance, &res_stats)) {
            bridge->state.active_mirror_channels = res_stats.active_channels;
            bridge->state.max_resonance = res_stats.max_resonance;
            bridge->state.mean_resonance = res_stats.mean_resonance;
            bridge->effects.current_resonance = res_stats.max_resonance;
            bridge->effects.motor_priming = res_stats.mean_resonance * bridge->config.resonance_gain;
        }
    }

    /* Update motor cortex state */
    if (bridge->motor) {
        bridge->state.motor_status = motor_get_status(bridge->motor);
    }

    /* Update execution tracking */
    float total_accuracy = 0.0f;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < bridge->num_executions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_executions > 256) {
            mirror_motor_bridge_heartbeat("mirror_motor_loop",
                             (float)(i + 1) / (float)bridge->num_executions);
        }

        imitation_state_t* exec = &bridge->executions[i];
        if (!exec->is_executing) continue;

        exec->elapsed_ms += delta_ms;
        exec->execution_progress = nimcp_myelin_clamp(
            (float)exec->elapsed_ms / exec->planned_duration_ms,
            0.0f, 1.0f
        );

        /* Check for completion */
        if (exec->execution_progress >= 1.0f ||
            (bridge->motor && motor_get_status(bridge->motor) == MOTOR_STATUS_COMPLETE)) {

            exec->is_executing = false;
            bridge->state.executing_programs--;
            bridge->stats.successful_executions++;

            /* Get final result if available */
            if (bridge->motor) {
                motor_result_t result;
                if (motor_get_result(bridge->motor, &result)) {
                    exec->accuracy = result.accuracy;
                }
            }

            LOG_DEBUG("Completed execution of program %u (accuracy=%.2f)",
                      exec->program_id, exec->accuracy);
        }

        if (exec->is_executing) {
            total_accuracy += exec->accuracy;
            active_count++;
        }
    }

    if (active_count > 0) {
        bridge->effects.avg_execution_accuracy = total_accuracy / active_count;
    }

    /* Automatic imitation in learning mode */
    if (bridge->config.enable_automatic_imitation && bridge->learning_mode) {
        if (bridge->state.max_resonance >= bridge->config.execution_threshold) {
            mirror_motor_execute_from_resonance(bridge);
        }
    }

    /* Update execution readiness */
    bridge->effects.execution_readiness = nimcp_myelin_clamp(
        bridge->effects.motor_priming - (1.0f - bridge->current_suppression_release) * 0.5f,
        0.0f, 1.0f
    );

    /* Update active programs count */
    bridge->effects.active_programs = bridge->state.executing_programs;

    /* Update state */
    bridge->state.extracted_programs = bridge->num_programs;
    bridge->state.queued_programs = 0; /* Not implementing queue */

    /* Record update */
    bridge_base_record_update(&bridge->base);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

/* ============================================================================
 * State/Stats API Implementation
 * ============================================================================ */

int mirror_motor_bridge_get_state(
    const mirror_motor_bridge_t* bridge,
    mirror_motor_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->state;
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_get_state", 0.0f);


    return 0;
}

int mirror_motor_bridge_get_effects(
    const mirror_motor_bridge_t* bridge,
    mirror_motor_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->effects;
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_get_effects", 0.0f);


    return 0;
}

int mirror_motor_bridge_get_stats(
    const mirror_motor_bridge_t* bridge,
    mirror_motor_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_motor_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_get_stats", 0.0f);


    return 0;
}

int mirror_motor_bridge_reset_stats(
    mirror_motor_bridge_t* bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_motor_bridge_heartbeat("mirror_motor_reset_stats", 0.0f);


    memset(&bridge->stats, 0, sizeof(mirror_motor_stats_t));
    return 0;
}

/* ============================================================================
 * Bio-Async Integration Implementation
 * ============================================================================ */

BRIDGE_DEFINE_BIO_ASYNC_FUNCS_TYPE(mirror_motor_bridge, mirror_motor_bridge_t)

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mirror_motor_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mirror_motor_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mirror_motor_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_motor_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_motor_bridge_heartbeat_instance(NULL, "mirror_motor_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_motor_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_motor_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_motor_bridge_heartbeat_instance(NULL, "mirror_motor_bridge_training_end", 1.0f);
    return 0;
}

int mirror_motor_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_motor_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_motor_bridge_heartbeat_instance(NULL, "mirror_motor_bridge_training_step", progress);
    return 0;
}

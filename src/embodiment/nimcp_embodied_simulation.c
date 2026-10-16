/**
 * @file nimcp_embodied_simulation.c
 * @brief Implementation of Motor Imagery and Action Simulation
 *
 * This implementation provides internal action rehearsal and predictive
 * motor simulation capabilities for embodied cognition.
 *
 * Biological basis:
 * - Premotor cortex simulates actions before execution
 * - Forward models predict sensory consequences
 * - Mental imagery shares neural substrates with action execution
 */

#include "embodiment/nimcp_embodied_simulation.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/geometry/nimcp_lie_group.h"

#define LOG_MODULE "embodied_simulation"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(embodied_simulation)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Single simulation instance
 */
typedef struct {
    uint32_t sim_id;                 /**< Simulation ID */
    nimcp_sim_state_t state;         /**< Current state */

    /* Action steps */
    nimcp_action_step_t steps[NIMCP_SIMULATION_MAX_STEPS];
    uint32_t num_steps;
    uint32_t current_step;

    /* Simulated state */
    nimcp_effector_state_t effectors[NIMCP_SIMULATION_MAX_EFFECTORS];
    uint32_t num_effectors;
    nimcp_sim_object_t objects[NIMCP_SIMULATION_MAX_OBJECTS];
    uint32_t num_objects;

    /* Trajectory */
    nimcp_trajectory_point_t trajectory[NIMCP_SIMULATION_TRAJECTORY_SIZE];
    uint32_t trajectory_index;

    /* Collision events */
    nimcp_collision_event_t collisions[8];
    uint32_t num_collisions;

    /* Timing */
    double sim_time;                 /**< Simulated time elapsed */
    uint64_t start_time;             /**< Real start time */
    uint64_t last_step_time;         /**< Last step real time */

    /* Effort accumulator */
    nimcp_effort_estimate_t total_effort;

    bool is_active;                  /**< Slot is in use */
} nimcp_sim_instance_t;

/**
 * @brief Internal simulation context
 */
struct nimcp_sim_context {
    nimcp_sim_config_t config;       /**< Configuration */
    bool initialized;                 /**< Initialization flag */

    /* Active simulations */
    nimcp_sim_instance_t simulations[NIMCP_SIMULATION_MAX_CONCURRENT];
    uint32_t num_active_sims;
    uint32_t next_sim_id;

    /* Reference state (starting point for simulations) */
    nimcp_effector_state_t ref_effectors[NIMCP_SIMULATION_MAX_EFFECTORS];
    uint32_t num_ref_effectors;
    nimcp_sim_object_t ref_objects[NIMCP_SIMULATION_MAX_OBJECTS];
    uint32_t num_ref_objects;

    /* Statistics */
    nimcp_sim_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in nanoseconds
 */
static inline uint64_t get_timestamp_ns(void) {
    return nimcp_time_get_us() * 1000ULL;
}

/**
 * @brief Calculate distance between positions
 */
static double position_distance(
    const nimcp_sim_position_t* p1,
    const nimcp_sim_position_t* p2
) {
    double dx = p2->x - p1->x;
    double dy = p2->y - p1->y;
    double dz = p2->z - p1->z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Normalize vector
 */
static void normalize_vector(double* v) {
    double mag = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (mag > 1e-9) {
        v[0] /= mag;
        v[1] /= mag;
        v[2] /= mag;
    }
}

/**
 * @brief Find simulation by ID
 */
static nimcp_sim_instance_t* find_simulation(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
) {
    for (uint32_t i = 0; i < NIMCP_SIMULATION_MAX_CONCURRENT; i++) {
        if (ctx->simulations[i].is_active && ctx->simulations[i].sim_id == sim_id) {
            return &ctx->simulations[i];
        }
    }
    return NULL;
}

/**
 * @brief Find effector in simulation
 */
static nimcp_effector_state_t* find_effector(
    nimcp_sim_instance_t* sim,
    uint32_t effector_id
) {
    for (uint32_t i = 0; i < sim->num_effectors; i++) {
        if (sim->effectors[i].effector_id == effector_id) {
            return &sim->effectors[i];
        }
    }
    return NULL;
}

/**
 * @brief Find object in simulation
 */
static nimcp_sim_object_t* find_object(
    nimcp_sim_instance_t* sim,
    uint32_t object_id
) {
    for (uint32_t i = 0; i < sim->num_objects; i++) {
        if (sim->objects[i].object_id == object_id) {
            return &sim->objects[i];
        }
    }
    return NULL;
}

/**
 * @brief Calculate effort for single step
 */
static void calculate_step_effort(
    const nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* step,
    const nimcp_effector_state_t* effector,
    nimcp_effort_estimate_t* effort
) {
    memset(effort, 0, sizeof(*effort));

    /* Base metabolic cost depends on action type */
    double base_metabolic = 0.0;
    switch (step->primitive) {
        case NIMCP_ACTION_PRIM_REACH:
            base_metabolic = 5.0;  /* Joules */
            break;
        case NIMCP_ACTION_PRIM_GRASP:
            base_metabolic = 2.0;
            break;
        case NIMCP_ACTION_PRIM_RELEASE:
            base_metabolic = 1.0;
            break;
        case NIMCP_ACTION_PRIM_TRANSPORT:
            base_metabolic = 10.0;
            break;
        case NIMCP_ACTION_PRIM_ROTATE:
            base_metabolic = 3.0;
            break;
        case NIMCP_ACTION_PRIM_PUSH:
        case NIMCP_ACTION_PRIM_PULL:
            base_metabolic = 8.0;
            break;
        case NIMCP_ACTION_PRIM_PRESS:
            base_metabolic = 1.5;
            break;
        case NIMCP_ACTION_PRIM_LOCOMOTE:
            base_metabolic = 50.0;
            break;
        case NIMCP_ACTION_PRIM_BALANCE:
            base_metabolic = 0.5;
            break;
        default:
            base_metabolic = 5.0;
            break;
    }

    /* Scale by distance */
    double dist = 0.0;
    if (effector) {
        nimcp_sim_position_t eff_pos = effector->position;
        dist = position_distance(&eff_pos, &step->target_position);
    } else {
        dist = sqrt(step->target_position.x * step->target_position.x +
                   step->target_position.y * step->target_position.y +
                   step->target_position.z * step->target_position.z);
    }
    /* Add SO(3) geodesic cost for rotation actions */
    double orient_dist = 0.0;
    if (effector && step->primitive == NIMCP_ACTION_PRIM_ROTATE) {
        /* Convert current and target quaternions to SO(3) rotation matrices,
         * then compute geodesic distance on the rotation manifold */
        nimcp_sim_quaternion_t q = effector->orientation;
        nimcp_sim_quaternion_t qt = step->target_orientation;
        double q_norm = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
        double qt_norm = sqrt(qt.w*qt.w + qt.x*qt.x + qt.y*qt.y + qt.z*qt.z);
        if (q_norm > 1e-9 && qt_norm > 1e-9) {
            /* Quaternion to axis-angle */
            float axis_a[3] = {0,0,1}, axis_b[3] = {0,0,1};
            float angle_a = (float)(2.0 * acos(fmin(fabs(q.w / q_norm), 1.0)));
            float angle_b = (float)(2.0 * acos(fmin(fabs(qt.w / qt_norm), 1.0)));
            if (angle_a > 1e-6f) {
                float s = (float)sin(angle_a * 0.5);
                axis_a[0] = (float)(q.x / (q_norm * s));
                axis_a[1] = (float)(q.y / (q_norm * s));
                axis_a[2] = (float)(q.z / (q_norm * s));
            }
            if (angle_b > 1e-6f) {
                float s = (float)sin(angle_b * 0.5);
                axis_b[0] = (float)(qt.x / (qt_norm * s));
                axis_b[1] = (float)(qt.y / (qt_norm * s));
                axis_b[2] = (float)(qt.z / (qt_norm * s));
            }
            so3_rotation_t R_a = so3_from_axis_angle(axis_a, angle_a);
            so3_rotation_t R_b = so3_from_axis_angle(axis_b, angle_b);
            orient_dist = (double)so3_distance(&R_a, &R_b);
        }
    }
    effort->metabolic_cost = base_metabolic * (1.0 + dist + 0.5 * orient_dist);

    /* Time cost */
    effort->time_cost = step->duration;

    /* Cognitive load based on precision */
    effort->cognitive_load = step->precision;

    /* Fatigue based on force and duration */
    double force_ratio = step->max_force / (effector ? effector->max_force : 100.0);
    effort->fatigue_cost = force_ratio * step->duration * 0.1;

    /* Risk based on action type and precision */
    effort->risk = 0.1;
    if (step->primitive == NIMCP_ACTION_PRIM_TRANSPORT && effector && effector->is_grasping) {
        effort->risk += 0.1;  /* Risk of dropping */
    }
    if (step->precision > 0.8) {
        effort->risk += 0.1;  /* High precision increases risk of failure */
    }

    /* Total weighted cost */
    effort->total_cost = ctx->config.metabolic_weight * effort->metabolic_cost / 100.0 +
                        ctx->config.time_weight * effort->time_cost +
                        ctx->config.fatigue_weight * effort->fatigue_cost +
                        ctx->config.risk_weight * effort->risk;
}

/**
 * @brief Simple linear dynamics simulation step
 */
static void simulate_dynamics_step(
    nimcp_sim_context_t* ctx,
    nimcp_sim_instance_t* sim,
    double dt
) {
    /* Update effectors */
    for (uint32_t i = 0; i < sim->num_effectors; i++) {
        nimcp_effector_state_t* eff = &sim->effectors[i];

        /* Update position based on velocity */
        eff->position.x += eff->velocity[0] * dt;
        eff->position.y += eff->velocity[1] * dt;
        eff->position.z += eff->velocity[2] * dt;

        /* Apply gravity effect (simplified) */
        eff->velocity[2] += ctx->config.gravity[2] * dt * 0.1;  /* Reduced effect */

        /* Update grasped object position */
        if (eff->is_grasping && eff->grasped_object_id > 0) {
            nimcp_sim_object_t* obj = find_object(sim, eff->grasped_object_id);
            if (obj) {
                obj->position = eff->position;
                obj->velocity[0] = eff->velocity[0];
                obj->velocity[1] = eff->velocity[1];
                obj->velocity[2] = eff->velocity[2];
            }
        }

        /* Update fatigue */
        double effort_level = sqrt(eff->velocity[0] * eff->velocity[0] +
                                  eff->velocity[1] * eff->velocity[1] +
                                  eff->velocity[2] * eff->velocity[2]) / 10.0;
        eff->fatigue += effort_level * dt * 0.01;
        eff->fatigue = fmin(1.0, eff->fatigue);
    }

    /* Update free objects */
    for (uint32_t i = 0; i < sim->num_objects; i++) {
        nimcp_sim_object_t* obj = &sim->objects[i];
        if (obj->is_static || obj->is_grasped) {
            continue;
        }

        /* Apply gravity */
        obj->velocity[2] += ctx->config.gravity[2] * dt;

        /* Update position */
        obj->position.x += obj->velocity[0] * dt;
        obj->position.y += obj->velocity[1] * dt;
        obj->position.z += obj->velocity[2] * dt;

        /* Simple ground collision */
        if (obj->position.z < obj->dimensions[2] / 2.0) {
            obj->position.z = obj->dimensions[2] / 2.0;
            obj->velocity[2] = -obj->velocity[2] * 0.3;  /* Bounce with damping */
        }
    }

    sim->sim_time += dt;
}

/**
 * @brief Execute action step
 */
static bool execute_step(
    nimcp_sim_context_t* ctx,
    nimcp_sim_instance_t* sim,
    nimcp_action_step_t* step
) {
    nimcp_effector_state_t* eff = find_effector(sim, step->effector_id);
    if (!eff) {
        LOG_WARN("Effector %u not found in simulation", step->effector_id);
        return false;  /* Normal validation failure, not an error */
    }

    double step_time = 0.0;
    double dt = ctx->config.time_step;

    while (step_time < step->duration) {
        /* Calculate velocity toward target */
        double dx = step->target_position.x - eff->position.x;
        double dy = step->target_position.y - eff->position.y;
        double dz = step->target_position.z - eff->position.z;
        double dist = sqrt(dx * dx + dy * dy + dz * dz);

        if (dist > 0.001) {
            /* Move toward target */
            double speed = fmin(step->max_velocity, dist / (step->duration - step_time + 0.01));
            eff->velocity[0] = (dx / dist) * speed;
            eff->velocity[1] = (dy / dist) * speed;
            eff->velocity[2] = (dz / dist) * speed;
        } else {
            /* Reached target */
            eff->velocity[0] = 0.0;
            eff->velocity[1] = 0.0;
            eff->velocity[2] = 0.0;
            break;
        }

        /* SO(3) orientation interpolation via geodesic SLERP */
        if (step->primitive == NIMCP_ACTION_PRIM_ROTATE && step->duration > 1e-9) {
            float t_frac = (float)((step_time + dt) / step->duration);
            if (t_frac > 1.0f) t_frac = 1.0f;
            /* Convert start/target quaternions to SO(3) */
            nimcp_sim_quaternion_t q0 = eff->orientation;
            nimcp_sim_quaternion_t q1 = step->target_orientation;
            float axis0[3] = {0,0,1}, axis1[3] = {0,0,1};
            double n0 = sqrt(q0.w*q0.w+q0.x*q0.x+q0.y*q0.y+q0.z*q0.z);
            double n1 = sqrt(q1.w*q1.w+q1.x*q1.x+q1.y*q1.y+q1.z*q1.z);
            float a0 = n0>1e-9 ? (float)(2.0*acos(fmin(fabs(q0.w/n0),1.0))) : 0.0f;
            float a1 = n1>1e-9 ? (float)(2.0*acos(fmin(fabs(q1.w/n1),1.0))) : 0.0f;
            if (a0 > 1e-6f && n0 > 1e-9) {
                float s = (float)sin(a0*0.5);
                axis0[0]=(float)(q0.x/(n0*s)); axis0[1]=(float)(q0.y/(n0*s)); axis0[2]=(float)(q0.z/(n0*s));
            }
            if (a1 > 1e-6f && n1 > 1e-9) {
                float s = (float)sin(a1*0.5);
                axis1[0]=(float)(q1.x/(n1*s)); axis1[1]=(float)(q1.y/(n1*s)); axis1[2]=(float)(q1.z/(n1*s));
            }
            so3_rotation_t R0 = so3_from_axis_angle(axis0, a0);
            so3_rotation_t R1 = so3_from_axis_angle(axis1, a1);
            so3_rotation_t R_interp = so3_slerp(&R0, &R1, t_frac);
            /* Extract interpolated quaternion from R_interp via so3_log */
            so3_algebra_t log_alg = so3_log(&R_interp);
            float angle = sqrtf(log_alg.v[0]*log_alg.v[0]+log_alg.v[1]*log_alg.v[1]+log_alg.v[2]*log_alg.v[2]);
            if (angle > 1e-6f) {
                float ha = angle * 0.5f;
                eff->orientation.w = cos(ha);
                eff->orientation.x = (log_alg.v[0]/angle) * sin(ha);
                eff->orientation.y = (log_alg.v[1]/angle) * sin(ha);
                eff->orientation.z = (log_alg.v[2]/angle) * sin(ha);
            }
        }

        /* Simulate dynamics */
        simulate_dynamics_step(ctx, sim, dt);
        step_time += dt;

        /* Record trajectory */
        if (sim->trajectory_index < NIMCP_SIMULATION_TRAJECTORY_SIZE) {
            nimcp_trajectory_point_t* pt = &sim->trajectory[sim->trajectory_index];
            pt->time = sim->sim_time;
            pt->position = eff->position;
            pt->velocity[0] = eff->velocity[0];
            pt->velocity[1] = eff->velocity[1];
            pt->velocity[2] = eff->velocity[2];
            pt->effort = sqrt(eff->velocity[0] * eff->velocity[0] +
                            eff->velocity[1] * eff->velocity[1] +
                            eff->velocity[2] * eff->velocity[2]) * 0.1;
            sim->trajectory_index++;
        }

        /* Check for collision */
        if (ctx->config.enable_collision_detection) {
            for (uint32_t j = 0; j < sim->num_objects; j++) {
                if (!sim->objects[j].is_grasped) {
                    double obj_dist = position_distance(&eff->position, &sim->objects[j].position);
                    double threshold = ctx->config.collision_margin +
                                      fmax(sim->objects[j].dimensions[0],
                                          fmax(sim->objects[j].dimensions[1],
                                               sim->objects[j].dimensions[2])) / 2.0;

                    if (obj_dist < threshold && sim->num_collisions < 8) {
                        nimcp_collision_event_t* col = &sim->collisions[sim->num_collisions];
                        col->effector_id = eff->effector_id;
                        col->object_id = sim->objects[j].object_id;
                        col->contact_point = eff->position;
                        col->impact_force = sqrt(eff->velocity[0] * eff->velocity[0] +
                                               eff->velocity[1] * eff->velocity[1] +
                                               eff->velocity[2] * eff->velocity[2]);
                        col->time = sim->sim_time;
                        col->is_self_collision = false;
                        sim->num_collisions++;
                        ctx->stats.collision_count++;
                    }
                }
            }
        }
    }

    /* Handle grasp/release */
    if (step->primitive == NIMCP_ACTION_PRIM_GRASP && step->target_object_id > 0) {
        nimcp_sim_object_t* obj = find_object(sim, step->target_object_id);
        if (obj && position_distance(&eff->position, &obj->position) < 0.1) {
            eff->is_grasping = true;
            eff->grasped_object_id = step->target_object_id;
            obj->is_grasped = true;
            obj->grasped_by = eff->effector_id;
        }
    } else if (step->primitive == NIMCP_ACTION_PRIM_RELEASE) {
        if (eff->is_grasping) {
            nimcp_sim_object_t* obj = find_object(sim, eff->grasped_object_id);
            if (obj) {
                obj->is_grasped = false;
                obj->grasped_by = 0;
            }
            eff->is_grasping = false;
            eff->grasped_object_id = 0;
        }
    }

    step->is_completed = true;
    step->is_successful = true;
    step->actual_duration = step_time;

    return true;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void nimcp_sim_default_config(nimcp_sim_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    config->fidelity = NIMCP_FIDELITY_MEDIUM;
    config->forward_model = NIMCP_FORWARD_MODEL_LINEAR;
    config->time_step = 0.01;  /* 10ms */
    config->max_sim_time = 30.0;  /* 30 seconds */

    config->gravity[0] = 0.0;
    config->gravity[1] = 0.0;
    config->gravity[2] = -9.81;
    config->air_resistance = 0.01;

    config->metabolic_weight = 0.3;
    config->time_weight = 0.3;
    config->fatigue_weight = 0.2;
    config->risk_weight = 0.2;

    config->enable_collision_detection = true;
    config->collision_margin = 0.05;

    config->max_steps = NIMCP_SIMULATION_MAX_STEPS;
    config->max_concurrent = NIMCP_SIMULATION_MAX_CONCURRENT;

    config->position_noise = 0.01;
    config->velocity_noise = 0.05;
}

nimcp_sim_context_t* nimcp_sim_create(const nimcp_sim_config_t* config) {
    /* Use defaults if config is NULL */
    nimcp_sim_config_t default_config;
    if (!config) {
        nimcp_sim_default_config(&default_config);
        config = &default_config;
    }

    nimcp_sim_context_t* ctx = nimcp_malloc(sizeof(nimcp_sim_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate simulation context");
        return NULL;
    }

    nimcp_sim_error_t err = nimcp_sim_init(ctx, config);
    if (err != NIMCP_SIM_OK) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_sim_create: validation failed");
        return NULL;
    }

    return ctx;
}

nimcp_sim_error_t nimcp_sim_init(
    nimcp_sim_context_t* ctx,
    const nimcp_sim_config_t* config
) {
    if (!ctx || !config) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->config = *config;
    ctx->initialized = true;
    ctx->next_sim_id = 1;
    ctx->stats.creation_time = get_timestamp_ns();

    LOG_INFO("Initialized embodied simulation context");

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_reset(nimcp_sim_context_t* ctx) {
    if (!ctx) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    /* Clear all simulations */
    memset(ctx->simulations, 0, sizeof(ctx->simulations));
    ctx->num_active_sims = 0;
    ctx->next_sim_id = 1;

    /* Clear reference state */
    ctx->num_ref_effectors = 0;
    ctx->num_ref_objects = 0;

    /* Reset statistics */
    uint64_t creation_time = ctx->stats.creation_time;
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.creation_time = creation_time;

    LOG_INFO("Reset simulation context");

    return NIMCP_SIM_OK;
}

void nimcp_sim_destroy(nimcp_sim_context_t* ctx) {
    if (!ctx) {
        return;
    }

    LOG_INFO("Destroying simulation context (total sims: %llu, success rate: %.1f%%)",
             (unsigned long long)ctx->stats.total_simulations,
             ctx->stats.total_simulations > 0 ?
                 100.0 * ctx->stats.successful_simulations / ctx->stats.total_simulations : 0.0);

    nimcp_free(ctx);
}

/* ============================================================================
 * Effector and Object Setup
 * ============================================================================ */

nimcp_sim_error_t nimcp_sim_set_effector(
    nimcp_sim_context_t* ctx,
    const nimcp_effector_state_t* effector
) {
    if (!ctx || !effector) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    /* Find or add effector */
    for (uint32_t i = 0; i < ctx->num_ref_effectors; i++) {
        if (ctx->ref_effectors[i].effector_id == effector->effector_id) {
            ctx->ref_effectors[i] = *effector;
            return NIMCP_SIM_OK;
        }
    }

    if (ctx->num_ref_effectors >= NIMCP_SIMULATION_MAX_EFFECTORS) {
        return NIMCP_SIM_ERROR_SIM_LIMIT;
    }

    ctx->ref_effectors[ctx->num_ref_effectors++] = *effector;

    LOG_DEBUG("Set effector %u (%s)", effector->effector_id,
              nimcp_sim_effector_name(effector->type));

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_get_effector(
    const nimcp_sim_context_t* ctx,
    uint32_t effector_id,
    nimcp_effector_state_t* effector
) {
    if (!ctx || !effector) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < ctx->num_ref_effectors; i++) {
        if (ctx->ref_effectors[i].effector_id == effector_id) {
            *effector = ctx->ref_effectors[i];
            return NIMCP_SIM_OK;
        }
    }

    return NIMCP_SIM_ERROR_INVALID_SIM;
}

nimcp_sim_error_t nimcp_sim_add_object(
    nimcp_sim_context_t* ctx,
    const nimcp_sim_object_t* object
) {
    if (!ctx || !object) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    /* Check for existing */
    for (uint32_t i = 0; i < ctx->num_ref_objects; i++) {
        if (ctx->ref_objects[i].object_id == object->object_id) {
            ctx->ref_objects[i] = *object;
            return NIMCP_SIM_OK;
        }
    }

    if (ctx->num_ref_objects >= NIMCP_SIMULATION_MAX_OBJECTS) {
        return NIMCP_SIM_ERROR_SIM_LIMIT;
    }

    ctx->ref_objects[ctx->num_ref_objects++] = *object;

    LOG_DEBUG("Added object %u to reference state", object->object_id);

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_update_object(
    nimcp_sim_context_t* ctx,
    const nimcp_sim_object_t* object
) {
    return nimcp_sim_add_object(ctx, object);
}

nimcp_sim_error_t nimcp_sim_remove_object(
    nimcp_sim_context_t* ctx,
    uint32_t object_id
) {
    if (!ctx) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    for (uint32_t i = 0; i < ctx->num_ref_objects; i++) {
        if (ctx->ref_objects[i].object_id == object_id) {
            /* Shift remaining */
            for (uint32_t j = i; j < ctx->num_ref_objects - 1; j++) {
                ctx->ref_objects[j] = ctx->ref_objects[j + 1];
            }
            ctx->num_ref_objects--;
            return NIMCP_SIM_OK;
        }
    }

    return NIMCP_SIM_ERROR_INVALID_SIM;
}

/* ============================================================================
 * Action Simulation
 * ============================================================================ */

nimcp_sim_error_t nimcp_sim_start(
    nimcp_sim_context_t* ctx,
    uint32_t* sim_id
) {
    if (!ctx || !sim_id) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    /* Find free slot */
    int free_slot = -1;
    for (uint32_t i = 0; i < NIMCP_SIMULATION_MAX_CONCURRENT; i++) {
        if (!ctx->simulations[i].is_active) {
            free_slot = (int)i;
            break;
        }
    }

    if (free_slot < 0) {
        LOG_WARN("Simulation limit reached (%u)", NIMCP_SIMULATION_MAX_CONCURRENT);
        return NIMCP_SIM_ERROR_SIM_LIMIT;
    }

    /* Initialize simulation */
    nimcp_sim_instance_t* sim = &ctx->simulations[free_slot];
    memset(sim, 0, sizeof(*sim));

    sim->sim_id = ctx->next_sim_id++;
    sim->state = NIMCP_SIM_STATE_IDLE;
    sim->is_active = true;
    sim->start_time = get_timestamp_ns();

    /* Copy reference state */
    memcpy(sim->effectors, ctx->ref_effectors, sizeof(ctx->ref_effectors));
    sim->num_effectors = ctx->num_ref_effectors;
    memcpy(sim->objects, ctx->ref_objects, sizeof(ctx->ref_objects));
    sim->num_objects = ctx->num_ref_objects;

    ctx->num_active_sims++;
    *sim_id = sim->sim_id;

    LOG_DEBUG("Started simulation %u", sim->sim_id);

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_add_step(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id,
    const nimcp_action_step_t* step
) {
    if (!ctx || !step) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    nimcp_sim_instance_t* sim = find_simulation(ctx, sim_id);
    if (!sim) {
        return NIMCP_SIM_ERROR_INVALID_SIM;
    }

    if (sim->num_steps >= NIMCP_SIMULATION_MAX_STEPS) {
        return NIMCP_SIM_ERROR_STEP_LIMIT;
    }

    sim->steps[sim->num_steps] = *step;
    sim->steps[sim->num_steps].step_id = sim->num_steps;
    sim->num_steps++;

    LOG_DEBUG("Added step %u (%s) to simulation %u",
              sim->num_steps - 1, nimcp_sim_primitive_name(step->primitive), sim_id);

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_run(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id,
    nimcp_sim_result_t* result
) {
    if (!ctx || !result) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    nimcp_sim_instance_t* sim = find_simulation(ctx, sim_id);
    if (!sim) {
        return NIMCP_SIM_ERROR_INVALID_SIM;
    }

    memset(result, 0, sizeof(*result));
    result->sim_id = sim_id;

    sim->state = NIMCP_SIM_STATE_RUNNING;
    uint64_t run_start = get_timestamp_ns();

    /* Execute all steps */
    bool success = true;
    for (uint32_t i = 0; i < sim->num_steps && success; i++) {
        sim->current_step = i;

        /* Calculate effort for this step */
        nimcp_effort_estimate_t step_effort;
        calculate_step_effort(ctx, &sim->steps[i],
                            find_effector(sim, sim->steps[i].effector_id),
                            &step_effort);

        /* Accumulate effort */
        sim->total_effort.metabolic_cost += step_effort.metabolic_cost;
        sim->total_effort.time_cost += step_effort.time_cost;
        sim->total_effort.cognitive_load = fmax(sim->total_effort.cognitive_load,
                                               step_effort.cognitive_load);
        sim->total_effort.fatigue_cost += step_effort.fatigue_cost;
        sim->total_effort.risk = 1.0 - (1.0 - sim->total_effort.risk) * (1.0 - step_effort.risk);

        /* Execute step */
        if (!execute_step(ctx, sim, &sim->steps[i])) {
            success = false;
            sim->state = NIMCP_SIM_STATE_FAILED;
        }

        /* Check timeout */
        if (sim->sim_time > ctx->config.max_sim_time) {
            LOG_WARN("Simulation %u timed out", sim_id);
            success = false;
            sim->state = NIMCP_SIM_STATE_FAILED;
        }

        ctx->stats.total_steps++;
    }

    /* Calculate total effort */
    sim->total_effort.total_cost = ctx->config.metabolic_weight * sim->total_effort.metabolic_cost / 100.0 +
                                   ctx->config.time_weight * sim->total_effort.time_cost +
                                   ctx->config.fatigue_weight * sim->total_effort.fatigue_cost +
                                   ctx->config.risk_weight * sim->total_effort.risk;

    /* Fill result */
    if (success) {
        sim->state = NIMCP_SIM_STATE_COMPLETED;
        result->goal_achieved = true;
        result->success_probability = 1.0 - sim->total_effort.risk;
        ctx->stats.successful_simulations++;
    } else {
        result->goal_achieved = false;
        result->success_probability = 0.0;
        ctx->stats.failed_simulations++;
    }

    result->final_state = sim->state;
    result->effort = sim->total_effort;

    /* Copy trajectory */
    result->trajectory_length = sim->trajectory_index;
    if (result->trajectory_length > 0) {
        memcpy(result->trajectory, sim->trajectory,
               result->trajectory_length * sizeof(nimcp_trajectory_point_t));
    }

    /* Copy collisions */
    result->num_collisions = sim->num_collisions;
    memcpy(result->collisions, sim->collisions,
           result->num_collisions * sizeof(nimcp_collision_event_t));

    result->sim_duration = sim->sim_time;
    result->real_duration = (get_timestamp_ns() - run_start) / 1e9;
    result->steps_completed = sim->current_step + 1;

    ctx->stats.total_simulations++;
    ctx->stats.avg_sim_duration = (ctx->stats.avg_sim_duration * (ctx->stats.total_simulations - 1) +
                                   result->sim_duration) / ctx->stats.total_simulations;
    ctx->stats.avg_effort_estimate = (ctx->stats.avg_effort_estimate * (ctx->stats.total_simulations - 1) +
                                      sim->total_effort.total_cost) / ctx->stats.total_simulations;

    LOG_DEBUG("Simulation %u completed: success=%s, steps=%u, time=%.2fs",
              sim_id, result->goal_achieved ? "true" : "false",
              result->steps_completed, result->sim_duration);

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_step(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
) {
    if (!ctx) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    nimcp_sim_instance_t* sim = find_simulation(ctx, sim_id);
    if (!sim) {
        return NIMCP_SIM_ERROR_INVALID_SIM;
    }

    if (sim->current_step >= sim->num_steps) {
        sim->state = NIMCP_SIM_STATE_COMPLETED;
        return NIMCP_SIM_OK;
    }

    sim->state = NIMCP_SIM_STATE_RUNNING;

    if (!execute_step(ctx, sim, &sim->steps[sim->current_step])) {
        sim->state = NIMCP_SIM_STATE_FAILED;
        return NIMCP_SIM_ERROR_INFEASIBLE;
    }

    sim->current_step++;
    ctx->stats.total_steps++;

    if (sim->current_step >= sim->num_steps) {
        sim->state = NIMCP_SIM_STATE_COMPLETED;
    }

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_pause(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
) {
    if (!ctx) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    nimcp_sim_instance_t* sim = find_simulation(ctx, sim_id);
    if (!sim) {
        return NIMCP_SIM_ERROR_INVALID_SIM;
    }

    if (sim->state == NIMCP_SIM_STATE_RUNNING) {
        sim->state = NIMCP_SIM_STATE_PAUSED;
    }

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_resume(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
) {
    if (!ctx) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    nimcp_sim_instance_t* sim = find_simulation(ctx, sim_id);
    if (!sim) {
        return NIMCP_SIM_ERROR_INVALID_SIM;
    }

    if (sim->state == NIMCP_SIM_STATE_PAUSED) {
        sim->state = NIMCP_SIM_STATE_RUNNING;
    }

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_abort(
    nimcp_sim_context_t* ctx,
    uint32_t sim_id
) {
    if (!ctx) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    nimcp_sim_instance_t* sim = find_simulation(ctx, sim_id);
    if (!sim) {
        return NIMCP_SIM_ERROR_INVALID_SIM;
    }

    sim->state = NIMCP_SIM_STATE_ABORTED;

    LOG_DEBUG("Aborted simulation %u", sim_id);

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_get_state(
    const nimcp_sim_context_t* ctx,
    uint32_t sim_id,
    nimcp_sim_state_t* state
) {
    if (!ctx || !state) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < NIMCP_SIMULATION_MAX_CONCURRENT; i++) {
        if (ctx->simulations[i].is_active && ctx->simulations[i].sim_id == sim_id) {
            *state = ctx->simulations[i].state;
            return NIMCP_SIM_OK;
        }
    }

    return NIMCP_SIM_ERROR_INVALID_SIM;
}

/* ============================================================================
 * Forward Model
 * ============================================================================ */

nimcp_sim_error_t nimcp_sim_forward_predict(
    nimcp_sim_context_t* ctx,
    uint32_t effector_id,
    const double* motor_command,
    double duration,
    nimcp_forward_prediction_t* prediction
) {
    if (!ctx || !motor_command || !prediction) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    memset(prediction, 0, sizeof(*prediction));

    uint64_t start = get_timestamp_ns();

    /* Find effector */
    nimcp_effector_state_t* eff = NULL;
    for (uint32_t i = 0; i < ctx->num_ref_effectors; i++) {
        if (ctx->ref_effectors[i].effector_id == effector_id) {
            eff = &ctx->ref_effectors[i];
            break;
        }
    }

    if (!eff) {
        return NIMCP_SIM_ERROR_INVALID_SIM;
    }

    /* Copy effector state for prediction */
    prediction->predicted_effector = *eff;

    /* Simulate forward */
    double dt = ctx->config.time_step;
    double t = 0.0;

    while (t < duration) {
        /* Apply motor command (velocity) */
        prediction->predicted_effector.velocity[0] = motor_command[0];
        prediction->predicted_effector.velocity[1] = motor_command[1];
        prediction->predicted_effector.velocity[2] = motor_command[2];

        /* Update position */
        prediction->predicted_effector.position.x += prediction->predicted_effector.velocity[0] * dt;
        prediction->predicted_effector.position.y += prediction->predicted_effector.velocity[1] * dt;
        prediction->predicted_effector.position.z += prediction->predicted_effector.velocity[2] * dt;

        /* Apply gravity */
        prediction->predicted_effector.velocity[2] += ctx->config.gravity[2] * dt * 0.1;

        t += dt;
    }

    /* Generate sensory predictions */
    prediction->predicted_visual[0] = prediction->predicted_effector.position.x;
    prediction->predicted_visual[1] = prediction->predicted_effector.position.y;
    prediction->predicted_visual[2] = prediction->predicted_effector.position.z;

    prediction->predicted_proprio[0] = prediction->predicted_effector.position.x;
    prediction->predicted_proprio[1] = prediction->predicted_effector.position.y;
    prediction->predicted_proprio[2] = prediction->predicted_effector.position.z;

    /* Add noise for uncertainty */
    prediction->position_uncertainty = ctx->config.position_noise * sqrt(duration);
    prediction->outcome_confidence = 1.0 / (1.0 + prediction->position_uncertainty);

    prediction->prediction_time = (get_timestamp_ns() - start) / 1e9;

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_predict_sequence(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* steps,
    uint32_t num_steps,
    nimcp_forward_prediction_t* prediction
) {
    if (!ctx || !steps || !prediction || num_steps == 0) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    /* Run full simulation and extract prediction */
    uint32_t sim_id;
    nimcp_sim_error_t err = nimcp_sim_start(ctx, &sim_id);
    if (err != NIMCP_SIM_OK) {
        return err;
    }

    for (uint32_t i = 0; i < num_steps; i++) {
        err = nimcp_sim_add_step(ctx, sim_id, &steps[i]);
        if (err != NIMCP_SIM_OK) {
            return err;
        }
    }

    nimcp_sim_result_t result;
    err = nimcp_sim_run(ctx, sim_id, &result);

    if (err == NIMCP_SIM_OK) {
        nimcp_sim_instance_t* sim = find_simulation(ctx, sim_id);
        if (sim && sim->num_effectors > 0) {
            prediction->predicted_effector = sim->effectors[0];
        }
        prediction->num_predicted_objects = sim ? sim->num_objects : 0;
        prediction->outcome_confidence = result.success_probability;
    }

    return err;
}

/* ============================================================================
 * Effort Estimation
 * ============================================================================ */

nimcp_sim_error_t nimcp_sim_estimate_effort(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* step,
    nimcp_effort_estimate_t* estimate
) {
    if (!ctx || !step || !estimate) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_SIM_ERROR_NOT_INITIALIZED;
    }

    nimcp_effector_state_t* eff = NULL;
    for (uint32_t i = 0; i < ctx->num_ref_effectors; i++) {
        if (ctx->ref_effectors[i].effector_id == step->effector_id) {
            eff = &ctx->ref_effectors[i];
            break;
        }
    }

    calculate_step_effort(ctx, step, eff, estimate);

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_estimate_sequence_effort(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* steps,
    uint32_t num_steps,
    nimcp_effort_estimate_t* estimate
) {
    if (!ctx || !steps || !estimate || num_steps == 0) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    memset(estimate, 0, sizeof(*estimate));

    for (uint32_t i = 0; i < num_steps; i++) {
        nimcp_effort_estimate_t step_effort;
        nimcp_sim_estimate_effort(ctx, &steps[i], &step_effort);

        estimate->metabolic_cost += step_effort.metabolic_cost;
        estimate->time_cost += step_effort.time_cost;
        estimate->cognitive_load = fmax(estimate->cognitive_load, step_effort.cognitive_load);
        estimate->fatigue_cost += step_effort.fatigue_cost;
        estimate->risk = 1.0 - (1.0 - estimate->risk) * (1.0 - step_effort.risk);
    }

    estimate->total_cost = ctx->config.metabolic_weight * estimate->metabolic_cost / 100.0 +
                          ctx->config.time_weight * estimate->time_cost +
                          ctx->config.fatigue_weight * estimate->fatigue_cost +
                          ctx->config.risk_weight * estimate->risk;

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_compare_alternatives(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* alternatives,
    uint32_t num_alternatives,
    uint32_t* best_index,
    nimcp_effort_estimate_t* estimates
) {
    if (!ctx || !alternatives || !best_index || !estimates || num_alternatives == 0) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    double best_cost = DBL_MAX;
    *best_index = 0;

    for (uint32_t i = 0; i < num_alternatives; i++) {
        nimcp_sim_estimate_effort(ctx, &alternatives[i], &estimates[i]);

        if (estimates[i].total_cost < best_cost) {
            best_cost = estimates[i].total_cost;
            *best_index = i;
        }
    }

    return NIMCP_SIM_OK;
}

/* ============================================================================
 * Feasibility and Collision
 * ============================================================================ */

nimcp_sim_error_t nimcp_sim_check_feasibility(
    nimcp_sim_context_t* ctx,
    const nimcp_action_step_t* step,
    bool* is_feasible,
    char* reason
) {
    if (!ctx || !step || !is_feasible) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    *is_feasible = true;

    /* Find effector */
    nimcp_effector_state_t* eff = NULL;
    for (uint32_t i = 0; i < ctx->num_ref_effectors; i++) {
        if (ctx->ref_effectors[i].effector_id == step->effector_id) {
            eff = &ctx->ref_effectors[i];
            break;
        }
    }

    if (!eff) {
        *is_feasible = false;
        if (reason) {
            snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Effector not found");
        }
        return NIMCP_SIM_OK;
    }

    /* Check reach */
    double dist = position_distance(&eff->position, &step->target_position);
    double max_reach = 1.0;  /* 1 meter reach assumption */

    if (dist > max_reach) {
        *is_feasible = false;
        if (reason) {
            snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Target out of reach (%.2fm > %.2fm)", dist, max_reach);
        }
        return NIMCP_SIM_OK;
    }

    /* Check force requirements */
    if (step->max_force > eff->max_force * (1.0 - eff->fatigue)) {
        *is_feasible = false;
        if (reason) {
            snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Insufficient force capacity");
        }
        return NIMCP_SIM_OK;
    }

    /* Check grasp requirements */
    if (step->primitive == NIMCP_ACTION_PRIM_GRASP && eff->is_grasping) {
        *is_feasible = false;
        if (reason) {
            snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Already grasping an object");
        }
        return NIMCP_SIM_OK;
    }

    if (step->primitive == NIMCP_ACTION_PRIM_RELEASE && !eff->is_grasping) {
        *is_feasible = false;
        if (reason) {
            snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Not holding any object");
        }
        return NIMCP_SIM_OK;
    }

    if (reason) {
        snprintf(reason, NIMCP_SIMULATION_REASON_MAX, "Feasible");
    }

    return NIMCP_SIM_OK;
}

nimcp_sim_error_t nimcp_sim_check_collision(
    nimcp_sim_context_t* ctx,
    uint32_t effector_id,
    const nimcp_sim_position_t* start,
    const nimcp_sim_position_t* end,
    bool* has_collision,
    nimcp_collision_event_t* collision
) {
    if (!ctx || !start || !end || !has_collision) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    *has_collision = false;

    if (!ctx->config.enable_collision_detection) {
        return NIMCP_SIM_OK;
    }

    /* Check path against all objects */
    double path_length = position_distance(start, end);
    if (path_length < 1e-9) {
        return NIMCP_SIM_OK;
    }

    double dx = (end->x - start->x) / path_length;
    double dy = (end->y - start->y) / path_length;
    double dz = (end->z - start->z) / path_length;

    /* Sample along path */
    double step_size = ctx->config.collision_margin;
    for (double t = 0.0; t <= path_length; t += step_size) {
        nimcp_sim_position_t sample = {
            start->x + dx * t,
            start->y + dy * t,
            start->z + dz * t
        };

        for (uint32_t i = 0; i < ctx->num_ref_objects; i++) {
            double dist = position_distance(&sample, &ctx->ref_objects[i].position);
            double threshold = ctx->config.collision_margin +
                              fmax(ctx->ref_objects[i].dimensions[0],
                                  fmax(ctx->ref_objects[i].dimensions[1],
                                       ctx->ref_objects[i].dimensions[2])) / 2.0;

            if (dist < threshold) {
                *has_collision = true;
                if (collision) {
                    collision->effector_id = effector_id;
                    collision->object_id = ctx->ref_objects[i].object_id;
                    collision->contact_point = sample;
                    collision->impact_force = 0.0;  /* Unknown until simulation */
                    collision->time = t / 1.0;  /* Approximate time */
                    collision->is_self_collision = false;
                }
                return NIMCP_SIM_OK;
            }
        }
    }

    return NIMCP_SIM_OK;
}

/* ============================================================================
 * Statistics and Utility
 * ============================================================================ */

nimcp_sim_error_t nimcp_sim_get_stats(
    const nimcp_sim_context_t* ctx,
    nimcp_sim_stats_t* stats
) {
    if (!ctx || !stats) {
        return NIMCP_SIM_ERROR_NULL_PARAM;
    }

    *stats = ctx->stats;
    return NIMCP_SIM_OK;
}

const char* nimcp_sim_state_name(nimcp_sim_state_t state) {
    static const char* names[] = {
        "Idle", "Running", "Paused", "Completed", "Failed", "Aborted"
    };

    if (state >= 0 && state < NIMCP_SIM_STATE_COUNT) {
        return names[state];
    }
    return "Unknown";
}

const char* nimcp_sim_primitive_name(nimcp_action_primitive_t primitive) {
    static const char* names[] = {
        "None", "Reach", "Grasp", "Release", "Transport",
        "Rotate", "Push", "Pull", "Press", "Locomote", "Balance"
    };

    if (primitive >= 0 && primitive < NIMCP_ACTION_PRIM_COUNT) {
        return names[primitive];
    }
    return "Unknown";
}

const char* nimcp_sim_effector_name(nimcp_effector_type_t type) {
    static const char* names[] = {
        "Unknown", "Left Hand", "Right Hand", "Left Foot", "Right Foot",
        "Head", "Torso", "Bimanual", "Whole Body"
    };

    if (type >= 0 && type < NIMCP_EFFECTOR_COUNT) {
        return names[type];
    }
    return "Unknown";
}

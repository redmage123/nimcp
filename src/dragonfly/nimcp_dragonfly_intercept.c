/**
 * @file nimcp_dragonfly_intercept.c
 * @brief Interception Planning Implementation
 *
 * WHAT: Implements interception trajectory planning
 * WHY:  Dragonflies use proportional navigation for 95% intercept success
 * HOW:  PN guidance + lead angle + energy optimization
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#include "dragonfly/nimcp_dragonfly_intercept.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_intercept)

//=============================================================================
// Constants
//=============================================================================


//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_dot(const float a[3], const float b[3]) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline void vec3_normalize(float v[3]) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

static inline void vec3_sub(float out[3], const float a[3], const float b[3]) {
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static inline void vec3_cross(float out[3], const float a[3], const float b[3]) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_interceptor_s {
    intercept_config_t config;
    intercept_stats_t stats;
    intercept_solution_t last_solution;
    nimcp_mutex_t* mutex;
    uint64_t creation_time_us;
};

//=============================================================================
// Configuration Functions
//=============================================================================

intercept_config_t intercept_default_config(void) {
    intercept_config_t config = {
        .pn_gain = 4.0f,
        .lead_time_factor = 1.0f,
        .min_intercept_time_s = 0.1f,
        .max_intercept_time_s = 10.0f,
        .min_closing_speed = 0.5f,
        .max_miss_distance = 1.0f,
        .safety_margin = 1.2f,
        .optimize_energy = false,
        .energy_weight = 0.3f,
        .preferred_strategy = INTERCEPT_PN,
        .auto_select_strategy = true
    };
    return config;
}

bool intercept_validate_config(const intercept_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intercept_validate_config: config is NULL");
        return false;
    }
    if (config->pn_gain < 1.0f || config->pn_gain > 10.0f) {
        return false;
    }
    if (config->min_intercept_time_s < 0.0f) {
        return false;
    }
    if (config->max_intercept_time_s <= config->min_intercept_time_s) {
        return false;
    }
    if (config->safety_margin < 1.0f) {
        return false;
    }
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_interceptor_t* dragonfly_interceptor_create(const intercept_config_t* config) {
    intercept_config_t cfg = config ? *config : intercept_default_config();

    if (!intercept_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_interceptor_create: invalid configuration");
        return NULL;
    }

    dragonfly_interceptor_t* inter = nimcp_calloc(1, sizeof(dragonfly_interceptor_t));
    if (!inter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_interceptor_create: failed to allocate interceptor");
        return NULL;
    }

    inter->config = cfg;
    inter->creation_time_us = get_time_us();

    inter->mutex = nimcp_mutex_create(NULL);
    if (!inter->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_interceptor_create: failed to create mutex");
        nimcp_free(inter);
        return NULL;
    }

    return inter;
}

void dragonfly_interceptor_destroy(dragonfly_interceptor_t* interceptor) {
    if (!interceptor) return;
    if (interceptor->mutex) {
        nimcp_mutex_destroy(interceptor->mutex);
    }
    nimcp_free(interceptor);
}

int dragonfly_interceptor_reset(dragonfly_interceptor_t* interceptor) {
    if (!interceptor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_interceptor_reset: interceptor is NULL");
        return -1;
    }

    nimcp_mutex_lock(interceptor->mutex);
    memset(&interceptor->last_solution, 0, sizeof(interceptor->last_solution));
    nimcp_mutex_unlock(interceptor->mutex);

    return 0;
}

//=============================================================================
// Core Computation Functions
//=============================================================================

static void compute_pursuit(
    const interceptor_state_t* self,
    const target_state_t* target,
    intercept_solution_t* solution
) {
    /* Pure pursuit: aim directly at target */
    float rel_pos[3];
    vec3_sub(rel_pos, target->position, self->position);
    float range = vec3_length(rel_pos);

    if (range < 1e-6f) {
        solution->feasibility = INTERCEPT_FEASIBLE;
        solution->intercept_time_s = 0.0f;
        return;
    }

    /* Heading to target */
    solution->heading_rad = atan2f(rel_pos[1], rel_pos[0]);
    solution->lead_angle_rad = 0.0f;

    /* Required velocity direction */
    float dir[3] = {rel_pos[0]/range, rel_pos[1]/range, rel_pos[2]/range};

    /* Compute closing speed */
    float rel_vel[3];
    vec3_sub(rel_vel, self->velocity, target->velocity);
    solution->closing_speed = -vec3_dot(rel_vel, dir);

    /* Estimate intercept time */
    if (solution->closing_speed > 0.1f) {
        solution->intercept_time_s = range / solution->closing_speed;
        solution->feasibility = INTERCEPT_FEASIBLE;
    } else {
        solution->intercept_time_s = range / self->max_speed;
        solution->feasibility = INTERCEPT_ESCAPING;
    }

    /* Intercept point (current target position for pursuit) */
    memcpy(solution->intercept_point, target->position, sizeof(solution->intercept_point));

    /* Required acceleration */
    float desired_vel[3] = {
        dir[0] * self->max_speed,
        dir[1] * self->max_speed,
        dir[2] * self->max_speed
    };
    solution->required_accel[0] = (desired_vel[0] - self->velocity[0]) * 2.0f;
    solution->required_accel[1] = (desired_vel[1] - self->velocity[1]) * 2.0f;
    solution->required_accel[2] = (desired_vel[2] - self->velocity[2]) * 2.0f;

    solution->required_speed = self->max_speed;
    solution->strategy = INTERCEPT_PURSUIT;
}

static void compute_lead_pursuit(
    const interceptor_state_t* self,
    const target_state_t* target,
    float lead_time_factor,
    intercept_solution_t* solution
) {
    /* Lead pursuit: aim ahead of target */
    float rel_pos[3];
    vec3_sub(rel_pos, target->position, self->position);
    float range = vec3_length(rel_pos);

    if (range < 1e-6f) {
        solution->feasibility = INTERCEPT_FEASIBLE;
        solution->intercept_time_s = 0.0f;
        return;
    }

    /* Estimate time to intercept */
    float target_speed = vec3_length(target->velocity);
    float self_speed = self->max_speed;
    float tti_estimate = range / (self_speed + 0.1f);

    /* Predicted target position */
    float lead_time = tti_estimate * lead_time_factor;
    float pred_target[3] = {
        target->position[0] + target->velocity[0] * lead_time,
        target->position[1] + target->velocity[1] * lead_time,
        target->position[2] + target->velocity[2] * lead_time
    };

    /* Direction to predicted position */
    float pred_rel[3];
    vec3_sub(pred_rel, pred_target, self->position);
    float pred_range = vec3_length(pred_rel);

    if (pred_range < 1e-6f) {
        memcpy(solution->intercept_point, pred_target, sizeof(solution->intercept_point));
        solution->intercept_time_s = lead_time;
        solution->feasibility = INTERCEPT_FEASIBLE;
        return;
    }

    /* Lead angle */
    float to_target[3] = {rel_pos[0]/range, rel_pos[1]/range, rel_pos[2]/range};
    float to_pred[3] = {pred_rel[0]/pred_range, pred_rel[1]/pred_range, pred_rel[2]/pred_range};
    float cos_lead = vec3_dot(to_target, to_pred);
    solution->lead_angle_rad = acosf(nimcp_clampf(cos_lead, -1.0f, 1.0f));

    /* Heading */
    solution->heading_rad = atan2f(pred_rel[1], pred_rel[0]);

    /* Intercept point and time */
    memcpy(solution->intercept_point, pred_target, sizeof(solution->intercept_point));
    solution->intercept_time_s = pred_range / self_speed;

    /* Closing speed */
    float rel_vel[3];
    vec3_sub(rel_vel, self->velocity, target->velocity);
    solution->closing_speed = -vec3_dot(rel_vel, to_pred);

    /* Feasibility */
    if (self_speed > target_speed * 0.8f) {
        solution->feasibility = INTERCEPT_FEASIBLE;
    } else {
        solution->feasibility = INTERCEPT_TOO_FAST;
    }

    /* Required acceleration */
    float desired_vel[3] = {
        to_pred[0] * self_speed,
        to_pred[1] * self_speed,
        to_pred[2] * self_speed
    };
    solution->required_accel[0] = (desired_vel[0] - self->velocity[0]) * 2.0f;
    solution->required_accel[1] = (desired_vel[1] - self->velocity[1]) * 2.0f;
    solution->required_accel[2] = (desired_vel[2] - self->velocity[2]) * 2.0f;

    solution->required_speed = self_speed;
    solution->strategy = INTERCEPT_LEAD;
}

static void compute_proportional_nav(
    const interceptor_state_t* self,
    const target_state_t* target,
    float pn_gain,
    intercept_solution_t* solution
) {
    /* Proportional Navigation: accel = N * Vc * LOS_rate */
    float rel_pos[3], rel_vel[3];
    vec3_sub(rel_pos, target->position, self->position);
    vec3_sub(rel_vel, target->velocity, self->velocity);

    float range = vec3_length(rel_pos);
    if (range < 1e-6f) {
        solution->feasibility = INTERCEPT_FEASIBLE;
        solution->intercept_time_s = 0.0f;
        return;
    }

    /* Line of sight unit vector */
    float los[3] = {rel_pos[0]/range, rel_pos[1]/range, rel_pos[2]/range};

    /* Closing velocity (negative = closing) */
    float Vc = -vec3_dot(rel_vel, los);
    solution->closing_speed = Vc;

    /* LOS rotation rate: omega = (r x v) / |r|^2 */
    float r_cross_v[3];
    vec3_cross(r_cross_v, rel_pos, rel_vel);
    float los_rate[3] = {
        r_cross_v[0] / (range * range),
        r_cross_v[1] / (range * range),
        r_cross_v[2] / (range * range)
    };

    /* PN acceleration command: a = N * Vc * los_rate */
    solution->required_accel[0] = pn_gain * Vc * los_rate[0];
    solution->required_accel[1] = pn_gain * Vc * los_rate[1];
    solution->required_accel[2] = pn_gain * Vc * los_rate[2];

    /* Time to intercept */
    if (Vc > 0.1f) {
        solution->intercept_time_s = range / Vc;
        solution->feasibility = INTERCEPT_FEASIBLE;
    } else {
        solution->intercept_time_s = range / self->max_speed;
        solution->feasibility = INTERCEPT_ESCAPING;
    }

    /* Predicted intercept point */
    float t = solution->intercept_time_s;
    solution->intercept_point[0] = target->position[0] + target->velocity[0] * t;
    solution->intercept_point[1] = target->position[1] + target->velocity[1] * t;
    solution->intercept_point[2] = target->position[2] + target->velocity[2] * t;

    /* Heading and lead angle */
    float to_intercept[3];
    vec3_sub(to_intercept, solution->intercept_point, self->position);
    solution->heading_rad = atan2f(to_intercept[1], to_intercept[0]);

    float to_target_heading = atan2f(rel_pos[1], rel_pos[0]);
    solution->lead_angle_rad = solution->heading_rad - to_target_heading;

    /* Normalize lead angle */
    while (solution->lead_angle_rad > M_PI) solution->lead_angle_rad -= 2*M_PI;
    while (solution->lead_angle_rad < -M_PI) solution->lead_angle_rad += 2*M_PI;

    solution->required_speed = vec3_length(self->velocity) +
        vec3_length(solution->required_accel) * 0.1f;
    solution->strategy = INTERCEPT_PN;
}

//=============================================================================
// Core Interception Functions
//=============================================================================

int dragonfly_intercept_compute(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    intercept_solution_t* solution
) {
    if (!interceptor || !self || !target || !solution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_intercept_compute: required parameter is NULL (interceptor, self, target, solution)");
        return -1;
    }

    nimcp_mutex_lock(interceptor->mutex);

    uint64_t start = get_time_us();
    memset(solution, 0, sizeof(*solution));

    /* Select strategy */
    intercept_strategy_t strategy = interceptor->config.preferred_strategy;

    /* Compute based on strategy */
    switch (strategy) {
        case INTERCEPT_PURSUIT:
            compute_pursuit(self, target, solution);
            break;
        case INTERCEPT_LEAD:
            compute_lead_pursuit(self, target,
                interceptor->config.lead_time_factor, solution);
            break;
        case INTERCEPT_PARALLEL:
        case INTERCEPT_PN:
        case INTERCEPT_OPTIMAL:
        default:
            compute_proportional_nav(self, target,
                interceptor->config.pn_gain, solution);
            break;
    }

    /* Clamp acceleration to limits */
    float accel_mag = vec3_length(solution->required_accel);
    if (accel_mag > self->max_accel) {
        float scale = self->max_accel / accel_mag;
        solution->required_accel[0] *= scale;
        solution->required_accel[1] *= scale;
        solution->required_accel[2] *= scale;
    }

    /* Compute miss distance - closest point of approach (CPA) */
    /* Miss distance = perpendicular distance between trajectories at CPA */
    {
        float rel_pos[3], rel_vel[3];
        vec3_sub(rel_pos, target->position, self->position);
        vec3_sub(rel_vel, target->velocity, self->velocity);

        /* Time to CPA: t_cpa = -(r · v) / |v|^2 */
        float rel_vel_sq = vec3_dot(rel_vel, rel_vel);
        float t_cpa = 0.0f;
        if (rel_vel_sq > 1e-6f) {
            t_cpa = -vec3_dot(rel_pos, rel_vel) / rel_vel_sq;
            t_cpa = nimcp_clampf(t_cpa, 0.0f, solution->intercept_time_s * 2.0f);
        }

        /* Position at CPA (without maneuvering) */
        float self_cpa[3] = {
            self->position[0] + self->velocity[0] * t_cpa,
            self->position[1] + self->velocity[1] * t_cpa,
            self->position[2] + self->velocity[2] * t_cpa
        };
        float target_cpa[3] = {
            target->position[0] + target->velocity[0] * t_cpa,
            target->position[1] + target->velocity[1] * t_cpa,
            target->position[2] + target->velocity[2] * t_cpa
        };

        /* Miss distance is separation at CPA */
        float miss_vec[3];
        vec3_sub(miss_vec, target_cpa, self_cpa);
        solution->miss_distance = vec3_length(miss_vec);

        /* Account for maneuver capability - can reduce miss distance */
        float accel_capability = self->max_accel * t_cpa * t_cpa * 0.5f;
        solution->miss_distance = fmaxf(0.0f, solution->miss_distance - accel_capability);
    }

    /* Confidence based on target confidence and closing speed */
    solution->confidence = target->confidence *
        nimcp_clampf(solution->closing_speed / 10.0f, 0.1f, 1.0f);

    solution->timestamp_us = start;

    /* Update statistics */
    interceptor->stats.solutions_computed++;
    if (solution->feasibility == INTERCEPT_FEASIBLE) {
        interceptor->stats.feasible_count++;
    } else {
        interceptor->stats.infeasible_count++;
    }
    interceptor->stats.strategy_usage[solution->strategy]++;

    uint64_t elapsed = get_time_us() - start;
    float alpha = 0.1f;
    interceptor->stats.avg_compute_time_us =
        (1.0f - alpha) * interceptor->stats.avg_compute_time_us + alpha * elapsed;
    interceptor->stats.avg_intercept_time_s =
        (1.0f - alpha) * interceptor->stats.avg_intercept_time_s +
        alpha * solution->intercept_time_s;
    interceptor->stats.avg_lead_angle_rad =
        (1.0f - alpha) * interceptor->stats.avg_lead_angle_rad +
        alpha * fabsf(solution->lead_angle_rad);

    interceptor->last_solution = *solution;

    nimcp_mutex_unlock(interceptor->mutex);

    return 0;
}

int dragonfly_intercept_compute_strategy(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    intercept_strategy_t strategy,
    intercept_solution_t* solution
) {
    if (!interceptor || !self || !target || !solution) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_intercept_compute_strategy: required parameter is NULL (interceptor, self, target, solution)");
        return -1;
    }

    nimcp_mutex_lock(interceptor->mutex);

    memset(solution, 0, sizeof(*solution));

    switch (strategy) {
        case INTERCEPT_PURSUIT:
            compute_pursuit(self, target, solution);
            break;
        case INTERCEPT_LEAD:
            compute_lead_pursuit(self, target,
                interceptor->config.lead_time_factor, solution);
            break;
        case INTERCEPT_PARALLEL:
        case INTERCEPT_PN:
        case INTERCEPT_OPTIMAL:
        default:
            compute_proportional_nav(self, target,
                interceptor->config.pn_gain, solution);
            break;
    }

    solution->timestamp_us = get_time_us();
    solution->confidence = target->confidence;

    nimcp_mutex_unlock(interceptor->mutex);

    return 0;
}

intercept_feasibility_t dragonfly_intercept_check_feasibility(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target
) {
    if (!interceptor || !self || !target) return INTERCEPT_UNCERTAIN;

    float rel_pos[3], rel_vel[3];
    vec3_sub(rel_pos, target->position, self->position);
    vec3_sub(rel_vel, target->velocity, self->velocity);

    float range = vec3_length(rel_pos);
    float target_speed = vec3_length(target->velocity);

    /* Check if target is too fast */
    if (target_speed > self->max_speed * 1.5f) {
        return INTERCEPT_TOO_FAST;
    }

    /* Check if target is escaping */
    float closing = -vec3_dot(rel_vel, rel_pos) / (range + 1e-6f);
    if (closing < -self->max_speed * 0.5f) {
        return INTERCEPT_ESCAPING;
    }

    /* Check range limits */
    float max_range = self->max_speed * interceptor->config.max_intercept_time_s;
    if (range > max_range * interceptor->config.safety_margin) {
        return INTERCEPT_OUT_OF_RANGE;
    }

    /* Check confidence */
    if (target->confidence < 0.3f) {
        return INTERCEPT_UNCERTAIN;
    }

    return INTERCEPT_FEASIBLE;
}

int dragonfly_intercept_get_command(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    float accel_cmd[3]
) {
    if (!interceptor || !self || !target || !accel_cmd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_intercept_get_command: required parameter is NULL (interceptor, self, target, accel_cmd)");
        return -1;
    }

    intercept_solution_t solution;
    int result = dragonfly_intercept_compute(interceptor, self, target, &solution);
    if (result != 0) return result;

    memcpy(accel_cmd, solution.required_accel, sizeof(float) * 3);
    return 0;
}

//=============================================================================
// Trajectory Planning Functions
//=============================================================================

int dragonfly_intercept_plan_trajectory(
    dragonfly_interceptor_t* interceptor,
    const interceptor_state_t* self,
    const target_state_t* target,
    intercept_trajectory_t* trajectory
) {
    if (!interceptor || !self || !target || !trajectory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_intercept_plan_trajectory: required parameter is NULL (interceptor, self, target, trajectory)");
        return -1;
    }

    intercept_solution_t solution;
    int result = dragonfly_intercept_compute(interceptor, self, target, &solution);
    if (result != 0) return result;

    /* Generate waypoints along trajectory */
    float dt = solution.intercept_time_s / INTERCEPT_MAX_WAYPOINTS;
    float pos[3], vel[3];
    memcpy(pos, self->position, sizeof(pos));
    memcpy(vel, self->velocity, sizeof(vel));

    trajectory->num_waypoints = 0;
    trajectory->total_energy = 0.0f;
    trajectory->strategy = solution.strategy;

    for (uint32_t i = 0; i < INTERCEPT_MAX_WAYPOINTS && i * dt < solution.intercept_time_s; i++) {
        intercept_waypoint_t* wp = &trajectory->waypoints[i];

        /* Integrate position and velocity */
        pos[0] += vel[0] * dt + 0.5f * solution.required_accel[0] * dt * dt;
        pos[1] += vel[1] * dt + 0.5f * solution.required_accel[1] * dt * dt;
        pos[2] += vel[2] * dt + 0.5f * solution.required_accel[2] * dt * dt;
        vel[0] += solution.required_accel[0] * dt;
        vel[1] += solution.required_accel[1] * dt;
        vel[2] += solution.required_accel[2] * dt;

        memcpy(wp->position, pos, sizeof(wp->position));
        memcpy(wp->velocity, vel, sizeof(wp->velocity));
        wp->time_s = (i + 1) * dt;

        /* Energy cost (proportional to accel^2 * dt) */
        float accel_mag = vec3_length(solution.required_accel);
        wp->energy_cost = accel_mag * accel_mag * dt;
        trajectory->total_energy += wp->energy_cost;

        trajectory->num_waypoints++;
    }

    trajectory->total_time_s = solution.intercept_time_s;

    return 0;
}

int dragonfly_intercept_get_waypoint(
    const intercept_trajectory_t* trajectory,
    float time_s,
    intercept_waypoint_t* waypoint
) {
    if (!trajectory || !waypoint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_intercept_get_waypoint: required parameter is NULL (trajectory, waypoint)");
        return -1;
    }
    if (trajectory->num_waypoints == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_intercept_get_waypoint: trajectory->num_waypoints is zero");
        return -1;
    }

    /* Find bracketing waypoints and interpolate */
    for (uint32_t i = 0; i < trajectory->num_waypoints; i++) {
        if (trajectory->waypoints[i].time_s >= time_s) {
            if (i == 0) {
                *waypoint = trajectory->waypoints[0];
            } else {
                /* Linear interpolation */
                const intercept_waypoint_t* w0 = &trajectory->waypoints[i-1];
                const intercept_waypoint_t* w1 = &trajectory->waypoints[i];
                float t = (time_s - w0->time_s) / (w1->time_s - w0->time_s);

                for (int k = 0; k < 3; k++) {
                    waypoint->position[k] = w0->position[k] + t * (w1->position[k] - w0->position[k]);
                    waypoint->velocity[k] = w0->velocity[k] + t * (w1->velocity[k] - w0->velocity[k]);
                }
                waypoint->time_s = time_s;
                waypoint->energy_cost = w0->energy_cost + t * (w1->energy_cost - w0->energy_cost);
            }
            return 0;
        }
    }

    /* Beyond trajectory - return last point */
    *waypoint = trajectory->waypoints[trajectory->num_waypoints - 1];
    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

float dragonfly_intercept_time_to_intercept(
    const interceptor_state_t* self,
    const target_state_t* target
) {
    if (!self || !target) return -1.0f;

    float rel_pos[3], rel_vel[3];
    vec3_sub(rel_pos, target->position, self->position);
    vec3_sub(rel_vel, target->velocity, self->velocity);

    float range = vec3_length(rel_pos);
    float closing = -vec3_dot(rel_vel, rel_pos) / (range + 1e-6f);

    if (closing > 0.1f) {
        return range / closing;
    }

    return range / self->max_speed;
}

float dragonfly_intercept_lead_angle(
    const interceptor_state_t* self,
    const target_state_t* target
) {
    if (!self || !target) return 0.0f;

    float rel_pos[3];
    vec3_sub(rel_pos, target->position, self->position);
    float range = vec3_length(rel_pos);

    if (range < 1e-6f) return 0.0f;

    float target_speed = vec3_length(target->velocity);
    float self_speed = self->max_speed;

    if (self_speed < 1e-6f) return 0.0f;

    /* Lead angle from law of sines */
    float sin_lead = target_speed * sinf(M_PI / 4.0f) / self_speed;
    sin_lead = nimcp_clampf(sin_lead, -1.0f, 1.0f);

    return asinf(sin_lead);
}

float dragonfly_intercept_bearing(
    const interceptor_state_t* self,
    const target_state_t* target
) {
    if (!self || !target) return 0.0f;

    float rel_pos[3];
    vec3_sub(rel_pos, target->position, self->position);

    return atan2f(rel_pos[1], rel_pos[0]);
}

float dragonfly_intercept_closing_speed(
    const interceptor_state_t* self,
    const target_state_t* target
) {
    if (!self || !target) return 0.0f;

    float rel_pos[3], rel_vel[3];
    vec3_sub(rel_pos, target->position, self->position);
    vec3_sub(rel_vel, target->velocity, self->velocity);

    float range = vec3_length(rel_pos);
    if (range < 1e-6f) return 0.0f;

    return -vec3_dot(rel_vel, rel_pos) / range;
}

float dragonfly_intercept_range(
    const interceptor_state_t* self,
    const target_state_t* target
) {
    if (!self || !target) return 0.0f;

    float rel_pos[3];
    vec3_sub(rel_pos, target->position, self->position);

    return vec3_length(rel_pos);
}

//=============================================================================
// Statistics and Configuration
//=============================================================================

int dragonfly_interceptor_get_stats(
    const dragonfly_interceptor_t* interceptor,
    intercept_stats_t* stats
) {
    if (!interceptor || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_interceptor_get_stats: required parameter is NULL (interceptor, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)interceptor->mutex);
    *stats = interceptor->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)interceptor->mutex);

    return 0;
}

int dragonfly_interceptor_reset_stats(dragonfly_interceptor_t* interceptor) {
    if (!interceptor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_interceptor_reset_stats: interceptor is NULL");
        return -1;
    }

    nimcp_mutex_lock(interceptor->mutex);
    memset(&interceptor->stats, 0, sizeof(interceptor->stats));
    nimcp_mutex_unlock(interceptor->mutex);

    return 0;
}

int dragonfly_interceptor_set_config(
    dragonfly_interceptor_t* interceptor,
    const intercept_config_t* config
) {
    if (!interceptor || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_interceptor_set_config: required parameter is NULL (interceptor, config)");
        return -1;
    }
    if (!intercept_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_interceptor_set_config: intercept_validate_config is NULL");
        return -1;
    }

    nimcp_mutex_lock(interceptor->mutex);
    interceptor->config = *config;
    nimcp_mutex_unlock(interceptor->mutex);

    return 0;
}

int dragonfly_interceptor_get_config(
    const dragonfly_interceptor_t* interceptor,
    intercept_config_t* config
) {
    if (!interceptor || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_interceptor_get_config: required parameter is NULL (interceptor, config)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)interceptor->mutex);
    *config = interceptor->config;
    nimcp_mutex_unlock((nimcp_mutex_t*)interceptor->mutex);

    return 0;
}

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_strategy_name(intercept_strategy_t strategy) {
    switch (strategy) {
        case INTERCEPT_PURSUIT:  return "PURSUIT";
        case INTERCEPT_LEAD:     return "LEAD";
        case INTERCEPT_PARALLEL: return "PARALLEL";
        case INTERCEPT_PN:       return "PN";
        case INTERCEPT_OPTIMAL:  return "OPTIMAL";
        default:                 return "UNKNOWN";
    }
}

const char* dragonfly_feasibility_name(intercept_feasibility_t feasibility) {
    switch (feasibility) {
        case INTERCEPT_FEASIBLE:     return "FEASIBLE";
        case INTERCEPT_TOO_FAST:     return "TOO_FAST";
        case INTERCEPT_OUT_OF_RANGE: return "OUT_OF_RANGE";
        case INTERCEPT_ESCAPING:     return "ESCAPING";
        case INTERCEPT_UNCERTAIN:    return "UNCERTAIN";
        default:                     return "UNKNOWN";
    }
}

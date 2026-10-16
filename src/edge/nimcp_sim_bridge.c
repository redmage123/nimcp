/**
 * @file nimcp_sim_bridge.c
 * @brief Simulation Bridge — sim-to-real pipeline for Gazebo/Unity/Isaac Sim.
 *
 * Two compilation modes:
 *   - Stub mode (default): No simulator dependency. Built-in cart-pole
 *     (inverted pendulum) physics provides non-trivial training data so
 *     developers can test the full sim-to-real pipeline without installing
 *     Gazebo, Unity, or Isaac Sim.
 *   - Full mode (NIMCP_HAS_SIM_GAZEBO / NIMCP_HAS_SIM_UNITY /
 *     NIMCP_HAS_SIM_ISAAC): Links against simulator SDKs.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "SIM_BRIDGE"

#include "edge/nimcp_sim_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** Default pendulum length (meters) */
#define STUB_PENDULUM_LENGTH    0.5f

/** Default gravity (m/s^2) */
#define STUB_DEFAULT_GRAVITY    9.81f

/** Default friction coefficient */
#define STUB_DEFAULT_FRICTION   0.1f

/** Default mass (kg) */
#define STUB_DEFAULT_MASS       1.0f

/** Default sensor noise level */
#define STUB_DEFAULT_NOISE      0.01f

/* Collision thresholds (hardcoded for cart-pole stub physics):
 * - angle > PI/2 (90 degrees) = fallen over
 * - cart position > 2.0m = off-track
 * For real sim backends, these would come from the sim config. */
#define STUB_ANGLE_THRESHOLD    (M_PI / 2.0f)
#define STUB_CART_THRESHOLD     2.0f

/** Reward angle threshold (radians) — reward=1.0 if |angle| < this */
#define STUB_REWARD_ANGLE       0.2f

/** Number of stub joints (cart-pole = 1 joint: the pole angle) */
#define STUB_NUM_JOINTS         1

/** Total features: 13 base + num_joints */
#define SIM_BASE_FEATURES       13

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_sim_bridge {
    nimcp_sim_config_t config;
    bool connected;

    /* Stub physics state (cart-pole / inverted pendulum) */
    float stub_angle;              /* Pendulum angle (rad) */
    float stub_angular_vel;        /* Angular velocity (rad/s) */
    float stub_cart_pos;           /* Cart position */
    float stub_cart_vel;           /* Cart velocity */
    float stub_time;               /* Simulation time */
    uint32_t stub_step_count;

    /* Domain randomization state */
    float gravity;                 /* Randomized gravity (default 9.81) */
    float friction;                /* Randomized friction */
    float mass;                    /* Randomized mass */
    float sensor_noise;            /* Current noise level */
    uint32_t rng_state;            /* Simple PRNG state */

#ifdef NIMCP_HAS_SIM_GAZEBO
    /* Gazebo-specific handles would go here */
#endif
#ifdef NIMCP_HAS_SIM_UNITY
    int socket_fd;
#endif
#ifdef NIMCP_HAS_SIM_ISAAC
    /* Isaac Sim-specific handles would go here */
#endif
};

/* ============================================================================
 * Simple PRNG (xorshift32)
 * ============================================================================ */

static uint32_t sim_rng_next(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/**
 * @brief Return a uniform random float in [0, 1].
 */
static float sim_rng_uniform(uint32_t* state) {
    return (float)(sim_rng_next(state) & 0x00FFFFFF) / (float)0x00FFFFFF;
}

/**
 * @brief Return a random float in [-range, +range].
 */
static float sim_rng_range(uint32_t* state, float range) {
    return (sim_rng_uniform(state) * 2.0f - 1.0f) * range;
}

/**
 * @brief Return an approximate Gaussian sample (Box-Muller, single output).
 */
static float sim_rng_gaussian(uint32_t* state, float stddev) {
    float u1 = sim_rng_uniform(state);
    float u2 = sim_rng_uniform(state);
    if (u1 < 1e-10f) { u1 = 1e-10f; }
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return z * stddev;
}

/* ============================================================================
 * Stub Physics: Cart-Pole Step
 * ============================================================================ */

/**
 * @brief Advance cart-pole physics by one timestep.
 *
 * Dynamics:
 *   angle_accel = (g * sin(angle) - friction * angular_vel
 *                  + action * cos(angle) / mass) / length
 *   angular_vel += angle_accel * dt
 *   angle       += angular_vel * dt
 *   cart_pos    += action * dt
 */
static void stub_physics_step(nimcp_sim_bridge_t* bridge, float action, float dt) {
    float g       = bridge->gravity;
    float fric    = bridge->friction;
    float m       = bridge->mass;
    float length  = STUB_PENDULUM_LENGTH;
    float angle   = bridge->stub_angle;
    float ang_vel = bridge->stub_angular_vel;

    /* Cart-pole dynamics */
    float angle_accel = (g * sinf(angle) - fric * ang_vel
                         + action * cosf(angle) / m) / length;
    ang_vel += angle_accel * dt;
    angle   += ang_vel * dt;

    /* Cart dynamics */
    bridge->stub_cart_vel = action;
    bridge->stub_cart_pos += action * dt;

    bridge->stub_angle      = angle;
    bridge->stub_angular_vel = ang_vel;
    bridge->stub_time       += dt;
    bridge->stub_step_count++;
}

/**
 * @brief Fill a sim state from the current stub physics state.
 */
static void stub_fill_state(const nimcp_sim_bridge_t* bridge,
                             nimcp_sim_state_t* state) {
    if (!state) { return; }

    /* Body position: cart on x-axis, pendulum tip on y-axis */
    if (state->body_position) {
        state->body_position[0] = bridge->stub_cart_pos;
        state->body_position[1] = STUB_PENDULUM_LENGTH * cosf(bridge->stub_angle);
        state->body_position[2] = 0.0f;
    }

    /* Body orientation: encode pole angle as a rotation about z */
    if (state->body_orientation) {
        float half_angle = bridge->stub_angle * 0.5f;
        state->body_orientation[0] = cosf(half_angle);  /* w */
        state->body_orientation[1] = 0.0f;               /* x */
        state->body_orientation[2] = 0.0f;               /* y */
        state->body_orientation[3] = sinf(half_angle);   /* z */
    }

    /* Body velocity */
    if (state->body_velocity) {
        state->body_velocity[0] = bridge->stub_cart_vel;
        state->body_velocity[1] = -STUB_PENDULUM_LENGTH * bridge->stub_angular_vel
                                    * sinf(bridge->stub_angle);
        state->body_velocity[2] = 0.0f;
    }

    /* Body angular velocity */
    if (state->body_angular_velocity) {
        state->body_angular_velocity[0] = 0.0f;
        state->body_angular_velocity[1] = 0.0f;
        state->body_angular_velocity[2] = bridge->stub_angular_vel;
    }

    /* Joint: pole angle */
    if (state->joint_positions && state->num_joints >= 1) {
        state->joint_positions[0] = bridge->stub_angle;
    }
    if (state->joint_velocities && state->num_joints >= 1) {
        state->joint_velocities[0] = bridge->stub_angular_vel;
    }

    state->sim_time = bridge->stub_time;

    /* Collision: angle too large or cart out of bounds */
    state->collision_detected = (fabsf(bridge->stub_angle) > STUB_ANGLE_THRESHOLD)
                                || (fabsf(bridge->stub_cart_pos) > STUB_CART_THRESHOLD);

    /* Reward: 1.0 if pole is near upright */
    state->reward = (fabsf(bridge->stub_angle) < STUB_REWARD_ANGLE) ? 1.0f : 0.0f;
}

/**
 * @brief Add sensor noise to a state (sim-to-real gap simulation).
 */
static void stub_add_noise(nimcp_sim_bridge_t* bridge, nimcp_sim_state_t* state) {
    float noise = bridge->sensor_noise;
    if (noise <= 0.0f) { return; }

    if (state->body_position) {
        for (int i = 0; i < 3; i++) {
            state->body_position[i] += sim_rng_gaussian(&bridge->rng_state, noise);
        }
    }
    if (state->body_velocity) {
        for (int i = 0; i < 3; i++) {
            state->body_velocity[i] += sim_rng_gaussian(&bridge->rng_state, noise);
        }
    }
    if (state->body_angular_velocity) {
        for (int i = 0; i < 3; i++) {
            state->body_angular_velocity[i] += sim_rng_gaussian(&bridge->rng_state, noise);
        }
    }
    if (state->joint_positions) {
        for (uint32_t i = 0; i < state->num_joints; i++) {
            state->joint_positions[i] += sim_rng_gaussian(&bridge->rng_state, noise);
        }
    }
}

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

nimcp_domain_randomization_t nimcp_domain_randomization_default(void) {
    nimcp_domain_randomization_t dr;
    memset(&dr, 0, sizeof(dr));
    dr.randomize_physics    = true;
    dr.randomize_visuals    = true;
    dr.randomize_sensors    = true;
    dr.randomize_dynamics   = true;
    dr.physics_range        = 0.3f;
    dr.visual_range         = 0.3f;
    dr.sensor_noise_range   = 0.3f;
    dr.dynamics_range       = 0.3f;
    dr.randomization_seed   = 42;
    return dr;
}

nimcp_sim_config_t nimcp_sim_config_default(void) {
    nimcp_sim_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.sim_type                    = NIMCP_SIM_GAZEBO;
    cfg.sim_step_hz                 = 240.0f;
    cfg.brain_hz                    = 30.0f;
    cfg.sync_mode                   = true;
    cfg.enable_domain_randomization = false;
    cfg.randomization               = nimcp_domain_randomization_default();
    cfg.action_noise_stddev         = 0.0f;
    cfg.observation_delay_ms        = 0.0f;
    cfg.observation_dropout_pct     = 0;
    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_sim_bridge_t* nimcp_sim_bridge_create(const nimcp_sim_config_t* config) {
    nimcp_sim_bridge_t* bridge = (nimcp_sim_bridge_t*)nimcp_calloc(1, sizeof(nimcp_sim_bridge_t));
    if (!bridge) {
        LOG_ERROR("Failed to allocate sim bridge");
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = nimcp_sim_config_default();
    }

    /* Initialize physics defaults */
    bridge->gravity      = STUB_DEFAULT_GRAVITY;
    bridge->friction     = STUB_DEFAULT_FRICTION;
    bridge->mass         = STUB_DEFAULT_MASS;
    bridge->sensor_noise = STUB_DEFAULT_NOISE;
    bridge->rng_state    = bridge->config.randomization.randomization_seed;
    if (bridge->rng_state == 0) { bridge->rng_state = 12345; }

    bridge->connected = false;

    const char* sim_names[] = { "Gazebo", "Unity", "Isaac Sim", "Custom" };
    int sim_idx = (int)bridge->config.sim_type;
    if (sim_idx < 0 || sim_idx > 3) { sim_idx = 3; }
    LOG_INFO("Sim bridge created (type=%s, step_hz=%.0f, brain_hz=%.0f, sync=%s)",
             sim_names[sim_idx], bridge->config.sim_step_hz, bridge->config.brain_hz,
             bridge->config.sync_mode ? "true" : "false");

    return bridge;
}

void nimcp_sim_bridge_destroy(nimcp_sim_bridge_t* bridge) {
    if (!bridge) { return; }

    if (bridge->connected) {
        nimcp_sim_bridge_disconnect(bridge);
    }

    LOG_INFO("Sim bridge destroyed (steps=%u, sim_time=%.2fs)",
             bridge->stub_step_count, bridge->stub_time);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection
 * ============================================================================ */

int nimcp_sim_bridge_connect(nimcp_sim_bridge_t* bridge) {
    if (!bridge) { return -1; }
    if (bridge->connected) {
        LOG_WARN("Sim bridge already connected");
        return 0;
    }

#if defined(NIMCP_HAS_SIM_GAZEBO)
    /* TODO: Connect to Gazebo via ROS 2 transport */
    LOG_ERROR("Gazebo full mode not yet implemented");
    return -1;
#elif defined(NIMCP_HAS_SIM_UNITY)
    /* TODO: Connect to Unity via TCP socket */
    LOG_ERROR("Unity full mode not yet implemented");
    return -1;
#elif defined(NIMCP_HAS_SIM_ISAAC)
    /* TODO: Connect to Isaac Sim via USD/ROS 2 */
    LOG_ERROR("Isaac Sim full mode not yet implemented");
    return -1;
#else
    /* Stub mode: always succeeds */
    bridge->connected = true;
    LOG_INFO("Sim bridge connected (stub mode — cart-pole physics)");
    return 0;
#endif
}

int nimcp_sim_bridge_disconnect(nimcp_sim_bridge_t* bridge) {
    if (!bridge) { return -1; }
    if (!bridge->connected) {
        LOG_WARN("Sim bridge not connected");
        return 0;
    }

#ifdef NIMCP_HAS_SIM_UNITY
    /* TODO: Close TCP socket */
#endif

    bridge->connected = false;
    LOG_INFO("Sim bridge disconnected");
    return 0;
}

bool nimcp_sim_bridge_is_connected(const nimcp_sim_bridge_t* bridge) {
    if (!bridge) { return false; }
    return bridge->connected;
}

/* ============================================================================
 * Simulation Control
 * ============================================================================ */

int nimcp_sim_bridge_step(nimcp_sim_bridge_t* bridge,
                          const float* actions, uint32_t num_actions,
                          nimcp_sim_state_t* state_out) {
    if (!bridge) { return -1; }
    if (!bridge->connected) {
        LOG_ERROR("Cannot step: sim bridge not connected");
        return -1;
    }

    /* Extract the first action as the cart-pole force input */
    float action = 0.0f;
    if (actions && num_actions > 0) {
        action = actions[0];
    }

    /* Add action noise for sim-to-real gap reduction */
    if (bridge->config.action_noise_stddev > 0.0f) {
        action += sim_rng_gaussian(&bridge->rng_state,
                                   bridge->config.action_noise_stddev);
    }

    /* Compute timestep from sim_step_hz */
    float dt = 1.0f / bridge->config.sim_step_hz;
    if (dt <= 0.0f || dt > 1.0f) { dt = 1.0f / 240.0f; }

    /* Step physics */
    stub_physics_step(bridge, action, dt);

    /* Fill output state */
    if (state_out) {
        stub_fill_state(bridge, state_out);

        /* Add sensor noise if configured */
        if (bridge->config.enable_domain_randomization
            && bridge->config.randomization.randomize_sensors) {
            stub_add_noise(bridge, state_out);
        }
    }

    return 0;
}

int nimcp_sim_bridge_reset(nimcp_sim_bridge_t* bridge,
                            nimcp_sim_state_t* initial_state) {
    if (!bridge) { return -1; }

    /* Reset physics state */
    bridge->stub_angle       = 0.0f;
    bridge->stub_angular_vel = 0.0f;
    bridge->stub_cart_pos    = 0.0f;
    bridge->stub_cart_vel    = 0.0f;
    bridge->stub_time        = 0.0f;
    bridge->stub_step_count  = 0;

    /* Add small random perturbation so episodes aren't identical */
    bridge->stub_angle       = sim_rng_range(&bridge->rng_state, 0.05f);
    bridge->stub_angular_vel = sim_rng_range(&bridge->rng_state, 0.05f);

    /* Apply domain randomization if enabled */
    if (bridge->config.enable_domain_randomization) {
        nimcp_sim_bridge_randomize(bridge);
    }

    /* Mark as connected if not already (reset implies readiness) */
    if (!bridge->connected) {
        bridge->connected = true;
    }

    /* Fill initial state */
    if (initial_state) {
        stub_fill_state(bridge, initial_state);
    }

    LOG_DEBUG("Sim reset (gravity=%.2f, friction=%.2f, mass=%.2f, angle=%.4f)",
              bridge->gravity, bridge->friction, bridge->mass, bridge->stub_angle);

    return 0;
}

int nimcp_sim_bridge_get_state(const nimcp_sim_bridge_t* bridge,
                                nimcp_sim_state_t* state) {
    if (!bridge || !state) { return -1; }
    stub_fill_state(bridge, state);
    return 0;
}

/* ============================================================================
 * Sensor Integration
 * ============================================================================ */

int nimcp_sim_bridge_compose_sensors(const nimcp_sim_bridge_t* bridge,
                                     const nimcp_sim_state_t* state,
                                     float* features, uint32_t max_features) {
    if (!bridge || !state || !features || max_features == 0) { return -1; }

    uint32_t total_needed = SIM_BASE_FEATURES + state->num_joints;
    uint32_t to_write = (total_needed < max_features) ? total_needed : max_features;
    uint32_t idx = 0;

    /* Features 0-2: body position (x,y,z) */
    if (state->body_position) {
        for (int i = 0; i < 3 && idx < to_write; i++, idx++) {
            features[idx] = state->body_position[i];
        }
    } else {
        for (int i = 0; i < 3 && idx < to_write; i++, idx++) {
            features[idx] = 0.0f;
        }
    }

    /* Features 3-6: body orientation (quaternion w,x,y,z) */
    if (state->body_orientation) {
        for (int i = 0; i < 4 && idx < to_write; i++, idx++) {
            features[idx] = state->body_orientation[i];
        }
    } else {
        /* Identity quaternion */
        if (idx < to_write) { features[idx++] = 1.0f; }
        for (int i = 0; i < 3 && idx < to_write; i++, idx++) {
            features[idx] = 0.0f;
        }
    }

    /* Features 7-9: body velocity (x,y,z) */
    if (state->body_velocity) {
        for (int i = 0; i < 3 && idx < to_write; i++, idx++) {
            features[idx] = state->body_velocity[i];
        }
    } else {
        for (int i = 0; i < 3 && idx < to_write; i++, idx++) {
            features[idx] = 0.0f;
        }
    }

    /* Features 10-12: body angular velocity (x,y,z) */
    if (state->body_angular_velocity) {
        for (int i = 0; i < 3 && idx < to_write; i++, idx++) {
            features[idx] = state->body_angular_velocity[i];
        }
    } else {
        for (int i = 0; i < 3 && idx < to_write; i++, idx++) {
            features[idx] = 0.0f;
        }
    }

    /* Features 13+: joint positions */
    if (state->joint_positions) {
        for (uint32_t j = 0; j < state->num_joints && idx < to_write; j++, idx++) {
            features[idx] = state->joint_positions[j];
        }
    }

    return (int)idx;
}

/* ============================================================================
 * Domain Randomization
 * ============================================================================ */

int nimcp_sim_bridge_randomize(nimcp_sim_bridge_t* bridge) {
    if (!bridge) { return -1; }

    const nimcp_domain_randomization_t* dr = &bridge->config.randomization;

    if (dr->randomize_physics) {
        float range = dr->physics_range;
        bridge->gravity  = STUB_DEFAULT_GRAVITY
                           * (1.0f + sim_rng_range(&bridge->rng_state, range));
        bridge->friction = STUB_DEFAULT_FRICTION
                           * (1.0f + sim_rng_range(&bridge->rng_state, range));
        bridge->mass     = STUB_DEFAULT_MASS
                           * (1.0f + sim_rng_range(&bridge->rng_state, range));

        /* Clamp to positive values */
        if (bridge->gravity  < 0.1f) { bridge->gravity  = 0.1f; }
        if (bridge->friction < 0.0f) { bridge->friction = 0.0f; }
        if (bridge->mass     < 0.1f) { bridge->mass     = 0.1f; }
    }

    if (dr->randomize_sensors) {
        bridge->sensor_noise = STUB_DEFAULT_NOISE
                               * (1.0f + sim_rng_range(&bridge->rng_state,
                                                        dr->sensor_noise_range));
        if (bridge->sensor_noise < 0.0f) { bridge->sensor_noise = 0.0f; }
    }

    LOG_DEBUG("Domain randomized: gravity=%.3f, friction=%.3f, mass=%.3f, noise=%.4f",
              bridge->gravity, bridge->friction, bridge->mass, bridge->sensor_noise);

    return 0;
}

/* ============================================================================
 * Sim State Memory Management
 * ============================================================================ */

nimcp_sim_state_t* nimcp_sim_state_create(uint32_t num_joints) {
    nimcp_sim_state_t* state = (nimcp_sim_state_t*)nimcp_calloc(1, sizeof(nimcp_sim_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate sim state");
        return NULL;
    }

    state->num_joints = num_joints;

    /* Allocate body arrays (fixed sizes) */
    state->body_position         = (float*)nimcp_calloc(3, sizeof(float));
    state->body_orientation      = (float*)nimcp_calloc(4, sizeof(float));
    state->body_velocity         = (float*)nimcp_calloc(3, sizeof(float));
    state->body_angular_velocity = (float*)nimcp_calloc(3, sizeof(float));

    if (!state->body_position || !state->body_orientation
        || !state->body_velocity || !state->body_angular_velocity) {
        nimcp_sim_state_destroy(state);
        return NULL;
    }

    /* Identity quaternion default */
    state->body_orientation[0] = 1.0f;

    /* Allocate joint arrays */
    if (num_joints > 0) {
        state->joint_positions  = (float*)nimcp_calloc(num_joints, sizeof(float));
        state->joint_velocities = (float*)nimcp_calloc(num_joints, sizeof(float));
        if (!state->joint_positions || !state->joint_velocities) {
            nimcp_sim_state_destroy(state);
            return NULL;
        }
    }

    return state;
}

void nimcp_sim_state_destroy(nimcp_sim_state_t* state) {
    if (!state) { return; }

    nimcp_free(state->joint_positions);
    nimcp_free(state->joint_velocities);
    nimcp_free(state->body_position);
    nimcp_free(state->body_orientation);
    nimcp_free(state->body_velocity);
    nimcp_free(state->body_angular_velocity);
    nimcp_free(state);
}

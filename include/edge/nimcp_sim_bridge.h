/**
 * @file nimcp_sim_bridge.h
 * @brief Simulation Bridge — sim-to-real pipeline for Gazebo/Unity/Isaac Sim.
 *
 * Enables NIMCP brains to train in simulation before deploying to real
 * hardware. Supports domain randomization, synchronous/async stepping,
 * and sensor composition for seamless integration with the sensor hub.
 *
 * Compiles in TWO modes:
 *   - Stub mode (default): No simulator dependency. Built-in cart-pole
 *     physics provides non-trivial training data for pipeline testing.
 *   - Full mode (NIMCP_HAS_SIM_GAZEBO / NIMCP_HAS_SIM_UNITY /
 *     NIMCP_HAS_SIM_ISAAC): Links against simulator SDKs for real
 *     sim-to-real transfer.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_SIM_BRIDGE_H
#define NIMCP_SIM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Simulator Type
 * ============================================================================ */

typedef enum {
    NIMCP_SIM_GAZEBO = 0,         /* Gazebo (via ROS 2 topics) */
    NIMCP_SIM_UNITY,               /* Unity (via TCP socket) */
    NIMCP_SIM_ISAAC,               /* NVIDIA Isaac Sim (via USD/ROS 2) */
    NIMCP_SIM_CUSTOM,              /* Custom simulator (via shared memory or socket) */
} nimcp_sim_type_t;

/* ============================================================================
 * Domain Randomization Config
 * ============================================================================ */

typedef struct {
    bool randomize_physics;        /* Vary gravity, friction, mass */
    bool randomize_visuals;        /* Vary lighting, textures, colors */
    bool randomize_sensors;        /* Add noise, delay, dropout to sensors */
    bool randomize_dynamics;       /* Vary motor response, latency */
    float physics_range;           /* How much to vary physics params (0-1) */
    float visual_range;
    float sensor_noise_range;
    float dynamics_range;
    uint32_t randomization_seed;
} nimcp_domain_randomization_t;

/* ============================================================================
 * Sim Bridge Config
 * ============================================================================ */

typedef struct {
    nimcp_sim_type_t sim_type;
    char connection_string[256];   /* e.g., "localhost:9090" for Unity TCP */
    float sim_step_hz;             /* Simulation step rate (default 240) */
    float brain_hz;                /* Brain inference rate (default 30) */
    bool sync_mode;                /* Step sim synchronously with brain (true) or real-time (false) */
    bool enable_domain_randomization;
    nimcp_domain_randomization_t randomization;

    /* Sim-to-real gap reduction */
    float action_noise_stddev;     /* Add noise to actions during sim training */
    float observation_delay_ms;    /* Simulate sensor latency */
    uint32_t observation_dropout_pct; /* Drop N% of sensor readings */
} nimcp_sim_config_t;

/* ============================================================================
 * Sim State Snapshot
 * ============================================================================ */

typedef struct {
    float* joint_positions;        /* Current joint angles */
    float* joint_velocities;       /* Current joint velocities */
    float* body_position;          /* Body position (x,y,z) */
    float* body_orientation;       /* Body orientation (quaternion w,x,y,z) */
    float* body_velocity;          /* Body linear velocity (x,y,z) */
    float* body_angular_velocity;  /* Body angular velocity (x,y,z) */
    uint32_t num_joints;
    float sim_time;                /* Current simulation time */
    bool collision_detected;       /* Any collision this step? */
    float reward;                  /* Optional reward signal from sim */
} nimcp_sim_state_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_sim_bridge nimcp_sim_bridge_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Create a simulation bridge with the given configuration.
 * @param config Pointer to configuration. NULL uses defaults.
 * @return Bridge handle, or NULL on allocation failure.
 */
nimcp_sim_bridge_t* nimcp_sim_bridge_create(const nimcp_sim_config_t* config);

/**
 * @brief Destroy a simulation bridge and free all resources. NULL-safe.
 */
void nimcp_sim_bridge_destroy(nimcp_sim_bridge_t* bridge);

/* ============================================================================
 * Connection
 * ============================================================================ */

/**
 * @brief Connect to the simulator.
 *
 * In stub mode: always succeeds (marks bridge as connected).
 * In full mode: establishes connection to Gazebo/Unity/Isaac Sim.
 *
 * @return 0 on success, -1 on failure.
 */
int nimcp_sim_bridge_connect(nimcp_sim_bridge_t* bridge);

/**
 * @brief Disconnect from the simulator.
 * @return 0 on success, -1 on failure.
 */
int nimcp_sim_bridge_disconnect(nimcp_sim_bridge_t* bridge);

/**
 * @brief Check if the bridge is connected to a simulator.
 */
bool nimcp_sim_bridge_is_connected(const nimcp_sim_bridge_t* bridge);

/* ============================================================================
 * Simulation Control
 * ============================================================================ */

/**
 * @brief Step the simulation and get the new state.
 *
 * Applies the given actions to the simulator, advances by one timestep,
 * and returns the resulting state. In stub mode, runs cart-pole dynamics.
 *
 * @param bridge       Bridge handle.
 * @param actions      Array of action values to apply.
 * @param num_actions  Number of action values.
 * @param state_out    Output state snapshot (caller-allocated).
 * @return 0 on success, -1 on failure.
 */
int nimcp_sim_bridge_step(nimcp_sim_bridge_t* bridge,
    const float* actions, uint32_t num_actions,
    nimcp_sim_state_t* state_out);

/**
 * @brief Reset simulation to initial state.
 *
 * Optionally applies domain randomization if enabled in config.
 *
 * @param bridge         Bridge handle.
 * @param initial_state  Output: the initial state after reset (caller-allocated).
 * @return 0 on success, -1 on failure.
 */
int nimcp_sim_bridge_reset(nimcp_sim_bridge_t* bridge,
    nimcp_sim_state_t* initial_state);

/**
 * @brief Get the current sim state without stepping.
 * @return 0 on success, -1 on failure.
 */
int nimcp_sim_bridge_get_state(const nimcp_sim_bridge_t* bridge,
    nimcp_sim_state_t* state);

/* ============================================================================
 * Sensor Integration
 * ============================================================================ */

/**
 * @brief Compose sensor-compatible features from a sim state.
 *
 * Converts the sim state into a flat feature vector suitable for feeding
 * into the sensor hub or directly into the brain's sensory cortex.
 *
 * Feature layout:
 *   [0-2]   body position (x,y,z)
 *   [3-6]   body orientation (quaternion w,x,y,z)
 *   [7-9]   body velocity (x,y,z)
 *   [10-12] body angular velocity (x,y,z)
 *   [13+]   joint positions
 *
 * Total: 13 + num_joints features.
 *
 * @param bridge        Bridge handle.
 * @param state         Sim state to convert.
 * @param features      Output feature array (caller-allocated).
 * @param max_features  Maximum number of floats to write.
 * @return Number of features written, or -1 on failure.
 */
int nimcp_sim_bridge_compose_sensors(const nimcp_sim_bridge_t* bridge,
    const nimcp_sim_state_t* state, float* features, uint32_t max_features);

/* ============================================================================
 * Domain Randomization
 * ============================================================================ */

/**
 * @brief Apply domain randomization to the current simulation.
 *
 * Randomizes physics parameters (gravity, friction, mass) and sensor noise
 * according to the configured randomization ranges.
 *
 * @return 0 on success, -1 on failure.
 */
int nimcp_sim_bridge_randomize(nimcp_sim_bridge_t* bridge);

/* ============================================================================
 * Sim State Memory Management
 * ============================================================================ */

/**
 * @brief Allocate a sim state with capacity for num_joints joints.
 * @return Allocated state, or NULL on failure.
 */
nimcp_sim_state_t* nimcp_sim_state_create(uint32_t num_joints);

/**
 * @brief Free a sim state allocated by nimcp_sim_state_create(). NULL-safe.
 */
void nimcp_sim_state_destroy(nimcp_sim_state_t* state);

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

/**
 * @brief Return a default sim bridge configuration.
 *
 * Defaults: Gazebo sim type, 240 Hz sim step, 30 Hz brain,
 * synchronous mode, no domain randomization.
 */
nimcp_sim_config_t nimcp_sim_config_default(void);

/**
 * @brief Return a default domain randomization configuration.
 *
 * Defaults: all randomization enabled, moderate ranges (0.3).
 */
nimcp_domain_randomization_t nimcp_domain_randomization_default(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SIM_BRIDGE_H */

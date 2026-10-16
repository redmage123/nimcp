/**
 * @file nimcp_safety_watchdog.h
 * @brief Safety watchdog for robot/drone actuator control.
 *
 * Critical safety layer that monitors brain output and stops motors if the
 * brain goes silent (missed heartbeats), produces invalid output (NaN/Inf,
 * out-of-range), or is manually e-stopped.
 *
 * Usage:
 *   1. Create watchdog with nimcp_watchdog_create(&config)
 *   2. Arm with nimcp_watchdog_arm(wd)
 *   3. After each inference: nimcp_watchdog_heartbeat(wd)
 *   4. Before sending to actuators: nimcp_watchdog_validate_output(wd, out, n)
 *   5. If validation fails: nimcp_watchdog_get_safe_output(wd, out, n)
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_SAFETY_WATCHDOG_H
#define NIMCP_SAFETY_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Watchdog State
 * ============================================================================ */

typedef enum {
    NIMCP_WATCHDOG_IDLE = 0,       /* Not armed */
    NIMCP_WATCHDOG_ARMED,          /* Running, expecting heartbeats */
    NIMCP_WATCHDOG_TRIGGERED,      /* Brain missed deadline — safe stop active */
    NIMCP_WATCHDOG_ESTOP,          /* Emergency stop — manual reset required */
} nimcp_watchdog_state_t;

/* ============================================================================
 * Safe Action (what to do when watchdog triggers)
 * ============================================================================ */

typedef enum {
    NIMCP_SAFE_ACTION_STOP = 0,    /* Zero all outputs (default) */
    NIMCP_SAFE_ACTION_HOLD,        /* Hold last valid output */
    NIMCP_SAFE_ACTION_POSE,        /* Go to configured safe pose */
    NIMCP_SAFE_ACTION_LAND,        /* Drone: controlled landing (treated as callback) */
    NIMCP_SAFE_ACTION_CALLBACK,    /* Call user-provided handler */
} nimcp_safe_action_t;

/* ============================================================================
 * Output Validation Config
 * ============================================================================ */

typedef struct {
    float max_output_magnitude;     /* Max absolute value (default 1.0) */
    float max_output_rate;          /* Max change per second (default 10.0) */
    uint32_t consecutive_nan_limit; /* NaN outputs before trigger (default 3) */
    bool check_nan;                 /* Enable NaN/Inf checking (default true) */
    bool check_magnitude;           /* Enable magnitude checking (default true) */
    bool check_rate;                /* Enable rate-of-change checking (default true) */
} nimcp_output_validation_t;

/* ============================================================================
 * Watchdog Config
 * ============================================================================ */

typedef struct {
    uint32_t timeout_ms;            /* Brain heartbeat timeout (default 500ms) */
    nimcp_safe_action_t action;     /* What to do on trigger */
    nimcp_output_validation_t validation;
    float safe_pose[32];            /* Safe pose joint values (for SAFE_ACTION_POSE) */
    uint32_t safe_pose_dim;
    uint32_t max_outputs;           /* Max actuator outputs (default 32) */
    void (*estop_callback)(void*);  /* User callback for SAFE_ACTION_CALLBACK/LAND */
    void* estop_user_data;
} nimcp_watchdog_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_safety_watchdog nimcp_safety_watchdog_t;

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Create a safety watchdog with the given configuration.
 * @param config Pointer to configuration. NULL uses defaults.
 * @return Watchdog handle, or NULL on allocation failure.
 */
nimcp_safety_watchdog_t* nimcp_watchdog_create(const nimcp_watchdog_config_t* config);

/**
 * @brief Destroy a safety watchdog and free all resources.
 * @param watchdog Handle. NULL-safe.
 */
void nimcp_watchdog_destroy(nimcp_safety_watchdog_t* watchdog);

/**
 * @brief Arm the watchdog. Starts the timer thread.
 * Transitions IDLE → ARMED. No-op if already armed.
 * @return 0 on success, -1 on failure.
 */
int nimcp_watchdog_arm(nimcp_safety_watchdog_t* watchdog);

/**
 * @brief Disarm the watchdog. Stops the timer thread.
 * Transitions any state → IDLE.
 * @return 0 on success, -1 on failure.
 */
int nimcp_watchdog_disarm(nimcp_safety_watchdog_t* watchdog);

/**
 * @brief Signal that the brain completed a successful inference.
 * Must be called within timeout_ms of the previous heartbeat.
 */
void nimcp_watchdog_heartbeat(nimcp_safety_watchdog_t* watchdog);

/**
 * @brief Validate brain output before sending to actuators.
 *
 * Checks for NaN/Inf, magnitude limits, and rate-of-change.
 * If invalid, may transition to TRIGGERED and execute safe action.
 * If valid, saves output as last_valid_output.
 *
 * @param watchdog  Handle.
 * @param output    Array of actuator outputs.
 * @param num_outputs Number of elements in output array.
 * @return 0 if valid, -1 if invalid (safe action triggered or output clamped).
 */
int nimcp_watchdog_validate_output(nimcp_safety_watchdog_t* watchdog,
                                   float* output, uint32_t num_outputs);

/**
 * @brief Get the current safe output values.
 *
 * Depending on config: zeros (STOP), last valid output (HOLD),
 * or configured safe pose (POSE).
 *
 * @param watchdog    Handle.
 * @param output      Buffer to fill with safe values.
 * @param num_outputs Size of output buffer.
 * @return 0 on success, -1 on failure.
 */
int nimcp_watchdog_get_safe_output(nimcp_safety_watchdog_t* watchdog,
                                   float* output, uint32_t num_outputs);

/**
 * @brief Trigger an emergency stop. Requires manual reset.
 * Transitions any state → ESTOP.
 */
void nimcp_watchdog_estop(nimcp_safety_watchdog_t* watchdog);

/**
 * @brief Reset watchdog from TRIGGERED or ESTOP back to IDLE.
 * @return 0 on success, -1 if watchdog is NULL.
 */
int nimcp_watchdog_reset(nimcp_safety_watchdog_t* watchdog);

/**
 * @brief Get the current watchdog state.
 * @return Current state, or NIMCP_WATCHDOG_IDLE if watchdog is NULL.
 */
nimcp_watchdog_state_t nimcp_watchdog_get_state(nimcp_safety_watchdog_t* watchdog);

/**
 * @brief Return a default watchdog configuration.
 */
nimcp_watchdog_config_t nimcp_watchdog_config_default(void);

/**
 * @brief Return a human-readable string for a watchdog state.
 */
const char* nimcp_watchdog_state_name(nimcp_watchdog_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SAFETY_WATCHDOG_H */

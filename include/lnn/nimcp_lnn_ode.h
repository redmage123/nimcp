/**
 * @file nimcp_lnn_ode.h
 * @brief ODE solver API for LNN continuous-time dynamics
 *
 * WHAT: Numerical integration methods for LTC neuron dynamics
 * WHY:  LNN neurons evolve via ODEs requiring numerical integration
 * HOW:  Multiple solver methods (Euler, Heun, RK4) with configurable timestep
 */

#ifndef NIMCP_LNN_ODE_H
#define NIMCP_LNN_ODE_H

#include "nimcp_lnn_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ODE derivative function signature
 *
 * WHAT: Function pointer for computing dx/dt
 * WHY:  ODE solvers need derivative evaluation
 * HOW:  User provides function computing dynamics
 *
 * @param t Current time
 * @param x Current state
 * @param params Additional parameters (neuron-specific)
 * @return dx/dt derivative value
 */
typedef float (*lnn_ode_derivative_fn)(float t, float x, void* params);

/**
 * @brief Single Euler step: x_new = x + dt * f(t, x)
 *
 * WHAT: First-order explicit Euler integration
 * WHY:  Fast, simple, but less accurate
 * HOW:  Single derivative evaluation per step
 *
 * @param t Current time
 * @param x Current state
 * @param dt Time step
 * @param derivative Function computing dx/dt
 * @param params Parameters for derivative function
 * @return x(t + dt) new state
 */
float lnn_ode_step_euler(float t, float x, float dt,
                         lnn_ode_derivative_fn derivative,
                         void* params);

/**
 * @brief Single Heun (RK2) step
 *
 * WHAT: Second-order Runge-Kutta (predictor-corrector)
 * WHY:  Better accuracy than Euler with moderate cost
 * HOW:  Two derivative evaluations per step
 *
 * @param t Current time
 * @param x Current state
 * @param dt Time step
 * @param derivative Function computing dx/dt
 * @param params Parameters for derivative function
 * @return x(t + dt) new state
 */
float lnn_ode_step_heun(float t, float x, float dt,
                        lnn_ode_derivative_fn derivative,
                        void* params);

/**
 * @brief Single RK4 step
 *
 * WHAT: Fourth-order Runge-Kutta integration
 * WHY:  High accuracy, standard for smooth systems
 * HOW:  Four derivative evaluations with weighted average
 *
 * @param t Current time
 * @param x Current state
 * @param dt Time step
 * @param derivative Function computing dx/dt
 * @param params Parameters for derivative function
 * @return x(t + dt) new state
 */
float lnn_ode_step_rk4(float t, float x, float dt,
                       lnn_ode_derivative_fn derivative,
                       void* params);

/**
 * @brief Unified ODE step using specified method
 *
 * WHAT: Single integration step with method selection
 * WHY:  Allows runtime method switching
 * HOW:  Dispatch to appropriate solver
 *
 * @param method ODE solver method to use
 * @param t Current time
 * @param x Current state
 * @param dt Time step
 * @param derivative Function computing dx/dt
 * @param params Parameters for derivative function
 * @return x(t + dt) new state
 */
float lnn_ode_step(lnn_ode_method_t method,
                   float t, float x, float dt,
                   lnn_ode_derivative_fn derivative,
                   void* params);

/**
 * @brief Tensor-based ODE integration step
 *
 * WHAT: Apply Euler integration to entire tensor
 * WHY:  Layer-level integration needs vectorized operations
 * HOW:  x_new = x + dt * dx_dt (simple Euler for now)
 *
 * @param x Current state tensor [n_neurons]
 * @param dx_dt Derivative tensor [n_neurons]
 * @param dt Time step
 * @param method ODE solver method (currently uses Euler for tensors)
 * @return New state tensor (caller must free)
 */
nimcp_tensor_t* lnn_ode_step_tensor(nimcp_tensor_t* x, nimcp_tensor_t* dx_dt,
                                     float dt, lnn_ode_method_t method);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_ODE_H */

/**
 * @file nimcp_lnn_ode.c
 * @brief Implementation of ODE solvers for LNN continuous-time dynamics
 *
 * WHAT: Numerical integration methods for ordinary differential equations
 * WHY:  LTC neurons evolve via dx/dt = f(t, x), requiring numerical integration
 * HOW:  Euler, Heun (RK2), and RK4 methods with varying accuracy/speed tradeoffs
 */

#include "lnn/nimcp_lnn_ode.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_ode)

/*=============================================================================
 * ODE Solver Implementations
 *===========================================================================*/

float lnn_ode_step_euler(float t, float x, float dt,
                         lnn_ode_derivative_fn derivative,
                         void* params) {
    /* Guard: validate inputs */
    if (!derivative) {
        NIMCP_LOGGING_ERROR("Derivative function is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null derivative function in lnn_ode_step_euler");
        return x;
    }
    if (dt <= 0.0f) {
        NIMCP_LOGGING_ERROR("Time step must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid time step %f in lnn_ode_step_euler", dt);
        return x;
    }

    /* Euler method: x_new = x + dt * f(t, x)
     *
     * WHAT: First-order explicit integration
     * WHY:  Simplest method, O(dt) local error
     * HOW:  Single derivative evaluation at current point
     */
    float dx_dt = derivative(t, x, params);
    return x + dt * dx_dt;
}

float lnn_ode_step_heun(float t, float x, float dt,
                        lnn_ode_derivative_fn derivative,
                        void* params) {
    /* Guard: validate inputs */
    if (!derivative) {
        NIMCP_LOGGING_ERROR("Derivative function is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null derivative function in lnn_ode_step_heun");
        return x;
    }
    if (dt <= 0.0f) {
        NIMCP_LOGGING_ERROR("Time step must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid time step %f in lnn_ode_step_heun", dt);
        return x;
    }

    /* Heun's method (RK2): Predictor-corrector
     *
     * WHAT: Second-order Runge-Kutta method
     * WHY:  Better accuracy than Euler with O(dt²) local error
     * HOW:  1. Predict with Euler step
     *       2. Evaluate derivative at predicted point
     *       3. Average two derivatives for correction
     *
     * BIOLOGICAL: Neurons integrate signals over time, averaging
     * past and future information for smoother dynamics.
     */

    /* Predictor: k1 = f(t, x) */
    float k1 = derivative(t, x, params);
    float x_pred = x + dt * k1;

    /* Corrector: k2 = f(t + dt, x_pred) */
    float k2 = derivative(t + dt, x_pred, params);

    /* Heun's update: average of k1 and k2 */
    return x + 0.5f * dt * (k1 + k2);
}

float lnn_ode_step_rk4(float t, float x, float dt,
                       lnn_ode_derivative_fn derivative,
                       void* params) {
    /* Guard: validate inputs */
    if (!derivative) {
        NIMCP_LOGGING_ERROR("Derivative function is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null derivative function in lnn_ode_step_rk4");
        return x;
    }
    if (dt <= 0.0f) {
        NIMCP_LOGGING_ERROR("Time step must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid time step %f in lnn_ode_step_rk4", dt);
        return x;
    }

    /* Runge-Kutta 4th order (RK4)
     *
     * WHAT: Fourth-order explicit integration
     * WHY:  Gold standard for smooth ODEs, O(dt⁴) local error
     * HOW:  Four derivative evaluations with weighted averaging
     *
     * FORMULA:
     * k1 = f(t, x)
     * k2 = f(t + dt/2, x + k1*dt/2)
     * k3 = f(t + dt/2, x + k2*dt/2)
     * k4 = f(t + dt, x + k3*dt)
     * x_new = x + (k1 + 2*k2 + 2*k3 + k4) * dt / 6
     *
     * BIOLOGICAL: High-order integration captures smooth membrane
     * potential dynamics with high accuracy, similar to how
     * biological neurons filter and integrate synaptic inputs.
     */

    /* k1: derivative at current point */
    float k1 = derivative(t, x, params);

    /* k2: derivative at midpoint using k1 */
    float x2 = x + 0.5f * dt * k1;
    float k2 = derivative(t + 0.5f * dt, x2, params);

    /* k3: derivative at midpoint using k2 */
    float x3 = x + 0.5f * dt * k2;
    float k3 = derivative(t + 0.5f * dt, x3, params);

    /* k4: derivative at endpoint using k3 */
    float x4 = x + dt * k3;
    float k4 = derivative(t + dt, x4, params);

    /* Weighted average: (k1 + 2*k2 + 2*k3 + k4) / 6 */
    return x + (dt / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);
}

float lnn_ode_step(lnn_ode_method_t method,
                   float t, float x, float dt,
                   lnn_ode_derivative_fn derivative,
                   void* params) {
    /* Guard: validate inputs */
    if (!derivative) {
        NIMCP_LOGGING_ERROR("Derivative function is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null derivative function in lnn_ode_step");
        return x;
    }

    /* Dispatch to appropriate solver */
    switch (method) {
        case LNN_ODE_EULER:
            return lnn_ode_step_euler(t, x, dt, derivative, params);

        case LNN_ODE_HEUN:
            return lnn_ode_step_heun(t, x, dt, derivative, params);

        case LNN_ODE_RK4:
            return lnn_ode_step_rk4(t, x, dt, derivative, params);

        case LNN_ODE_DOPRI5:
        case LNN_ODE_IMPLICIT_EULER:
            /* Not yet implemented, fall back to RK4 */
            NIMCP_LOGGING_WARN("Method not implemented, using RK4");
            return lnn_ode_step_rk4(t, x, dt, derivative, params);

        default:
            NIMCP_LOGGING_ERROR("Unknown ODE method, using RK4");
            return lnn_ode_step_rk4(t, x, dt, derivative, params);
    }
}

/**
 * @brief Tensor-based ODE integration step (simple Euler for tensors)
 *
 * WHAT: Apply Euler integration to entire tensor
 * WHY:  Layer-level integration needs vectorized operations
 * HOW:  x_new = x + dt * dx_dt
 */
nimcp_tensor_t* lnn_ode_step_tensor(nimcp_tensor_t* x, nimcp_tensor_t* dx_dt,
                                     float dt, lnn_ode_method_t method) {
    /* Guard: validate inputs */
    if (!x || !dx_dt) {
        NIMCP_LOGGING_ERROR("NULL tensor input");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "Null tensor in lnn_ode_step_tensor: x=%p, dx_dt=%p",
                             (void*)x, (void*)dx_dt);
        return NULL;
    }

    if (dt <= 0.0f) {
        NIMCP_LOGGING_ERROR("Time step must be positive");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid time step %f in lnn_ode_step_tensor", dt);
        return NULL;
    }

    /* Clone x to create new state tensor */
    nimcp_tensor_t* x_new = nimcp_tensor_clone(x);
    if (!x_new) {
        NIMCP_LOGGING_ERROR("Failed to clone state tensor");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                          "Failed to clone state tensor in lnn_ode_step_tensor");
        return NULL;
    }

    /* Get tensor size */
    uint64_t size = nimcp_tensor_numel(x);
    float* x_data = (float*)nimcp_tensor_data(x_new);
    const float* dx_data = (const float*)nimcp_tensor_data(dx_dt);

    /* Simple Euler integration: x_new = x + dt * dx_dt */
    for (uint64_t i = 0; i < size; i++) {
        x_data[i] = x_data[i] + dt * dx_data[i];
    }

    /* Note: For higher-order methods (RK4, etc.), we would need
     * derivative functions that operate on tensors, which requires
     * more complex layer-level integration. For now, Euler suffices. */
    (void)method;  /* TODO: Use method for higher-order tensor integration */

    return x_new;
}

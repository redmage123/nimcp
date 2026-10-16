/**
 * @file nimcp_integration.h
 * @brief Numerical integration methods for ODEs in NIMCP
 *
 * WHAT: Provides numerical integration methods (Euler, RK4, adaptive) for ODEs
 * WHY:  Neuron dynamics require accurate ODE integration; RK4 provides 10x better accuracy
 * HOW:  Generic interface accepting derivative functions, multiple method implementations
 *
 * DESIGN RATIONALE:
 * - Strategy pattern: Pluggable integration methods
 * - Single Responsibility: Each method in separate function
 * - Generic interface: Works with any ODE system
 * - Zero-copy where possible: Minimize allocations
 *
 * MATHEMATICAL BACKGROUND:
 * ODEs have form: dy/dt = f(y, t, params)
 * Given y(t), compute y(t+dt):
 * - Euler: y_new = y + dt * f(y, t) [1st order: O(dt)]
 * - RK4: 4-stage Runge-Kutta [4th order: O(dt^4)]
 * - Adaptive: Variable timestep based on error estimate
 *
 * PERFORMANCE:
 * - Euler: Fastest (1x), least accurate
 * - RK4: 2-3x slower, 10-1000x more accurate
 * - Adaptive: 1-10x faster for varying dynamics, auto-tuned accuracy
 *
 * USAGE:
 * @code
 *   // Define derivative function
 *   void my_derivatives(const float* state, float t, void* params, float* derivatives) {
 *       // Compute dy/dt = f(y, t)
 *       derivatives[0] = -0.1f * state[0];  // exponential decay
 *   }
 *
 *   // Integrate
 *   float state[1] = {1.0f};  // y(0) = 1
 *   integration_step(INTEGRATION_RK4, my_derivatives, state, 0.0f, 0.01f, 1, NULL);
 *   // state[0] now contains y(0.01)
 * @endcode
 *
 * PHASE: 11 (Mathematical Enhancements - Part A1.1)
 * DEPENDENCIES: None
 * THREAD-SAFETY: Reentrant if derivative_fn is reentrant
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 1.0.0
 */

#ifndef NIMCP_INTEGRATION_H
#define NIMCP_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Integration method selection
 *
 * WHAT: Enum for selecting ODE integration algorithm
 * WHY:  Different methods balance speed vs accuracy
 * HOW:  Pass to integration_step() to select method
 */
typedef enum {
    INTEGRATION_EULER,      /**< Explicit Euler (1st order, fast, inaccurate) */
    INTEGRATION_RK4,        /**< Runge-Kutta 4th order (4th order, accurate) */
    INTEGRATION_ADAPTIVE,   /**< Adaptive RK45 (variable timestep, future) */
    INTEGRATION_IMPLICIT    /**< Implicit methods for stiff systems (future) */
} integration_method_t;

/**
 * @brief Derivative function pointer type
 *
 * WHAT: Function signature for computing dy/dt = f(y, t, params)
 * WHY:  Generic interface for any ODE system
 * HOW:  User implements this for their specific ODE
 *
 * @param state Current state vector y [n]
 * @param t Current time
 * @param params User-defined parameters (can be NULL)
 * @param derivatives Output: dy/dt [n] (must be pre-allocated)
 *
 * COMPLEXITY: User-defined (typically O(n) or O(n^2) for coupled systems)
 * THREAD-SAFE: Should be reentrant if used with parallel integration
 *
 * EXAMPLE:
 * @code
 *   // Harmonic oscillator: d²x/dt² = -ω²x
 *   // Rewrite as: dx/dt = v, dv/dt = -ω²x
 *   void harmonic_derivatives(const float* state, float t, void* params, float* derivatives) {
 *       float omega = *(float*)params;
 *       float x = state[0];
 *       float v = state[1];
 *       derivatives[0] = v;              // dx/dt = v
 *       derivatives[1] = -omega*omega*x; // dv/dt = -ω²x
 *   }
 * @endcode
 */
typedef void (*derivative_fn_t)(const float* state, float t, void* params, float* derivatives);

/**
 * @brief Configuration for adaptive integration (future)
 *
 * WHAT: Parameters controlling adaptive timestep integration
 * WHY:  Allows automatic accuracy control
 * STATUS: Reserved for future implementation (A1.2)
 */
typedef struct {
    float min_timestep;      /**< Minimum allowed timestep (e.g., 0.001 ms) */
    float max_timestep;      /**< Maximum allowed timestep (e.g., 10.0 ms) */
    float error_tolerance;   /**< Target local error (e.g., 1e-6) */
    uint32_t max_steps;      /**< Safety limit on steps */
} adaptive_config_t;

//=============================================================================
// Core Integration Functions
//=============================================================================

/**
 * @brief Perform single integration step
 *
 * WHAT: Advances ODE system by one timestep using specified method
 * WHY:  Core numerical integration primitive for all NIMCP dynamics
 * HOW:  Calls method-specific implementation (Euler, RK4, etc.)
 *
 * @param method Integration method to use
 * @param derivative_fn Function computing dy/dt = f(y, t)
 * @param state State vector [n] (modified in-place)
 * @param t Current time
 * @param dt Timestep (must be > 0)
 * @param n State dimension (must be > 0)
 * @param params User parameters passed to derivative_fn (can be NULL)
 * @return true on success, false on error
 *
 * COMPLEXITY:
 * - Euler: O(n) + cost of derivative_fn
 * - RK4: O(4n) + 4 * cost of derivative_fn
 *
 * MEMORY: O(n) temporary storage (k1-k4 for RK4)
 * THREAD-SAFE: Yes (if derivative_fn is reentrant)
 *
 * ERROR HANDLING:
 * - Returns false if method invalid, dt <= 0, n == 0, or state/derivative_fn NULL
 * - Derivative function errors propagate as NaN/inf in state
 *
 * EXAMPLE:
 * @code
 *   float y = 1.0f;
 *   float omega = 1.0f;
 *   bool success = integration_step(INTEGRATION_RK4, my_derivative, &y, 0.0f, 0.01f, 1, &omega);
 *   if (!success) {
 *       // Handle error
 *   }
 * @endcode
 */
bool integration_step(
    integration_method_t method,
    derivative_fn_t derivative_fn,
    float* state,
    float t,
    float dt,
    uint32_t n,
    void* params
);

/**
 * @brief Integrate over time interval [t_start, t_end]
 *
 * WHAT: Performs multiple integration steps from t_start to t_end
 * WHY:  Convenience function for batch integration
 * HOW:  Repeatedly calls integration_step() with fixed dt
 *
 * @param method Integration method
 * @param derivative_fn Derivative function
 * @param state Initial state [n] (modified to final state)
 * @param t_start Start time
 * @param t_end End time (must be >= t_start)
 * @param dt Timestep (must be > 0)
 * @param n State dimension
 * @param params User parameters
 * @param trajectory Optional: output trajectory [num_steps][n] (can be NULL)
 * @param num_steps Optional: output number of steps taken (can be NULL)
 * @return true on success, false on error
 *
 * COMPLEXITY: O((t_end - t_start) / dt) calls to integration_step
 * MEMORY: O(1) if trajectory==NULL, else O(num_steps * n)
 *
 * NOTE: If (t_end - t_start) not evenly divisible by dt, final step is shortened
 *
 * EXAMPLE:
 * @code
 *   float state[2] = {1.0f, 0.0f};  // Initial condition
 *   uint32_t steps;
 *   bool success = integration_integrate(
 *       INTEGRATION_RK4, harmonic_derivatives,
 *       state, 0.0f, 10.0f, 0.01f, 2, &omega, NULL, &steps
 *   );
 *   printf("Integrated %u steps to t=10.0\n", steps);
 * @endcode
 */
bool integration_integrate(
    integration_method_t method,
    derivative_fn_t derivative_fn,
    float* state,
    float t_start,
    float t_end,
    float dt,
    uint32_t n,
    void* params,
    float** trajectory,
    uint32_t* num_steps
);

//=============================================================================
// Method-Specific Functions (mostly internal)
//=============================================================================

/**
 * @brief Explicit Euler integration step
 *
 * WHAT: y_new = y + dt * f(y, t)
 * WHY:  Simplest method, fast but inaccurate
 * HOW:  One derivative evaluation, one vector add
 *
 * @param derivative_fn Derivative function
 * @param state State [n] (modified in-place)
 * @param t Current time
 * @param dt Timestep
 * @param n Dimension
 * @param params User parameters
 * @return true on success
 *
 * COMPLEXITY: O(n) + 1 derivative call
 * ACCURACY: O(dt) local error, O(dt^0) = O(1) global error (accumulates!)
 * STABILITY: Conditionally stable (dt < stability limit)
 */
bool integration_euler_step(
    derivative_fn_t derivative_fn,
    float* state,
    float t,
    float dt,
    uint32_t n,
    void* params
);

/**
 * @brief Runge-Kutta 4th order integration step
 *
 * WHAT: Classic RK4 method with 4 stages
 * WHY:  4th order accuracy (error ∝ dt^4), good stability
 * HOW:  4 derivative evaluations, weighted combination
 *
 * ALGORITHM:
 *   k1 = f(y, t)
 *   k2 = f(y + dt*k1/2, t + dt/2)
 *   k3 = f(y + dt*k2/2, t + dt/2)
 *   k4 = f(y + dt*k3, t + dt)
 *   y_new = y + dt*(k1 + 2*k2 + 2*k3 + k4)/6
 *
 * @param derivative_fn Derivative function
 * @param state State [n] (modified in-place)
 * @param t Current time
 * @param dt Timestep
 * @param n Dimension
 * @param params User parameters
 * @return true on success
 *
 * COMPLEXITY: O(4n) + 4 derivative calls
 * ACCURACY: O(dt^4) local error, O(dt^4) global error (excellent!)
 * STABILITY: Good (larger stability region than Euler)
 * MEMORY: O(4n) temporary storage for k1-k4
 */
bool integration_rk4_step(
    derivative_fn_t derivative_fn,
    float* state,
    float t,
    float dt,
    uint32_t n,
    void* params
);

/**
 * @brief Adaptive RK45 (Dormand-Prince) integration step
 *
 * WHAT: 5th-order Runge-Kutta with 4th-order error estimate and adaptive timestep
 * WHY:  Automatic accuracy control, 3-10x faster for slowly-varying dynamics
 * HOW:  Uses 6 derivative evaluations, adapts dt based on local error estimate
 *
 * ALGORITHM: Dormand-Prince 1980
 *   - Computes 5th-order solution (main result)
 *   - Computes 4th-order solution (error estimate)
 *   - Adjusts timestep: dt_new = dt * (tol/error)^(1/5)
 *   - Rejects and retries step if error > tolerance
 *
 * @param derivative_fn Derivative function
 * @param state State [n] (modified in-place with accepted step)
 * @param t Pointer to current time (modified to t + actual_dt_taken)
 * @param dt Pointer to timestep (modified to suggested dt for next step)
 * @param n Dimension
 * @param params User parameters
 * @param config Adaptive configuration (NULL for defaults)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(6n) + 6 derivative calls per accepted step
 * ACCURACY: O(dt^5) local error, O(dt^4) global error
 * MEMORY: O(7n) temporary storage
 * THREAD-SAFE: Yes (if derivative_fn is reentrant)
 *
 * CONFIGURATION:
 * If config is NULL, uses defaults:
 *   - min_timestep = 1e-6
 *   - max_timestep = 1.0
 *   - error_tolerance = 1e-6
 *   - max_steps = 10000
 *
 * EXAMPLE:
 * @code
 *   float y = 1.0f;
 *   float t = 0.0f;
 *   float dt = 0.1f;
 *   adaptive_config_t config = {
 *       .min_timestep = 0.001f,
 *       .max_timestep = 1.0f,
 *       .error_tolerance = 1e-6f,
 *       .max_steps = 10000
 *   };
 *
 *   while (t < 10.0f) {
 *       bool success = integration_adaptive_step(my_derivative, &y, &t, &dt, 1, NULL, &config);
 *       if (!success) break;
 *       // dt is automatically adjusted for next step
 *   }
 * @endcode
 */
bool integration_adaptive_step(
    derivative_fn_t derivative_fn,
    float* state,
    float* t,
    float* dt,
    uint32_t n,
    void* params,
    const adaptive_config_t* config
);

/**
 * @brief Integrate with adaptive timestep over interval [t_start, t_end]
 *
 * WHAT: Performs adaptive integration with automatic error control
 * WHY:  More efficient than fixed-step for varying dynamics (3-10x speedup)
 * HOW:  Repeatedly calls integration_adaptive_step(), timestep adapts automatically
 *
 * @param derivative_fn Derivative function
 * @param state Initial state [n] (modified to final state)
 * @param t_start Start time
 * @param t_end End time
 * @param dt_initial Initial timestep guess
 * @param n State dimension
 * @param params User parameters
 * @param config Adaptive configuration (NULL for defaults)
 * @param num_steps_taken Optional: output number of steps taken (can be NULL)
 * @return true on success, false on error
 *
 * COMPLEXITY: Varies - typically 30-70% fewer steps than fixed RK4
 * PERFORMANCE: ~3-10x faster than fixed-step for stiff/slow dynamics
 *
 * EXAMPLE:
 * @code
 *   float state[2] = {1.0f, 0.0f};
 *   uint32_t steps;
 *   adaptive_config_t config = {
 *       .min_timestep = 0.0001f,
 *       .max_timestep = 0.1f,
 *       .error_tolerance = 1e-6f,
 *       .max_steps = 100000
 *   };
 *   bool success = integration_integrate_adaptive(
 *       stiff_derivative, state, 0.0f, 100.0f, 0.01f,
 *       2, NULL, &config, &steps
 *   );
 *   printf("Completed in %u adaptive steps\n", steps);
 * @endcode
 */
bool integration_integrate_adaptive(
    derivative_fn_t derivative_fn,
    float* state,
    float t_start,
    float t_end,
    float dt_initial,
    uint32_t n,
    void* params,
    const adaptive_config_t* config,
    uint32_t* num_steps_taken
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get human-readable name of integration method
 *
 * @param method Integration method
 * @return Method name string (static, do not free)
 */
const char* integration_method_name(integration_method_t method);

/**
 * @brief Get computational cost multiplier relative to Euler
 *
 * @param method Integration method
 * @return Cost multiplier (1.0 = same as Euler, 4.0 = 4x slower)
 */
float integration_method_cost(integration_method_t method);

/**
 * @brief Get accuracy order of integration method
 *
 * @param method Integration method
 * @return Accuracy order (1 = O(dt), 4 = O(dt^4))
 */
uint32_t integration_method_order(integration_method_t method);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_INTEGRATION_H

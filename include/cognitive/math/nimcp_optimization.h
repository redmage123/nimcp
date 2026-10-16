/**
 * @file nimcp_optimization.h
 * @brief Mathematical optimization engine for NIMCP cognitive mathematics
 *
 * Unconstrained: gradient descent (fixed/line-search/Armijo), Newton's method
 * with Hessian, conjugate gradient (Fletcher-Reeves), BFGS quasi-Newton,
 * Nelder-Mead simplex. Constrained: projected gradient descent, Lagrange
 * multiplier computation, penalty method, augmented Lagrangian. Convexity
 * checks, convergence criteria.
 */

#ifndef NIMCP_OPTIMIZATION_H
#define NIMCP_OPTIMIZATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

#define OPT_MAX_DIM             64      /* maximum problem dimension          */
#define OPT_MAX_ITER            10000   /* default maximum iterations         */
#define OPT_DEFAULT_TOL         1e-8    /* default gradient norm tolerance    */
#define OPT_DEFAULT_FTOL        1e-12   /* default function value tolerance   */
#define OPT_DEFAULT_LR          0.01    /* default learning rate              */
#define OPT_ARMIJO_C1           1e-4    /* Armijo sufficient decrease param   */
#define OPT_ARMIJO_BETA         0.5     /* Armijo backtracking factor         */
#define OPT_BFGS_EPSILON        1e-10   /* BFGS curvature condition threshold */
#define OPT_SIMPLEX_ALPHA       1.0     /* Nelder-Mead reflection coeff      */
#define OPT_SIMPLEX_GAMMA       2.0     /* Nelder-Mead expansion coeff       */
#define OPT_SIMPLEX_RHO         0.5     /* Nelder-Mead contraction coeff     */
#define OPT_SIMPLEX_SIGMA       0.5     /* Nelder-Mead shrink coeff          */
#define OPT_PENALTY_INITIAL     1.0     /* initial penalty parameter          */
#define OPT_PENALTY_GROWTH      10.0    /* penalty growth factor              */

/* --------------------------------------------------------------------------
 * Enums
 * -------------------------------------------------------------------------- */

typedef enum {
    OPT_GRADIENT_DESCENT = 0,
    OPT_GRADIENT_DESCENT_LINE_SEARCH,
    OPT_GRADIENT_DESCENT_ARMIJO,
    OPT_NEWTON,
    OPT_CONJUGATE_GRADIENT,
    OPT_BFGS,
    OPT_NELDER_MEAD,
    OPT_PROJECTED_GRADIENT,
    OPT_PENALTY_METHOD,
    OPT_AUGMENTED_LAGRANGIAN,
    OPT_METHOD_COUNT
} opt_method_t;

typedef enum {
    OPT_NOT_CONVERGED = 0,
    OPT_CONVERGED_GRADIENT,
    OPT_CONVERGED_FVAL,
    OPT_CONVERGED_STEP,
    OPT_MAX_ITER_REACHED,
    OPT_ERROR
} opt_status_t;

/* --------------------------------------------------------------------------
 * Function types
 * -------------------------------------------------------------------------- */

/** Objective function: f(x) -> scalar */
typedef double (*opt_func_t)(const double *x, uint32_t n, void *params);

/** Gradient function: nabla f(x) -> grad[] */
typedef void (*opt_grad_t)(const double *x, uint32_t n, double *grad,
                           void *params);

/** Hessian function: H_ij(x) -> hess[n*n] row-major */
typedef void (*opt_hess_t)(const double *x, uint32_t n, double *hess,
                           void *params);

/** Constraint function: g(x) -> scalar (g(x) <= 0 for inequality) */
typedef double (*opt_constraint_t)(const double *x, uint32_t n, void *params);

/** Projection onto feasible set: project x in-place */
typedef void (*opt_project_t)(double *x, uint32_t n, void *params);

/* --------------------------------------------------------------------------
 * Structures
 * -------------------------------------------------------------------------- */

/** Convergence configuration */
typedef struct {
    double   grad_tol;          /* gradient norm tolerance               */
    double   ftol;              /* function value change tolerance       */
    double   step_tol;          /* step size tolerance                   */
    uint32_t max_iter;          /* maximum iterations                    */
    double   learning_rate;     /* step size for gradient methods        */
} opt_convergence_t;

/** Box constraints: lower[i] <= x[i] <= upper[i] */
typedef struct {
    double lower[OPT_MAX_DIM];
    double upper[OPT_MAX_DIM];
    bool   has_lower[OPT_MAX_DIM];
    bool   has_upper[OPT_MAX_DIM];
} opt_box_constraints_t;

/** Optimization result */
typedef struct {
    double       x[OPT_MAX_DIM];     /* solution point                  */
    double       fval;                /* objective at solution           */
    double       grad_norm;           /* final gradient norm             */
    uint32_t     iterations;          /* iterations used                 */
    uint32_t     func_evals;          /* function evaluations            */
    opt_status_t status;              /* convergence status              */
} opt_result_t;

/** Top-level optimizer */
typedef struct {
    opt_method_t       method;
    uint32_t           dim;           /* problem dimension               */
    opt_convergence_t  conv;          /* convergence settings            */

    /* objective + derivatives */
    opt_func_t         func;
    opt_grad_t         grad;
    opt_hess_t         hess;          /* required for Newton only        */
    void              *params;        /* user data for func/grad/hess    */

    /* constrained optimization */
    opt_constraint_t  *constraints;   /* array of inequality constraints */
    uint32_t           n_constraints;
    opt_project_t      project;       /* projection for projected GD     */
    void              *project_params;

    /* BFGS inverse Hessian approximation H_k (n x n, row-major) */
    double            *bfgs_H;

    /* internal workspace */
    double            *work;          /* scratch buffer                  */
    uint32_t           work_size;     /* bytes allocated                 */
} optimizer_t;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

optimizer_t *optim_create(opt_method_t method, uint32_t dim);
void         optim_destroy(optimizer_t *opt);

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

void opt_set_objective(optimizer_t *opt, opt_func_t func, opt_grad_t grad,
                       opt_hess_t hess, void *params);
void opt_set_convergence(optimizer_t *opt, double grad_tol, double ftol,
                         uint32_t max_iter, double learning_rate);
void opt_set_constraints(optimizer_t *opt, opt_constraint_t *constraints,
                         uint32_t n_constraints);
void opt_set_projection(optimizer_t *opt, opt_project_t project, void *params);

/* --------------------------------------------------------------------------
 * Unconstrained solvers
 * -------------------------------------------------------------------------- */

opt_result_t opt_gradient_descent(optimizer_t *opt, const double *x0);
opt_result_t opt_gradient_descent_armijo(optimizer_t *opt, const double *x0);
opt_result_t opt_newton(optimizer_t *opt, const double *x0);
opt_result_t opt_conjugate_gradient(optimizer_t *opt, const double *x0);
opt_result_t opt_bfgs(optimizer_t *opt, const double *x0);
opt_result_t opt_nelder_mead(optimizer_t *opt, const double *x0);

/* --------------------------------------------------------------------------
 * Constrained solvers
 * -------------------------------------------------------------------------- */

opt_result_t opt_projected_gradient(optimizer_t *opt, const double *x0);
opt_result_t opt_penalty_method(optimizer_t *opt, const double *x0);
opt_result_t opt_augmented_lagrangian(optimizer_t *opt, const double *x0);

/** Compute Lagrange multipliers at point x for active constraints */
bool opt_lagrange_multipliers(optimizer_t *opt, const double *x,
                              double *lambda_out, uint32_t *n_active);

/* --------------------------------------------------------------------------
 * Dispatch
 * -------------------------------------------------------------------------- */

/** Run the configured method */
opt_result_t opt_minimize(optimizer_t *opt, const double *x0);

/* --------------------------------------------------------------------------
 * Utility
 * -------------------------------------------------------------------------- */

/** Check if Hessian at x is positive semi-definite (convexity check) */
bool opt_is_convex_at(optimizer_t *opt, const double *x);

/** Compute gradient norm */
double opt_gradient_norm(const double *grad, uint32_t n);

/** Numerical gradient via central differences (for verification) */
void opt_numerical_gradient(opt_func_t func, const double *x, uint32_t n,
                            void *params, double h, double *grad_out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OPTIMIZATION_H */

/**
 * @file nimcp_optimization.c
 * @brief Mathematical optimization engine implementation
 *
 * Gradient descent (fixed/Armijo), Newton, conjugate gradient, BFGS,
 * Nelder-Mead, projected gradient, penalty, augmented Lagrangian.
 */

#include "cognitive/math/nimcp_optimization.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "optimization"

/* ========================================================================== */
/* Internal helpers                                                           */
/* ========================================================================== */

static void vec_copy(double *dst, const double *src, uint32_t n) {
    memcpy(dst, src, n * sizeof(double));
}

static double vec_dot(const double *a, const double *b, uint32_t n) {
    double s = 0.0;
    for (uint32_t i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

static void vec_axpy(double *y, double a, const double *x, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) y[i] += a * x[i];
}

static void vec_scale(double *x, double a, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) x[i] *= a;
}

static void mat_vec(const double *A, const double *x, double *out,
                    uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        out[i] = 0.0;
        for (uint32_t j = 0; j < n; j++) {
            out[i] += A[i * n + j] * x[j];
        }
    }
}

/** Solve Ax = b via Cholesky or LU (simple Gaussian elimination with pivoting) */
static bool solve_linear(const double *A, const double *b, double *x,
                         uint32_t n) {
    double M[OPT_MAX_DIM * OPT_MAX_DIM];
    double rhs[OPT_MAX_DIM];
    memcpy(M, A, n * n * sizeof(double));
    memcpy(rhs, b, n * sizeof(double));

    /* Gaussian elimination with partial pivoting */
    for (uint32_t k = 0; k < n; k++) {
        /* find pivot */
        uint32_t pivot = k;
        double max_val = fabs(M[k * n + k]);
        for (uint32_t i = k + 1; i < n; i++) {
            double v = fabs(M[i * n + k]);
            if (v > max_val) { max_val = v; pivot = i; }
        }
        if (max_val < 1e-14) return false; /* singular */

        /* swap rows */
        if (pivot != k) {
            for (uint32_t j = 0; j < n; j++) {
                double tmp = M[k * n + j];
                M[k * n + j] = M[pivot * n + j];
                M[pivot * n + j] = tmp;
            }
            double tmp = rhs[k]; rhs[k] = rhs[pivot]; rhs[pivot] = tmp;
        }

        /* eliminate below */
        for (uint32_t i = k + 1; i < n; i++) {
            double factor = M[i * n + k] / M[k * n + k];
            for (uint32_t j = k; j < n; j++) {
                M[i * n + j] -= factor * M[k * n + j];
            }
            rhs[i] -= factor * rhs[k];
        }
    }

    /* back substitution */
    for (int32_t i = (int32_t)n - 1; i >= 0; i--) {
        x[i] = rhs[i];
        for (uint32_t j = (uint32_t)i + 1; j < n; j++) {
            x[i] -= M[i * n + j] * x[j];
        }
        x[i] /= M[i * n + i];
    }
    return true;
}

static opt_result_t make_result(const double *x, uint32_t n, double fval,
                                double gnorm, uint32_t iters, uint32_t fevals,
                                opt_status_t status) {
    opt_result_t r;
    memset(&r, 0, sizeof(r));
    if (x) vec_copy(r.x, x, n);
    r.fval = fval;
    r.grad_norm = gnorm;
    r.iterations = iters;
    r.func_evals = fevals;
    r.status = status;
    return r;
}

/* ========================================================================== */
/* Lifecycle                                                                  */
/* ========================================================================== */

optimizer_t *optim_create(opt_method_t method, uint32_t dim) {
    if (dim == 0 || dim > OPT_MAX_DIM) {
        LOG_ERROR(LOG_TAG, "Invalid dimension %u (max %d)", dim, OPT_MAX_DIM);
        return NULL;
    }

    optimizer_t *opt = (optimizer_t *)nimcp_calloc(1, sizeof(optimizer_t));
    if (!opt) return NULL;

    opt->method = method;
    opt->dim = dim;
    opt->conv.grad_tol = OPT_DEFAULT_TOL;
    opt->conv.ftol = OPT_DEFAULT_FTOL;
    opt->conv.step_tol = 1e-12;
    opt->conv.max_iter = OPT_MAX_ITER;
    opt->conv.learning_rate = OPT_DEFAULT_LR;

    /* workspace: enough for several n-vectors + n*n matrix */
    opt->work_size = (6 * dim + dim * dim) * sizeof(double);
    opt->work = (double *)nimcp_calloc(1, opt->work_size);
    if (!opt->work) {
        nimcp_free(opt);
        return NULL;
    }

    /* BFGS needs inverse Hessian approximation */
    if (method == OPT_BFGS) {
        opt->bfgs_H = (double *)nimcp_calloc(dim * dim, sizeof(double));
        if (!opt->bfgs_H) {
            nimcp_free(opt->work);
            nimcp_free(opt);
            return NULL;
        }
        /* initialize to identity */
        for (uint32_t i = 0; i < dim; i++) {
            opt->bfgs_H[i * dim + i] = 1.0;
        }
    }

    return opt;
}

void optim_destroy(optimizer_t *opt) {
    if (!opt) return;
    if (opt->bfgs_H) nimcp_free(opt->bfgs_H);
    if (opt->work) nimcp_free(opt->work);
    nimcp_free(opt);
}

/* ========================================================================== */
/* Configuration                                                              */
/* ========================================================================== */

void opt_set_objective(optimizer_t *opt, opt_func_t func, opt_grad_t grad,
                       opt_hess_t hess, void *params) {
    if (!opt) return;
    opt->func = func;
    opt->grad = grad;
    opt->hess = hess;
    opt->params = params;
}

void opt_set_convergence(optimizer_t *opt, double grad_tol, double ftol,
                         uint32_t max_iter, double learning_rate) {
    if (!opt) return;
    opt->conv.grad_tol = grad_tol;
    opt->conv.ftol = ftol;
    opt->conv.max_iter = max_iter;
    opt->conv.learning_rate = learning_rate;
}

void opt_set_constraints(optimizer_t *opt, opt_constraint_t *constraints,
                         uint32_t n_constraints) {
    if (!opt) return;
    opt->constraints = constraints;
    opt->n_constraints = n_constraints;
}

void opt_set_projection(optimizer_t *opt, opt_project_t project, void *params) {
    if (!opt) return;
    opt->project = project;
    opt->project_params = params;
}

/* ========================================================================== */
/* Utility                                                                    */
/* ========================================================================== */

double opt_gradient_norm(const double *grad, uint32_t n) {
    return sqrt(vec_dot(grad, grad, n));
}

void opt_numerical_gradient(opt_func_t func, const double *x, uint32_t n,
                            void *params, double h, double *grad_out) {
    double xp[OPT_MAX_DIM], xm[OPT_MAX_DIM];
    vec_copy(xp, x, n);
    vec_copy(xm, x, n);
    for (uint32_t i = 0; i < n; i++) {
        xp[i] = x[i] + h;
        xm[i] = x[i] - h;
        grad_out[i] = (func(xp, n, params) - func(xm, n, params)) / (2.0 * h);
        xp[i] = x[i];
        xm[i] = x[i];
    }
}

bool opt_is_convex_at(optimizer_t *opt, const double *x) {
    if (!opt || !opt->hess) return false;
    uint32_t n = opt->dim;
    double H[OPT_MAX_DIM * OPT_MAX_DIM];
    opt->hess(x, n, H, opt->params);

    /* Check positive semi-definiteness via Cholesky attempt */
    double L[OPT_MAX_DIM * OPT_MAX_DIM];
    memset(L, 0, sizeof(L));
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            double sum = H[i * n + j];
            for (uint32_t k = 0; k < j; k++) {
                sum -= L[i * n + k] * L[j * n + k];
            }
            if (i == j) {
                if (sum < -1e-12) return false; /* not PSD */
                L[i * n + j] = sqrt(fmax(sum, 0.0));
            } else {
                L[i * n + j] = (L[j * n + j] > 1e-14)
                    ? sum / L[j * n + j] : 0.0;
            }
        }
    }
    return true;
}

/* ========================================================================== */
/* Gradient Descent (fixed step)                                              */
/* ========================================================================== */

opt_result_t opt_gradient_descent(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    double x[OPT_MAX_DIM], g[OPT_MAX_DIM];
    vec_copy(x, x0, n);

    double lr = opt->conv.learning_rate;
    uint32_t fevals = 0;

    for (uint32_t iter = 0; iter < opt->conv.max_iter; iter++) {
        double fval = opt->func(x, n, opt->params); fevals++;
        opt->grad(x, n, g, opt->params);
        double gnorm = opt_gradient_norm(g, n);

        if (gnorm < opt->conv.grad_tol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_GRADIENT);
        }

        /* x = x - lr * g */
        vec_axpy(x, -lr, g, n);
    }

    double fval = opt->func(x, n, opt->params); fevals++;
    opt->grad(x, n, g, opt->params);
    return make_result(x, n, fval, opt_gradient_norm(g, n),
                       opt->conv.max_iter, fevals, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* Gradient Descent with Armijo line search                                   */
/* ========================================================================== */

opt_result_t opt_gradient_descent_armijo(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    double x[OPT_MAX_DIM], g[OPT_MAX_DIM], xnew[OPT_MAX_DIM];
    vec_copy(x, x0, n);
    uint32_t fevals = 0;
    double prev_fval = INFINITY;

    for (uint32_t iter = 0; iter < opt->conv.max_iter; iter++) {
        double fval = opt->func(x, n, opt->params); fevals++;
        opt->grad(x, n, g, opt->params);
        double gnorm = opt_gradient_norm(g, n);

        if (gnorm < opt->conv.grad_tol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_GRADIENT);
        }
        if (iter > 0 && fabs(prev_fval - fval) < opt->conv.ftol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_FVAL);
        }

        /* Armijo backtracking line search */
        double alpha = 1.0;
        double slope = -vec_dot(g, g, n); /* directional derivative along -g */
        for (uint32_t ls = 0; ls < 50; ls++) {
            vec_copy(xnew, x, n);
            vec_axpy(xnew, -alpha, g, n);
            double fnew = opt->func(xnew, n, opt->params); fevals++;
            if (fnew <= fval + OPT_ARMIJO_C1 * alpha * slope) {
                break;
            }
            alpha *= OPT_ARMIJO_BETA;
        }

        vec_copy(x, x, n);
        vec_axpy(x, -alpha, g, n);
        prev_fval = fval;
    }

    double fval = opt->func(x, n, opt->params); fevals++;
    opt->grad(x, n, g, opt->params);
    return make_result(x, n, fval, opt_gradient_norm(g, n),
                       opt->conv.max_iter, fevals, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* Newton's method                                                            */
/* ========================================================================== */

opt_result_t opt_newton(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !opt->hess || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    double x[OPT_MAX_DIM], g[OPT_MAX_DIM];
    double H[OPT_MAX_DIM * OPT_MAX_DIM], step[OPT_MAX_DIM];
    vec_copy(x, x0, n);
    uint32_t fevals = 0;

    for (uint32_t iter = 0; iter < opt->conv.max_iter; iter++) {
        double fval = opt->func(x, n, opt->params); fevals++;
        opt->grad(x, n, g, opt->params);
        double gnorm = opt_gradient_norm(g, n);

        if (gnorm < opt->conv.grad_tol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_GRADIENT);
        }

        opt->hess(x, n, H, opt->params);

        /* Solve H * step = -g */
        double neg_g[OPT_MAX_DIM];
        for (uint32_t i = 0; i < n; i++) neg_g[i] = -g[i];

        if (!solve_linear(H, neg_g, step, n)) {
            /* Hessian singular — fall back to gradient step */
            vec_copy(step, neg_g, n);
            vec_scale(step, opt->conv.learning_rate, n);
        }

        double step_norm = opt_gradient_norm(step, n);
        if (step_norm < opt->conv.step_tol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_STEP);
        }

        vec_axpy(x, 1.0, step, n);
    }

    double fval = opt->func(x, n, opt->params); fevals++;
    opt->grad(x, n, g, opt->params);
    return make_result(x, n, fval, opt_gradient_norm(g, n),
                       opt->conv.max_iter, fevals, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* Conjugate Gradient (Fletcher-Reeves)                                       */
/* ========================================================================== */

opt_result_t opt_conjugate_gradient(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    double x[OPT_MAX_DIM], g[OPT_MAX_DIM], g_prev[OPT_MAX_DIM];
    double d[OPT_MAX_DIM], xnew[OPT_MAX_DIM];
    vec_copy(x, x0, n);
    uint32_t fevals = 0;

    opt->grad(x, n, g, opt->params);
    for (uint32_t i = 0; i < n; i++) d[i] = -g[i]; /* initial direction */

    for (uint32_t iter = 0; iter < opt->conv.max_iter; iter++) {
        double fval = opt->func(x, n, opt->params); fevals++;
        double gnorm = opt_gradient_norm(g, n);

        if (gnorm < opt->conv.grad_tol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_GRADIENT);
        }

        /* Armijo line search along d */
        double alpha = 1.0;
        double slope = vec_dot(g, d, n);
        if (slope > 0) { /* reset if not descent */
            for (uint32_t i = 0; i < n; i++) d[i] = -g[i];
            slope = -vec_dot(g, g, n);
        }
        for (uint32_t ls = 0; ls < 50; ls++) {
            vec_copy(xnew, x, n);
            vec_axpy(xnew, alpha, d, n);
            double fnew = opt->func(xnew, n, opt->params); fevals++;
            if (fnew <= fval + OPT_ARMIJO_C1 * alpha * slope) break;
            alpha *= OPT_ARMIJO_BETA;
        }

        vec_axpy(x, alpha, d, n);
        vec_copy(g_prev, g, n);
        opt->grad(x, n, g, opt->params);

        /* Fletcher-Reeves beta */
        double g_dot = vec_dot(g, g, n);
        double gp_dot = vec_dot(g_prev, g_prev, n);
        double beta = (gp_dot > 1e-30) ? g_dot / gp_dot : 0.0;

        /* update direction: d = -g + beta * d */
        for (uint32_t i = 0; i < n; i++) {
            d[i] = -g[i] + beta * d[i];
        }

        /* restart every n iterations */
        if ((iter + 1) % n == 0) {
            for (uint32_t i = 0; i < n; i++) d[i] = -g[i];
        }
    }

    double fval = opt->func(x, n, opt->params); fevals++;
    return make_result(x, n, fval, opt_gradient_norm(g, n),
                       opt->conv.max_iter, fevals, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* BFGS quasi-Newton                                                          */
/* ========================================================================== */

opt_result_t opt_bfgs(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !x0 || !opt->bfgs_H) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    double x[OPT_MAX_DIM], g[OPT_MAX_DIM], g_prev[OPT_MAX_DIM];
    double d[OPT_MAX_DIM], s[OPT_MAX_DIM], y[OPT_MAX_DIM];
    double xnew[OPT_MAX_DIM];
    vec_copy(x, x0, n);
    uint32_t fevals = 0;
    double *H = opt->bfgs_H;

    /* Reset H to identity */
    memset(H, 0, n * n * sizeof(double));
    for (uint32_t i = 0; i < n; i++) H[i * n + i] = 1.0;

    opt->grad(x, n, g, opt->params);

    for (uint32_t iter = 0; iter < opt->conv.max_iter; iter++) {
        double fval = opt->func(x, n, opt->params); fevals++;
        double gnorm = opt_gradient_norm(g, n);

        if (gnorm < opt->conv.grad_tol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_GRADIENT);
        }

        /* d = -H * g */
        mat_vec(H, g, d, n);
        vec_scale(d, -1.0, n);

        /* Armijo line search */
        double alpha = 1.0;
        double slope = vec_dot(g, d, n);
        if (slope > 0) { /* H not PD, reset to identity */
            memset(H, 0, n * n * sizeof(double));
            for (uint32_t i = 0; i < n; i++) H[i * n + i] = 1.0;
            for (uint32_t i = 0; i < n; i++) d[i] = -g[i];
            slope = -vec_dot(g, g, n);
        }
        for (uint32_t ls = 0; ls < 50; ls++) {
            vec_copy(xnew, x, n);
            vec_axpy(xnew, alpha, d, n);
            double fnew = opt->func(xnew, n, opt->params); fevals++;
            if (fnew <= fval + OPT_ARMIJO_C1 * alpha * slope) break;
            alpha *= OPT_ARMIJO_BETA;
        }

        /* s = alpha * d, update x */
        vec_copy(g_prev, g, n);
        for (uint32_t i = 0; i < n; i++) s[i] = alpha * d[i];
        vec_axpy(x, 1.0, s, n);
        opt->grad(x, n, g, opt->params);

        /* y = g - g_prev */
        for (uint32_t i = 0; i < n; i++) y[i] = g[i] - g_prev[i];

        /* BFGS update: H <- (I - rho*s*y') * H * (I - rho*y*s') + rho*s*s' */
        double sy = vec_dot(s, y, n);
        if (sy < OPT_BFGS_EPSILON) continue; /* skip update if curvature bad */
        double rho = 1.0 / sy;

        /* Hy = H * y */
        double Hy[OPT_MAX_DIM];
        mat_vec(H, y, Hy, n);

        /* H = H - rho*(s*Hy' + Hy*s') + rho*(rho*y'Hy + 1)*s*s' */
        double yHy = vec_dot(y, Hy, n);
        double factor = rho * rho * yHy + rho;

        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = 0; j < n; j++) {
                H[i * n + j] += factor * s[i] * s[j]
                    - rho * (s[i] * Hy[j] + Hy[i] * s[j]);
            }
        }
    }

    double fval = opt->func(x, n, opt->params); fevals++;
    return make_result(x, n, fval, opt_gradient_norm(g, n),
                       opt->conv.max_iter, fevals, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* Nelder-Mead simplex (derivative-free)                                      */
/* ========================================================================== */

opt_result_t opt_nelder_mead(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    uint32_t nv = n + 1; /* number of simplex vertices */
    double simplex[OPT_MAX_DIM + 1][OPT_MAX_DIM];
    double fvals[OPT_MAX_DIM + 1];
    double centroid[OPT_MAX_DIM], xr[OPT_MAX_DIM], xe[OPT_MAX_DIM];
    double xc[OPT_MAX_DIM];
    uint32_t fevals = 0;

    /* Initialize simplex: x0 and x0 + delta*e_i */
    vec_copy(simplex[0], x0, n);
    for (uint32_t i = 0; i < n; i++) {
        vec_copy(simplex[i + 1], x0, n);
        double delta = (fabs(x0[i]) > 1e-8) ? 0.05 * x0[i] : 0.00025;
        simplex[i + 1][i] += delta;
    }

    for (uint32_t i = 0; i < nv; i++) {
        fvals[i] = opt->func(simplex[i], n, opt->params); fevals++;
    }

    for (uint32_t iter = 0; iter < opt->conv.max_iter; iter++) {
        /* Sort: find best, worst, second-worst */
        uint32_t best = 0, worst = 0, sec_worst = 0;
        for (uint32_t i = 1; i < nv; i++) {
            if (fvals[i] < fvals[best]) best = i;
            if (fvals[i] > fvals[worst]) worst = i;
        }
        for (uint32_t i = 0; i < nv; i++) {
            if (i == worst) continue;
            if (sec_worst == worst || fvals[i] > fvals[sec_worst]) sec_worst = i;
        }

        /* Check convergence: range of function values */
        double frange = fvals[worst] - fvals[best];
        if (frange < opt->conv.ftol) {
            return make_result(simplex[best], n, fvals[best], frange, iter,
                               fevals, OPT_CONVERGED_FVAL);
        }

        /* Centroid of all points except worst */
        memset(centroid, 0, n * sizeof(double));
        for (uint32_t i = 0; i < nv; i++) {
            if (i == worst) continue;
            vec_axpy(centroid, 1.0, simplex[i], n);
        }
        vec_scale(centroid, 1.0 / (double)n, n);

        /* Reflection: xr = centroid + alpha*(centroid - worst) */
        for (uint32_t i = 0; i < n; i++) {
            xr[i] = centroid[i] + OPT_SIMPLEX_ALPHA *
                     (centroid[i] - simplex[worst][i]);
        }
        double fr = opt->func(xr, n, opt->params); fevals++;

        if (fr < fvals[best]) {
            /* Expansion: xe = centroid + gamma*(xr - centroid) */
            for (uint32_t i = 0; i < n; i++) {
                xe[i] = centroid[i] + OPT_SIMPLEX_GAMMA *
                         (xr[i] - centroid[i]);
            }
            double fe = opt->func(xe, n, opt->params); fevals++;
            if (fe < fr) {
                vec_copy(simplex[worst], xe, n); fvals[worst] = fe;
            } else {
                vec_copy(simplex[worst], xr, n); fvals[worst] = fr;
            }
        } else if (fr < fvals[sec_worst]) {
            vec_copy(simplex[worst], xr, n); fvals[worst] = fr;
        } else {
            /* Contraction */
            const double *xh = (fr < fvals[worst]) ? xr : simplex[worst];
            double fh = (fr < fvals[worst]) ? fr : fvals[worst];
            for (uint32_t i = 0; i < n; i++) {
                xc[i] = centroid[i] + OPT_SIMPLEX_RHO * (xh[i] - centroid[i]);
            }
            double fc = opt->func(xc, n, opt->params); fevals++;

            if (fc < fh) {
                vec_copy(simplex[worst], xc, n); fvals[worst] = fc;
            } else {
                /* Shrink towards best */
                for (uint32_t i = 0; i < nv; i++) {
                    if (i == best) continue;
                    for (uint32_t j = 0; j < n; j++) {
                        simplex[i][j] = simplex[best][j] +
                            OPT_SIMPLEX_SIGMA * (simplex[i][j] - simplex[best][j]);
                    }
                    fvals[i] = opt->func(simplex[i], n, opt->params); fevals++;
                }
            }
        }
    }

    uint32_t best = 0;
    for (uint32_t i = 1; i < nv; i++) {
        if (fvals[i] < fvals[best]) best = i;
    }
    return make_result(simplex[best], n, fvals[best], 0.0,
                       opt->conv.max_iter, fevals, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* Projected gradient descent                                                 */
/* ========================================================================== */

opt_result_t opt_projected_gradient(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !opt->project || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    double x[OPT_MAX_DIM], g[OPT_MAX_DIM];
    vec_copy(x, x0, n);
    opt->project(x, n, opt->project_params);
    double lr = opt->conv.learning_rate;
    uint32_t fevals = 0;

    for (uint32_t iter = 0; iter < opt->conv.max_iter; iter++) {
        double fval = opt->func(x, n, opt->params); fevals++;
        opt->grad(x, n, g, opt->params);
        double gnorm = opt_gradient_norm(g, n);

        if (gnorm < opt->conv.grad_tol) {
            return make_result(x, n, fval, gnorm, iter, fevals,
                               OPT_CONVERGED_GRADIENT);
        }

        vec_axpy(x, -lr, g, n);
        opt->project(x, n, opt->project_params);
    }

    double fval = opt->func(x, n, opt->params); fevals++;
    opt->grad(x, n, g, opt->params);
    return make_result(x, n, fval, opt_gradient_norm(g, n),
                       opt->conv.max_iter, fevals, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* Penalty method                                                             */
/* ========================================================================== */

typedef struct {
    opt_func_t        orig_func;
    opt_constraint_t *constraints;
    uint32_t          n_constraints;
    double            penalty;
    void             *orig_params;
} penalty_ctx_t;

static double penalty_objective(const double *x, uint32_t n, void *ctx) {
    penalty_ctx_t *pc = (penalty_ctx_t *)ctx;
    double f = pc->orig_func(x, n, pc->orig_params);
    for (uint32_t i = 0; i < pc->n_constraints; i++) {
        double g = pc->constraints[i](x, n, pc->orig_params);
        if (g > 0.0) { /* violated */
            f += pc->penalty * g * g;
        }
    }
    return f;
}

opt_result_t opt_penalty_method(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !opt->constraints || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    double x[OPT_MAX_DIM];
    vec_copy(x, x0, n);

    penalty_ctx_t pc;
    pc.orig_func = opt->func;
    pc.constraints = opt->constraints;
    pc.n_constraints = opt->n_constraints;
    pc.penalty = OPT_PENALTY_INITIAL;
    pc.orig_params = opt->params;

    opt_func_t saved_func = opt->func;
    void *saved_params = opt->params;
    opt->params = &pc;

    opt_result_t result;
    result.status = OPT_NOT_CONVERGED;

    for (uint32_t outer = 0; outer < 20; outer++) {
        /* Temporarily replace objective with penalized version */
        opt->func = penalty_objective;
        result = opt_gradient_descent_armijo(opt, x);
        vec_copy(x, result.x, n);

        /* Check constraint satisfaction */
        bool feasible = true;
        for (uint32_t i = 0; i < opt->n_constraints; i++) {
            double g = pc.constraints[i](x, n, saved_params);
            if (g > 1e-6) { feasible = false; break; }
        }
        if (feasible) break;
        pc.penalty *= OPT_PENALTY_GROWTH;
    }

    opt->func = saved_func;
    opt->params = saved_params;
    result.fval = opt->func(result.x, n, opt->params);
    return result;
}

/* ========================================================================== */
/* Augmented Lagrangian                                                       */
/* ========================================================================== */

opt_result_t opt_augmented_lagrangian(optimizer_t *opt, const double *x0) {
    if (!opt || !opt->func || !opt->grad || !opt->constraints || !x0) {
        return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
    uint32_t n = opt->dim;
    uint32_t nc = opt->n_constraints;
    double x[OPT_MAX_DIM];
    double lambda[OPT_MAX_DIM]; /* Lagrange multiplier estimates */
    vec_copy(x, x0, n);
    memset(lambda, 0, sizeof(lambda));
    double mu = OPT_PENALTY_INITIAL;

    opt_result_t result;
    result.status = OPT_NOT_CONVERGED;

    for (uint32_t outer = 0; outer < 30; outer++) {
        /* Inner solve: minimize L_A(x, lambda, mu) via gradient descent */
        for (uint32_t inner = 0; inner < 500; inner++) {
            double fval = opt->func(x, n, opt->params);
            double g[OPT_MAX_DIM];
            opt->grad(x, n, g, opt->params);

            /* Add constraint gradient contributions */
            for (uint32_t c = 0; c < nc; c++) {
                double cv = opt->constraints[c](x, n, opt->params);
                double aug = lambda[c] + mu * cv;
                if (aug > 0.0 || cv > 0.0) {
                    /* Numerical constraint gradient */
                    double xp[OPT_MAX_DIM];
                    for (uint32_t i = 0; i < n; i++) {
                        vec_copy(xp, x, n);
                        xp[i] += 1e-7;
                        double cvp = opt->constraints[c](xp, n, opt->params);
                        g[i] += (lambda[c] + mu * fmax(cv, 0.0)) *
                                (cvp - cv) / 1e-7;
                    }
                }
                (void)fval;
            }

            double gnorm = opt_gradient_norm(g, n);
            if (gnorm < opt->conv.grad_tol * 10.0) break;
            vec_axpy(x, -opt->conv.learning_rate, g, n);
        }

        /* Update multipliers: lambda_c = max(0, lambda_c + mu * g_c(x)) */
        bool feasible = true;
        for (uint32_t c = 0; c < nc; c++) {
            double cv = opt->constraints[c](x, n, opt->params);
            lambda[c] = fmax(0.0, lambda[c] + mu * cv);
            if (cv > 1e-6) feasible = false;
        }

        if (feasible) {
            result = make_result(x, n, opt->func(x, n, opt->params), 0.0,
                                 outer, 0, OPT_CONVERGED_GRADIENT);
            return result;
        }

        mu *= 2.0;
    }

    return make_result(x, n, opt->func(x, n, opt->params), 0.0,
                       30, 0, OPT_MAX_ITER_REACHED);
}

/* ========================================================================== */
/* Lagrange multipliers                                                       */
/* ========================================================================== */

bool opt_lagrange_multipliers(optimizer_t *opt, const double *x,
                              double *lambda_out, uint32_t *n_active) {
    if (!opt || !opt->func || !opt->grad || !opt->constraints || !x) {
        return false;
    }
    uint32_t n = opt->dim;
    double g[OPT_MAX_DIM];
    opt->grad(x, n, g, opt->params);

    /* Find active constraints (g_c(x) near zero) */
    uint32_t active[OPT_MAX_DIM];
    uint32_t na = 0;
    for (uint32_t c = 0; c < opt->n_constraints; c++) {
        double cv = opt->constraints[c](x, n, opt->params);
        if (fabs(cv) < 1e-6) {
            active[na++] = c;
        }
    }
    if (n_active) *n_active = na;
    if (na == 0) return true;

    /* Build constraint Jacobian A (na x n) and solve A' * lambda = -grad */
    double A[OPT_MAX_DIM * OPT_MAX_DIM];
    memset(A, 0, sizeof(A));
    for (uint32_t a = 0; a < na; a++) {
        double xp[OPT_MAX_DIM];
        for (uint32_t i = 0; i < n; i++) {
            vec_copy(xp, x, n);
            xp[i] += 1e-7;
            double cv0 = opt->constraints[active[a]](x, n, opt->params);
            double cv1 = opt->constraints[active[a]](xp, n, opt->params);
            A[a * n + i] = (cv1 - cv0) / 1e-7;
        }
    }

    /* Solve A * A' * lambda = A * (-g) via normal equations */
    double AAt[OPT_MAX_DIM * OPT_MAX_DIM];
    double Ag[OPT_MAX_DIM];
    memset(AAt, 0, sizeof(AAt));
    memset(Ag, 0, sizeof(Ag));
    for (uint32_t i = 0; i < na; i++) {
        for (uint32_t j = 0; j < na; j++) {
            for (uint32_t k = 0; k < n; k++) {
                AAt[i * na + j] += A[i * n + k] * A[j * n + k];
            }
        }
        for (uint32_t k = 0; k < n; k++) {
            Ag[i] -= A[i * n + k] * g[k];
        }
    }

    return solve_linear(AAt, Ag, lambda_out, na);
}

/* ========================================================================== */
/* Dispatch                                                                   */
/* ========================================================================== */

opt_result_t opt_minimize(optimizer_t *opt, const double *x0) {
    if (!opt) return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);

    switch (opt->method) {
        case OPT_GRADIENT_DESCENT:             return opt_gradient_descent(opt, x0);
        case OPT_GRADIENT_DESCENT_LINE_SEARCH: return opt_gradient_descent_armijo(opt, x0);
        case OPT_GRADIENT_DESCENT_ARMIJO:      return opt_gradient_descent_armijo(opt, x0);
        case OPT_NEWTON:                       return opt_newton(opt, x0);
        case OPT_CONJUGATE_GRADIENT:           return opt_conjugate_gradient(opt, x0);
        case OPT_BFGS:                         return opt_bfgs(opt, x0);
        case OPT_NELDER_MEAD:                  return opt_nelder_mead(opt, x0);
        case OPT_PROJECTED_GRADIENT:           return opt_projected_gradient(opt, x0);
        case OPT_PENALTY_METHOD:               return opt_penalty_method(opt, x0);
        case OPT_AUGMENTED_LAGRANGIAN:         return opt_augmented_lagrangian(opt, x0);
        default:
            return make_result(NULL, 0, INFINITY, INFINITY, 0, 0, OPT_ERROR);
    }
}

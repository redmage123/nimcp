/**
 * @file test_optimization.cpp
 * @brief Tests for the mathematical optimization engine (Google Test)
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/math/nimcp_optimization.h"
}

static const double TOL = 1e-4;

/* --- objective functions --- */

/* f(x) = x^2, minimum at x=0 */
static double f_quadratic(const double *x, uint32_t n, void *p) {
    (void)n; (void)p;
    return x[0] * x[0];
}
static void g_quadratic(const double *x, uint32_t n, double *grad, void *p) {
    (void)n; (void)p;
    grad[0] = 2.0 * x[0];
}

/* f(x) = x^4, minimum at x=0 */
static double f_quartic(const double *x, uint32_t n, void *p) {
    (void)n; (void)p;
    double v = x[0];
    return v * v * v * v;
}
static void g_quartic(const double *x, uint32_t n, double *grad, void *p) {
    (void)n; (void)p;
    double v = x[0];
    grad[0] = 4.0 * v * v * v;
}
static void h_quartic(const double *x, uint32_t n, double *hess, void *p) {
    (void)n; (void)p;
    double v = x[0];
    hess[0] = 12.0 * v * v;
}

/* Rosenbrock: f(x,y) = (1-x)^2 + 100*(y-x^2)^2, minimum at (1,1) */
static double f_rosenbrock(const double *x, uint32_t n, void *p) {
    (void)n; (void)p;
    double a = 1.0 - x[0];
    double b = x[1] - x[0] * x[0];
    return a * a + 100.0 * b * b;
}
static void g_rosenbrock(const double *x, uint32_t n, double *grad, void *p) {
    (void)n; (void)p;
    double a = 1.0 - x[0];
    double b = x[1] - x[0] * x[0];
    grad[0] = -2.0 * a - 400.0 * x[0] * b;
    grad[1] = 200.0 * b;
}

/* ---------- lifecycle ---------- */

TEST(OptimizationTest, CreateDestroy) {
    optimizer_t *opt = optim_create(OPT_GRADIENT_DESCENT, 2);
    ASSERT_NE(opt, nullptr);
    optim_destroy(opt);
}

/* ---------- gradient descent on x^2 ---------- */

TEST(OptimizationTest, GdQuadratic) {
    optimizer_t *opt = optim_create(OPT_GRADIENT_DESCENT, 1);
    ASSERT_NE(opt, nullptr);
    opt_set_objective(opt, f_quadratic, g_quadratic, NULL, NULL);
    opt_set_convergence(opt, 1e-10, 1e-12, 10000, 0.1);

    double x0[] = {5.0};
    opt_result_t res = opt_gradient_descent(opt, x0);

    EXPECT_NEAR(res.x[0], 0.0, TOL);
    EXPECT_NEAR(res.fval, 0.0, TOL);
    EXPECT_NE(res.status, OPT_ERROR);

    optim_destroy(opt);
}

/* ---------- Armijo line search ---------- */

TEST(OptimizationTest, GdArmijoQuadratic) {
    optimizer_t *opt = optim_create(OPT_GRADIENT_DESCENT_ARMIJO, 1);
    ASSERT_NE(opt, nullptr);
    opt_set_objective(opt, f_quadratic, g_quadratic, NULL, NULL);
    opt_set_convergence(opt, 1e-10, 1e-12, 10000, 1.0);

    double x0[] = {10.0};
    opt_result_t res = opt_gradient_descent_armijo(opt, x0);

    EXPECT_NEAR(res.x[0], 0.0, TOL);

    optim_destroy(opt);
}

/* ---------- Newton on x^4 ---------- */

TEST(OptimizationTest, NewtonQuartic) {
    optimizer_t *opt = optim_create(OPT_NEWTON, 1);
    ASSERT_NE(opt, nullptr);
    opt_set_objective(opt, f_quartic, g_quartic, h_quartic, NULL);
    opt_set_convergence(opt, 1e-10, 1e-12, 10000, 1.0);

    double x0[] = {3.0};
    opt_result_t res = opt_newton(opt, x0);

    EXPECT_NEAR(res.x[0], 0.0, TOL);
    EXPECT_NEAR(res.fval, 0.0, TOL);

    optim_destroy(opt);
}

/* ---------- Nelder-Mead on Rosenbrock ---------- */

TEST(OptimizationTest, NelderMeadRosenbrock) {
    optimizer_t *opt = optim_create(OPT_NELDER_MEAD, 2);
    ASSERT_NE(opt, nullptr);
    opt_set_objective(opt, f_rosenbrock, NULL, NULL, NULL);
    opt_set_convergence(opt, 1e-8, 1e-12, 50000, 0.0);

    double x0[] = {-1.0, 1.0};
    opt_result_t res = opt_nelder_mead(opt, x0);

    /* Rosenbrock minimum at (1,1) with f=0 */
    EXPECT_NEAR(res.x[0], 1.0, 0.05);
    EXPECT_NEAR(res.x[1], 1.0, 0.05);
    EXPECT_LT(res.fval, 0.01);

    optim_destroy(opt);
}

/* ---------- BFGS on Rosenbrock ---------- */

TEST(OptimizationTest, BfgsRosenbrock) {
    optimizer_t *opt = optim_create(OPT_BFGS, 2);
    ASSERT_NE(opt, nullptr);
    opt_set_objective(opt, f_rosenbrock, g_rosenbrock, NULL, NULL);
    opt_set_convergence(opt, 1e-8, 1e-12, 10000, 1.0);

    double x0[] = {-1.0, 1.0};
    opt_result_t res = opt_bfgs(opt, x0);

    EXPECT_NEAR(res.x[0], 1.0, 0.01);
    EXPECT_NEAR(res.x[1], 1.0, 0.01);
    EXPECT_LT(res.fval, 1e-6);

    optim_destroy(opt);
}

/* ---------- conjugate gradient ---------- */

TEST(OptimizationTest, ConjugateGradientQuadratic) {
    optimizer_t *opt = optim_create(OPT_CONJUGATE_GRADIENT, 1);
    ASSERT_NE(opt, nullptr);
    opt_set_objective(opt, f_quadratic, g_quadratic, NULL, NULL);
    opt_set_convergence(opt, 1e-10, 1e-12, 10000, 0.1);

    double x0[] = {7.0};
    opt_result_t res = opt_conjugate_gradient(opt, x0);

    EXPECT_NEAR(res.x[0], 0.0, TOL);

    optim_destroy(opt);
}

/* ---------- numerical gradient verification ---------- */

TEST(OptimizationTest, NumericalGradient) {
    double x[] = {3.0};
    double grad_num[1];
    opt_numerical_gradient(f_quadratic, x, 1, NULL, 1e-6, grad_num);
    /* df/dx of x^2 at x=3 is 6.0 */
    EXPECT_NEAR(grad_num[0], 6.0, 1e-4);
}

/* ---------- gradient norm ---------- */

TEST(OptimizationTest, GradientNormComputation) {
    double grad[] = {3.0, 4.0};
    double norm = opt_gradient_norm(grad, 2);
    EXPECT_NEAR(norm, 5.0, 1e-9);
}

/* ---------- dispatch via opt_minimize ---------- */

TEST(OptimizationTest, MinimizeDispatch) {
    optimizer_t *opt = optim_create(OPT_GRADIENT_DESCENT, 1);
    ASSERT_NE(opt, nullptr);
    opt_set_objective(opt, f_quadratic, g_quadratic, NULL, NULL);
    opt_set_convergence(opt, 1e-10, 1e-12, 10000, 0.1);

    double x0[] = {5.0};
    opt_result_t res = opt_minimize(opt, x0);

    EXPECT_NEAR(res.x[0], 0.0, TOL);

    optim_destroy(opt);
}

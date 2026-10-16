/**
 * @file nimcp_numerical_methods.h
 * @brief Numerical analysis engine
 *
 * Root finding (bisection, Newton, secant, Brent), interpolation
 * (Lagrange, Newton, cubic spline), quadrature (trapezoidal, Simpson,
 * Gauss-Legendre, Romberg), FFT (Cooley-Tukey radix-2), numerical
 * differentiation, iterative linear solvers (Jacobi, Gauss-Seidel).
 */

#ifndef NIMCP_NUMERICAL_METHODS_H
#define NIMCP_NUMERICAL_METHODS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- constants ---------- */
#define NUM_MAX_ITERATIONS    1000
#define NUM_DEFAULT_TOL       1e-10
#define NUM_MAX_SPLINE_POINTS 512
#define NUM_MAX_FFT_SIZE      4096
#define NUM_MAX_ROMBERG_DEPTH 10
#define NUM_MAX_SYSTEM_DIM    64
#define NUM_GAUSS_MAX_POINTS  5

/* ---------- function type ---------- */
typedef double (*math_func_t)(double x, void* params);

/* ---------- complex number ---------- */
typedef struct complex_d_s {
    double re;
    double im;
} complex_d_t;

/* ---------- root-finding results ---------- */
typedef struct root_result_s {
    double root;
    int    iterations;
    double residual;
    bool   converged;
} root_result_t;

/* ---------- cubic spline ---------- */
typedef struct cubic_spline_s {
    double* x;
    double* a;  /* spline coefficients */
    double* b;
    double* c;
    double* d;
    int     n;
} cubic_spline_t;

/* ---------- FFT result ---------- */
typedef struct fft_result_s {
    complex_d_t* data;
    int           n;
} fft_result_t;

/* ---------- Romberg table ---------- */
typedef struct romberg_result_s {
    double table[NUM_MAX_ROMBERG_DEPTH][NUM_MAX_ROMBERG_DEPTH];
    double result;
    int    depth;
} romberg_result_t;

/* ---------- iterative solver result ---------- */
typedef struct iter_solve_result_s {
    double* x;
    int     n;
    int     iterations;
    double  residual;
    bool    converged;
} iter_solve_result_t;

/* ---------- lifecycle ---------- */
/* No persistent state needed; functions are stateless or take explicit params */

/* ---------- root finding ---------- */
root_result_t num_bisection(math_func_t f, void* params, double a, double b,
                            double tol, int max_iter);
root_result_t num_newton(math_func_t f, math_func_t df, void* params,
                         double x0, double tol, int max_iter);
root_result_t num_secant(math_func_t f, void* params,
                         double x0, double x1, double tol, int max_iter);
root_result_t num_brent(math_func_t f, void* params,
                        double a, double b, double tol, int max_iter);

/* ---------- interpolation ---------- */
double num_lagrange_interp(const double* x, const double* y, int n, double xi);
double num_newton_divided_diff(const double* x, const double* y, int n, double xi);

cubic_spline_t* num_cubic_spline_create(const double* x, const double* y, int n);
double          num_cubic_spline_eval(const cubic_spline_t* sp, double xi);
void            num_cubic_spline_free(cubic_spline_t* sp);

/* ---------- quadrature ---------- */
double num_trapezoidal(math_func_t f, void* params, double a, double b, int n);
double num_simpson(math_func_t f, void* params, double a, double b, int n);
double num_gauss_legendre(math_func_t f, void* params, double a, double b, int n_points);
romberg_result_t num_romberg(math_func_t f, void* params, double a, double b,
                             int max_depth, double tol);

/* ---------- FFT ---------- */
fft_result_t* num_fft(const complex_d_t* input, int n);
fft_result_t* num_ifft(const complex_d_t* input, int n);
void          num_fft_free(fft_result_t* r);

/* ---------- numerical differentiation ---------- */
double num_deriv_3point(math_func_t f, void* params, double x, double h);
double num_deriv_5point(math_func_t f, void* params, double x, double h);

/* ---------- iterative linear solvers ---------- */
/** Solve Ax=b by Jacobi iteration. A is n x n row-major. */
iter_solve_result_t* num_jacobi(const double* A, const double* b, int n,
                                double tol, int max_iter);

/** Solve Ax=b by Gauss-Seidel iteration. */
iter_solve_result_t* num_gauss_seidel(const double* A, const double* b, int n,
                                      double tol, int max_iter);

void num_iter_solve_free(iter_solve_result_t* r);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NUMERICAL_METHODS_H */

/**
 * @file nimcp_numerical_methods.c
 * @brief Numerical analysis engine implementation
 */

#include "cognitive/math/nimcp_numerical_methods.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "NUMERICAL"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================
 * ROOT FINDING
 * ================================================================ */

root_result_t num_bisection(math_func_t f, void* params, double a, double b,
                            double tol, int max_iter) {
    root_result_t r;
    memset(&r, 0, sizeof(r));
    if (!f) return r;
    if (tol <= 0) tol = NUM_DEFAULT_TOL;
    if (max_iter <= 0) max_iter = NUM_MAX_ITERATIONS;

    double fa = f(a, params), fb = f(b, params);
    if (fa * fb > 0.0) {
        LOG_WARN(LOG_TAG, "Bisection: f(a) and f(b) have same sign");
        return r;
    }

    for (int i = 0; i < max_iter; i++) {
        double mid = (a + b) / 2.0;
        double fm = f(mid, params);
        r.iterations = i + 1;
        r.root = mid;
        r.residual = fabs(fm);

        if (fabs(fm) < tol || (b - a) / 2.0 < tol) {
            r.converged = true;
            return r;
        }
        if (fa * fm < 0.0) { b = mid; fb = fm; }
        else { a = mid; fa = fm; }
    }
    return r;
}

root_result_t num_newton(math_func_t f, math_func_t df, void* params,
                         double x0, double tol, int max_iter) {
    root_result_t r;
    memset(&r, 0, sizeof(r));
    if (!f || !df) return r;
    if (tol <= 0) tol = NUM_DEFAULT_TOL;
    if (max_iter <= 0) max_iter = NUM_MAX_ITERATIONS;

    double x = x0;
    for (int i = 0; i < max_iter; i++) {
        double fx = f(x, params);
        double dfx = df(x, params);
        r.iterations = i + 1;
        r.root = x;
        r.residual = fabs(fx);

        if (fabs(fx) < tol) { r.converged = true; return r; }
        if (fabs(dfx) < 1e-15) {
            LOG_WARN(LOG_TAG, "Newton: derivative near zero at x=%.6e", x);
            return r;
        }
        x = x - fx / dfx;
    }
    r.root = x;
    r.residual = fabs(f(x, params));
    return r;
}

root_result_t num_secant(math_func_t f, void* params,
                         double x0, double x1, double tol, int max_iter) {
    root_result_t r;
    memset(&r, 0, sizeof(r));
    if (!f) return r;
    if (tol <= 0) tol = NUM_DEFAULT_TOL;
    if (max_iter <= 0) max_iter = NUM_MAX_ITERATIONS;

    double f0 = f(x0, params), f1 = f(x1, params);
    for (int i = 0; i < max_iter; i++) {
        r.iterations = i + 1;
        if (fabs(f1 - f0) < 1e-15) break;

        double x2 = x1 - f1 * (x1 - x0) / (f1 - f0);
        double f2 = f(x2, params);
        r.root = x2;
        r.residual = fabs(f2);

        if (fabs(f2) < tol) { r.converged = true; return r; }
        x0 = x1; f0 = f1;
        x1 = x2; f1 = f2;
    }
    return r;
}

root_result_t num_brent(math_func_t f, void* params,
                        double a, double b, double tol, int max_iter) {
    root_result_t r;
    memset(&r, 0, sizeof(r));
    if (!f) return r;
    if (tol <= 0) tol = NUM_DEFAULT_TOL;
    if (max_iter <= 0) max_iter = NUM_MAX_ITERATIONS;

    double fa = f(a, params), fb = f(b, params);
    if (fa * fb > 0.0) {
        LOG_WARN(LOG_TAG, "Brent: f(a) and f(b) have same sign");
        return r;
    }

    double c = a, fc = fa, d = b - a, e = d;
    if (fabs(fc) < fabs(fb)) {
        double ta = a, tb = b, tc = c;
        double tfa = fa, tfb = fb, tfc = fc;
        a = tb; b = tc; c = ta;
        fa = tfb; fb = tfc; fc = tfa;
    }

    for (int i = 0; i < max_iter; i++) {
        r.iterations = i + 1;
        double tol1 = 2.0 * 2.22e-16 * fabs(b) + 0.5 * tol;
        double m = 0.5 * (c - b);
        r.root = b;
        r.residual = fabs(fb);

        if (fabs(m) <= tol1 || fabs(fb) < tol) {
            r.converged = true;
            return r;
        }

        if (fabs(e) >= tol1 && fabs(fa) > fabs(fb)) {
            /* Attempt inverse quadratic interpolation */
            double s, p, q;
            if (fabs(a - c) < 1e-15) {
                s = fb / fa;
                p = 2.0 * m * s;
                q = 1.0 - s;
            } else {
                double r_val = fb / fc, s_val = fb / fa, t_val = fa / fc;
                p = s_val * (2.0 * m * r_val * (r_val - t_val) - (b - a) * (r_val - 1.0));
                q = (r_val - 1.0) * (s_val - 1.0) * (t_val - 1.0);
                s = 0; /* unused */
                (void)s;
            }
            if (p > 0.0) q = -q; else p = -p;

            if (2.0 * p < (3.0 * m * q - fabs(tol1 * q)) &&
                2.0 * p < fabs(e * q)) {
                e = d;
                d = p / q;
            } else {
                d = m; e = m;
            }
        } else {
            d = m; e = m;
        }

        a = b; fa = fb;
        if (fabs(d) > tol1) b += d;
        else b += (m > 0.0) ? tol1 : -tol1;
        fb = f(b, params);

        if ((fb > 0.0 && fc > 0.0) || (fb < 0.0 && fc < 0.0)) {
            c = a; fc = fa; d = b - a; e = d;
        }
    }
    return r;
}

/* ================================================================
 * INTERPOLATION
 * ================================================================ */

double num_lagrange_interp(const double* x, const double* y, int n, double xi) {
    if (!x || !y || n <= 0) return 0.0;
    double result = 0.0;
    for (int i = 0; i < n; i++) {
        double li = 1.0;
        for (int j = 0; j < n; j++) {
            if (j != i) {
                double denom = x[i] - x[j];
                if (fabs(denom) < 1e-15) continue;
                li *= (xi - x[j]) / denom;
            }
        }
        result += y[i] * li;
    }
    return result;
}

double num_newton_divided_diff(const double* x, const double* y, int n, double xi) {
    if (!x || !y || n <= 0) return 0.0;

    double* dd = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!dd) return 0.0;
    memcpy(dd, y, (size_t)n * sizeof(double));

    /* Build divided difference table */
    for (int j = 1; j < n; j++) {
        for (int i = n - 1; i >= j; i--) {
            double denom = x[i] - x[i - j];
            if (fabs(denom) < 1e-15) { dd[i] = 0.0; continue; }
            dd[i] = (dd[i] - dd[i - 1]) / denom;
        }
    }

    /* Evaluate via Horner-like scheme */
    double result = dd[n - 1];
    for (int i = n - 2; i >= 0; i--) {
        result = result * (xi - x[i]) + dd[i];
    }
    nimcp_free(dd);
    return result;
}

/* ================================================================
 * CUBIC SPLINE (natural boundary conditions)
 * ================================================================ */

cubic_spline_t* num_cubic_spline_create(const double* x, const double* y, int n) {
    if (!x || !y || n < 2 || n > NUM_MAX_SPLINE_POINTS) return NULL;

    cubic_spline_t* sp = (cubic_spline_t*)nimcp_calloc(1, sizeof(cubic_spline_t));
    if (!sp) return NULL;
    sp->n = n;
    sp->x = (double*)nimcp_calloc((size_t)n, sizeof(double));
    sp->a = (double*)nimcp_calloc((size_t)n, sizeof(double));
    sp->b = (double*)nimcp_calloc((size_t)n, sizeof(double));
    sp->c = (double*)nimcp_calloc((size_t)n, sizeof(double));
    sp->d = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!sp->x || !sp->a || !sp->b || !sp->c || !sp->d) {
        num_cubic_spline_free(sp);
        return NULL;
    }

    memcpy(sp->x, x, (size_t)n * sizeof(double));
    memcpy(sp->a, y, (size_t)n * sizeof(double));

    int nm1 = n - 1;
    double* h = (double*)nimcp_calloc((size_t)nm1, sizeof(double));
    double* alpha = (double*)nimcp_calloc((size_t)n, sizeof(double));
    double* l = (double*)nimcp_calloc((size_t)n, sizeof(double));
    double* mu = (double*)nimcp_calloc((size_t)n, sizeof(double));
    double* z = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!h || !alpha || !l || !mu || !z) {
        nimcp_free(h); nimcp_free(alpha); nimcp_free(l); nimcp_free(mu); nimcp_free(z);
        num_cubic_spline_free(sp);
        return NULL;
    }

    for (int i = 0; i < nm1; i++) h[i] = x[i + 1] - x[i];

    for (int i = 1; i < nm1; i++) {
        alpha[i] = (3.0 / h[i]) * (sp->a[i + 1] - sp->a[i])
                 - (3.0 / h[i - 1]) * (sp->a[i] - sp->a[i - 1]);
    }

    /* Tridiagonal system solve */
    l[0] = 1.0; mu[0] = 0.0; z[0] = 0.0;
    for (int i = 1; i < nm1; i++) {
        l[i] = 2.0 * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1];
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }
    l[nm1] = 1.0; z[nm1] = 0.0; sp->c[nm1] = 0.0;

    for (int j = nm1 - 1; j >= 0; j--) {
        sp->c[j] = z[j] - mu[j] * sp->c[j + 1];
        sp->b[j] = (sp->a[j + 1] - sp->a[j]) / h[j]
                  - h[j] * (sp->c[j + 1] + 2.0 * sp->c[j]) / 3.0;
        sp->d[j] = (sp->c[j + 1] - sp->c[j]) / (3.0 * h[j]);
    }

    nimcp_free(h); nimcp_free(alpha); nimcp_free(l); nimcp_free(mu); nimcp_free(z);
    LOG_DEBUG(LOG_TAG, "Created cubic spline with %d knots", n);
    return sp;
}

double num_cubic_spline_eval(const cubic_spline_t* sp, double xi) {
    if (!sp || sp->n < 2) return 0.0;

    /* Find interval via binary search */
    int lo = 0, hi = sp->n - 1;
    if (xi <= sp->x[0]) lo = 0;
    else if (xi >= sp->x[hi]) lo = hi - 1;
    else {
        while (hi - lo > 1) {
            int mid = (lo + hi) / 2;
            if (sp->x[mid] > xi) hi = mid; else lo = mid;
        }
    }

    double dx = xi - sp->x[lo];
    return sp->a[lo] + sp->b[lo] * dx + sp->c[lo] * dx * dx + sp->d[lo] * dx * dx * dx;
}

void num_cubic_spline_free(cubic_spline_t* sp) {
    if (!sp) return;
    nimcp_free(sp->x);
    nimcp_free(sp->a);
    nimcp_free(sp->b);
    nimcp_free(sp->c);
    nimcp_free(sp->d);
    nimcp_free(sp);
}

/* ================================================================
 * QUADRATURE
 * ================================================================ */

double num_trapezoidal(math_func_t f, void* params, double a, double b, int n) {
    if (!f || n < 1) return 0.0;
    double h = (b - a) / (double)n;
    double sum = 0.5 * (f(a, params) + f(b, params));
    for (int i = 1; i < n; i++) sum += f(a + (double)i * h, params);
    return sum * h;
}

double num_simpson(math_func_t f, void* params, double a, double b, int n) {
    if (!f || n < 2) return 0.0;
    if (n % 2 != 0) n++; /* Simpson requires even n */
    double h = (b - a) / (double)n;
    double sum = f(a, params) + f(b, params);
    for (int i = 1; i < n; i += 2) sum += 4.0 * f(a + (double)i * h, params);
    for (int i = 2; i < n; i += 2) sum += 2.0 * f(a + (double)i * h, params);
    return sum * h / 3.0;
}

double num_gauss_legendre(math_func_t f, void* params, double a, double b, int n_points) {
    if (!f || n_points < 1 || n_points > NUM_GAUSS_MAX_POINTS) return 0.0;

    /* Gauss-Legendre nodes and weights on [-1, 1] */
    static const double nodes2[] = {-0.5773502691896258, 0.5773502691896258};
    static const double wts2[]   = {1.0, 1.0};

    static const double nodes3[] = {-0.7745966692414834, 0.0, 0.7745966692414834};
    static const double wts3[]   = {0.5555555555555556, 0.8888888888888889, 0.5555555555555556};

    static const double nodes5[] = {
        -0.9061798459386640, -0.5384693101056831, 0.0,
         0.5384693101056831,  0.9061798459386640
    };
    static const double wts5[] = {
        0.2369268850561891, 0.4786286704993665, 0.5688888888888889,
        0.4786286704993665, 0.2369268850561891
    };

    const double* nodes;
    const double* wts;
    int np;

    switch (n_points) {
        case 2: nodes = nodes2; wts = wts2; np = 2; break;
        case 3: nodes = nodes3; wts = wts3; np = 3; break;
        case 5: nodes = nodes5; wts = wts5; np = 5; break;
        default: nodes = nodes3; wts = wts3; np = 3; break;
    }

    /* Transform from [-1,1] to [a,b]: x = (b-a)/2 * t + (a+b)/2 */
    double half_len = (b - a) / 2.0;
    double mid = (a + b) / 2.0;
    double sum = 0.0;
    for (int i = 0; i < np; i++) {
        double x = half_len * nodes[i] + mid;
        sum += wts[i] * f(x, params);
    }
    return sum * half_len;
}

romberg_result_t num_romberg(math_func_t f, void* params, double a, double b,
                             int max_depth, double tol) {
    romberg_result_t r;
    memset(&r, 0, sizeof(r));
    if (!f) return r;
    if (max_depth <= 0 || max_depth > NUM_MAX_ROMBERG_DEPTH) max_depth = NUM_MAX_ROMBERG_DEPTH;
    if (tol <= 0) tol = NUM_DEFAULT_TOL;

    /* R[0][0] = trapezoidal with 1 panel */
    r.table[0][0] = (b - a) / 2.0 * (f(a, params) + f(b, params));
    r.depth = 1;

    for (int i = 1; i < max_depth; i++) {
        /* Composite trapezoidal with 2^i panels (add midpoints) */
        int n_new = 1 << (i - 1);
        double h = (b - a) / (double)(1 << i);
        double sum = 0.0;
        for (int j = 0; j < n_new; j++) {
            sum += f(a + (double)(2 * j + 1) * h, params);
        }
        r.table[i][0] = 0.5 * r.table[i - 1][0] + h * sum;

        /* Richardson extrapolation */
        for (int j = 1; j <= i; j++) {
            double factor = pow(4.0, (double)j);
            r.table[i][j] = (factor * r.table[i][j - 1] - r.table[i - 1][j - 1]) / (factor - 1.0);
        }

        r.depth = i + 1;
        r.result = r.table[i][i];

        if (i > 0 && fabs(r.table[i][i] - r.table[i - 1][i - 1]) < tol) {
            LOG_DEBUG(LOG_TAG, "Romberg converged at depth %d: %.12e", i + 1, r.result);
            return r;
        }
    }

    r.result = r.table[r.depth - 1][r.depth - 1];
    return r;
}

/* ================================================================
 * FFT (Cooley-Tukey radix-2 DIT)
 * ================================================================ */

static void fft_core(complex_d_t* data, int n, int inverse) {
    /* Bit-reversal permutation */
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (i < j) {
            complex_d_t tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) { j -= m; m >>= 1; }
        j += m;
    }

    /* Cooley-Tukey butterfly */
    for (int len = 2; len <= n; len <<= 1) {
        double angle = 2.0 * M_PI / (double)len * (inverse ? -1.0 : 1.0);
        complex_d_t wlen;
        wlen.re = cos(angle);
        wlen.im = sin(angle);

        for (int i = 0; i < n; i += len) {
            complex_d_t w = {1.0, 0.0};
            int half = len >> 1;
            for (int k = 0; k < half; k++) {
                /* t = w * data[i + k + half] */
                complex_d_t t;
                t.re = w.re * data[i + k + half].re - w.im * data[i + k + half].im;
                t.im = w.re * data[i + k + half].im + w.im * data[i + k + half].re;

                complex_d_t u = data[i + k];
                data[i + k].re = u.re + t.re;
                data[i + k].im = u.im + t.im;
                data[i + k + half].re = u.re - t.re;
                data[i + k + half].im = u.im - t.im;

                /* w *= wlen */
                double wr = w.re * wlen.re - w.im * wlen.im;
                double wi = w.re * wlen.im + w.im * wlen.re;
                w.re = wr;
                w.im = wi;
            }
        }
    }

    if (inverse) {
        for (int i = 0; i < n; i++) {
            data[i].re /= (double)n;
            data[i].im /= (double)n;
        }
    }
}

static bool is_power_of_two(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

fft_result_t* num_fft(const complex_d_t* input, int n) {
    if (!input || n <= 0 || n > NUM_MAX_FFT_SIZE || !is_power_of_two(n)) {
        LOG_ERROR(LOG_TAG, "FFT: invalid size %d (must be power-of-2, max %d)", n, NUM_MAX_FFT_SIZE);
        return NULL;
    }

    fft_result_t* r = (fft_result_t*)nimcp_calloc(1, sizeof(fft_result_t));
    if (!r) return NULL;
    r->n = n;
    r->data = (complex_d_t*)nimcp_calloc((size_t)n, sizeof(complex_d_t));
    if (!r->data) { nimcp_free(r); return NULL; }

    memcpy(r->data, input, (size_t)n * sizeof(complex_d_t));
    fft_core(r->data, n, 0);
    return r;
}

fft_result_t* num_ifft(const complex_d_t* input, int n) {
    if (!input || n <= 0 || n > NUM_MAX_FFT_SIZE || !is_power_of_two(n)) return NULL;

    fft_result_t* r = (fft_result_t*)nimcp_calloc(1, sizeof(fft_result_t));
    if (!r) return NULL;
    r->n = n;
    r->data = (complex_d_t*)nimcp_calloc((size_t)n, sizeof(complex_d_t));
    if (!r->data) { nimcp_free(r); return NULL; }

    memcpy(r->data, input, (size_t)n * sizeof(complex_d_t));
    fft_core(r->data, n, 1);
    return r;
}

void num_fft_free(fft_result_t* r) {
    if (!r) return;
    nimcp_free(r->data);
    nimcp_free(r);
}

/* ================================================================
 * NUMERICAL DIFFERENTIATION
 * ================================================================ */

double num_deriv_3point(math_func_t f, void* params, double x, double h) {
    if (!f) return 0.0;
    if (h <= 0) h = 1e-5;
    return (f(x + h, params) - f(x - h, params)) / (2.0 * h);
}

double num_deriv_5point(math_func_t f, void* params, double x, double h) {
    if (!f) return 0.0;
    if (h <= 0) h = 1e-5;
    /* (-f(x+2h) + 8f(x+h) - 8f(x-h) + f(x-2h)) / (12h) */
    return (-f(x + 2.0 * h, params) + 8.0 * f(x + h, params)
            - 8.0 * f(x - h, params) + f(x - 2.0 * h, params)) / (12.0 * h);
}

/* ================================================================
 * ITERATIVE LINEAR SOLVERS
 * ================================================================ */

iter_solve_result_t* num_jacobi(const double* A, const double* b, int n,
                                double tol, int max_iter) {
    if (!A || !b || n <= 0 || n > NUM_MAX_SYSTEM_DIM) return NULL;
    if (tol <= 0) tol = NUM_DEFAULT_TOL;
    if (max_iter <= 0) max_iter = NUM_MAX_ITERATIONS;

    iter_solve_result_t* r = (iter_solve_result_t*)nimcp_calloc(1, sizeof(iter_solve_result_t));
    if (!r) return NULL;
    r->n = n;
    r->x = (double*)nimcp_calloc((size_t)n, sizeof(double));
    double* x_new = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!r->x || !x_new) { nimcp_free(x_new); num_iter_solve_free(r); return NULL; }

    for (int iter = 0; iter < max_iter; iter++) {
        for (int i = 0; i < n; i++) {
            double sigma = 0.0;
            for (int j = 0; j < n; j++) {
                if (j != i) sigma += A[i * n + j] * r->x[j];
            }
            double diag = A[i * n + i];
            if (fabs(diag) < 1e-15) { nimcp_free(x_new); return r; }
            x_new[i] = (b[i] - sigma) / diag;
        }

        /* Check convergence */
        double max_diff = 0.0;
        for (int i = 0; i < n; i++) {
            double diff = fabs(x_new[i] - r->x[i]);
            if (diff > max_diff) max_diff = diff;
        }
        memcpy(r->x, x_new, (size_t)n * sizeof(double));
        r->iterations = iter + 1;
        r->residual = max_diff;

        if (max_diff < tol) {
            r->converged = true;
            break;
        }
    }

    nimcp_free(x_new);
    LOG_DEBUG(LOG_TAG, "Jacobi: %d iter, residual=%.2e, %s",
              r->iterations, r->residual, r->converged ? "converged" : "not converged");
    return r;
}

iter_solve_result_t* num_gauss_seidel(const double* A, const double* b, int n,
                                      double tol, int max_iter) {
    if (!A || !b || n <= 0 || n > NUM_MAX_SYSTEM_DIM) return NULL;
    if (tol <= 0) tol = NUM_DEFAULT_TOL;
    if (max_iter <= 0) max_iter = NUM_MAX_ITERATIONS;

    iter_solve_result_t* r = (iter_solve_result_t*)nimcp_calloc(1, sizeof(iter_solve_result_t));
    if (!r) return NULL;
    r->n = n;
    r->x = (double*)nimcp_calloc((size_t)n, sizeof(double));
    double* x_old = (double*)nimcp_calloc((size_t)n, sizeof(double));
    if (!r->x || !x_old) { nimcp_free(x_old); num_iter_solve_free(r); return NULL; }

    for (int iter = 0; iter < max_iter; iter++) {
        memcpy(x_old, r->x, (size_t)n * sizeof(double));

        for (int i = 0; i < n; i++) {
            double sigma = 0.0;
            for (int j = 0; j < n; j++) {
                if (j != i) sigma += A[i * n + j] * r->x[j]; /* uses updated values */
            }
            double diag = A[i * n + i];
            if (fabs(diag) < 1e-15) { nimcp_free(x_old); return r; }
            r->x[i] = (b[i] - sigma) / diag;
        }

        /* Check convergence */
        double max_diff = 0.0;
        for (int i = 0; i < n; i++) {
            double diff = fabs(r->x[i] - x_old[i]);
            if (diff > max_diff) max_diff = diff;
        }
        r->iterations = iter + 1;
        r->residual = max_diff;

        if (max_diff < tol) {
            r->converged = true;
            break;
        }
    }

    nimcp_free(x_old);
    LOG_DEBUG(LOG_TAG, "Gauss-Seidel: %d iter, residual=%.2e, %s",
              r->iterations, r->residual, r->converged ? "converged" : "not converged");
    return r;
}

void num_iter_solve_free(iter_solve_result_t* r) {
    if (!r) return;
    nimcp_free(r->x);
    nimcp_free(r);
}

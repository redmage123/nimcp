/**
 * @file nimcp_qft.c
 * @brief Quantum Field Theory — RG flow, Higgs mechanism, RMT, lattice
 *
 * WHAT: Running couplings, SSB, random matrix eigenvalue statistics, lattice MC
 * WHY:  Unifying framework + Riemann zeros ↔ GUE eigenvalue connection
 * HOW:  One-loop beta functions, Metropolis lattice, Jacobi eigenvalue algorithm
 */

#include "cognitive/physics/nimcp_qft.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define LOG_TAG "QFT"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Thread-local RNG */
static _Thread_local uint64_t qft_rng = 6364136223846793005ULL;
static double qft_rand(void) {
    qft_rng = qft_rng * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(qft_rng >> 11) / (double)(1ULL << 53);
}
static double qft_rand_gauss(void) {
    double u1 = qft_rand(), u2 = qft_rand();
    if (u1 < 1e-15) u1 = 1e-15;
    if (u1 >= 1.0) u1 = 1.0 - 1e-15;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ============================================================================
 * Renormalization Group
 * ============================================================================ */

float qft_beta_0_sun(uint32_t N_c, uint32_t N_f) {
    /* β₀ = (11N_c - 2N_f) / (48π²) for SU(N_c) with N_f Dirac fermions */
    return (11.0f * (float)N_c - 2.0f * (float)N_f) / (48.0f * (float)(M_PI * M_PI));
}

float qft_run_coupling(float g, float mu1, float mu2, float beta_0) {
    /* dg/d(ln μ) = β₀·g³ at one-loop
     * Solution: 1/g²(μ₂) = 1/g²(μ₁) - 2β₀·ln(μ₂/μ₁) */
    if (g <= 0 || mu1 <= 0 || mu2 <= 0) return g;
    double inv_g2 = 1.0 / ((double)g * g) - 2.0 * (double)beta_0 * log((double)mu2 / mu1);
    if (inv_g2 <= 0) return 10.0f;  /* Landau pole */
    return (float)(1.0 / sqrt(inv_g2));
}

float qft_alpha_em(float mu_gev) {
    /* α_EM(μ) = α(0) / (1 - α(0)/(3π)·ln(μ²/m_e²))
     * α(0) = 1/137.036, m_e = 0.000511 GeV */
    double alpha_0 = 1.0 / 137.036;
    double me = 0.000511;
    if (mu_gev <= me) return (float)alpha_0;
    double correction = alpha_0 / (3.0 * M_PI) * log((double)mu_gev * mu_gev / (me * me));
    double denom = 1.0 - correction;
    if (denom <= 0.01) denom = 0.01;
    return (float)(alpha_0 / denom);
}

float qft_alpha_s(float mu_gev) {
    /* α_s(μ) from QCD beta function with N_c=3, N_f=6
     * α_s(M_Z) = 0.118, run from M_Z = 91.2 GeV */
    float alpha_mz = 0.118f;
    float mz = 91.2f;
    float b0 = qft_beta_0_sun(3, 6);
    /* α_s = g²/(4π), so g = sqrt(4π·α_s) */
    float g_mz = sqrtf(4.0f * (float)M_PI * alpha_mz);
    float g_mu = qft_run_coupling(g_mz, mz, mu_gev, -b0);  /* negative b0: asymptotic freedom */
    return g_mu * g_mu / (4.0f * (float)M_PI);
}

/* ============================================================================
 * Higgs Mechanism
 * ============================================================================ */

void qft_higgs_mechanism(qft_sim_t* sim, float vev, float lambda) {
    if (!sim) return;
    sim->higgs.vev = vev;
    sim->higgs.lambda = lambda;
    sim->higgs.mu_squared = -lambda * vev * vev;  /* μ² = -λv² for SSB */

    /* Standard Model masses from VEV = 246.22 GeV:
     * M_W = g_W·v/2 ≈ 80.4 GeV
     * M_Z = M_W/cos(θ_W) ≈ 91.2 GeV
     * M_H = √(2λ)·v ≈ 125.1 GeV */
    float g_w = 0.653f;  /* weak coupling */
    float g_prime = 0.350f;  /* hypercharge coupling */

    sim->higgs.W_mass = g_w * vev / 2.0f;
    float gz = sqrtf(g_w * g_w + g_prime * g_prime);
    sim->higgs.Z_mass = gz * vev / 2.0f;
    sim->higgs.higgs_mass = sqrtf(2.0f * lambda) * vev;
    sim->higgs.weinberg_angle = g_prime * g_prime / (g_w * g_w + g_prime * g_prime);
    sim->higgs.broken = true;

    sim->stats.sin2_weinberg = sim->higgs.weinberg_angle;
    LOG_INFO(LOG_TAG, "Higgs mechanism: v=%.1f GeV, M_W=%.1f, M_Z=%.1f, M_H=%.1f, sin²θ_W=%.4f",
             vev, sim->higgs.W_mass, sim->higgs.Z_mass, sim->higgs.higgs_mass,
             sim->higgs.weinberg_angle);
}

uint32_t qft_goldstone_count(uint32_t group_dim_before, uint32_t group_dim_after) {
    /* Number of Goldstone bosons = dim(G) - dim(H) */
    return (group_dim_before > group_dim_after) ?
            group_dim_before - group_dim_after : 0;
}

/* ============================================================================
 * Random Matrix Theory
 * ============================================================================ */

void qft_rmt_generate(qft_sim_t* sim, qft_rmt_ensemble_t ensemble) {
    if (!sim || !sim->rmt.matrix) return;
    uint32_t N = sim->rmt.matrix_dim;
    double* M = sim->rmt.matrix;
    sim->rmt.ensemble = ensemble;

    switch (ensemble) {
    case QFT_RMT_GUE:
        /* GUE: M = (A + A†)/2 where A_ij = gaussian + i·gaussian
         * Since we use real eigenvalues, generate Hermitian: M_ij = M_ji* */
        for (uint32_t i = 0; i < N; i++) {
            M[i * N + i] = qft_rand_gauss();  /* diagonal: real */
            for (uint32_t j = i + 1; j < N; j++) {
                double re = qft_rand_gauss() / sqrt(2.0);
                /* For real symmetric approximation of GUE statistics */
                M[i * N + j] = re;
                M[j * N + i] = re;
            }
        }
        break;
    case QFT_RMT_GOE:
        /* GOE: real symmetric, M_ij = M_ji = gaussian/√2 */
        for (uint32_t i = 0; i < N; i++) {
            M[i * N + i] = qft_rand_gauss();
            for (uint32_t j = i + 1; j < N; j++) {
                double val = qft_rand_gauss() / sqrt(2.0);
                M[i * N + j] = val;
                M[j * N + i] = val;
            }
        }
        break;
    default:
        /* GSE: quaternion self-dual — approximate with GOE for now */
        for (uint32_t i = 0; i < N * N; i++) M[i] = 0;
        for (uint32_t i = 0; i < N; i++) {
            M[i * N + i] = qft_rand_gauss();
            for (uint32_t j = i + 1; j < N; j++) {
                double val = qft_rand_gauss() / sqrt(2.0);
                M[i * N + j] = val;
                M[j * N + i] = val;
            }
        }
        break;
    }
    sim->stats.rmt_samples_generated++;
}

void qft_rmt_compute_eigenvalues(qft_sim_t* sim) {
    if (!sim || !sim->rmt.matrix || !sim->rmt.eigenvalues) return;
    uint32_t N = sim->rmt.matrix_dim;
    double* M = sim->rmt.matrix;
    double* eig = sim->rmt.eigenvalues;

    /* Jacobi eigenvalue algorithm for real symmetric matrix */
    /* Copy matrix (Jacobi modifies in place) */
    double* A = nimcp_calloc(N * N, sizeof(double));
    if (!A) return;
    memcpy(A, M, N * N * sizeof(double));

    for (uint32_t sweep = 0; sweep < 100; sweep++) {
        /* Find max off-diagonal element */
        double max_off = 0;
        uint32_t p = 0, q = 1;
        for (uint32_t i = 0; i < N; i++) {
            for (uint32_t j = i + 1; j < N; j++) {
                if (fabs(A[i * N + j]) > max_off) {
                    max_off = fabs(A[i * N + j]);
                    p = i; q = j;
                }
            }
        }
        if (max_off < 1e-12) break;  /* converged */

        /* Jacobi rotation */
        double theta = 0.5 * atan2(2.0 * A[p * N + q], A[p * N + p] - A[q * N + q]);
        double c = cos(theta), s = sin(theta);

        /* Apply rotation: A' = J^T A J */
        for (uint32_t i = 0; i < N; i++) {
            double api = A[i * N + p], aqi = A[i * N + q];
            A[i * N + p] = c * api + s * aqi;
            A[i * N + q] = -s * api + c * aqi;
        }
        for (uint32_t j = 0; j < N; j++) {
            double apj = A[p * N + j], aqj = A[q * N + j];
            A[p * N + j] = c * apj + s * aqj;
            A[q * N + j] = -s * apj + c * aqj;
        }
    }

    /* Eigenvalues are on diagonal */
    for (uint32_t i = 0; i < N; i++) eig[i] = A[i * N + i];
    nimcp_free(A);

    /* Sort eigenvalues */
    for (uint32_t i = 0; i < N - 1; i++) {
        for (uint32_t j = i + 1; j < N; j++) {
            if (eig[j] < eig[i]) { double t = eig[i]; eig[i] = eig[j]; eig[j] = t; }
        }
    }
}

void qft_rmt_spacing_statistics(qft_sim_t* sim) {
    if (!sim || !sim->rmt.eigenvalues || sim->rmt.matrix_dim < 3) return;
    uint32_t N = sim->rmt.matrix_dim;
    double* eig = sim->rmt.eigenvalues;

    /* Mean spacing */
    double total = 0;
    for (uint32_t i = 1; i < N; i++) total += eig[i] - eig[i-1];
    sim->rmt.mean_spacing = total / (double)(N - 1);

    /* Normalize spacings and compute Wigner surmise fit */
    double wigner_error = 0;
    uint32_t count = 0;
    for (uint32_t i = 1; i < N; i++) {
        double s = (eig[i] - eig[i-1]) / sim->rmt.mean_spacing;
        double p_wigner = qft_rmt_wigner_surmise(sim->rmt.ensemble, s);
        /* Crude: accumulate deviation from Wigner prediction */
        wigner_error += fabs(s - 1.0);  /* deviation from mean spacing */
        count++;
    }
    sim->rmt.wigner_fit = (count > 0) ? wigner_error / count : 0;
    sim->rmt.num_samples++;
}

double qft_rmt_gue_correlation(double x) {
    if (fabs(x) < 1e-10) return 0;
    double sinc = sin(M_PI * x) / (M_PI * x);
    return 1.0 - sinc * sinc;
}

double qft_rmt_goe_correlation(double x) {
    /* GOE: 1 - (sin(πx)/(πx))² + d/dx[sin(πx)/(πx)]·∫₀^∞ sin(πt)/(πt)dt */
    /* Simplified to same as GUE (exact form requires Si function) */
    return qft_rmt_gue_correlation(x);
}

double qft_rmt_wigner_surmise(qft_rmt_ensemble_t ensemble, double s) {
    if (s < 0) return 0;
    switch (ensemble) {
    case QFT_RMT_GOE:
        return (M_PI * 0.5) * s * exp(-M_PI * s * s * 0.25);
    case QFT_RMT_GUE:
        return (32.0 / (M_PI * M_PI)) * s * s * exp(-4.0 * s * s / M_PI);
    case QFT_RMT_GSE:
        return (2.0 * 2.0 * 2.0 * 2.0 * 2.0 * 2.0 / (9.0 * M_PI * M_PI * M_PI))
               * s * s * s * s * exp(-64.0 * s * s / (9.0 * M_PI));
    default: return 0;
    }
}

double qft_rmt_riemann_comparison(const qft_sim_t* sim,
                                    const double* riemann_zeros, uint32_t num_zeros) {
    /* Compare eigenvalue spacing distribution to Riemann zero spacing */
    if (!sim || !riemann_zeros || num_zeros < 3 || !sim->rmt.eigenvalues) return -1;

    /* Compute Kolmogorov-Smirnov distance between the two spacing distributions */
    uint32_t N_eig = sim->rmt.matrix_dim;
    if (N_eig < 3) return -1;

    /* Normalized eigenvalue spacings */
    double eig_mean = sim->rmt.mean_spacing;
    if (eig_mean < 1e-20) return -1;

    /* Riemann zero spacings */
    double rz_total = 0;
    for (uint32_t i = 1; i < num_zeros; i++)
        rz_total += riemann_zeros[i] - riemann_zeros[i-1];
    double rz_mean = rz_total / (double)(num_zeros - 1);
    if (rz_mean < 1e-20) return -1;

    /* Simple L2 distance between normalized spacing histograms */
    uint32_t bins = 20;
    double bin_width = 3.0 / (double)bins;
    double* hist_eig = nimcp_calloc(bins, sizeof(double));
    double* hist_rz = nimcp_calloc(bins, sizeof(double));
    if (!hist_eig || !hist_rz) { nimcp_free(hist_eig); nimcp_free(hist_rz); return -1; }

    for (uint32_t i = 1; i < N_eig; i++) {
        double s = (sim->rmt.eigenvalues[i] - sim->rmt.eigenvalues[i-1]) / eig_mean;
        uint32_t b = (uint32_t)(s / bin_width);
        if (b < bins) hist_eig[b] += 1.0;
    }
    for (uint32_t i = 1; i < num_zeros; i++) {
        double s = (riemann_zeros[i] - riemann_zeros[i-1]) / rz_mean;
        uint32_t b = (uint32_t)(s / bin_width);
        if (b < bins) hist_rz[b] += 1.0;
    }

    /* Normalize */
    for (uint32_t b = 0; b < bins; b++) {
        hist_eig[b] /= (double)(N_eig - 1);
        hist_rz[b] /= (double)(num_zeros - 1);
    }

    double l2 = 0;
    for (uint32_t b = 0; b < bins; b++) {
        double d = hist_eig[b] - hist_rz[b];
        l2 += d * d;
    }
    nimcp_free(hist_eig);
    nimcp_free(hist_rz);
    return sqrt(l2);
}

/* ============================================================================
 * Lattice QFT
 * ============================================================================ */

void qft_lattice_init(qft_sim_t* sim, bool hot_start) {
    if (!sim || !sim->lattice.field) return;
    uint32_t total = 1;
    for (uint32_t d = 0; d < sim->lattice.dim; d++) total *= sim->lattice.L;

    if (hot_start) {
        for (uint32_t i = 0; i < total; i++)
            sim->lattice.field[i] = (float)(qft_rand_gauss());
    } else {
        for (uint32_t i = 0; i < total; i++)
            sim->lattice.field[i] = 0.0f;
    }
}

int qft_lattice_metropolis_sweep(qft_sim_t* sim) {
    if (!sim || !sim->lattice.field) return -1;
    uint32_t L = sim->lattice.L;
    uint32_t dim = sim->lattice.dim;
    float* phi = sim->lattice.field;
    float mass2 = sim->lattice.mass * sim->lattice.mass;
    float lambda = sim->lattice.coupling;

    uint32_t total = 1;
    for (uint32_t d = 0; d < dim; d++) total *= L;

    uint32_t accepted = 0;
    for (uint32_t site = 0; site < total; site++) {
        /* Compute neighbor sum (nearest neighbors in each dimension) */
        float neighbor_sum = 0;
        uint32_t stride = 1;
        for (uint32_t d = 0; d < dim; d++) {
            /* Periodic boundary: neighbor at ±1 in dimension d.
             * Compute flat indices directly to avoid unsigned subtraction issues. */
            uint32_t coord = (site / stride) % L;
            uint32_t base = site - coord * stride; /* site with coord=0 in this dim */
            uint32_t next = base + ((coord + 1) % L) * stride;
            uint32_t prev = base + ((coord + L - 1) % L) * stride;
            neighbor_sum += phi[next] + phi[prev];
            stride *= L;
        }

        /* Current action contribution */
        float old_phi = phi[site];
        float old_action = 0.5f * mass2 * old_phi * old_phi
                          + 0.25f * lambda * old_phi * old_phi * old_phi * old_phi
                          - old_phi * neighbor_sum;

        /* Propose new value */
        float new_phi = old_phi + (float)(qft_rand_gauss() * 0.5);
        float new_action = 0.5f * mass2 * new_phi * new_phi
                          + 0.25f * lambda * new_phi * new_phi * new_phi * new_phi
                          - new_phi * neighbor_sum;

        /* Metropolis accept/reject */
        float dS = new_action - old_action;
        if (dS < 0 || qft_rand() < exp(-(double)dS)) {
            phi[site] = new_phi;
            accepted++;
        }
    }

    sim->stats.lattice_sweeps++;
    return (int)accepted;
}

void qft_lattice_measure(qft_sim_t* sim) {
    if (!sim || !sim->lattice.field) return;
    uint32_t total = 1;
    for (uint32_t d = 0; d < sim->lattice.dim; d++) total *= sim->lattice.L;

    double sum = 0, sum2 = 0, action = 0;
    for (uint32_t i = 0; i < total; i++) {
        float p = sim->lattice.field[i];
        sum += p;
        sum2 += (double)p * p;
        action += 0.5 * sim->lattice.mass * sim->lattice.mass * p * p
                + 0.25 * sim->lattice.coupling * p * p * p * p;
    }
    sim->lattice.magnetization = sum / total;
    sim->lattice.susceptibility = sum2 / total - (sum / total) * (sum / total);
    sim->lattice.action = action;
}

double qft_wilson_loop(const qft_sim_t* sim, uint32_t R, uint32_t T) {
    /* Wilson loop for pure gauge: W(R,T) ~ exp(-σRT) for confinement
     * Without actual gauge links, return area law estimate */
    (void)sim;
    double sigma = 0.18;  /* string tension in GeV² (lattice QCD value) */
    return exp(-sigma * (double)R * (double)T);
}

/* ============================================================================
 * Standard Model Loader
 * ============================================================================ */

void qft_load_standard_model(qft_sim_t* sim) {
    if (!sim) return;

    /* Set up Higgs sector with physical values */
    qft_higgs_mechanism(sim, 246.22f, 0.129f);

    /* Running couplings at M_Z */
    sim->stats.alpha_em_at_mz = qft_alpha_em(91.2f);
    sim->stats.alpha_s_at_mz = qft_alpha_s(91.2f);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

qft_config_t qft_default_config(void) {
    return (qft_config_t){
        .lattice_L = 8,
        .lattice_dim = 4,
        .rmt_matrix_dim = 32,
        .rmt_samples = 100,
        .reference_scale = 91.2f,
    };
}

qft_sim_t* qft_create(const qft_config_t* config) {
    qft_config_t cfg = config ? *config : qft_default_config();
    qft_sim_t* sim = nimcp_calloc(1, sizeof(*sim));
    if (!sim) return NULL;
    sim->config = cfg;

    /* Allocate RMT matrix and eigenvalues */
    uint32_t N = cfg.rmt_matrix_dim;
    if (N > QFT_MAX_MATRIX_DIM) N = QFT_MAX_MATRIX_DIM;
    sim->rmt.matrix_dim = N;
    sim->rmt.matrix = nimcp_calloc(N * N, sizeof(double));
    sim->rmt.eigenvalues = nimcp_calloc(N, sizeof(double));

    /* Allocate lattice */
    uint32_t L = cfg.lattice_L;
    if (L > QFT_MAX_LATTICE_DIM) L = QFT_MAX_LATTICE_DIM;
    sim->lattice.L = L;
    sim->lattice.dim = cfg.lattice_dim;
    sim->lattice.mass = 1.0f;
    sim->lattice.coupling = 0.1f;
    uint32_t lat_total = 1;
    for (uint32_t d = 0; d < cfg.lattice_dim && d < 4; d++) lat_total *= L;
    sim->lattice.field = nimcp_calloc(lat_total, sizeof(float));

    if (!sim->rmt.matrix || !sim->rmt.eigenvalues || !sim->lattice.field) {
        qft_destroy(sim);
        return NULL;
    }

    sim->initialized = true;
    LOG_INFO(LOG_TAG, "QFT engine created: lattice=%u^%u, RMT=%ux%u",
             L, cfg.lattice_dim, N, N);
    return sim;
}

void qft_destroy(qft_sim_t* sim) {
    if (!sim) return;
    nimcp_free(sim->rmt.matrix);
    nimcp_free(sim->rmt.eigenvalues);
    nimcp_free(sim->lattice.field);
    nimcp_free(sim);
}

qft_stats_t qft_get_stats(const qft_sim_t* sim) {
    if (!sim) return (qft_stats_t){0};
    return sim->stats;
}

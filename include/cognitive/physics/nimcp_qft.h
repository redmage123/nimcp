/**
 * @file nimcp_qft.h
 * @brief Quantum Field Theory — path integrals, gauge symmetry, renormalization
 *
 * WHAT: General QFT framework: Lagrangian field theories, Feynman path integral
 *       evaluation, gauge groups (U(1)/SU(2)/SU(3)), renormalization group flow,
 *       spontaneous symmetry breaking, random matrix theory (GUE/GOE).
 * WHY:  Unifying framework for all fundamental forces. The Standard Model IS
 *       a QFT. Random matrix theory connects to Riemann zeros (Montgomery-
 *       Odlyzko). Renormalization connects to scale-invariance in neural nets.
 * HOW:  Lattice discretization for path integrals, Wilson loops for confinement,
 *       one-loop beta functions, Higgs mechanism, eigenvalue statistics.
 *
 * THEORETICAL FOUNDATION:
 *   Path integral: Z = ∫Dφ exp(iS[φ]/ℏ)
 *   Action: S = ∫d⁴x L(φ, ∂μφ)
 *   Standard Model: L = L_gauge + L_fermion + L_Higgs + L_Yukawa
 *   Beta function: μ dg/dμ = β(g) (running coupling)
 *   RG flow: QED β = g³/(12π²) (grows at high energy)
 *            QCD β = -g³(11N_c-2N_f)/(48π²) (asymptotic freedom)
 *   Random matrices: eigenvalue spacing ↔ Riemann zero spacing
 */

#ifndef NIMCP_QFT_H
#define NIMCP_QFT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QFT_MAX_FIELDS          16
#define QFT_MAX_COUPLINGS       32
#define QFT_MAX_LATTICE_DIM     16      /* per axis for lattice QFT */
#define QFT_MAX_MATRIX_DIM      64      /* for random matrix theory */
#define QFT_MAX_NAME            32

/* ============================================================================
 * Gauge Groups
 * ============================================================================ */

typedef enum {
    QFT_GAUGE_U1       = 0,     /* electromagnetism: α_EM */
    QFT_GAUGE_SU2      = 1,     /* weak force: g_W */
    QFT_GAUGE_SU3      = 2,     /* strong force: g_s (QCD) */
    QFT_GAUGE_SU2xU1   = 3,     /* electroweak: before SSB */
    QFT_GAUGE_SM       = 4,     /* full Standard Model: SU(3)×SU(2)×U(1) */
} qft_gauge_group_t;

/* ============================================================================
 * Field Types
 * ============================================================================ */

typedef enum {
    QFT_FIELD_SCALAR   = 0,     /* Klein-Gordon: (∂²+m²)φ = 0 */
    QFT_FIELD_SPINOR   = 1,     /* Dirac: (iγ^μ∂_μ - m)ψ = 0 */
    QFT_FIELD_VECTOR   = 2,     /* Proca/Maxwell: gauge boson */
    QFT_FIELD_HIGGS    = 3,     /* scalar with Mexican hat potential */
} qft_field_type_t;

typedef struct {
    uint32_t            id;
    char                name[QFT_MAX_NAME];
    qft_field_type_t    type;
    float               mass;           /* GeV */
    float               charge;         /* coupling to gauge field */
    float               spin;           /* 0, 0.5, 1 */
    uint32_t            color_dim;      /* 1=singlet, 3=triplet */
    uint32_t            weak_isospin;   /* 0=singlet, 2=doublet */
    bool                active;
} qft_field_t;

/* ============================================================================
 * Coupling Constants
 * ============================================================================ */

typedef struct {
    qft_gauge_group_t   group;
    float               coupling;       /* g at reference scale */
    float               reference_scale;/* μ₀ in GeV */
    /* Beta function coefficients: β(g) = b₀g³ + b₁g⁵ + ... */
    float               beta_0;         /* one-loop */
    float               beta_1;         /* two-loop */
} qft_coupling_t;

/* ============================================================================
 * Symmetry Breaking
 * ============================================================================ */

typedef struct {
    float               vev;            /* vacuum expectation value (GeV) */
    float               lambda;         /* quartic coupling */
    float               mu_squared;     /* mass parameter (negative for SSB) */
    /* Masses generated: m = g·v/2 for gauge bosons, y·v/√2 for fermions */
    float               W_mass;         /* W± mass after SSB */
    float               Z_mass;         /* Z⁰ mass after SSB */
    float               higgs_mass;     /* physical Higgs mass */
    float               weinberg_angle; /* sin²θ_W */
    bool                broken;         /* has SSB occurred? */
} qft_higgs_sector_t;

/* ============================================================================
 * Random Matrix Theory (for Riemann connection)
 * ============================================================================ */

typedef enum {
    QFT_RMT_GOE = 0,   /* Gaussian Orthogonal Ensemble (time-reversal symmetric) */
    QFT_RMT_GUE = 1,   /* Gaussian Unitary Ensemble (broken TRS) — matches Riemann zeros */
    QFT_RMT_GSE = 2,   /* Gaussian Symplectic Ensemble */
} qft_rmt_ensemble_t;

typedef struct {
    qft_rmt_ensemble_t  ensemble;
    uint32_t            matrix_dim;     /* N × N */
    double*             eigenvalues;    /* [N] sorted */
    double*             matrix;         /* [N*N] */
    /* Spacing statistics */
    double              mean_spacing;
    double              pair_correlation_at_1;
    double              wigner_fit;     /* deviation from Wigner surmise */
    uint32_t            num_samples;    /* ensemble average count */
} qft_rmt_state_t;

/* ============================================================================
 * Lattice QFT (for path integral)
 * ============================================================================ */

typedef struct {
    float*              field;          /* [L^d] lattice field values */
    uint32_t            L;              /* lattice extent per dimension */
    uint32_t            dim;            /* number of dimensions (1-4) */
    float               coupling;       /* lattice coupling */
    float               mass;           /* lattice mass */
    float               beta_lat;       /* inverse coupling (lattice convention) */
    /* Measurements */
    double              action;         /* current total action */
    double              magnetization;  /* <φ> */
    double              susceptibility; /* <φ²> - <φ>² */
    double              plaquette;      /* average plaquette (gauge) */
} qft_lattice_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    lattice_L;          /* lattice size (default: 8) */
    uint32_t    lattice_dim;        /* dimensions (default: 4) */
    uint32_t    rmt_matrix_dim;     /* random matrix size (default: 32) */
    uint32_t    rmt_samples;        /* ensemble samples (default: 100) */
    float       reference_scale;    /* GeV for running couplings (default: 91.2 = M_Z) */
} qft_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    rg_flow_steps;
    uint64_t    lattice_sweeps;
    uint64_t    rmt_samples_generated;
    float       alpha_em_at_mz;     /* α(M_Z) */
    float       alpha_s_at_mz;      /* α_s(M_Z) */
    float       sin2_weinberg;      /* sin²θ_W */
} qft_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct qft_sim {
    qft_field_t         fields[QFT_MAX_FIELDS];
    uint32_t            num_fields;
    qft_coupling_t      couplings[QFT_MAX_COUPLINGS];
    uint32_t            num_couplings;
    qft_higgs_sector_t  higgs;
    qft_rmt_state_t     rmt;
    qft_lattice_t       lattice;
    qft_config_t        config;
    qft_stats_t         stats;
    bool                initialized;
} qft_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

qft_sim_t* qft_create(const qft_config_t* config);
void qft_destroy(qft_sim_t* sim);

/* === Renormalization Group === */

/** Run coupling g from scale μ₁ to μ₂ using one-loop beta function */
float qft_run_coupling(float g, float mu1, float mu2, float beta_0);

/** α_EM(μ): running electromagnetic coupling */
float qft_alpha_em(float mu_gev);

/** α_s(μ): running strong coupling (QCD) */
float qft_alpha_s(float mu_gev);

/** Compute one-loop beta function coefficient for SU(N) with N_f fermions */
float qft_beta_0_sun(uint32_t N_c, uint32_t N_f);

/* === Symmetry Breaking === */

/** Compute Higgs mechanism: given v (VEV), compute W/Z/H masses */
void qft_higgs_mechanism(qft_sim_t* sim, float vev, float lambda);

/** Goldstone theorem: count massless bosons after SSB */
uint32_t qft_goldstone_count(uint32_t group_dim_before, uint32_t group_dim_after);

/* === Random Matrix Theory === */

/** Generate a random matrix from the specified ensemble */
void qft_rmt_generate(qft_sim_t* sim, qft_rmt_ensemble_t ensemble);

/** Compute eigenvalues of the random matrix */
void qft_rmt_compute_eigenvalues(qft_sim_t* sim);

/** Compute spacing statistics from eigenvalues */
void qft_rmt_spacing_statistics(qft_sim_t* sim);

/** GUE pair correlation: 1 - (sin(πx)/(πx))² */
double qft_rmt_gue_correlation(double x);

/** GOE pair correlation: 1 - (sin(πx)/(πx))² - d/dx[sin(πx)/(πx)]·Si(πx) */
double qft_rmt_goe_correlation(double x);

/** Wigner surmise for ensemble: GOE p(s)=(π/2)s·e^(-πs²/4), GUE p(s)=(32/π²)s²·e^(-4s²/π) */
double qft_rmt_wigner_surmise(qft_rmt_ensemble_t ensemble, double s);

/** Compare eigenvalue spacings to Riemann zero spacings */
double qft_rmt_riemann_comparison(const qft_sim_t* sim,
                                    const double* riemann_zeros, uint32_t num_zeros);

/* === Lattice QFT === */

/** Initialize scalar lattice field (hot/cold start) */
void qft_lattice_init(qft_sim_t* sim, bool hot_start);

/** Metropolis sweep on scalar lattice field */
int qft_lattice_metropolis_sweep(qft_sim_t* sim);

/** Measure lattice observables (action, magnetization, susceptibility) */
void qft_lattice_measure(qft_sim_t* sim);

/** Compute Wilson loop W(R,T) on gauge lattice (for confinement) */
double qft_wilson_loop(const qft_sim_t* sim, uint32_t R, uint32_t T);

/* === Standard Model === */

/** Load Standard Model fields and couplings */
void qft_load_standard_model(qft_sim_t* sim);

/** Default config */
qft_config_t qft_default_config(void);

/** Get stats */
qft_stats_t qft_get_stats(const qft_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QFT_H */

/**
 * @file nimcp_qed.h
 * @brief Quantum Electrodynamics — quantum field theory of light and matter
 *
 * WHAT: Simulates QED processes: Feynman diagram amplitudes, cross-sections,
 *       electron-photon vertex, vacuum polarization, pair production,
 *       Compton scattering, Bremsstrahlung, anomalous magnetic moment.
 * WHY:  The most precisely tested theory in physics (g-2 agreement to 12
 *       decimal places). Enables reasoning about quantum light-matter
 *       interaction, laser physics, quantum optics, and precision measurements.
 * HOW:  Tree-level Feynman diagram rules for QED vertex (-ieγ^μ),
 *       propagators, trace technology for unpolarized cross-sections,
 *       running coupling α(q²), one-loop corrections.
 *
 * THEORETICAL FOUNDATION:
 *   QED Lagrangian: L = ψ̄(iγ^μ∂_μ - m)ψ - eψ̄γ^μψA_μ - ¼F_μνF^μν
 *   Vertex factor: -ieγ^μ
 *   Photon propagator: -ig_μν/q²
 *   Electron propagator: i(γ^μp_μ + m)/(p² - m²)
 *   Fine structure constant: α = e²/(4πε₀ℏc) ≈ 1/137.036
 *   Running coupling: α(q²) = α/(1 - (α/3π)ln(q²/m_e²))
 *   Anomalous magnetic moment: a_e = α/(2π) + ... = 0.00115965218...
 */

#ifndef NIMCP_QED_H
#define NIMCP_QED_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_relativistic_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define QED_ALPHA               (1.0f / 137.036f)   /* fine structure constant */
#define QED_ALPHA_INV           137.036f
#define QED_ELECTRON_MASS       0.000511f            /* GeV/c² */
#define QED_MUON_MASS           0.10566f             /* GeV/c² */
#define QED_TAU_MASS            1.777f               /* GeV/c² */
#define QED_HBAR_C              0.1973f              /* GeV·fm (ℏc) */
#define QED_HBAR_C2             0.3894e6f            /* GeV²·pb (ℏc)² for cross-sections */
#define QED_CLASSICAL_E_RADIUS  2.818e-15f           /* m (e²/4πε₀m_ec²) */
#define QED_BOHR_MAGNETON       5.789e-11f           /* MeV/T */

#define QED_MAX_PROCESSES       32
#define QED_MAX_VERTICES        8
#define QED_MAX_PROPAGATORS     8
#define QED_MAX_NAME            32

/* ============================================================================
 * QED Process Types
 * ============================================================================ */

typedef enum {
    QED_PROC_COMPTON         = 0,    /* e + γ → e + γ (Klein-Nishina) */
    QED_PROC_PAIR_PRODUCTION = 1,    /* γ → e⁺ + e⁻ (near nucleus) */
    QED_PROC_PAIR_ANNIHILATION = 2,  /* e⁺ + e⁻ → γ + γ */
    QED_PROC_BREMSSTRAHLUNG  = 3,    /* e + Z → e + Z + γ */
    QED_PROC_MOLLER          = 4,    /* e⁻ + e⁻ → e⁻ + e⁻ */
    QED_PROC_BHABHA          = 5,    /* e⁺ + e⁻ → e⁺ + e⁻ */
    QED_PROC_PHOTON_PHOTON   = 6,    /* γ + γ → e⁺ + e⁻ (Breit-Wheeler) */
    QED_PROC_DELBRUCK        = 7,    /* γ + Z → γ + Z (light-by-light) */
    QED_PROC_THOMSON         = 8,    /* low-energy Compton limit */
    QED_PROC_PHOTOELECTRIC   = 9,    /* γ + atom → e⁻ + ion */
    QED_PROC_COUNT
} qed_process_type_t;

/* ============================================================================
 * Feynman Diagram Representation
 * ============================================================================ */

typedef enum {
    QED_LINE_ELECTRON   = 0,    /* solid line with arrow */
    QED_LINE_POSITRON   = 1,    /* solid line, reversed arrow */
    QED_LINE_PHOTON     = 2,    /* wavy line */
} qed_line_type_t;

typedef struct {
    qed_line_type_t type;
    rel_four_vector_t momentum;
    bool            is_virtual;     /* internal (off-shell) */
    bool            is_incoming;
} qed_external_line_t;

typedef struct {
    uint32_t            num_vertices;
    uint32_t            num_external;
    uint32_t            num_internal;   /* propagators */
    qed_external_line_t externals[8];
    uint32_t            order;          /* perturbation theory order (tree=0, 1-loop=1) */
    float               amplitude_squared; /* |M|² (computed) */
} qed_diagram_t;

/* ============================================================================
 * QED Process (with cross-section)
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    char                name[QED_MAX_NAME];
    qed_process_type_t  type;
    float               threshold_energy;   /* GeV (CM frame) */
    /* Cross-section as function of CM energy */
    float               sigma_total;        /* current total cross-section (pb) */
    float               sigma_differential; /* dσ/dΩ at current angle */
    /* Kinematics */
    float               cm_energy;          /* √s (GeV) */
    float               scattering_angle;   /* θ (radians) */
    bool                active;
} qed_process_t;

/* ============================================================================
 * Vacuum Polarization / Running Coupling
 * ============================================================================ */

typedef struct {
    float       q2;                 /* momentum transfer squared (GeV²) */
    float       alpha_running;      /* α(q²) at this scale */
    float       vacuum_polarization;/* Π(q²) correction */
} qed_running_coupling_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    bool        enable_radiative_corrections; /* one-loop corrections */
    bool        enable_vacuum_polarization;
    uint32_t    perturbation_order;     /* 0=tree, 1=one-loop, 2=two-loop */
    float       ir_cutoff;              /* infrared cutoff (GeV) for soft photons */
} qed_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    total_processes_computed;
    float       anomalous_magnetic_moment;  /* a_e = (g-2)/2 */
    float       lamb_shift_2s_2p;           /* MHz (hydrogen) */
    float       max_cross_section;
    float       min_running_alpha;
    float       max_running_alpha;
} qed_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct qed_sim {
    qed_process_t   processes[QED_MAX_PROCESSES];
    uint32_t        num_processes;
    qed_config_t    config;
    qed_stats_t     stats;
    bool            initialized;
} qed_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

qed_sim_t* qed_create(const qed_config_t* config);
void qed_destroy(qed_sim_t* sim);

/** Load standard QED processes (Compton, pair prod/annihilation, Bhabha, Moller) */
void qed_load_standard_processes(qed_sim_t* sim);

/** Compute cross-section for a process at given CM energy and angle */
float qed_compute_cross_section(qed_sim_t* sim, qed_process_type_t type,
                                  float cm_energy_gev, float angle_rad);

/* === Specific Cross-Sections === */

/** Klein-Nishina (Compton): dσ/dΩ for e+γ→e+γ */
float qed_klein_nishina(float photon_energy_gev, float angle_rad);

/** Thomson scattering: σ_T = 8π/3 · r_e² (low-energy Compton limit) */
float qed_thomson_cross_section(void);

/** Pair production threshold: E_γ > 2m_e (1.022 MeV) */
float qed_pair_production_threshold(void);

/** Pair annihilation: σ = πr_e²/γ · [(γ²+4γ+1)/(γ²-1)·ln(γ+√(γ²-1)) - (γ+3)/√(γ²-1)] */
float qed_pair_annihilation_cross_section(float gamma_cm);

/** Möller scattering: e⁻e⁻→e⁻e⁻ at tree level */
float qed_moller_cross_section(float cm_energy_gev, float angle_rad);

/** Bhabha scattering: e⁺e⁻→e⁺e⁻ at tree level */
float qed_bhabha_cross_section(float cm_energy_gev, float angle_rad);

/** Breit-Wheeler: γγ→e⁺e⁻ threshold and cross-section */
float qed_breit_wheeler_cross_section(float cm_energy_gev);

/* === Running Coupling === */

/** Running coupling α(q²) at one-loop: α(q²) = α/(1 - Π(q²)) */
float qed_running_alpha(float q2_gev2);

/** Vacuum polarization Π(q²) at one-loop */
float qed_vacuum_polarization(float q2_gev2);

/* === Precision QED === */

/** Anomalous magnetic moment: a_e = α/(2π) (Schwinger, first order) */
float qed_anomalous_moment_first_order(void);

/** Anomalous magnetic moment: through O(α³) */
float qed_anomalous_moment_third_order(void);

/** Lamb shift 2S₁/₂ - 2P₁/₂ in hydrogen (MHz) */
float qed_lamb_shift_hydrogen(void);

/** Bethe logarithm contribution to Lamb shift */
float qed_bethe_logarithm(uint32_t n, uint32_t l);

/* === Feynman Rules === */

/** QED vertex factor magnitude: |eγ^μ| → e = sqrt(4πα) */
float qed_vertex_coupling(void);

/** Photon propagator: -ig_μν/q² → 1/q² */
float qed_photon_propagator(float q2_gev2);

/** Electron propagator: i(p̸+m)/(p²-m²) → 1/(p²-m²) */
float qed_electron_propagator(float p2_gev2, float mass_gev);

/** Phase space factor for 2→2 scattering */
float qed_phase_space_2to2(float cm_energy_gev, float m1, float m2, float m3, float m4);

/** Default config */
qed_config_t qed_default_config(void);

/** Get stats */
qed_stats_t qed_get_stats(const qed_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QED_H */

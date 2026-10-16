/**
 * @file nimcp_nuclear_physics.h
 * @brief Nuclear Physics simulation engine — binding energy, decay, fission/fusion
 *
 * WHAT: Semi-empirical mass formula, radioactive decay chains, Q-values,
 *       fission/fusion energetics, cross-sections, Lawson criterion.
 * WHY:  Enables reasoning about nuclear energy, radioactivity, radiation
 *       safety, stellar nucleosynthesis, medical isotopes, dating.
 * HOW:  Weizsacker SEMF for binding energies, exponential decay law,
 *       multi-species decay chain integration, energy balance for reactions.
 *
 * THEORETICAL FOUNDATION:
 *   SEMF (Weizsacker):
 *     B = aV*A - aS*A^(2/3) - aC*Z^2/A^(1/3) - aA*(A-2Z)^2/A + delta
 *     delta = +aP*A^(-1/2) (even-even), 0 (odd-A), -aP*A^(-1/2) (odd-odd)
 *
 *   Decay Law:          N(t) = N0 * exp(-lambda * t)
 *   Half-Life:          t_1/2 = ln(2) / lambda
 *   Activity:           A = lambda * N
 *   Q-Value:            Q = (M_parent - sum M_products) * c^2
 *   Lawson Criterion:   n * tau_E > 1.5e20 m^-3 s  (for D-T fusion)
 *   Cross-Section:      R = n * sigma * phi  (reaction rate)
 */

#ifndef NIMCP_NUCLEAR_PHYSICS_H
#define NIMCP_NUCLEAR_PHYSICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* SEMF coefficients (MeV) — Weizsacker */
#define NP_AV               15.56f      /* volume term */
#define NP_AS               17.23f      /* surface term */
#define NP_AC                0.697f     /* Coulomb term */
#define NP_AA               23.285f     /* asymmetry term */
#define NP_AP               12.0f       /* pairing term */

/* Fundamental */
#define NP_AMU              931.494f    /* MeV/c^2 per atomic mass unit */
#define NP_PROTON_MASS      938.272f    /* MeV/c^2 */
#define NP_NEUTRON_MASS     939.565f    /* MeV/c^2 */
#define NP_ELECTRON_MASS    0.511f      /* MeV/c^2 */
#define NP_ALPHA_MASS       3727.379f   /* MeV/c^2 */
#define NP_C_SQUARED        8.988e16f   /* (m/s)^2 */
#define NP_AVOGADRO         6.022e23f   /* /mol */
#define NP_LN2              0.693147f
#define NP_EV_TO_JOULE      1.602e-19f

/* Decay types */
#define NP_MAX_NUCLIDES     64
#define NP_MAX_CHAIN_LENGTH 16

/* Lawson criterion threshold for D-T fusion */
#define NP_LAWSON_DT        1.5e20f     /* m^-3 * s */

/* Common cross-sections (barns, 1 barn = 1e-24 cm^2 = 1e-28 m^2) */
#define NP_BARN             1.0e-28f    /* m^2 */
#define NP_U235_FISSION_XS  583.0f      /* barns, thermal neutrons */
#define NP_U238_CAPTURE_XS  2.68f       /* barns, thermal neutrons */
#define NP_PU239_FISSION_XS 748.0f      /* barns, thermal neutrons */

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    NP_DECAY_ALPHA      = 0,    /* A-4, Z-2 */
    NP_DECAY_BETA_MINUS = 1,    /* Z+1, A same */
    NP_DECAY_BETA_PLUS  = 2,    /* Z-1, A same */
    NP_DECAY_GAMMA      = 3,    /* no change in A,Z */
    NP_DECAY_NEUTRON    = 4,    /* A-1, Z same */
    NP_DECAY_FISSION    = 5,    /* spontaneous fission */
    NP_DECAY_STABLE     = 6,
} np_decay_type_t;

typedef enum {
    NP_REACTION_FISSION     = 0,
    NP_REACTION_FUSION      = 1,
    NP_REACTION_CAPTURE     = 2,    /* neutron capture */
    NP_REACTION_SCATTER     = 3,
} np_reaction_type_t;

/* ============================================================================
 * Nuclide
 * ============================================================================ */

typedef struct {
    uint16_t        Z;              /* proton number */
    uint16_t        A;              /* mass number */
    char            symbol[8];      /* e.g. "U-235" */
    float           mass_excess;    /* MeV (M - A*amu) */
    float           half_life;      /* seconds (0 = stable, <0 = unknown) */
    float           abundance;      /* natural abundance (0-1) */
    np_decay_type_t decay_mode;
    float           decay_energy;   /* Q-value of dominant decay (MeV) */
    /* Daughter nuclide indices (for chain tracking) */
    int32_t         daughter_idx;   /* index in nuclide array, -1 = none */
} np_nuclide_t;

/* ============================================================================
 * Decay Chain State
 * ============================================================================ */

typedef struct {
    uint32_t        nuclide_idx[NP_MAX_CHAIN_LENGTH];
    double          population[NP_MAX_CHAIN_LENGTH];    /* number of atoms */
    double          activity[NP_MAX_CHAIN_LENGTH];      /* Bq (decays/s) */
    uint32_t        chain_length;
} np_decay_chain_t;

/* ============================================================================
 * Nuclear Reaction
 * ============================================================================ */

typedef struct {
    np_reaction_type_t  type;
    uint16_t    target_Z, target_A;
    uint16_t    projectile_Z, projectile_A;
    uint16_t    product1_Z, product1_A;
    uint16_t    product2_Z, product2_A;
    float       q_value;            /* MeV (positive = exothermic) */
    float       cross_section;      /* barns */
    float       threshold_energy;   /* MeV (for endothermic) */
} np_reaction_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       dt;                 /* time step (seconds) */
    float       temperature;        /* K (for thermal cross-sections) */
    bool        enable_decay_chains;
    bool        enable_fission;
    bool        enable_fusion;
    uint32_t    max_chain_steps;
} np_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    double      total_activity;         /* Bq */
    double      total_binding_energy;   /* MeV (sum over all nuclides) */
    double      total_decay_energy;     /* MeV released this step */
    double      total_fission_energy;
    double      total_fusion_energy;
    uint32_t    num_decays;
    uint32_t    num_fissions;
    uint32_t    num_fusions;
    float       time;
} np_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct nuclear_physics_sim {
    np_nuclide_t        nuclides[NP_MAX_NUCLIDES];
    double              populations[NP_MAX_NUCLIDES];   /* atom counts */
    uint32_t            num_nuclides;

    np_decay_chain_t    chains[NP_MAX_NUCLIDES];
    uint32_t            num_chains;

    np_reaction_t       reactions[NP_MAX_NUCLIDES];
    uint32_t            num_reactions;

    np_config_t         config;
    np_stats_t          stats;
    float               time;
    bool                initialized;
} nuclear_physics_sim_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

nuclear_physics_sim_t* np_create(const np_config_t* config);
void np_destroy(nuclear_physics_sim_t* sim);
int np_step(nuclear_physics_sim_t* sim, float dt);
np_config_t np_default_config(void);
np_stats_t np_get_stats(const nuclear_physics_sim_t* sim);

/* ============================================================================
 * Setup
 * ============================================================================ */

/** Add a nuclide species (returns index) */
uint32_t np_add_nuclide(nuclear_physics_sim_t* sim, const np_nuclide_t* nuclide,
                          double initial_population);

/** Add a nuclear reaction */
uint32_t np_add_reaction(nuclear_physics_sim_t* sim, const np_reaction_t* reaction);

/** Build decay chain from a parent nuclide */
void np_build_decay_chain(nuclear_physics_sim_t* sim, uint32_t parent_idx);

/** Load common nuclides (U-235, U-238, Pu-239, etc.) */
void np_load_common_nuclides(nuclear_physics_sim_t* sim);

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

/** Semi-empirical mass formula: binding energy in MeV */
float np_binding_energy_semf(uint16_t Z, uint16_t A);

/** Binding energy per nucleon: B/A */
float np_binding_energy_per_nucleon(uint16_t Z, uint16_t A);

/** Nuclear mass from SEMF: M = Z*Mp + N*Mn - B/c^2 (in MeV/c^2) */
float np_nuclear_mass(uint16_t Z, uint16_t A);

/** Decay constant from half-life: lambda = ln(2) / t_half */
float np_decay_constant(float half_life);

/** Population after time t: N(t) = N0 * exp(-lambda*t) */
double np_decay_population(double N0, float decay_constant, float time);

/** Activity: A = lambda * N (Bq) */
double np_activity(float decay_constant, double population);

/** Half-life from decay constant */
float np_half_life(float decay_constant);

/** Q-value: Q = (M_parent - M_products) * c^2 (in MeV) */
float np_q_value(uint16_t parent_Z, uint16_t parent_A,
                  uint16_t product1_Z, uint16_t product1_A,
                  uint16_t product2_Z, uint16_t product2_A);

/** Alpha decay Q-value: Q = M(Z,A) - M(Z-2,A-4) - M(2,4) */
float np_alpha_q_value(uint16_t Z, uint16_t A);

/** Fission energy estimate for thermal neutron fission of heavy nucleus */
float np_fission_energy(uint16_t Z, uint16_t A);

/** Lawson criterion check: returns n*tau_E needed for ignition */
float np_lawson_criterion(float density, float confinement_time,
                           float temperature_keV);

/** Coulomb barrier: V_C = k_e * Z1*Z2*e^2 / r (MeV) */
float np_coulomb_barrier(uint16_t Z1, uint16_t Z2, float radius_fm);

/** Nuclear radius: R = r0 * A^(1/3), r0 = 1.25 fm */
float np_nuclear_radius(uint16_t A);

/* Legacy API */
typedef np_config_t nuclear_physics_config_t;
typedef np_stats_t  nuclear_physics_stats_t;

nuclear_physics_sim_t* nuclear_physics_create(const nuclear_physics_config_t* config);
void nuclear_physics_destroy(nuclear_physics_sim_t* sim);
int nuclear_physics_step(nuclear_physics_sim_t* sim, float dt);
nuclear_physics_config_t nuclear_physics_default_config(void);
nuclear_physics_stats_t nuclear_physics_get_stats(const nuclear_physics_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NUCLEAR_PHYSICS_H */

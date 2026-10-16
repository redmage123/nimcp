/**
 * @file nimcp_relativistic_physics.h
 * @brief Relativistic Physics — special and general relativistic corrections
 *
 * WHAT: Extends the Newtonian physics engine with Lorentz transformations,
 *       relativistic momentum, mass-energy equivalence, time dilation,
 *       length contraction, and gravitational time dilation (weak field).
 * WHY:  Enables reasoning about high-energy physics, astrophysics, GPS
 *       corrections, particle accelerators, and nuclear reactions.
 * HOW:  Hamiltonian formulation with relativistic kinetic energy
 *       T = (gamma-1)*m*c^2, Lorentz factor gamma = 1/sqrt(1-v^2/c^2),
 *       4-momentum conservation, optional Schwarzschild metric for
 *       weak-field general relativity.
 *
 * THEORETICAL FOUNDATION:
 *   - Special Relativity (Einstein, 1905): Lorentz invariance, E=mc^2
 *   - Relativistic Hamiltonian: H = sqrt(p^2*c^2 + m^2*c^4) + V
 *   - Schwarzschild metric for weak-field GR: dt_proper/dt_coord = sqrt(1 - 2GM/rc^2)
 *   - 4-vector formalism: p^mu = (E/c, px, py, pz)
 */

#ifndef NIMCP_RELATIVISTIC_PHYSICS_H
#define NIMCP_RELATIVISTIC_PHYSICS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define REL_C               299792458.0f        /* speed of light (m/s) */
#define REL_C2              (REL_C * REL_C)     /* c^2 */
#define REL_G_NEWTON        6.674e-11f          /* gravitational constant */
#define REL_PLANCK          6.626e-34f          /* Planck's constant */
#define REL_BOLTZMANN       1.381e-23f          /* Boltzmann constant */
#define REL_ELECTRON_MASS   9.109e-31f          /* electron rest mass (kg) */
#define REL_PROTON_MASS     1.673e-27f          /* proton rest mass (kg) */
#define REL_MAX_PARTICLES   256

/* Threshold: apply relativistic corrections when v > REL_THRESHOLD * c */
#define REL_THRESHOLD       0.01f               /* 1% speed of light */

/* ============================================================================
 * 4-Vector
 * ============================================================================ */

typedef struct {
    float t;    /* time component (ct or E/c) */
    float x;
    float y;
    float z;
} rel_four_vector_t;

/* ============================================================================
 * Relativistic Particle
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    wm_parietal_vec3_t  position;
    wm_parietal_vec3_t  velocity;       /* 3-velocity */
    wm_parietal_vec3_t  momentum;       /* relativistic 3-momentum: gamma*m*v */
    float               rest_mass;      /* invariant mass (kg) */
    float               charge;         /* electric charge (Coulombs) */
    float               gamma;          /* Lorentz factor: 1/sqrt(1-v^2/c^2) */
    float               kinetic_energy; /* (gamma-1)*m*c^2 */
    float               total_energy;   /* gamma*m*c^2 */
    float               proper_time;    /* accumulated proper time */
    rel_four_vector_t   four_momentum;  /* (E/c, px, py, pz) */
    bool                active;
} rel_particle_t;

/* ============================================================================
 * Lorentz Boost (frame transformation)
 * ============================================================================ */

typedef struct {
    float               beta_x, beta_y, beta_z; /* v/c components */
    float               gamma;
    float               matrix[4][4];   /* full Lorentz transformation matrix */
} rel_lorentz_boost_t;

/* ============================================================================
 * Gravitational Source (for weak-field GR)
 * ============================================================================ */

typedef struct {
    wm_parietal_vec3_t  position;
    float               mass;           /* kg */
    float               schwarzschild_r;/* 2GM/c^2 */
} rel_gravity_source_t;

#define REL_MAX_GRAVITY_SOURCES 8

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    bool        enable_special_relativity;
    bool        enable_general_relativity;  /* weak-field Schwarzschild */
    bool        enable_mass_energy;         /* E=mc^2 conversions */
    float       velocity_threshold;         /* fraction of c to activate corrections */
    float       dt;                         /* coordinate time step */
} rel_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       max_gamma;              /* highest Lorentz factor seen */
    float       max_velocity_fraction;  /* highest v/c seen */
    float       total_rest_energy;      /* sum of m*c^2 */
    float       total_kinetic_energy;
    float       total_energy;           /* should be conserved */
    float       energy_drift;
    float       max_time_dilation;      /* largest proper_time/coord_time ratio */
    uint32_t    active_particles;
} rel_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct relativistic_engine {
    rel_particle_t      particles[REL_MAX_PARTICLES];
    uint32_t            num_particles;
    rel_gravity_source_t gravity_sources[REL_MAX_GRAVITY_SOURCES];
    uint32_t            num_gravity_sources;
    rel_config_t        config;
    rel_stats_t         stats;
    double              initial_total_energy; /* baseline for energy drift (per-instance) */
    struct electromagnetic_sim* em_coupling; /* optional: EM engine for Lorentz force coupling */
    float               coordinate_time;
    bool                initialized;
} relativistic_engine_t;

/* ============================================================================
 * API
 * ============================================================================ */

relativistic_engine_t* relativistic_create(const rel_config_t* config);
void relativistic_destroy(relativistic_engine_t* engine);

/** Add a particle. Returns particle id */
uint32_t relativistic_add_particle(relativistic_engine_t* engine,
                                    const rel_particle_t* particle);

/** Add a gravitational source for weak-field GR */
uint32_t relativistic_add_gravity_source(relativistic_engine_t* engine,
                                          wm_parietal_vec3_t position, float mass);

/** Step the simulation using relativistic Hamiltonian dynamics */
int relativistic_step(relativistic_engine_t* engine, float dt);

/** Compute Lorentz factor for a velocity */
float relativistic_gamma(wm_parietal_vec3_t velocity);

/** Compute relativistic momentum: p = gamma * m * v */
wm_parietal_vec3_t relativistic_momentum(float rest_mass, wm_parietal_vec3_t velocity);

/** Compute total energy: E = gamma * m * c^2 */
float relativistic_total_energy(float rest_mass, wm_parietal_vec3_t velocity);

/** Compute kinetic energy: T = (gamma - 1) * m * c^2 */
float relativistic_kinetic_energy(float rest_mass, wm_parietal_vec3_t velocity);

/** Build Lorentz boost matrix for frame transformation */
rel_lorentz_boost_t relativistic_build_boost(wm_parietal_vec3_t frame_velocity);

/** Transform a 4-vector under a Lorentz boost */
rel_four_vector_t relativistic_boost_transform(const rel_lorentz_boost_t* boost,
                                                rel_four_vector_t vec);

/** Compute proper time interval given coordinate time and velocity */
float relativistic_proper_time(float coord_dt, wm_parietal_vec3_t velocity);

/** Compute gravitational time dilation factor at a point (weak-field) */
float relativistic_gravitational_dilation(const relativistic_engine_t* engine,
                                           wm_parietal_vec3_t position);

/** Mass-energy equivalence: E = m * c^2 */
float relativistic_rest_energy(float rest_mass);

/** Velocity addition (relativistic): u' = (u + v) / (1 + u*v/c^2) for collinear */
float relativistic_velocity_addition(float u, float v);

/** Compute invariant mass from 4-momentum: m^2*c^2 = E^2/c^2 - |p|^2 */
float relativistic_invariant_mass(rel_four_vector_t four_momentum);

/** Get stats */
rel_stats_t relativistic_get_stats(const relativistic_engine_t* engine);

/** Default config */
rel_config_t relativistic_default_config(void);

/** Connect EM engine for Lorentz force coupling (F = q(E + v×B)) */
void relativistic_connect_em(relativistic_engine_t* engine,
                              struct electromagnetic_sim* em);

/** Step with RK4 integration (4th-order accuracy instead of symplectic Euler) */
int relativistic_step_rk4(relativistic_engine_t* engine, float dt);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RELATIVISTIC_PHYSICS_H */

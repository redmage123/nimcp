/**
 * @file nimcp_electromagnetic.h
 * @brief Electromagnetic Field Simulator — Maxwell's equations on a grid
 *
 * WHAT: Simulates electric and magnetic fields, charge dynamics, Lorentz force,
 *       electromagnetic wave propagation, and field energy.
 * WHY:  Enables reasoning about electricity, magnetism, light, radio waves,
 *       circuits, motors, generators, and electromagnetic induction.
 * HOW:  Yee grid (FDTD) for field propagation, point charges with Lorentz
 *       force F = q(E + v x B), Gauss/Faraday/Ampere law enforcement.
 *
 * THEORETICAL FOUNDATION:
 *   Maxwell's Equations (differential form):
 *     div E = rho/epsilon_0          (Gauss's law)
 *     div B = 0                      (no magnetic monopoles)
 *     curl E = -dB/dt                (Faraday's law)
 *     curl B = mu_0*J + mu_0*eps_0*dE/dt  (Ampere-Maxwell law)
 *
 *   Lorentz Force: F = q(E + v x B)
 *   Field Energy: u = (eps_0*|E|^2 + |B|^2/mu_0) / 2
 *   Poynting Vector: S = E x B / mu_0
 */

#ifndef NIMCP_ELECTROMAGNETIC_H
#define NIMCP_ELECTROMAGNETIC_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define EM_EPSILON_0        8.854e-12f      /* vacuum permittivity (F/m) */
#define EM_MU_0             1.257e-6f       /* vacuum permeability (H/m) */
#define EM_C                299792458.0f    /* speed of light (m/s) */
#define EM_COULOMB_K        8.988e9f        /* Coulomb constant 1/(4*pi*eps_0) */
#define EM_ELECTRON_CHARGE  1.602e-19f      /* elementary charge (C) */

#define EM_MAX_CHARGES      128
#define EM_MAX_CURRENTS      32
#define EM_MAX_GRID_DIM      64             /* per axis — 64^3 = 262K cells */
#define EM_MAX_DIPOLES       32
#define EM_MAX_CONDUCTORS    16

/* ============================================================================
 * Vector Field (3D grid of 3-vectors)
 * ============================================================================ */

typedef struct {
    float*      data;           /* [nx * ny * nz * 3] interleaved xyz */
    uint32_t    nx, ny, nz;
    float       dx, dy, dz;    /* cell size (meters) */
    float       origin_x, origin_y, origin_z;
} em_vector_field_t;

/* ============================================================================
 * Scalar Field (3D grid of scalars)
 * ============================================================================ */

typedef struct {
    float*      data;           /* [nx * ny * nz] */
    uint32_t    nx, ny, nz;
    float       dx, dy, dz;
} em_scalar_field_t;

/* ============================================================================
 * Point Charge
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    wm_parietal_vec3_t  position;
    wm_parietal_vec3_t  velocity;
    float               charge;         /* Coulombs */
    float               mass;           /* kg (for Lorentz force dynamics) */
    bool                fixed;          /* immovable (electrode, etc.) */
    bool                active;
} em_charge_t;

/* ============================================================================
 * Current Source (steady current along a path)
 * ============================================================================ */

typedef struct {
    wm_parietal_vec3_t  start;
    wm_parietal_vec3_t  end;
    float               current;        /* Amperes */
    bool                active;
} em_current_t;

/* ============================================================================
 * Magnetic Dipole
 * ============================================================================ */

typedef struct {
    wm_parietal_vec3_t  position;
    wm_parietal_vec3_t  moment;         /* magnetic dipole moment (A*m^2) */
    bool                active;
} em_dipole_t;

/* ============================================================================
 * Conductor (simplified — perfect conductor region)
 * ============================================================================ */

typedef struct {
    wm_parietal_vec3_t  min_corner;
    wm_parietal_vec3_t  max_corner;
    float               conductivity;   /* S/m (0 = insulator, inf = perfect) */
    bool                active;
} em_conductor_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    grid_dim;           /* cells per axis (default: 32) */
    float       cell_size;          /* meters per cell (default: 0.01) */
    float       dt;                 /* time step (default: cell_size / (2*c) for CFL) */
    float       permittivity;       /* relative permittivity (default: 1.0 = vacuum) */
    float       permeability;       /* relative permeability (default: 1.0 = vacuum) */
    bool        enable_wave_propagation; /* FDTD wave propagation (default: true) */
    bool        enable_charge_dynamics;  /* move charges under Lorentz force */
} em_config_t;

/* ============================================================================
 * Boundary Conditions
 * ============================================================================ */

typedef enum {
    EM_BOUNDARY_PEC     = 0,    /* perfect electric conductor (E_tangential = 0) */
    EM_BOUNDARY_PMC     = 1,    /* perfect magnetic conductor (B_tangential = 0) */
    EM_BOUNDARY_ABSORBING = 2,  /* Mur first-order absorbing */
    EM_BOUNDARY_PERIODIC = 3,
} em_boundary_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       total_field_energy;     /* integral of (eps|E|^2 + |B|^2/mu)/2 */
    float       total_kinetic_energy;   /* sum of 0.5*m*v^2 for charges */
    float       max_E_magnitude;
    float       max_B_magnitude;
    float       total_charge;           /* should be conserved */
    float       energy_drift;
    uint32_t    active_charges;
} em_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct electromagnetic_sim {
    /* Fields (Yee grid) */
    em_vector_field_t   E_field;        /* electric field */
    em_vector_field_t   B_field;        /* magnetic field */
    em_scalar_field_t   charge_density; /* rho */
    em_vector_field_t   current_density;/* J */

    /* Sources */
    em_charge_t         charges[EM_MAX_CHARGES];
    uint32_t            num_charges;
    em_current_t        currents[EM_MAX_CURRENTS];
    uint32_t            num_currents;
    em_dipole_t         dipoles[EM_MAX_DIPOLES];
    uint32_t            num_dipoles;
    em_conductor_t      conductors[EM_MAX_CONDUCTORS];
    uint32_t            num_conductors;

    em_config_t         config;
    em_boundary_t       boundary;
    em_stats_t          stats;
    float               time;
    bool                initialized;
} electromagnetic_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

electromagnetic_sim_t* em_create(const em_config_t* config);
void em_destroy(electromagnetic_sim_t* sim);

/** Add a point charge (returns charge id) */
uint32_t em_add_charge(electromagnetic_sim_t* sim, const em_charge_t* charge);

/** Add a steady current source */
uint32_t em_add_current(electromagnetic_sim_t* sim, const em_current_t* current);

/** Add a magnetic dipole */
uint32_t em_add_dipole(electromagnetic_sim_t* sim, const em_dipole_t* dipole);

/** Add a conductor region */
uint32_t em_add_conductor(electromagnetic_sim_t* sim, const em_conductor_t* conductor);

/** Step the simulation (FDTD update + charge dynamics) */
int em_step(electromagnetic_sim_t* sim, float dt);

/** Get electric field at a point (trilinear interpolation) */
wm_parietal_vec3_t em_get_E_at(const electromagnetic_sim_t* sim, wm_parietal_vec3_t pos);

/** Get magnetic field at a point (trilinear interpolation) */
wm_parietal_vec3_t em_get_B_at(const electromagnetic_sim_t* sim, wm_parietal_vec3_t pos);

/** Compute Lorentz force on a charge at given position/velocity */
wm_parietal_vec3_t em_lorentz_force(const electromagnetic_sim_t* sim,
                                     float charge, wm_parietal_vec3_t position,
                                     wm_parietal_vec3_t velocity);

/** Compute Coulomb force between two charges */
wm_parietal_vec3_t em_coulomb_force(float q1, wm_parietal_vec3_t pos1,
                                     float q2, wm_parietal_vec3_t pos2);

/** Compute magnetic field from a current segment (Biot-Savart) */
wm_parietal_vec3_t em_biot_savart(const em_current_t* current, wm_parietal_vec3_t point);

/** Compute Poynting vector (energy flux) at a point */
wm_parietal_vec3_t em_poynting_vector(const electromagnetic_sim_t* sim,
                                       wm_parietal_vec3_t pos);

/** Compute total field energy (volume integral) */
float em_total_field_energy(const electromagnetic_sim_t* sim);

/** Compute total charge (should be conserved) */
float em_total_charge(const electromagnetic_sim_t* sim);

/** Compute electric potential at a point (sum of Coulomb potentials) */
float em_electric_potential(const electromagnetic_sim_t* sim, wm_parietal_vec3_t pos);

/** Inject a plane wave (for wave propagation testing) */
void em_inject_plane_wave(electromagnetic_sim_t* sim,
                           wm_parietal_vec3_t direction,
                           wm_parietal_vec3_t polarization,
                           float frequency, float amplitude);

/** Set boundary conditions */
void em_set_boundary(electromagnetic_sim_t* sim, em_boundary_t boundary);

/** Get stats */
em_stats_t em_get_stats(const electromagnetic_sim_t* sim);

/** Default config */
em_config_t em_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ELECTROMAGNETIC_H */

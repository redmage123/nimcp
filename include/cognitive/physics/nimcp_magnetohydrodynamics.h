/**
 * @file nimcp_magnetohydrodynamics.h
 * @brief Magnetohydrodynamics (MHD) — coupled fluid + electromagnetic dynamics
 *
 * WHAT: Simulates electrically conducting fluids in magnetic fields: plasmas,
 *       stellar interiors, fusion reactors, solar wind, liquid metals.
 * WHY:  Enables reasoning about plasma physics, stellar evolution, fusion energy,
 *       geodynamo (Earth's magnetic field), space weather, and industrial MHD.
 * HOW:  Ideal MHD equations: Navier-Stokes + Maxwell coupled through Lorentz
 *       force (J x B) and Ohm's law (E + v x B = eta * J). Finite volume
 *       method on a 3D grid with constrained transport for div(B)=0.
 *
 * THEORETICAL FOUNDATION:
 *   Ideal MHD Equations:
 *     d(rho)/dt + div(rho*v) = 0               (mass conservation)
 *     rho*dv/dt = -grad(p) + J x B + rho*g     (momentum: Navier-Stokes + Lorentz)
 *     dB/dt = curl(v x B) - curl(eta * J)      (induction equation)
 *     d(e)/dt = -p*div(v) + eta*|J|^2          (energy: PdV work + Ohmic heating)
 *     div(B) = 0                                (solenoidal constraint)
 *     J = curl(B) / mu_0                        (Ampere's law, no displacement)
 *
 *   Key dimensionless numbers:
 *     Rm = mu_0 * sigma * L * V                 (magnetic Reynolds number)
 *     Ha = B * L * sqrt(sigma / (rho * nu))     (Hartmann number)
 *     beta = 2 * mu_0 * p / B^2                 (plasma beta)
 *     Alfven speed: v_A = B / sqrt(mu_0 * rho)
 */

#ifndef NIMCP_MAGNETOHYDRODYNAMICS_H
#define NIMCP_MAGNETOHYDRODYNAMICS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MHD_MAX_GRID_DIM        48          /* per axis — 48^3 = 110K cells */
#define MHD_MAX_SOURCES         16
#define MHD_MU_0                1.257e-6f   /* vacuum permeability */
#define MHD_BOLTZMANN           1.381e-23f

/* ============================================================================
 * Fluid Cell State
 * ============================================================================ */

typedef struct {
    float               density;        /* rho (kg/m^3) */
    wm_parietal_vec3_t  velocity;       /* v (m/s) */
    wm_parietal_vec3_t  B;              /* magnetic field (Tesla) */
    float               pressure;       /* p (Pa) */
    float               temperature;    /* T (Kelvin) */
    float               resistivity;    /* eta (Ohm*m) — 0 for ideal MHD */
} mhd_cell_t;

/* ============================================================================
 * Grid
 * ============================================================================ */

typedef struct {
    mhd_cell_t* cells;              /* [nx * ny * nz] */
    uint32_t    nx, ny, nz;
    float       dx, dy, dz;         /* cell size (meters) */
    float       origin_x, origin_y, origin_z;
} mhd_grid_t;

/* ============================================================================
 * External Source (driving fields/flows)
 * ============================================================================ */

typedef enum {
    MHD_SOURCE_UNIFORM_B    = 0,    /* uniform background field */
    MHD_SOURCE_DIPOLE_B     = 1,    /* magnetic dipole (planet/star) */
    MHD_SOURCE_FLOW         = 2,    /* imposed flow (solar wind, channel) */
    MHD_SOURCE_HEATING      = 3,    /* localized heating (fusion, corona) */
    MHD_SOURCE_GRAVITY      = 4,    /* gravitational acceleration */
} mhd_source_type_t;

typedef struct {
    mhd_source_type_t   type;
    wm_parietal_vec3_t  position;       /* center of source */
    wm_parietal_vec3_t  direction;      /* field/flow direction */
    float               magnitude;      /* field strength (T) or flow speed (m/s) */
    float               radius;         /* extent of source */
    bool                active;
} mhd_source_t;

/* ============================================================================
 * Boundary Conditions
 * ============================================================================ */

typedef enum {
    MHD_BC_PERIODIC     = 0,
    MHD_BC_REFLECTIVE   = 1,
    MHD_BC_OUTFLOW      = 2,
    MHD_BC_CONDUCTING   = 3,    /* perfect conductor wall */
    MHD_BC_INSULATING   = 4,
} mhd_boundary_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    grid_dim;           /* cells per axis (default: 32) */
    float       cell_size;          /* meters per cell */
    float       dt;                 /* time step (must satisfy CFL) */
    float       gamma;              /* adiabatic index (5/3 for monatomic gas) */
    float       resistivity;        /* uniform resistivity (0 = ideal MHD) */
    float       viscosity;          /* kinematic viscosity (0 = inviscid) */
    mhd_boundary_t boundary;
    bool        constrained_transport; /* enforce div(B)=0 (default: true) */
    bool        enable_ohmic_heating;  /* eta * |J|^2 heating term */
} mhd_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       total_kinetic_energy;       /* integral of 0.5*rho*|v|^2 */
    float       total_magnetic_energy;      /* integral of |B|^2/(2*mu_0) */
    float       total_thermal_energy;       /* integral of p/(gamma-1) */
    float       total_energy;               /* sum of all three */
    float       initial_total_energy;
    float       energy_drift;
    float       max_velocity;
    float       max_B;
    float       max_density;
    float       alfven_speed;               /* B/sqrt(mu_0*rho) at center */
    float       plasma_beta;                /* 2*mu_0*p/B^2 at center */
    float       magnetic_reynolds;          /* estimated Rm */
    float       max_div_B;                  /* div(B) error (should be ~0) */
    float       max_mach;                   /* max v/c_s (sound speed) */
} mhd_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct mhd_sim {
    mhd_grid_t      grid;
    mhd_grid_t      grid_temp;      /* scratch for time-stepping */
    mhd_source_t    sources[MHD_MAX_SOURCES];
    uint32_t        num_sources;
    mhd_config_t    config;
    mhd_stats_t     stats;
    float           time;
    bool            initialized;
} mhd_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

mhd_sim_t* mhd_create(const mhd_config_t* config);
void mhd_destroy(mhd_sim_t* sim);

/** Add an external source (background field, flow, heating) */
uint32_t mhd_add_source(mhd_sim_t* sim, const mhd_source_t* source);

/** Set initial conditions for a cell */
void mhd_set_cell(mhd_sim_t* sim, uint32_t ix, uint32_t iy, uint32_t iz,
                    const mhd_cell_t* state);

/** Set uniform initial conditions */
void mhd_set_uniform(mhd_sim_t* sim, float density, float pressure,
                      wm_parietal_vec3_t velocity, wm_parietal_vec3_t B);

/** Step the MHD simulation (finite volume + constrained transport) */
int mhd_step(mhd_sim_t* sim, float dt);

/** Get cell state at grid indices */
const mhd_cell_t* mhd_get_cell(const mhd_sim_t* sim,
                                 uint32_t ix, uint32_t iy, uint32_t iz);

/** Get interpolated state at a world position */
mhd_cell_t mhd_get_at(const mhd_sim_t* sim, wm_parietal_vec3_t pos);

/** Compute current density J = curl(B)/mu_0 at a cell */
wm_parietal_vec3_t mhd_current_density(const mhd_sim_t* sim,
                                         uint32_t ix, uint32_t iy, uint32_t iz);

/** Compute Lorentz force density J x B at a cell */
wm_parietal_vec3_t mhd_lorentz_force(const mhd_sim_t* sim,
                                       uint32_t ix, uint32_t iy, uint32_t iz);

/** Compute Alfven speed at a cell: v_A = |B| / sqrt(mu_0 * rho) */
float mhd_alfven_speed(const mhd_sim_t* sim,
                        uint32_t ix, uint32_t iy, uint32_t iz);

/** Compute plasma beta at a cell: beta = 2*mu_0*p / |B|^2 */
float mhd_plasma_beta(const mhd_sim_t* sim,
                       uint32_t ix, uint32_t iy, uint32_t iz);

/** Compute div(B) at a cell (should be ~0) */
float mhd_div_B(const mhd_sim_t* sim, uint32_t ix, uint32_t iy, uint32_t iz);

/** Compute total magnetic helicity (integral of A dot B) — topological invariant */
float mhd_magnetic_helicity(const mhd_sim_t* sim);

/** Initialize Orszag-Tang vortex (standard 2D MHD test problem) */
void mhd_init_orszag_tang(mhd_sim_t* sim);

/** Initialize Kelvin-Helmholtz instability */
void mhd_init_kelvin_helmholtz(mhd_sim_t* sim, float B_parallel);

/** Initialize magnetic reconnection (Harris current sheet) */
void mhd_init_harris_sheet(mhd_sim_t* sim, float B0, float thickness);

/** Initialize Rayleigh-Taylor instability in MHD */
void mhd_init_rayleigh_taylor(mhd_sim_t* sim, float density_ratio,
                                wm_parietal_vec3_t gravity);

/** Get stats */
mhd_stats_t mhd_get_stats(const mhd_sim_t* sim);

/** Default config */
mhd_config_t mhd_default_config(void);

/** Compute CFL-limited timestep */
float mhd_cfl_dt(const mhd_sim_t* sim, float cfl_factor);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MAGNETOHYDRODYNAMICS_H */

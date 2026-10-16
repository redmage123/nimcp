/**
 * @file nimcp_fluid_dynamics.h
 * @brief Fluid Dynamics simulation engine — Navier-Stokes on a 3D grid
 *
 * WHAT: Compressible/incompressible flow on a 3D Eulerian grid using finite
 *       volume method.  Mass conservation, momentum (pressure gradient +
 *       viscosity + gravity), energy transport.
 * WHY:  Enables reasoning about fluid flow, aerodynamics, pipe systems,
 *       weather, ocean currents, blood flow, turbulence.
 * HOW:  Finite-volume discretization, explicit Euler time integration,
 *       CFL-limited time stepping.
 *
 * THEORETICAL FOUNDATION:
 *   Continuity:   d(rho)/dt + div(rho*v) = 0
 *   Momentum:     rho * Dv/Dt = -grad(p) + mu*laplacian(v) + rho*g
 *   Energy:       rho*Cp * DT/dt = k*laplacian(T) + Phi   (Phi = viscous dissipation)
 *   Bernoulli:    p + 0.5*rho*v^2 + rho*g*h = const (along streamline, inviscid)
 *   Reynolds:     Re = rho*v*L / mu
 *   Drag:         F_d = 0.5 * C_d * rho * A * v^2
 *   Poiseuille:   Q = pi*R^4*dP / (8*mu*L)
 */

#ifndef NIMCP_FLUID_DYNAMICS_H
#define NIMCP_FLUID_DYNAMICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FD_AIR_DENSITY          1.225f          /* kg/m^3 at sea level 15C */
#define FD_WATER_DENSITY        998.2f          /* kg/m^3 at 20C */
#define FD_AIR_VISCOSITY        1.81e-5f        /* Pa*s (dynamic viscosity) */
#define FD_WATER_VISCOSITY      1.002e-3f       /* Pa*s at 20C */
#define FD_ATM_PRESSURE         101325.0f       /* Pa */
#define FD_GRAVITY              9.80665f        /* m/s^2 */
#define FD_AIR_SPEED_OF_SOUND   343.0f          /* m/s at 20C */
#define FD_GAS_CONSTANT         287.058f        /* J/(kg*K) for dry air */
#define FD_GAMMA_AIR            1.4f            /* ratio of specific heats */

#define FD_MAX_GRID_DIM         64              /* per axis — 64^3 = 262K cells */
#define FD_MAX_OBSTACLES        32
#define FD_MAX_SOURCES          16

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    FD_BC_NO_SLIP       = 0,    /* v = 0 at wall */
    FD_BC_FREE_SLIP     = 1,    /* v_normal = 0, v_tangential free */
    FD_BC_INFLOW        = 2,    /* prescribed velocity */
    FD_BC_OUTFLOW       = 3,    /* zero gradient */
    FD_BC_PERIODIC      = 4,
} fd_boundary_type_t;

typedef enum {
    FD_FLUID_AIR        = 0,
    FD_FLUID_WATER      = 1,
    FD_FLUID_CUSTOM     = 2,
} fd_fluid_type_t;

/* ============================================================================
 * 3D Vector / Scalar Fields
 * ============================================================================ */

typedef struct {
    float*      data;           /* [nx * ny * nz * 3] interleaved xyz */
    uint32_t    nx, ny, nz;
    float       dx, dy, dz;    /* cell size (meters) */
} fd_vector_field_t;

typedef struct {
    float*      data;           /* [nx * ny * nz] */
    uint32_t    nx, ny, nz;
    float       dx, dy, dz;
} fd_scalar_field_t;

/* ============================================================================
 * Obstacle (solid body in flow)
 * ============================================================================ */

typedef struct {
    float       center_x, center_y, center_z;
    float       radius;         /* spherical obstacle */
    float       drag_coeff;     /* C_d override (0 = auto) */
    bool        active;
} fd_obstacle_t;

/* ============================================================================
 * Flow Source / Sink
 * ============================================================================ */

typedef struct {
    float       pos_x, pos_y, pos_z;
    float       velocity_x, velocity_y, velocity_z;
    float       mass_rate;      /* kg/s (positive = source, negative = sink) */
    bool        active;
} fd_source_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t            grid_dim;           /* cells per axis (default: 32) */
    float               cell_size;          /* meters per cell (default: 0.01) */
    float               dt;                 /* time step (default: auto CFL) */
    fd_fluid_type_t     fluid_type;
    float               density;            /* kg/m^3 */
    float               viscosity;          /* Pa*s (dynamic) */
    float               gravity_x, gravity_y, gravity_z;
    fd_boundary_type_t  boundary;
    float               inflow_vx, inflow_vy, inflow_vz;
    float               reference_pressure; /* Pa */
    bool                compressible;       /* enable compressible solver */
} fd_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       max_velocity;           /* |v|_max across grid */
    float       max_vorticity;          /* |omega|_max */
    float       total_kinetic_energy;   /* sum 0.5*rho*|v|^2 * dV */
    float       max_pressure;
    float       min_pressure;
    float       avg_reynolds;
    float       cfl_number;             /* max |v|*dt/dx */
    float       total_mass;             /* should be conserved */
    float       time;
} fd_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct fluid_dynamics_sim {
    /* Primitive variables */
    fd_vector_field_t   velocity;       /* u, v, w */
    fd_scalar_field_t   pressure;
    fd_scalar_field_t   density_field;  /* for compressible flow */
    fd_scalar_field_t   temperature;    /* for energy equation */

    /* Derived */
    fd_vector_field_t   vorticity;      /* curl(v) */

    /* Sources and obstacles */
    fd_obstacle_t       obstacles[FD_MAX_OBSTACLES];
    uint32_t            num_obstacles;
    fd_source_t         sources[FD_MAX_SOURCES];
    uint32_t            num_sources;

    fd_config_t         config;
    fd_stats_t          stats;
    float               time;
    bool                initialized;
} fluid_dynamics_sim_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

fluid_dynamics_sim_t* fd_create(const fd_config_t* config);
void fd_destroy(fluid_dynamics_sim_t* sim);
int fd_step(fluid_dynamics_sim_t* sim, float dt);
fd_config_t fd_default_config(void);
fd_stats_t fd_get_stats(const fluid_dynamics_sim_t* sim);

/* ============================================================================
 * Setup
 * ============================================================================ */

/** Initialize uniform channel flow (Poiseuille-like) */
void fd_init_channel_flow(fluid_dynamics_sim_t* sim, float velocity);

/** Initialize lid-driven cavity flow */
void fd_init_cavity_flow(fluid_dynamics_sim_t* sim, float lid_velocity);

/** Initialize flow around a sphere at grid center */
void fd_init_flow_around_sphere(fluid_dynamics_sim_t* sim, float radius,
                                 float freestream_v);

/** Add obstacle */
uint32_t fd_add_obstacle(fluid_dynamics_sim_t* sim, const fd_obstacle_t* obs);

/** Add flow source/sink */
uint32_t fd_add_source(fluid_dynamics_sim_t* sim, const fd_source_t* src);

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

/** Reynolds number: Re = rho * v * L / mu */
float fd_reynolds_number(float density, float velocity, float length, float viscosity);

/** Drag force: F = 0.5 * Cd * rho * A * v^2 */
float fd_drag_force(float drag_coeff, float density, float area, float velocity);

/** Lift force: F = 0.5 * Cl * rho * A * v^2 */
float fd_lift_force(float lift_coeff, float density, float area, float velocity);

/** Bernoulli pressure: p + 0.5*rho*v^2 + rho*g*h = const */
float fd_bernoulli_pressure(float density, float velocity, float height,
                             float reference_pressure);

/** Terminal velocity: v = sqrt(2*m*g / (rho * Cd * A)) */
float fd_terminal_velocity(float mass, float drag_coeff, float density,
                            float area);

/** Poiseuille flow rate: Q = pi*R^4*dP / (8*mu*L) */
float fd_poiseuille_flow_rate(float radius, float pressure_drop,
                               float viscosity, float length);

/** Mach number: M = v / c_sound */
float fd_mach_number(float velocity, float speed_of_sound);

/** Get velocity at grid point */
void fd_get_velocity_at(const fluid_dynamics_sim_t* sim,
                         uint32_t ix, uint32_t iy, uint32_t iz,
                         float* vx, float* vy, float* vz);

/** Get pressure at grid point */
float fd_get_pressure_at(const fluid_dynamics_sim_t* sim,
                          uint32_t ix, uint32_t iy, uint32_t iz);

/* Legacy types map to new types */
typedef fd_config_t fluid_dynamics_config_t;
typedef fd_stats_t  fluid_dynamics_stats_t;

/* Legacy API compatibility */
fluid_dynamics_sim_t* fluid_dynamics_create(const fluid_dynamics_config_t* config);
void fluid_dynamics_destroy(fluid_dynamics_sim_t* sim);
int fluid_dynamics_step(fluid_dynamics_sim_t* sim, float dt);
fluid_dynamics_config_t fluid_dynamics_default_config(void);
fluid_dynamics_stats_t fluid_dynamics_get_stats(const fluid_dynamics_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FLUID_DYNAMICS_H */

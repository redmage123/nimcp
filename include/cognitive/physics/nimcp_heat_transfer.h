/**
 * @file nimcp_heat_transfer.h
 * @brief Heat Transfer simulation engine — conduction, convection, radiation
 *
 * WHAT: 3D heat conduction (Fourier's law) on a grid with convective and
 *       radiative boundary conditions, multi-material support.
 * WHY:  Enables reasoning about thermal systems, insulation, cooling,
 *       heat exchangers, thermal management, fire, cooking.
 * HOW:  Explicit Euler time integration on a finite-difference grid,
 *       material properties per cell, boundary condition enforcement.
 *
 * THEORETICAL FOUNDATION:
 *   Fourier's Law:        q = -k * grad(T)
 *   Heat Equation:        rho*Cp * dT/dt = k * laplacian(T) + Q_gen
 *   Newton's Cooling:     q = h * (T_surface - T_ambient)
 *   Stefan-Boltzmann:     q = epsilon * sigma * (T^4 - T_env^4)
 *   Thermal Resistance:   R = L / (k * A)
 *   Biot Number:          Bi = h * L_c / k
 *   Nusselt Number:       Nu = h * L / k_fluid
 *   Fin Efficiency:       eta = tanh(m*L) / (m*L),  m = sqrt(h*P/(k*A_c))
 */

#ifndef NIMCP_HEAT_TRANSFER_H
#define NIMCP_HEAT_TRANSFER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HT_STEFAN_BOLTZMANN     5.670374e-8f    /* W/(m^2*K^4) */
#define HT_BOLTZMANN_K          1.380649e-23f   /* J/K */
#define HT_ABSOLUTE_ZERO        -273.15f        /* Celsius */

/* Thermal conductivities (W/(m*K)) */
#define HT_K_COPPER             401.0f
#define HT_K_ALUMINUM           237.0f
#define HT_K_STEEL              50.2f
#define HT_K_GLASS              1.05f
#define HT_K_WOOD               0.12f
#define HT_K_AIR                0.026f
#define HT_K_WATER              0.606f
#define HT_K_CONCRETE           1.7f
#define HT_K_STYROFOAM          0.033f

/* Specific heats (J/(kg*K)) */
#define HT_CP_COPPER            385.0f
#define HT_CP_ALUMINUM          897.0f
#define HT_CP_STEEL             502.0f
#define HT_CP_WATER             4186.0f
#define HT_CP_AIR               1005.0f
#define HT_CP_GLASS             840.0f
#define HT_CP_CONCRETE          880.0f

/* Densities (kg/m^3) */
#define HT_RHO_COPPER           8960.0f
#define HT_RHO_ALUMINUM         2700.0f
#define HT_RHO_STEEL            7850.0f
#define HT_RHO_WATER            998.2f
#define HT_RHO_AIR              1.225f
#define HT_RHO_CONCRETE         2300.0f

#define HT_MAX_GRID_DIM         64
#define HT_MAX_MATERIALS        16
#define HT_MAX_HEAT_SOURCES     32

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    HT_BC_ISOTHERMAL    = 0,    /* fixed temperature */
    HT_BC_ADIABATIC     = 1,    /* zero heat flux (dT/dn = 0) */
    HT_BC_CONVECTIVE    = 2,    /* Newton's cooling: q = h*(T - T_amb) */
    HT_BC_RADIATIVE     = 3,    /* Stefan-Boltzmann: q = eps*sigma*(T^4 - T_env^4) */
    HT_BC_MIXED         = 4,    /* convective + radiative */
} ht_boundary_type_t;

typedef enum {
    HT_MAT_COPPER       = 0,
    HT_MAT_ALUMINUM     = 1,
    HT_MAT_STEEL        = 2,
    HT_MAT_GLASS        = 3,
    HT_MAT_WOOD         = 4,
    HT_MAT_WATER        = 5,
    HT_MAT_AIR          = 6,
    HT_MAT_CONCRETE     = 7,
    HT_MAT_CUSTOM       = 8,
} ht_material_type_t;

/* ============================================================================
 * Material Properties
 * ============================================================================ */

typedef struct {
    ht_material_type_t  type;
    float               conductivity;       /* k (W/(m*K)) */
    float               specific_heat;      /* Cp (J/(kg*K)) */
    float               density;            /* rho (kg/m^3) */
    float               emissivity;         /* epsilon (0-1) for radiation */
    char                name[32];
} ht_material_t;

/* ============================================================================
 * Heat Source
 * ============================================================================ */

typedef struct {
    uint32_t    ix, iy, iz;     /* grid location */
    float       power;          /* Watts (total) */
    float       power_density;  /* W/m^3 (volumetric) */
    bool        active;
} ht_heat_source_t;

/* ============================================================================
 * Boundary Condition
 * ============================================================================ */

typedef struct {
    ht_boundary_type_t  type;
    float               temperature;    /* for isothermal */
    float               h_conv;         /* convective coefficient (W/(m^2*K)) */
    float               t_ambient;      /* ambient temperature (K) */
    float               emissivity;     /* for radiative */
} ht_boundary_config_t;

/* ============================================================================
 * Scalar Field (temperature, material index)
 * ============================================================================ */

typedef struct {
    float*      data;           /* [nx * ny * nz] */
    uint32_t    nx, ny, nz;
    float       dx, dy, dz;
} ht_scalar_field_t;

typedef struct {
    uint8_t*    data;           /* [nx * ny * nz] material index */
    uint32_t    nx, ny, nz;
} ht_material_field_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t            grid_dim;
    float               cell_size;          /* meters */
    float               dt;                 /* seconds */
    float               initial_temp;       /* K */
    ht_boundary_config_t boundary[6];       /* -x, +x, -y, +y, -z, +z */
    ht_material_type_t  default_material;
    bool                enable_radiation;
    bool                enable_convection;
} ht_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       max_temperature;
    float       min_temperature;
    float       avg_temperature;
    float       total_heat_flux;        /* net flux through boundaries (W) */
    float       total_internal_energy;  /* sum rho*Cp*T*dV (J) */
    float       max_gradient;           /* max |grad(T)| */
    float       time;
} ht_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct heat_transfer_sim {
    ht_scalar_field_t   temperature;
    ht_scalar_field_t   temp_next;      /* double-buffer for explicit step */
    ht_material_field_t material_map;

    ht_material_t       materials[HT_MAX_MATERIALS];
    uint32_t            num_materials;

    ht_heat_source_t    sources[HT_MAX_HEAT_SOURCES];
    uint32_t            num_sources;

    ht_config_t         config;
    ht_stats_t          stats;
    float               time;
    bool                initialized;
} heat_transfer_sim_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

heat_transfer_sim_t* ht_create(const ht_config_t* config);
void ht_destroy(heat_transfer_sim_t* sim);
int ht_step(heat_transfer_sim_t* sim, float dt);
ht_config_t ht_default_config(void);
ht_stats_t ht_get_stats(const heat_transfer_sim_t* sim);

/* ============================================================================
 * Setup
 * ============================================================================ */

/** Register a material (returns material index) */
uint32_t ht_add_material(heat_transfer_sim_t* sim, const ht_material_t* mat);

/** Get built-in material properties */
ht_material_t ht_builtin_material(ht_material_type_t type);

/** Set material for a grid region */
void ht_set_material_region(heat_transfer_sim_t* sim,
                             uint32_t x0, uint32_t y0, uint32_t z0,
                             uint32_t x1, uint32_t y1, uint32_t z1,
                             uint32_t mat_index);

/** Add a heat source */
uint32_t ht_add_heat_source(heat_transfer_sim_t* sim, const ht_heat_source_t* src);

/** Set temperature at a point */
void ht_set_temperature(heat_transfer_sim_t* sim,
                         uint32_t ix, uint32_t iy, uint32_t iz, float temp_k);

/** Get temperature at a point */
float ht_get_temperature(const heat_transfer_sim_t* sim,
                          uint32_t ix, uint32_t iy, uint32_t iz);

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

/** Thermal resistance: R = L / (k * A) */
float ht_thermal_resistance(float length, float conductivity, float area);

/** Biot number: Bi = h * L_c / k_solid */
float ht_biot_number(float h_conv, float char_length, float conductivity);

/** Nusselt number: Nu = h * L / k_fluid */
float ht_nusselt_number(float h_conv, float length, float k_fluid);

/** Fin efficiency: eta = tanh(mL) / (mL) where m = sqrt(h*P/(k*Ac)) */
float ht_fin_efficiency(float h_conv, float perimeter, float conductivity,
                         float cross_area, float fin_length);

/** Radiative heat flux: q = eps * sigma * (T1^4 - T2^4) */
float ht_radiative_flux(float emissivity, float T1, float T2);

/** Thermal diffusivity: alpha = k / (rho * Cp) */
float ht_thermal_diffusivity(float conductivity, float density, float specific_heat);

/** Lumped capacitance time constant: tau = rho*V*Cp / (h*A) */
float ht_lumped_time_constant(float density, float volume, float specific_heat,
                               float h_conv, float surface_area);

/* Legacy API compatibility */
typedef ht_config_t heat_transfer_config_t;
typedef ht_stats_t  heat_transfer_stats_t;

heat_transfer_sim_t* heat_transfer_create(const heat_transfer_config_t* config);
void heat_transfer_destroy(heat_transfer_sim_t* sim);
int heat_transfer_step(heat_transfer_sim_t* sim, float dt);
heat_transfer_config_t heat_transfer_default_config(void);
heat_transfer_stats_t heat_transfer_get_stats(const heat_transfer_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HEAT_TRANSFER_H */

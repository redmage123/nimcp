/**
 * @file nimcp_surface_physics.h
 * @brief Surface Physics — interfacial phenomena, wetting, capillarity, tribology
 *
 * WHAT: Simulates surface tension, contact angles, capillary action, adhesion,
 *       thin film dynamics, surface waves, and heat transfer at interfaces.
 * WHY:  Enables reasoning about everyday surface phenomena: why water beads on
 *       wax, why soap breaks surface tension, why oil and water don't mix,
 *       why sandpaper is rough, why ice is slippery.
 * HOW:  Young-Laplace equation for capillary pressure, Young's equation for
 *       contact angles, Stokes drag for thin films, Fresnel equations for
 *       optical interfaces, Fourier's law for surface heat transfer.
 *
 * THEORETICAL FOUNDATION:
 *   - Young-Laplace: ΔP = γ(1/R₁ + 1/R₂)  (capillary pressure)
 *   - Young's equation: cos θ = (γ_SG - γ_SL) / γ_LG  (contact angle)
 *   - Gibbs adsorption: dγ = -Σ Γᵢ dμᵢ  (surface tension change)
 *   - Marangoni effect: τ = dγ/dx  (surface tension gradient → flow)
 *   - Fourier's law: q = -k ∇T  (heat conduction)
 *   - Stefan-Boltzmann: q = εσT⁴  (radiative transfer)
 *   - Fresnel equations: R_s, R_p  (reflection coefficients)
 *   - Coulomb friction: F = μN  (macroscopic), Amontons-Coulomb laws
 */

#ifndef NIMCP_SURFACE_PHYSICS_H
#define NIMCP_SURFACE_PHYSICS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SURF_MAX_MATERIALS      64
#define SURF_MAX_INTERFACES     128
#define SURF_MAX_DROPLETS       32
#define SURF_MAX_FILMS          16
#define SURF_STEFAN_BOLTZMANN   5.670e-8f   /* W/(m²·K⁴) */
#define SURF_WATER_TENSION      0.0728f     /* N/m at 20°C */
#define SURF_WATER_VISCOSITY    1.002e-3f   /* Pa·s at 20°C */

/* ============================================================================
 * Material Surface Properties
 * ============================================================================ */

typedef enum {
    SURF_TYPE_SOLID     = 0,
    SURF_TYPE_LIQUID    = 1,
    SURF_TYPE_GAS       = 2,
    SURF_TYPE_COUNT
} surf_phase_t;

typedef struct {
    uint32_t        id;
    char            name[32];
    surf_phase_t    phase;
    float           surface_energy;     /* J/m² (surface free energy) */
    float           surface_tension;    /* N/m (for liquids) */
    float           viscosity;          /* Pa·s (dynamic viscosity) */
    float           density;            /* kg/m³ */
    float           thermal_conductivity; /* W/(m·K) */
    float           specific_heat;      /* J/(kg·K) */
    float           emissivity;         /* [0..1] for radiation */
    float           refractive_index;   /* n (optical) */
    float           roughness;          /* Ra (arithmetic mean roughness, meters) */
    float           hardness;           /* Mohs or Vickers scale */
    bool            hydrophobic;        /* contact angle > 90° with water */
    bool            active;
} surf_material_t;

/* ============================================================================
 * Interface (contact between two materials)
 * ============================================================================ */

typedef struct {
    uint32_t        id;
    uint32_t        material_a;         /* first material index */
    uint32_t        material_b;         /* second material index */
    float           contact_angle;      /* θ in radians (Young's equation) */
    float           interfacial_energy; /* γ_AB (J/m²) */
    float           friction_static;    /* μ_s (static friction coefficient) */
    float           friction_kinetic;   /* μ_k (kinetic friction coefficient) */
    float           adhesion_energy;    /* work of adhesion (J/m²) */
    float           area;               /* contact area (m²) */
    float           temperature;        /* interface temperature (K) */
    float           heat_flux;          /* heat flow across interface (W/m²) */
    bool            active;
} surf_interface_t;

/* ============================================================================
 * Droplet (liquid on a surface)
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    wm_parietal_vec3_t  position;       /* center of mass */
    float               volume;         /* m³ */
    float               contact_radius; /* radius of contact circle (m) */
    float               contact_angle;  /* current contact angle (rad) */
    float               height;         /* droplet height (m) */
    uint32_t            liquid_id;      /* material index */
    uint32_t            surface_id;     /* material index of surface */
    float               velocity;       /* sliding velocity (m/s) */
    float               evaporation_rate; /* m³/s */
    bool                active;
} surf_droplet_t;

/* ============================================================================
 * Thin Film
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    uint32_t            material_id;
    float               thickness;      /* meters */
    float               area;           /* m² */
    wm_parietal_vec3_t  position;
    wm_parietal_vec3_t  normal;         /* surface normal */
    float               flow_velocity;  /* Marangoni or gravity-driven (m/s) */
    float               tension_gradient; /* dγ/dx (N/m²) → Marangoni flow */
    bool                active;
} surf_thin_film_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       ambient_temperature;    /* K (default: 293.15 = 20°C) */
    float       ambient_pressure;       /* Pa (default: 101325 = 1 atm) */
    float       gravity;                /* m/s² (default: 9.81) */
    bool        enable_marangoni;       /* surface tension gradient flow */
    bool        enable_evaporation;
    bool        enable_heat_transfer;
    bool        enable_optical;         /* Fresnel reflection/refraction */
} surf_phys_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       total_surface_energy;
    float       total_heat_transferred;
    float       total_evaporated;
    float       max_marangoni_velocity;
    uint32_t    active_interfaces;
    uint32_t    active_droplets;
} surf_phys_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct surface_physics_sim {
    surf_material_t     materials[SURF_MAX_MATERIALS];
    uint32_t            num_materials;
    surf_interface_t    interfaces[SURF_MAX_INTERFACES];
    uint32_t            num_interfaces;
    surf_droplet_t      droplets[SURF_MAX_DROPLETS];
    uint32_t            num_droplets;
    surf_thin_film_t    films[SURF_MAX_FILMS];
    uint32_t            num_films;
    surf_phys_config_t  config;
    surf_phys_stats_t   stats;
    float               time;
    bool                initialized;
} surface_physics_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

surface_physics_sim_t* surface_physics_create(const surf_phys_config_t* config);
void surface_physics_destroy(surface_physics_sim_t* sim);

/** Register a material (returns id) */
uint32_t surface_physics_add_material(surface_physics_sim_t* sim, const surf_material_t* mat);

/** Create an interface between two materials (computes contact angle etc.) */
uint32_t surface_physics_create_interface(surface_physics_sim_t* sim,
                                           uint32_t material_a, uint32_t material_b,
                                           float area);

/** Place a droplet on a surface */
uint32_t surface_physics_add_droplet(surface_physics_sim_t* sim,
                                      uint32_t liquid_id, uint32_t surface_id,
                                      wm_parietal_vec3_t position, float volume);

/** Add a thin film */
uint32_t surface_physics_add_film(surface_physics_sim_t* sim, const surf_thin_film_t* film);

/** Step the simulation */
int surface_physics_step(surface_physics_sim_t* sim, float dt);

/* === Queries === */

/** Young-Laplace capillary pressure: ΔP = γ(1/R₁ + 1/R₂) */
float surface_physics_capillary_pressure(float surface_tension, float R1, float R2);

/** Young's contact angle: cos θ = (γ_SG - γ_SL) / γ_LG */
float surface_physics_contact_angle(float gamma_sg, float gamma_sl, float gamma_lg);

/** Capillary rise height: h = 2γ cos θ / (ρgr) */
float surface_physics_capillary_rise(float surface_tension, float contact_angle,
                                      float density, float gravity, float tube_radius);

/** Marangoni velocity from surface tension gradient */
float surface_physics_marangoni_velocity(float tension_gradient, float viscosity,
                                          float thickness);

/** Heat transfer rate across an interface (conduction) */
float surface_physics_heat_transfer(const surface_physics_sim_t* sim,
                                     uint32_t interface_id);

/** Radiative heat flux: q = εσ(T₁⁴ - T₂⁴) */
float surface_physics_radiative_flux(float emissivity, float T1, float T2);

/** Fresnel reflectance at normal incidence: R = ((n₁-n₂)/(n₁+n₂))² */
float surface_physics_fresnel_normal(float n1, float n2);

/** Fresnel reflectance at angle (s-polarization) */
float surface_physics_fresnel_s(float n1, float n2, float angle_incidence);

/** Fresnel reflectance at angle (p-polarization) */
float surface_physics_fresnel_p(float n1, float n2, float angle_incidence);

/** Critical angle for total internal reflection: sin θ_c = n₂/n₁ */
float surface_physics_critical_angle(float n1, float n2);

/** Friction force: F = μ * N */
float surface_physics_friction_force(const surface_physics_sim_t* sim,
                                      uint32_t interface_id, float normal_force,
                                      bool kinetic);

/** Is the droplet in equilibrium? (no net forces) */
bool surface_physics_droplet_equilibrium(const surface_physics_sim_t* sim, uint32_t droplet_id);

/** Load common materials (water, glass, steel, wood, teflon, etc.) */
void surface_physics_load_common_materials(surface_physics_sim_t* sim);

/** Get stats */
surf_phys_stats_t surface_physics_get_stats(const surface_physics_sim_t* sim);

/** Default config */
surf_phys_config_t surface_physics_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_PHYSICS_H */

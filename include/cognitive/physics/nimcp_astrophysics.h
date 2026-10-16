/**
 * @file nimcp_astrophysics.h
 * @brief Astrophysics — stellar evolution, orbital mechanics, cosmology, black holes
 *
 * Kepler orbits, N-body gravity, stellar structure (main sequence, giants, WD, NS, BH),
 * Hertzsprung-Russell diagram, Hubble expansion, Friedmann equations.
 */

#ifndef NIMCP_ASTROPHYSICS_H
#define NIMCP_ASTROPHYSICS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/physics/nimcp_intuitive_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASTRO_MAX_BODIES        128
#define ASTRO_MAX_NAME          32
#define ASTRO_G                 6.674e-11f      /* m³/(kg·s²) */
#define ASTRO_C                 299792458.0f    /* m/s */
#define ASTRO_AU                1.496e11f       /* meters */
#define ASTRO_PC                3.086e16f       /* parsec in meters */
#define ASTRO_LY                9.461e15f       /* light-year in meters */
#define ASTRO_SOLAR_MASS        1.989e30f       /* kg */
#define ASTRO_SOLAR_RADIUS      6.957e8f        /* m */
#define ASTRO_SOLAR_LUMINOSITY  3.828e26f        /* W */
#define ASTRO_SOLAR_TEMP        5778.0f         /* K */
#define ASTRO_STEFAN_BOLTZMANN  5.670e-8f       /* W/(m²·K⁴) */
#define ASTRO_HUBBLE            2.2e-18f        /* H₀ ≈ 67.4 km/s/Mpc in 1/s */

typedef enum {
    ASTRO_TYPE_STAR         = 0,
    ASTRO_TYPE_PLANET       = 1,
    ASTRO_TYPE_MOON         = 2,
    ASTRO_TYPE_ASTEROID     = 3,
    ASTRO_TYPE_COMET        = 4,
    ASTRO_TYPE_BLACK_HOLE   = 5,
    ASTRO_TYPE_NEUTRON_STAR = 6,
    ASTRO_TYPE_WHITE_DWARF  = 7,
    ASTRO_TYPE_GALAXY       = 8,
} astro_body_type_t;

typedef enum {
    ASTRO_SPECTRAL_O=0, ASTRO_SPECTRAL_B, ASTRO_SPECTRAL_A, ASTRO_SPECTRAL_F,
    ASTRO_SPECTRAL_G, ASTRO_SPECTRAL_K, ASTRO_SPECTRAL_M,
} astro_spectral_class_t;

typedef enum {
    ASTRO_STAGE_PROTOSTAR=0, ASTRO_STAGE_MAIN_SEQUENCE, ASTRO_STAGE_RED_GIANT,
    ASTRO_STAGE_HORIZONTAL_BRANCH, ASTRO_STAGE_AGB, ASTRO_STAGE_PLANETARY_NEBULA,
    ASTRO_STAGE_WHITE_DWARF, ASTRO_STAGE_SUPERNOVA, ASTRO_STAGE_NEUTRON_STAR,
    ASTRO_STAGE_BLACK_HOLE,
} astro_stellar_stage_t;

typedef struct {
    uint32_t            id;
    char                name[ASTRO_MAX_NAME];
    astro_body_type_t   type;
    /* Orbital state (3D) */
    wm_parietal_vec3_t  position;       /* meters */
    wm_parietal_vec3_t  velocity;       /* m/s */
    float               mass;           /* kg */
    float               radius;         /* m */
    /* Stellar properties */
    float               luminosity;     /* W */
    float               temperature;    /* K (surface) */
    float               metallicity;    /* [Fe/H] relative to solar */
    float               age;            /* years */
    astro_spectral_class_t spectral;
    astro_stellar_stage_t stage;
    float               main_sequence_lifetime; /* years */
    /* Orbital elements (Keplerian) */
    float               semi_major_axis;/* m */
    float               eccentricity;
    float               inclination;    /* radians */
    float               orbital_period; /* seconds */
    uint32_t            parent_body;    /* orbits around this (UINT32_MAX = none) */
    /* Black hole */
    float               schwarzschild_radius; /* 2GM/c² */
    float               spin;           /* Kerr parameter a/M [0..1] */
    bool                active;
} astro_body_t;

typedef struct {
    float       dt;                 /* seconds */
    float       softening_length;   /* gravitational softening (m) */
    bool        enable_stellar_evolution;
    bool        enable_cosmology;
    float       hubble_constant;    /* 1/s */
} astro_config_t;

typedef struct {
    uint64_t    step_count;
    float       total_kinetic_energy;
    float       total_potential_energy;
    float       total_energy;
    float       energy_drift;
    double      initial_total_energy;
    uint32_t    active_bodies;
    float       max_velocity;
} astro_stats_t;

typedef struct astrophysics_sim {
    astro_body_t    bodies[ASTRO_MAX_BODIES];
    uint32_t        num_bodies;
    astro_config_t  config;
    astro_stats_t   stats;
    double          time;           /* seconds (double for cosmological timescales) */
    bool            initialized;
} astrophysics_sim_t;

astrophysics_sim_t* astrophysics_create(const astro_config_t* config);
void astrophysics_destroy(astrophysics_sim_t* sim);
uint32_t astrophysics_add_body(astrophysics_sim_t* sim, const astro_body_t* body);
int astrophysics_step(astrophysics_sim_t* sim, float dt);

/** Kepler's third law: T² = 4π²a³/(GM) */
float astro_orbital_period(float semi_major_axis, float central_mass);
/** Orbital velocity: v = sqrt(GM/r) (circular) */
float astro_orbital_velocity(float central_mass, float radius);
/** Escape velocity: v_esc = sqrt(2GM/r) */
float astro_escape_velocity(float mass, float radius);
/** Schwarzschild radius: r_s = 2GM/c² */
float astro_schwarzschild_radius(float mass);
/** Gravitational time dilation: dt_proper/dt_coord = sqrt(1 - r_s/r) */
float astro_gravitational_time_dilation(float mass, float distance);
/** Tidal force: F_tidal = 2GMmr/d³ */
float astro_tidal_force(float M, float m, float r, float d);
/** Stellar luminosity: L = 4πR²σT⁴ (Stefan-Boltzmann) */
float astro_luminosity(float radius, float temperature);
/** Main sequence lifetime: t = t_sun · (M/M_sun)^(-2.5) */
float astro_main_sequence_lifetime(float mass);
/** Hubble's law: v = H₀ · d */
float astro_hubble_velocity(float distance, float H0);
/** Cosmological redshift: 1+z = a(t_obs)/a(t_emit) */
float astro_redshift_from_velocity(float velocity);
/** Apparent magnitude from absolute: m = M + 5·log₁₀(d/10pc) */
float astro_apparent_magnitude(float absolute_mag, float distance_pc);
/** Hill sphere radius: r_H = a·(m/(3M))^(1/3) */
float astro_hill_radius(float semi_major, float m_body, float M_central);
/** Roche limit: d = R·(2·ρ_M/ρ_m)^(1/3) */
float astro_roche_limit(float R_primary, float rho_primary, float rho_secondary);

/** Load Solar System (Sun + 8 planets) */
void astrophysics_load_solar_system(astrophysics_sim_t* sim);
/** Load binary star system */
void astrophysics_load_binary_star(astrophysics_sim_t* sim, float m1, float m2, float separation);

astro_config_t astrophysics_default_config(void);
astro_stats_t astrophysics_get_stats(const astrophysics_sim_t* sim);

#ifdef __cplusplus
}
#endif
#endif

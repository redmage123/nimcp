/**
 * @file nimcp_optics.h
 * @brief Optics simulation engine — ray tracing, lenses, diffraction
 *
 * WHAT: Geometric and wave optics: ray tracing through optical elements
 *       (lenses, mirrors, prisms), Snell's law refraction, Fresnel reflection,
 *       thin lens equation, diffraction gratings, polarization.
 * WHY:  Enables reasoning about vision, cameras, telescopes, microscopes,
 *       fiber optics, lasers, rainbows, color, shadows.
 * HOW:  Sequential ray tracing through surfaces, analytic thin lens/mirror
 *       equations, diffraction/interference formulas.
 *
 * THEORETICAL FOUNDATION:
 *   Snell's Law:         n1*sin(theta1) = n2*sin(theta2)
 *   Thin Lens:           1/f = 1/d_o + 1/d_i
 *   Mirror:              1/f = 1/d_o + 1/d_i,  f = R/2
 *   Magnification:       M = -d_i / d_o
 *   Fresnel (s-pol):     Rs = ((n1*cos_i - n2*cos_t)/(n1*cos_i + n2*cos_t))^2
 *   Fresnel (p-pol):     Rp = ((n2*cos_i - n1*cos_t)/(n2*cos_i + n1*cos_t))^2
 *   Diffraction Grating: d*sin(theta) = m*lambda
 *   Single Slit:         a*sin(theta) = m*lambda  (minima)
 *   Malus's Law:         I = I_0 * cos^2(theta)
 *   Brewster's Angle:    tan(theta_B) = n2/n1
 *   Critical Angle:      sin(theta_c) = n2/n1  (n1 > n2)
 */

#ifndef NIMCP_OPTICS_H
#define NIMCP_OPTICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define OPT_SPEED_OF_LIGHT      2.998e8f        /* m/s */
#define OPT_PLANCK              6.626e-34f       /* J*s */
#define OPT_PLANCK_EV           4.136e-15f       /* eV*s */

/* Refractive indices */
#define OPT_N_VACUUM            1.0f
#define OPT_N_AIR               1.000293f
#define OPT_N_WATER             1.333f
#define OPT_N_GLASS             1.52f
#define OPT_N_DIAMOND           2.417f
#define OPT_N_FUSED_SILICA      1.458f
#define OPT_N_CROWN_GLASS       1.523f
#define OPT_N_FLINT_GLASS       1.62f
#define OPT_N_SAPPHIRE          1.77f
#define OPT_N_ICE               1.31f

/* Wavelengths (meters) */
#define OPT_LAMBDA_RED          700e-9f
#define OPT_LAMBDA_GREEN        550e-9f
#define OPT_LAMBDA_BLUE         450e-9f
#define OPT_LAMBDA_UV           300e-9f
#define OPT_LAMBDA_IR           1000e-9f

#define OPT_MAX_RAYS            512
#define OPT_MAX_SURFACES        64
#define OPT_MAX_TRACE_DEPTH     32

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    OPT_SURFACE_FLAT        = 0,
    OPT_SURFACE_SPHERICAL   = 1,    /* lens/mirror */
    OPT_SURFACE_GRATING     = 2,    /* diffraction grating */
    OPT_SURFACE_SLIT        = 3,    /* single/double slit */
} opt_surface_type_t;

typedef enum {
    OPT_INTERACT_REFRACT    = 0,    /* transparent surface */
    OPT_INTERACT_REFLECT    = 1,    /* mirror */
    OPT_INTERACT_BOTH       = 2,    /* Fresnel: partial reflect + refract */
} opt_interaction_t;

typedef enum {
    OPT_POLARIZATION_NONE   = 0,
    OPT_POLARIZATION_H      = 1,    /* horizontal (s) */
    OPT_POLARIZATION_V      = 2,    /* vertical (p) */
    OPT_POLARIZATION_CIRC   = 3,    /* circular */
} opt_polarization_t;

/* ============================================================================
 * Ray
 * ============================================================================ */

typedef struct {
    float       ox, oy, oz;     /* origin */
    float       dx, dy, dz;     /* direction (normalized) */
    float       wavelength;     /* meters */
    float       intensity;      /* W/m^2 (or relative) */
    opt_polarization_t polarization;
    float       pol_angle;      /* polarization angle (radians) */
    uint32_t    bounces;        /* number of interactions so far */
    bool        active;
} opt_ray_t;

/* ============================================================================
 * Optical Surface
 * ============================================================================ */

typedef struct {
    opt_surface_type_t  type;
    opt_interaction_t   interaction;
    /* Position: plane at z = position, normal along z */
    float               position;       /* z coordinate of surface */
    float               center_x, center_y;
    float               aperture;       /* radius of clear aperture */
    /* Lens/mirror */
    float               radius_of_curvature; /* positive = center behind surface */
    float               n_before;       /* refractive index before surface */
    float               n_after;        /* refractive index after surface */
    float               reflectivity;   /* 0-1 for mirrors */
    /* Grating */
    float               grating_spacing;/* d (meters between lines) */
    int32_t             grating_order;  /* m (diffraction order) */
    /* Slit */
    float               slit_width;     /* a (meters) */
    float               slit_separation;/* for double slit */
    bool                active;
} opt_surface_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    uint32_t    max_trace_depth;    /* max bounces per ray */
    float       min_intensity;      /* below this, ray terminates */
    float       default_wavelength; /* meters */
    float       ambient_n;          /* ambient refractive index */
    bool        enable_fresnel;     /* compute Fresnel coefficients */
    bool        enable_polarization;
    bool        enable_dispersion;  /* wavelength-dependent n */
} opt_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    uint32_t    rays_traced;
    uint32_t    total_reflections;
    uint32_t    total_refractions;
    uint32_t    total_internal_reflections;
    float       total_intensity_in;
    float       total_intensity_out;
    float       avg_path_length;
} opt_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct optics_sim {
    opt_ray_t       rays[OPT_MAX_RAYS];
    uint32_t        num_rays;

    opt_surface_t   surfaces[OPT_MAX_SURFACES];
    uint32_t        num_surfaces;

    opt_config_t    config;
    opt_stats_t     stats;
    bool            initialized;
} optics_sim_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

optics_sim_t* opt_create(const opt_config_t* config);
void opt_destroy(optics_sim_t* sim);

/** Trace all active rays through surfaces (one step = one surface interaction) */
int opt_step(optics_sim_t* sim, float dt);

/** Trace all rays to completion (through all surfaces) */
int opt_trace_all(optics_sim_t* sim);

opt_config_t opt_default_config(void);
opt_stats_t opt_get_stats(const optics_sim_t* sim);

/* ============================================================================
 * Setup
 * ============================================================================ */

/** Add a ray */
uint32_t opt_add_ray(optics_sim_t* sim, const opt_ray_t* ray);

/** Add an optical surface */
uint32_t opt_add_surface(optics_sim_t* sim, const opt_surface_t* surface);

/** Create a thin lens (two surfaces, returns index of first) */
uint32_t opt_add_thin_lens(optics_sim_t* sim, float position, float focal_length,
                            float aperture, float n_lens);

/** Create a mirror */
uint32_t opt_add_mirror(optics_sim_t* sim, float position, float radius_curvature,
                          float aperture);

/** Create a diffraction grating */
uint32_t opt_add_grating(optics_sim_t* sim, float position, float line_spacing,
                           int32_t order, float aperture);

/** Generate a fan of rays from a point */
void opt_generate_ray_fan(optics_sim_t* sim, float ox, float oy, float oz,
                           float target_z, float fan_half_angle,
                           uint32_t num_rays, float wavelength);

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

/** Snell's law: returns sin(theta_t), or -1 if total internal reflection */
float opt_snell_refraction(float n1, float n2, float sin_theta_i);

/** Fresnel reflectance (s-polarization) */
float opt_fresnel_rs(float n1, float n2, float cos_theta_i, float cos_theta_t);

/** Fresnel reflectance (p-polarization) */
float opt_fresnel_rp(float n1, float n2, float cos_theta_i, float cos_theta_t);

/** Average Fresnel reflectance (unpolarized) */
float opt_fresnel_average(float n1, float n2, float cos_theta_i);

/** Thin lens equation: 1/f = 1/do + 1/di. Returns image distance. */
float opt_thin_lens_image(float focal_length, float object_distance);

/** Mirror equation: returns image distance (same as thin lens) */
float opt_mirror_image(float focal_length, float object_distance);

/** Magnification: M = -di/do */
float opt_magnification(float object_distance, float image_distance);

/** Lensmaker's equation: 1/f = (n-1)[1/R1 - 1/R2] */
float opt_lensmaker(float n_lens, float R1, float R2);

/** Diffraction grating angle: d*sin(theta) = m*lambda */
float opt_grating_angle(float grating_spacing, int32_t order, float wavelength);

/** Single slit diffraction minima: a*sin(theta) = m*lambda */
float opt_slit_minimum_angle(float slit_width, int32_t order, float wavelength);

/** Malus's law: I = I0 * cos^2(theta) */
float opt_malus_law(float I0, float angle);

/** Brewster's angle: theta_B = atan(n2/n1) */
float opt_brewster_angle(float n1, float n2);

/** Critical angle for total internal reflection */
float opt_critical_angle(float n1, float n2);

/** Wavelength to approximate RGB */
void opt_wavelength_to_rgb(float wavelength, float* r, float* g, float* b);

/* Legacy API */
typedef opt_config_t optics_config_t;
typedef opt_stats_t  optics_stats_t;

optics_sim_t* optics_create(const optics_config_t* config);
void optics_destroy(optics_sim_t* sim);
int optics_step(optics_sim_t* sim, float dt);
optics_config_t optics_default_config(void);
optics_stats_t optics_get_stats(const optics_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OPTICS_H */

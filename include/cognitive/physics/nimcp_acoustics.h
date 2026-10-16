/**
 * @file nimcp_acoustics.h
 * @brief Acoustics & Wave Mechanics simulation engine
 *
 * WHAT: Wave equation solver on 1D/2D grids with point/line/plane sources,
 *       reflections, absorption, resonance, Doppler effects.
 * WHY:  Enables reasoning about sound, music, speech, sonar, ultrasound,
 *       room acoustics, noise control, musical instruments.
 * HOW:  Finite-difference time-domain (FDTD) solution of the wave equation,
 *       leapfrog integration, absorbing boundary conditions.
 *
 * THEORETICAL FOUNDATION:
 *   Wave Equation:      d^2p/dt^2 = c^2 * laplacian(p)
 *   Sound Intensity:    I = p^2 / (rho * c)
 *   Decibels:           L = 20 * log10(p / p_ref),  p_ref = 20 uPa
 *   Doppler Shift:      f' = f * (c + v_obs) / (c + v_src)
 *   Standing Wave:      f_n = n * c / (2L)  (open-open pipe)
 *   Impedance:          Z = rho * c
 *   Reflection Coeff:   R = (Z2 - Z1) / (Z2 + Z1)
 *   Resonant String:    f_n = n / (2L) * sqrt(T / mu)
 */

#ifndef NIMCP_ACOUSTICS_H
#define NIMCP_ACOUSTICS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define AC_SPEED_AIR_20C        343.0f          /* m/s at 20C */
#define AC_SPEED_WATER          1482.0f         /* m/s at 20C */
#define AC_SPEED_STEEL          5960.0f         /* m/s */
#define AC_AIR_DENSITY          1.225f          /* kg/m^3 */
#define AC_WATER_DENSITY        998.2f          /* kg/m^3 */
#define AC_REF_PRESSURE         2.0e-5f         /* 20 uPa reference for dB SPL */
#define AC_REF_INTENSITY        1.0e-12f        /* W/m^2 reference for dB SIL */
#define AC_IMPEDANCE_AIR        420.5f          /* rho*c for air (Pa*s/m) */
#define AC_IMPEDANCE_WATER      1.48e6f         /* rho*c for water */

#define AC_MAX_GRID_DIM         256             /* per axis for 1D/2D */
#define AC_MAX_SOURCES          32
#define AC_MAX_MEDIA            8

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    AC_DIM_1D           = 1,
    AC_DIM_2D           = 2,
} ac_dimension_t;

typedef enum {
    AC_BC_RIGID         = 0,    /* dp/dn = 0 (hard wall) */
    AC_BC_OPEN          = 1,    /* p = 0 (pressure release) */
    AC_BC_ABSORBING     = 2,    /* Mur first-order ABC */
    AC_BC_PERIODIC      = 3,
} ac_boundary_type_t;

typedef enum {
    AC_SRC_POINT        = 0,
    AC_SRC_LINE         = 1,
    AC_SRC_PLANE        = 2,
} ac_source_type_t;

typedef enum {
    AC_WAVE_SINE        = 0,
    AC_WAVE_GAUSSIAN    = 1,    /* Gaussian pulse */
    AC_WAVE_IMPULSE     = 2,
} ac_waveform_t;

/* ============================================================================
 * Sound Source
 * ============================================================================ */

typedef struct {
    ac_source_type_t    type;
    ac_waveform_t       waveform;
    float               x, y;           /* position (grid units) */
    float               frequency;      /* Hz */
    float               amplitude;      /* Pa */
    float               phase;          /* radians */
    float               velocity_x;     /* for Doppler (m/s) */
    float               velocity_y;
    bool                active;
} ac_source_t;

/* ============================================================================
 * Medium Properties
 * ============================================================================ */

typedef struct {
    float               speed;          /* speed of sound (m/s) */
    float               density;        /* kg/m^3 */
    float               absorption;     /* attenuation coefficient (Np/m) */
    char                name[32];
} ac_medium_t;

/* ============================================================================
 * Pressure Field
 * ============================================================================ */

typedef struct {
    float*      p_curr;         /* current pressure */
    float*      p_prev;         /* previous step pressure (leapfrog) */
    uint32_t    nx, ny;
    float       dx, dy;
} ac_field_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    ac_dimension_t      dimension;
    uint32_t            grid_nx;
    uint32_t            grid_ny;        /* 1 for 1D */
    float               cell_size;      /* meters */
    float               dt;             /* seconds */
    float               speed_of_sound; /* m/s */
    float               medium_density; /* kg/m^3 */
    ac_boundary_type_t  boundary;
    float               absorption_coeff;
    bool                enable_doppler;
} ac_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       max_pressure;
    float       min_pressure;
    float       rms_pressure;
    float       total_energy;           /* sum p^2 / (rho*c^2) * dV */
    float       max_spl_db;            /* max sound pressure level */
    float       time;
} ac_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct acoustics_sim {
    ac_field_t          field;
    uint8_t*            medium_map;     /* per-cell medium index */

    ac_source_t         sources[AC_MAX_SOURCES];
    uint32_t            num_sources;

    ac_medium_t         media[AC_MAX_MEDIA];
    uint32_t            num_media;

    ac_config_t         config;
    ac_stats_t          stats;
    float               time;
    bool                initialized;
} acoustics_sim_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

acoustics_sim_t* ac_create(const ac_config_t* config);
void ac_destroy(acoustics_sim_t* sim);
int ac_step(acoustics_sim_t* sim, float dt);
ac_config_t ac_default_config(void);
ac_stats_t ac_get_stats(const acoustics_sim_t* sim);

/* ============================================================================
 * Setup
 * ============================================================================ */

/** Add a sound source */
uint32_t ac_add_source(acoustics_sim_t* sim, const ac_source_t* src);

/** Register a medium */
uint32_t ac_add_medium(acoustics_sim_t* sim, const ac_medium_t* medium);

/** Init vibrating string (1D standing wave) */
void ac_init_vibrating_string(acoustics_sim_t* sim, float tension,
                               float linear_density, float length);

/** Init organ pipe resonance */
void ac_init_organ_pipe(acoustics_sim_t* sim, float pipe_length, bool open_open);

/** Init 2D room acoustics */
void ac_init_room(acoustics_sim_t* sim, float width, float height,
                   float wall_absorption);

/** Get pressure at grid point */
float ac_get_pressure(const acoustics_sim_t* sim, uint32_t ix, uint32_t iy);

/* ============================================================================
 * Physics Computations
 * ============================================================================ */

/** Sound intensity: I = p_rms^2 / (rho * c) */
float ac_sound_intensity(float pressure_rms, float density, float speed);

/** Pressure to decibels: L = 20*log10(p/p_ref) */
float ac_pressure_to_db(float pressure);

/** Intensity to decibels: L = 10*log10(I/I_ref) */
float ac_intensity_to_db(float intensity);

/** Doppler shifted frequency: f' = f*(c + v_obs)/(c + v_src) */
float ac_doppler_shift(float frequency, float speed_of_sound,
                        float v_observer, float v_source);

/** Resonant frequencies of pipe: f_n = n*c/(2L) or (2n-1)*c/(4L) */
float ac_pipe_resonance(float speed_of_sound, float length,
                         uint32_t harmonic, bool open_both_ends);

/** Resonant frequency of string: f_n = n/(2L)*sqrt(T/mu) */
float ac_string_resonance(float tension, float linear_density,
                           float length, uint32_t harmonic);

/** Standing wave node positions (returns number of nodes, fills array) */
uint32_t ac_standing_wave_nodes(float length, uint32_t harmonic,
                                 float* node_positions, uint32_t max_nodes);

/** Acoustic impedance: Z = rho * c */
float ac_impedance(float density, float speed);

/** Reflection coefficient: R = (Z2 - Z1) / (Z2 + Z1) */
float ac_reflection_coefficient(float Z1, float Z2);

/** Transmission coefficient: T = 2*Z2 / (Z2 + Z1) */
float ac_transmission_coefficient(float Z1, float Z2);

/* Legacy API */
typedef ac_config_t acoustics_config_t;
typedef ac_stats_t  acoustics_stats_t;

acoustics_sim_t* acoustics_create(const acoustics_config_t* config);
void acoustics_destroy(acoustics_sim_t* sim);
int acoustics_step(acoustics_sim_t* sim, float dt);
acoustics_config_t acoustics_default_config(void);
acoustics_stats_t acoustics_get_stats(const acoustics_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ACOUSTICS_H */

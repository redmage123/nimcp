/**
 * @file nimcp_analytical_chemistry.h
 * @brief Analytical Chemistry simulation engine for world model
 *
 * Titration curves (strong/weak acid-base), Henderson-Hasselbalch,
 * buffer capacity, Van Deemter chromatography, Beer-Lambert,
 * calibration/LOD, Nernst potentiometry.
 */

#ifndef NIMCP_ANALYTICAL_CHEMISTRY_H
#define NIMCP_ANALYTICAL_CHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Limits
 * ============================================================================ */

#define ANACHEM_MAX_SOLUTIONS       32
#define ANACHEM_MAX_TITRATIONS      16
#define ANACHEM_MAX_COLUMNS         8
#define ANACHEM_MAX_CALIBRATION_PTS 32
#define ANACHEM_MAX_NAME            48
#define ANACHEM_MAX_COMPONENTS      8

/* ============================================================================
 * Physical constants
 * ============================================================================ */

#define ANACHEM_R_GAS           8.314f      /* J/(mol*K) */
#define ANACHEM_FARADAY         96485.0f    /* C/mol */
#define ANACHEM_KW              1.0e-14f    /* water autoionization at 25C */
#define ANACHEM_LN10            2.302585f

/* Common buffer pKa values at 25C */
#define ANACHEM_PKA_PHOSPHATE1  2.15f       /* H3PO4 -> H2PO4- */
#define ANACHEM_PKA_PHOSPHATE2  7.20f       /* H2PO4- -> HPO4^2- */
#define ANACHEM_PKA_PHOSPHATE3  12.35f      /* HPO4^2- -> PO4^3- */
#define ANACHEM_PKA_TRIS        8.07f       /* Tris base */
#define ANACHEM_PKA_HEPES       7.55f       /* HEPES */
#define ANACHEM_PKA_ACETIC      4.76f       /* acetic acid */
#define ANACHEM_PKA_CARBONIC1   6.35f       /* H2CO3 -> HCO3- */
#define ANACHEM_PKA_CARBONIC2   10.33f      /* HCO3- -> CO3^2- */
#define ANACHEM_PKA_CITRIC1     3.13f
#define ANACHEM_PKA_CITRIC2     4.76f
#define ANACHEM_PKA_CITRIC3     6.40f

/* Standard reduction potentials (V vs SHE) */
#define ANACHEM_E0_AG           0.7996f     /* Ag+/Ag */
#define ANACHEM_E0_CU           0.3419f     /* Cu2+/Cu */
#define ANACHEM_E0_ZN          -0.7618f     /* Zn2+/Zn */
#define ANACHEM_E0_H2           0.0000f     /* H+/H2 (reference) */
#define ANACHEM_E0_FE2         -0.447f      /* Fe2+/Fe */
#define ANACHEM_E0_FE3          0.771f      /* Fe3+/Fe2+ */

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    ANACHEM_ACID_STRONG = 0,
    ANACHEM_ACID_WEAK,
    ANACHEM_BASE_STRONG,
    ANACHEM_BASE_WEAK,
    ANACHEM_BUFFER,
    ANACHEM_AMPHOTERIC,
    ANACHEM_SOL_TYPE_COUNT
} anachem_solution_type_t;

typedef enum {
    ANACHEM_TITRATION_SA_SB = 0,    /* strong acid + strong base */
    ANACHEM_TITRATION_WA_SB,        /* weak acid + strong base */
    ANACHEM_TITRATION_SA_WB,        /* strong acid + weak base */
    ANACHEM_TITRATION_WA_WB,        /* weak acid + weak base */
    ANACHEM_TITRATION_REDOX,        /* redox titration */
    ANACHEM_TITRATION_TYPE_COUNT
} anachem_titration_type_t;

typedef enum {
    ANACHEM_BUFFER_PHOSPHATE = 0,
    ANACHEM_BUFFER_TRIS,
    ANACHEM_BUFFER_HEPES,
    ANACHEM_BUFFER_ACETATE,
    ANACHEM_BUFFER_CARBONATE,
    ANACHEM_BUFFER_CITRATE,
    ANACHEM_BUFFER_ID_COUNT
} anachem_buffer_id_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

typedef struct {
    uint32_t                id;
    char                    name[ANACHEM_MAX_NAME];
    anachem_solution_type_t type;
    float                   concentration;  /* mol/L */
    float                   volume;         /* L */
    float                   pKa;            /* for weak acid/base */
    float                   pH;             /* current pH */
    float                   ionic_strength; /* mol/L */
    bool                    active;
} anachem_solution_t;

typedef struct {
    uint32_t                id;
    anachem_titration_type_t type;
    uint32_t                analyte_idx;    /* index into solutions */
    uint32_t                titrant_idx;
    float                   volume_added;   /* mL of titrant added */
    float                   equivalence_vol;/* mL at equivalence point */
    float                   current_pH;
    float                   endpoint_pH;
    bool                    past_equivalence;
    bool                    active;
} anachem_titration_t;

/* Van Deemter chromatography column */
typedef struct {
    uint32_t                id;
    char                    name[ANACHEM_MAX_NAME];
    float                   A;              /* eddy diffusion term, cm */
    float                   B;              /* longitudinal diffusion, cm^2/s */
    float                   C;              /* mass transfer, s */
    float                   length;         /* column length, cm */
    float                   diameter;       /* column diameter, cm */
    float                   particle_size;  /* dp, um */
    float                   flow_rate;      /* linear velocity u, cm/s */
    float                   plate_height;   /* H = A + B/u + C*u, cm */
    uint32_t                theoretical_plates; /* N = L/H */
    float                   retention_factor;   /* k' */
    float                   selectivity;        /* alpha */
    float                   resolution;         /* Rs */
    bool                    active;
} anachem_column_t;

/* Beer-Lambert spectrophotometry */
typedef struct {
    float                   absorbance;     /* A (unitless) */
    float                   epsilon;        /* molar absorptivity, L/(mol*cm) */
    float                   path_length;    /* l, cm */
    float                   concentration;  /* c, mol/L */
    float                   transmittance;  /* T = 10^(-A) */
    float                   wavelength;     /* nm */
} anachem_beer_lambert_t;

/* Calibration curve (linear regression) */
typedef struct {
    float                   x[ANACHEM_MAX_CALIBRATION_PTS];
    float                   y[ANACHEM_MAX_CALIBRATION_PTS];
    uint32_t                n_points;
    float                   slope;
    float                   intercept;
    float                   r_squared;
    float                   std_error;
    float                   lod;            /* limit of detection = 3*sigma/slope */
    float                   loq;            /* limit of quantitation = 10*sigma/slope */
} anachem_calibration_t;

/* Nernst equation for potentiometry */
typedef struct {
    float                   E0;             /* standard potential, V */
    float                   E;              /* measured potential, V */
    int                     n_electrons;
    float                   Q;              /* reaction quotient */
    float                   temperature;    /* K */
} anachem_nernst_t;

typedef struct {
    float                   dt;
    float                   temperature;    /* K */
    bool                    enabled;
} analytical_chemistry_config_t;

typedef struct {
    uint64_t                step_count;
    float                   total_energy;
    float                   max_value;
    uint32_t                titrations_completed;
    float                   avg_resolution;
} analytical_chemistry_stats_t;

typedef struct analytical_chemistry_sim {
    anachem_solution_t      solutions[ANACHEM_MAX_SOLUTIONS];
    uint32_t                num_solutions;
    anachem_titration_t     titrations[ANACHEM_MAX_TITRATIONS];
    uint32_t                num_titrations;
    anachem_column_t        columns[ANACHEM_MAX_COLUMNS];
    uint32_t                num_columns;
    anachem_calibration_t   calibrations[ANACHEM_MAX_COLUMNS];
    uint32_t                num_calibrations;
    analytical_chemistry_config_t config;
    analytical_chemistry_stats_t  stats;
    float                   time;
    bool                    initialized;
} analytical_chemistry_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

analytical_chemistry_sim_t*    analytical_chemistry_create(const analytical_chemistry_config_t* config);
void                           analytical_chemistry_destroy(analytical_chemistry_sim_t* sim);
int                            analytical_chemistry_step(analytical_chemistry_sim_t* sim, float dt);
analytical_chemistry_config_t  analytical_chemistry_default_config(void);
analytical_chemistry_stats_t   analytical_chemistry_get_stats(const analytical_chemistry_sim_t* sim);

/* Entity management */
uint32_t analytical_chemistry_add_solution(analytical_chemistry_sim_t* sim, const anachem_solution_t* s);
uint32_t analytical_chemistry_add_titration(analytical_chemistry_sim_t* sim, const anachem_titration_t* t);
uint32_t analytical_chemistry_add_column(analytical_chemistry_sim_t* sim, const anachem_column_t* c);

/* Load common buffers (phosphate, Tris, HEPES, acetate, carbonate, citrate) */
int analytical_chemistry_load_common_buffers(analytical_chemistry_sim_t* sim);

/* pH calculations */
float analytical_chemistry_strong_acid_pH(float concentration);
float analytical_chemistry_strong_base_pH(float concentration);
float analytical_chemistry_weak_acid_pH(float concentration, float pKa);
float analytical_chemistry_weak_base_pH(float concentration, float pKb);

/* Henderson-Hasselbalch: pH = pKa + log([A-]/[HA]) */
float analytical_chemistry_henderson_hasselbalch(float pKa, float conc_base, float conc_acid);

/* Buffer capacity: beta = 2.303 * C * Ka * [H+] / (Ka + [H+])^2 */
float analytical_chemistry_buffer_capacity(float total_conc, float Ka, float H_conc);

/* Titration pH at given volume of titrant added */
float analytical_chemistry_titration_pH(const anachem_titration_t* tit,
                                         const anachem_solution_t* analyte,
                                         const anachem_solution_t* titrant,
                                         float vol_added_mL);

/* Van Deemter: H = A + B/u + Cu */
float analytical_chemistry_van_deemter(float A, float B, float C, float u);

/* Optimal flow rate: u_opt = sqrt(B/C) */
float analytical_chemistry_optimal_flow(float B, float C);

/* Resolution: Rs = sqrt(N)/4 * (alpha-1)/alpha * k'/(1+k') */
float analytical_chemistry_resolution(uint32_t N, float alpha, float k_prime);

/* Beer-Lambert: A = epsilon * l * c */
anachem_beer_lambert_t analytical_chemistry_beer_lambert(float epsilon, float path_length, float concentration);

/* Linear calibration from data points */
int analytical_chemistry_calibrate(anachem_calibration_t* cal);

/* Nernst equation: E = E0 - (RT/nF) * ln(Q) */
float analytical_chemistry_nernst(float E0, int n, float Q, float temperature);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANALYTICAL_CHEMISTRY_H */

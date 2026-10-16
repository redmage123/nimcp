/**
 * @file nimcp_inorganic_chemistry.h
 * @brief Inorganic Chemistry simulation engine for world model
 *
 * Crystal field theory, d-orbital splitting, CFSE, spectrochemical series,
 * HSAB theory, coordination geometries, Jahn-Teller distortion,
 * magnetic moment, Irving-Williams stability series.
 */

#ifndef NIMCP_INORGANIC_CHEMISTRY_H
#define NIMCP_INORGANIC_CHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Limits
 * ============================================================================ */

#define INOCHEM_MAX_METALS          32
#define INOCHEM_MAX_LIGANDS         32
#define INOCHEM_MAX_COMPLEXES       32
#define INOCHEM_MAX_NAME            48
#define INOCHEM_MAX_COORD           6       /* max coordination number for arrays */
#define INOCHEM_SPECTRO_SERIES_LEN  12      /* spectrochemical series entries */

/* ============================================================================
 * Physical constants
 * ============================================================================ */

#define INOCHEM_BOHR_MAGNETON   9.274e-24f  /* J/T */
#define INOCHEM_R_GAS           8.314f      /* J/(mol*K) */

/* Typical 10Dq values in cm^-1 for common ligands with Cr3+ as reference */
#define INOCHEM_DQ_IODIDE       7500.0f
#define INOCHEM_DQ_BROMIDE      8500.0f
#define INOCHEM_DQ_CHLORIDE     13000.0f
#define INOCHEM_DQ_FLUORIDE     15000.0f
#define INOCHEM_DQ_HYDROXIDE    16000.0f
#define INOCHEM_DQ_WATER        17400.0f
#define INOCHEM_DQ_AMMONIA      21500.0f
#define INOCHEM_DQ_EN           21900.0f    /* ethylenediamine */
#define INOCHEM_DQ_NITRITE      23000.0f    /* NO2- N-bonded */
#define INOCHEM_DQ_CYANIDE      26600.0f
#define INOCHEM_DQ_CO           29000.0f
#define INOCHEM_DQ_NO_PLUS      30000.0f    /* NO+ */

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    INOCHEM_GEOM_LINEAR = 0,       /* CN=2 */
    INOCHEM_GEOM_TRIGONAL_PLANAR,  /* CN=3 */
    INOCHEM_GEOM_TETRAHEDRAL,      /* CN=4 */
    INOCHEM_GEOM_SQUARE_PLANAR,    /* CN=4, d8 */
    INOCHEM_GEOM_TRIG_BIPYRAMIDAL, /* CN=5 */
    INOCHEM_GEOM_SQUARE_PYRAMIDAL, /* CN=5 */
    INOCHEM_GEOM_OCTAHEDRAL,       /* CN=6 */
    INOCHEM_GEOM_COUNT
} inochem_geometry_t;

typedef enum {
    INOCHEM_HSAB_HARD = 0,
    INOCHEM_HSAB_BORDERLINE,
    INOCHEM_HSAB_SOFT
} inochem_hsab_class_t;

typedef enum {
    INOCHEM_SPIN_LOW = 0,
    INOCHEM_SPIN_HIGH
} inochem_spin_state_t;

typedef enum {
    /* Spectrochemical series order (weak field to strong field) */
    INOCHEM_LIG_IODIDE = 0,
    INOCHEM_LIG_BROMIDE,
    INOCHEM_LIG_CHLORIDE,
    INOCHEM_LIG_FLUORIDE,
    INOCHEM_LIG_HYDROXIDE,
    INOCHEM_LIG_WATER,
    INOCHEM_LIG_AMMONIA,
    INOCHEM_LIG_EN,             /* ethylenediamine */
    INOCHEM_LIG_NITRITE,        /* NO2- */
    INOCHEM_LIG_CYANIDE,
    INOCHEM_LIG_CO,
    INOCHEM_LIG_NO_PLUS,
    INOCHEM_LIG_COUNT
} inochem_ligand_id_t;

/* Common first-row transition metals */
typedef enum {
    INOCHEM_METAL_TI = 0,  /* Ti(III) d1 */
    INOCHEM_METAL_V,       /* V(III) d2 */
    INOCHEM_METAL_CR,      /* Cr(III) d3 */
    INOCHEM_METAL_MN2,     /* Mn(II) d5 */
    INOCHEM_METAL_MN3,     /* Mn(III) d4 */
    INOCHEM_METAL_FE2,     /* Fe(II) d6 */
    INOCHEM_METAL_FE3,     /* Fe(III) d5 */
    INOCHEM_METAL_CO2,     /* Co(II) d7 */
    INOCHEM_METAL_CO3,     /* Co(III) d6 */
    INOCHEM_METAL_NI,      /* Ni(II) d8 */
    INOCHEM_METAL_CU2,     /* Cu(II) d9 */
    INOCHEM_METAL_ZN,      /* Zn(II) d10 */
    INOCHEM_METAL_ID_COUNT
} inochem_metal_id_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    char                name[INOCHEM_MAX_NAME];
    inochem_metal_id_t  type;
    int                 oxidation_state;
    int                 d_electrons;
    float               ionic_radius;       /* pm */
    float               electronegativity;  /* Pauling */
    inochem_hsab_class_t hsab;
    /* Irving-Williams relative stability (arb units, higher = more stable) */
    float               irving_williams;
    bool                active;
} inochem_metal_t;

typedef struct {
    uint32_t            id;
    char                name[INOCHEM_MAX_NAME];
    inochem_ligand_id_t type;
    float               field_strength;     /* 10Dq contribution, cm^-1 */
    int                 denticity;          /* mono=1, bi=2, hexa=6 */
    int                 charge;
    inochem_hsab_class_t hsab;
    bool                is_pi_acceptor;
    bool                is_pi_donor;
    bool                active;
} inochem_ligand_t;

typedef struct {
    uint32_t            id;
    char                name[INOCHEM_MAX_NAME];
    uint32_t            metal_idx;          /* index into metals array */
    uint32_t            ligand_indices[INOCHEM_MAX_COORD];
    uint32_t            num_ligands;
    inochem_geometry_t  geometry;
    inochem_spin_state_t spin_state;
    float               delta_oct;          /* crystal field splitting, cm^-1 */
    float               cfse;               /* crystal field stabilization energy, Dq units */
    float               magnetic_moment;    /* mu_eff in Bohr magnetons */
    int                 unpaired_electrons;
    int                 total_charge;
    bool                jahn_teller;        /* Jahn-Teller active? */
    bool                jahn_teller_strong; /* strong distortion? */
    float               stability_constant; /* log K_f */
    bool                active;
} inochem_complex_t;

typedef struct {
    float               dt;
    float               temperature;        /* K */
    bool                enabled;
} inorganic_chemistry_config_t;

typedef struct {
    uint64_t            step_count;
    float               total_energy;
    float               max_value;
    float               avg_cfse;
    float               avg_magnetic_moment;
} inorganic_chemistry_stats_t;

typedef struct inorganic_chemistry_sim {
    inochem_metal_t     metals[INOCHEM_MAX_METALS];
    uint32_t            num_metals;
    inochem_ligand_t    ligands[INOCHEM_MAX_LIGANDS];
    uint32_t            num_ligands;
    inochem_complex_t   complexes[INOCHEM_MAX_COMPLEXES];
    uint32_t            num_complexes;
    /* Spectrochemical series: ordered field strengths */
    float               spectro_series[INOCHEM_SPECTRO_SERIES_LEN];
    inorganic_chemistry_config_t config;
    inorganic_chemistry_stats_t  stats;
    float               time;
    bool                initialized;
} inorganic_chemistry_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

inorganic_chemistry_sim_t*    inorganic_chemistry_create(const inorganic_chemistry_config_t* config);
void                          inorganic_chemistry_destroy(inorganic_chemistry_sim_t* sim);
int                           inorganic_chemistry_step(inorganic_chemistry_sim_t* sim, float dt);
inorganic_chemistry_config_t  inorganic_chemistry_default_config(void);
inorganic_chemistry_stats_t   inorganic_chemistry_get_stats(const inorganic_chemistry_sim_t* sim);

/* Entity management */
uint32_t inorganic_chemistry_add_metal(inorganic_chemistry_sim_t* sim, const inochem_metal_t* m);
uint32_t inorganic_chemistry_add_ligand(inorganic_chemistry_sim_t* sim, const inochem_ligand_t* l);
uint32_t inorganic_chemistry_add_complex(inorganic_chemistry_sim_t* sim, const inochem_complex_t* c);

/* Load common complexes and spectrochemical series */
int inorganic_chemistry_load_common_complexes(inorganic_chemistry_sim_t* sim);

/* Crystal field splitting: Delta_oct for given metal + ligand set */
float inorganic_chemistry_compute_delta_oct(const inochem_metal_t* metal,
                                             const inochem_ligand_t* ligands,
                                             uint32_t num_ligands);

/* CFSE = (-0.4 * t2g + 0.6 * eg) * Delta, returns in Dq units */
float inorganic_chemistry_compute_cfse(int d_electrons, inochem_spin_state_t spin,
                                        inochem_geometry_t geom);

/* Determine spin state from d-electron count and Delta */
inochem_spin_state_t inorganic_chemistry_predict_spin(int d_electrons, float delta_oct,
                                                       float pairing_energy);

/* Magnetic moment: mu = sqrt(n*(n+2)) BM */
float inorganic_chemistry_magnetic_moment(int unpaired_electrons);

/* Unpaired electrons for given d count, spin state, geometry */
int inorganic_chemistry_unpaired_electrons(int d_electrons, inochem_spin_state_t spin,
                                            inochem_geometry_t geom);

/* Jahn-Teller distortion check */
bool inorganic_chemistry_is_jahn_teller(int d_electrons, inochem_spin_state_t spin,
                                         inochem_geometry_t geom);

/* HSAB stability prediction: returns relative log K_f */
float inorganic_chemistry_hsab_stability(inochem_hsab_class_t acid, inochem_hsab_class_t base);

/* Predict geometry from d-electron count and coordination number */
inochem_geometry_t inorganic_chemistry_predict_geometry(int d_electrons, int coord_number);

/* Compute full complex properties (delta, CFSE, spin, JT, mu) */
int inorganic_chemistry_analyze_complex(inorganic_chemistry_sim_t* sim, uint32_t complex_idx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INORGANIC_CHEMISTRY_H */

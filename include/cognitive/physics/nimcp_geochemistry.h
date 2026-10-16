/**
 * @file nimcp_geochemistry.h
 * @brief Geochemistry simulation engine for world model
 *
 * Eh-pH (Pourbaix) diagrams, weathering (Arrhenius), carbon cycle reservoirs,
 * ocean carbonate system, isotope fractionation, Goldschmidt classification,
 * Stokes sedimentation.
 */

#ifndef NIMCP_GEOCHEMISTRY_H
#define NIMCP_GEOCHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Limits
 * ============================================================================ */

#define GEOCHEM_MAX_MINERALS        32
#define GEOCHEM_MAX_RESERVOIRS      16
#define GEOCHEM_MAX_ISOTOPES        16
#define GEOCHEM_MAX_NAME            48
#define GEOCHEM_MAX_FLUXES          32

/* ============================================================================
 * Physical constants
 * ============================================================================ */

#define GEOCHEM_R_GAS           8.314f      /* J/(mol*K) */
#define GEOCHEM_FARADAY         96485.0f    /* C/mol */
#define GEOCHEM_LN10            2.302585f
#define GEOCHEM_G               9.81f       /* m/s^2 */
#define GEOCHEM_PI              3.14159265f

/* Ocean carbonate equilibrium constants at 25C */
#define GEOCHEM_KA1_CARBONIC    4.3e-7f     /* H2CO3 -> HCO3- + H+ */
#define GEOCHEM_KA2_CARBONIC    4.7e-11f    /* HCO3- -> CO3^2- + H+ */
#define GEOCHEM_KH_CO2          3.4e-2f     /* Henry's law, mol/(L*atm) */
#define GEOCHEM_KSP_CALCITE     3.3e-9f     /* CaCO3 solubility product */
#define GEOCHEM_KW              1.0e-14f    /* water autoionization */

/* Mineral-specific activation energies for weathering (kJ/mol) */
#define GEOCHEM_EA_QUARTZ       87.7f
#define GEOCHEM_EA_FELDSPAR     67.7f       /* plagioclase */
#define GEOCHEM_EA_OLIVINE      79.0f
#define GEOCHEM_EA_CALCITE      41.8f
#define GEOCHEM_EA_PYRITE       56.9f
#define GEOCHEM_EA_KAOLINITE    62.8f

/* Weathering rate constants (mol/m^2/s at 25C, neutral pH) */
#define GEOCHEM_K0_QUARTZ       1.0e-14f
#define GEOCHEM_K0_FELDSPAR     1.0e-12f
#define GEOCHEM_K0_OLIVINE      1.0e-10f
#define GEOCHEM_K0_CALCITE      1.0e-6f
#define GEOCHEM_K0_PYRITE       1.0e-9f

/* Earth carbon reservoirs (GtC = gigatons carbon) */
#define GEOCHEM_C_ATMOSPHERE    870.0f      /* ~870 GtC (2024) */
#define GEOCHEM_C_OCEAN_SURFACE 900.0f      /* surface ocean */
#define GEOCHEM_C_OCEAN_DEEP    37100.0f    /* deep ocean */
#define GEOCHEM_C_BIOSPHERE     550.0f      /* living biomass */
#define GEOCHEM_C_SOIL          1500.0f     /* soil organic */
#define GEOCHEM_C_FOSSIL        10000.0f    /* fossil fuels (est) */
#define GEOCHEM_C_LITHOSPHERE   75000000.0f /* carbonate rocks */

/* Annual carbon fluxes (GtC/yr) */
#define GEOCHEM_FLUX_PHOTOSYNTHESIS  120.0f
#define GEOCHEM_FLUX_RESPIRATION     120.0f
#define GEOCHEM_FLUX_OCEAN_UPTAKE    92.0f
#define GEOCHEM_FLUX_OCEAN_RELEASE   90.0f
#define GEOCHEM_FLUX_FOSSIL_BURN     9.5f   /* anthropogenic */
#define GEOCHEM_FLUX_LAND_USE        1.5f   /* deforestation */
#define GEOCHEM_FLUX_VOLCANISM       0.1f
#define GEOCHEM_FLUX_WEATHERING      0.3f

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    GEOCHEM_CLASS_LITHOPHILE = 0,   /* affinity for silicate/oxide */
    GEOCHEM_CLASS_SIDEROPHILE,      /* affinity for iron/metal */
    GEOCHEM_CLASS_CHALCOPHILE,      /* affinity for sulfide */
    GEOCHEM_CLASS_ATMOPHILE,        /* volatile / gaseous */
    GEOCHEM_CLASS_COUNT
} geochem_goldschmidt_t;

typedef enum {
    GEOCHEM_MIN_QUARTZ = 0,
    GEOCHEM_MIN_FELDSPAR,
    GEOCHEM_MIN_OLIVINE,
    GEOCHEM_MIN_CALCITE,
    GEOCHEM_MIN_PYRITE,
    GEOCHEM_MIN_KAOLINITE,
    GEOCHEM_MIN_MAGNETITE,
    GEOCHEM_MIN_HEMATITE,
    GEOCHEM_MIN_GYPSUM,
    GEOCHEM_MIN_DOLOMITE,
    GEOCHEM_MIN_COUNT
} geochem_mineral_id_t;

typedef enum {
    GEOCHEM_RES_ATMOSPHERE = 0,
    GEOCHEM_RES_OCEAN_SURFACE,
    GEOCHEM_RES_OCEAN_DEEP,
    GEOCHEM_RES_BIOSPHERE,
    GEOCHEM_RES_SOIL,
    GEOCHEM_RES_FOSSIL,
    GEOCHEM_RES_LITHOSPHERE,
    GEOCHEM_RES_COUNT
} geochem_reservoir_id_t;

typedef enum {
    GEOCHEM_ISO_C13 = 0,        /* 13C/12C */
    GEOCHEM_ISO_O18,            /* 18O/16O */
    GEOCHEM_ISO_S34,            /* 34S/32S */
    GEOCHEM_ISO_H2,             /* 2H/1H (deuterium) */
    GEOCHEM_ISO_N15,            /* 15N/14N */
    GEOCHEM_ISO_SR87,           /* 87Sr/86Sr */
    GEOCHEM_ISO_COUNT
} geochem_isotope_id_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

typedef struct {
    uint32_t            id;
    char                name[GEOCHEM_MAX_NAME];
    geochem_mineral_id_t type;
    float               activation_energy;  /* Ea, kJ/mol */
    float               rate_constant_25C;  /* k0 at 25C, mol/m^2/s */
    float               current_rate;       /* computed weathering rate */
    float               surface_area;       /* m^2 */
    float               mass;               /* kg */
    float               density;            /* kg/m^3 */
    float               particle_radius;    /* m, for Stokes settling */
    geochem_goldschmidt_t goldschmidt;
    /* Eh-pH stability */
    float               eh_min;             /* min Eh for stability, V */
    float               eh_max;
    float               ph_min;             /* min pH for stability */
    float               ph_max;
    bool                active;
} geochem_mineral_t;

typedef struct {
    uint32_t            id;
    char                name[GEOCHEM_MAX_NAME];
    geochem_reservoir_id_t type;
    float               carbon_mass;        /* GtC */
    float               initial_mass;       /* GtC at t=0 */
    /* Fluxes: flux_to[i] = rate of transfer to reservoir i (GtC/yr) */
    float               flux_to[GEOCHEM_MAX_RESERVOIRS];
    uint32_t            num_connections;
    float               temperature;        /* K (affects dissolution) */
    float               pH;                 /* for aqueous reservoirs */
    float               eh;                 /* redox potential, V */
    bool                active;
} geochem_reservoir_t;

typedef struct {
    uint32_t            id;
    char                name[GEOCHEM_MAX_NAME];
    geochem_isotope_id_t type;
    float               ratio;              /* R = heavy/light */
    float               delta;              /* delta = (R_sample/R_std - 1)*1000 permil */
    float               standard_ratio;     /* R_standard (e.g., VPDB for C13) */
    float               fractionation_alpha;/* alpha = R_product / R_reactant */
} geochem_isotope_t;

/* Ocean carbonate system state */
typedef struct {
    float               pCO2;               /* atmospheric CO2, atm */
    float               DIC;                /* dissolved inorganic carbon, mol/L */
    float               H2CO3;              /* carbonic acid, mol/L */
    float               HCO3;              /* bicarbonate, mol/L */
    float               CO3;               /* carbonate, mol/L */
    float               pH;
    float               alkalinity;         /* total alkalinity, eq/L */
    float               omega_calcite;      /* saturation state */
    float               temperature;        /* K */
} geochem_carbonate_state_t;

/* Stokes settling result */
typedef struct {
    float               velocity;           /* terminal velocity, m/s */
    float               reynolds;           /* particle Reynolds number */
    float               particle_radius;    /* m */
    float               particle_density;   /* kg/m^3 */
    float               fluid_density;      /* kg/m^3 */
    float               fluid_viscosity;    /* Pa*s */
} geochem_stokes_result_t;

typedef struct {
    float               dt;                 /* timestep in years */
    float               temperature;        /* K */
    float               atmospheric_co2;    /* ppm */
    bool                enable_anthropogenic;
    bool                enabled;
} geochemistry_config_t;

typedef struct {
    uint64_t            step_count;
    float               total_energy;
    float               max_value;
    float               total_carbon;       /* sum of all reservoirs */
    float               atmospheric_co2;    /* current ppm */
    float               ocean_pH;
} geochemistry_stats_t;

typedef struct geochemistry_sim {
    geochem_mineral_t       minerals[GEOCHEM_MAX_MINERALS];
    uint32_t                num_minerals;
    geochem_reservoir_t     reservoirs[GEOCHEM_MAX_RESERVOIRS];
    uint32_t                num_reservoirs;
    geochem_isotope_t       isotopes[GEOCHEM_MAX_ISOTOPES];
    uint32_t                num_isotopes;
    geochem_carbonate_state_t carbonate;
    geochemistry_config_t   config;
    geochemistry_stats_t    stats;
    float                   time;           /* years */
    bool                    initialized;
} geochemistry_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

geochemistry_sim_t*     geochemistry_create(const geochemistry_config_t* config);
void                    geochemistry_destroy(geochemistry_sim_t* sim);
int                     geochemistry_step(geochemistry_sim_t* sim, float dt);
geochemistry_config_t   geochemistry_default_config(void);
geochemistry_stats_t    geochemistry_get_stats(const geochemistry_sim_t* sim);

/* Entity management */
uint32_t geochemistry_add_mineral(geochemistry_sim_t* sim, const geochem_mineral_t* m);
uint32_t geochemistry_add_reservoir(geochemistry_sim_t* sim, const geochem_reservoir_t* r);
uint32_t geochemistry_add_isotope(geochemistry_sim_t* sim, const geochem_isotope_t* iso);

/* Load Earth's carbon cycle */
int geochemistry_load_earth_carbon_cycle(geochemistry_sim_t* sim);

/* Weathering: k = k0 * exp(-Ea / (R*T)) */
float geochemistry_weathering_rate(float k0, float Ea, float temperature);

/* Eh-pH (Nernst at mineral surface): E = E0 - (RT/nF)*ln(Q) */
float geochemistry_nernst_eh(float E0, int n_electrons, float Q, float temperature);

/* Check mineral stability at given Eh, pH */
bool geochemistry_mineral_stable(const geochem_mineral_t* mineral, float Eh, float pH);

/* Ocean carbonate system: compute speciation from pCO2 and temperature */
int geochemistry_compute_carbonate(geochem_carbonate_state_t* state);

/* Isotope fractionation: delta_product = delta_reactant + 1000*(alpha - 1) */
float geochemistry_isotope_fractionate(float delta_reactant, float alpha);

/* Goldschmidt classification lookup */
geochem_goldschmidt_t geochemistry_classify_element(const char* symbol);

/* Stokes settling: v = 2/9 * (rho_p - rho_f) * g * r^2 / mu */
geochem_stokes_result_t geochemistry_stokes_settling(float particle_radius,
                                                      float particle_density,
                                                      float fluid_density,
                                                      float fluid_viscosity);

/* Carbon cycle step: update reservoir masses from fluxes */
int geochemistry_carbon_cycle_step(geochemistry_sim_t* sim, float dt_years);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GEOCHEMISTRY_H */

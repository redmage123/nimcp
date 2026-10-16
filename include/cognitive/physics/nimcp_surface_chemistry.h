/**
 * @file nimcp_surface_chemistry.h
 * @brief Surface Chemistry — adsorption, catalysis, corrosion, electrochemistry
 *
 * WHAT: Simulates chemical processes at interfaces: adsorption/desorption,
 *       heterogeneous catalysis, corrosion, electrode reactions, self-assembly.
 * WHY:  Enables reasoning about rust forming on iron, catalytic converters,
 *       batteries charging, soap cleaning grease, paint drying, food cooking.
 * HOW:  Langmuir/BET adsorption isotherms, Arrhenius surface kinetics,
 *       Butler-Volmer electrode kinetics, Nernst equation.
 *
 * THEORETICAL FOUNDATION:
 *   - Langmuir isotherm: θ = KP/(1+KP)  (monolayer coverage)
 *   - BET isotherm: multilayer extension for porous surfaces
 *   - Arrhenius: k = A·exp(-Ea/RT)  (temperature-dependent rate)
 *   - Nernst equation: E = E° - (RT/nF)·ln(Q)  (electrode potential)
 *   - Butler-Volmer: j = j₀[exp(αnFη/RT) - exp(-(1-α)nFη/RT)]
 *   - Fick's first law: J = -D·∂C/∂x  (diffusion to surface)
 *   - Gibbs adsorption: Γ = -(1/RT)·(∂γ/∂ln c)  (surfactant adsorption)
 */

#ifndef NIMCP_SURFACE_CHEMISTRY_H
#define NIMCP_SURFACE_CHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SCHEM_MAX_ADSORBATES    32
#define SCHEM_MAX_SURFACES      16
#define SCHEM_MAX_REACTIONS     64
#define SCHEM_MAX_ELECTRODES    8
#define SCHEM_MAX_NAME_LEN      32
#define SCHEM_GAS_CONSTANT      8.314f      /* J/(mol·K) */
#define SCHEM_FARADAY           96485.0f    /* C/mol */
#define SCHEM_AVOGADRO          6.022e23f

/* ============================================================================
 * Adsorbate Species
 * ============================================================================ */

typedef struct {
    uint32_t    id;
    char        name[SCHEM_MAX_NAME_LEN];   /* "O2", "CO", "H2O", etc. */
    float       molar_mass;                 /* g/mol */
    float       partial_pressure;           /* Pa (for gas-phase) */
    float       concentration;              /* mol/L (for solution-phase) */
    float       diffusion_coefficient;      /* m²/s */
    float       adsorption_energy;          /* kJ/mol (binding to surface) */
    bool        active;
} schem_adsorbate_t;

/* ============================================================================
 * Catalytic Surface
 * ============================================================================ */

typedef enum {
    SCHEM_SURF_METAL        = 0,    /* Pt, Pd, Fe, Cu, etc. */
    SCHEM_SURF_OXIDE        = 1,    /* TiO2, Al2O3, etc. */
    SCHEM_SURF_CARBON       = 2,    /* graphene, activated carbon */
    SCHEM_SURF_POLYMER      = 3,    /* Teflon, nylon, etc. */
    SCHEM_SURF_BIOLOGICAL   = 4,    /* enzyme, cell membrane */
    SCHEM_SURF_COUNT
} schem_surface_type_t;

typedef struct {
    uint32_t            id;
    char                name[SCHEM_MAX_NAME_LEN];
    schem_surface_type_t type;
    float               area;               /* m² */
    float               site_density;       /* active sites per m² */
    float               temperature;        /* K */
    /* Adsorption state: coverage θ ∈ [0,1] for each adsorbate */
    float               coverage[SCHEM_MAX_ADSORBATES];
    /* Langmuir equilibrium constants per adsorbate */
    float               K_langmuir[SCHEM_MAX_ADSORBATES];
    /* Catalytic activity factor (1.0 = ideal, <1.0 = poisoned) */
    float               activity;
    float               poison_level;       /* [0..1] fraction of sites blocked */
    bool                active;
} schem_surface_t;

/* ============================================================================
 * Surface Reaction
 * ============================================================================ */

typedef enum {
    SCHEM_RXN_LANGMUIR_HINSHELWOOD = 0, /* both reactants adsorbed */
    SCHEM_RXN_ELEY_RIDEAL          = 1, /* one adsorbed, one gas-phase */
    SCHEM_RXN_MARS_VAN_KREVELEN    = 2, /* lattice oxygen mechanism */
    SCHEM_RXN_CORROSION            = 3, /* metal dissolution */
    SCHEM_RXN_ELECTROCHEMICAL      = 4, /* electrode reaction */
    SCHEM_RXN_DISSOLUTION          = 5, /* solid dissolving */
    SCHEM_RXN_PRECIPITATION        = 6, /* solid forming from solution */
    SCHEM_RXN_POLYMERIZATION       = 7, /* surface-initiated chain growth */
} schem_rxn_mechanism_t;

typedef struct {
    uint32_t                id;
    char                    name[SCHEM_MAX_NAME_LEN];
    schem_rxn_mechanism_t   mechanism;
    uint32_t                surface_id;
    /* Reactants (adsorbate indices + stoichiometry) */
    uint32_t                reactant_ids[4];
    float                   reactant_coeffs[4];
    uint32_t                num_reactants;
    /* Products */
    uint32_t                product_ids[4];
    float                   product_coeffs[4];
    uint32_t                num_products;
    /* Arrhenius kinetics */
    float                   pre_exponential;    /* A (1/s or m²/(mol·s)) */
    float                   activation_energy;  /* Ea (kJ/mol) */
    float                   rate;               /* current reaction rate (mol/(m²·s)) */
    /* Coverage dependence */
    float                   coverage_order[4];  /* reaction order in coverage */
    bool                    active;
} schem_reaction_t;

/* ============================================================================
 * Electrode (for electrochemistry)
 * ============================================================================ */

typedef struct {
    uint32_t    id;
    char        name[SCHEM_MAX_NAME_LEN];   /* "anode", "cathode" */
    uint32_t    surface_id;                 /* which surface is the electrode */
    float       standard_potential;         /* E° (V) */
    float       current_potential;          /* E (V) — changes during operation */
    float       overpotential;              /* η = E - E_eq (V) */
    float       exchange_current_density;   /* j₀ (A/m²) */
    float       transfer_coefficient;       /* α (typically 0.5) */
    uint32_t    electrons_transferred;      /* n in the half-reaction */
    float       current_density;            /* j (A/m²) */
    float       charge_transferred;         /* total Coulombs */
    bool        active;
} schem_electrode_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       temperature;            /* K (default: 298.15 = 25°C) */
    float       pressure;               /* Pa (default: 101325) */
    float       dt;                     /* time step (seconds) */
    bool        enable_catalysis;
    bool        enable_corrosion;
    bool        enable_electrochemistry;
    bool        enable_diffusion;       /* mass transport to surface */
} schem_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       total_adsorbed;         /* total moles adsorbed */
    float       total_desorbed;
    float       total_reacted;          /* total moles converted by catalysis */
    float       total_corroded;         /* moles of metal dissolved */
    float       total_charge;           /* Coulombs transferred (electrochemistry) */
    float       max_coverage;           /* highest surface coverage */
    float       max_reaction_rate;
} schem_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct surface_chemistry_sim {
    schem_adsorbate_t   adsorbates[SCHEM_MAX_ADSORBATES];
    uint32_t            num_adsorbates;
    schem_surface_t     surfaces[SCHEM_MAX_SURFACES];
    uint32_t            num_surfaces;
    schem_reaction_t    reactions[SCHEM_MAX_REACTIONS];
    uint32_t            num_reactions;
    schem_electrode_t   electrodes[SCHEM_MAX_ELECTRODES];
    uint32_t            num_electrodes;
    schem_config_t      config;
    schem_stats_t       stats;
    float               time;
    bool                initialized;
} surface_chemistry_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

surface_chemistry_sim_t* surface_chemistry_create(const schem_config_t* config);
void surface_chemistry_destroy(surface_chemistry_sim_t* sim);

/** Add an adsorbate species */
uint32_t surface_chemistry_add_adsorbate(surface_chemistry_sim_t* sim,
                                          const schem_adsorbate_t* ads);

/** Add a catalytic surface */
uint32_t surface_chemistry_add_surface(surface_chemistry_sim_t* sim,
                                        const schem_surface_t* surf);

/** Add a surface reaction */
uint32_t surface_chemistry_add_reaction(surface_chemistry_sim_t* sim,
                                         const schem_reaction_t* rxn);

/** Add an electrode for electrochemistry */
uint32_t surface_chemistry_add_electrode(surface_chemistry_sim_t* sim,
                                          const schem_electrode_t* electrode);

/** Step the simulation (adsorption + reactions + electrochemistry) */
int surface_chemistry_step(surface_chemistry_sim_t* sim, float dt);

/* === Isotherm Queries === */

/** Langmuir coverage: θ = KP/(1+KP) */
float surface_chemistry_langmuir(float K, float pressure);

/** BET multilayer coverage: V = Vm·c·P / [(Ps-P)(1+(c-1)P/Ps)] */
float surface_chemistry_bet(float Vm, float c, float P, float Ps);

/** Freundlich isotherm: θ = K·P^(1/n) */
float surface_chemistry_freundlich(float K, float P, float n);

/* === Kinetics === */

/** Arrhenius rate constant: k = A·exp(-Ea/RT) */
float surface_chemistry_arrhenius(float A, float Ea, float T);

/** Turnover frequency: TOF = rate / site_density */
float surface_chemistry_tof(float rate, float site_density);

/* === Electrochemistry === */

/** Nernst equation: E = E° - (RT/nF)·ln(Q) */
float surface_chemistry_nernst(float E0, float T, uint32_t n, float Q);

/** Butler-Volmer current density: j = j₀[exp(αnFη/RT) - exp(-(1-α)nFη/RT)] */
float surface_chemistry_butler_volmer(float j0, float alpha, uint32_t n,
                                       float eta, float T);

/** Tafel slope: b = 2.303·RT/(α·n·F) */
float surface_chemistry_tafel_slope(float alpha, uint32_t n, float T);

/* === Corrosion === */

/** Corrosion rate from current density: CR = j·M/(n·F·ρ) (m/s) */
float surface_chemistry_corrosion_rate(float current_density, float molar_mass,
                                        uint32_t electrons, float density);

/** Pilling-Bedworth ratio: PBR = V_oxide / V_metal */
float surface_chemistry_pilling_bedworth(float oxide_molar_volume,
                                          float metal_molar_volume,
                                          float stoich_ratio);

/* === Preloaded Systems === */

/** Load common adsorbates (O2, CO, H2O, N2, CO2, H2, CH4, NH3) */
void surface_chemistry_load_common_adsorbates(surface_chemistry_sim_t* sim);

/** Load a platinum catalyst surface with CO oxidation reaction */
void surface_chemistry_load_pt_catalyst(surface_chemistry_sim_t* sim);

/** Load an iron corrosion system (Fe + O2 + H2O → rust) */
void surface_chemistry_load_iron_corrosion(surface_chemistry_sim_t* sim);

/** Load a zinc-copper battery (Daniell cell) */
void surface_chemistry_load_daniell_cell(surface_chemistry_sim_t* sim);

/** Get stats */
schem_stats_t surface_chemistry_get_stats(const surface_chemistry_sim_t* sim);

/** Default config */
schem_config_t surface_chemistry_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_CHEMISTRY_H */

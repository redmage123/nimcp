/**
 * @file nimcp_polymer_chemistry.h
 * @brief Polymer Chemistry simulation engine for world model
 *
 * Chain polymerization kinetics, step-growth (Carothers), MW distribution,
 * glass transition (Fox), rubber elasticity, Mark-Houwink viscosity,
 * Flory-Huggins mixing, cross-linking degree.
 */

#ifndef NIMCP_POLYMER_CHEMISTRY_H
#define NIMCP_POLYMER_CHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Limits
 * ============================================================================ */

#define POLYCHEM_MAX_MONOMERS       32
#define POLYCHEM_MAX_POLYMERS       32
#define POLYCHEM_MAX_REACTIONS      64
#define POLYCHEM_MAX_NAME           48
#define POLYCHEM_MAX_COMPONENTS     8   /* for copolymers / blends */

/* ============================================================================
 * Physical constants (polymer-relevant)
 * ============================================================================ */

#define POLYCHEM_R_GAS          8.314f      /* J/(mol*K) */
#define POLYCHEM_BOLTZMANN      1.380649e-23f /* J/K */
#define POLYCHEM_AVOGADRO       6.022e23f

/* Common monomer molecular weights (g/mol) */
#define POLYCHEM_MW_ETHYLENE    28.05f
#define POLYCHEM_MW_PROPYLENE   42.08f
#define POLYCHEM_MW_STYRENE     104.15f
#define POLYCHEM_MW_PET_REPEAT  192.17f     /* PET repeat unit */
#define POLYCHEM_MW_NYLON66     226.32f     /* nylon-6,6 repeat unit */
#define POLYCHEM_MW_ISOPRENE    68.12f      /* natural rubber monomer */

/* Glass transition temperatures (K) of common homopolymers */
#define POLYCHEM_TG_PE          148.0f      /* polyethylene */
#define POLYCHEM_TG_PP          253.0f      /* polypropylene */
#define POLYCHEM_TG_PS          373.0f      /* polystyrene */
#define POLYCHEM_TG_PET         345.0f      /* PET */
#define POLYCHEM_TG_NYLON66     330.0f      /* nylon-6,6 */
#define POLYCHEM_TG_RUBBER      200.0f      /* natural rubber */

/* Mark-Houwink constants (K in mL/g, a dimensionless) */
#define POLYCHEM_MH_PE_K        0.0062f     /* PE in decalin at 135C */
#define POLYCHEM_MH_PE_A        0.70f
#define POLYCHEM_MH_PS_K        0.011f      /* PS in toluene at 25C */
#define POLYCHEM_MH_PS_A        0.725f

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    POLYCHEM_CHAIN_FREE_RADICAL = 0,
    POLYCHEM_CHAIN_CATIONIC,
    POLYCHEM_CHAIN_ANIONIC,
    POLYCHEM_CHAIN_COORDINATION,    /* Ziegler-Natta */
    POLYCHEM_STEP_CONDENSATION,
    POLYCHEM_STEP_ADDITION,
    POLYCHEM_RING_OPENING,
    POLYCHEM_RXN_TYPE_COUNT
} polychem_rxn_type_t;

typedef enum {
    POLYCHEM_MONO_ETHYLENE = 0,
    POLYCHEM_MONO_PROPYLENE,
    POLYCHEM_MONO_STYRENE,
    POLYCHEM_MONO_VINYL_CHLORIDE,
    POLYCHEM_MONO_METHYL_METHACRYLATE,
    POLYCHEM_MONO_ISOPRENE,
    POLYCHEM_MONO_BUTADIENE,
    POLYCHEM_MONO_TEREPHTHALIC_ACID,
    POLYCHEM_MONO_ETHYLENE_GLYCOL,
    POLYCHEM_MONO_HEXAMETHYLENE_DIAMINE,
    POLYCHEM_MONO_ADIPIC_ACID,
    POLYCHEM_MONO_CAPROLACTAM,
    POLYCHEM_MONO_COUNT
} polychem_monomer_id_t;

typedef enum {
    POLYCHEM_POLYMER_PE = 0,
    POLYCHEM_POLYMER_PP,
    POLYCHEM_POLYMER_PS,
    POLYCHEM_POLYMER_PVC,
    POLYCHEM_POLYMER_PMMA,
    POLYCHEM_POLYMER_PET,
    POLYCHEM_POLYMER_NYLON66,
    POLYCHEM_POLYMER_NATURAL_RUBBER,
    POLYCHEM_POLYMER_COUNT
} polychem_polymer_id_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

typedef struct {
    uint32_t                id;
    char                    name[POLYCHEM_MAX_NAME];
    polychem_monomer_id_t   type;
    float                   mw;             /* molecular weight, g/mol */
    float                   concentration;  /* mol/L */
    float                   functionality;  /* reactive groups per molecule */
    float                   tg;             /* Tg of homopolymer, K */
    bool                    active;
} polychem_monomer_t;

typedef struct {
    uint32_t                id;
    char                    name[POLYCHEM_MAX_NAME];
    polychem_polymer_id_t   type;
    float                   mn;             /* number-avg MW, g/mol */
    float                   mw;             /* weight-avg MW, g/mol */
    float                   pdi;            /* polydispersity Mw/Mn */
    float                   dp_n;           /* number-avg degree of polymerization */
    float                   conversion;     /* p, extent of reaction [0,1] */
    float                   tg;             /* glass transition, K */
    float                   intrinsic_visc; /* [eta], mL/g */
    float                   crosslink_density; /* mol crosslinks / cm^3 */
    float                   monomer_mw;     /* repeat unit MW */
    /* Mark-Houwink parameters for this polymer */
    float                   mh_K;
    float                   mh_a;
    /* Blend / copolymer composition */
    float                   weight_fractions[POLYCHEM_MAX_COMPONENTS];
    float                   tg_components[POLYCHEM_MAX_COMPONENTS];
    uint32_t                num_components;
    bool                    active;
} polychem_polymer_t;

typedef struct {
    uint32_t                id;
    polychem_rxn_type_t     type;
    uint32_t                monomer_idx;    /* index into monomers array */
    uint32_t                polymer_idx;    /* index into polymers array */
    float                   kp;             /* propagation rate, L/(mol*s) */
    float                   kt;             /* termination rate, L/(mol*s) */
    float                   ki;             /* initiation rate, 1/s */
    float                   initiator_conc; /* [I], mol/L */
    float                   f;              /* initiator efficiency [0,1] */
    float                   activation_energy; /* Ea, kJ/mol */
    bool                    active;
} polychem_reaction_t;

/* Flory-Huggins mixing result */
typedef struct {
    float                   dG_mix;         /* free energy of mixing, J/mol */
    float                   dH_mix;         /* enthalpy of mixing */
    float                   dS_mix;         /* entropy of mixing */
    float                   chi;            /* Flory-Huggins interaction parameter */
    bool                    miscible;       /* dG_mix < 0 */
} polychem_mixing_result_t;

/* Rubber elasticity result */
typedef struct {
    float                   stress;         /* sigma, Pa */
    float                   lambda;         /* extension ratio */
    float                   crosslink_density; /* n, mol/m^3 */
    float                   modulus;        /* E = 3nkT */
} polychem_rubber_result_t;

typedef struct {
    float                   dt;
    float                   temperature;    /* K */
    bool                    enable_crosslinking;
    bool                    enabled;
} polymer_chemistry_config_t;

typedef struct {
    uint64_t                step_count;
    float                   avg_mn;
    float                   avg_mw;
    float                   avg_pdi;
    float                   avg_conversion;
    float                   total_energy;
    float                   max_value;
} polymer_chemistry_stats_t;

typedef struct polymer_chemistry_sim {
    polychem_monomer_t      monomers[POLYCHEM_MAX_MONOMERS];
    uint32_t                num_monomers;
    polychem_polymer_t      polymers[POLYCHEM_MAX_POLYMERS];
    uint32_t                num_polymers;
    polychem_reaction_t     reactions[POLYCHEM_MAX_REACTIONS];
    uint32_t                num_reactions;
    polymer_chemistry_config_t config;
    polymer_chemistry_stats_t  stats;
    float                   time;
    bool                    initialized;
} polymer_chemistry_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

polymer_chemistry_sim_t*    polymer_chemistry_create(const polymer_chemistry_config_t* config);
void                        polymer_chemistry_destroy(polymer_chemistry_sim_t* sim);
int                         polymer_chemistry_step(polymer_chemistry_sim_t* sim, float dt);
polymer_chemistry_config_t  polymer_chemistry_default_config(void);
polymer_chemistry_stats_t   polymer_chemistry_get_stats(const polymer_chemistry_sim_t* sim);

/* Entity management */
uint32_t polymer_chemistry_add_monomer(polymer_chemistry_sim_t* sim, const polychem_monomer_t* m);
uint32_t polymer_chemistry_add_polymer(polymer_chemistry_sim_t* sim, const polychem_polymer_t* p);
uint32_t polymer_chemistry_add_reaction(polymer_chemistry_sim_t* sim, const polychem_reaction_t* r);

/* Load common polymer database */
int polymer_chemistry_load_common_polymers(polymer_chemistry_sim_t* sim);

/* Chain polymerization: rate = kp * [M] * [M*] */
float polymer_chemistry_chain_rate(float kp, float monomer_conc, float radical_conc);

/* Kinetic chain length: v = kp[M] / (2*kt*f*[I])^0.5 */
float polymer_chemistry_kinetic_chain_length(float kp, float monomer_conc,
                                              float kt, float f, float initiator_conc);

/* Carothers equation: DP_n = 1/(1-p) for step-growth */
float polymer_chemistry_carothers_dp(float conversion);

/* MW distribution: Mn, Mw, PDI */
void polymer_chemistry_compute_mw(polychem_polymer_t* polymer);

/* Fox equation for copolymer/blend Tg */
float polymer_chemistry_fox_tg(const float* weight_fracs, const float* tg_values, uint32_t n);

/* Rubber elasticity: sigma = nkT(lambda - 1/lambda^2) */
polychem_rubber_result_t polymer_chemistry_rubber_stress(float crosslink_density,
                                                          float temperature, float lambda);

/* Mark-Houwink: [eta] = K * M^a */
float polymer_chemistry_intrinsic_viscosity(float K, float a, float mw);

/* Flory-Huggins mixing free energy */
polychem_mixing_result_t polymer_chemistry_flory_huggins(float n1, float phi1,
                                                          float n2, float phi2,
                                                          float chi, float temperature);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POLYMER_CHEMISTRY_H */

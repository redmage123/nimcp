/**
 * @file nimcp_biochemistry.h
 * @brief Biochemistry — enzyme kinetics, metabolic pathways, signal transduction
 *
 * Michaelis-Menten kinetics, allosteric regulation, metabolic flux analysis,
 * ATP/NADH energy currency, signal cascades, cofactor dynamics.
 */

#ifndef NIMCP_BIOCHEMISTRY_H
#define NIMCP_BIOCHEMISTRY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIOCHEM_MAX_ENZYMES         64
#define BIOCHEM_MAX_METABOLITES     128
#define BIOCHEM_MAX_PATHWAYS        32
#define BIOCHEM_MAX_SIGNALS         32
#define BIOCHEM_MAX_NAME            32
#define BIOCHEM_MAX_STEPS           16
#define BIOCHEM_GAS_CONSTANT        8.314f

typedef struct {
    uint32_t    id;
    char        name[BIOCHEM_MAX_NAME];
    float       concentration;      /* mol/L */
    float       molecular_weight;   /* Da */
    float       charge;             /* net charge at pH 7 */
    bool        is_cofactor;        /* NAD+, FAD, CoA, etc. */
    bool        is_energy_carrier;  /* ATP, GTP, NADH */
    float       standard_free_energy; /* ΔG° kJ/mol */
    bool        active;
} biochem_metabolite_t;

typedef enum {
    BIOCHEM_INHIBIT_NONE        = 0,
    BIOCHEM_INHIBIT_COMPETITIVE = 1,
    BIOCHEM_INHIBIT_UNCOMPETITIVE = 2,
    BIOCHEM_INHIBIT_NONCOMPETITIVE = 3,
    BIOCHEM_INHIBIT_ALLOSTERIC  = 4,
} biochem_inhibition_t;

typedef struct {
    uint32_t    id;
    char        name[BIOCHEM_MAX_NAME];
    float       Vmax;               /* max velocity (mol/L/s) */
    float       Km;                 /* Michaelis constant (mol/L) */
    float       kcat;               /* turnover number (1/s) */
    uint32_t    substrate_id;
    uint32_t    product_id;
    uint32_t    cofactor_id;        /* UINT32_MAX if none */
    float       pH_optimum;
    float       temp_optimum;       /* K */
    /* Inhibition */
    biochem_inhibition_t inhibition_type;
    uint32_t    inhibitor_id;       /* UINT32_MAX if none */
    float       Ki;                 /* inhibition constant */
    /* Allosteric */
    uint32_t    activator_id;       /* UINT32_MAX if none */
    float       Ka;                 /* activation constant */
    float       hill_coefficient;   /* cooperativity (1 = normal, >1 = positive) */
    bool        active;
} biochem_enzyme_t;

typedef struct {
    uint32_t    id;
    char        name[BIOCHEM_MAX_NAME];     /* "glycolysis", "krebs_cycle", "ETC" */
    uint32_t    enzyme_ids[BIOCHEM_MAX_STEPS];
    uint32_t    num_enzymes;
    float       net_atp_yield;
    float       net_nadh_yield;
    float       flux;               /* mol/s through pathway */
    bool        active;
} biochem_pathway_t;

typedef struct {
    uint32_t    id;
    char        name[BIOCHEM_MAX_NAME];     /* "insulin_cascade", "MAPK" */
    uint32_t    receptor_metabolite;
    uint32_t    effector_metabolite;
    float       amplification_factor;
    float       signal_strength;    /* [0..1] current activation */
    float       decay_rate;         /* 1/s */
    bool        active;
} biochem_signal_t;

typedef struct {
    float       temperature;        /* K */
    float       pH;
    float       dt;
    bool        enable_allosteric;
    bool        enable_signal_cascades;
} biochem_config_t;

typedef struct {
    uint64_t    step_count;
    float       total_atp;
    float       total_nadh;
    float       total_flux;
    float       max_enzyme_rate;
} biochem_stats_t;

typedef struct biochemistry_sim {
    biochem_metabolite_t metabolites[BIOCHEM_MAX_METABOLITES];
    uint32_t            num_metabolites;
    biochem_enzyme_t    enzymes[BIOCHEM_MAX_ENZYMES];
    uint32_t            num_enzymes;
    biochem_pathway_t   pathways[BIOCHEM_MAX_PATHWAYS];
    uint32_t            num_pathways;
    biochem_signal_t    signals[BIOCHEM_MAX_SIGNALS];
    uint32_t            num_signals;
    biochem_config_t    config;
    biochem_stats_t     stats;
    float               time;
    bool                initialized;
} biochemistry_sim_t;

biochemistry_sim_t* biochemistry_create(const biochem_config_t* config);
void biochemistry_destroy(biochemistry_sim_t* sim);
uint32_t biochemistry_add_metabolite(biochemistry_sim_t* sim, const biochem_metabolite_t* m);
uint32_t biochemistry_add_enzyme(biochemistry_sim_t* sim, const biochem_enzyme_t* e);
uint32_t biochemistry_add_pathway(biochemistry_sim_t* sim, const biochem_pathway_t* p);
uint32_t biochemistry_add_signal(biochemistry_sim_t* sim, const biochem_signal_t* s);
int biochemistry_step(biochemistry_sim_t* sim, float dt);

/** Michaelis-Menten: v = Vmax·[S]/(Km+[S]) */
float biochemistry_michaelis_menten(float Vmax, float Km, float substrate_conc);
/** With competitive inhibitor: v = Vmax·[S]/(Km(1+[I]/Ki)+[S]) */
float biochemistry_competitive_inhibition(float Vmax, float Km, float S, float I, float Ki);
/** Hill equation: v = Vmax·[S]^n/(K^n+[S]^n) */
float biochemistry_hill_equation(float Vmax, float K, float S, float n);
/** pH dependence: activity = 1/(1+10^(pK1-pH)+10^(pH-pK2)) */
float biochemistry_ph_activity(float pH, float pK1, float pK2);
/** Temperature dependence: Arrhenius with thermal denaturation */
float biochemistry_temp_activity(float T, float T_opt, float Ea);

void biochemistry_load_glycolysis(biochemistry_sim_t* sim);
void biochemistry_load_krebs_cycle(biochemistry_sim_t* sim);
void biochemistry_load_electron_transport(biochemistry_sim_t* sim);
biochem_config_t biochemistry_default_config(void);
biochem_stats_t biochemistry_get_stats(const biochemistry_sim_t* sim);

#ifdef __cplusplus
}
#endif
#endif

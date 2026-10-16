/**
 * @file nimcp_biology_sim.h
 * @brief Biology Simulator — population dynamics, metabolism, genetics, ecosystems
 *
 * WHAT: Simulates biological systems: populations, food webs, metabolic pathways,
 *       cell growth, genetic inheritance, homeostasis, and immune responses.
 * WHY:  Provides biology prior for world model. "Plants need sunlight to grow"
 *       and "predators reduce prey populations" require biological reasoning.
 * HOW:  Lotka-Volterra dynamics for populations, metabolic network simulation,
 *       Mendelian genetics with dominance, logistic growth with carrying capacity.
 *
 * THEORETICAL FOUNDATION:
 *   - Lotka-Volterra equations: dP/dt = αP - βPQ (predator-prey)
 *   - Logistic growth: dN/dt = rN(1 - N/K)
 *   - Hardy-Weinberg equilibrium for genetics
 *   - Michaelis-Menten enzyme kinetics
 *   - Homeostatic regulation (negative feedback)
 */

#ifndef NIMCP_BIOLOGY_SIM_H
#define NIMCP_BIOLOGY_SIM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BIO_MAX_SPECIES         64
#define BIO_MAX_INTERACTIONS    128
#define BIO_MAX_GENES           32
#define BIO_MAX_METABOLITES     32
#define BIO_MAX_ORGANS          16
#define BIO_MAX_NAME_LEN        32

/* ============================================================================
 * Species / Population
 * ============================================================================ */

typedef enum {
    BIO_KINGDOM_PLANT   = 0,
    BIO_KINGDOM_ANIMAL  = 1,
    BIO_KINGDOM_FUNGI   = 2,
    BIO_KINGDOM_BACTERIA = 3,
    BIO_KINGDOM_COUNT
} bio_kingdom_t;

typedef enum {
    BIO_TROPHIC_PRODUCER    = 0,    /* plants, algae */
    BIO_TROPHIC_PRIMARY     = 1,    /* herbivores */
    BIO_TROPHIC_SECONDARY   = 2,    /* carnivores eating herbivores */
    BIO_TROPHIC_TERTIARY    = 3,    /* apex predators */
    BIO_TROPHIC_DECOMPOSER  = 4,    /* fungi, bacteria */
    BIO_TROPHIC_COUNT
} bio_trophic_level_t;

typedef struct {
    uint32_t        id;
    char            name[BIO_MAX_NAME_LEN];
    bio_kingdom_t   kingdom;
    bio_trophic_level_t trophic_level;
    /* Population dynamics */
    float           population;         /* current count */
    float           carrying_capacity;  /* K — max sustainable */
    float           growth_rate;        /* r — intrinsic growth */
    float           birth_rate;
    float           death_rate;
    /* Needs */
    float           energy_need;        /* energy units per time step */
    float           water_need;
    float           sunlight_need;      /* 0 for animals */
    float           oxygen_need;
    float           co2_need;           /* for plants */
    /* State */
    float           health;             /* [0..1] */
    float           energy_stored;
    bool            active;
} bio_species_t;

/* ============================================================================
 * Interactions (food web edges)
 * ============================================================================ */

typedef enum {
    BIO_INTERACT_PREDATION      = 0,    /* A eats B */
    BIO_INTERACT_COMPETITION    = 1,    /* A and B compete for resource */
    BIO_INTERACT_MUTUALISM      = 2,    /* both benefit */
    BIO_INTERACT_PARASITISM     = 3,    /* A benefits, B harmed */
    BIO_INTERACT_COMMENSALISM   = 4,    /* A benefits, B unaffected */
    BIO_INTERACT_POLLINATION    = 5,    /* specific mutualism */
    BIO_INTERACT_DECOMPOSITION  = 6,    /* A breaks down dead B */
    BIO_INTERACT_COUNT
} bio_interaction_type_t;

typedef struct {
    uint32_t                species_a;
    uint32_t                species_b;
    bio_interaction_type_t  type;
    float                   strength;   /* interaction coefficient */
    float                   efficiency; /* energy transfer efficiency (typ 0.1) */
    bool                    active;
} bio_interaction_t;

/* ============================================================================
 * Gene (simplified Mendelian)
 * ============================================================================ */

typedef struct {
    char        name[BIO_MAX_NAME_LEN]; /* e.g., "eye_color" */
    uint32_t    species_id;
    char        allele_dominant;        /* 'B' */
    char        allele_recessive;       /* 'b' */
    float       dominant_freq;          /* frequency of dominant allele */
} bio_gene_t;

/* ============================================================================
 * Organism body (simplified physiology)
 * ============================================================================ */

typedef struct {
    char        name[BIO_MAX_NAME_LEN]; /* "heart", "lungs", "liver" */
    float       health;                 /* [0..1] */
    float       metabolic_rate;         /* energy consumed per timestep */
    float       function_level;         /* [0..1] output capacity */
} bio_organ_t;

typedef struct {
    bio_organ_t organs[BIO_MAX_ORGANS];
    uint32_t    num_organs;
    float       body_temperature;       /* Celsius */
    float       blood_oxygen;           /* [0..1] */
    float       blood_glucose;          /* mmol/L */
    float       hydration;              /* [0..1] */
    float       immune_strength;        /* [0..1] */
} bio_body_t;

/* ============================================================================
 * Environment
 * ============================================================================ */

typedef struct {
    float       temperature;    /* Celsius */
    float       sunlight;       /* [0..1] intensity */
    float       water_available;
    float       oxygen_level;   /* [0..1] */
    float       co2_level;      /* ppm */
    float       nutrient_level; /* [0..1] soil nutrients */
    float       time_of_day;    /* 0-24 hours */
    float       season;         /* 0-1 (0=winter, 0.5=summer) */
} bio_environment_t;

/* ============================================================================
 * Violation flags
 * ============================================================================ */

typedef enum {
    BIO_VIOLATION_NONE              = 0,
    BIO_VIOLATION_NEGATIVE_POP      = (1 << 0),
    BIO_VIOLATION_EXCEED_CAPACITY   = (1 << 1),
    BIO_VIOLATION_ENERGY_FROM_NOTHING = (1 << 2),
    BIO_VIOLATION_DEAD_PREDATION    = (1 << 3),  /* dead species hunting */
    BIO_VIOLATION_PLANT_NO_LIGHT    = (1 << 4),  /* plant growing without sunlight */
    BIO_VIOLATION_IMPOSSIBLE_GROWTH = (1 << 5),  /* growth exceeds biological limits */
} bio_violation_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float       dt;                     /* time step (days) */
    float       extinction_threshold;   /* population below this = extinct */
    float       energy_transfer_eff;    /* default trophic efficiency (0.1) */
    bool        enable_genetics;
    bool        enable_physiology;      /* detailed body simulation */
} bio_config_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct biology_sim {
    bio_species_t       species[BIO_MAX_SPECIES];
    uint32_t            num_species;
    bio_interaction_t   interactions[BIO_MAX_INTERACTIONS];
    uint32_t            num_interactions;
    bio_gene_t          genes[BIO_MAX_GENES];
    uint32_t            num_genes;
    bio_body_t          body;           /* single organism physiology */
    bio_environment_t   environment;
    bio_config_t        config;
    /* Statistics */
    uint64_t            step_count;
    uint64_t            extinctions;
    float               total_biomass;
    float               total_energy;
    float               biodiversity_index; /* Shannon index */
    bool                initialized;
} biology_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

biology_sim_t* biology_sim_create(const bio_config_t* config);
void biology_sim_destroy(biology_sim_t* sim);

/** Add a species (returns species id) */
uint32_t biology_sim_add_species(biology_sim_t* sim, const bio_species_t* sp);

/** Add an interaction (food web edge) */
uint32_t biology_sim_add_interaction(biology_sim_t* sim, const bio_interaction_t* inter);

/** Add a gene for Mendelian tracking */
uint32_t biology_sim_add_gene(biology_sim_t* sim, const bio_gene_t* gene);

/** Set environment conditions */
void biology_sim_set_environment(biology_sim_t* sim, const bio_environment_t* env);

/** Step the ecosystem simulation (population dynamics + metabolism) */
int biology_sim_step(biology_sim_t* sim, float dt);

/** Step the body physiology simulation (organs, homeostasis) */
int biology_sim_step_body(biology_sim_t* sim, float dt);

/** Check violations in a predicted state */
bio_violation_t biology_sim_check_violations(const biology_sim_t* sim,
                                              const bio_species_t* predicted_species,
                                              uint32_t num_species);

/** Compute Shannon biodiversity index */
float biology_sim_biodiversity(const biology_sim_t* sim);

/** Compute total biomass */
float biology_sim_total_biomass(const biology_sim_t* sim);

/** Predict offspring genotype (Mendelian cross) */
void biology_sim_cross(const biology_sim_t* sim, uint32_t gene_id,
                        char parent1_a1, char parent1_a2,
                        char parent2_a1, char parent2_a2,
                        float offspring_probs[4], char offspring_genotypes[4][2]);

/** Load a common ecosystem (grassland, ocean, forest) */
void biology_sim_load_grassland(biology_sim_t* sim);
void biology_sim_load_ocean(biology_sim_t* sim);
void biology_sim_load_forest(biology_sim_t* sim);

/** Load standard human body physiology */
void biology_sim_load_human_body(biology_sim_t* sim);

/** Default config */
bio_config_t biology_sim_default_config(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIOLOGY_SIM_H */

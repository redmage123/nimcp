/**
 * @file nimcp_ecology.h
 * @brief Ecology simulation engine for world model
 *
 * WHAT: Simulates ecosystems: energy pyramids, nutrient cycling (N/P/C),
 *       ecological succession, island biogeography, niche theory, diversity
 *       indices, and food web stability.
 * WHY:  Provides ecological reasoning for world model. Understanding energy
 *       flow, nutrient cycles, and species interactions enables reasoning
 *       about environmental systems and biodiversity.
 * HOW:  Trophic transfer efficiency (10%), Lotka-Volterra competition,
 *       species-area relationships, Shannon/Simpson diversity, May's
 *       stability criterion, nitrogen/phosphorus/carbon cycle kinetics.
 *
 * THEORETICAL FOUNDATION:
 *   - Energy: 10% trophic transfer efficiency (Lindeman)
 *   - Species-area: S = c * A^z (z ~ 0.25 for islands)
 *   - Island biogeography: immigration = I0*(1-S/P), extinction = E0*(S/P)
 *   - Shannon: H = -sum(pi * ln(pi))
 *   - Simpson: D = 1 - sum(pi^2)
 *   - May's criterion: sqrt(SC) * sigma < 1 for stability
 *   - Lotka-Volterra competition: dN1/dt = r1*N1*(K1-N1-a12*N2)/K1
 */

#ifndef NIMCP_ECOLOGY_H
#define NIMCP_ECOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ECOL_MAX_SPECIES            64
#define ECOL_MAX_TROPHIC_LEVELS     5
#define ECOL_MAX_NUTRIENT_POOLS     8
#define ECOL_MAX_PATCHES            16
#define ECOL_MAX_NAME_LEN           32

/* Energy transfer */
#define ECOL_TROPHIC_EFFICIENCY     0.10f   /* 10% Lindeman efficiency */
#define ECOL_RESPIRATION_LOSS       0.60f   /* 60% lost to respiration */
#define ECOL_DECOMPOSITION_RATE     0.05f   /* fraction decomposed per day */

/* Species-area relationship defaults */
#define ECOL_SPECIES_AREA_C         10.0f   /* intercept constant */
#define ECOL_SPECIES_AREA_Z         0.25f   /* island exponent */
#define ECOL_SPECIES_AREA_Z_CONT    0.15f   /* continental exponent */

/* Nitrogen cycle rate constants (day^-1) */
#define ECOL_N_FIXATION_RATE        0.02f   /* N2 -> NH4+ */
#define ECOL_NITRIFICATION_RATE     0.05f   /* NH4+ -> NO2- -> NO3- */
#define ECOL_DENITRIFICATION_RATE   0.03f   /* NO3- -> N2 */
#define ECOL_MINERALIZATION_RATE    0.04f   /* organic N -> NH4+ */
#define ECOL_N_UPTAKE_RATE          0.06f   /* plant uptake of NO3- */

/* Phosphorus cycle rate constants (day^-1) */
#define ECOL_P_WEATHERING_RATE      0.001f  /* rock -> dissolved P */
#define ECOL_P_UPTAKE_RATE          0.04f   /* plant uptake */
#define ECOL_P_DECOMP_RATE          0.03f   /* organic P -> dissolved P */
#define ECOL_P_SEDIMENTATION_RATE   0.005f  /* dissolved -> sediment */

/* Carbon cycle rate constants (day^-1) */
#define ECOL_C_PHOTOSYNTHESIS_RATE  0.08f   /* CO2 -> organic C */
#define ECOL_C_RESPIRATION_RATE     0.05f   /* organic C -> CO2 */
#define ECOL_C_DECOMPOSITION_RATE   0.03f   /* dead organic -> CO2 */

/* Succession time scales (years) */
#define ECOL_SUCCESSION_PIONEER_YR  5.0f
#define ECOL_SUCCESSION_EARLY_YR    25.0f
#define ECOL_SUCCESSION_MID_YR      100.0f
#define ECOL_SUCCESSION_LATE_YR     500.0f

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    ECOL_TROPHIC_PRODUCER = 0,      /* Autotrophs: plants, algae */
    ECOL_TROPHIC_PRIMARY_CONSUMER,  /* Herbivores */
    ECOL_TROPHIC_SECONDARY_CONSUMER,/* Small carnivores */
    ECOL_TROPHIC_TERTIARY_CONSUMER, /* Apex predators */
    ECOL_TROPHIC_DECOMPOSER,        /* Fungi, bacteria */
    ECOL_TROPHIC_LEVEL_COUNT
} ecol_trophic_level_t;

typedef enum {
    ECOL_NUTRIENT_N_ATMOSPHERIC = 0, /* N2 gas */
    ECOL_NUTRIENT_N_AMMONIUM,       /* NH4+ */
    ECOL_NUTRIENT_N_NITRATE,        /* NO3- */
    ECOL_NUTRIENT_N_ORGANIC,        /* Organic nitrogen */
    ECOL_NUTRIENT_P_DISSOLVED,      /* PO4 3- */
    ECOL_NUTRIENT_P_ORGANIC,        /* Organic phosphorus */
    ECOL_NUTRIENT_C_ATMOSPHERIC,    /* CO2 */
    ECOL_NUTRIENT_C_ORGANIC,        /* Organic carbon (biomass) */
    ECOL_NUTRIENT_POOL_COUNT
} ecol_nutrient_pool_type_t;

typedef enum {
    ECOL_SUCCESSION_BARE = 0,       /* No vegetation */
    ECOL_SUCCESSION_PIONEER,        /* Lichens, mosses, annuals */
    ECOL_SUCCESSION_EARLY,          /* Grasses, shrubs */
    ECOL_SUCCESSION_MID,            /* Young forest */
    ECOL_SUCCESSION_LATE,           /* Mature forest */
    ECOL_SUCCESSION_CLIMAX,         /* Stable climax community */
    ECOL_SUCCESSION_STAGE_COUNT
} ecol_succession_stage_t;

typedef enum {
    ECOL_INTERACTION_COMPETITION = 0,
    ECOL_INTERACTION_PREDATION,
    ECOL_INTERACTION_MUTUALISM,
    ECOL_INTERACTION_PARASITISM,
    ECOL_INTERACTION_TYPE_COUNT
} ecol_interaction_type_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

/** Ecological species */
typedef struct {
    char                name[ECOL_MAX_NAME_LEN];
    ecol_trophic_level_t trophic_level;
    float               abundance;          /* population count */
    float               carrying_capacity;  /* K */
    float               growth_rate;        /* r intrinsic */
    float               biomass_per_individual; /* kg */
    float               metabolic_rate;     /* energy consumption rate */
    float               niche_breadth;      /* [0..1] generalist vs specialist */
    float               niche_position;     /* [0..1] niche axis position */
    float               competitive_ability; /* [0..1] */
    bool                active;
} ecol_species_t;

/** Nutrient pool */
typedef struct {
    ecol_nutrient_pool_type_t type;
    float               amount;     /* kg/ha or ppm */
    float               flux_in;    /* input rate per day */
    float               flux_out;   /* output rate per day */
} ecol_nutrient_pool_t;

/** Island/patch for biogeography */
typedef struct {
    char                name[ECOL_MAX_NAME_LEN];
    float               area_km2;
    float               distance_to_mainland_km;
    float               species_count;
    float               immigration_rate;   /* species/year */
    float               extinction_rate;    /* species/year */
    float               mainland_pool_size; /* P: total species on mainland */
} ecol_island_t;

/** Food web interaction */
typedef struct {
    uint32_t            species_a;
    uint32_t            species_b;
    ecol_interaction_type_t type;
    float               strength;       /* interaction coefficient */
    float               alpha;          /* competition coefficient (LV) */
} ecol_food_web_link_t;

/** Succession state */
typedef struct {
    ecol_succession_stage_t stage;
    float               time_in_stage_yr;
    float               species_richness;
    float               total_biomass;
    float               soil_depth_cm;
    float               canopy_cover;       /* [0..1] */
} ecol_succession_state_t;

/** Diversity metrics */
typedef struct {
    float   shannon_h;          /* Shannon-Wiener H' */
    float   simpson_d;          /* Simpson's D (1 - sum(pi^2)) */
    float   evenness;           /* Pielou's J = H/ln(S) */
    float   species_richness;   /* S = number of species */
    float   total_abundance;
} ecol_diversity_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float   dt;                     /* time step in days */
    float   temperature_c;
    bool    enable_nutrient_cycling;
    bool    enable_succession;
    bool    enable_biogeography;
    float   trophic_efficiency;     /* default 0.10 */
    float   disturbance_rate;       /* probability of disturbance per step */
} ecology_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    float       total_energy;
    float       total_biomass;
    float       shannon_diversity;
    float       simpson_diversity;
    uint32_t    species_count;
    float       n_total;
    float       p_total;
    float       c_total;
    ecol_succession_stage_t succession_stage;
    float       food_web_stability;
} ecology_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct ecology_sim {
    ecol_species_t          species[ECOL_MAX_SPECIES];
    uint32_t                num_species;
    ecol_food_web_link_t    links[ECOL_MAX_SPECIES * 4];
    uint32_t                num_links;
    ecol_nutrient_pool_t    nutrients[ECOL_NUTRIENT_POOL_COUNT];
    ecol_island_t           islands[ECOL_MAX_PATCHES];
    uint32_t                num_islands;
    ecol_succession_state_t succession;
    ecol_diversity_t        diversity;
    ecology_config_t        config;
    ecology_stats_t         stats;
    float                   energy_pyramid[ECOL_TROPHIC_LEVEL_COUNT];
    bool                    initialized;
} ecology_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

ecology_sim_t* ecology_create(const ecology_config_t* config);
void ecology_destroy(ecology_sim_t* sim);
int ecology_step(ecology_sim_t* sim, float dt);
ecology_config_t ecology_default_config(void);
ecology_stats_t ecology_get_stats(const ecology_sim_t* sim);

/** Species / food web */
int ecology_add_species(ecology_sim_t* sim, const ecol_species_t* sp);
int ecology_add_link(ecology_sim_t* sim, const ecol_food_web_link_t* link);

/** Nutrient cycling */
int ecology_step_nitrogen_cycle(ecology_sim_t* sim, float dt);
int ecology_step_phosphorus_cycle(ecology_sim_t* sim, float dt);
int ecology_step_carbon_cycle(ecology_sim_t* sim, float dt);

/** Energy pyramid */
int ecology_compute_energy_pyramid(ecology_sim_t* sim);

/** Diversity indices */
ecol_diversity_t ecology_compute_diversity(const ecology_sim_t* sim);
float ecology_shannon_index(const float* abundances, uint32_t n);
float ecology_simpson_index(const float* abundances, uint32_t n);

/** Island biogeography */
float ecology_species_area(float c, float area, float z);
int ecology_step_island_biogeography(ecology_sim_t* sim, uint32_t island_idx, float dt);

/** Succession */
int ecology_step_succession(ecology_sim_t* sim, float dt);

/** Food web stability (May's criterion) */
float ecology_may_stability(uint32_t S, float C, float sigma);

/** Competition (Lotka-Volterra) */
int ecology_step_competition(ecology_sim_t* sim, float dt);

/** Load preset: temperate forest ecosystem */
void ecology_load_temperate_forest(ecology_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ECOLOGY_H */

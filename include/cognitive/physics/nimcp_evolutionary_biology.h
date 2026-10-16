/**
 * @file nimcp_evolutionary_biology.h
 * @brief Evolutionary Biology simulation engine for world model
 *
 * WHAT: Simulates evolutionary processes: Hardy-Weinberg equilibrium, natural
 *       selection, genetic drift, mutation-selection balance, fitness landscapes,
 *       speciation, phylogenetics, and coalescent theory.
 * WHY:  Provides evolutionary reasoning for world model. Understanding how
 *       populations change over generations enables reasoning about adaptation,
 *       biodiversity, and genetic variation.
 * HOW:  Diploid selection model with dominance, Wright-Fisher drift sampling,
 *       NK fitness landscapes, UPGMA phylogenetic clustering, coalescent
 *       time estimation.
 *
 * THEORETICAL FOUNDATION:
 *   - Hardy-Weinberg: p^2 + 2pq + q^2 = 1 (equilibrium genotype frequencies)
 *   - Selection: dp = sp(1-p)[ph + (1-p)(1-h)] / (1 - 2pqsh - sq^2)
 *   - Drift: Var(dp) ~ p(1-p)/(2N)
 *   - Mutation-selection: q_hat = sqrt(mu/s) (recessive deleterious)
 *   - Coalescent: E[T_MRCA] = 4N generations
 *   - Molecular clock: ~10^-9 substitutions/site/year
 */

#ifndef NIMCP_EVOLUTIONARY_BIOLOGY_H
#define NIMCP_EVOLUTIONARY_BIOLOGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define EVO_MAX_POPULATIONS         16
#define EVO_MAX_LOCI                64
#define EVO_MAX_ALLELES_PER_LOCUS   4
#define EVO_MAX_SPECIES             32
#define EVO_MAX_TREE_NODES          63      /* 2*MAX_SPECIES - 1 */
#define EVO_MAX_NAME_LEN            32
#define EVO_NK_MAX_N                20      /* Max loci for NK model */
#define EVO_NK_MAX_K                5       /* Max epistatic interactions */

/* Molecular evolution rates */
#define EVO_MOLECULAR_CLOCK_RATE    1.0e-9f /* substitutions/site/year */
#define EVO_MUTATION_RATE_PER_BASE  1.0e-8f /* per base per generation */
#define EVO_MUTATION_RATE_PER_GENE  1.0e-5f /* per gene per generation */

/* Selection coefficients (typical ranges) */
#define EVO_NEUTRAL_THRESHOLD       1.0e-4f /* |s| < 1/(2N) effectively neutral */
#define EVO_WEAK_SELECTION          0.01f
#define EVO_MODERATE_SELECTION      0.05f
#define EVO_STRONG_SELECTION        0.20f
#define EVO_LETHAL_SELECTION        1.0f

/* Drift */
#define EVO_EFFECTIVE_POP_SMALL     100
#define EVO_EFFECTIVE_POP_MEDIUM    10000
#define EVO_EFFECTIVE_POP_LARGE     1000000

/* Speciation */
#define EVO_REPRODUCTIVE_ISOLATION_THRESHOLD 0.9f
#define EVO_DM_INCOMPATIBILITY_THRESHOLD    3   /* Dobzhansky-Muller loci */

/* ============================================================================
 * Enums
 * ============================================================================ */

typedef enum {
    EVO_SELECTION_NONE = 0,         /* No selection (neutral) */
    EVO_SELECTION_DIRECTIONAL,      /* One allele favored */
    EVO_SELECTION_STABILIZING,      /* Intermediate phenotype favored */
    EVO_SELECTION_DISRUPTIVE,       /* Extreme phenotypes favored */
    EVO_SELECTION_BALANCING,        /* Heterozygote advantage */
    EVO_SELECTION_FREQUENCY_DEP,    /* Rare allele advantage */
    EVO_SELECTION_TYPE_COUNT
} evo_selection_type_t;

typedef enum {
    EVO_SPECIATION_NONE = 0,
    EVO_SPECIATION_ALLOPATRIC,      /* Geographic isolation */
    EVO_SPECIATION_SYMPATRIC,       /* Same habitat, ecological divergence */
    EVO_SPECIATION_PARAPATRIC,      /* Adjacent habitats */
    EVO_SPECIATION_PERIPATRIC,      /* Small peripheral population */
    EVO_SPECIATION_TYPE_COUNT
} evo_speciation_type_t;

typedef enum {
    EVO_DOMINANCE_RECESSIVE = 0,    /* h = 0: aa only affected */
    EVO_DOMINANCE_PARTIAL,          /* 0 < h < 1 */
    EVO_DOMINANCE_COMPLETE,         /* h = 1: Aa = AA */
    EVO_DOMINANCE_OVERDOMINANCE,    /* h > 1: heterozygote advantage */
    EVO_DOMINANCE_TYPE_COUNT
} evo_dominance_type_t;

/* ============================================================================
 * Structs
 * ============================================================================ */

/** Single genetic locus with allele frequencies */
typedef struct {
    char        name[EVO_MAX_NAME_LEN];
    uint32_t    num_alleles;
    float       allele_freq[EVO_MAX_ALLELES_PER_LOCUS];  /* p, q, r, ... */
    float       selection_coeff;    /* s: fitness = 1-s for homozygote */
    float       dominance_h;        /* h: heterozygote effect (0=recessive, 1=dominant) */
    float       mutation_rate;      /* mu: per generation */
    float       back_mutation_rate; /* nu: reverse mutation */
    evo_dominance_type_t dominance_type;
} evo_locus_t;

/** Population with allele frequencies and demographic parameters */
typedef struct {
    char                name[EVO_MAX_NAME_LEN];
    uint32_t            id;
    uint32_t            effective_size;      /* Ne */
    uint32_t            census_size;         /* N */
    float               generation_time_yr;  /* years per generation */
    evo_locus_t         loci[EVO_MAX_LOCI];
    uint32_t            num_loci;
    float               mean_fitness;        /* w_bar */
    float               inbreeding_coeff;    /* F (Wright's F-statistic) */
    float               migration_rate;      /* m: fraction immigrants */
    evo_selection_type_t selection_type;
    float               generations_elapsed;
    bool                active;
} evo_population_t;

/** NK fitness landscape model */
typedef struct {
    uint32_t    n;                          /* number of loci */
    uint32_t    k;                          /* epistatic interactions per locus */
    float       fitness_table[EVO_NK_MAX_N][(1 << (EVO_NK_MAX_K + 1))];
    uint32_t    neighbors[EVO_NK_MAX_N][EVO_NK_MAX_K];
    float       max_fitness;
    float       mean_fitness;
    uint32_t    num_peaks;                  /* local optima count */
} evo_nk_landscape_t;

/** Phylogenetic tree node (for UPGMA) */
typedef struct {
    uint32_t    id;
    char        name[EVO_MAX_NAME_LEN];
    int32_t     left_child;     /* -1 if leaf */
    int32_t     right_child;    /* -1 if leaf */
    float       branch_length;  /* distance to parent */
    float       height;         /* node height (time) */
    bool        is_leaf;
} evo_phylo_node_t;

/** Phylogenetic tree */
typedef struct {
    evo_phylo_node_t    nodes[EVO_MAX_TREE_NODES];
    uint32_t            num_nodes;
    uint32_t            num_leaves;
    int32_t             root;
    float               total_tree_length;
} evo_phylo_tree_t;

/** Speciation state between two populations */
typedef struct {
    uint32_t    pop_a;
    uint32_t    pop_b;
    float       genetic_distance;
    float       reproductive_isolation; /* [0..1] */
    uint32_t    dm_incompatibilities;   /* Dobzhansky-Muller count */
    evo_speciation_type_t mode;
    bool        speciated;              /* past threshold */
} evo_speciation_state_t;

/** Coalescent result */
typedef struct {
    float   expected_tmrca;     /* E[T_MRCA] in generations */
    float   theta;              /* 4*Ne*mu (population mutation rate) */
    float   expected_pairwise_diff; /* E[pi] = theta */
    uint32_t sample_size;
} evo_coalescent_t;

/* ============================================================================
 * Config
 * ============================================================================ */

typedef struct {
    float   dt;                     /* time step in generations */
    float   temperature;            /* not used, API compat */
    bool    enable_drift;
    bool    enable_mutation;
    bool    enable_migration;
    bool    enable_selection;
    uint32_t random_seed;
} evolutionary_biology_config_t;

/* ============================================================================
 * Stats
 * ============================================================================ */

typedef struct {
    uint64_t    step_count;
    uint32_t    num_populations;
    float       mean_heterozygosity;
    float       mean_fst;           /* population differentiation */
    float       total_genetic_variance;
    float       mean_fitness;
    uint32_t    speciation_events;
    float       total_generations;
} evolutionary_biology_stats_t;

/* ============================================================================
 * Engine
 * ============================================================================ */

typedef struct evolutionary_biology_sim {
    evo_population_t            populations[EVO_MAX_POPULATIONS];
    uint32_t                    num_populations;
    evo_nk_landscape_t          landscape;
    evo_phylo_tree_t            tree;
    evo_speciation_state_t      speciation[EVO_MAX_POPULATIONS * (EVO_MAX_POPULATIONS - 1) / 2];
    uint32_t                    num_speciation_pairs;
    evolutionary_biology_config_t config;
    evolutionary_biology_stats_t stats;
    uint64_t                    rng_state;      /* PRNG state for drift */
    bool                        initialized;
} evolutionary_biology_sim_t;

/* ============================================================================
 * API
 * ============================================================================ */

evolutionary_biology_sim_t* evolutionary_biology_create(
    const evolutionary_biology_config_t* config);
void evolutionary_biology_destroy(evolutionary_biology_sim_t* sim);
int evolutionary_biology_step(evolutionary_biology_sim_t* sim, float dt);
evolutionary_biology_config_t evolutionary_biology_default_config(void);
evolutionary_biology_stats_t evolutionary_biology_get_stats(
    const evolutionary_biology_sim_t* sim);

/** Population management */
int evo_add_population(evolutionary_biology_sim_t* sim,
                       const evo_population_t* pop);
int evo_add_locus(evolutionary_biology_sim_t* sim, uint32_t pop_idx,
                  const evo_locus_t* locus);

/** Hardy-Weinberg */
bool evo_check_hardy_weinberg(float p, float observed_het, float n,
                               float chi_sq_threshold);
void evo_hw_genotype_frequencies(float p, float* p_AA, float* p_Aa, float* p_aa);

/** Natural selection */
float evo_delta_p_selection(float p, float s, float h);
float evo_mean_fitness(float p, float s, float h);

/** Genetic drift */
float evo_drift_variance(float p, uint32_t ne);
float evo_drift_sample(evolutionary_biology_sim_t* sim, float p, uint32_t ne);

/** Mutation-selection balance */
float evo_mutation_selection_balance(float mu, float s, float h);

/** Fitness landscape (NK model) */
int evo_init_nk_landscape(evolutionary_biology_sim_t* sim, uint32_t n, uint32_t k);
float evo_nk_fitness(const evo_nk_landscape_t* landscape, const uint8_t* genotype);
int evo_adaptive_walk(evolutionary_biology_sim_t* sim, uint8_t* genotype,
                      uint32_t max_steps);

/** Phylogenetics */
int evo_build_upgma(evolutionary_biology_sim_t* sim,
                    const float* distance_matrix, uint32_t n_taxa,
                    const char names[][EVO_MAX_NAME_LEN]);
float evo_molecular_clock_distance(float divergence_time_yr, float rate);

/** Coalescent */
evo_coalescent_t evo_coalescent_stats(uint32_t ne, float mu, uint32_t sample_size);

/** Speciation */
int evo_check_speciation(evolutionary_biology_sim_t* sim, uint32_t pop_a,
                         uint32_t pop_b);

/** Load preset: two-population divergence scenario */
void evo_load_divergence_scenario(evolutionary_biology_sim_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EVOLUTIONARY_BIOLOGY_H */

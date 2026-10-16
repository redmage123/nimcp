/**
 * @file nimcp_evolutionary_biology.c
 * @brief Evolutionary Biology simulation engine -- Hardy-Weinberg, natural selection,
 *        genetic drift, mutation-selection balance, NK fitness landscapes,
 *        phylogenetics (UPGMA), coalescent theory, speciation
 *
 * WHAT: Simulates evolutionary processes with real population genetics equations.
 * WHY:  Evolutionary prior for world model reasoning about adaptation and diversity.
 * HOW:  Diploid selection with dominance, Wright-Fisher drift, NK landscapes,
 *       UPGMA clustering, coalescent time estimation, Dobzhansky-Muller speciation.
 */

#include "cognitive/physics/nimcp_evolutionary_biology.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "EVO_BIOLOGY"

/* ============================================================================
 * Helpers
 * ============================================================================ */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/** Simple xorshift64 PRNG for drift simulation */
static uint64_t evo_rng_next(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

/** Uniform random float [0,1) from PRNG state */
static float evo_rand_float(uint64_t* state) {
    return (float)(evo_rng_next(state) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

/**
 * Approximate binomial sampling: given n trials with probability p,
 * return the fraction of successes. Uses normal approximation for large n.
 */
static float evo_binomial_sample(uint64_t* rng, float p, uint32_t n) {
    if (n == 0) return p;
    if (n < 20) {
        /* Direct sampling for small n */
        uint32_t successes = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (evo_rand_float(rng) < p) successes++;
        }
        return (float)successes / (float)n;
    }
    /* Normal approximation: X ~ N(np, np(1-p)) */
    float mean = p;
    float var = p * (1.0f - p) / (float)n;
    float sd = sqrtf(var + 1e-12f);
    /* Box-Muller for normal variate */
    float u1 = evo_rand_float(rng);
    float u2 = evo_rand_float(rng);
    if (u1 < 1e-7f) u1 = 1e-7f;
    if (u1 >= 1.0f) u1 = 1.0f - 1e-7f;  /* logf(1.0)=0 → sqrtf(0)=0, logf(>1)→sqrtf(-ve)=NaN */
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
    float result = mean + z * sd;
    return clampf(result, 0.0f, 1.0f);
}

/* ============================================================================
 * Create / Destroy / Config
 * ============================================================================ */

evolutionary_biology_config_t evolutionary_biology_default_config(void) {
    evolutionary_biology_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dt = 1.0f;                  /* 1 generation per step */
    cfg.temperature = 20.0f;
    cfg.enable_drift = true;
    cfg.enable_mutation = true;
    cfg.enable_migration = false;
    cfg.enable_selection = true;
    cfg.random_seed = 42;
    return cfg;
}

evolutionary_biology_sim_t* evolutionary_biology_create(
    const evolutionary_biology_config_t* config) {
    evolutionary_biology_sim_t* sim = (evolutionary_biology_sim_t*)nimcp_calloc(
        1, sizeof(evolutionary_biology_sim_t));
    if (!sim) {
        LOG_ERROR(LOG_TAG, "Failed to allocate evolutionary biology sim");
        return NULL;
    }
    sim->config = config ? *config : evolutionary_biology_default_config();
    sim->rng_state = (uint64_t)sim->config.random_seed * 6364136223846793005ULL + 1;
    sim->initialized = true;
    LOG_INFO(LOG_TAG, "Evolutionary biology sim created (dt=%.1f gen)", sim->config.dt);
    return sim;
}

void evolutionary_biology_destroy(evolutionary_biology_sim_t* sim) {
    if (!sim) return;
    LOG_INFO(LOG_TAG, "Evo bio sim destroyed after %lu steps", sim->stats.step_count);
    nimcp_free(sim);
}

evolutionary_biology_stats_t evolutionary_biology_get_stats(
    const evolutionary_biology_sim_t* sim) {
    evolutionary_biology_stats_t empty;
    memset(&empty, 0, sizeof(empty));
    return sim ? sim->stats : empty;
}

/* ============================================================================
 * Population / Locus Management
 * ============================================================================ */

int evo_add_population(evolutionary_biology_sim_t* sim,
                       const evo_population_t* pop) {
    if (!sim || !pop) return -1;
    if (sim->num_populations >= EVO_MAX_POPULATIONS) return -1;
    sim->populations[sim->num_populations] = *pop;
    sim->populations[sim->num_populations].id = sim->num_populations;
    sim->num_populations++;
    return 0;
}

int evo_add_locus(evolutionary_biology_sim_t* sim, uint32_t pop_idx,
                  const evo_locus_t* locus) {
    if (!sim || !locus || pop_idx >= sim->num_populations) return -1;
    evo_population_t* pop = &sim->populations[pop_idx];
    if (pop->num_loci >= EVO_MAX_LOCI) return -1;
    pop->loci[pop->num_loci] = *locus;
    pop->num_loci++;
    return 0;
}

/* ============================================================================
 * Hardy-Weinberg Equilibrium
 * ============================================================================ */

/**
 * Compute Hardy-Weinberg genotype frequencies from allele frequency p.
 * p_AA = p^2, p_Aa = 2pq, p_aa = q^2 where q = 1-p.
 */
void evo_hw_genotype_frequencies(float p, float* p_AA, float* p_Aa, float* p_aa) {
    float q = 1.0f - p;
    if (p_AA) *p_AA = p * p;
    if (p_Aa) *p_Aa = 2.0f * p * q;
    if (p_aa) *p_aa = q * q;
}

/**
 * Test Hardy-Weinberg equilibrium using chi-squared approximation.
 * Returns true if population is in HW equilibrium.
 */
bool evo_check_hardy_weinberg(float p, float observed_het, float n,
                               float chi_sq_threshold) {
    float expected_het = 2.0f * p * (1.0f - p);
    float diff = observed_het - expected_het;
    /* Simplified chi-sq with 1 df */
    float chi_sq = n * diff * diff / (expected_het + 1e-10f);
    return chi_sq < chi_sq_threshold; /* 3.84 for p=0.05 */
}

/* ============================================================================
 * Natural Selection
 * ============================================================================ */

/**
 * Change in allele frequency under diploid selection with dominance.
 *
 * Genotype fitnesses:
 *   AA: 1         (wild-type)
 *   Aa: 1 - h*s   (heterozygote)
 *   aa: 1 - s     (homozygote mutant)
 *
 * dp = s*p*q * [p*h + q*(1-h)] / w_bar
 * where w_bar = p^2 + 2pq(1-hs) + q^2(1-s)
 *             = 1 - 2pqhs - q^2*s
 */
float evo_delta_p_selection(float p, float s, float h) {
    float q = 1.0f - p;
    if (p < 1e-10f || q < 1e-10f) return 0.0f;

    float w_bar = 1.0f - 2.0f * p * q * h * s - q * q * s;
    if (w_bar < 1e-10f) return 0.0f;

    float dp = s * p * q * (p * h + q * (1.0f - h)) / w_bar;
    return dp;
}

/**
 * Mean population fitness: w_bar = 1 - 2pqhs - q^2*s
 */
float evo_mean_fitness(float p, float s, float h) {
    float q = 1.0f - p;
    return 1.0f - 2.0f * p * q * h * s - q * q * s;
}

/* ============================================================================
 * Genetic Drift
 * ============================================================================ */

/**
 * Expected variance in allele frequency change due to drift.
 * Var(dp) = p(1-p) / (2*Ne)
 */
float evo_drift_variance(float p, uint32_t ne) {
    if (ne == 0) return 0.0f;
    return p * (1.0f - p) / (2.0f * (float)ne);
}

/**
 * Sample new allele frequency after drift using binomial sampling.
 */
float evo_drift_sample(evolutionary_biology_sim_t* sim, float p, uint32_t ne) {
    if (!sim || ne == 0) return p;
    return evo_binomial_sample(&sim->rng_state, p, 2 * ne);
}

/* ============================================================================
 * Mutation-Selection Balance
 * ============================================================================ */

/**
 * Equilibrium allele frequency under mutation-selection balance.
 * For recessive deleterious (h=0): q_hat = sqrt(mu/s)
 * For dominant (h=1): q_hat = mu/s
 * General: q_hat ~ mu / (h*s) for h > 0
 */
float evo_mutation_selection_balance(float mu, float s, float h) {
    if (s < 1e-10f) return 0.5f; /* No selection, drift dominates */
    if (h < 1e-6f) {
        /* Recessive: q = sqrt(mu/s) */
        return sqrtf(mu / s);
    }
    /* Partially/fully dominant: q = mu / (h*s) */
    return mu / (h * s);
}

/* ============================================================================
 * NK Fitness Landscape
 * ============================================================================ */

int evo_init_nk_landscape(evolutionary_biology_sim_t* sim, uint32_t n, uint32_t k) {
    if (!sim) return -1;
    if (n > EVO_NK_MAX_N || k > EVO_NK_MAX_K) return -1;

    evo_nk_landscape_t* land = &sim->landscape;
    land->n = n;
    land->k = k;

    /* Generate random fitness contributions */
    uint32_t entries_per_locus = 1u << (k + 1);
    for (uint32_t i = 0; i < n; i++) {
        /* Random epistatic neighbors */
        for (uint32_t j = 0; j < k; j++) {
            land->neighbors[i][j] = (uint32_t)(evo_rand_float(&sim->rng_state) * n);
            if (land->neighbors[i][j] == i) {
                land->neighbors[i][j] = (i + 1) % n;
            }
        }
        /* Random fitness table */
        for (uint32_t e = 0; e < entries_per_locus; e++) {
            land->fitness_table[i][e] = evo_rand_float(&sim->rng_state);
        }
    }

    LOG_INFO(LOG_TAG, "Initialized NK landscape (N=%u, K=%u)", n, k);
    return 0;
}

/**
 * Compute fitness of a genotype on the NK landscape.
 * Fitness = (1/N) * sum(f_i(x_i, x_neighbors))
 */
float evo_nk_fitness(const evo_nk_landscape_t* landscape, const uint8_t* genotype) {
    if (!landscape || !genotype) return 0.0f;

    float total = 0.0f;
    for (uint32_t i = 0; i < landscape->n; i++) {
        /* Build index from locus i and its K neighbors */
        uint32_t idx = genotype[i];
        for (uint32_t j = 0; j < landscape->k; j++) {
            uint32_t nb = landscape->neighbors[i][j];
            idx = (idx << 1) | (genotype[nb] & 1);
        }
        uint32_t max_idx = (1u << (landscape->k + 1)) - 1;
        idx &= max_idx;
        total += landscape->fitness_table[i][idx];
    }

    return total / (float)landscape->n;
}

/**
 * Adaptive walk: greedy hill-climbing on NK landscape.
 * Returns number of steps taken.
 */
int evo_adaptive_walk(evolutionary_biology_sim_t* sim, uint8_t* genotype,
                      uint32_t max_steps) {
    if (!sim || !genotype) return -1;
    evo_nk_landscape_t* land = &sim->landscape;

    float current_fitness = evo_nk_fitness(land, genotype);
    uint32_t steps = 0;

    for (uint32_t s = 0; s < max_steps; s++) {
        bool improved = false;
        /* Try flipping each locus */
        for (uint32_t i = 0; i < land->n; i++) {
            genotype[i] ^= 1; /* flip */
            float new_fitness = evo_nk_fitness(land, genotype);
            if (new_fitness > current_fitness) {
                current_fitness = new_fitness;
                improved = true;
                steps++;
                break; /* greedy: take first improvement */
            }
            genotype[i] ^= 1; /* flip back */
        }
        if (!improved) break; /* local optimum reached */
    }

    land->max_fitness = current_fitness;
    return (int)steps;
}

/* ============================================================================
 * Phylogenetics (UPGMA)
 * ============================================================================ */

/**
 * Build UPGMA (Unweighted Pair Group Method with Arithmetic Mean) tree.
 * distance_matrix: n_taxa x n_taxa symmetric matrix (row-major).
 */
int evo_build_upgma(evolutionary_biology_sim_t* sim,
                    const float* distance_matrix, uint32_t n_taxa,
                    const char names[][EVO_MAX_NAME_LEN]) {
    if (!sim || !distance_matrix || n_taxa < 2) return -1;
    if (n_taxa > EVO_MAX_SPECIES) return -1;

    evo_phylo_tree_t* tree = &sim->tree;
    memset(tree, 0, sizeof(evo_phylo_tree_t));
    tree->num_leaves = n_taxa;

    /* Initialize leaf nodes */
    for (uint32_t i = 0; i < n_taxa; i++) {
        tree->nodes[i].id = i;
        tree->nodes[i].is_leaf = true;
        tree->nodes[i].left_child = -1;
        tree->nodes[i].right_child = -1;
        tree->nodes[i].height = 0.0f;
        if (names) {
            strncpy(tree->nodes[i].name, names[i], EVO_MAX_NAME_LEN - 1);
        }
    }
    tree->num_nodes = n_taxa;

    /* Working copy of distance matrix */
    float dist[EVO_MAX_SPECIES][EVO_MAX_SPECIES];
    for (uint32_t i = 0; i < n_taxa; i++) {
        for (uint32_t j = 0; j < n_taxa; j++) {
            dist[i][j] = distance_matrix[i * n_taxa + j];
        }
    }

    /* Cluster sizes */
    uint32_t cluster_size[EVO_MAX_SPECIES];
    int32_t cluster_node[EVO_MAX_SPECIES]; /* which tree node represents this cluster */
    bool active[EVO_MAX_SPECIES];
    for (uint32_t i = 0; i < n_taxa; i++) {
        cluster_size[i] = 1;
        cluster_node[i] = (int32_t)i;
        active[i] = true;
    }

    /* UPGMA iterations: merge closest pair */
    for (uint32_t step = 0; step < n_taxa - 1; step++) {
        /* Find minimum distance */
        float min_dist = 1e30f;
        uint32_t mi = 0, mj = 0;
        for (uint32_t i = 0; i < n_taxa; i++) {
            if (!active[i]) continue;
            for (uint32_t j = i + 1; j < n_taxa; j++) {
                if (!active[j]) continue;
                if (dist[i][j] < min_dist) {
                    min_dist = dist[i][j];
                    mi = i; mj = j;
                }
            }
        }

        /* Create new internal node */
        uint32_t new_node_idx = tree->num_nodes;
        if (new_node_idx >= EVO_MAX_TREE_NODES) break;

        evo_phylo_node_t* new_node = &tree->nodes[new_node_idx];
        new_node->id = new_node_idx;
        new_node->is_leaf = false;
        new_node->left_child = cluster_node[mi];
        new_node->right_child = cluster_node[mj];
        new_node->height = min_dist / 2.0f;

        /* Branch lengths */
        tree->nodes[cluster_node[mi]].branch_length =
            new_node->height - tree->nodes[cluster_node[mi]].height;
        tree->nodes[cluster_node[mj]].branch_length =
            new_node->height - tree->nodes[cluster_node[mj]].height;

        tree->num_nodes++;

        /* Update distances: UPGMA average linkage */
        uint32_t ni = cluster_size[mi];
        uint32_t nj = cluster_size[mj];
        for (uint32_t k = 0; k < n_taxa; k++) {
            if (!active[k] || k == mi || k == mj) continue;
            dist[mi][k] = (dist[mi][k] * ni + dist[mj][k] * nj) / (float)(ni + nj);
            dist[k][mi] = dist[mi][k];
        }

        cluster_size[mi] = ni + nj;
        cluster_node[mi] = (int32_t)new_node_idx;
        active[mj] = false;
    }

    tree->root = (int32_t)(tree->num_nodes - 1);

    /* Total tree length */
    tree->total_tree_length = 0.0f;
    for (uint32_t i = 0; i < tree->num_nodes; i++) {
        tree->total_tree_length += tree->nodes[i].branch_length;
    }

    return 0;
}

/**
 * Molecular clock: expected genetic distance given divergence time.
 * d = 2 * rate * time (factor of 2 for both lineages diverging)
 */
float evo_molecular_clock_distance(float divergence_time_yr, float rate) {
    return 2.0f * rate * divergence_time_yr;
}

/* ============================================================================
 * Coalescent Theory
 * ============================================================================ */

/**
 * Compute coalescent statistics for a sample.
 * E[T_MRCA] = 4*Ne * (1 - 1/n) generations (for sample of size n)
 * For n=2: E[T_MRCA] = 2*Ne
 * theta = 4*Ne*mu (population-scaled mutation rate)
 */
evo_coalescent_t evo_coalescent_stats(uint32_t ne, float mu, uint32_t sample_size) {
    evo_coalescent_t coal;
    memset(&coal, 0, sizeof(coal));
    coal.sample_size = sample_size;
    coal.theta = 4.0f * (float)ne * mu;

    /* E[T_MRCA] = sum over k=2..n of 4*Ne / (k*(k-1)) */
    float tmrca = 0.0f;
    for (uint32_t k = 2; k <= sample_size; k++) {
        tmrca += 4.0f * (float)ne / ((float)k * ((float)k - 1.0f));
    }
    coal.expected_tmrca = tmrca;

    /* Expected pairwise differences = theta (Watterson's estimator) */
    coal.expected_pairwise_diff = coal.theta;

    return coal;
}

/* ============================================================================
 * Speciation
 * ============================================================================ */

int evo_check_speciation(evolutionary_biology_sim_t* sim, uint32_t pop_a,
                         uint32_t pop_b) {
    if (!sim || pop_a >= sim->num_populations || pop_b >= sim->num_populations) return -1;

    evo_population_t* pa = &sim->populations[pop_a];
    evo_population_t* pb = &sim->populations[pop_b];

    /* Compute genetic distance (Nei's D) between populations */
    float genetic_dist = 0.0f;
    uint32_t shared_loci = 0;

    for (uint32_t i = 0; i < pa->num_loci && i < pb->num_loci; i++) {
        float diff = fabsf(pa->loci[i].allele_freq[0] - pb->loci[i].allele_freq[0]);
        genetic_dist += diff;
        shared_loci++;
    }
    if (shared_loci > 0) genetic_dist /= shared_loci;

    /* Reproductive isolation increases with genetic distance */
    float ri = 1.0f - expf(-5.0f * genetic_dist);

    /* Dobzhansky-Muller incompatibilities */
    uint32_t dm_count = 0;
    for (uint32_t i = 0; i < pa->num_loci && i < pb->num_loci; i++) {
        /* Fixed differences between populations */
        if ((pa->loci[i].allele_freq[0] > 0.95f && pb->loci[i].allele_freq[0] < 0.05f) ||
            (pa->loci[i].allele_freq[0] < 0.05f && pb->loci[i].allele_freq[0] > 0.95f)) {
            dm_count++;
        }
    }

    /* Store speciation state */
    for (uint32_t s = 0; s < sim->num_speciation_pairs; s++) {
        evo_speciation_state_t* sp = &sim->speciation[s];
        if ((sp->pop_a == pop_a && sp->pop_b == pop_b) ||
            (sp->pop_a == pop_b && sp->pop_b == pop_a)) {
            sp->genetic_distance = genetic_dist;
            sp->reproductive_isolation = ri;
            sp->dm_incompatibilities = dm_count;
            sp->speciated = (ri >= EVO_REPRODUCTIVE_ISOLATION_THRESHOLD ||
                            dm_count >= EVO_DM_INCOMPATIBILITY_THRESHOLD);
            if (sp->speciated) {
                sim->stats.speciation_events++;
                LOG_INFO(LOG_TAG, "Speciation detected between pop %u and %u "
                         "(RI=%.3f, DM=%u)", pop_a, pop_b, ri, dm_count);
            }
            return 0;
        }
    }

    /* Add new pair */
    uint32_t idx = sim->num_speciation_pairs;
    if (idx >= EVO_MAX_POPULATIONS * (EVO_MAX_POPULATIONS - 1) / 2) return -1;
    sim->speciation[idx].pop_a = pop_a;
    sim->speciation[idx].pop_b = pop_b;
    sim->speciation[idx].genetic_distance = genetic_dist;
    sim->speciation[idx].reproductive_isolation = ri;
    sim->speciation[idx].dm_incompatibilities = dm_count;
    sim->speciation[idx].speciated = (ri >= EVO_REPRODUCTIVE_ISOLATION_THRESHOLD ||
                                       dm_count >= EVO_DM_INCOMPATIBILITY_THRESHOLD);
    sim->num_speciation_pairs++;

    return 0;
}

/* ============================================================================
 * Main Step
 * ============================================================================ */

int evolutionary_biology_step(evolutionary_biology_sim_t* sim, float dt) {
    if (!sim || !sim->initialized) return -1;
    if (dt <= 0.0f) dt = sim->config.dt;

    float total_het = 0.0f;
    float total_fitness = 0.0f;
    uint32_t total_loci = 0;

    for (uint32_t p = 0; p < sim->num_populations; p++) {
        evo_population_t* pop = &sim->populations[p];
        if (!pop->active) continue;

        pop->mean_fitness = 1.0f;

        for (uint32_t l = 0; l < pop->num_loci; l++) {
            evo_locus_t* loc = &pop->loci[l];
            float freq = loc->allele_freq[0];

            /* 1. Natural selection: change in allele frequency */
            if (sim->config.enable_selection && loc->selection_coeff > EVO_NEUTRAL_THRESHOLD) {
                float dp = evo_delta_p_selection(freq, loc->selection_coeff, loc->dominance_h);
                freq += dp * dt;
            }

            /* 2. Mutation: forward and back mutation */
            if (sim->config.enable_mutation) {
                float dp_mut = -(loc->mutation_rate) * freq +
                               loc->back_mutation_rate * (1.0f - freq);
                freq += dp_mut * dt;
            }

            /* 3. Migration */
            if (sim->config.enable_migration && pop->migration_rate > 0.0f) {
                /* Assume migrants have frequency 0.5 (mainland) */
                float dp_mig = pop->migration_rate * (0.5f - freq);
                freq += dp_mig * dt;
            }

            /* 4. Genetic drift (binomial sampling) */
            if (sim->config.enable_drift) {
                freq = evo_drift_sample(sim, freq, pop->effective_size);
            }

            /* Clamp and store */
            freq = clampf(freq, 0.0f, 1.0f);
            loc->allele_freq[0] = freq;
            if (loc->num_alleles >= 2) {
                loc->allele_freq[1] = 1.0f - freq;
            }

            /* Track heterozygosity */
            total_het += 2.0f * freq * (1.0f - freq);
            total_loci++;

            /* Update mean fitness */
            pop->mean_fitness *= evo_mean_fitness(freq, loc->selection_coeff, loc->dominance_h);
        }

        total_fitness += pop->mean_fitness;
        pop->generations_elapsed += dt;
    }

    /* Check for speciation between all population pairs */
    for (uint32_t i = 0; i < sim->num_populations; i++) {
        for (uint32_t j = i + 1; j < sim->num_populations; j++) {
            if (sim->populations[i].active && sim->populations[j].active) {
                evo_check_speciation(sim, i, j);
            }
        }
    }

    /* Update stats */
    sim->stats.step_count++;
    sim->stats.num_populations = sim->num_populations;
    sim->stats.mean_heterozygosity = total_loci > 0 ? total_het / total_loci : 0.0f;
    sim->stats.mean_fitness = sim->num_populations > 0 ?
        total_fitness / sim->num_populations : 0.0f;
    sim->stats.total_generations += dt;

    /* Compute FST (simplified: variance in allele freq / total variance) */
    if (sim->num_populations > 1 && total_loci > 0) {
        float mean_p = 0.0f;
        uint32_t count = 0;
        for (uint32_t p = 0; p < sim->num_populations; p++) {
            if (!sim->populations[p].active) continue;
            for (uint32_t l = 0; l < sim->populations[p].num_loci; l++) {
                mean_p += sim->populations[p].loci[l].allele_freq[0];
                count++;
            }
        }
        if (count > 0) mean_p /= count;

        float var_p = 0.0f;
        count = 0;
        for (uint32_t p = 0; p < sim->num_populations; p++) {
            if (!sim->populations[p].active) continue;
            for (uint32_t l = 0; l < sim->populations[p].num_loci; l++) {
                float diff = sim->populations[p].loci[l].allele_freq[0] - mean_p;
                var_p += diff * diff;
                count++;
            }
        }
        if (count > 0) var_p /= count;

        float ht = mean_p * (1.0f - mean_p);
        sim->stats.mean_fst = (ht > 1e-10f) ? var_p / ht : 0.0f;
        sim->stats.total_genetic_variance = var_p;
    }

    return 0;
}

/* ============================================================================
 * Preset: Two-Population Divergence
 * ============================================================================ */

void evo_load_divergence_scenario(evolutionary_biology_sim_t* sim) {
    if (!sim) return;

    sim->num_populations = 0;
    sim->num_speciation_pairs = 0;

    /* Population A: mainland, large Ne */
    evo_population_t pop_a = {0};
    strncpy(pop_a.name, "Mainland", EVO_MAX_NAME_LEN - 1);
    pop_a.effective_size = EVO_EFFECTIVE_POP_MEDIUM;
    pop_a.census_size = 50000;
    pop_a.generation_time_yr = 5.0f;
    pop_a.selection_type = EVO_SELECTION_DIRECTIONAL;
    pop_a.active = true;

    /* Add 5 loci with different selection regimes */
    evo_locus_t locus = {0};
    locus.num_alleles = 2;
    locus.mutation_rate = EVO_MUTATION_RATE_PER_GENE;
    locus.back_mutation_rate = EVO_MUTATION_RATE_PER_GENE * 0.1f;

    strncpy(locus.name, "Coat color", EVO_MAX_NAME_LEN - 1);
    locus.allele_freq[0] = 0.7f; locus.allele_freq[1] = 0.3f;
    locus.selection_coeff = 0.02f; locus.dominance_h = 0.5f;
    locus.dominance_type = EVO_DOMINANCE_PARTIAL;
    pop_a.loci[pop_a.num_loci++] = locus;

    strncpy(locus.name, "Body size", EVO_MAX_NAME_LEN - 1);
    locus.allele_freq[0] = 0.5f; locus.allele_freq[1] = 0.5f;
    locus.selection_coeff = 0.05f; locus.dominance_h = 0.3f;
    pop_a.loci[pop_a.num_loci++] = locus;

    strncpy(locus.name, "Disease resist", EVO_MAX_NAME_LEN - 1);
    locus.allele_freq[0] = 0.4f; locus.allele_freq[1] = 0.6f;
    locus.selection_coeff = 0.1f; locus.dominance_h = 1.5f; /* overdominance */
    locus.dominance_type = EVO_DOMINANCE_OVERDOMINANCE;
    pop_a.loci[pop_a.num_loci++] = locus;

    strncpy(locus.name, "Neutral marker", EVO_MAX_NAME_LEN - 1);
    locus.allele_freq[0] = 0.6f; locus.allele_freq[1] = 0.4f;
    locus.selection_coeff = 0.0f; locus.dominance_h = 0.0f;
    locus.dominance_type = EVO_DOMINANCE_RECESSIVE;
    pop_a.loci[pop_a.num_loci++] = locus;

    evo_add_population(sim, &pop_a);

    /* Population B: island, small Ne (founder effect) */
    evo_population_t pop_b = pop_a;
    strncpy(pop_b.name, "Island", EVO_MAX_NAME_LEN - 1);
    pop_b.effective_size = 500; /* bottleneck */
    pop_b.census_size = 2000;
    /* Founder effect: shifted allele frequencies */
    pop_b.loci[0].allele_freq[0] = 0.3f; pop_b.loci[0].allele_freq[1] = 0.7f;
    pop_b.loci[1].allele_freq[0] = 0.8f; pop_b.loci[1].allele_freq[1] = 0.2f;
    pop_b.loci[2].allele_freq[0] = 0.2f; pop_b.loci[2].allele_freq[1] = 0.8f;
    pop_b.loci[3].allele_freq[0] = 0.9f; pop_b.loci[3].allele_freq[1] = 0.1f;

    evo_add_population(sim, &pop_b);

    LOG_INFO(LOG_TAG, "Loaded divergence scenario: mainland (Ne=%d) vs island (Ne=%d), 4 loci",
             EVO_EFFECTIVE_POP_MEDIUM, 500);
}

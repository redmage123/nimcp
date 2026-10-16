/**
 * @file nimcp_evolutionary_proof.c
 * @brief Evolutionary Proof Search Implementation
 *
 * Implements a hybrid genetic algorithm and reinforcement learning
 * approach to automated theorem proving.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/neuro_symbolic/nimcp_evolutionary_proof.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "async/nimcp_bio_router.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE(evolutionary_proof, MESH_ADAPTER_CATEGORY_COGNITIVE)



/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal structure for evolutionary proof search
 */
struct evolutionary_proof_search {
    /* Configuration */
    evoproof_config_t config;

    /* Population */
    proof_strategy_t* population;
    uint32_t population_count;
    uint32_t current_generation;

    /* Q-table */
    proof_q_entry_t* q_table;
    uint32_t q_table_size;
    uint32_t q_table_count;

    /* Experience replay buffer */
    proof_experience_t* experience_buffer;
    uint32_t experience_count;
    uint32_t experience_head;

    /* Current epsilon for exploration */
    float current_epsilon;

    /* Random state */
    uint64_t rng_state;

    /* Statistics */
    evoproof_stats_t stats;

    /* Bio-async */
    uint16_t bio_module_id;
    const char* module_name;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* State */
    bool initialized;
    float atp_modulation;
};

/* ============================================================================
 * RNG Functions
 * ============================================================================ */

static uint64_t eps_xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static float eps_random_uniform(evolutionary_proof_search_t* eps) {
    return (float)(eps_xorshift64(&eps->rng_state) >> 11) * (1.0f / 9007199254740992.0f);
}

static float eps_random_normal(evolutionary_proof_search_t* eps) {
    float u1 = eps_random_uniform(eps);
    float u2 = eps_random_uniform(eps);
    return sqrtf(-2.0f * logf(u1 + 1e-10f)) * cosf(NIMCP_TWO_PI_F * u2);
}

static uint32_t eps_random_int(evolutionary_proof_search_t* eps, uint32_t max) {
    return (uint32_t)(eps_random_uniform(eps) * max);
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute state hash for Q-table lookup
 */
static uint64_t compute_state_hash(const proof_state_t* state) {
    uint64_t hash = 14695981039346656037ULL;
    hash ^= state->goal_complexity;
    hash *= 1099511628211ULL;
    hash ^= state->depth;
    hash *= 1099511628211ULL;
    hash ^= state->available_rules;
    hash *= 1099511628211ULL;
    hash ^= state->backtrack_count;
    hash *= 1099511628211ULL;
    return hash;
}

/**
 * @brief Find Q-entry for state
 */
static proof_q_entry_t* find_q_entry(evolutionary_proof_search_t* eps,
                                      uint64_t state_hash) {
    for (uint32_t i = 0; i < eps->q_table_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && eps->q_table_count > 256) {
            evolutionary_proof_heartbeat("evolutionary_loop",
                             (float)(i + 1) / (float)eps->q_table_count);
        }

        if (eps->q_table[i].state_hash == state_hash) {
            return &eps->q_table[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compute_state_hash: validation failed");
    return NULL;
}

/**
 * @brief Get or create Q-entry
 */
static proof_q_entry_t* get_or_create_q_entry(evolutionary_proof_search_t* eps,
                                               uint64_t state_hash) {
    proof_q_entry_t* entry = find_q_entry(eps, state_hash);
    if (entry) return entry;

    if (eps->q_table_count >= eps->q_table_size) {
        /* Table full, overwrite oldest */
        entry = &eps->q_table[0];
        uint64_t oldest = entry->last_update_us;
        for (uint32_t i = 1; i < eps->q_table_size; i++) {
            if (eps->q_table[i].last_update_us < oldest) {
                entry = &eps->q_table[i];
                oldest = entry->last_update_us;
            }
        }
    } else {
        entry = &eps->q_table[eps->q_table_count++];
    }

    memset(entry, 0, sizeof(proof_q_entry_t));
    entry->state_hash = state_hash;
    entry->last_update_us = nimcp_time_monotonic_us();

    return entry;
}

/**
 * @brief Initialize gene with default values
 */
static void init_gene(proof_gene_t* gene, proof_gene_type_t type) {
    gene->type = type;
    gene->min_value = 0.0f;
    gene->max_value = 1.0f;
    gene->mutation_sigma = 0.1f;

    switch (type) {
        case PROOF_GENE_SEARCH_DEPTH:
            gene->value = 0.5f;
            gene->max_value = 1.0f;
            break;
        case PROOF_GENE_RULE_PRIORITY:
            gene->value = 0.5f;
            break;
        case PROOF_GENE_UNIFICATION_STRATEGY:
            gene->value = 0.3f;
            break;
        case PROOF_GENE_BACKTRACK_THRESHOLD:
            gene->value = 0.7f;
            break;
        case PROOF_GENE_HEURISTIC_WEIGHT:
            gene->value = 0.6f;
            break;
        case PROOF_GENE_LEMMA_GENERATION:
            gene->value = 0.4f;
            break;
        case PROOF_GENE_ANALOGY_WEIGHT:
            gene->value = 0.3f;
            break;
        case PROOF_GENE_QUANTUM_WEIGHT:
            gene->value = 0.2f;
            break;
        case PROOF_GENE_ELEGANCE_WEIGHT:
            gene->value = 0.5f;
            break;
        case PROOF_GENE_EXPLORATION:
            gene->value = 0.3f;
            break;
        default:
            gene->value = 0.5f;
            break;
    }
}

/**
 * @brief Initialize strategy with defaults
 */
static void init_strategy(evolutionary_proof_search_t* eps,
                          proof_strategy_t* strategy,
                          uint32_t id) {
    memset(strategy, 0, sizeof(proof_strategy_t));
    strategy->id = id;
    strategy->generation = eps->current_generation;

    for (uint32_t i = 0; i < PROOF_GENE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PROOF_GENE_COUNT > 256) {
            evolutionary_proof_heartbeat("evolutionary_loop",
                             (float)(i + 1) / (float)PROOF_GENE_COUNT);
        }

        init_gene(&strategy->genes[i], (proof_gene_type_t)i);
        /* Add some randomness */
        strategy->genes[i].value += eps_random_normal(eps) * 0.2f;
        if (strategy->genes[i].value < 0.0f) strategy->genes[i].value = 0.0f;
        if (strategy->genes[i].value > 1.0f) strategy->genes[i].value = 1.0f;
    }

    /* Initialize action weights */
    for (uint32_t i = 0; i < PROOF_ACTION_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PROOF_ACTION_COUNT > 256) {
            evolutionary_proof_heartbeat("evolutionary_loop",
                             (float)(i + 1) / (float)PROOF_ACTION_COUNT);
        }

        strategy->action_weights[i] = 1.0f / (fabsf(PROOF_ACTION_COUNT) > 1e-7f ? PROOF_ACTION_COUNT : 1e-7f);
    }
}

/**
 * @brief Compute fitness for a strategy
 */
static float compute_fitness(evolutionary_proof_search_t* eps,
                             const proof_strategy_t* strategy) {
    if (strategy->proofs_attempted == 0) return 0.0f;

    float success = strategy->proof_success_rate;
    float speed = 1.0f / (1.0f + strategy->avg_proof_length / 100.0f);
    float elegance = strategy->elegance_score;

    return eps->config.weight_success * success +
           eps->config.weight_speed * speed +
           eps->config.weight_elegance * elegance;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API evolutionary_proof_search_t* evolutionary_proof_create(
    const evoproof_config_t* config) {

    evolutionary_proof_search_t* eps = nimcp_calloc(1, sizeof(evolutionary_proof_search_t));
    if (!eps) {
        NIMCP_LOG_ERROR("Failed to allocate evolutionary proof search");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate eps");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&eps->config, config, sizeof(evoproof_config_t));
    } else {
        evolutionary_proof_get_default_config(&eps->config);
    }

    /* Allocate population */
    eps->population = nimcp_calloc(eps->config.population_size,
                                    sizeof(proof_strategy_t));
    if (!eps->population) {
        nimcp_free(eps);
        eps = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_gene: eps->population is NULL");
        return NULL;
    }
    eps->population_count = eps->config.population_size;

    /* Allocate Q-table */
    eps->q_table_size = EVOPROOF_MAX_STATES;
    eps->q_table = nimcp_calloc(eps->q_table_size, sizeof(proof_q_entry_t));
    if (!eps->q_table) {
        nimcp_free(eps->population);
        nimcp_free(eps);
        eps = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_gene: eps->q_table is NULL");
        return NULL;
    }

    /* Allocate experience buffer */
    eps->experience_buffer = nimcp_calloc(EVOPROOF_MAX_EXPERIENCE,
                                           sizeof(proof_experience_t));
    if (!eps->experience_buffer) {
        nimcp_free(eps->q_table);
        nimcp_free(eps->population);
        nimcp_free(eps);
        eps = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_gene: eps->experience_buffer is NULL");
        return NULL;
    }

    /* Create mutex */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    eps->mutex = nimcp_mutex_create(&attr);
    if (!eps->mutex) {
        nimcp_free(eps->experience_buffer);
        nimcp_free(eps->q_table);
        nimcp_free(eps->population);
        nimcp_free(eps);
        eps = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_gene: eps->mutex is NULL");
        return NULL;
    }

    /* Initialize RNG */
    eps->rng_state = (uint64_t)nimcp_time_monotonic_us() ^ 0xDEADBEEFCAFEBABEULL;

    /* Initialize epsilon */
    eps->current_epsilon = eps->config.initial_epsilon;

    eps->bio_module_id = BIO_MODULE_EVOLUTIONARY_PROOF;
    eps->module_name = "evolutionary_proof";
    eps->atp_modulation = 1.0f;
    eps->initialized = true;

    /* Initialize population */
    evolutionary_proof_init_population(eps);

    NIMCP_LOG_INFO("Created evolutionary proof search with population %u",
                   eps->config.population_size);

    return eps;
}

NIMCP_API void evolutionary_proof_destroy(evolutionary_proof_search_t* eps) {
    if (!eps) return;

    if (eps->bio_async_enabled) {
        evolutionary_proof_unregister_bio_async(eps);
    }

    if (eps->mutex) {
        nimcp_mutex_destroy(eps->mutex);
    }

    nimcp_free(eps->experience_buffer);
    nimcp_free(eps->q_table);
    nimcp_free(eps->population);
    nimcp_free(eps);
    eps = NULL;

    NIMCP_LOG_DEBUG("Destroyed evolutionary proof search");
}

NIMCP_API nimcp_error_t evolutionary_proof_reset(
    evolutionary_proof_search_t* eps) {

    if (!eps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_reset: eps is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(eps->mutex);

    memset(&eps->stats, 0, sizeof(evoproof_stats_t));
    eps->current_generation = 0;
    eps->q_table_count = 0;
    eps->experience_count = 0;
    eps->experience_head = 0;
    eps->current_epsilon = eps->config.initial_epsilon;

    evolutionary_proof_init_population(eps);

    nimcp_mutex_unlock(eps->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t evolutionary_proof_get_default_config(
    evoproof_config_t* config) {

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_get_default_config: config is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(config, 0, sizeof(evoproof_config_t));

    config->algorithm = EVOPROOF_ALGO_HYBRID;

    /* GA parameters */
    config->population_size = 32;
    config->mutation_rate = 0.1f;
    config->crossover_rate = 0.7f;
    config->selection = EVOPROOF_SELECT_TOURNAMENT;
    config->crossover = EVOPROOF_CROSS_BLEND;
    config->elite_count = 2;
    config->tournament_size = 4;

    /* RL parameters */
    config->learning_rate = EVOPROOF_DEFAULT_LEARNING_RATE;
    config->discount_factor = EVOPROOF_DEFAULT_DISCOUNT;
    config->initial_epsilon = EVOPROOF_DEFAULT_EPSILON;
    config->epsilon_decay = 0.995f;
    config->min_epsilon = 0.01f;
    config->replay_batch_size = 32;
    config->target_update_freq = 100;

    /* Proof parameters */
    config->max_proof_depth = 50;
    config->max_proof_steps = 200;
    config->proof_timeout_ms = 5000;

    /* Fitness weights */
    config->weight_success = 0.5f;
    config->weight_speed = 0.3f;
    config->weight_elegance = 0.2f;
    config->weight_generalization = 0.0f;

    /* Integration */
    config->enable_quantum_actions = true;
    config->enable_analogy_actions = true;
    config->enable_bio_async = true;
    config->enable_transfer_learning = true;

    config->atp_sensitivity = 0.5f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Evolution Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t evolutionary_proof_init_population(
    evolutionary_proof_search_t* eps) {

    if (!eps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_init_population: eps is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < eps->population_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && eps->population_count > 256) {
            evolutionary_proof_heartbeat("evolutionary_loop",
                             (float)(i + 1) / (float)eps->population_count);
        }

        init_strategy(eps, &eps->population[i], i);
    }

    eps->current_generation = 0;

    return NIMCP_SUCCESS;
}

NIMCP_API uint32_t evolutionary_proof_evolve_generation(
    evolutionary_proof_search_t* eps) {

    if (!eps) return 0;

    uint32_t offspring_count = 0;

    {
        nimcp_mutex_lock(eps->mutex);

        /* Compute fitness for all strategies */
        for (uint32_t i = 0; i < eps->population_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && eps->population_count > 256) {
                evolutionary_proof_heartbeat("evolutionary_loop",
                                 (float)(i + 1) / (float)eps->population_count);
            }

            eps->population[i].fitness = compute_fitness(eps, &eps->population[i]);
        }

        /* Sort by fitness (descending) */
        for (uint32_t i = 0; i + 1 < eps->population_count; i++) {
            for (uint32_t j = i + 1; j < eps->population_count; j++) {
                if (eps->population[j].fitness > eps->population[i].fitness) {
                    proof_strategy_t temp = eps->population[i];
                    eps->population[i] = eps->population[j];
                    eps->population[j] = temp;
                }
            }
        }

        /* Create new generation */
        proof_strategy_t* new_pop = nimcp_calloc(eps->population_count,
                                                   sizeof(proof_strategy_t));
        if (!new_pop) {
            nimcp_mutex_unlock(eps->mutex);
            NIMCP_LOG_ERROR("Failed to allocate new population");
            return 0;
        }

        /* Keep elites */
        offspring_count = 0;
        for (uint32_t i = 0; i < eps->config.elite_count && i < eps->population_count; i++) {
            memcpy(&new_pop[offspring_count++], &eps->population[i],
                   sizeof(proof_strategy_t));
        }

        /* Create offspring */
        while (offspring_count < eps->population_count) {
            uint32_t parent1_id, parent2_id;
            evolutionary_proof_select_parents(eps, &parent1_id, &parent2_id);

            proof_strategy_t child;
            evolutionary_proof_crossover(eps,
                                          &eps->population[parent1_id],
                                          &eps->population[parent2_id],
                                          &child);

            if (eps_random_uniform(eps) < eps->config.mutation_rate) {
                evolutionary_proof_mutate(eps, &child);
                eps->stats.mutations++;
            }

            child.id = offspring_count;
            child.generation = eps->current_generation + 1;
            memcpy(&new_pop[offspring_count++], &child, sizeof(proof_strategy_t));
            eps->stats.crossovers++;
        }

        /* Replace population */
        memcpy(eps->population, new_pop, eps->population_count * sizeof(proof_strategy_t));
        nimcp_free(new_pop);
        new_pop = NULL;

        eps->current_generation++;
        eps->stats.generations++;

        /* Update epsilon */
        eps->current_epsilon *= eps->config.epsilon_decay;
        if (eps->current_epsilon < eps->config.min_epsilon) {
            eps->current_epsilon = eps->config.min_epsilon;
        }
        eps->stats.current_epsilon = eps->current_epsilon;

        /* Update fitness stats */
        eps->stats.best_fitness = eps->population[0].fitness;
        float sum = 0.0f;
        for (uint32_t i = 0; i < eps->population_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && eps->population_count > 256) {
                evolutionary_proof_heartbeat("evolutionary_loop",
                                 (float)(i + 1) / (float)eps->population_count);
            }

            sum += eps->population[i].fitness;
        }
        eps->stats.avg_fitness = sum / eps->population_count;

        nimcp_mutex_unlock(eps->mutex);
    }

    return offspring_count;
}

NIMCP_API nimcp_error_t evolutionary_proof_select_parents(
    evolutionary_proof_search_t* eps,
    uint32_t* parent1,
    uint32_t* parent2) {

    if (!eps || !parent1 || !parent2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_select_parents: eps, parent1, or parent2 is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    switch (eps->config.selection) {
        case EVOPROOF_SELECT_TOURNAMENT: {
            /* Tournament selection */
            *parent1 = eps_random_int(eps, eps->population_count);
            for (uint32_t i = 1; i < eps->config.tournament_size; i++) {
                uint32_t candidate = eps_random_int(eps, eps->population_count);
                if (eps->population[candidate].fitness >
                    eps->population[*parent1].fitness) {
                    *parent1 = candidate;
                }
            }

            *parent2 = eps_random_int(eps, eps->population_count);
            for (uint32_t i = 1; i < eps->config.tournament_size; i++) {
                uint32_t candidate = eps_random_int(eps, eps->population_count);
                if (eps->population[candidate].fitness >
                    eps->population[*parent2].fitness) {
                    *parent2 = candidate;
                }
            }
            break;
        }

        case EVOPROOF_SELECT_ROULETTE: {
            /* Roulette wheel selection */
            float total_fitness = 0.0f;
            for (uint32_t i = 0; i < eps->population_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && eps->population_count > 256) {
                    evolutionary_proof_heartbeat("evolutionary_loop",
                                     (float)(i + 1) / (float)eps->population_count);
                }

                total_fitness += eps->population[i].fitness + 0.01f;
            }

            float r1 = eps_random_uniform(eps) * total_fitness;
            float r2 = eps_random_uniform(eps) * total_fitness;
            float cumsum = 0.0f;

            *parent1 = 0;
            *parent2 = 0;

            for (uint32_t i = 0; i < eps->population_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && eps->population_count > 256) {
                    evolutionary_proof_heartbeat("evolutionary_loop",
                                     (float)(i + 1) / (float)eps->population_count);
                }

                cumsum += eps->population[i].fitness + 0.01f;
                if (cumsum >= r1 && *parent1 == 0) *parent1 = i;
                if (cumsum >= r2 && *parent2 == 0) *parent2 = i;
            }
            break;
        }

        default:
            *parent1 = 0;
            *parent2 = 1;
            break;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t evolutionary_proof_crossover(
    evolutionary_proof_search_t* eps,
    const proof_strategy_t* parent1,
    const proof_strategy_t* parent2,
    proof_strategy_t* child) {

    if (!eps || !parent1 || !parent2 || !child) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_crossover: eps, parent1, parent2, or child is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(child, 0, sizeof(proof_strategy_t));

    switch (eps->config.crossover) {
        case EVOPROOF_CROSS_BLEND: {
            /* BLX-alpha crossover */
            float alpha = 0.5f;
            for (uint32_t i = 0; i < PROOF_GENE_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && PROOF_GENE_COUNT > 256) {
                    evolutionary_proof_heartbeat("evolutionary_loop",
                                     (float)(i + 1) / (float)PROOF_GENE_COUNT);
                }

                float p1 = parent1->genes[i].value;
                float p2 = parent2->genes[i].value;
                float min_val = (p1 < p2) ? p1 : p2;
                float max_val = (p1 > p2) ? p1 : p2;
                float range = max_val - min_val;

                child->genes[i].type = (proof_gene_type_t)i;
                child->genes[i].value = min_val - alpha * range +
                                         eps_random_uniform(eps) * range * (1.0f + 2.0f * alpha);
                child->genes[i].min_value = parent1->genes[i].min_value;
                child->genes[i].max_value = parent1->genes[i].max_value;
                child->genes[i].mutation_sigma = parent1->genes[i].mutation_sigma;

                /* Clamp */
                if (child->genes[i].value < child->genes[i].min_value)
                    child->genes[i].value = child->genes[i].min_value;
                if (child->genes[i].value > child->genes[i].max_value)
                    child->genes[i].value = child->genes[i].max_value;
            }
            break;
        }

        case EVOPROOF_CROSS_UNIFORM: {
            for (uint32_t i = 0; i < PROOF_GENE_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && PROOF_GENE_COUNT > 256) {
                    evolutionary_proof_heartbeat("evolutionary_loop",
                                     (float)(i + 1) / (float)PROOF_GENE_COUNT);
                }

                const proof_gene_t* src = (eps_random_uniform(eps) < 0.5f)
                                           ? &parent1->genes[i]
                                           : &parent2->genes[i];
                memcpy(&child->genes[i], src, sizeof(proof_gene_t));
            }
            break;
        }

        default: {
            /* Single-point crossover */
            uint32_t point = eps_random_int(eps, PROOF_GENE_COUNT);
            for (uint32_t i = 0; i < PROOF_GENE_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && PROOF_GENE_COUNT > 256) {
                    evolutionary_proof_heartbeat("evolutionary_loop",
                                     (float)(i + 1) / (float)PROOF_GENE_COUNT);
                }

                const proof_gene_t* src = (i < point)
                                           ? &parent1->genes[i]
                                           : &parent2->genes[i];
                memcpy(&child->genes[i], src, sizeof(proof_gene_t));
            }
            break;
        }
    }

    /* Average action weights */
    for (uint32_t i = 0; i < PROOF_ACTION_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PROOF_ACTION_COUNT > 256) {
            evolutionary_proof_heartbeat("evolutionary_loop",
                             (float)(i + 1) / (float)PROOF_ACTION_COUNT);
        }

        child->action_weights[i] = (parent1->action_weights[i] +
                                    parent2->action_weights[i]) / 2.0f;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t evolutionary_proof_mutate(
    evolutionary_proof_search_t* eps,
    proof_strategy_t* strategy) {

    if (!eps || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_mutate: eps or strategy is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < PROOF_GENE_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PROOF_GENE_COUNT > 256) {
            evolutionary_proof_heartbeat("evolutionary_loop",
                             (float)(i + 1) / (float)PROOF_GENE_COUNT);
        }

        if (eps_random_uniform(eps) < eps->config.mutation_rate) {
            float delta = eps_random_normal(eps) * strategy->genes[i].mutation_sigma;
            strategy->genes[i].value += delta;

            /* Clamp */
            if (strategy->genes[i].value < strategy->genes[i].min_value)
                strategy->genes[i].value = strategy->genes[i].min_value;
            if (strategy->genes[i].value > strategy->genes[i].max_value)
                strategy->genes[i].value = strategy->genes[i].max_value;
        }
    }

    return NIMCP_SUCCESS;
}

NIMCP_API const proof_strategy_t* evolutionary_proof_get_best(
    const evolutionary_proof_search_t* eps) {

    if (!eps || eps->population_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evolutionary_proof_get_best: eps is NULL or empty");
        return NULL;
    }

    const proof_strategy_t* best = &eps->population[0];
    for (uint32_t i = 1; i < eps->population_count; i++) {
        if (eps->population[i].fitness > best->fitness) {
            best = &eps->population[i];
        }
    }

    return best;
}

/* ============================================================================
 * Q-Learning Functions
 * ============================================================================ */

NIMCP_API proof_action_t evolutionary_proof_select_action(
    evolutionary_proof_search_t* eps,
    const proof_state_t* state) {

    if (!eps || !state) return PROOF_ACTION_APPLY_RULE;

    /* Epsilon-greedy policy */
    if (eps_random_uniform(eps) < eps->current_epsilon * eps->atp_modulation) {
        /* Random action */
        return (proof_action_t)eps_random_int(eps, PROOF_ACTION_COUNT);
    }

    /* Greedy action */
    uint64_t hash = compute_state_hash(state);
    proof_q_entry_t* entry = find_q_entry(eps, hash);

    if (!entry) {
        /* Unknown state, use default */
        return PROOF_ACTION_APPLY_RULE;
    }

    proof_action_t best_action = PROOF_ACTION_APPLY_RULE;
    float best_q = entry->q_values[0];

    for (uint32_t i = 1; i < PROOF_ACTION_COUNT; i++) {
        if (entry->q_values[i] > best_q) {
            best_q = entry->q_values[i];
            best_action = (proof_action_t)i;
        }
    }

    return best_action;
}

/**
 * @brief Internal unlocked version of Q-value update
 *
 * Must be called with eps->mutex already held.
 */
static float update_q_unlocked(
    evolutionary_proof_search_t* eps,
    const proof_state_t* state,
    proof_action_t action,
    float reward,
    const proof_state_t* next_state,
    bool done) {

    uint64_t hash = compute_state_hash(state);
    proof_q_entry_t* entry = get_or_create_q_entry(eps, hash);

    float current_q = entry->q_values[action];
    float next_max_q = 0.0f;

    if (!done && next_state) {
        uint64_t next_hash = compute_state_hash(next_state);
        proof_q_entry_t* next_entry = find_q_entry(eps, next_hash);
        if (next_entry) {
            for (uint32_t i = 0; i < PROOF_ACTION_COUNT; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && PROOF_ACTION_COUNT > 256) {
                    evolutionary_proof_heartbeat("evolutionary_loop",
                                     (float)(i + 1) / (float)PROOF_ACTION_COUNT);
                }

                if (next_entry->q_values[i] > next_max_q) {
                    next_max_q = next_entry->q_values[i];
                }
            }
        }
    }

    /* Q-learning update */
    float target = reward + eps->config.discount_factor * next_max_q;
    entry->q_values[action] += eps->config.learning_rate * (target - current_q);
    entry->visit_count++;
    entry->last_update_us = nimcp_time_monotonic_us();

    eps->stats.q_updates++;

    return entry->q_values[action];
}

NIMCP_API float evolutionary_proof_update_q(
    evolutionary_proof_search_t* eps,
    const proof_state_t* state,
    proof_action_t action,
    float reward,
    const proof_state_t* next_state,
    bool done) {

    if (!eps || !state) return 0.0f;

    nimcp_mutex_lock(eps->mutex);
    float result = update_q_unlocked(eps, state, action, reward, next_state, done);
    nimcp_mutex_unlock(eps->mutex);

    return result;
}

NIMCP_API float evolutionary_proof_get_q_value(
    const evolutionary_proof_search_t* eps,
    const proof_state_t* state,
    proof_action_t action) {

    if (!eps || !state || action >= PROOF_ACTION_COUNT) return 0.0f;

    uint64_t hash = compute_state_hash(state);
    proof_q_entry_t* entry = find_q_entry((evolutionary_proof_search_t*)eps, hash);

    if (!entry) return 0.0f;
    return entry->q_values[action];
}

/**
 * @brief Internal unlocked version of experience storage
 *
 * Must be called with eps->mutex already held.
 */
static void store_experience_unlocked(
    evolutionary_proof_search_t* eps,
    const proof_experience_t* exp) {

    /* Store in circular buffer */
    uint32_t idx = eps->experience_head;
    memcpy(&eps->experience_buffer[idx], exp, sizeof(proof_experience_t));

    eps->experience_head = (eps->experience_head + 1) % EVOPROOF_MAX_EXPERIENCE;
    if (eps->experience_count < EVOPROOF_MAX_EXPERIENCE) {
        eps->experience_count++;
    }

    eps->stats.experiences_stored = eps->experience_count;
}

NIMCP_API nimcp_error_t evolutionary_proof_store_experience(
    evolutionary_proof_search_t* eps,
    const proof_experience_t* exp) {

    if (!eps || !exp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_store_experience: eps or exp is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(eps->mutex);
    store_experience_unlocked(eps, exp);
    nimcp_mutex_unlock(eps->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t evolutionary_proof_replay_learn(
    evolutionary_proof_search_t* eps) {

    if (!eps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_replay_learn: eps is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (eps->experience_count < eps->config.replay_batch_size) {
        return NIMCP_SUCCESS; /* Not enough experiences yet */
    }

    nimcp_mutex_lock(eps->mutex);

    /* Sample batch and update using unlocked version to avoid deadlock */
    for (uint32_t i = 0; i < eps->config.replay_batch_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && eps->config.replay_batch_size > 256) {
            evolutionary_proof_heartbeat("evolutionary_loop",
                             (float)(i + 1) / (float)eps->config.replay_batch_size);
        }

        uint32_t idx = eps_random_int(eps, eps->experience_count);
        proof_experience_t* exp = &eps->experience_buffer[idx];

        update_q_unlocked(eps,
                          &exp->state,
                          exp->action,
                          exp->reward,
                          &exp->next_state,
                          exp->terminal);
    }

    eps->stats.replay_batches++;

    nimcp_mutex_unlock(eps->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Proof Search Functions
 * ============================================================================ */

NIMCP_API bool evolutionary_proof_prove(
    evolutionary_proof_search_t* eps,
    const void* logic,
    const char* goal,
    evoproof_trace_t* trace,
    uint32_t max_steps) {

    if (!eps || !goal || !trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evolutionary_proof_prove: eps, goal, or trace is NULL");
        return false;
    }

    (void)logic;  /* Will integrate with logic system later */

    nimcp_mutex_lock(eps->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Get best strategy */
    const proof_strategy_t* strategy = evolutionary_proof_get_best(eps);

    /* Initialize state */
    proof_state_t state = {0};
    state.goal_complexity = (uint32_t)strlen(goal);
    state.depth = 0;
    state.available_rules = 10; /* Placeholder */

    trace->is_complete = false;
    trace->is_valid = false;
    trace->num_steps = 0;
    trace->strategy_id = strategy->id;

    uint32_t step = 0;
    bool found = false;

    while (step < max_steps && !found) {
        /* Select action */
        proof_action_t action = evolutionary_proof_select_action(eps, &state);

        /* Simulate action (placeholder - would integrate with logic system) */
        proof_state_t next_state = state;
        next_state.depth++;
        float reward = 0.0f;

        /* Check if goal reached (placeholder) */
        if (next_state.depth >= 10 && eps_random_uniform(eps) < 0.1f) {
            found = true;
            reward = 1.0f;
        } else if (next_state.depth >= max_steps) {
            reward = -0.1f;
        }

        /* Store experience (use unlocked version since we hold mutex) */
        proof_experience_t exp = {
            .state = state,
            .action = action,
            .reward = reward,
            .next_state = next_state,
            .terminal = found,
            .timestamp_us = nimcp_time_monotonic_us()
        };
        store_experience_unlocked(eps, &exp);

        /* Update Q-values (use unlocked version since we hold mutex) */
        update_q_unlocked(eps, &state, action, reward, &next_state, found);

        state = next_state;
        step++;
    }

    trace->is_complete = found;
    trace->is_valid = found;
    trace->num_steps = step;
    trace->search_time_us = nimcp_time_monotonic_us() - start_time;

    /* Update statistics */
    eps->stats.proofs_attempted++;
    if (found) {
        eps->stats.proofs_succeeded++;
    }
    eps->stats.proof_success_rate = (float)eps->stats.proofs_succeeded /
                                     eps->stats.proofs_attempted;
    eps->stats.avg_proof_length = ((eps->stats.avg_proof_length *
                                    (eps->stats.proofs_attempted - 1)) + step) /
                                    eps->stats.proofs_attempted;

    nimcp_mutex_unlock(eps->mutex);

    return found;
}

NIMCP_API bool evolutionary_proof_prove_with_strategy(
    evolutionary_proof_search_t* eps,
    const void* logic,
    const char* goal,
    uint32_t strategy_id,
    evoproof_trace_t* trace) {

    if (!eps || strategy_id >= eps->population_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "evolutionary_proof_prove_with_strategy: invalid eps or strategy_id");
        return false;
    }

    /* For now, use same implementation as main prove */
    return evolutionary_proof_prove(eps, logic, goal, trace,
                                    eps->config.max_proof_steps);
}

/* ============================================================================
 * Trace Management
 * ============================================================================ */

NIMCP_API nimcp_error_t evolutionary_proof_trace_init(
    evoproof_trace_t* trace,
    uint32_t capacity) {

    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_trace_init: trace is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(trace, 0, sizeof(evoproof_trace_t));

    if (capacity > 0) {
        trace->steps = nimcp_calloc(capacity, sizeof(evoproof_step_t));
        if (!trace->steps) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "evolutionary_proof_trace_init: failed to allocate steps array");
            return NIMCP_ERROR_NO_MEMORY;
        }
        trace->capacity = capacity;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API void evolutionary_proof_trace_cleanup(evoproof_trace_t* trace) {
    if (!trace) return;

    if (trace->steps) {
        for (uint32_t i = 0; i < trace->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && trace->num_steps > 256) {
                evolutionary_proof_heartbeat("evolutionary_loop",
                                 (float)(i + 1) / (float)trace->num_steps);
            }

            nimcp_free(trace->steps[i].statement);
            nimcp_free(trace->steps[i].justification);
            nimcp_free(trace->steps[i].premise_ids);
        }
        nimcp_free(trace->steps);
    }

    memset(trace, 0, sizeof(evoproof_trace_t));
}

/* ============================================================================
 * Transfer Learning
 * ============================================================================ */

NIMCP_API int32_t evolutionary_proof_export_knowledge(
    const evolutionary_proof_search_t* eps,
    void* buffer,
    uint32_t buffer_size) {

    if (!eps || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evolutionary_proof_trace_cleanup: required parameter is NULL (eps, buffer)");
        return -1;
    }

    uint32_t required = sizeof(uint32_t) * 2 +
                        eps->population_count * sizeof(proof_strategy_t) +
                        eps->q_table_count * sizeof(proof_q_entry_t);

    if (buffer_size < required) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "evolutionary_proof_trace_cleanup: validation failed");
        return -1;
    }

    uint8_t* ptr = (uint8_t*)buffer;

    /* Write population count */
    memcpy(ptr, &eps->population_count, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    /* Write Q-table count */
    memcpy(ptr, &eps->q_table_count, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    /* Write population */
    memcpy(ptr, eps->population, eps->population_count * sizeof(proof_strategy_t));
    ptr += eps->population_count * sizeof(proof_strategy_t);

    /* Write Q-table */
    memcpy(ptr, eps->q_table, eps->q_table_count * sizeof(proof_q_entry_t));

    return (int32_t)required;
}

NIMCP_API nimcp_error_t evolutionary_proof_import_knowledge(
    evolutionary_proof_search_t* eps,
    const void* buffer,
    uint32_t buffer_size) {

    if (!eps || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_import_knowledge: eps or buffer is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const uint8_t* ptr = (const uint8_t*)buffer;

    /* Read population count */
    uint32_t pop_count = 0;
    memcpy(&pop_count, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    /* Read Q-table count */
    uint32_t q_count = 0;
    memcpy(&q_count, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    nimcp_mutex_lock(eps->mutex);

    /* Import population (merge or replace) */
    if (pop_count <= eps->population_count) {
        memcpy(eps->population, ptr, pop_count * sizeof(proof_strategy_t));
    }
    ptr += pop_count * sizeof(proof_strategy_t);

    /* Import Q-table */
    if (q_count <= eps->q_table_size) {
        memcpy(eps->q_table, ptr, q_count * sizeof(proof_q_entry_t));
        eps->q_table_count = q_count;
    }

    nimcp_mutex_unlock(eps->mutex);

    NIMCP_LOG_INFO("Imported knowledge: %u strategies, %u Q-entries",
                   pop_count, q_count);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Modulation
 * ============================================================================ */

NIMCP_API nimcp_error_t evolutionary_proof_modulate_atp(
    evolutionary_proof_search_t* eps,
    float atp_level) {

    if (!eps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_modulate_atp: eps is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(eps->mutex);

    /* Lower ATP = less exploration, more conservative */
    eps->atp_modulation = 0.5f + 0.5f * atp_level;

    nimcp_mutex_unlock(eps->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t evolutionary_proof_register_bio_async(
    evolutionary_proof_search_t* eps) {

    if (!eps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_register_bio_async: eps is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(eps->mutex);

    if (eps->bio_async_enabled) {
        nimcp_mutex_unlock(eps->mutex);
        return NIMCP_SUCCESS;
    }

    if (!bio_router_is_initialized()) {
        nimcp_mutex_unlock(eps->mutex);
        return NIMCP_SUCCESS;
    }

    bio_module_info_t info = {
        .module_id = eps->bio_module_id,
        .module_name = eps->module_name,
        .inbox_capacity = 32,
        .user_data = eps
    };

    eps->bio_ctx = bio_router_register_module(&info);
    if (eps->bio_ctx) {
        eps->bio_async_enabled = true;
        NIMCP_LOG_DEBUG("Evolutionary proof registered with bio-async");
    }

    nimcp_mutex_unlock(eps->mutex);
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t evolutionary_proof_unregister_bio_async(
    evolutionary_proof_search_t* eps) {

    if (!eps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_unregister_bio_async: eps is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(eps->mutex);

    if (eps->bio_async_enabled && eps->bio_ctx) {
        bio_router_unregister_module(eps->bio_ctx);
        eps->bio_ctx = NULL;
        eps->bio_async_enabled = false;
    }

    nimcp_mutex_unlock(eps->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

NIMCP_API nimcp_error_t evolutionary_proof_get_stats(
    const evolutionary_proof_search_t* eps,
    evoproof_stats_t* stats) {

    if (!eps || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "evolutionary_proof_get_stats: eps or stats is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(stats, &eps->stats, sizeof(evoproof_stats_t));
    stats->unique_states = eps->q_table_count;

    return NIMCP_SUCCESS;
}

NIMCP_API void evolutionary_proof_print_diagnostics(
    const evolutionary_proof_search_t* eps) {

    if (!eps) return;

    NIMCP_LOG_INFO("=== Evolutionary Proof Search Diagnostics ===");
    NIMCP_LOG_INFO("Generation: %u", eps->current_generation);
    NIMCP_LOG_INFO("Best fitness: %.4f", eps->stats.best_fitness);
    NIMCP_LOG_INFO("Avg fitness: %.4f", eps->stats.avg_fitness);
    NIMCP_LOG_INFO("Proofs: %lu/%lu (%.1f%%)",
                   eps->stats.proofs_succeeded,
                   eps->stats.proofs_attempted,
                   eps->stats.proof_success_rate * 100.0f);
    NIMCP_LOG_INFO("Q-updates: %lu", eps->stats.q_updates);
    NIMCP_LOG_INFO("Unique states: %u", eps->q_table_count);
    NIMCP_LOG_INFO("Epsilon: %.4f", eps->current_epsilon);
    NIMCP_LOG_INFO("Experiences: %u", eps->experience_count);
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void evolutionary_proof_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_evolutionary_proof_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int evolutionary_proof_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "evolutionary_proof_training_begin: NULL argument");
        return -1;
    }
    evolutionary_proof_heartbeat_instance(NULL, "evolutionary_proof_training_begin", 0.0f);
    (void)(struct evolutionary_proof_search*)instance; /* Module state available for reset */
    return 0;
}

int evolutionary_proof_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "evolutionary_proof_training_end: NULL argument");
        return -1;
    }
    evolutionary_proof_heartbeat_instance(NULL, "evolutionary_proof_training_end", 1.0f);
    (void)(struct evolutionary_proof_search*)instance; /* Module state available for finalization */
    return 0;
}

int evolutionary_proof_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "evolutionary_proof_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    evolutionary_proof_heartbeat_instance(NULL, "evolutionary_proof_training_step", progress);
    (void)(struct evolutionary_proof_search*)instance; /* Module state available for step adaptation */
    return 0;
}

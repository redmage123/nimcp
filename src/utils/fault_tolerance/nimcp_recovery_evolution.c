/**
 * @file nimcp_recovery_evolution.c
 * @brief Implementation of Recovery Strategy Evolution using Genetic Algorithms and RL
 * @version 1.0.0
 * @date 2025-12-11
 */

#include "utils/fault_tolerance/nimcp_recovery_evolution.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(recovery_evolution)

//=============================================================================
// Internal Structures
//=============================================================================

struct re_context {
    re_config_t config;
    re_strategy_t population[RE_MAX_POPULATION];
    uint32_t population_size;
    uint32_t generation;
    re_q_entry_t q_table[RE_MAX_STATES];
    uint32_t q_table_size;
    re_experience_t history[RE_MAX_HISTORY];
    uint32_t history_size;
    uint32_t history_head;
    float epsilon;
    uint32_t next_strategy_id;
    re_stats_t stats;
    nimcp_mutex_t lock;
    bool initialized;
};

//=============================================================================
// Random Number Generation
//=============================================================================

static float re_random_float(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

static uint32_t re_random_uint(uint32_t max) {
    return (uint32_t)(re_random_float() * max);
}

//=============================================================================
// Private Functions
//=============================================================================

/**
 * @brief Find Q-entry for state
 */
static re_q_entry_t* re_find_q_entry(re_context_t* ctx, uint32_t state) {
    for (uint32_t i = 0; i < ctx->q_table_size; i++) {
        if (ctx->q_table[i].state_id == state) {
            return &ctx->q_table[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_find_q_entry: validation failed");
    return NULL;
}

/**
 * @brief Get or create Q-entry
 */
static re_q_entry_t* re_get_or_create_q_entry(re_context_t* ctx, uint32_t state) {
    re_q_entry_t* entry = re_find_q_entry(ctx, state);
    if (entry) return entry;

    if (ctx->q_table_size >= RE_MAX_STATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "re_get_or_create_q_entry: capacity exceeded");
        return NULL;
    }

    entry = &ctx->q_table[ctx->q_table_size++];
    entry->state_id = state;
    memset(entry->q_values, 0, sizeof(entry->q_values));
    entry->visit_count = 0;

    ctx->stats.unique_states_visited++;
    return entry;
}

/**
 * @brief Calculate strategy fitness
 */
static float re_calc_fitness(re_context_t* ctx, const re_strategy_t* strategy, const re_outcome_t* outcome) {
    float fitness = 0.0f;

    switch (ctx->config.fitness_criteria) {
        case RE_FIT_RECOVERY_TIME:
            // Lower time = higher fitness, normalized to [0, 1]
            fitness = 1.0f / (1.0f + (float)outcome->recovery_time_ms / 1000.0f);
            break;

        case RE_FIT_SUCCESS_RATE:
            fitness = outcome->success ? 1.0f : 0.0f;
            break;

        case RE_FIT_RESOURCE_USAGE:
            fitness = 1.0f - outcome->resource_usage;
            break;

        case RE_FIT_DATA_LOSS:
            fitness = 1.0f - outcome->data_loss;
            break;

        case RE_FIT_COMPOSITE:
        default: {
            float w = ctx->config.fitness_weights[0] + ctx->config.fitness_weights[1] +
                     ctx->config.fitness_weights[2] + ctx->config.fitness_weights[3];
            if (w < 0.01f) w = 1.0f;

            float time_score = 1.0f / (1.0f + (float)outcome->recovery_time_ms / 1000.0f);
            float success_score = outcome->success ? 1.0f : 0.0f;
            float resource_score = 1.0f - outcome->resource_usage;
            float data_score = 1.0f - outcome->data_loss;

            fitness = (ctx->config.fitness_weights[0] * time_score +
                      ctx->config.fitness_weights[1] * success_score +
                      ctx->config.fitness_weights[2] * resource_score +
                      ctx->config.fitness_weights[3] * data_score) / w;
            break;
        }
    }

    return fitness;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

re_config_t re_default_config(void) {
    re_config_t config = {
        .algorithm = RE_ALGO_HYBRID,
        .population_size = 32,
        .elite_count = 4,
        .mutation_rate = RE_DEFAULT_MUTATION_RATE,
        .crossover_rate = RE_DEFAULT_CROSSOVER_RATE,
        .selection = RE_SELECT_TOURNAMENT,
        .crossover = RE_CROSS_UNIFORM,
        .learning_rate = RE_DEFAULT_LEARNING_RATE,
        .discount_factor = RE_DEFAULT_DISCOUNT,
        .epsilon = 0.3f,
        .epsilon_decay = 0.995f,
        .min_epsilon = 0.01f,
        .fitness_criteria = RE_FIT_COMPOSITE,
        .fitness_weights = {0.3f, 0.4f, 0.2f, 0.1f, 0.0f},
        .history_size = RE_MAX_HISTORY,
        .batch_size = NIMCP_DEFAULT_BATCH_SIZE,
        .enable_transfer = true
    };
    return config;
}

re_context_t* re_create(const re_config_t* config) {
    if (!config) {
        LOG_ERROR("RE", "NULL config provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    re_context_t* ctx = nimcp_malloc(sizeof(re_context_t));
    if (!ctx) {
        LOG_ERROR("RE", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    memset(ctx, 0, sizeof(re_context_t));
    ctx->config = *config;
    ctx->epsilon = config->epsilon;
    ctx->next_strategy_id = 1;

    if (nimcp_mutex_init(&ctx->lock, NULL) != 0) {
        LOG_ERROR("RE", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "re_create: validation failed");
        return NULL;
    }

    ctx->initialized = true;

    // Register with security
    bbb_register_module("recovery_evolution", BBB_MODULE_TYPE_CORE);
    bbb_audit_log(BBB_AUDIT_INFO, "RE", "CREATE", "Created recovery evolution context");

    LOG_INFO("RE", "Created recovery evolution context");
    return ctx;
}

void re_destroy(re_context_t* ctx) {
    if (!ctx) return;
    nimcp_mutex_destroy(&ctx->lock);
    nimcp_free(ctx);
}

//=============================================================================
// Genetic Algorithm Functions
//=============================================================================

bool re_init_population(re_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_init_population: required parameter is NULL (ctx, ctx->initialized)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Create random initial population
    for (uint32_t i = 0; i < ctx->config.population_size && i < RE_MAX_POPULATION; i++) {
        re_strategy_t* strategy = &ctx->population[i];

        strategy->id = ctx->next_strategy_id++;
        strategy->generation = 0;
        strategy->fitness = 0.0f;

        // Create random genes
        strategy->gene_count = 4 + re_random_uint(4);
        for (uint32_t j = 0; j < strategy->gene_count; j++) {
            snprintf(strategy->genes[j].name, sizeof(strategy->genes[j].name), "gene_%u", j);
            strategy->genes[j].value = re_random_float();
            strategy->genes[j].min_value = 0.0f;
            strategy->genes[j].max_value = 1.0f;
            strategy->genes[j].mutation_sigma = 0.1f;
            strategy->genes[j].is_integer = false;
        }

        // Create random action sequence
        strategy->action_count = 2 + re_random_uint(3);
        for (uint32_t j = 0; j < strategy->action_count; j++) {
            strategy->actions[j] = (re_action_t)re_random_uint(RE_ACTION_COUNT);
        }
    }

    ctx->population_size = ctx->config.population_size;
    ctx->generation = 0;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("RE", "Initialized population with %u strategies", ctx->population_size);
    return true;
}

bool re_add_strategy(re_context_t* ctx, const re_strategy_t* strategy) {
    if (!ctx || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_add_strategy: required parameter is NULL (ctx, strategy)");
        return false;
    }
    if (ctx->population_size >= RE_MAX_POPULATION) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "re_add_strategy: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    ctx->population[ctx->population_size++] = *strategy;
    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

float re_evaluate_fitness(re_context_t* ctx, uint32_t strategy_id, const re_outcome_t* outcome) {
    if (!ctx || !outcome) return 0.0f;

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->population_size; i++) {
        if (ctx->population[i].id == strategy_id) {
            re_strategy_t* strategy = &ctx->population[i];

            float new_fitness = re_calc_fitness(ctx, strategy, outcome);

            // Exponential moving average
            strategy->fitness = 0.7f * strategy->fitness + 0.3f * new_fitness;
            strategy->times_used++;

            if (outcome->success) {
                strategy->times_succeeded++;
            }

            // Update avg recovery time
            uint64_t total = strategy->avg_recovery_time_ms * (strategy->times_used - 1);
            strategy->avg_recovery_time_ms = (total + outcome->recovery_time_ms) / strategy->times_used;

            ctx->stats.total_evaluations++;

            nimcp_mutex_unlock(&ctx->lock);
            return strategy->fitness;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    return 0.0f;
}

bool re_select_parents(re_context_t* ctx, re_strategy_t* parent1, re_strategy_t* parent2) {
    if (!ctx || !parent1 || !parent2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_select_parents: required parameter is NULL (ctx, parent1, parent2)");
        return false;
    }
    if (ctx->population_size < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "re_select_parents: validation failed");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    switch (ctx->config.selection) {
        case RE_SELECT_TOURNAMENT: {
            // Tournament selection
            uint32_t t1a = re_random_uint(ctx->population_size);
            uint32_t t1b = re_random_uint(ctx->population_size);
            uint32_t p1 = (ctx->population[t1a].fitness > ctx->population[t1b].fitness) ? t1a : t1b;

            uint32_t t2a = re_random_uint(ctx->population_size);
            uint32_t t2b = re_random_uint(ctx->population_size);
            uint32_t p2 = (ctx->population[t2a].fitness > ctx->population[t2b].fitness) ? t2a : t2b;

            *parent1 = ctx->population[p1];
            *parent2 = ctx->population[p2];
            break;
        }

        case RE_SELECT_ROULETTE: {
            // Calculate total fitness
            float total = 0.0f;
            for (uint32_t i = 0; i < ctx->population_size; i++) {
                total += ctx->population[i].fitness + 1.0f; // +1 to handle negative fitness
            }

            // Select parent 1
            float r1 = re_random_float() * total;
            float sum = 0.0f;
            for (uint32_t i = 0; i < ctx->population_size; i++) {
                sum += ctx->population[i].fitness + 1.0f;
                if (sum >= r1) {
                    *parent1 = ctx->population[i];
                    break;
                }
            }

            // Select parent 2
            float r2 = re_random_float() * total;
            sum = 0.0f;
            for (uint32_t i = 0; i < ctx->population_size; i++) {
                sum += ctx->population[i].fitness + 1.0f;
                if (sum >= r2) {
                    *parent2 = ctx->population[i];
                    break;
                }
            }
            break;
        }

        default:
            // Random selection
            *parent1 = ctx->population[re_random_uint(ctx->population_size)];
            *parent2 = ctx->population[re_random_uint(ctx->population_size)];
            break;
    }

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

bool re_crossover(re_context_t* ctx, const re_strategy_t* parent1, const re_strategy_t* parent2, re_strategy_t* child) {
    if (!ctx || !parent1 || !parent2 || !child) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_crossover: required parameter is NULL (ctx, parent1, parent2, child)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    child->id = ctx->next_strategy_id++;
    child->generation = ctx->generation + 1;
    child->fitness = 0.0f;
    child->times_used = 0;
    child->times_succeeded = 0;
    child->avg_recovery_time_ms = 0;

    switch (ctx->config.crossover) {
        case RE_CROSS_UNIFORM:
            // Uniform crossover for genes
            child->gene_count = (parent1->gene_count + parent2->gene_count) / 2;
            for (uint32_t i = 0; i < child->gene_count; i++) {
                if (re_random_float() < 0.5f && i < parent1->gene_count) {
                    child->genes[i] = parent1->genes[i];
                } else if (i < parent2->gene_count) {
                    child->genes[i] = parent2->genes[i];
                }
            }

            // Uniform crossover for actions
            child->action_count = (parent1->action_count + parent2->action_count) / 2;
            for (uint32_t i = 0; i < child->action_count; i++) {
                if (re_random_float() < 0.5f && i < parent1->action_count) {
                    child->actions[i] = parent1->actions[i];
                } else if (i < parent2->action_count) {
                    child->actions[i] = parent2->actions[i];
                }
            }
            break;

        case RE_CROSS_SINGLE: {
            // Single-point crossover
            uint32_t gene_point = re_random_uint(parent1->gene_count);
            child->gene_count = parent1->gene_count;
            for (uint32_t i = 0; i < gene_point && i < parent1->gene_count; i++) {
                child->genes[i] = parent1->genes[i];
            }
            for (uint32_t i = gene_point; i < parent2->gene_count && i < RE_MAX_GENES; i++) {
                child->genes[i] = parent2->genes[i];
                child->gene_count = i + 1;
            }

            uint32_t action_point = re_random_uint(parent1->action_count);
            child->action_count = parent1->action_count;
            for (uint32_t i = 0; i < action_point && i < parent1->action_count; i++) {
                child->actions[i] = parent1->actions[i];
            }
            for (uint32_t i = action_point; i < parent2->action_count && i < RE_MAX_ACTIONS; i++) {
                child->actions[i] = parent2->actions[i];
                child->action_count = i + 1;
            }
            break;
        }

        default:
            // Default: copy from parent1
            *child = *parent1;
            child->id = ctx->next_strategy_id++;
            break;
    }

    ctx->stats.total_crossovers++;
    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

bool re_mutate(re_context_t* ctx, re_strategy_t* strategy) {
    if (!ctx || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_mutate: required parameter is NULL (ctx, strategy)");
        return false;
    }

    bool mutated = false;

    nimcp_mutex_lock(&ctx->lock);

    // Mutate genes
    for (uint32_t i = 0; i < strategy->gene_count; i++) {
        if (re_random_float() < ctx->config.mutation_rate) {
            re_gene_t* gene = &strategy->genes[i];

            // Gaussian mutation
            float delta = (re_random_float() - 0.5f) * 2.0f * gene->mutation_sigma;
            gene->value += delta;

            // Clamp to bounds
            if (gene->value < gene->min_value) gene->value = gene->min_value;
            if (gene->value > gene->max_value) gene->value = gene->max_value;

            if (gene->is_integer) {
                gene->value = roundf(gene->value);
            }

            mutated = true;
            ctx->stats.total_mutations++;
        }
    }

    // Mutate actions
    for (uint32_t i = 0; i < strategy->action_count; i++) {
        if (re_random_float() < ctx->config.mutation_rate) {
            strategy->actions[i] = (re_action_t)re_random_uint(RE_ACTION_COUNT);
            mutated = true;
            ctx->stats.total_mutations++;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    return mutated;
}

uint32_t re_evolve_generation(re_context_t* ctx) {
    if (!ctx || ctx->population_size < 2) return 0;

    nimcp_mutex_lock(&ctx->lock);

    // Sort population by fitness (descending)
    for (uint32_t i = 0; i < ctx->population_size - 1; i++) {
        for (uint32_t j = i + 1; j < ctx->population_size; j++) {
            if (ctx->population[j].fitness > ctx->population[i].fitness) {
                re_strategy_t temp = ctx->population[i];
                ctx->population[i] = ctx->population[j];
                ctx->population[j] = temp;
            }
        }
    }

    // Keep elite strategies
    re_strategy_t new_population[RE_MAX_POPULATION];
    uint32_t new_count = 0;

    for (uint32_t i = 0; i < ctx->config.elite_count && i < ctx->population_size; i++) {
        new_population[new_count++] = ctx->population[i];
    }

    // Generate offspring
    while (new_count < ctx->config.population_size && new_count < RE_MAX_POPULATION) {
        if (re_random_float() < ctx->config.crossover_rate) {
            // Crossover
            re_strategy_t parent1, parent2, child;
            nimcp_mutex_unlock(&ctx->lock);
            re_select_parents(ctx, &parent1, &parent2);
            re_crossover(ctx, &parent1, &parent2, &child);
            re_mutate(ctx, &child);
            nimcp_mutex_lock(&ctx->lock);
            new_population[new_count++] = child;
        } else {
            // Direct copy with mutation
            uint32_t idx = re_random_uint(ctx->population_size);
            new_population[new_count] = ctx->population[idx];
            new_population[new_count].id = ctx->next_strategy_id++;
            nimcp_mutex_unlock(&ctx->lock);
            re_mutate(ctx, &new_population[new_count]);
            nimcp_mutex_lock(&ctx->lock);
            new_count++;
        }
    }

    // Replace population
    memcpy(ctx->population, new_population, new_count * sizeof(re_strategy_t));
    ctx->population_size = new_count;
    ctx->generation++;
    ctx->stats.total_generations++;

    // Update stats
    ctx->stats.best_fitness = ctx->population[0].fitness;
    float total_fitness = 0.0f;
    for (uint32_t i = 0; i < ctx->population_size; i++) {
        total_fitness += ctx->population[i].fitness;
    }
    ctx->stats.avg_fitness = total_fitness / ctx->population_size;
    ctx->stats.worst_fitness = ctx->population[ctx->population_size - 1].fitness;
    ctx->stats.best_strategy_id = ctx->population[0].id;

    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("RE", "Evolved to generation %u, best fitness: %.2f", ctx->generation, ctx->stats.best_fitness);
    return ctx->generation;
}

bool re_get_best_strategy(re_context_t* ctx, re_strategy_t* strategy) {
    if (!ctx || !strategy || ctx->population_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_get_best_strategy: required parameter is NULL (ctx, strategy)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    // Find best by fitness
    uint32_t best = 0;
    for (uint32_t i = 1; i < ctx->population_size; i++) {
        if (ctx->population[i].fitness > ctx->population[best].fitness) {
            best = i;
        }
    }

    *strategy = ctx->population[best];
    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

bool re_get_strategy(re_context_t* ctx, uint32_t strategy_id, re_strategy_t* strategy) {
    if (!ctx || !strategy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_get_strategy: required parameter is NULL (ctx, strategy)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    for (uint32_t i = 0; i < ctx->population_size; i++) {
        if (ctx->population[i].id == strategy_id) {
            *strategy = ctx->population[i];
            nimcp_mutex_unlock(&ctx->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "re_get_strategy: validation failed");
    return false;
}

//=============================================================================
// Reinforcement Learning Functions
//=============================================================================

re_action_t re_select_action(re_context_t* ctx, uint32_t state) {
    if (!ctx) return RE_ACTION_RETRY;

    nimcp_mutex_lock(&ctx->lock);

    // Epsilon-greedy policy
    if (re_random_float() < ctx->epsilon) {
        // Explore: random action
        re_action_t action = (re_action_t)re_random_uint(RE_ACTION_COUNT);
        nimcp_mutex_unlock(&ctx->lock);
        return action;
    }

    // Exploit: best action
    re_q_entry_t* entry = re_get_or_create_q_entry(ctx, state);
    if (!entry) {
        nimcp_mutex_unlock(&ctx->lock);
        return RE_ACTION_RETRY;
    }

    entry->visit_count++;

    re_action_t best_action = RE_ACTION_RETRY;
    float best_q = entry->q_values[0];
    for (int i = 1; i < RE_ACTION_COUNT; i++) {
        if (entry->q_values[i] > best_q) {
            best_q = entry->q_values[i];
            best_action = (re_action_t)i;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    return best_action;
}

float re_update_q(re_context_t* ctx, uint32_t state, re_action_t action, float reward, uint32_t next_state, bool terminal) {
    if (!ctx || action >= RE_ACTION_COUNT) return 0.0f;

    nimcp_mutex_lock(&ctx->lock);

    re_q_entry_t* entry = re_get_or_create_q_entry(ctx, state);
    if (!entry) {
        nimcp_mutex_unlock(&ctx->lock);
        return 0.0f;
    }

    float max_next_q = 0.0f;
    if (!terminal) {
        re_q_entry_t* next_entry = re_get_or_create_q_entry(ctx, next_state);
        if (next_entry) {
            for (int i = 0; i < RE_ACTION_COUNT; i++) {
                if (next_entry->q_values[i] > max_next_q) {
                    max_next_q = next_entry->q_values[i];
                }
            }
        }
    }

    // Q-learning update
    float td_target = reward + ctx->config.discount_factor * max_next_q;
    float td_error = td_target - entry->q_values[action];
    entry->q_values[action] += ctx->config.learning_rate * td_error;

    nimcp_mutex_unlock(&ctx->lock);
    return entry->q_values[action];
}

float re_get_q_value(re_context_t* ctx, uint32_t state, re_action_t action) {
    if (!ctx || action >= RE_ACTION_COUNT) return 0.0f;

    nimcp_mutex_lock(&ctx->lock);

    re_q_entry_t* entry = re_find_q_entry(ctx, state);
    float q = entry ? entry->q_values[action] : 0.0f;

    nimcp_mutex_unlock(&ctx->lock);
    return q;
}

re_action_t re_get_best_action(re_context_t* ctx, uint32_t state) {
    if (!ctx) return RE_ACTION_RETRY;

    nimcp_mutex_lock(&ctx->lock);

    re_q_entry_t* entry = re_find_q_entry(ctx, state);
    if (!entry) {
        nimcp_mutex_unlock(&ctx->lock);
        return RE_ACTION_RETRY;
    }

    re_action_t best = RE_ACTION_RETRY;
    float best_q = entry->q_values[0];
    for (int i = 1; i < RE_ACTION_COUNT; i++) {
        if (entry->q_values[i] > best_q) {
            best_q = entry->q_values[i];
            best = (re_action_t)i;
        }
    }

    nimcp_mutex_unlock(&ctx->lock);
    return best;
}

float re_decay_epsilon(re_context_t* ctx) {
    if (!ctx) return 0.0f;

    nimcp_mutex_lock(&ctx->lock);

    ctx->epsilon *= ctx->config.epsilon_decay;
    if (ctx->epsilon < ctx->config.min_epsilon) {
        ctx->epsilon = ctx->config.min_epsilon;
    }
    ctx->stats.current_epsilon = ctx->epsilon;

    nimcp_mutex_unlock(&ctx->lock);
    return ctx->epsilon;
}

//=============================================================================
// Experience Replay
//=============================================================================

bool re_store_experience(re_context_t* ctx, const re_experience_t* experience) {
    if (!ctx || !experience) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_store_experience: required parameter is NULL (ctx, experience)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    ctx->history[ctx->history_head] = *experience;
    ctx->history_head = (ctx->history_head + 1) % RE_MAX_HISTORY;
    if (ctx->history_size < RE_MAX_HISTORY) {
        ctx->history_size++;
    }

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

uint32_t re_sample_batch(re_context_t* ctx, re_experience_t* batch, uint32_t batch_size) {
    if (!ctx || !batch || batch_size == 0) return 0;

    nimcp_mutex_lock(&ctx->lock);

    uint32_t actual_size = (batch_size < ctx->history_size) ? batch_size : ctx->history_size;

    // Random sampling
    for (uint32_t i = 0; i < actual_size; i++) {
        uint32_t idx = re_random_uint(ctx->history_size);
        batch[i] = ctx->history[idx];
    }

    nimcp_mutex_unlock(&ctx->lock);
    return actual_size;
}

float re_learn_from_batch(re_context_t* ctx) {
    if (!ctx) return 0.0f;

    re_experience_t batch[64];
    uint32_t batch_size = re_sample_batch(ctx, batch, ctx->config.batch_size);

    float total_loss = 0.0f;
    for (uint32_t i = 0; i < batch_size; i++) {
        float old_q = re_get_q_value(ctx, batch[i].state, batch[i].action);
        float new_q = re_update_q(ctx, batch[i].state, batch[i].action,
                                  batch[i].reward, batch[i].next_state, batch[i].terminal);
        total_loss += fabsf(new_q - old_q);
    }

    return (batch_size > 0) ? (total_loss / batch_size) : 0.0f;
}

//=============================================================================
// Transfer Learning
//=============================================================================

size_t re_export_knowledge(re_context_t* ctx, void* buffer, size_t buffer_size) {
    if (!ctx || !buffer) return 0;

    nimcp_mutex_lock(&ctx->lock);

    size_t required = sizeof(re_q_entry_t) * ctx->q_table_size + sizeof(uint32_t);
    if (buffer_size < required) {
        nimcp_mutex_unlock(&ctx->lock);
        return required;
    }

    // Write size
    uint32_t* size_ptr = (uint32_t*)buffer;
    *size_ptr = ctx->q_table_size;

    // Write Q-table
    memcpy(size_ptr + 1, ctx->q_table, sizeof(re_q_entry_t) * ctx->q_table_size);

    nimcp_mutex_unlock(&ctx->lock);
    return required;
}

bool re_import_knowledge(re_context_t* ctx, const void* data, size_t data_size) {
    if (!ctx || !data || data_size < sizeof(uint32_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_import_knowledge: required parameter is NULL (ctx, data)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    const uint32_t* size_ptr = (const uint32_t*)data;
    uint32_t count = *size_ptr;

    if (count > RE_MAX_STATES) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "re_import_knowledge: validation failed");
        return false;
    }

    size_t expected = sizeof(re_q_entry_t) * count + sizeof(uint32_t);
    if (data_size < expected) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "re_import_knowledge: validation failed");
        return false;
    }

    ctx->q_table_size = count;
    memcpy(ctx->q_table, size_ptr + 1, sizeof(re_q_entry_t) * count);

    nimcp_mutex_unlock(&ctx->lock);

    bbb_audit_log(BBB_AUDIT_INFO, "RE", "IMPORT", "Imported %u Q-table entries", count);
    return true;
}

bool re_transfer_from(re_context_t* ctx, const re_context_t* source_ctx, float transfer_rate) {
    if (!ctx || !source_ctx || transfer_rate <= 0.0f || transfer_rate > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_transfer_from: required parameter is NULL (ctx, source_ctx)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);
    nimcp_mutex_lock((nimcp_mutex_t*)&source_ctx->lock);

    // Transfer Q-values with blend
    for (uint32_t i = 0; i < source_ctx->q_table_size; i++) {
        re_q_entry_t* entry = re_get_or_create_q_entry(ctx, source_ctx->q_table[i].state_id);
        if (entry) {
            for (int j = 0; j < RE_ACTION_COUNT; j++) {
                entry->q_values[j] = (1.0f - transfer_rate) * entry->q_values[j] +
                                    transfer_rate * source_ctx->q_table[i].q_values[j];
            }
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)&source_ctx->lock);
    nimcp_mutex_unlock(&ctx->lock);

    LOG_INFO("RE", "Transferred knowledge with rate %.2f", transfer_rate);
    return true;
}

//=============================================================================
// Strategy Recommendation
//=============================================================================

float re_recommend_strategy(re_context_t* ctx, uint32_t fault_type, uint32_t fault_severity, re_strategy_t* strategy) {
    if (!ctx || !strategy) return 0.0f;

    // Combine GA and RL recommendations
    re_strategy_t ga_strategy;
    if (!re_get_best_strategy(ctx, &ga_strategy)) {
        return 0.0f;
    }

    uint32_t state = re_encode_state(fault_type, fault_severity, 0);
    re_action_t rl_action = re_get_best_action(ctx, state);

    // Return GA strategy but with RL action as first action
    *strategy = ga_strategy;
    if (strategy->action_count > 0) {
        strategy->actions[0] = rl_action;
    }

    // Confidence based on experience
    float confidence = 0.5f;
    nimcp_mutex_lock(&ctx->lock);
    re_q_entry_t* entry = re_find_q_entry(ctx, state);
    if (entry && entry->visit_count > 10) {
        confidence = 0.9f;
    } else if (entry && entry->visit_count > 3) {
        confidence = 0.7f;
    }
    nimcp_mutex_unlock(&ctx->lock);

    return confidence;
}

uint32_t re_get_action_sequence(re_context_t* ctx, uint32_t fault_type, re_action_t* actions, uint32_t max_actions) {
    if (!ctx || !actions || max_actions == 0) return 0;

    re_strategy_t strategy;
    if (!re_get_best_strategy(ctx, &strategy)) return 0;

    uint32_t count = (strategy.action_count < max_actions) ? strategy.action_count : max_actions;
    memcpy(actions, strategy.actions, count * sizeof(re_action_t));

    return count;
}

//=============================================================================
// Statistics
//=============================================================================

bool re_get_stats(re_context_t* ctx, re_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "re_get_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    nimcp_mutex_lock(&ctx->lock);

    *stats = ctx->stats;

    // Calculate success rate
    uint64_t total_used = 0;
    uint64_t total_succeeded = 0;
    for (uint32_t i = 0; i < ctx->population_size; i++) {
        total_used += ctx->population[i].times_used;
        total_succeeded += ctx->population[i].times_succeeded;
    }
    stats->avg_success_rate = (total_used > 0) ? (float)total_succeeded / total_used : 0.0f;

    nimcp_mutex_unlock(&ctx->lock);
    return true;
}

void re_reset_stats(re_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->lock);
    memset(&ctx->stats, 0, sizeof(re_stats_t));
    ctx->stats.current_epsilon = ctx->epsilon;
    nimcp_mutex_unlock(&ctx->lock);
}

float re_get_diversity(re_context_t* ctx) {
    if (!ctx || ctx->population_size < 2) return 0.0f;

    nimcp_mutex_lock(&ctx->lock);

    // Calculate fitness variance as diversity metric
    float mean = ctx->stats.avg_fitness;
    float variance = 0.0f;

    for (uint32_t i = 0; i < ctx->population_size; i++) {
        float diff = ctx->population[i].fitness - mean;
        variance += diff * diff;
    }
    variance /= ctx->population_size;

    float diversity = sqrtf(variance) / (fabsf(mean) + 1.0f);

    nimcp_mutex_unlock(&ctx->lock);
    return diversity;
}

void re_print_q_table(re_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->lock);

    LOG_DEBUG("RE", "Q-Table (%u states):", ctx->q_table_size);
    for (uint32_t i = 0; i < ctx->q_table_size && i < 10; i++) {
        LOG_DEBUG("RE", "  State %u: visits=%u", ctx->q_table[i].state_id, ctx->q_table[i].visit_count);
    }

    nimcp_mutex_unlock(&ctx->lock);
}

//=============================================================================
// Utility Functions
//=============================================================================

float re_calculate_fitness(re_context_t* ctx, const re_outcome_t* outcome) {
    re_strategy_t dummy;
    return re_calc_fitness(ctx, &dummy, outcome);
}

float re_calculate_reward(const re_outcome_t* outcome) {
    if (!outcome) return 0.0f;

    float reward = 0.0f;

    // Success bonus
    if (outcome->success) {
        reward += 10.0f;
    } else {
        reward -= 5.0f;
    }

    // Time penalty (normalized to 0-5)
    float time_penalty = (float)outcome->recovery_time_ms / 1000.0f;
    if (time_penalty > 5.0f) time_penalty = 5.0f;
    reward -= time_penalty;

    // Resource usage penalty
    reward -= outcome->resource_usage * 3.0f;

    // Data loss penalty
    reward -= outcome->data_loss * 10.0f;

    return reward;
}

uint32_t re_encode_state(uint32_t fault_type, uint32_t severity, uint32_t context) {
    // Encode state as: fault_type (8 bits) | severity (8 bits) | context (16 bits)
    return ((fault_type & 0xFF) << 24) | ((severity & 0xFF) << 16) | (context & 0xFFFF);
}

//=============================================================================
// String Conversion
//=============================================================================

const char* re_algorithm_to_string(re_algorithm_t algo) {
    switch (algo) {
        case RE_ALGO_GENETIC: return "Genetic";
        case RE_ALGO_Q_LEARNING: return "Q-Learning";
        case RE_ALGO_SARSA: return "SARSA";
        case RE_ALGO_ACTOR_CRITIC: return "Actor-Critic";
        case RE_ALGO_HYBRID: return "Hybrid";
        default: return "UNKNOWN";
    }
}

const char* re_action_to_string(re_action_t action) {
    switch (action) {
        case RE_ACTION_RETRY: return "Retry";
        case RE_ACTION_CHECKPOINT: return "Checkpoint";
        case RE_ACTION_REDUCE_LOAD: return "ReduceLoad";
        case RE_ACTION_ISOLATE: return "Isolate";
        case RE_ACTION_RESTART: return "Restart";
        case RE_ACTION_FAILOVER: return "Failover";
        case RE_ACTION_DEGRADE: return "Degrade";
        case RE_ACTION_ESCALATE: return "Escalate";
        case RE_ACTION_CACHE_CLEAR: return "CacheClear";
        case RE_ACTION_GC: return "GC";
        case RE_ACTION_REDUCE_LR: return "ReduceLR";
        case RE_ACTION_CLIP_GRADIENT: return "ClipGradient";
        default: return "UNKNOWN";
    }
}

const char* re_selection_to_string(re_selection_t selection) {
    switch (selection) {
        case RE_SELECT_ROULETTE: return "Roulette";
        case RE_SELECT_TOURNAMENT: return "Tournament";
        case RE_SELECT_RANK: return "Rank";
        case RE_SELECT_ELITISM: return "Elitism";
        default: return "UNKNOWN";
    }
}

const char* re_crossover_to_string(re_crossover_t crossover) {
    switch (crossover) {
        case RE_CROSS_SINGLE: return "Single";
        case RE_CROSS_TWO_POINT: return "TwoPoint";
        case RE_CROSS_UNIFORM: return "Uniform";
        case RE_CROSS_BLEND: return "Blend";
        default: return "UNKNOWN";
    }
}

const char* re_fitness_to_string(re_fitness_criteria_t criteria) {
    switch (criteria) {
        case RE_FIT_RECOVERY_TIME: return "RecoveryTime";
        case RE_FIT_SUCCESS_RATE: return "SuccessRate";
        case RE_FIT_RESOURCE_USAGE: return "ResourceUsage";
        case RE_FIT_DATA_LOSS: return "DataLoss";
        case RE_FIT_COMPOSITE: return "Composite";
        default: return "UNKNOWN";
    }
}

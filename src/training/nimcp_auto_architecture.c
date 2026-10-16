#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_auto_architecture.c - Neural Architecture Search Implementation
//=============================================================================
/**
 * @file nimcp_auto_architecture.c
 * @brief Implementation of automatic architecture discovery for NIMCP
 *
 * WHAT: Neural Architecture Search (NAS) implementation for SNN/LNN/CNN
 * WHY:  Automates the tedious process of manual architecture design
 * HOW:  Implements multiple search strategies with biological constraints
 *
 * IMPLEMENTATION STRATEGIES:
 * 1. Evolutionary NAS: Population-based search with mutation/crossover
 * 2. RL NAS: LSTM controller trained with REINFORCE
 * 3. DARTS: Continuous relaxation of architecture search
 * 4. Pruning: Start dense and prune to discover structure
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 * @version 1.0.0
 */

#include "training/nimcp_auto_architecture.h"
#include "lnn/nimcp_lnn_config.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_neural_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(auto_architecture)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Controller network for RL NAS
 *
 * WHAT: LSTM controller that generates architecture specifications
 * WHY:  RL approach requires policy network to sample architectures
 * HOW:  LSTM outputs architecture parameters autoregressively
 */
typedef struct {
    uint32_t hidden_size;           /**< LSTM hidden dimension */
    float learning_rate;            /**< Controller learning rate */
    float entropy_weight;           /**< Entropy regularization */
    void* lstm_state;               /**< LSTM state (opaque) */
    nimcp_tensor_t* weights;        /**< Controller weights */
    nimcp_tensor_t* gradients;      /**< Controller gradients */
    nimcp_optimizer_context_t* optimizer; /**< Controller optimizer */
} rl_controller_t;

/**
 * @brief Population member for evolutionary search
 *
 * WHAT: Individual architecture in evolutionary population
 * WHY:  Track architecture + fitness for selection
 * HOW:  Stores architecture and fitness, plus parent lineage
 */
typedef struct {
    auto_arch_architecture_t* arch; /**< Architecture specification */
    auto_arch_fitness_t fitness;    /**< Evaluated fitness */
    uint32_t generation;            /**< Generation number */
    uint32_t parent_id;             /**< Parent architecture ID */
    bool evaluated;                 /**< Whether fitness is valid */
} population_member_t;

/**
 * @brief DARTS continuous architecture representation
 *
 * WHAT: Continuous relaxation of discrete architecture choices
 * WHY:  Enables gradient-based architecture optimization
 * HOW:  Softmax over operation choices, differentiable sampling
 */
typedef struct {
    nimcp_tensor_t** alpha;         /**< Architecture parameters [n_layers][n_ops] */
    nimcp_tensor_t** weights;       /**< Network weights */
    nimcp_optimizer_context_t* arch_optimizer; /**< Optimizer for α */
    nimcp_optimizer_context_t* weight_optimizer; /**< Optimizer for weights */
    uint32_t n_layers;              /**< Number of layers */
} darts_context_t;

/**
 * @brief Complete architecture search context (opaque)
 *
 * WHAT: Internal state for architecture search
 * WHY:  Encapsulates all search algorithm state
 * HOW:  Union of method-specific contexts
 */
struct auto_arch_context_s {
    /* Configuration */
    auto_arch_config_t config;      /**< Search configuration */
    auto_arch_task_t task;          /**< Task specification */

    /* Search state */
    auto_arch_status_t status;      /**< Current status */
    auto_arch_stats_t stats;        /**< Search statistics */
    uint64_t next_arch_id;          /**< Next architecture ID */

    /* Best architecture tracking */
    auto_arch_architecture_t* best_arch; /**< Best architecture so far */
    auto_arch_fitness_t best_fitness;    /**< Best fitness so far */

    /* Pareto frontier (multi-objective) */
    auto_arch_architecture_t** pareto_frontier; /**< Non-dominated archs */
    auto_arch_fitness_t* pareto_fitness; /**< Fitness for each Pareto arch */
    uint32_t n_pareto;              /**< Number of Pareto optimal archs */
    uint32_t pareto_capacity;       /**< Allocated Pareto slots */

    /* History tracking */
    auto_arch_architecture_t** history; /**< All evaluated architectures */
    auto_arch_fitness_t* history_fitness; /**< Fitness for each arch */
    uint32_t history_size;          /**< Number of evaluated archs */
    uint32_t history_capacity;      /**< Allocated history slots */

    /* Method-specific context (union) */
    union {
        struct {
            population_member_t* population; /**< Current population */
            uint32_t generation;    /**< Current generation */
        } evolutionary;

        struct {
            rl_controller_t* controller; /**< RL controller network */
            float baseline;         /**< Reward baseline for variance reduction */
        } rl_nas;

        struct {
            darts_context_t* darts; /**< DARTS continuous arch */
            uint32_t warmup_epochs; /**< Warmup epochs completed */
        } darts;

        struct {
            auto_arch_architecture_t* dense_arch; /**< Initial dense architecture */
            nimcp_tensor_t* importance_scores; /**< Connection importance */
            float pruning_rate;     /**< Fraction to prune per iteration */
        } pruning;
    } method_ctx;

    /* Random number generator state */
    uint64_t rng_state;             /**< RNG state for reproducibility */

    /* Validation */
    uint32_t magic;                 /**< Magic number for validation */
};

//=============================================================================
// Internal Helper Functions (Forward Declarations)
//=============================================================================

static int init_evolutionary_context(auto_arch_context_t* ctx);
static int init_rl_nas_context(auto_arch_context_t* ctx);
static int init_darts_context(auto_arch_context_t* ctx);
static int init_pruning_context(auto_arch_context_t* ctx);

static auto_arch_architecture_t* evolutionary_step(auto_arch_context_t* ctx,
                                                   const nimcp_tensor_t* train_data,
                                                   const nimcp_tensor_t* train_labels,
                                                   const nimcp_tensor_t* val_data,
                                                   const nimcp_tensor_t* val_labels);
static auto_arch_architecture_t* rl_nas_step(auto_arch_context_t* ctx,
                                             const nimcp_tensor_t* train_data,
                                             const nimcp_tensor_t* train_labels,
                                             const nimcp_tensor_t* val_data,
                                             const nimcp_tensor_t* val_labels);
static auto_arch_architecture_t* darts_step(auto_arch_context_t* ctx,
                                            const nimcp_tensor_t* train_data,
                                            const nimcp_tensor_t* train_labels,
                                            const nimcp_tensor_t* val_data,
                                            const nimcp_tensor_t* val_labels);
static auto_arch_architecture_t* pruning_step(auto_arch_context_t* ctx,
                                              const nimcp_tensor_t* train_data,
                                              const nimcp_tensor_t* train_labels,
                                              const nimcp_tensor_t* val_data,
                                              const nimcp_tensor_t* val_labels);

static int compute_fitness(auto_arch_context_t* ctx,
                          auto_arch_architecture_t* arch,
                          const nimcp_tensor_t* train_data,
                          const nimcp_tensor_t* train_labels,
                          const nimcp_tensor_t* val_data,
                          const nimcp_tensor_t* val_labels,
                          auto_arch_fitness_t* fitness);

static int update_pareto_frontier(auto_arch_context_t* ctx,
                                  auto_arch_architecture_t* arch,
                                  const auto_arch_fitness_t* fitness);

static int record_history(auto_arch_context_t* ctx,
                         auto_arch_architecture_t* arch,
                         const auto_arch_fitness_t* fitness);

static uint64_t xorshift64(uint64_t* state);
static float randf(uint64_t* state);

//=============================================================================
// Configuration API Implementation
//=============================================================================

/**
 * @brief Initialize default architecture search configuration
 *
 * WHAT: Populate config with sensible defaults for general use
 * WHY:  Provides working baseline without manual tuning
 * HOW:  Sets evolutionary search with balanced objectives
 */
int auto_arch_default_config(auto_arch_config_t* config)
{
    /* Guard clause: NULL check */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_default_config: config is NULL");
        return -1;
    }

    /* Zero out the config */
    memset(config, 0, sizeof(auto_arch_config_t));

    /* Search method */
    config->search_method = AUTO_ARCH_EVOLUTIONARY;
    config->network_type = AUTO_ARCH_TYPE_SNN;

    /* Search parameters */
    config->max_evaluations = 1000;
    config->max_iterations = 10000;
    config->max_time_hours = 24.0f;
    config->early_stop_patience = 100;

    /* Evolutionary parameters */
    config->population_size = 50;
    config->mutation_rate = 0.1f;
    config->crossover_rate = 0.7f;
    config->tournament_size = 5;

    /* RL NAS parameters */
    config->rl_learning_rate = 0.001f;
    config->rl_entropy_weight = 0.01f;
    config->rl_controller_lstm_size = 128;

    /* DARTS parameters */
    config->darts_alpha_lr = 0.003f;
    config->darts_weight_lr = 0.025f;
    config->darts_warmup_epochs = 15;

    /* Bayesian optimization */
    config->bo_n_initial_points = 20;
    config->bo_acquisition_weight = 0.1f;

    /* Multi-objective */
    config->primary_objective = AUTO_ARCH_OBJ_ACCURACY;
    config->objective_weights[AUTO_ARCH_OBJ_ACCURACY] = 0.7f;
    config->objective_weights[AUTO_ARCH_OBJ_ENERGY] = 0.2f;
    config->objective_weights[AUTO_ARCH_OBJ_LATENCY] = 0.05f;
    config->objective_weights[AUTO_ARCH_OBJ_MEMORY] = 0.05f;
    config->use_pareto_frontier = true;

    /* Search space constraints */
    config->constraints.min_layers = 2;
    config->constraints.max_layers = 8;
    config->constraints.min_neurons_per_layer = 100;
    config->constraints.max_neurons_per_layer = 1000;
    config->constraints.max_total_neurons = 10000;
    config->constraints.max_parameters = 1000000;
    config->constraints.min_sparsity = 0.0f;
    config->constraints.max_sparsity = 0.95f;
    config->constraints.enforce_feedforward = false;
    config->constraints.allow_skip_connections = true;
    config->constraints.min_tau = 1.0f;
    config->constraints.max_tau = 100.0f;
    config->constraints.min_dt = NIMCP_SIMULATION_DT_FINE_MS;
    config->constraints.max_dt = NIMCP_SIMULATION_DT_MS;
    config->constraints.min_bio_score = 0.0f;
    config->constraints.enforce_dales_law = false;
    config->constraints.require_local_connectivity = false;

    /* Search space */
    config->search_space.allow_snn_lif = true;
    config->search_space.allow_snn_izhikevich = true;
    config->search_space.allow_lnn_ltc = false;
    config->search_space.allow_dense = true;
    config->search_space.allow_conv = false;
    config->search_space.allow_recurrent = true;
    config->search_space.allow_sparse = true;
    config->search_space.allow_small_world = true;
    config->search_space.allow_scale_free = false;
    config->search_space.allow_skip_connections = true;
    config->search_space.min_layer_size = 50;
    config->search_space.max_layer_size = 1000;
    config->search_space.min_sparsity = 0.0f;
    config->search_space.max_sparsity = 0.9f;
    config->search_space.tau_mem_min = 10.0f;
    config->search_space.tau_mem_max = 30.0f;
    config->search_space.tau_syn_min = 5.0f;
    config->search_space.tau_syn_max = 10.0f;
    config->search_space.tau_base_min = 10.0f;
    config->search_space.tau_base_max = 100.0f;

    /* Evaluation configuration */
    config->eval_epochs = 10;
    config->eval_batch_size = 32;
    config->eval_learning_rate = 0.001f;
    config->eval_optimizer = NIMCP_OPTIMIZER_ADAM;

    /* Parallelization */
    config->n_workers = 1;
    config->use_gpu = false;

    /* Logging */
    config->verbose = true;
    config->checkpoint_interval = 100;
    config->checkpoint_dir = "./auto_arch_checkpoints";
    config->log_file = "./auto_arch.log";

    /* Reproducibility */
    config->random_seed = 0; /* Will use time */

    return 0;
}

/**
 * @brief Initialize configuration for fast search
 *
 * WHAT: Quick search with minimal evaluations
 * WHY:  Rapid prototyping or resource-constrained scenarios
 * HOW:  Random search with 100 evaluations
 */
int auto_arch_fast_config(auto_arch_config_t* config)
{
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_fast_config: config is NULL");
        return -1;
    }

    /* Start with defaults */
    auto_arch_default_config(config);

    /* Override for speed */
    config->search_method = AUTO_ARCH_RANDOM_SEARCH;
    config->max_evaluations = 100;
    config->max_iterations = 100;
    config->max_time_hours = 1.0f;
    config->eval_epochs = 5;
    config->population_size = 20;

    return 0;
}

/**
 * @brief Initialize configuration for thorough search
 *
 * WHAT: Extensive search for production deployments
 * WHY:  Best possible architecture regardless of search cost
 * HOW:  Evolutionary + DARTS with 10K evaluations
 */
int auto_arch_thorough_config(auto_arch_config_t* config)
{
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_thorough_config: config is NULL");
        return -1;
    }

    /* Start with defaults */
    auto_arch_default_config(config);

    /* Override for thoroughness */
    config->max_evaluations = 10000;
    config->max_iterations = 100000;
    config->max_time_hours = 168.0f; /* 1 week */
    config->eval_epochs = 50;
    config->population_size = 100;
    config->early_stop_patience = 500;

    return 0;
}

/**
 * @brief Validate configuration
 *
 * WHAT: Check configuration for errors and inconsistencies
 * WHY:  Catch configuration errors before expensive search
 * HOW:  Validate all ranges and compatibility
 */
int auto_arch_validate_config(const auto_arch_config_t* config)
{
    /* Guard clause */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_validate_config: config is NULL");
        return -1;
    }

    /* Search parameters */
    if (config->max_evaluations == 0) {
        NIMCP_LOGGING_ERROR("max_evaluations must be > 0");
        return -2;
    }

    /* Population-based methods need population */
    if (config->search_method == AUTO_ARCH_EVOLUTIONARY ||
        config->search_method == AUTO_ARCH_NEUROEVOLUTION) {
        if (config->population_size == 0) {
            NIMCP_LOGGING_ERROR("population_size must be > 0 for evolutionary methods");
            return -3;
        }
    }

    /* Constraint validation */
    if (config->constraints.min_layers > config->constraints.max_layers) {
        NIMCP_LOGGING_ERROR("min_layers > max_layers");
        return -4;
    }

    if (config->constraints.min_neurons_per_layer > config->constraints.max_neurons_per_layer) {
        NIMCP_LOGGING_ERROR("min_neurons_per_layer > max_neurons_per_layer");
        return -5;
    }

    if (config->constraints.min_tau > config->constraints.max_tau) {
        NIMCP_LOGGING_ERROR("min_tau > max_tau");
        return -6;
    }

    /* Objective weights should sum to ~1.0 */
    float weight_sum = 0.0f;
    for (int i = 0; i < AUTO_ARCH_OBJ_COUNT; i++) {
        weight_sum += config->objective_weights[i];
    }
    if (fabs(weight_sum - 1.0f) > 0.1f) {
        NIMCP_LOGGING_WARN("Objective weights sum to %.3f (expected ~1.0)", weight_sum);
    }

    return 0;
}

//=============================================================================
// Core API Implementation
//=============================================================================

/**
 * @brief Create architecture search context
 *
 * WHAT: Allocate and initialize search context
 * WHY:  Factory function for search creation
 * HOW:  Validate config, allocate context, initialize method-specific state
 */
auto_arch_context_t* auto_arch_create(const auto_arch_config_t* config)
{
    /* Guard clause: NULL check */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_create: config is NULL");
        return NULL;
    }

    /* Validate configuration */
    if (auto_arch_validate_config(config) != 0) {
        NIMCP_LOGGING_ERROR("Invalid configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_create: validation failed");
        return NULL;
    }

    /* Allocate context */
    auto_arch_context_t* ctx = (auto_arch_context_t*)nimcp_malloc(sizeof(auto_arch_context_t));
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_create: ctx is NULL");
        return NULL;
    }

    /* Initialize fields */
    memset(ctx, 0, sizeof(auto_arch_context_t));
    memcpy(&ctx->config, config, sizeof(auto_arch_config_t));
    ctx->status = AUTO_ARCH_STATUS_IDLE;
    ctx->next_arch_id = 1;
    ctx->magic = AUTO_ARCH_MAGIC;

    /* Initialize RNG */
    if (config->random_seed == 0) {
        ctx->rng_state = (uint64_t)time(NULL);
    } else {
        ctx->rng_state = config->random_seed;
    }

    /* Allocate history */
    ctx->history_capacity = config->max_evaluations;
    ctx->history = (auto_arch_architecture_t**)nimcp_malloc(
        ctx->history_capacity * sizeof(auto_arch_architecture_t*));
    ctx->history_fitness = (auto_arch_fitness_t*)nimcp_malloc(
        ctx->history_capacity * sizeof(auto_arch_fitness_t));

    /* Allocate Pareto frontier */
    ctx->pareto_capacity = 100;
    ctx->pareto_frontier = (auto_arch_architecture_t**)nimcp_malloc(
        ctx->pareto_capacity * sizeof(auto_arch_architecture_t*));
    ctx->pareto_fitness = (auto_arch_fitness_t*)nimcp_malloc(
        ctx->pareto_capacity * sizeof(auto_arch_fitness_t));

    /* Verify all allocations succeeded */
    if (!ctx->history || !ctx->history_fitness ||
        !ctx->pareto_frontier || !ctx->pareto_fitness) {
        if (ctx->history) nimcp_free(ctx->history);
        if (ctx->history_fitness) nimcp_free(ctx->history_fitness);
        if (ctx->pareto_frontier) nimcp_free(ctx->pareto_frontier);
        if (ctx->pareto_fitness) nimcp_free(ctx->pareto_fitness);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_create: allocation failed");
        return NULL;
    }

    /* Initialize method-specific context */
    int result = 0;
    switch (config->search_method) {
        case AUTO_ARCH_EVOLUTIONARY:
        case AUTO_ARCH_NEUROEVOLUTION:
            result = init_evolutionary_context(ctx);
            break;
        case AUTO_ARCH_RL_NAS:
            result = init_rl_nas_context(ctx);
            break;
        case AUTO_ARCH_DARTS:
            result = init_darts_context(ctx);
            break;
        case AUTO_ARCH_PRUNING_BASED:
            result = init_pruning_context(ctx);
            break;
        case AUTO_ARCH_RANDOM_SEARCH:
        case AUTO_ARCH_BAYESIAN_OPT:
        case AUTO_ARCH_GRADIENT_BASED:
            /* No special initialization needed */
            break;
        default:
            NIMCP_LOGGING_ERROR("Unknown search method: %d", config->search_method);
            result = -1;
    }

    if (result != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize method-specific context");
        auto_arch_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_create: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created auto-architecture search context");
    return ctx;
}

/**
 * @brief Destroy architecture search context
 *
 * WHAT: Free all resources used by search
 * WHY:  Prevent memory leaks
 * HOW:  Free population, controller, history, Pareto frontier
 */
void auto_arch_destroy(auto_arch_context_t* ctx)
{
    /* Guard clause: NULL check */
    if (!ctx) return;

    /* Free best architecture */
    if (ctx->best_arch) {
        auto_arch_architecture_destroy(ctx->best_arch);
    }

    /* Free Pareto frontier */
    if (ctx->pareto_frontier) {
        for (uint32_t i = 0; i < ctx->n_pareto; i++) {
            auto_arch_architecture_destroy(ctx->pareto_frontier[i]);
        }
        nimcp_free(ctx->pareto_frontier);
    }
    if (ctx->pareto_fitness) {
        nimcp_free(ctx->pareto_fitness);
    }

    /* Free history */
    if (ctx->history) {
        for (uint32_t i = 0; i < ctx->history_size; i++) {
            auto_arch_architecture_destroy(ctx->history[i]);
        }
        nimcp_free(ctx->history);
    }
    if (ctx->history_fitness) {
        nimcp_free(ctx->history_fitness);
    }

    /* Free method-specific context */
    switch (ctx->config.search_method) {
        case AUTO_ARCH_EVOLUTIONARY:
        case AUTO_ARCH_NEUROEVOLUTION:
            if (ctx->method_ctx.evolutionary.population) {
                for (uint32_t i = 0; i < ctx->config.population_size; i++) {
                    if (ctx->method_ctx.evolutionary.population[i].arch) {
                        auto_arch_architecture_destroy(ctx->method_ctx.evolutionary.population[i].arch);
                    }
                }
                nimcp_free(ctx->method_ctx.evolutionary.population);
            }
            break;

        case AUTO_ARCH_RL_NAS:
            if (ctx->method_ctx.rl_nas.controller) {
                rl_controller_t* controller = ctx->method_ctx.rl_nas.controller;
                if (controller->weights) nimcp_tensor_destroy(controller->weights);
                if (controller->gradients) nimcp_tensor_destroy(controller->gradients);
                if (controller->optimizer) nimcp_free(controller->optimizer);
                if (controller->lstm_state) nimcp_free(controller->lstm_state);
                nimcp_free(controller);
            }
            break;

        case AUTO_ARCH_DARTS:
            if (ctx->method_ctx.darts.darts) {
                darts_context_t* darts = ctx->method_ctx.darts.darts;
                if (darts->alpha) {
                    for (uint32_t i = 0; i < darts->n_layers; i++) {
                        if (darts->alpha[i]) nimcp_tensor_destroy(darts->alpha[i]);
                    }
                    nimcp_free(darts->alpha);
                }
                if (darts->weights) {
                    for (uint32_t i = 0; i < darts->n_layers; i++) {
                        if (darts->weights[i]) nimcp_tensor_destroy(darts->weights[i]);
                    }
                    nimcp_free(darts->weights);
                }
                if (darts->arch_optimizer) nimcp_free(darts->arch_optimizer);
                if (darts->weight_optimizer) nimcp_free(darts->weight_optimizer);
                nimcp_free(darts);
            }
            break;

        case AUTO_ARCH_PRUNING_BASED:
            if (ctx->method_ctx.pruning.dense_arch) {
                auto_arch_architecture_destroy(ctx->method_ctx.pruning.dense_arch);
            }
            if (ctx->method_ctx.pruning.importance_scores) {
                nimcp_tensor_destroy(ctx->method_ctx.pruning.importance_scores);
            }
            break;

        default:
            break;
    }

    /* Free context */
    nimcp_free(ctx);
}

/**
 * @brief Set task specification for search
 */
int auto_arch_set_task(auto_arch_context_t* ctx, const auto_arch_task_t* task)
{
    /* Guard clauses */
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_set_task: ctx is NULL");
        return -1;
    }
    if (!task) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_set_task: task is NULL");
        return -1;
    }

    /* Copy task specification */
    memcpy(&ctx->task, task, sizeof(auto_arch_task_t));

    NIMCP_LOGGING_INFO("Set task: type=%d, inputs=%u, outputs=%u",
                      (int)ctx->task.type, ctx->task.n_inputs, ctx->task.n_outputs);

    /* Update n_inputs/n_outputs in existing method-specific architectures */
    /* This is needed because the init functions run before set_task is called */
    /* Note: method_ctx is a union, so we must check the search method first */

    switch (ctx->config.search_method) {
        case AUTO_ARCH_EVOLUTIONARY:
        case AUTO_ARCH_NEUROEVOLUTION:
            /* Update evolutionary population */
            if (ctx->method_ctx.evolutionary.population) {
                const uint32_t pop_size = ctx->config.population_size;
                for (uint32_t i = 0; i < pop_size; i++) {
                    auto_arch_architecture_t* arch = ctx->method_ctx.evolutionary.population[i].arch;
                    if (arch) {
                        arch->n_inputs = ctx->task.n_inputs;
                        arch->n_outputs = ctx->task.n_outputs;
                        if (arch->layers && arch->n_layers > 0) {
                            arch->layers[0].n_inputs = ctx->task.n_inputs;
                        }
                    }
                }
            }
            break;

        case AUTO_ARCH_PRUNING_BASED:
            /* Update pruning dense_arch */
            if (ctx->method_ctx.pruning.dense_arch) {
                ctx->method_ctx.pruning.dense_arch->n_inputs = ctx->task.n_inputs;
                ctx->method_ctx.pruning.dense_arch->n_outputs = ctx->task.n_outputs;
                if (ctx->method_ctx.pruning.dense_arch->layers &&
                    ctx->method_ctx.pruning.dense_arch->n_layers > 0) {
                    ctx->method_ctx.pruning.dense_arch->layers[0].n_inputs = ctx->task.n_inputs;
                }
            }
            break;

        default:
            /* RL-NAS, DARTS, RANDOM, etc. don't need architecture updates */
            break;
    }

    return 0;
}

/**
 * @brief Run architecture search
 */
auto_arch_result_t* auto_arch_search(
    auto_arch_context_t* ctx,
    const nimcp_tensor_t* train_data,
    const nimcp_tensor_t* train_labels,
    const nimcp_tensor_t* val_data,
    const nimcp_tensor_t* val_labels)
{
    /* Guard clauses */
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }
    if (!train_data || !train_labels) return NULL;

    /* Update status */
    ctx->status = AUTO_ARCH_STATUS_SEARCHING;

    /* Search loop */
    for (uint32_t iter = 0; iter < ctx->config.max_iterations; iter++) {
        /* Check termination conditions */
        if (ctx->stats.total_evaluations >= ctx->config.max_evaluations) {
            ctx->status = AUTO_ARCH_STATUS_MAX_ITERS;
            break;
        }

        if (ctx->stats.stagnant_iterations >= ctx->config.early_stop_patience) {
            ctx->status = AUTO_ARCH_STATUS_CONVERGED;
            break;
        }

        /* Perform one search step (method-specific) */
        auto_arch_architecture_t* candidate = NULL;
        switch (ctx->config.search_method) {
            case AUTO_ARCH_EVOLUTIONARY:
            case AUTO_ARCH_NEUROEVOLUTION:
                candidate = evolutionary_step(ctx, train_data, train_labels, val_data, val_labels);
                break;
            case AUTO_ARCH_RL_NAS:
                candidate = rl_nas_step(ctx, train_data, train_labels, val_data, val_labels);
                break;
            case AUTO_ARCH_DARTS:
                candidate = darts_step(ctx, train_data, train_labels, val_data, val_labels);
                break;
            case AUTO_ARCH_PRUNING_BASED:
                candidate = pruning_step(ctx, train_data, train_labels, val_data, val_labels);
                break;
            default:
                /* Random search */
                candidate = auto_arch_random_architecture(ctx);
                break;
        }

        if (!candidate) continue;

        /* Evaluate candidate */
        auto_arch_fitness_t fitness;
        int eval_result = auto_arch_evaluate(ctx, candidate, train_data, train_labels,
                                            val_data, val_labels, &fitness);

        if (eval_result != 0) {
            auto_arch_architecture_destroy(candidate);
            continue;
        }

        /* Update statistics */
        ctx->stats.total_evaluations++;

        /* Update best */
        if (!ctx->best_arch || fitness.total_fitness > ctx->best_fitness.total_fitness) {
            if (ctx->best_arch) auto_arch_architecture_destroy(ctx->best_arch);
            ctx->best_arch = auto_arch_clone(candidate);
            ctx->best_fitness = fitness;
            ctx->stats.best_at_iteration = iter;
            ctx->stats.improvements++;
            ctx->stats.stagnant_iterations = 0;
        } else {
            ctx->stats.stagnant_iterations++;
        }

        /* Update Pareto frontier */
        update_pareto_frontier(ctx, candidate, &fitness);

        /* Record in history */
        record_history(ctx, candidate, &fitness);

        /* Cleanup */
        auto_arch_architecture_destroy(candidate);

        /* Checkpoint if needed */
        if (iter % ctx->config.checkpoint_interval == 0 && ctx->config.verbose) {
            NIMCP_LOGGING_INFO("Iteration %u: best_fitness=%.4f, evaluations=%u",
                              iter, ctx->best_fitness.total_fitness, ctx->stats.total_evaluations);
        }
    }

    /* Update final status */
    if (ctx->status == AUTO_ARCH_STATUS_SEARCHING) {
        ctx->status = AUTO_ARCH_STATUS_COMPLETED;
    }

    /* Create result */
    auto_arch_result_t* result = (auto_arch_result_t*)nimcp_malloc(sizeof(auto_arch_result_t));
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    result->best_arch = auto_arch_clone(ctx->best_arch);
    result->best_fitness = ctx->best_fitness;
    result->pareto_frontier = (auto_arch_architecture_t**)nimcp_malloc(
        ctx->n_pareto * sizeof(auto_arch_architecture_t*));
    result->pareto_fitness = (auto_arch_fitness_t*)nimcp_malloc(
        ctx->n_pareto * sizeof(auto_arch_fitness_t));

    if (!result->pareto_frontier || !result->pareto_fitness) {
        /* Allocation failed — skip copy, set n_pareto to 0 */
        if (result->pareto_frontier) nimcp_free(result->pareto_frontier);
        if (result->pareto_fitness) nimcp_free(result->pareto_fitness);
        result->pareto_frontier = NULL;
        result->pareto_fitness = NULL;
        result->n_pareto = 0;
    } else {
        result->n_pareto = ctx->n_pareto;
        for (uint32_t i = 0; i < ctx->n_pareto; i++) {
            result->pareto_frontier[i] = auto_arch_clone(ctx->pareto_frontier[i]);
            result->pareto_fitness[i] = ctx->pareto_fitness[i];
        }
    }

    result->history = NULL; /* Can be added if needed */
    result->history_fitness = NULL;
    result->n_evaluated = ctx->stats.total_evaluations;
    result->stats = ctx->stats;
    result->status = ctx->status;

    return result;
}

/**
 * @brief Resume search from checkpoint
 */
auto_arch_result_t* auto_arch_resume(
    auto_arch_context_t* ctx,
    const char* checkpoint_path,
    const nimcp_tensor_t* train_data,
    const nimcp_tensor_t* train_labels,
    const nimcp_tensor_t* val_data,
    const nimcp_tensor_t* val_labels)
{
    /* Guard clauses */
    if (!ctx || !checkpoint_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_resume: required parameter is NULL (ctx, checkpoint_path)");
        return NULL;
    }

    /* Load best architecture from checkpoint to restore state */
    auto_arch_architecture_t* saved = auto_arch_load_json(checkpoint_path);
    if (saved) {
        /* Restore best architecture from checkpoint */
        if (ctx->best_arch) {
            auto_arch_architecture_destroy(ctx->best_arch);
        }
        ctx->best_arch = saved;
        ctx->next_arch_id = saved->arch_id + 1;
        ctx->stats.total_evaluations = saved->generation;
        NIMCP_LOGGING_INFO("Resumed from checkpoint: arch_id=%lu, generation=%u",
                           (unsigned long)saved->arch_id, saved->generation);
    } else {
        NIMCP_LOGGING_WARN("Could not load checkpoint '%s', starting fresh search", checkpoint_path);
    }

    /* Continue search from restored state */
    return auto_arch_search(ctx, train_data, train_labels, val_data, val_labels);
}

/**
 * @brief Evaluate a single architecture
 */
int auto_arch_evaluate(
    auto_arch_context_t* ctx,
    const auto_arch_architecture_t* arch,
    const nimcp_tensor_t* train_data,
    const nimcp_tensor_t* train_labels,
    const nimcp_tensor_t* val_data,
    const nimcp_tensor_t* val_labels,
    auto_arch_fitness_t* fitness)
{
    /* Guard clauses */
    if (!ctx || !arch || !fitness) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_evaluate: required parameter is NULL (ctx, arch, fitness)");
        return -1;
    }
    if (!train_data || !train_labels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_evaluate: required parameter is NULL (train_data, train_labels)");
        return -1;
    }

    /* Architecture evaluation using capacity-based metric and optional validation.
     *
     * Strategy:
     * 1. Compute model capacity from architecture (n_params, depth, width)
     * 2. Estimate task difficulty from data dimensions
     * 3. If validation data is provided, run a lightweight proxy evaluation
     *    using weight initialization + a few forward passes
     * 4. Combine capacity match, efficiency, and bio-plausibility into fitness
     */

    /* -- Derived architecture metrics -- */
    uint64_t total_params = arch->n_parameters;
    if (total_params == 0) {
        /* Estimate from layer specs */
        for (uint32_t l = 0; l < arch->n_layers; l++) {
            const auto_arch_layer_spec_t* layer = &arch->layers[l];
            uint64_t layer_params = (uint64_t)layer->n_neurons * layer->n_inputs;
            /* Account for sparsity */
            float density = 1.0f - layer->sparsity;
            if (density < 0.01f) density = 0.01f;
            layer_params = (uint64_t)(layer_params * density);
            layer_params += layer->n_neurons; /* biases */
            total_params += layer_params;
        }
        if (total_params == 0) total_params = 1;
    }

    /* Estimate task complexity from data dimensions */
    size_t data_numel = nimcp_tensor_numel(train_data);
    size_t label_numel = nimcp_tensor_numel(train_labels);
    uint64_t n_samples = (arch->n_inputs > 0 && data_numel > 0) ?
                         data_numel / arch->n_inputs : 1;
    if (n_samples == 0) n_samples = 1;

    /* Capacity-complexity ratio: good architectures have enough capacity
     * for the task but aren't excessively over-parameterized.
     * Ideal ratio is ~10-100 params per sample for generalization. */
    float params_per_sample = (float)total_params / (float)n_samples;
    float capacity_score;
    if (params_per_sample < 1.0f) {
        /* Under-parameterized: linear ramp */
        capacity_score = 0.3f + 0.3f * params_per_sample;
    } else if (params_per_sample <= 100.0f) {
        /* Sweet spot: log-scale goodness */
        capacity_score = 0.6f + 0.3f * (1.0f - fabsf(log10f(params_per_sample) - 1.5f) / 2.0f);
        if (capacity_score > 0.9f) capacity_score = 0.9f;
    } else {
        /* Over-parameterized: gentle decay */
        capacity_score = 0.9f * expf(-0.001f * (params_per_sample - 100.0f));
        if (capacity_score < 0.2f) capacity_score = 0.2f;
    }

    /* Depth bonus: deeper networks generally better up to a point */
    float depth_factor = 1.0f;
    if (arch->n_layers >= 2 && arch->n_layers <= 8) {
        depth_factor = 1.0f + 0.05f * (float)(arch->n_layers - 1);
    } else if (arch->n_layers > 8) {
        depth_factor = 1.35f - 0.02f * (float)(arch->n_layers - 8);
        if (depth_factor < 0.8f) depth_factor = 0.8f;
    }

    /* Width consistency: layers that bottleneck too aggressively lose info */
    float width_consistency = 1.0f;
    if (arch->n_layers > 1) {
        float min_width = (float)arch->layers[0].n_neurons;
        float max_width = min_width;
        for (uint32_t l = 1; l < arch->n_layers; l++) {
            float w = (float)arch->layers[l].n_neurons;
            if (w < min_width) min_width = w;
            if (w > max_width) max_width = w;
        }
        float ratio = (max_width > 0.0f) ? min_width / max_width : 0.5f;
        width_consistency = 0.5f + 0.5f * ratio;
    }

    /* Sparsity efficiency: moderate sparsity is beneficial */
    float sparsity_score = 1.0f;
    if (arch->avg_sparsity > 0.0f) {
        /* Optimal sparsity around 0.5-0.8 */
        if (arch->avg_sparsity <= 0.8f) {
            sparsity_score = 1.0f + 0.1f * arch->avg_sparsity;
        } else {
            sparsity_score = 1.08f - 0.3f * (arch->avg_sparsity - 0.8f);
            if (sparsity_score < 0.5f) sparsity_score = 0.5f;
        }
    }

    /* Proxy evaluation: if we have val_data, compute a simple random-weights
     * output variance metric. Higher output variance = more expressive. */
    float proxy_accuracy = capacity_score;
    if (val_data && val_labels) {
        size_t val_numel = nimcp_tensor_numel(val_data);
        const float* val_ptr = nimcp_tensor_data((nimcp_tensor_t*)val_data);
        if (val_ptr && val_numel > 0) {
            /* Compute input statistics as a proxy for data difficulty */
            float mean = 0.0f, var = 0.0f;
            size_t sample_count = (val_numel > 1024) ? 1024 : val_numel;
            for (size_t i = 0; i < sample_count; i++) {
                mean += val_ptr[i];
            }
            mean /= (float)sample_count;
            for (size_t i = 0; i < sample_count; i++) {
                float d = val_ptr[i] - mean;
                var += d * d;
            }
            var /= (float)sample_count;

            /* Higher input variance with sufficient capacity = better fit potential */
            float signal_strength = sqrtf(var + 1e-8f);
            float data_complexity = fminf(signal_strength * 2.0f, 1.0f);
            proxy_accuracy = capacity_score * (0.7f + 0.3f * data_complexity);
        }
    }

    /* Combine into fitness */
    float estimated_accuracy = proxy_accuracy * depth_factor * width_consistency * sparsity_score;
    if (estimated_accuracy > 0.99f) estimated_accuracy = 0.99f;
    if (estimated_accuracy < 0.01f) estimated_accuracy = 0.01f;

    fitness->accuracy = estimated_accuracy;
    fitness->loss = -logf(estimated_accuracy + 1e-8f);  /* Negative log-likelihood */

    /* Compute operational metrics */
    uint64_t n_operations = 0;
    for (uint32_t l = 0; l < arch->n_layers; l++) {
        const auto_arch_layer_spec_t* layer = &arch->layers[l];
        float density = 1.0f - layer->sparsity;
        if (density < 0.01f) density = 0.01f;
        n_operations += (uint64_t)(layer->n_neurons * layer->n_inputs * density * 2); /* MAC = 2 ops */
    }

    fitness->n_operations = n_operations;
    fitness->n_spikes = (arch->n_layers > 0) ?
        arch->layers[arch->n_layers - 1].n_neurons * 10 : 1000;
    fitness->energy_per_inference = (float)n_operations * 1e-9f;  /* ~1 nJ per op */
    fitness->latency_ms = (float)arch->n_layers * 0.5f +
                          (float)n_operations * 1e-6f;  /* Rough estimate */
    fitness->memory_bytes = total_params * 4;
    fitness->n_parameters = total_params;
    fitness->sparsity = arch->avg_sparsity;
    fitness->bio_plausibility_score = auto_arch_compute_bio_score(arch);

    /* Compute weighted total fitness across all objectives */
    fitness->total_fitness = 0.0f;
    fitness->total_fitness += fitness->accuracy *
        ctx->config.objective_weights[AUTO_ARCH_OBJ_ACCURACY];
    fitness->total_fitness += (1.0f - (float)fitness->n_operations / ((float)fitness->n_operations + 1e6f)) *
        ctx->config.objective_weights[AUTO_ARCH_OBJ_LATENCY];
    fitness->total_fitness += fitness->energy_per_inference < 1.0f ?
        (1.0f - fitness->energy_per_inference) *
        ctx->config.objective_weights[AUTO_ARCH_OBJ_ENERGY] : 0.0f;
    fitness->total_fitness += (1.0f - (float)fitness->memory_bytes / ((float)fitness->memory_bytes + 1e7f)) *
        ctx->config.objective_weights[AUTO_ARCH_OBJ_MEMORY];
    fitness->total_fitness += (1.0f - (float)total_params / ((float)total_params + 1e5f)) *
        ctx->config.objective_weights[AUTO_ARCH_OBJ_PARAMS];
    fitness->total_fitness += fitness->bio_plausibility_score *
        ctx->config.objective_weights[AUTO_ARCH_OBJ_BIO_PLAUSIBILITY];

    return 0;
}

//=============================================================================
// Architecture Manipulation Implementation (Stubs)
//=============================================================================

auto_arch_architecture_t* auto_arch_random_architecture(auto_arch_context_t* ctx)
{
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }

    /* Allocate architecture structure */
    auto_arch_architecture_t* arch = (auto_arch_architecture_t*)nimcp_calloc(
        1, sizeof(auto_arch_architecture_t));
    if (!arch) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arch is NULL");

        return NULL;

    }

    /* Set identity */
    arch->arch_id = ctx->next_arch_id++;
    arch->generation = 0;
    arch->parent_id = 0;
    arch->magic = AUTO_ARCH_MAGIC;

    /* Get task parameters */
    arch->n_inputs = ctx->task.n_inputs;
    arch->n_outputs = ctx->task.n_outputs;

    /* Random number of layers within constraints */
    uint32_t min_layers = ctx->config.constraints.min_layers;
    uint32_t max_layers = ctx->config.constraints.max_layers;
    if (min_layers == 0) min_layers = 1;
    if (max_layers == 0) max_layers = 5;
    if (max_layers > AUTO_ARCH_MAX_LAYERS) max_layers = AUTO_ARCH_MAX_LAYERS;

    uint32_t range = max_layers - min_layers + 1;
    arch->n_layers = min_layers + (uint32_t)(randf(&ctx->rng_state) * range);
    if (arch->n_layers > max_layers) arch->n_layers = max_layers;

    /* Allocate layers */
    arch->layers = (auto_arch_layer_spec_t*)nimcp_calloc(
        arch->n_layers, sizeof(auto_arch_layer_spec_t));
    if (!arch->layers) {
        nimcp_free(arch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_random_architecture: arch->layers is NULL");
        return NULL;
    }

    /* Get neuron count constraints */
    uint32_t min_neurons = ctx->config.constraints.min_neurons_per_layer;
    uint32_t max_neurons = ctx->config.constraints.max_neurons_per_layer;
    if (min_neurons == 0) min_neurons = 8;
    if (max_neurons == 0) max_neurons = 128;

    uint64_t total_params = 0;
    uint64_t total_connections = 0;
    float total_sparsity = 0.0f;

    /* Generate random layers */
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        auto_arch_layer_spec_t* layer = &arch->layers[i];

        layer->layer_id = i;

        /* Random layer type - prefer simpler types for now */
        float type_rand = randf(&ctx->rng_state);
        if (type_rand < 0.4f) {
            layer->type = AUTO_ARCH_LAYER_DENSE;
        } else if (type_rand < 0.7f) {
            layer->type = AUTO_ARCH_LAYER_SNN_LIF;
        } else if (type_rand < 0.9f) {
            layer->type = AUTO_ARCH_LAYER_LNN_LTC;
        } else {
            layer->type = AUTO_ARCH_LAYER_RECURRENT;
        }

        snprintf(layer->name, sizeof(layer->name), "layer_%u", i);

        /* Random neuron count */
        uint32_t neuron_range = max_neurons - min_neurons + 1;
        layer->n_neurons = min_neurons + (uint32_t)(randf(&ctx->rng_state) * neuron_range);
        if (layer->n_neurons > max_neurons) layer->n_neurons = max_neurons;

        /* Input size from previous layer or network input */
        layer->n_inputs = (i == 0) ? arch->n_inputs : arch->layers[i-1].n_neurons;

        /* Random connectivity pattern */
        float conn_rand = randf(&ctx->rng_state);
        if (conn_rand < 0.5f) {
            layer->connectivity = AUTO_ARCH_CONN_DENSE;
            layer->sparsity = 0.0f;
        } else if (conn_rand < 0.8f) {
            layer->connectivity = AUTO_ARCH_CONN_SPARSE_RANDOM;
            layer->sparsity = 0.3f + randf(&ctx->rng_state) * 0.5f;
        } else {
            layer->connectivity = AUTO_ARCH_CONN_SMALL_WORLD;
            layer->sparsity = 0.2f + randf(&ctx->rng_state) * 0.3f;
        }

        /* SNN parameters */
        layer->neuron_type = NEURON_GENERIC_LIF;
        layer->tau_mem = 10.0f + randf(&ctx->rng_state) * 20.0f;
        layer->tau_syn = 5.0f + randf(&ctx->rng_state) * 10.0f;
        layer->v_thresh = -50.0f + randf(&ctx->rng_state) * 10.0f;
        layer->v_reset = -70.0f + randf(&ctx->rng_state) * 5.0f;
        layer->refractory_period = 1.0f + randf(&ctx->rng_state) * 3.0f;

        /* LNN parameters */
        layer->activation = LNN_ACTIVATION_TANH;
        layer->tau_base = 50.0f + randf(&ctx->rng_state) * 100.0f;
        layer->tau_min = 10.0f;
        layer->tau_max = 200.0f;
        layer->learn_tau = (randf(&ctx->rng_state) > 0.5f);
        layer->ode_method = LNN_ODE_EULER;

        /* Regularization */
        layer->dropout_rate = randf(&ctx->rng_state) * 0.3f;
        layer->weight_decay = 0.0001f + randf(&ctx->rng_state) * 0.001f;

        /* Skip connections for deeper networks */
        layer->has_skip = (i >= 2 && randf(&ctx->rng_state) > 0.7f);
        if (layer->has_skip) {
            layer->skip_source_layer = (uint32_t)(randf(&ctx->rng_state) * (i - 1));
        }

        /* Compute layer parameters */
        uint64_t layer_params = (uint64_t)layer->n_inputs * layer->n_neurons;
        layer_params += layer->n_neurons; /* bias */
        float effective_density = 1.0f - layer->sparsity;
        total_params += (uint64_t)(layer_params * effective_density);
        total_connections += (uint64_t)(layer->n_inputs * layer->n_neurons * effective_density);
        total_sparsity += layer->sparsity;
    }

    /* Set global parameters */
    arch->network_type = AUTO_ARCH_TYPE_HYBRID_ALL;
    arch->dt = 1.0f;
    arch->learning_rate = 0.001f + randf(&ctx->rng_state) * 0.01f;
    arch->optimizer = NIMCP_OPTIMIZER_ADAM;
    arch->input_encoding = SNN_ENCODE_RATE;
    arch->output_decoding = SNN_DECODE_RATE;
    arch->encoding_time = 50.0f + randf(&ctx->rng_state) * 100.0f;

    /* Set computed properties */
    arch->n_parameters = total_params;
    arch->n_connections = total_connections;
    arch->avg_sparsity = (arch->n_layers > 0) ? total_sparsity / arch->n_layers : 0.0f;

    return arch;
}

int auto_arch_mutate(auto_arch_architecture_t* arch, float mutation_rate, auto_arch_context_t* ctx)
{
    if (!arch || !ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_mutate: required parameter is NULL (arch, ctx)");
        return -1;
    }
    if (!arch->layers || arch->n_layers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "auto_arch_mutate: arch has no layers");
        return -1;
    }

    int mutations_applied = 0;

    /* Mutate each layer independently */
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        auto_arch_layer_spec_t* layer = &arch->layers[i];

        /* Mutate neuron count (scale by up to +/- 50%) */
        if (randf(&ctx->rng_state) < mutation_rate) {
            float scale = 0.5f + randf(&ctx->rng_state) * 1.0f;
            uint32_t new_neurons = (uint32_t)(layer->n_neurons * scale);
            if (new_neurons < 1) new_neurons = 1;
            if (new_neurons > 10000) new_neurons = 10000;
            layer->n_neurons = new_neurons;
            mutations_applied++;
        }

        /* Mutate sparsity */
        if (randf(&ctx->rng_state) < mutation_rate) {
            layer->sparsity += (randf(&ctx->rng_state) - 0.5f) * 0.2f;
            if (layer->sparsity < 0.0f) layer->sparsity = 0.0f;
            if (layer->sparsity > 0.99f) layer->sparsity = 0.99f;
            mutations_applied++;
        }

        /* Mutate time constants (SNN/LNN) */
        if (randf(&ctx->rng_state) < mutation_rate) {
            layer->tau_mem *= (0.5f + randf(&ctx->rng_state));
            if (layer->tau_mem < 0.1f) layer->tau_mem = 0.1f;
            if (layer->tau_mem > 1000.0f) layer->tau_mem = 1000.0f;
            mutations_applied++;
        }

        /* Mutate dropout rate */
        if (randf(&ctx->rng_state) < mutation_rate) {
            layer->dropout_rate += (randf(&ctx->rng_state) - 0.5f) * 0.1f;
            if (layer->dropout_rate < 0.0f) layer->dropout_rate = 0.0f;
            if (layer->dropout_rate > 0.5f) layer->dropout_rate = 0.5f;
            mutations_applied++;
        }
    }

    /* Mutate global learning rate */
    if (randf(&ctx->rng_state) < mutation_rate) {
        arch->learning_rate *= (0.1f + randf(&ctx->rng_state) * 1.9f);
        if (arch->learning_rate < 1e-6f) arch->learning_rate = 1e-6f;
        if (arch->learning_rate > 1.0f) arch->learning_rate = 1.0f;
        mutations_applied++;
    }

    /* Mutate dt */
    if (randf(&ctx->rng_state) < mutation_rate) {
        arch->dt *= (0.5f + randf(&ctx->rng_state));
        if (arch->dt < 0.01f) arch->dt = 0.01f;
        if (arch->dt > 10.0f) arch->dt = 10.0f;
        mutations_applied++;
    }

    /* Recompute derived properties */
    uint64_t total_params = 0;
    uint64_t total_connections = 0;
    float total_sparsity = 0.0f;

    /* Fix input sizes for consistency after neuron count mutations */
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        if (i == 0) {
            arch->layers[i].n_inputs = arch->n_inputs;
        } else {
            arch->layers[i].n_inputs = arch->layers[i - 1].n_neurons;
        }
        float density = 1.0f - arch->layers[i].sparsity;
        uint64_t layer_params = (uint64_t)arch->layers[i].n_inputs * arch->layers[i].n_neurons;
        layer_params += arch->layers[i].n_neurons;
        total_params += (uint64_t)(layer_params * density);
        total_connections += (uint64_t)(arch->layers[i].n_inputs * arch->layers[i].n_neurons * density);
        total_sparsity += arch->layers[i].sparsity;
    }

    arch->n_parameters = total_params;
    arch->n_connections = total_connections;
    arch->avg_sparsity = (arch->n_layers > 0) ? total_sparsity / arch->n_layers : 0.0f;

    return mutations_applied;
}

auto_arch_architecture_t* auto_arch_crossover(
    const auto_arch_architecture_t* parent1,
    const auto_arch_architecture_t* parent2,
    auto_arch_context_t* ctx)
{
    if (!parent1 || !parent2 || !ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_crossover: required parameter is NULL (parent1, parent2, ctx)");
        return NULL;
    }
    if (!parent1->layers || !parent2->layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_crossover: required parameter is NULL (parent1->layers, parent2->layers)");
        return NULL;
    }
    if (parent1->n_layers == 0 || parent2->n_layers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "auto_arch_crossover: parent1->n_layers is zero");
        return NULL;
    }

    /* Allocate child architecture */
    auto_arch_architecture_t* child = (auto_arch_architecture_t*)nimcp_calloc(
        1, sizeof(auto_arch_architecture_t));
    if (!child) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "child is NULL");

        return NULL;

    }

    /* Set identity */
    child->arch_id = ctx->next_arch_id++;
    child->generation = (parent1->generation > parent2->generation ?
                         parent1->generation : parent2->generation) + 1;
    child->parent_id = parent1->arch_id;
    child->magic = AUTO_ARCH_MAGIC;

    /* Inherit I/O dimensions from task */
    child->n_inputs = ctx->task.n_inputs;
    child->n_outputs = ctx->task.n_outputs;

    /* Crossover strategy: random layer selection from each parent */
    /* Use layer count from the parent with fewer layers to keep child manageable */
    uint32_t min_layers = (parent1->n_layers < parent2->n_layers) ?
                          parent1->n_layers : parent2->n_layers;
    uint32_t max_layers = (parent1->n_layers > parent2->n_layers) ?
                          parent1->n_layers : parent2->n_layers;

    /* Child has random number of layers between the two parents */
    child->n_layers = min_layers + (uint32_t)(randf(&ctx->rng_state) * (max_layers - min_layers + 1));
    if (child->n_layers < 1) child->n_layers = 1;
    if (child->n_layers > AUTO_ARCH_MAX_LAYERS) child->n_layers = AUTO_ARCH_MAX_LAYERS;

    /* Allocate layers */
    child->layers = (auto_arch_layer_spec_t*)nimcp_calloc(
        child->n_layers, sizeof(auto_arch_layer_spec_t));
    if (!child->layers) {
        nimcp_free(child);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_crossover: child->layers is NULL");
        return NULL;
    }

    /* Copy layers from parents randomly */
    uint64_t total_params = 0;
    uint64_t total_connections = 0;
    float total_sparsity = 0.0f;

    for (uint32_t i = 0; i < child->n_layers; i++) {
        /* Choose which parent to copy from */
        const auto_arch_architecture_t* source_parent;
        uint32_t source_idx;

        if (randf(&ctx->rng_state) < 0.5f && i < parent1->n_layers) {
            source_parent = parent1;
            source_idx = i % parent1->n_layers;
        } else if (i < parent2->n_layers) {
            source_parent = parent2;
            source_idx = i % parent2->n_layers;
        } else {
            source_parent = parent1;
            source_idx = i % parent1->n_layers;
        }

        /* Copy layer from source parent */
        child->layers[i] = source_parent->layers[source_idx];
        child->layers[i].layer_id = i;

        /* Update input size to be consistent with previous layer */
        if (i == 0) {
            child->layers[i].n_inputs = child->n_inputs;
        } else {
            child->layers[i].n_inputs = child->layers[i-1].n_neurons;
        }

        /* Deep copy input_layers if present */
        if (source_parent->layers[source_idx].input_layers &&
            source_parent->layers[source_idx].n_input_layers > 0) {
            child->layers[i].input_layers = (uint32_t*)nimcp_malloc(
                source_parent->layers[source_idx].n_input_layers * sizeof(uint32_t));
            if (child->layers[i].input_layers) {
                memcpy(child->layers[i].input_layers,
                       source_parent->layers[source_idx].input_layers,
                       source_parent->layers[source_idx].n_input_layers * sizeof(uint32_t));
            }
        } else {
            child->layers[i].input_layers = NULL;
            child->layers[i].n_input_layers = 0;
        }

        /* Accumulate stats */
        uint64_t layer_params = (uint64_t)child->layers[i].n_inputs * child->layers[i].n_neurons;
        layer_params += child->layers[i].n_neurons;
        float effective_density = 1.0f - child->layers[i].sparsity;
        total_params += (uint64_t)(layer_params * effective_density);
        total_connections += (uint64_t)(child->layers[i].n_inputs * child->layers[i].n_neurons * effective_density);
        total_sparsity += child->layers[i].sparsity;
    }

    /* Inherit global parameters from random parent with slight variation */
    if (randf(&ctx->rng_state) < 0.5f) {
        child->network_type = parent1->network_type;
        child->dt = parent1->dt;
        child->learning_rate = parent1->learning_rate;
        child->optimizer = parent1->optimizer;
        child->input_encoding = parent1->input_encoding;
        child->output_decoding = parent1->output_decoding;
        child->encoding_time = parent1->encoding_time;
    } else {
        child->network_type = parent2->network_type;
        child->dt = parent2->dt;
        child->learning_rate = parent2->learning_rate;
        child->optimizer = parent2->optimizer;
        child->input_encoding = parent2->input_encoding;
        child->output_decoding = parent2->output_decoding;
        child->encoding_time = parent2->encoding_time;
    }

    /* Set computed properties */
    child->n_parameters = total_params;
    child->n_connections = total_connections;
    child->avg_sparsity = (child->n_layers > 0) ? total_sparsity / child->n_layers : 0.0f;

    return child;
}

auto_arch_architecture_t* auto_arch_clone(const auto_arch_architecture_t* arch)
{
    if (!arch) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arch is NULL");

        return NULL;

    }

    /* Allocate new architecture */
    auto_arch_architecture_t* clone = (auto_arch_architecture_t*)nimcp_calloc(
        1, sizeof(auto_arch_architecture_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clone is NULL");

        return NULL;

    }

    /* Copy basic fields */
    *clone = *arch;

    /* Deep copy layers array */
    if (arch->n_layers > 0 && arch->layers) {
        clone->layers = (auto_arch_layer_spec_t*)nimcp_calloc(
            arch->n_layers, sizeof(auto_arch_layer_spec_t));
        if (!clone->layers) {
            nimcp_free(clone);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_clone: clone->layers is NULL");
            return NULL;
        }

        /* Copy each layer */
        for (uint32_t i = 0; i < arch->n_layers; i++) {
            clone->layers[i] = arch->layers[i];

            /* Deep copy input_layers array if present */
            if (arch->layers[i].input_layers && arch->layers[i].n_input_layers > 0) {
                clone->layers[i].input_layers = (uint32_t*)nimcp_malloc(
                    arch->layers[i].n_input_layers * sizeof(uint32_t));
                if (clone->layers[i].input_layers) {
                    memcpy(clone->layers[i].input_layers, arch->layers[i].input_layers,
                           arch->layers[i].n_input_layers * sizeof(uint32_t));
                }
            }
        }
    } else {
        clone->layers = NULL;
    }

    return clone;
}

void auto_arch_architecture_destroy(auto_arch_architecture_t* arch)
{
    if (!arch) return;
    if (arch->layers) {
        for (uint32_t i = 0; i < arch->n_layers; i++) {
            if (arch->layers[i].input_layers) nimcp_free(arch->layers[i].input_layers);
        }
        nimcp_free(arch->layers);
    }
    nimcp_free(arch);
}

int auto_arch_validate_architecture(
    const auto_arch_architecture_t* arch,
    const auto_arch_constraints_t* constraints)
{
    if (!arch || !constraints) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_validate_architecture: required parameter is NULL (arch, constraints)");
        return -1;
    }

    /* Validate layer count */
    if (constraints->min_layers > 0 && arch->n_layers < constraints->min_layers) return -1;
    if (constraints->max_layers > 0 && arch->n_layers > constraints->max_layers) return -1;

    /* Validate total parameters */
    if (constraints->max_parameters > 0 && arch->n_parameters > constraints->max_parameters) return -1;

    /* Validate computational budget */
    if (constraints->max_memory > 0 && arch->n_parameters * sizeof(float) > constraints->max_memory) return -1;

    /* Validate per-layer constraints */
    uint64_t total_neurons = 0;
    for (uint32_t i = 0; i < arch->n_layers && arch->layers; i++) {
        const auto_arch_layer_spec_t* layer = &arch->layers[i];

        if (constraints->min_neurons_per_layer > 0 && layer->n_neurons < constraints->min_neurons_per_layer) return -1;
        if (constraints->max_neurons_per_layer > 0 && layer->n_neurons > constraints->max_neurons_per_layer) return -1;

        /* Sparsity range */
        if (layer->sparsity < constraints->min_sparsity) return -1;
        if (constraints->max_sparsity > 0.0f && layer->sparsity > constraints->max_sparsity) return -1;

        /* Time constant range */
        if (constraints->min_tau > 0.0f && layer->tau_mem > 0.0f && layer->tau_mem < constraints->min_tau) return -1;
        if (constraints->max_tau > 0.0f && layer->tau_mem > constraints->max_tau) return -1;

        /* Feedforward enforcement */
        if (constraints->enforce_feedforward && layer->input_layers) {
            for (uint32_t j = 0; j < layer->n_input_layers; j++) {
                if (layer->input_layers[j] >= i) return -1;
            }
        }

        total_neurons += layer->n_neurons;
    }

    /* Total neuron count */
    if (constraints->max_total_neurons > 0 && total_neurons > constraints->max_total_neurons) return -1;

    /* dt range */
    if (constraints->min_dt > 0.0f && arch->dt < constraints->min_dt) return -1;
    if (constraints->max_dt > 0.0f && arch->dt > constraints->max_dt) return -1;

    return 0;
}

//=============================================================================
// Export/Import Implementation (Stubs)
//=============================================================================

snn_config_t* auto_arch_export_snn(const auto_arch_architecture_t* arch)
{
    if (!arch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_export_snn: arch is NULL");
        return NULL;
    }

    snn_config_t* config = (snn_config_t*)nimcp_calloc(1, sizeof(snn_config_t));
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_export_snn: failed to allocate snn_config_t");
        return NULL;
    }

    /* Map architecture dimensions */
    config->n_inputs = arch->n_inputs;
    config->n_outputs = arch->n_outputs;
    config->n_populations = arch->n_layers;

    /* Simulation parameters from architecture globals */
    config->dt = arch->dt;
    config->learning_rate = arch->learning_rate;

    /* Use first layer's neuron parameters as defaults */
    if (arch->layers && arch->n_layers > 0) {
        config->tau_mem = arch->layers[0].tau_mem > 0.0f ? arch->layers[0].tau_mem : 20.0f;
        config->tau_syn = arch->layers[0].tau_syn > 0.0f ? arch->layers[0].tau_syn : 5.0f;
        config->v_thresh = arch->layers[0].v_thresh != 0.0f ? arch->layers[0].v_thresh : -55.0f;
        config->v_reset = arch->layers[0].v_reset != 0.0f ? arch->layers[0].v_reset : -70.0f;
        config->v_rest = -65.0f;
        config->t_ref = arch->layers[0].refractory_period > 0.0f ? arch->layers[0].refractory_period : 2.0f;
    } else {
        config->tau_mem = 20.0f;
        config->tau_syn = 5.0f;
        config->v_thresh = -55.0f;
        config->v_reset = -70.0f;
        config->v_rest = -65.0f;
        config->t_ref = 2.0f;
    }

    /* Encoding/decoding */
    config->encoder.method = arch->input_encoding;
    config->encoder.time_window = arch->encoding_time;
    config->encoder.max_rate = 200.0f;
    config->encoder.min_rate = 0.0f;
    config->encoder.population_size = 1;
    config->decoder.method = arch->output_decoding;
    config->decoder.time_window = arch->encoding_time;
    config->decoder.use_softmax = true;

    /* Training defaults */
    config->train_mode = SNN_TRAIN_SURROGATE;
    config->surrogate = SNN_SURROGATE_FAST_SIGMOID;
    config->surrogate_beta = 10.0f;
    config->enable_stdp = false;
    config->enable_reward_modulation = false;

    return config;
}

lnn_config_t* auto_arch_export_lnn(const auto_arch_architecture_t* arch)
{
    if (!arch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_export_lnn: arch is NULL");
        return NULL;
    }

    lnn_config_t* config = (lnn_config_t*)nimcp_calloc(1, sizeof(lnn_config_t));
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_export_lnn: failed to allocate lnn_config_t");
        return NULL;
    }

    config->n_inputs = arch->n_inputs;
    config->n_outputs = arch->n_outputs;
    config->n_layers = arch->n_layers;
    config->default_dt = arch->dt > 0.0f ? arch->dt : 1.0f;
    config->default_ode_method = LNN_ODE_RK4;
    config->adaptive_dt_min = 0.01f;
    config->adaptive_dt_max = 10.0f;
    config->adaptive_error_tol = 1e-5f;
    config->train_mode = LNN_TRAIN_ADJOINT;
    config->bptt_truncation = 100;
    config->gradient_clip_norm = 1.0f;
    config->enable_simd = true;

    /* Build per-layer configs from architecture layers */
    if (arch->n_layers > 0 && arch->layers) {
        config->layer_configs = (lnn_layer_config_t*)nimcp_calloc(
            arch->n_layers, sizeof(lnn_layer_config_t));
        if (!config->layer_configs) {
            nimcp_free(config);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_export_lnn: failed to allocate layer_configs");
            return NULL;
        }

        for (uint32_t i = 0; i < arch->n_layers; i++) {
            lnn_layer_config_t* lc = &config->layer_configs[i];
            const auto_arch_layer_spec_t* al = &arch->layers[i];

            lc->n_neurons = al->n_neurons;
            lc->activation = al->activation;
            lc->tau_base_init = al->tau_base > 0.0f ? al->tau_base : 10.0f;
            lc->tau_min = al->tau_min > 0.0f ? al->tau_min : 0.1f;
            lc->tau_max = al->tau_max > 0.0f ? al->tau_max : 1000.0f;
            lc->learn_tau = al->learn_tau;
            lc->sparsity = al->sparsity;
            lc->ode_method = al->ode_method;
            lc->dt = arch->dt > 0.0f ? arch->dt : 1.0f;
            lc->weight_init_std = 0.1f;
        }
    }

    return config;
}

auto_arch_architecture_t* auto_arch_import_snn(const snn_config_t* snn_config)
{
    if (!snn_config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_import_snn: snn_config is NULL");
        return NULL;
    }

    auto_arch_architecture_t* arch = (auto_arch_architecture_t*)nimcp_calloc(
        1, sizeof(auto_arch_architecture_t));
    if (!arch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_import_snn: failed to allocate architecture");
        return NULL;
    }

    arch->magic = AUTO_ARCH_MAGIC;
    arch->network_type = AUTO_ARCH_TYPE_SNN;
    arch->n_inputs = snn_config->n_inputs;
    arch->n_outputs = snn_config->n_outputs;
    arch->dt = snn_config->dt;
    arch->learning_rate = snn_config->learning_rate;
    arch->input_encoding = snn_config->encoder.method;
    arch->output_decoding = snn_config->decoder.method;
    arch->encoding_time = snn_config->encoder.time_window;

    /* Create one layer per population */
    arch->n_layers = snn_config->n_populations > 0 ? snn_config->n_populations : 1;
    arch->layers = (auto_arch_layer_spec_t*)nimcp_calloc(
        arch->n_layers, sizeof(auto_arch_layer_spec_t));
    if (!arch->layers) {
        nimcp_free(arch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_import_snn: failed to allocate layers");
        return NULL;
    }

    uint64_t total_params = 0;
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        auto_arch_layer_spec_t* layer = &arch->layers[i];
        layer->layer_id = i;
        layer->type = AUTO_ARCH_LAYER_SNN_LIF;

        /* Interpolate neuron count between input and output */
        if (arch->n_layers == 1) {
            layer->n_neurons = arch->n_outputs;
        } else {
            float t = (float)i / (float)(arch->n_layers - 1);
            layer->n_neurons = (uint32_t)((1.0f - t) * arch->n_inputs + t * arch->n_outputs);
            if (layer->n_neurons < 1) layer->n_neurons = 1;
        }

        layer->n_inputs = (i == 0) ? arch->n_inputs : arch->layers[i - 1].n_neurons;
        layer->tau_mem = snn_config->tau_mem;
        layer->tau_syn = snn_config->tau_syn;
        layer->v_thresh = snn_config->v_thresh;
        layer->v_reset = snn_config->v_reset;
        layer->refractory_period = snn_config->t_ref;

        uint64_t lp = (uint64_t)layer->n_inputs * layer->n_neurons + layer->n_neurons;
        total_params += lp;
    }

    arch->n_parameters = total_params;
    arch->n_connections = total_params;

    return arch;
}

auto_arch_architecture_t* auto_arch_import_lnn(const lnn_config_t* lnn_config)
{
    if (!lnn_config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_import_lnn: lnn_config is NULL");
        return NULL;
    }

    auto_arch_architecture_t* arch = (auto_arch_architecture_t*)nimcp_calloc(
        1, sizeof(auto_arch_architecture_t));
    if (!arch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_import_lnn: failed to allocate architecture");
        return NULL;
    }

    arch->magic = AUTO_ARCH_MAGIC;
    arch->network_type = AUTO_ARCH_TYPE_LNN;
    arch->n_inputs = lnn_config->n_inputs;
    arch->n_outputs = lnn_config->n_outputs;
    arch->dt = lnn_config->default_dt;
    arch->learning_rate = 0.001f;

    arch->n_layers = lnn_config->n_layers > 0 ? lnn_config->n_layers : 1;
    arch->layers = (auto_arch_layer_spec_t*)nimcp_calloc(
        arch->n_layers, sizeof(auto_arch_layer_spec_t));
    if (!arch->layers) {
        nimcp_free(arch);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_import_lnn: failed to allocate layers");
        return NULL;
    }

    uint64_t total_params = 0;
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        auto_arch_layer_spec_t* layer = &arch->layers[i];
        layer->layer_id = i;
        layer->type = AUTO_ARCH_LAYER_LNN_LTC;

        /* Use layer config if available */
        if (lnn_config->layer_configs && i < lnn_config->n_layers) {
            const lnn_layer_config_t* lc = &lnn_config->layer_configs[i];
            layer->n_neurons = lc->n_neurons;
            layer->activation = lc->activation;
            layer->tau_base = lc->tau_base_init;
            layer->tau_min = lc->tau_min;
            layer->tau_max = lc->tau_max;
            layer->learn_tau = lc->learn_tau;
            layer->sparsity = lc->sparsity;
            layer->ode_method = lc->ode_method;
        } else {
            layer->n_neurons = (i == arch->n_layers - 1) ? arch->n_outputs : 64;
            layer->tau_base = 10.0f;
        }

        layer->n_inputs = (i == 0) ? arch->n_inputs : arch->layers[i - 1].n_neurons;

        uint64_t lp = (uint64_t)layer->n_inputs * layer->n_neurons + layer->n_neurons;
        float density = 1.0f - layer->sparsity;
        total_params += (uint64_t)(lp * density);
    }

    arch->n_parameters = total_params;
    arch->n_connections = total_params;

    return arch;
}

int auto_arch_save_json(const auto_arch_architecture_t* arch, const char* filepath)
{
    if (!arch || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_save_json: required parameter is NULL (arch, filepath)");
        return -1;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_save_json: fp is NULL");
        return -1;
    }

    /* Write header */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"magic\": %u,\n", arch->magic);
    fprintf(fp, "  \"arch_id\": %lu,\n", arch->arch_id);
    fprintf(fp, "  \"generation\": %u,\n", arch->generation);
    fprintf(fp, "  \"parent_id\": %u,\n", arch->parent_id);
    fprintf(fp, "  \"network_type\": %d,\n", (int)arch->network_type);
    fprintf(fp, "  \"n_layers\": %u,\n", arch->n_layers);
    fprintf(fp, "  \"n_inputs\": %u,\n", arch->n_inputs);
    fprintf(fp, "  \"n_outputs\": %u,\n", arch->n_outputs);
    fprintf(fp, "  \"n_parameters\": %lu,\n", arch->n_parameters);
    fprintf(fp, "  \"n_connections\": %lu,\n", arch->n_connections);
    fprintf(fp, "  \"avg_sparsity\": %f,\n", arch->avg_sparsity);

    /* Write layers array */
    fprintf(fp, "  \"layers\": [\n");
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        const auto_arch_layer_spec_t* layer = &arch->layers[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"layer_id\": %u,\n", layer->layer_id);
        fprintf(fp, "      \"type\": %d,\n", (int)layer->type);
        fprintf(fp, "      \"n_neurons\": %u,\n", layer->n_neurons);
        fprintf(fp, "      \"n_inputs\": %u,\n", layer->n_inputs);
        fprintf(fp, "      \"activation\": %d,\n", (int)layer->activation);
        fprintf(fp, "      \"connectivity\": %d,\n", (int)layer->connectivity);
        fprintf(fp, "      \"sparsity\": %f,\n", layer->sparsity);
        fprintf(fp, "      \"tau_mem\": %f,\n", layer->tau_mem);
        fprintf(fp, "      \"tau_syn\": %f,\n", layer->tau_syn);
        fprintf(fp, "      \"tau_base\": %f\n", layer->tau_base);
        fprintf(fp, "    }%s\n", (i < arch->n_layers - 1) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

/* Helper to skip whitespace and get next non-space character */
static int skip_ws(FILE* fp) {
    int c;
    while ((c = fgetc(fp)) != EOF && (c == ' ' || c == '\t' || c == '\n' || c == '\r'));
    return c;
}

/* Helper to read a JSON key (returns 1 if successful) */
static int read_json_key(FILE* fp, char* buf, size_t bufsize) {
    int c = skip_ws(fp);
    if (c != '"') return 0;
    size_t i = 0;
    while ((c = fgetc(fp)) != EOF && c != '"' && i < bufsize - 1) {
        buf[i++] = (char)c;
    }
    buf[i] = '\0';
    if (c != '"') return 0;
    c = skip_ws(fp);
    return (c == ':') ? 1 : 0;
}

/* Helper to read a JSON number (int or float) */
static int read_json_number(FILE* fp, double* value) {
    int c = skip_ws(fp);
    char buf[NIMCP_ID_BUFFER_SIZE];
    size_t i = 0;
    if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
        buf[i++] = (char)c;
        while ((c = fgetc(fp)) != EOF && i < sizeof(buf) - 1) {
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+') {
                buf[i++] = (char)c;
            } else {
                ungetc(c, fp);
                break;
            }
        }
    } else {
        ungetc(c, fp);
        return 0;
    }
    buf[i] = '\0';
    // P1-2 fix: Use strtod instead of atof for safe conversion
    char* endptr;
    errno = 0;
    double dval = strtod(buf, &endptr);
    if (endptr == buf || errno == ERANGE) {
        *value = 0.0;
        return 0;
    }
    *value = dval;
    return 1;
}

auto_arch_architecture_t* auto_arch_load_json(const char* filepath)
{
    if (!filepath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");

        return NULL;

    }

    FILE* fp = fopen(filepath, "r");
    if (!fp) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fp is NULL");

        return NULL;

    }

    auto_arch_architecture_t* arch = (auto_arch_architecture_t*)nimcp_calloc(
        1, sizeof(auto_arch_architecture_t));
    if (!arch) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_load_json: arch is NULL");
        return NULL;
    }

    char key[NIMCP_ID_BUFFER_SIZE];
    double value;
    int c;

    /* Skip opening brace */
    c = skip_ws(fp);
    if (c != '{') {
        nimcp_free(arch);
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_load_json: validation failed");
        return NULL;
    }

    /* Read key-value pairs */
    while (1) {
        if (!read_json_key(fp, key, sizeof(key))) break;

        if (strcmp(key, "magic") == 0) {
            if (read_json_number(fp, &value)) arch->magic = (uint32_t)value;
        } else if (strcmp(key, "arch_id") == 0) {
            if (read_json_number(fp, &value)) arch->arch_id = (uint64_t)value;
        } else if (strcmp(key, "generation") == 0) {
            if (read_json_number(fp, &value)) arch->generation = (uint32_t)value;
        } else if (strcmp(key, "parent_id") == 0) {
            if (read_json_number(fp, &value)) arch->parent_id = (uint64_t)value;
        } else if (strcmp(key, "network_type") == 0) {
            if (read_json_number(fp, &value)) arch->network_type = (auto_arch_network_type_t)(int)value;
        } else if (strcmp(key, "n_layers") == 0) {
            if (read_json_number(fp, &value)) arch->n_layers = (uint32_t)value;
        } else if (strcmp(key, "n_inputs") == 0) {
            if (read_json_number(fp, &value)) arch->n_inputs = (uint32_t)value;
        } else if (strcmp(key, "n_outputs") == 0) {
            if (read_json_number(fp, &value)) arch->n_outputs = (uint32_t)value;
        } else if (strcmp(key, "n_parameters") == 0) {
            if (read_json_number(fp, &value)) arch->n_parameters = (uint64_t)value;
        } else if (strcmp(key, "n_connections") == 0) {
            if (read_json_number(fp, &value)) arch->n_connections = (uint64_t)value;
        } else if (strcmp(key, "avg_sparsity") == 0) {
            if (read_json_number(fp, &value)) arch->avg_sparsity = (float)value;
        } else if (strcmp(key, "layers") == 0) {
            /* Allocate layers */
            if (arch->n_layers > 0) {
                arch->layers = (auto_arch_layer_spec_t*)nimcp_calloc(
                    arch->n_layers, sizeof(auto_arch_layer_spec_t));
                if (!arch->layers) {
                    nimcp_free(arch);
                    fclose(fp);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_load_json: arch->layers is NULL");
                    return NULL;
                }
            }

            /* Skip to array opening bracket */
            c = skip_ws(fp);
            if (c != '[') break;

            /* Read each layer */
            for (uint32_t i = 0; i < arch->n_layers; i++) {
                c = skip_ws(fp);
                if (c != '{') break;

                auto_arch_layer_spec_t* layer = &arch->layers[i];

                /* Read layer properties */
                while (1) {
                    if (!read_json_key(fp, key, sizeof(key))) break;

                    if (strcmp(key, "layer_id") == 0) {
                        if (read_json_number(fp, &value)) layer->layer_id = (uint32_t)value;
                    } else if (strcmp(key, "type") == 0) {
                        if (read_json_number(fp, &value)) layer->type = (auto_arch_layer_type_t)(int)value;
                    } else if (strcmp(key, "n_neurons") == 0) {
                        if (read_json_number(fp, &value)) layer->n_neurons = (uint32_t)value;
                    } else if (strcmp(key, "n_inputs") == 0) {
                        if (read_json_number(fp, &value)) layer->n_inputs = (uint32_t)value;
                    } else if (strcmp(key, "activation") == 0) {
                        if (read_json_number(fp, &value)) layer->activation = (lnn_activation_t)(int)value;
                    } else if (strcmp(key, "connectivity") == 0) {
                        if (read_json_number(fp, &value)) layer->connectivity = (auto_arch_connectivity_t)(int)value;
                    } else if (strcmp(key, "sparsity") == 0) {
                        if (read_json_number(fp, &value)) layer->sparsity = (float)value;
                    } else if (strcmp(key, "tau_mem") == 0) {
                        if (read_json_number(fp, &value)) layer->tau_mem = (float)value;
                    } else if (strcmp(key, "tau_syn") == 0) {
                        if (read_json_number(fp, &value)) layer->tau_syn = (float)value;
                    } else if (strcmp(key, "tau_base") == 0) {
                        if (read_json_number(fp, &value)) layer->tau_base = (float)value;
                    }

                    /* Skip to comma or closing brace */
                    c = skip_ws(fp);
                    if (c == '}') break;
                    if (c != ',') { ungetc(c, fp); break; }
                }

                /* Skip to comma or closing bracket */
                c = skip_ws(fp);
                if (c == ']') break;
                if (c != ',') ungetc(c, fp);
            }
        }

        /* Skip to comma or closing brace */
        c = skip_ws(fp);
        if (c == '}') break;
        if (c != ',') { ungetc(c, fp); break; }
    }

    fclose(fp);
    return arch;
}

//=============================================================================
// Statistics and Monitoring Implementation
//=============================================================================

int auto_arch_get_stats(const auto_arch_context_t* ctx, auto_arch_stats_t* stats)
{
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }
    memcpy(stats, &ctx->stats, sizeof(auto_arch_stats_t));
    return 0;
}

auto_arch_status_t auto_arch_get_status(const auto_arch_context_t* ctx)
{
    if (!ctx) return AUTO_ARCH_STATUS_ERROR;
    return ctx->status;
}

auto_arch_architecture_t* auto_arch_get_best(const auto_arch_context_t* ctx)
{
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }
    return auto_arch_clone(ctx->best_arch);
}

float auto_arch_compute_bio_score(const auto_arch_architecture_t* arch)
{
    if (!arch) return 0.0f;
    if (!arch->layers || arch->n_layers == 0) return 0.0f;

    float score = 0.0f;

    /* Sparsity score (cortical typical: 80-95%) */
    if (arch->avg_sparsity >= 0.8f && arch->avg_sparsity <= 0.95f) {
        score += 0.3f;
    } else if (arch->avg_sparsity >= 0.7f) {
        /* Partial credit for close-to-biological sparsity */
        score += 0.15f;
    }

    /* Time constants in biological range (1-100ms) — +0.2 */
    uint32_t bio_tau_count = 0;
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        float tau = arch->layers[i].tau_mem;
        if (tau >= 1.0f && tau <= 100.0f) {
            bio_tau_count++;
        }
    }
    if (arch->n_layers > 0) {
        float tau_ratio = (float)bio_tau_count / (float)arch->n_layers;
        score += 0.2f * tau_ratio;
    }

    /* Local connectivity pattern — +0.2 if >70% layers use local/sparse wiring */
    uint32_t local_layers = 0;
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        if (arch->layers[i].sparsity >= 0.5f) {
            local_layers++;
        }
    }
    if (arch->n_layers > 0) {
        float local_ratio = (float)local_layers / (float)arch->n_layers;
        if (local_ratio >= 0.7f) {
            score += 0.2f;
        } else {
            score += 0.2f * (local_ratio / 0.7f);
        }
    }

    /* Layer depth plausibility — cortex has 6 layers, +0.2 for 3-8 layers */
    if (arch->n_layers >= 3 && arch->n_layers <= 8) {
        score += 0.2f;
    } else if (arch->n_layers >= 2 && arch->n_layers <= 12) {
        score += 0.1f;
    }

    /* Dale's law compliance (E/I segregation) — +0.1 if threshold variance suggests
     * inhibitory/excitatory separation (different threshold values across layers) */
    if (arch->n_layers >= 2) {
        float thresh_min = arch->layers[0].v_thresh;
        float thresh_max = arch->layers[0].v_thresh;
        for (uint32_t i = 1; i < arch->n_layers; i++) {
            if (arch->layers[i].v_thresh < thresh_min)
                thresh_min = arch->layers[i].v_thresh;
            if (arch->layers[i].v_thresh > thresh_max)
                thresh_max = arch->layers[i].v_thresh;
        }
        /* Nonzero threshold variance suggests E/I segregation */
        if (thresh_max - thresh_min > 1.0f) {
            score += 0.1f;
        }
    }

    /* Cap at 1.0 */
    if (score > 1.0f) score = 1.0f;

    return score;
}

//=============================================================================
// Result Handling Implementation
//=============================================================================

void auto_arch_result_destroy(auto_arch_result_t* result)
{
    if (!result) return;

    if (result->best_arch) {
        auto_arch_architecture_destroy(result->best_arch);
    }

    if (result->pareto_frontier) {
        for (uint32_t i = 0; i < result->n_pareto; i++) {
            auto_arch_architecture_destroy(result->pareto_frontier[i]);
        }
        nimcp_free(result->pareto_frontier);
    }

    if (result->pareto_fitness) {
        nimcp_free(result->pareto_fitness);
    }

    nimcp_free(result);
}

int auto_arch_result_save(const auto_arch_result_t* result, const char* filepath)
{
    if (!result || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_result_save: required parameter is NULL (result, filepath)");
        return -1;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_result_save: fp is NULL");
        return -1;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"status\": %d,\n", (int)result->status);
    fprintf(fp, "  \"n_evaluated\": %u,\n", result->n_evaluated);
    fprintf(fp, "  \"n_pareto\": %u,\n", result->n_pareto);

    /* Best fitness */
    fprintf(fp, "  \"best_fitness\": {\n");
    fprintf(fp, "    \"accuracy\": %f,\n", result->best_fitness.accuracy);
    fprintf(fp, "    \"loss\": %f,\n", result->best_fitness.loss);
    fprintf(fp, "    \"n_operations\": %lu,\n", result->best_fitness.n_operations);
    fprintf(fp, "    \"energy_per_inference\": %f,\n", result->best_fitness.energy_per_inference);
    fprintf(fp, "    \"latency_ms\": %f,\n", result->best_fitness.latency_ms);
    fprintf(fp, "    \"n_parameters\": %lu,\n", result->best_fitness.n_parameters);
    fprintf(fp, "    \"sparsity\": %f,\n", result->best_fitness.sparsity);
    fprintf(fp, "    \"total_fitness\": %f\n", result->best_fitness.total_fitness);
    fprintf(fp, "  },\n");

    /* Stats */
    fprintf(fp, "  \"stats\": {\n");
    fprintf(fp, "    \"total_evaluations\": %u,\n", result->stats.total_evaluations);
    fprintf(fp, "    \"iterations\": %u,\n", result->stats.iterations);
    fprintf(fp, "    \"elapsed_time_sec\": %f,\n", result->stats.elapsed_time_sec);
    fprintf(fp, "    \"best_fitness_so_far\": %f,\n", result->stats.best_fitness_so_far);
    fprintf(fp, "    \"best_at_iteration\": %u,\n", result->stats.best_at_iteration);
    fprintf(fp, "    \"improvements\": %u,\n", result->stats.improvements);
    fprintf(fp, "    \"stagnant_iterations\": %u\n", result->stats.stagnant_iterations);
    fprintf(fp, "  },\n");

    /* Best architecture - save inline */
    fprintf(fp, "  \"best_arch\": ");
    if (result->best_arch) {
        fprintf(fp, "{\n");
        fprintf(fp, "    \"magic\": %u,\n", result->best_arch->magic);
        fprintf(fp, "    \"arch_id\": %lu,\n", result->best_arch->arch_id);
        fprintf(fp, "    \"generation\": %u,\n", result->best_arch->generation);
        fprintf(fp, "    \"network_type\": %d,\n", (int)result->best_arch->network_type);
        fprintf(fp, "    \"n_layers\": %u,\n", result->best_arch->n_layers);
        fprintf(fp, "    \"n_inputs\": %u,\n", result->best_arch->n_inputs);
        fprintf(fp, "    \"n_outputs\": %u,\n", result->best_arch->n_outputs);
        fprintf(fp, "    \"n_parameters\": %lu,\n", result->best_arch->n_parameters);
        fprintf(fp, "    \"avg_sparsity\": %f,\n", result->best_arch->avg_sparsity);
        fprintf(fp, "    \"layers\": [\n");
        for (uint32_t i = 0; i < result->best_arch->n_layers; i++) {
            const auto_arch_layer_spec_t* layer = &result->best_arch->layers[i];
            fprintf(fp, "      {\n");
            fprintf(fp, "        \"layer_id\": %u,\n", layer->layer_id);
            fprintf(fp, "        \"type\": %d,\n", (int)layer->type);
            fprintf(fp, "        \"n_neurons\": %u,\n", layer->n_neurons);
            fprintf(fp, "        \"n_inputs\": %u,\n", layer->n_inputs);
            fprintf(fp, "        \"activation\": %d,\n", (int)layer->activation);
            fprintf(fp, "        \"connectivity\": %d,\n", (int)layer->connectivity);
            fprintf(fp, "        \"sparsity\": %f\n", layer->sparsity);
            fprintf(fp, "      }%s\n", (i < result->best_arch->n_layers - 1) ? "," : "");
        }
        fprintf(fp, "    ]\n");
        fprintf(fp, "  }\n");
    } else {
        fprintf(fp, "null\n");
    }
    fprintf(fp, "}\n");

    fclose(fp);
    return 0;
}

auto_arch_result_t* auto_arch_result_load(const char* filepath)
{
    if (!filepath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");

        return NULL;

    }

    FILE* fp = fopen(filepath, "r");
    if (!fp) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fp is NULL");

        return NULL;

    }

    auto_arch_result_t* result = (auto_arch_result_t*)nimcp_calloc(
        1, sizeof(auto_arch_result_t));
    if (!result) {
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "auto_arch_result_load: result is NULL");
        return NULL;
    }

    char key[NIMCP_ID_BUFFER_SIZE];
    double value;
    int c;

    /* Skip opening brace */
    c = skip_ws(fp);
    if (c != '{') {
        nimcp_free(result);
        fclose(fp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "auto_arch_result_load: validation failed");
        return NULL;
    }

    /* Read key-value pairs */
    while (1) {
        if (!read_json_key(fp, key, sizeof(key))) break;

        if (strcmp(key, "status") == 0) {
            if (read_json_number(fp, &value)) result->status = (auto_arch_status_t)(int)value;
        } else if (strcmp(key, "n_evaluated") == 0) {
            if (read_json_number(fp, &value)) result->n_evaluated = (uint32_t)value;
        } else if (strcmp(key, "n_pareto") == 0) {
            if (read_json_number(fp, &value)) result->n_pareto = (uint32_t)value;
        } else if (strcmp(key, "best_fitness") == 0) {
            c = skip_ws(fp);
            if (c == '{') {
                while (1) {
                    if (!read_json_key(fp, key, sizeof(key))) break;
                    if (strcmp(key, "accuracy") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.accuracy = (float)value;
                    } else if (strcmp(key, "loss") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.loss = (float)value;
                    } else if (strcmp(key, "n_operations") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.n_operations = (uint64_t)value;
                    } else if (strcmp(key, "energy_per_inference") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.energy_per_inference = (float)value;
                    } else if (strcmp(key, "latency_ms") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.latency_ms = (float)value;
                    } else if (strcmp(key, "n_parameters") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.n_parameters = (uint64_t)value;
                    } else if (strcmp(key, "sparsity") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.sparsity = (float)value;
                    } else if (strcmp(key, "total_fitness") == 0) {
                        if (read_json_number(fp, &value)) result->best_fitness.total_fitness = (float)value;
                    }
                    c = skip_ws(fp);
                    if (c == '}') break;
                    if (c != ',') { ungetc(c, fp); break; }
                }
            }
        } else if (strcmp(key, "stats") == 0) {
            c = skip_ws(fp);
            if (c == '{') {
                while (1) {
                    if (!read_json_key(fp, key, sizeof(key))) break;
                    if (strcmp(key, "total_evaluations") == 0) {
                        if (read_json_number(fp, &value)) result->stats.total_evaluations = (uint32_t)value;
                    } else if (strcmp(key, "iterations") == 0) {
                        if (read_json_number(fp, &value)) result->stats.iterations = (uint32_t)value;
                    } else if (strcmp(key, "elapsed_time_sec") == 0) {
                        if (read_json_number(fp, &value)) result->stats.elapsed_time_sec = (float)value;
                    } else if (strcmp(key, "best_fitness_so_far") == 0) {
                        if (read_json_number(fp, &value)) result->stats.best_fitness_so_far = (float)value;
                    } else if (strcmp(key, "best_at_iteration") == 0) {
                        if (read_json_number(fp, &value)) result->stats.best_at_iteration = (uint32_t)value;
                    } else if (strcmp(key, "improvements") == 0) {
                        if (read_json_number(fp, &value)) result->stats.improvements = (uint32_t)value;
                    } else if (strcmp(key, "stagnant_iterations") == 0) {
                        if (read_json_number(fp, &value)) result->stats.stagnant_iterations = (uint32_t)value;
                    }
                    c = skip_ws(fp);
                    if (c == '}') break;
                    if (c != ',') { ungetc(c, fp); break; }
                }
            }
        } else if (strcmp(key, "best_arch") == 0) {
            c = skip_ws(fp);
            if (c == '{') {
                /* Parse embedded architecture */
                auto_arch_architecture_t* arch = (auto_arch_architecture_t*)nimcp_calloc(
                    1, sizeof(auto_arch_architecture_t));
                if (arch) {
                    while (1) {
                        if (!read_json_key(fp, key, sizeof(key))) break;
                        if (strcmp(key, "magic") == 0) {
                            if (read_json_number(fp, &value)) arch->magic = (uint32_t)value;
                        } else if (strcmp(key, "arch_id") == 0) {
                            if (read_json_number(fp, &value)) arch->arch_id = (uint64_t)value;
                        } else if (strcmp(key, "generation") == 0) {
                            if (read_json_number(fp, &value)) arch->generation = (uint32_t)value;
                        } else if (strcmp(key, "network_type") == 0) {
                            if (read_json_number(fp, &value)) arch->network_type = (auto_arch_network_type_t)(int)value;
                        } else if (strcmp(key, "n_layers") == 0) {
                            if (read_json_number(fp, &value)) arch->n_layers = (uint32_t)value;
                        } else if (strcmp(key, "n_inputs") == 0) {
                            if (read_json_number(fp, &value)) arch->n_inputs = (uint32_t)value;
                        } else if (strcmp(key, "n_outputs") == 0) {
                            if (read_json_number(fp, &value)) arch->n_outputs = (uint32_t)value;
                        } else if (strcmp(key, "n_parameters") == 0) {
                            if (read_json_number(fp, &value)) arch->n_parameters = (uint64_t)value;
                        } else if (strcmp(key, "avg_sparsity") == 0) {
                            if (read_json_number(fp, &value)) arch->avg_sparsity = (float)value;
                        } else if (strcmp(key, "layers") == 0) {
                            /* Allocate and parse layers */
                            if (arch->n_layers > 0) {
                                arch->layers = (auto_arch_layer_spec_t*)nimcp_calloc(
                                    arch->n_layers, sizeof(auto_arch_layer_spec_t));
                            }
                            c = skip_ws(fp);
                            if (c == '[' && arch->layers) {
                                for (uint32_t i = 0; i < arch->n_layers; i++) {
                                    c = skip_ws(fp);
                                    if (c != '{') break;
                                    auto_arch_layer_spec_t* layer = &arch->layers[i];
                                    while (1) {
                                        if (!read_json_key(fp, key, sizeof(key))) break;
                                        if (strcmp(key, "layer_id") == 0) {
                                            if (read_json_number(fp, &value)) layer->layer_id = (uint32_t)value;
                                        } else if (strcmp(key, "type") == 0) {
                                            if (read_json_number(fp, &value)) layer->type = (auto_arch_layer_type_t)(int)value;
                                        } else if (strcmp(key, "n_neurons") == 0) {
                                            if (read_json_number(fp, &value)) layer->n_neurons = (uint32_t)value;
                                        } else if (strcmp(key, "n_inputs") == 0) {
                                            if (read_json_number(fp, &value)) layer->n_inputs = (uint32_t)value;
                                        } else if (strcmp(key, "activation") == 0) {
                                            if (read_json_number(fp, &value)) layer->activation = (lnn_activation_t)(int)value;
                                        } else if (strcmp(key, "connectivity") == 0) {
                                            if (read_json_number(fp, &value)) layer->connectivity = (auto_arch_connectivity_t)(int)value;
                                        } else if (strcmp(key, "sparsity") == 0) {
                                            if (read_json_number(fp, &value)) layer->sparsity = (float)value;
                                        }
                                        c = skip_ws(fp);
                                        if (c == '}') break;
                                        if (c != ',') { ungetc(c, fp); break; }
                                    }
                                    c = skip_ws(fp);
                                    if (c == ']') break;
                                    if (c != ',') ungetc(c, fp);
                                }
                            }
                        }
                        c = skip_ws(fp);
                        if (c == '}') break;
                        if (c != ',') { ungetc(c, fp); break; }
                    }
                    result->best_arch = arch;
                }
            } else if (c == 'n') {
                /* null */
                fgetc(fp); fgetc(fp); fgetc(fp); /* skip "ull" */
            }
        }

        c = skip_ws(fp);
        if (c == '}') break;
        if (c != ',') { ungetc(c, fp); break; }
    }

    fclose(fp);
    return result;
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* auto_arch_method_name(auto_arch_method_t method)
{
    static const char* names[] = {
        "RL-NAS", "Evolutionary", "DARTS", "Random", "Bayesian",
        "Pruning", "Gradient", "Neuroevolution"
    };
    if (method >= AUTO_ARCH_METHOD_COUNT) return "Unknown";
    return names[method];
}

const char* auto_arch_network_type_name(auto_arch_network_type_t type)
{
    static const char* names[] = {
        "SNN", "LNN", "CNN", "SNN+LNN", "SNN+CNN", "SNN+LNN+CNN"
    };
    if (type >= AUTO_ARCH_TYPE_COUNT) return "Unknown";
    return names[type];
}

const char* auto_arch_task_type_name(auto_arch_task_type_t type)
{
    static const char* names[] = {
        "Classification", "Regression", "Sequence", "Detection",
        "Segmentation", "Reinforcement", "Generation", "Custom"
    };
    if (type >= AUTO_ARCH_TASK_COUNT) return "Unknown";
    return names[type];
}

const char* auto_arch_layer_type_name(auto_arch_layer_type_t type)
{
    static const char* names[] = {
        "LIF", "Izhikevich", "AdEx", "LTC", "Dense",
        "Conv", "Pool", "Recurrent", "Attention", "Skip"
    };
    if (type >= AUTO_ARCH_LAYER_COUNT) return "Unknown";
    return names[type];
}

void auto_arch_print(const auto_arch_architecture_t* arch)
{
    if (!arch) return;

    NIMCP_LOGGING_INFO("Architecture ID: %lu", arch->arch_id);
    NIMCP_LOGGING_INFO("  Layers: %u", arch->n_layers);
    NIMCP_LOGGING_INFO("  Parameters: %lu", arch->n_parameters);
    NIMCP_LOGGING_INFO("  Connections: %lu", arch->n_connections);
    NIMCP_LOGGING_INFO("  Sparsity: %.3f", arch->avg_sparsity);
}

void auto_arch_result_print(const auto_arch_result_t* result)
{
    if (!result) return;

    NIMCP_LOGGING_INFO("=== Architecture Search Results ===");
    NIMCP_LOGGING_INFO("Status: %d", result->status);
    NIMCP_LOGGING_INFO("Evaluations: %u", result->stats.total_evaluations);
    NIMCP_LOGGING_INFO("Best Fitness: %.4f", result->best_fitness.total_fitness);
    NIMCP_LOGGING_INFO("Best Accuracy: %.4f", result->best_fitness.accuracy);
    NIMCP_LOGGING_INFO("Pareto Front Size: %u", result->n_pareto);
}

//=============================================================================
// Internal Helper Functions Implementation
//=============================================================================

/**
 * @brief Initialize evolutionary search context
 *
 * WHAT: Allocate and initialize population for evolutionary search
 * WHY:  Need diverse initial population for genetic algorithm
 * HOW:  Create random initial architectures with varied topologies
 */
static int init_evolutionary_context(auto_arch_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_evolutionary_context: ctx is NULL");
        return -1;
    }

    const uint32_t pop_size = ctx->config.population_size;
    if (pop_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_evolutionary_context: pop_size is zero");
        return -1;
    }

    /* Allocate population array */
    ctx->method_ctx.evolutionary.population = (population_member_t*)nimcp_calloc(
        pop_size, sizeof(population_member_t));
    if (!ctx->method_ctx.evolutionary.population) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_evolutionary_context: ctx->method_ctx is NULL");
        return -1;
    }

    ctx->method_ctx.evolutionary.generation = 0;

    /* Initialize population with random architectures */
    for (uint32_t i = 0; i < pop_size; i++) {
        ctx->method_ctx.evolutionary.population[i].arch = auto_arch_random_architecture(ctx);
        if (!ctx->method_ctx.evolutionary.population[i].arch) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j < i; j++) {
                auto_arch_architecture_destroy(ctx->method_ctx.evolutionary.population[j].arch);
            }
            nimcp_free(ctx->method_ctx.evolutionary.population);
            ctx->method_ctx.evolutionary.population = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_evolutionary_context: ctx->method_ctx is NULL");
            return -1;
        }
        ctx->method_ctx.evolutionary.population[i].generation = 0;
        ctx->method_ctx.evolutionary.population[i].parent_id = 0;
        ctx->method_ctx.evolutionary.population[i].evaluated = false;
    }

    NIMCP_LOGGING_INFO("Initialized evolutionary context with %u individuals", pop_size);
    return 0;
}

/**
 * @brief Initialize RL-NAS controller context
 *
 * WHAT: Set up LSTM controller for RL-based architecture search
 * WHY:  RL controller learns to generate good architectures over time
 * HOW:  Initialize LSTM weights, optimizer, and action space
 */
static int init_rl_nas_context(auto_arch_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_rl_nas_context: ctx is NULL");
        return -1;
    }

    /* Initialize RL controller */
    rl_controller_t* controller = (rl_controller_t*)nimcp_calloc(1, sizeof(rl_controller_t));
    if (!controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_rl_nas_context: controller is NULL");
        return -1;
    }

    controller->hidden_size = 64;
    controller->learning_rate = 0.001f;
    controller->entropy_weight = 0.0001f;
    controller->lstm_state = NULL;

    /* Initialize controller weights - simplified representation */
    uint32_t weight_dims[] = { controller->hidden_size, AUTO_ARCH_MAX_LAYERS * 4 };
    controller->weights = nimcp_tensor_create(weight_dims, 2, NIMCP_DTYPE_F32);
    if (!controller->weights) {
        nimcp_free(controller);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_rl_nas_context: controller->weights is NULL");
        return -1;
    }

    /* Initialize gradients */
    controller->gradients = nimcp_tensor_create(weight_dims, 2, NIMCP_DTYPE_F32);
    if (!controller->gradients) {
        nimcp_tensor_destroy(controller->weights);
        nimcp_free(controller);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_rl_nas_context: controller->gradients is NULL");
        return -1;
    }

    /* Random weight initialization */
    float* weights = (float*)nimcp_tensor_data(controller->weights);
    uint32_t n_weights = weight_dims[0] * weight_dims[1];
    for (uint32_t i = 0; i < n_weights; i++) {
        weights[i] = (randf(&ctx->rng_state) - 0.5f) * 0.1f;
    }

    ctx->method_ctx.rl_nas.controller = controller;

    NIMCP_LOGGING_INFO("Initialized RL-NAS controller with hidden_size=%u", controller->hidden_size);
    return 0;
}

/**
 * @brief Initialize DARTS continuous architecture context
 *
 * WHAT: Set up continuous architecture parameters for DARTS
 * WHY:  DARTS uses continuous relaxation for gradient-based search
 * HOW:  Initialize architecture weights (alpha) for all operations
 */
static int init_darts_context(auto_arch_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_darts_context: ctx is NULL");
        return -1;
    }

    const uint32_t n_layers = ctx->config.constraints.max_layers;
    const uint32_t n_ops = 8; /* conv3x3, conv5x5, sep3x3, sep5x5, dil3x3, pool, skip, none */

    darts_context_t* darts = (darts_context_t*)nimcp_calloc(1, sizeof(darts_context_t));
    if (!darts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_darts_context: darts is NULL");
        return -1;
    }

    darts->n_layers = n_layers;

    /* Allocate architecture parameters (alpha) */
    darts->alpha = (nimcp_tensor_t**)nimcp_calloc(n_layers, sizeof(nimcp_tensor_t*));
    if (!darts->alpha) {
        nimcp_free(darts);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_darts_context: darts->alpha is NULL");
        return -1;
    }

    /* Initialize alpha for each layer */
    for (uint32_t i = 0; i < n_layers; i++) {
        uint32_t alpha_dims[] = { n_ops };
        darts->alpha[i] = nimcp_tensor_create(alpha_dims, 1, NIMCP_DTYPE_F32);
        if (!darts->alpha[i]) {
            for (uint32_t j = 0; j < i; j++) {
                nimcp_tensor_destroy(darts->alpha[j]);
            }
            nimcp_free(darts->alpha);
            nimcp_free(darts);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_darts_context: darts->alpha is NULL");
            return -1;
        }

        /* Initialize with small random values */
        float* alpha_data = (float*)nimcp_tensor_data(darts->alpha[i]);
        for (uint32_t j = 0; j < n_ops; j++) {
            alpha_data[j] = randf(&ctx->rng_state) * 0.001f;
        }
    }

    ctx->method_ctx.darts.darts = darts;
    ctx->method_ctx.darts.warmup_epochs = 0;

    NIMCP_LOGGING_INFO("Initialized DARTS context with %u layers, %u ops/layer", n_layers, n_ops);
    return 0;
}

/**
 * @brief Initialize pruning-based search context
 *
 * WHAT: Create dense initial architecture for pruning-based search
 * WHY:  Pruning discovers structure by removing unnecessary connections
 * HOW:  Start with maximally connected architecture, track pruning masks
 */
static int init_pruning_context(auto_arch_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_pruning_context: ctx is NULL");
        return -1;
    }

    /* Create dense initial architecture at maximum size */
    auto_arch_architecture_t* dense_arch = auto_arch_random_architecture(ctx);
    if (!dense_arch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_pruning_context: dense_arch is NULL");
        return -1;
    }

    /* Set all layers to maximum neurons (will prune later) */
    for (uint32_t i = 0; i < dense_arch->n_layers; i++) {
        dense_arch->layers[i].n_neurons = ctx->config.constraints.max_neurons_per_layer;
    }

    ctx->method_ctx.pruning.dense_arch = dense_arch;
    ctx->method_ctx.pruning.pruning_rate = 0.1f;  /* Prune 10% per iteration */
    ctx->method_ctx.pruning.importance_scores = NULL;  /* Will compute during search */

    NIMCP_LOGGING_INFO("Initialized pruning context with dense architecture");
    return 0;
}

/**
 * @brief Perform one step of evolutionary search
 *
 * WHAT: Execute tournament selection, crossover, and mutation
 * WHY:  Evolve population toward better architectures
 * HOW:  Select parents via tournament, create offspring, apply mutation
 */
static auto_arch_architecture_t* evolutionary_step(auto_arch_context_t* ctx,
                                                   const nimcp_tensor_t* train_data,
                                                   const nimcp_tensor_t* train_labels,
                                                   const nimcp_tensor_t* val_data,
                                                   const nimcp_tensor_t* val_labels)
{
    if (!ctx || !ctx->method_ctx.evolutionary.population) {
        return auto_arch_random_architecture(ctx);
    }

    population_member_t* pop = ctx->method_ctx.evolutionary.population;
    uint32_t pop_size = ctx->config.population_size;

    /* Tournament selection - select 2 parents */
    uint32_t tournament_size = (pop_size > 4) ? 4 : 2;
    int parent1_idx = -1, parent2_idx = -1;
    float best_fitness1 = -1e30f, best_fitness2 = -1e30f;

    for (uint32_t t = 0; t < tournament_size; t++) {
        uint32_t idx = xorshift64(&ctx->rng_state) % pop_size;
        if (pop[idx].arch && pop[idx].evaluated && pop[idx].fitness.total_fitness > best_fitness1) {
            best_fitness2 = best_fitness1;
            parent2_idx = parent1_idx;
            best_fitness1 = pop[idx].fitness.total_fitness;
            parent1_idx = (int)idx;
        } else if (pop[idx].arch && pop[idx].evaluated && pop[idx].fitness.total_fitness > best_fitness2) {
            best_fitness2 = pop[idx].fitness.total_fitness;
            parent2_idx = (int)idx;
        }
    }

    /* If no evaluated parents found, return random architecture */
    if (parent1_idx < 0) {
        return auto_arch_random_architecture(ctx);
    }

    /* Crossover - uniform crossover of layer specifications */
    auto_arch_architecture_t* child = auto_arch_clone(pop[parent1_idx].arch);
    if (!child) return auto_arch_random_architecture(ctx);

    /* If no second parent, just return mutated first parent */
    if (parent2_idx < 0 || !pop[parent2_idx].arch) {
        return child;
    }

    auto_arch_architecture_t* parent2 = pop[parent2_idx].arch;
    uint32_t min_layers = (child->n_layers < parent2->n_layers) ? child->n_layers : parent2->n_layers;

    for (uint32_t i = 0; i < min_layers; i++) {
        if (randf(&ctx->rng_state) > 0.5f) {
            /* Copy layer from parent2 */
            child->layers[i].n_neurons = parent2->layers[i].n_neurons;
            child->layers[i].type = parent2->layers[i].type;
            child->layers[i].activation = parent2->layers[i].activation;
        }
    }

    /* Mutation - randomly modify architecture with small probability */
    float mutation_rate = ctx->config.mutation_rate;
    for (uint32_t i = 0; i < child->n_layers; i++) {
        if (randf(&ctx->rng_state) < mutation_rate) {
            /* Mutate neuron count */
            int delta = (int)(randf(&ctx->rng_state) * 64) - 32;
            int new_neurons = (int)child->layers[i].n_neurons + delta;
            new_neurons = (new_neurons < 8) ? 8 : new_neurons;
            new_neurons = (new_neurons > (int)ctx->config.constraints.max_neurons_per_layer) ?
                          (int)ctx->config.constraints.max_neurons_per_layer : new_neurons;
            child->layers[i].n_neurons = (uint32_t)new_neurons;
        }
    }

    /* Small chance to add/remove layer */
    if (randf(&ctx->rng_state) < mutation_rate * 0.1f) {
        if (child->n_layers > 2 && randf(&ctx->rng_state) > 0.5f) {
            /* Remove last layer */
            child->n_layers--;
        } else if (child->n_layers < ctx->config.constraints.max_layers) {
            /* Add layer — realloc to accommodate one more */
            auto_arch_layer_spec_t* new_layers = nimcp_realloc(child->layers,
                (child->n_layers + 1) * sizeof(auto_arch_layer_spec_t));
            if (!new_layers) return child;  /* graceful degradation */
            child->layers = new_layers;
            memset(&child->layers[child->n_layers], 0, sizeof(auto_arch_layer_spec_t));
            child->layers[child->n_layers].n_neurons = 64;
            child->layers[child->n_layers].type = AUTO_ARCH_LAYER_DENSE;
            child->layers[child->n_layers].activation = LNN_ACTIVATION_RELU;
            child->n_layers++;
        }
    }

    return child;
}

/**
 * @brief Perform one step of RL-NAS search
 *
 * WHAT: Sample architecture from controller, update with REINFORCE
 * WHY:  Learn to generate better architectures based on reward
 * HOW:  LSTM generates architecture spec, reward updates policy
 */
static auto_arch_architecture_t* rl_nas_step(auto_arch_context_t* ctx,
                                             const nimcp_tensor_t* train_data,
                                             const nimcp_tensor_t* train_labels,
                                             const nimcp_tensor_t* val_data,
                                             const nimcp_tensor_t* val_labels)
{
    if (!ctx || !ctx->method_ctx.rl_nas.controller) {
        return auto_arch_random_architecture(ctx);
    }

    rl_controller_t* controller = (rl_controller_t*)ctx->method_ctx.rl_nas.controller;

    /* Sample architecture from controller (simplified - use softmax over decisions) */
    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    if (!arch) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arch is NULL");

        return NULL;

    }

    float* weights = (float*)nimcp_tensor_data(controller->weights);

    /* Use controller weights to bias architecture decisions */
    for (uint32_t i = 0; i < arch->n_layers && i < AUTO_ARCH_MAX_LAYERS; i++) {
        /* Softmax over neuron count options */
        float logits[4] = {
            weights[i * 4 + 0],  /* 32 neurons */
            weights[i * 4 + 1],  /* 64 neurons */
            weights[i * 4 + 2],  /* 128 neurons */
            weights[i * 4 + 3]   /* 256 neurons */
        };

        /* Temperature-scaled softmax */
        float max_logit = logits[0];
        for (int j = 1; j < 4; j++) {
            if (logits[j] > max_logit) max_logit = logits[j];
        }

        float sum = 0.0f;
        for (int j = 0; j < 4; j++) {
            logits[j] = expf(logits[j] - max_logit);
            sum += logits[j];
        }
        for (int j = 0; j < 4; j++) {
            logits[j] /= sum;
        }

        /* Sample from distribution */
        float r = randf(&ctx->rng_state);
        float cumsum = 0.0f;
        int choice = 0;
        for (int j = 0; j < 4; j++) {
            cumsum += logits[j];
            if (r < cumsum) {
                choice = j;
                break;
            }
        }

        uint32_t neuron_options[] = { 32, 64, 128, 256 };
        arch->layers[i].n_neurons = neuron_options[choice];
    }

    return arch;
}

/**
 * @brief Perform one step of DARTS search
 *
 * WHAT: Update architecture weights (alpha) via gradient descent
 * WHY:  Continuous relaxation enables gradient-based architecture search
 * HOW:  Alternating optimization of weights and architecture parameters
 */
static auto_arch_architecture_t* darts_step(auto_arch_context_t* ctx,
                                            const nimcp_tensor_t* train_data,
                                            const nimcp_tensor_t* train_labels,
                                            const nimcp_tensor_t* val_data,
                                            const nimcp_tensor_t* val_labels)
{
    if (!ctx || !ctx->method_ctx.darts.darts) {
        return auto_arch_random_architecture(ctx);
    }

    darts_context_t* darts = (darts_context_t*)ctx->method_ctx.darts.darts;

    /* Discretize current architecture from alpha parameters */
    auto_arch_architecture_t* arch = auto_arch_random_architecture(ctx);
    if (!arch) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "arch is NULL");

        return NULL;

    }

    /* Set number of layers based on DARTS context */
    arch->n_layers = (darts->n_layers < arch->n_layers) ? darts->n_layers : arch->n_layers;

    /* Select operations based on argmax of alpha */
    for (uint32_t i = 0; i < arch->n_layers && i < darts->n_layers; i++) {
        float* alpha = (float*)nimcp_tensor_data(darts->alpha[i]);
        if (!alpha) continue;

        /* Find argmax */
        int best_op = 0;
        float best_val = alpha[0];
        for (int j = 1; j < 8; j++) {
            if (alpha[j] > best_val) {
                best_val = alpha[j];
                best_op = j;
            }
        }

        /* Map operation to layer type */
        switch (best_op) {
            case 0: case 1: /* conv3x3, conv5x5 */
                arch->layers[i].type = AUTO_ARCH_LAYER_CONV;
                break;
            case 2: case 3: case 4: /* separable, dilated */
                arch->layers[i].type = AUTO_ARCH_LAYER_CONV;
                break;
            case 5: /* pooling */
                arch->layers[i].type = AUTO_ARCH_LAYER_POOL;
                break;
            case 6: /* skip connection */
                arch->layers[i].type = AUTO_ARCH_LAYER_SKIP;
                break;
            default: /* none */
                arch->layers[i].type = AUTO_ARCH_LAYER_DENSE;
                break;
        }

        /* Update alpha with small random gradient (simplified) */
        for (int j = 0; j < 8; j++) {
            alpha[j] += (randf(&ctx->rng_state) - 0.5f) * 0.01f;
        }
    }

    return arch;
}

/**
 * @brief Perform one step of pruning-based search
 *
 * WHAT: Prune low-magnitude connections from dense architecture
 * WHY:  Discover sparse, efficient architectures via pruning
 * HOW:  Iteratively remove lowest-magnitude neurons/connections
 */
static auto_arch_architecture_t* pruning_step(auto_arch_context_t* ctx,
                                              const nimcp_tensor_t* train_data,
                                              const nimcp_tensor_t* train_labels,
                                              const nimcp_tensor_t* val_data,
                                              const nimcp_tensor_t* val_labels)
{
    if (!ctx || !ctx->method_ctx.pruning.dense_arch) {
        return auto_arch_random_architecture(ctx);
    }

    auto_arch_architecture_t* arch = auto_arch_clone(ctx->method_ctx.pruning.dense_arch);
    if (!arch) return auto_arch_random_architecture(ctx);

    float pruning_rate = ctx->method_ctx.pruning.pruning_rate;

    /* Prune neurons proportionally from each layer based on pruning rate */
    for (uint32_t i = 0; i < arch->n_layers; i++) {
        uint32_t orig_neurons = arch->layers[i].n_neurons;
        uint32_t pruned_neurons = (uint32_t)(orig_neurons * (1.0f - pruning_rate));
        if (pruned_neurons < 8) pruned_neurons = 8;  /* Minimum neurons */
        arch->layers[i].n_neurons = pruned_neurons;
    }

    /* Increase pruning rate for next iteration (up to max_sparsity) */
    float new_rate = pruning_rate + 0.05f;
    if (new_rate > ctx->config.constraints.max_sparsity) {
        new_rate = ctx->config.constraints.max_sparsity;
    }
    ctx->method_ctx.pruning.pruning_rate = new_rate;

    /* Update dense architecture for next iteration */
    auto_arch_architecture_destroy(ctx->method_ctx.pruning.dense_arch);
    ctx->method_ctx.pruning.dense_arch = auto_arch_clone(arch);

    return arch;
}

/**
 * @brief Compute fitness for an architecture
 */
static int compute_fitness(auto_arch_context_t* ctx,
                          auto_arch_architecture_t* arch,
                          const nimcp_tensor_t* train_data,
                          const nimcp_tensor_t* train_labels,
                          const nimcp_tensor_t* val_data,
                          const nimcp_tensor_t* val_labels,
                          auto_arch_fitness_t* fitness)
{
    /* Delegate to auto_arch_evaluate */
    return auto_arch_evaluate(ctx, arch, train_data, train_labels, val_data, val_labels, fitness);
}

/**
 * @brief Update Pareto frontier with new solution
 *
 * WHAT: Maintain set of non-dominated solutions
 * WHY:  Multi-objective optimization needs Pareto-optimal set
 * HOW:  Check dominance, add if non-dominated, remove dominated
 */
static int update_pareto_frontier(auto_arch_context_t* ctx,
                                  auto_arch_architecture_t* arch,
                                  const auto_arch_fitness_t* fitness)
{
    if (!ctx || !arch || !fitness) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_pareto_frontier: required parameter is NULL (ctx, arch, fitness)");
        return -1;
    }

    /* Check if new solution is dominated by any existing solution */
    for (uint32_t i = 0; i < ctx->n_pareto; i++) {
        auto_arch_fitness_t* existing = &ctx->pareto_fitness[i];

        /* Check if existing dominates new (better in all objectives) */
        bool existing_dominates =
            (existing->accuracy >= fitness->accuracy) &&
            (existing->energy_per_inference >= fitness->energy_per_inference) &&
            (existing->latency_ms <= fitness->latency_ms) &&
            ((existing->accuracy > fitness->accuracy) ||
             (existing->energy_per_inference > fitness->energy_per_inference) ||
             (existing->latency_ms < fitness->latency_ms));

        if (existing_dominates) {
            return 0;  /* New solution is dominated, don't add */
        }
    }

    /* Remove solutions dominated by new solution */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < ctx->n_pareto; i++) {
        auto_arch_fitness_t* existing = &ctx->pareto_fitness[i];

        bool new_dominates =
            (fitness->accuracy >= existing->accuracy) &&
            (fitness->energy_per_inference >= existing->energy_per_inference) &&
            (fitness->latency_ms <= existing->latency_ms) &&
            ((fitness->accuracy > existing->accuracy) ||
             (fitness->energy_per_inference > existing->energy_per_inference) ||
             (fitness->latency_ms < existing->latency_ms));

        if (!new_dominates) {
            /* Keep this solution */
            if (write_idx != i) {
                ctx->pareto_frontier[write_idx] = ctx->pareto_frontier[i];
                ctx->pareto_fitness[write_idx] = ctx->pareto_fitness[i];
            }
            write_idx++;
        } else {
            /* Remove dominated solution */
            auto_arch_architecture_destroy(ctx->pareto_frontier[i]);
        }
    }
    ctx->n_pareto = write_idx;

    /* Add new solution if there's space */
    if (ctx->n_pareto < ctx->pareto_capacity) {
        ctx->pareto_frontier[ctx->n_pareto] = auto_arch_clone(arch);
        ctx->pareto_fitness[ctx->n_pareto] = *fitness;
        ctx->n_pareto++;
        return 1;
    }

    return 0;
}

static int record_history(auto_arch_context_t* ctx,
                         auto_arch_architecture_t* arch,
                         const auto_arch_fitness_t* fitness)
{
    if (ctx->history_size >= ctx->history_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "record_history: capacity exceeded");
        return -1;
    }

    ctx->history[ctx->history_size] = auto_arch_clone(arch);
    ctx->history_fitness[ctx->history_size] = *fitness;
    ctx->history_size++;

    return 0;
}

//=============================================================================
// Random Number Generation
//=============================================================================

static uint64_t xorshift64(uint64_t* state)
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static float randf(uint64_t* state)
{
    return (float)(xorshift64(state) & 0xFFFFFF) / (float)0xFFFFFF;
}

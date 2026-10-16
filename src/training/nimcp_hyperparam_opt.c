/**
 * @file nimcp_hyperparam_opt.c
 * @brief Implementation of Hyperparameter Optimization (HPO) for NIMCP
 *
 * WHAT: Automatic hyperparameter tuning for training configurations
 * WHY:  Optimal hyperparameters crucial for training success
 * HOW:  Bayesian optimization, random search, population-based training
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_hyperparam_opt.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define LOG_MODULE "HPO"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(hpo)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Trial state
 */
typedef struct {
    uint32_t trial_id;               /**< Trial identifier */
    hpo_params_t params;             /**< Trial parameters */
    hpo_trial_status_t status;       /**< Trial status */
    double objective;                /**< Objective value */
    double start_time;               /**< Start timestamp */
    double end_time;                 /**< End timestamp */
    double* intermediate_values;     /**< Intermediate values */
    uint32_t num_intermediate;       /**< Number of intermediate values */
    uint32_t max_intermediate;       /**< Capacity for intermediate values */
} trial_state_t;

/**
 * @brief TPE sampler state
 */
typedef struct {
    double* good_values;             /**< Values from good trials */
    double* bad_values;              /**< Values from bad trials */
    uint32_t num_good;               /**< Number of good trials */
    uint32_t num_bad;                /**< Number of bad trials */
    double gamma;                    /**< Good/bad split ratio */
} tpe_sampler_t;

/**
 * @brief PBT population member
 */
typedef struct {
    hpo_params_t params;             /**< Member parameters */
    double objective;                /**< Current objective */
    uint32_t generation;             /**< Generation number */
    bool active;                     /**< Whether member is active */
} pbt_member_t;

/**
 * @brief HPO study
 */
struct hpo_study_s {
    char name[NIMCP_NAME_BUFFER_SIZE];                  /**< Study name */
    trial_state_t* trials;           /**< All trials */
    uint32_t num_trials;             /**< Number of trials */
    uint32_t max_trials;             /**< Capacity */
    uint32_t best_trial_id;          /**< Best trial index */
    double best_objective;           /**< Best objective value */
};

/**
 * @brief HPO context
 */
struct hpo_ctx_s {
    hpo_config_t config;             /**< Configuration */
    hpo_search_space_t search_space; /**< Search space */

    /* Study */
    hpo_study_t* study;              /**< Current study */

    /* Algorithm-specific state */
    tpe_sampler_t* tpe_samplers;     /**< TPE samplers per param */
    pbt_member_t* pbt_population;    /**< PBT population */
    uint32_t pbt_population_size;    /**< PBT population size */

    /* Random state */
    uint32_t random_seed;            /**< Random seed */

    /* Integration handles */
    void* dist_ctx;                  /**< Distributed context */
    void* callbacks;                 /**< Training callbacks */

    /* Statistics */
    hpo_stats_t stats;               /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;            /**< Mutex for thread safety */
};

//=============================================================================
// Name Strings
//=============================================================================

static const char* algorithm_names[] = {
    "Random",
    "Grid",
    "Bayesian TPE",
    "Bayesian GP",
    "CMA-ES",
    "Hyperband",
    "BOHB",
    "PBT",
    "Optuna"
};

static const char* param_type_names[] = {
    "Float",
    "Int",
    "Categorical",
    "LogUniform",
    "QUniform",
    "Bool",
    "Conditional"
};

//=============================================================================
// Forward Declarations
//=============================================================================

static double random_uniform(void);
static double random_log_uniform(double low, double high);
static int64_t random_int(int64_t low, int64_t high);
static double sample_tpe(tpe_sampler_t* sampler, double low, double high);
static void update_tpe(tpe_sampler_t* sampler, double value, double objective, bool is_good);
static bool should_prune(hpo_ctx_t* ctx, trial_state_t* trial);
static double get_time_sec(void);

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

int hpo_default_config(hpo_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "hpo_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(hpo_config_t));

    /* Default: TPE algorithm */
    config->algorithm = HPO_ALG_BAYESIAN_TPE;
    config->direction = HPO_MINIMIZE;

    /* Search limits */
    config->n_trials = HPO_DEFAULT_TRIALS;
    config->timeout_hours = 24.0f;
    config->n_parallel = 1;

    /* Bayesian config */
    config->bayesian.n_initial_points = 10;
    config->bayesian.acquisition_weight = 0.5f;
    config->bayesian.acquisition_func = "ei";
    config->bayesian.n_restarts = 5;

    /* Hyperband config */
    config->hyperband.min_resource = 1;
    config->hyperband.max_resource = 81;
    config->hyperband.reduction_factor = 3;
    config->hyperband.early_stopping = true;

    /* PBT config */
    config->pbt.population_size = 10;
    config->pbt.exploit_fraction = 0.2f;
    config->pbt.perturb_factor = 1.2f;
    config->pbt.resample_probability = 25;
    config->pbt.log_mutations = false;

    /* Pruner config */
    config->pruner.strategy = HPO_PRUNE_MEDIAN;
    config->pruner.percentile = 25.0f;
    config->pruner.n_startup_trials = 5;
    config->pruner.n_warmup_steps = 10;
    config->pruner.interval_steps = 1;

    /* Integration */
    config->integrate_distributed = false;
    config->integrate_callbacks = false;

    /* Persistence */
    config->study_name = NULL;
    config->storage_path = NULL;

    /* Debugging */
    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

hpo_ctx_t* hpo_create(
    const hpo_config_t* config,
    const hpo_search_space_t* search_space
) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "hpo_create: config is NULL");
        return NULL;
    }
    if (!search_space) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "hpo_create: search_space is NULL");
        return NULL;
    }

    /* Validate configuration */
    if (hpo_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "hpo_create: config validation failed");
        return NULL;
    }

    hpo_ctx_t* ctx = nimcp_calloc(1, sizeof(hpo_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(hpo_ctx_t),
                          "hpo_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(hpo_config_t));

    /* Copy search space */
    ctx->search_space.num_params = search_space->num_params;
    if (search_space->num_params > 0 && search_space->params) {
        ctx->search_space.params = nimcp_calloc(search_space->num_params, sizeof(hpo_param_def_t));
        if (!ctx->search_space.params) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                              search_space->num_params * sizeof(hpo_param_def_t),
                              "hpo_create: failed to allocate search space params");
            nimcp_free(ctx);
            return NULL;
        }
        memcpy(ctx->search_space.params, search_space->params,
               search_space->num_params * sizeof(hpo_param_def_t));
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "hpo_create: failed to create mutex");
        if (ctx->search_space.params) nimcp_free(ctx->search_space.params);
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize random seed (nimcp_rand auto-seeds on first use) */
    ctx->random_seed = (uint32_t)time(NULL);

    /* Initialize TPE samplers for Bayesian methods */
    if (config->algorithm == HPO_ALG_BAYESIAN_TPE ||
        config->algorithm == HPO_ALG_BOHB) {
        ctx->tpe_samplers = nimcp_calloc(search_space->num_params, sizeof(tpe_sampler_t));
        if (ctx->tpe_samplers) {
            for (uint32_t i = 0; i < search_space->num_params; i++) {
                ctx->tpe_samplers[i].gamma = 0.25f;  /* Top 25% are "good" */
            }
        }
    }

    /* Initialize PBT population */
    if (config->algorithm == HPO_ALG_PBT) {
        ctx->pbt_population_size = config->pbt.population_size;
        ctx->pbt_population = nimcp_calloc(ctx->pbt_population_size, sizeof(pbt_member_t));
    }

    /* Create default study */
    ctx->study = nimcp_calloc(1, sizeof(hpo_study_t));
    if (ctx->study) {
        ctx->study->max_trials = HPO_MAX_TRIALS;
        ctx->study->trials = nimcp_calloc(ctx->study->max_trials, sizeof(trial_state_t));
        ctx->study->best_objective = config->direction == HPO_MINIMIZE ? DBL_MAX : -DBL_MAX;
    }

    /* Reset statistics */
    memset(&ctx->stats, 0, sizeof(hpo_stats_t));
    ctx->stats.best_objective = config->direction == HPO_MINIMIZE ? DBL_MAX : -DBL_MAX;

    if (config->verbose) {
        printf("[HPO] Created context: alg=%s, n_trials=%u, n_params=%u\n",
               hpo_algorithm_name(config->algorithm),
               config->n_trials, search_space->num_params);
    }

    return ctx;
}

void hpo_destroy(hpo_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Clean up study */
    if (ctx->study) {
        if (ctx->study->trials) {
            for (uint32_t i = 0; i < ctx->study->num_trials; i++) {
                hpo_free_params(&ctx->study->trials[i].params);
                if (ctx->study->trials[i].intermediate_values) {
                    nimcp_free(ctx->study->trials[i].intermediate_values);
                }
            }
            nimcp_free(ctx->study->trials);
        }
        nimcp_free(ctx->study);
    }

    /* Clean up TPE samplers */
    if (ctx->tpe_samplers) {
        for (uint32_t i = 0; i < ctx->search_space.num_params; i++) {
            if (ctx->tpe_samplers[i].good_values) {
                nimcp_free(ctx->tpe_samplers[i].good_values);
            }
            if (ctx->tpe_samplers[i].bad_values) {
                nimcp_free(ctx->tpe_samplers[i].bad_values);
            }
        }
        nimcp_free(ctx->tpe_samplers);
    }

    /* Clean up PBT population */
    if (ctx->pbt_population) {
        for (uint32_t i = 0; i < ctx->pbt_population_size; i++) {
            hpo_free_params(&ctx->pbt_population[i].params);
        }
        nimcp_free(ctx->pbt_population);
    }

    /* Clean up search space */
    if (ctx->search_space.params) {
        nimcp_free(ctx->search_space.params);
    }

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

hpo_study_t* hpo_get_study(
    hpo_ctx_t* ctx,
    const char* study_name,
    bool create_if_missing
) {
    if (!ctx || !study_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hpo_get_study: required parameter is NULL (ctx, study_name)");
        return NULL;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Check if current study matches */
    if (ctx->study && strcmp(ctx->study->name, study_name) == 0) {
        nimcp_mutex_unlock(ctx->mutex);
        return ctx->study;
    }

    /* Would load from storage in full implementation */
    if (create_if_missing) {
        if (ctx->study) {
            /* Clean up old study - free each trial's internal allocations */
            if (ctx->study->trials) {
                for (uint32_t i = 0; i < ctx->study->num_trials; i++) {
                    hpo_free_params(&ctx->study->trials[i].params);
                    if (ctx->study->trials[i].intermediate_values) {
                        nimcp_free(ctx->study->trials[i].intermediate_values);
                    }
                }
                nimcp_free(ctx->study->trials);
            }
            nimcp_free(ctx->study);
        }

        ctx->study = nimcp_calloc(1, sizeof(hpo_study_t));
        if (ctx->study) {
            strncpy(ctx->study->name, study_name, sizeof(ctx->study->name) - 1);
            ctx->study->max_trials = HPO_MAX_TRIALS;
            ctx->study->trials = nimcp_calloc(ctx->study->max_trials, sizeof(trial_state_t));
            ctx->study->best_objective = ctx->config.direction == HPO_MINIMIZE ? DBL_MAX : -DBL_MAX;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return ctx->study;
}

//=============================================================================
// Search Space API Implementation
//=============================================================================

int hpo_add_float(
    hpo_search_space_t* space,
    const char* name,
    double low,
    double high,
    bool log_scale
) {
    if (!space || !name || low >= high) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_add_float: required parameter is NULL (space, name)");
        return -1;
    }

    if (space->num_params >= HPO_MAX_PARAMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "hpo_add_float: capacity exceeded");
        return -1;
    }

    /* Allocate params array if needed */
    if (!space->params) {
        space->params = nimcp_calloc(HPO_MAX_PARAMS, sizeof(hpo_param_def_t));
        if (!space->params) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hpo_add_float: space->params is NULL");
            return -1;
        }
    }

    hpo_param_def_t* param = &space->params[space->num_params];
    param->name = name;
    param->type = log_scale ? HPO_PARAM_LOGUNIFORM : HPO_PARAM_FLOAT;
    param->low = low;
    param->high = high;
    param->step = 0.0;
    param->default_value = (low + high) / 2.0;

    space->num_params++;
    return 0;
}

int hpo_add_int(
    hpo_search_space_t* space,
    const char* name,
    int64_t low,
    int64_t high,
    int64_t step
) {
    if (!space || !name || low >= high) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_add_int: required parameter is NULL (space, name)");
        return -1;
    }

    if (space->num_params >= HPO_MAX_PARAMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "hpo_add_int: capacity exceeded");
        return -1;
    }

    if (!space->params) {
        space->params = nimcp_calloc(HPO_MAX_PARAMS, sizeof(hpo_param_def_t));
        if (!space->params) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hpo_add_int: space->params is NULL");
            return -1;
        }
    }

    hpo_param_def_t* param = &space->params[space->num_params];
    param->name = name;
    param->type = HPO_PARAM_INT;
    param->low = (double)low;
    param->high = (double)high;
    param->step = (double)(step > 0 ? step : 1);
    param->default_value = (double)((low + high) / 2);

    space->num_params++;
    return 0;
}

int hpo_add_categorical(
    hpo_search_space_t* space,
    const char* name,
    const char** choices,
    uint32_t num_choices
) {
    if (!space || !name || !choices || num_choices == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_add_categorical: required parameter is NULL (space, name, choices)");
        return -1;
    }

    if (space->num_params >= HPO_MAX_PARAMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "hpo_add_categorical: capacity exceeded");
        return -1;
    }

    if (!space->params) {
        space->params = nimcp_calloc(HPO_MAX_PARAMS, sizeof(hpo_param_def_t));
        if (!space->params) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hpo_add_categorical: space->params is NULL");
            return -1;
        }
    }

    hpo_param_def_t* param = &space->params[space->num_params];
    param->name = name;
    param->type = HPO_PARAM_CATEGORICAL;
    param->choices = choices;
    param->num_choices = num_choices;
    param->default_choice = choices[0];

    space->num_params++;
    return 0;
}

int hpo_add_conditional(
    hpo_search_space_t* space,
    const char* name,
    const hpo_param_def_t* def,
    const char* depends_on,
    const char* depends_value
) {
    if (!space || !name || !def || !depends_on || !depends_value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_add_conditional: required parameter is NULL (space, name, def, depends_on, depends_value)");
        return -1;
    }

    if (space->num_params >= HPO_MAX_PARAMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "hpo_add_conditional: capacity exceeded");
        return -1;
    }

    if (!space->params) {
        space->params = nimcp_calloc(HPO_MAX_PARAMS, sizeof(hpo_param_def_t));
        if (!space->params) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hpo_add_conditional: space->params is NULL");
            return -1;
        }
    }

    hpo_param_def_t* param = &space->params[space->num_params];
    memcpy(param, def, sizeof(hpo_param_def_t));
    param->name = name;
    param->type = HPO_PARAM_CONDITIONAL;
    param->depends_on = depends_on;
    param->depends_value = depends_value;

    space->num_params++;
    return 0;
}

//=============================================================================
// Optimization API Implementation
//=============================================================================

double hpo_optimize(
    hpo_ctx_t* ctx,
    double (*objective_fn)(const hpo_params_t* params, void* user_data),
    void* user_data,
    hpo_params_t** best_params
) {
    if (!ctx) return DBL_MAX;
    if (!objective_fn) {
        return ctx->config.direction == HPO_MINIMIZE ? DBL_MAX : -DBL_MAX;
    }

    nimcp_mutex_lock(ctx->mutex);

    double start_time = get_time_sec();
    double best_objective = ctx->config.direction == HPO_MINIMIZE ? DBL_MAX : -DBL_MAX;
    hpo_params_t* best = NULL;

    uint32_t n_trials = ctx->config.n_trials;

    for (uint32_t t = 0; t < n_trials; t++) {
        /* Check timeout */
        double elapsed_hours = (get_time_sec() - start_time) / 3600.0;
        if (elapsed_hours >= ctx->config.timeout_hours) {
            if (ctx->config.verbose) {
                printf("[HPO] Timeout reached after %.2f hours\n", elapsed_hours);
            }
            break;
        }

        /* Suggest next trial */
        hpo_params_t* params = NULL;
        int trial_id = hpo_suggest(ctx, &params);
        if (trial_id < 0 || !params) {
            continue;
        }

        /* Evaluate objective */
        double trial_start = get_time_sec();
        double objective = objective_fn(params, user_data);
        double trial_end = get_time_sec();

        /* Report result */
        hpo_report(ctx, trial_id, objective);

        /* Track best */
        bool is_better = (ctx->config.direction == HPO_MINIMIZE) ?
                         (objective < best_objective) : (objective > best_objective);

        if (is_better) {
            best_objective = objective;
            if (best) {
                hpo_free_params(best);
                nimcp_free(best);
            }
            best = params;

            ctx->stats.best_objective = best_objective;
            ctx->stats.trials_to_best = t + 1;

            if (ctx->config.verbose) {
                printf("[HPO] New best at trial %u: %.6f\n", t + 1, best_objective);
            }
        } else {
            hpo_free_params(params);
            nimcp_free(params);
        }

        /* Update statistics */
        ctx->stats.total_trials++;
        ctx->stats.avg_trial_time_sec =
            (ctx->stats.avg_trial_time_sec * (t) + (trial_end - trial_start)) / (t + 1);
    }

    double end_time = get_time_sec();
    ctx->stats.total_time_hours = (end_time - start_time) / 3600.0;

    if (best_params) {
        *best_params = best;
    } else if (best) {
        hpo_free_params(best);
        nimcp_free(best);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return best_objective;
}

int hpo_suggest(hpo_ctx_t* ctx, hpo_params_t** params) {
    if (!ctx || !params || !ctx->study) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_suggest: required parameter is NULL (ctx, params, ctx->study)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Check capacity */
    if (ctx->study->num_trials >= ctx->study->max_trials) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "hpo_suggest: capacity exceeded");
        return -1;
    }

    /* Allocate params */
    hpo_params_t* p = nimcp_calloc(1, sizeof(hpo_params_t));
    if (!p) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hpo_suggest: p is NULL");
        return -1;
    }

    p->num_params = ctx->search_space.num_params;
    p->param_names = nimcp_calloc(p->num_params, sizeof(const char*));
    p->param_values = nimcp_calloc(p->num_params, sizeof(double));
    p->param_choices = nimcp_calloc(p->num_params, sizeof(const char*));

    if (!p->param_names || !p->param_values || !p->param_choices) {
        hpo_free_params(p);
        nimcp_free(p);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hpo_suggest: required parameter is NULL (p->param_names, p->param_values, p->param_choices)");
        return -1;
    }

    /* Sample parameters based on algorithm */
    for (uint32_t i = 0; i < ctx->search_space.num_params; i++) {
        hpo_param_def_t* def = &ctx->search_space.params[i];
        p->param_names[i] = def->name;

        switch (ctx->config.algorithm) {
            case HPO_ALG_RANDOM:
            case HPO_ALG_GRID:
                /* Random/Grid sampling */
                switch (def->type) {
                    case HPO_PARAM_FLOAT:
                        p->param_values[i] = def->low + random_uniform() * (def->high - def->low);
                        break;
                    case HPO_PARAM_LOGUNIFORM:
                        p->param_values[i] = random_log_uniform(def->low, def->high);
                        break;
                    case HPO_PARAM_INT:
                    case HPO_PARAM_QUNIFORM:
                        p->param_values[i] = (double)random_int((int64_t)def->low, (int64_t)def->high);
                        break;
                    case HPO_PARAM_CATEGORICAL:
                        {
                            uint32_t choice = (uint32_t)(random_uniform() * def->num_choices);
                            if (choice >= def->num_choices) choice = def->num_choices - 1;
                            p->param_choices[i] = def->choices[choice];
                            p->param_values[i] = (double)choice;
                        }
                        break;
                    case HPO_PARAM_BOOL:
                        p->param_values[i] = random_uniform() < 0.5 ? 0.0 : 1.0;
                        break;
                    default:
                        p->param_values[i] = def->default_value;
                        break;
                }
                break;

            case HPO_ALG_BAYESIAN_TPE:
            case HPO_ALG_BOHB:
                /* TPE sampling */
                if (ctx->tpe_samplers && ctx->study->num_trials >= ctx->config.bayesian.n_initial_points) {
                    p->param_values[i] = sample_tpe(&ctx->tpe_samplers[i], def->low, def->high);
                } else {
                    /* Random during initial phase */
                    p->param_values[i] = def->low + random_uniform() * (def->high - def->low);
                }
                break;

            default:
                /* Default to random */
                p->param_values[i] = def->low + random_uniform() * (def->high - def->low);
                break;
        }
    }

    /* Create trial state */
    int trial_id = (int)ctx->study->num_trials;
    trial_state_t* trial = &ctx->study->trials[trial_id];
    trial->trial_id = trial_id;
    trial->status = HPO_TRIAL_RUNNING;
    trial->start_time = get_time_sec();

    /* Copy params to trial */
    trial->params.num_params = p->num_params;
    trial->params.param_names = nimcp_calloc(p->num_params, sizeof(const char*));
    trial->params.param_values = nimcp_calloc(p->num_params, sizeof(double));
    trial->params.param_choices = nimcp_calloc(p->num_params, sizeof(const char*));

    if (trial->params.param_names && trial->params.param_values) {
        memcpy((void*)trial->params.param_names, p->param_names, p->num_params * sizeof(const char*));
        memcpy(trial->params.param_values, p->param_values, p->num_params * sizeof(double));
        if (p->param_choices) {
            memcpy((void*)trial->params.param_choices, p->param_choices, p->num_params * sizeof(const char*));
        }
    }

    ctx->study->num_trials++;

    *params = p;

    nimcp_mutex_unlock(ctx->mutex);
    return trial_id;
}

int hpo_report(hpo_ctx_t* ctx, int trial_id, double objective) {
    if (!ctx || !ctx->study || trial_id < 0 || trial_id >= (int)ctx->study->num_trials) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_report: required parameter is NULL (ctx, ctx->study)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    trial_state_t* trial = &ctx->study->trials[trial_id];
    trial->objective = objective;
    trial->status = HPO_TRIAL_COMPLETED;
    trial->end_time = get_time_sec();

    ctx->stats.completed_trials++;

    /* Update best */
    bool is_better = (ctx->config.direction == HPO_MINIMIZE) ?
                     (objective < ctx->study->best_objective) :
                     (objective > ctx->study->best_objective);

    if (is_better) {
        ctx->study->best_objective = objective;
        ctx->study->best_trial_id = trial_id;
    }

    /* Update TPE models */
    if (ctx->tpe_samplers && ctx->search_space.num_params > 0) {
        /* Determine if this trial is "good" (top percentile) */
        uint32_t completed = ctx->stats.completed_trials;
        uint32_t good_threshold = (uint32_t)(completed * ctx->tpe_samplers[0].gamma);
        if (good_threshold == 0) good_threshold = 1;

        /* Simplified: just mark as good if in top 25% */
        bool is_good = is_better;

        for (uint32_t i = 0; i < ctx->search_space.num_params; i++) {
            update_tpe(&ctx->tpe_samplers[i], trial->params.param_values[i], objective, is_good);
        }
    }

    /* Update running average */
    ctx->stats.avg_objective =
        (ctx->stats.avg_objective * (ctx->stats.completed_trials - 1) + objective) /
        ctx->stats.completed_trials;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int hpo_report_intermediate(
    hpo_ctx_t* ctx,
    int trial_id,
    uint32_t step,
    double value
) {
    if (!ctx || !ctx->study || trial_id < 0 || trial_id >= (int)ctx->study->num_trials) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_report_intermediate: required parameter is NULL (ctx, ctx->study)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    trial_state_t* trial = &ctx->study->trials[trial_id];

    /* Store intermediate value */
    if (!trial->intermediate_values) {
        trial->max_intermediate = 1000;
        trial->intermediate_values = nimcp_calloc(trial->max_intermediate, sizeof(double));
    }

    if (trial->intermediate_values && trial->num_intermediate < trial->max_intermediate) {
        trial->intermediate_values[trial->num_intermediate++] = value;
    }

    /* Check if should prune */
    bool should_prune_trial = should_prune(ctx, trial);

    nimcp_mutex_unlock(ctx->mutex);

    return should_prune_trial ? 1 : 0;
}

int hpo_prune_trial(hpo_ctx_t* ctx, int trial_id) {
    if (!ctx || !ctx->study || trial_id < 0 || trial_id >= (int)ctx->study->num_trials) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_prune_trial: required parameter is NULL (ctx, ctx->study)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->study->trials[trial_id].status = HPO_TRIAL_PRUNED;
    ctx->study->trials[trial_id].end_time = get_time_sec();
    ctx->stats.pruned_trials++;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int hpo_fail_trial(hpo_ctx_t* ctx, int trial_id, const char* error_msg) {
    if (!ctx || !ctx->study || trial_id < 0 || trial_id >= (int)ctx->study->num_trials) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_fail_trial: required parameter is NULL (ctx, ctx->study)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    ctx->study->trials[trial_id].status = HPO_TRIAL_FAILED;
    ctx->study->trials[trial_id].end_time = get_time_sec();
    ctx->stats.failed_trials++;

    if (ctx->config.verbose && error_msg) {
        printf("[HPO] Trial %d failed: %s\n", trial_id, error_msg);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Results API Implementation
//=============================================================================

int hpo_get_best_trial(hpo_ctx_t* ctx, hpo_trial_result_t* result) {
    if (!ctx || !ctx->study || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_get_best_trial: required parameter is NULL (ctx, ctx->study, result)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->stats.completed_trials == 0) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_get_best_trial: ctx->stats.completed_trials is zero");
        return -1;
    }

    trial_state_t* best = &ctx->study->trials[ctx->study->best_trial_id];

    result->trial_id = best->trial_id;
    result->objective = best->objective;
    result->status = best->status;
    result->duration_sec = best->end_time - best->start_time;

    /* Copy params */
    result->params.num_params = best->params.num_params;
    result->params.param_names = nimcp_calloc(best->params.num_params, sizeof(const char*));
    result->params.param_values = nimcp_calloc(best->params.num_params, sizeof(double));

    if (result->params.param_names && result->params.param_values) {
        memcpy((void*)result->params.param_names, best->params.param_names,
               best->params.num_params * sizeof(const char*));
        memcpy(result->params.param_values, best->params.param_values,
               best->params.num_params * sizeof(double));
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int hpo_get_all_trials(
    hpo_ctx_t* ctx,
    hpo_trial_result_t** results,
    uint32_t* num_results
) {
    if (!ctx || !ctx->study || !results || !num_results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_get_all_trials: required parameter is NULL (ctx, ctx->study, results, num_results)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t n = ctx->study->num_trials;
    *num_results = n;

    if (n == 0) {
        *results = NULL;
        nimcp_mutex_unlock(ctx->mutex);
        return 0;
    }

    *results = nimcp_calloc(n, sizeof(hpo_trial_result_t));
    if (!*results) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_get_all_trials: validation failed");
        return -1;
    }

    for (uint32_t i = 0; i < n; i++) {
        trial_state_t* trial = &ctx->study->trials[i];
        hpo_trial_result_t* result = &(*results)[i];

        result->trial_id = trial->trial_id;
        result->objective = trial->objective;
        result->status = trial->status;
        result->duration_sec = trial->end_time - trial->start_time;
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int hpo_get_importance(
    hpo_ctx_t* ctx,
    const char*** param_names,
    double** importance,
    uint32_t* num_params
) {
    if (!ctx || !param_names || !importance || !num_params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_get_importance: required parameter is NULL (ctx, param_names, importance, num_params)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    uint32_t n = ctx->search_space.num_params;
    *num_params = n;

    *param_names = nimcp_calloc(n, sizeof(const char*));
    *importance = nimcp_calloc(n, sizeof(double));

    if (!*param_names || !*importance) {
        if (*param_names) nimcp_free((void*)*param_names);
        if (*importance) nimcp_free(*importance);
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_get_importance: validation failed");
        return -1;
    }

    /* Compute simple importance based on correlation with objective */
    /* In full implementation, would use more sophisticated methods */
    for (uint32_t i = 0; i < n; i++) {
        (*param_names)[i] = ctx->search_space.params[i].name;
        (*importance)[i] = 1.0 / (double)n;  /* Placeholder: uniform importance */
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Integration API Implementation
//=============================================================================

int hpo_connect_distributed(hpo_ctx_t* ctx, void* dist_ctx) {
    if (!ctx || !dist_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_connect_distributed: required parameter is NULL (ctx, dist_ctx)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->dist_ctx = dist_ctx;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int hpo_connect_callbacks(hpo_ctx_t* ctx, void* callbacks) {
    if (!ctx || !callbacks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_connect_callbacks: required parameter is NULL (ctx, callbacks)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->callbacks = callbacks;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int hpo_get_stats(const hpo_ctx_t* ctx, hpo_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(hpo_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);

    return 0;
}

void hpo_reset_stats(hpo_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(hpo_stats_t));
    ctx->stats.best_objective = ctx->config.direction == HPO_MINIMIZE ? DBL_MAX : -DBL_MAX;
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* hpo_algorithm_name(hpo_algorithm_t alg) {
    if (alg >= HPO_ALG_COUNT) {
        return "Unknown";
    }
    return algorithm_names[alg];
}

const char* hpo_param_type_name(hpo_param_type_t type) {
    if (type >= HPO_PARAM_TYPE_COUNT) {
        return "Unknown";
    }
    return param_type_names[type];
}

int hpo_validate_config(const hpo_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Validate algorithm */
    if (config->algorithm >= HPO_ALG_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "hpo_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate trials */
    if (config->n_trials == 0 || config->n_trials > HPO_MAX_TRIALS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_validate_config: config->n_trials is zero");
        return -1;
    }

    /* Validate parallel */
    if (config->n_parallel > HPO_MAX_PARALLEL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_validate_config: validation failed");
        return -1;
    }

    return 0;
}

void hpo_free_params(hpo_params_t* params) {
    if (!params) {
        return;
    }

    /* Free internal arrays only - params itself may be stack-allocated */
    if (params->param_names) {
        nimcp_free((void*)params->param_names);
        params->param_names = NULL;
    }
    if (params->param_values) {
        nimcp_free(params->param_values);
        params->param_values = NULL;
    }
    if (params->param_choices) {
        nimcp_free((void*)params->param_choices);
        params->param_choices = NULL;
    }
    params->num_params = 0;

    /* NOTE: Do not free params itself - it may be stack-allocated.
     * Use hpo_destroy_params() to free a heap-allocated hpo_params_t */
}

void hpo_free_trial_result(hpo_trial_result_t* result) {
    if (!result) {
        return;
    }

    if (result->params.param_names) {
        nimcp_free((void*)result->params.param_names);
    }
    if (result->params.param_values) {
        nimcp_free(result->params.param_values);
    }
    if (result->params.param_choices) {
        nimcp_free((void*)result->params.param_choices);
    }
    if (result->intermediate_values) {
        nimcp_free(result->intermediate_values);
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Generate uniform random number in [0, 1)
 */
static double random_uniform(void) {
    return (double)nimcp_tl_rand() / ((double)RAND_MAX + 1.0);
}

/**
 * @brief Generate log-uniform random number
 */
static double random_log_uniform(double low, double high) {
    double log_low = log(low);
    double log_high = log(high);
    double log_val = log_low + random_uniform() * (log_high - log_low);
    return exp(log_val);
}

/**
 * @brief Generate random integer in [low, high]
 */
static int64_t random_int(int64_t low, int64_t high) {
    return low + (int64_t)(random_uniform() * (double)(high - low + 1));
}

/**
 * @brief Sample from TPE model
 */
static double sample_tpe(tpe_sampler_t* sampler, double low, double high) {
    if (!sampler || sampler->num_good == 0) {
        return low + random_uniform() * (high - low);
    }

    /* Simplified TPE: sample from good values with perturbation */
    uint32_t idx = (uint32_t)(random_uniform() * sampler->num_good);
    if (idx >= sampler->num_good) idx = sampler->num_good - 1;

    double base = sampler->good_values[idx];
    double perturbation = (random_uniform() - 0.5) * 0.1 * (high - low);
    double value = base + perturbation;

    /* Clamp to bounds */
    if (value < low) value = low;
    if (value > high) value = high;

    return value;
}

/**
 * @brief Update TPE model with new observation
 */
static void update_tpe(tpe_sampler_t* sampler, double value, double objective, bool is_good) {
    if (!sampler) return;

    /* BUG FIX: Previously, realloc result was assigned directly to the array
     * pointer AND count was incremented before the NULL check. If realloc
     * returned NULL: (1) the old pointer was leaked, (2) count was wrong,
     * (3) subsequent access would NULL-deref. Fix: use temp pointer, only
     * commit on success. */
    if (is_good) {
        double* tmp = nimcp_realloc(sampler->good_values,
                                     (sampler->num_good + 1) * sizeof(double));
        if (tmp) {
            sampler->good_values = tmp;
            sampler->good_values[sampler->num_good] = value;
            sampler->num_good++;
        }
    } else {
        double* tmp = nimcp_realloc(sampler->bad_values,
                                     (sampler->num_bad + 1) * sizeof(double));
        if (tmp) {
            sampler->bad_values = tmp;
            sampler->bad_values[sampler->num_bad] = value;
            sampler->num_bad++;
        }
    }
}

/**
 * @brief Check if trial should be pruned
 */
static bool should_prune(hpo_ctx_t* ctx, trial_state_t* trial) {
    if (!ctx || !trial || ctx->config.pruner.strategy == HPO_PRUNE_NONE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "should_prune: required parameter is NULL (ctx, trial)");
        return false;
    }

    /* Wait for startup trials */
    if (ctx->stats.completed_trials < ctx->config.pruner.n_startup_trials) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "should_prune: validation failed");
        return false;
    }

    /* Wait for warmup steps */
    if (trial->num_intermediate < ctx->config.pruner.n_warmup_steps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "should_prune: validation failed");
        return false;
    }

    /* Get current value */
    double current = trial->intermediate_values[trial->num_intermediate - 1];

    /* Compute median of other trials at this step */
    /* Simplified: just check if significantly worse than best */
    bool is_minimize = ctx->config.direction == HPO_MINIMIZE;
    double best = ctx->study->best_objective;

    if (is_minimize && current > best * 2.0) {
        return true;
    }
    if (!is_minimize && current < best * 0.5) {
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "should_prune: is_minimize is NULL");
    return false;
}

/**
 * @brief Get current time in seconds
 */
static double get_time_sec(void) {
    return (double)time(NULL);
}

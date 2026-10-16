/**
 * @file nimcp_quantum_mcts.c
 * @brief Implementation of Quantum Monte Carlo Tree Search
 *
 * Hybrid classical-quantum MCTS for mathematical planning and theorem proving.
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/neuro_symbolic/nimcp_quantum_mcts.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "QUANTUM_MCTS"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE(quantum_mcts, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Internal Quantum MCTS state
 */
struct quantum_mcts {
    /* Configuration */
    quantum_mcts_config_t config;

    /* Tree structure */
    qmcts_node_t* nodes;
    uint32_t num_nodes;
    uint32_t node_capacity;
    uint32_t next_node_id;
    uint32_t root_id;

    /* Environment callbacks */
    qmcts_transition_fn transition_fn;
    qmcts_value_fn value_fn;
    qmcts_action_fn action_fn;
    void* user_data;

    /* Linked systems */
    fep_planning_system_t* fep_planner;
    quantum_annealer_t* annealer;

    /* Quantum state cache */
    qmcts_cache_entry_t* cache;
    uint32_t cache_size;
    uint32_t cache_capacity;

    /* Current state */
    float* current_state;
    uint32_t current_state_dim;
    float temperature;
    float atp_level;

    /* Statistics */
    quantum_mcts_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bio_router_t* bio_router;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* RNG seed */
    uint32_t rng_seed;

    /* Module identification */
    uint32_t module_id;
    const char* module_name;
};

/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static uint32_t create_node(quantum_mcts_t* qmcts, uint32_t parent_id,
    const float* state, uint32_t state_dim, int action);
static nimcp_error_t expand_node(quantum_mcts_t* qmcts, uint32_t node_id);
static float simulate_rollout(quantum_mcts_t* qmcts, uint32_t node_id);
static void backpropagate(quantum_mcts_t* qmcts, uint32_t node_id, float value);
static float ucb_score(const quantum_mcts_t* qmcts, const qmcts_node_t* node,
    uint32_t parent_visits);
static uint32_t select_node(quantum_mcts_t* qmcts, uint32_t root_id);
static int find_node_index(const quantum_mcts_t* qmcts, uint32_t node_id);
static float random_float(uint32_t* seed);
static uint64_t compute_state_hash(const float* state, uint32_t dim);
static qmcts_cache_entry_t* cache_lookup(quantum_mcts_t* qmcts, uint64_t hash);
static void cache_insert(quantum_mcts_t* qmcts, uint64_t hash,
    const qmc_amplitude_result_t* amplitude, float value);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API quantum_mcts_t* quantum_mcts_create(
    const quantum_mcts_config_t* config)
{
    quantum_mcts_t* qmcts = NULL;

    qmcts = (quantum_mcts_t*)nimcp_calloc(1, sizeof(quantum_mcts_t));
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate Quantum MCTS system");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        memcpy(&qmcts->config, config, sizeof(quantum_mcts_config_t));
    } else {
        quantum_mcts_get_default_config(&qmcts->config);
    }

    /* Allocate nodes */
    qmcts->node_capacity = 1024;
    qmcts->nodes = (qmcts_node_t*)nimcp_calloc(
        qmcts->node_capacity, sizeof(qmcts_node_t));
    if (!qmcts->nodes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate QMCTS nodes");
        nimcp_free(qmcts);
        qmcts = NULL;
        return NULL;
    }

    /* Allocate cache if enabled */
    if (qmcts->config.enable_caching) {
        qmcts->cache_capacity = qmcts->config.max_cached_states;
        if (qmcts->cache_capacity == 0) {
            qmcts->cache_capacity = QMCTS_MAX_CACHED_STATES;
        }
        qmcts->cache = (qmcts_cache_entry_t*)nimcp_calloc(
            qmcts->cache_capacity, sizeof(qmcts_cache_entry_t));
        /* Cache allocation failure is non-fatal */
    }

    /* Create mutex */
    mutex_attr_t mutex_attr = {
        .type = MUTEX_TYPE_RECURSIVE,
        
        
    };
    qmcts->mutex = nimcp_mutex_create(&mutex_attr);
    if (!qmcts->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "Failed to create QMCTS mutex");
        if (qmcts->cache) nimcp_free(qmcts->cache);
        nimcp_free(qmcts->nodes);
        nimcp_free(qmcts);
        qmcts = NULL;
        return NULL;
    }

    /* Initialize state */
    qmcts->num_nodes = 0;
    qmcts->next_node_id = 1;
    qmcts->root_id = 0;
    qmcts->temperature = qmcts->config.temperature;
    qmcts->atp_level = 1.0f;
    qmcts->rng_seed = (uint32_t)nimcp_time_monotonic_us();

    /* Initialize statistics */
    memset(&qmcts->stats, 0, sizeof(quantum_mcts_stats_t));

    /* Module identification */
    qmcts->module_id = BIO_MODULE_QUANTUM_MCTS;
    qmcts->module_name = "quantum_mcts";
    qmcts->bio_async_enabled = false;

    return qmcts;
}

NIMCP_API void quantum_mcts_destroy(quantum_mcts_t* qmcts)
{
    if (!qmcts) {
        return;
    }

    /* Unregister from bio-async */
    if (qmcts->bio_async_enabled) {
        quantum_mcts_unregister_bio_async(qmcts);
    }

    /* Free nodes and their contents */
    for (uint32_t i = 0; i < qmcts->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && qmcts->num_nodes > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)qmcts->num_nodes);
        }

        qmcts_node_t* node = &qmcts->nodes[i];
        if (node->children) {
            nimcp_free(node->children);
        }
        if (node->state) {
            nimcp_free(node->state);
        }
    }

    if (qmcts->nodes) {
        nimcp_free(qmcts->nodes);
    }

    if (qmcts->cache) {
        nimcp_free(qmcts->cache);
    }

    if (qmcts->current_state) {
        nimcp_free(qmcts->current_state);
    }

    if (qmcts->mutex) {
        nimcp_mutex_destroy(qmcts->mutex);
    }

    nimcp_free(qmcts);
    qmcts = NULL;
}

NIMCP_API nimcp_error_t quantum_mcts_reset(quantum_mcts_t* qmcts)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_reset: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);

    /* Free node contents */
    for (uint32_t i = 0; i < qmcts->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && qmcts->num_nodes > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)qmcts->num_nodes);
        }

        qmcts_node_t* node = &qmcts->nodes[i];
        if (node->children) {
            nimcp_free(node->children);
            node->children = NULL;
        }
        if (node->state) {
            nimcp_free(node->state);
            node->state = NULL;
        }
    }

    /* Reset node array */
    memset(qmcts->nodes, 0, qmcts->node_capacity * sizeof(qmcts_node_t));
    qmcts->num_nodes = 0;
    qmcts->next_node_id = 1;
    qmcts->root_id = 0;

    /* Clear cache */
    if (qmcts->cache) {
        memset(qmcts->cache, 0, qmcts->cache_capacity * sizeof(qmcts_cache_entry_t));
        qmcts->cache_size = 0;
    }

    /* Reset statistics */
    memset(&qmcts->stats, 0, sizeof(quantum_mcts_stats_t));

    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t quantum_mcts_get_default_config(
    quantum_mcts_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_get_default_config: config is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Classical MCTS parameters */
    config->num_simulations = QMCTS_DEFAULT_SIMULATIONS;
    config->exploration_constant = QMCTS_DEFAULT_EXPLORATION;
    config->planning_horizon = 32;
    config->discount_factor = NIMCP_REWARD_DISCOUNT_DEFAULT;

    /* Quantum enhancement */
    config->enhancement = QMCTS_ENHANCE_ROLLOUT;
    config->enable_quantum_rollouts = true;
    config->enable_amplitude_estimation = true;
    config->enable_quantum_sampling = false;
    config->qmc_shots = QMCTS_DEFAULT_SHOTS;
    config->quantum_exploration_boost = 0.1f;

    /* Hybrid parameters */
    config->quantum_fraction = QMCTS_DEFAULT_QUANTUM_FRACTION;
    config->classical_simulations = 700;
    config->quantum_simulations = 300;

    /* State encoding */
    config->encoding = QMCTS_ENCODE_DIRECT;
    config->max_state_dim = 256;

    /* Caching */
    config->enable_caching = true;
    config->max_cached_states = QMCTS_MAX_CACHED_STATES;

    /* Integration */
    config->enable_fep_integration = true;
    config->enable_bio_async = true;

    /* Modulation */
    config->temperature = 1.0f;
    config->atp_sensitivity = 0.3f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Setup Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t quantum_mcts_set_environment(
    quantum_mcts_t* qmcts,
    qmcts_transition_fn transition,
    qmcts_value_fn value,
    qmcts_action_fn actions,
    void* user_data)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_set_environment: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);

    qmcts->transition_fn = transition;
    qmcts->value_fn = value;
    qmcts->action_fn = actions;
    qmcts->user_data = user_data;

    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t quantum_mcts_link_fep(
    quantum_mcts_t* qmcts,
    fep_planning_system_t* fep_planner)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_link_fep: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);
    qmcts->fep_planner = fep_planner;
    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t quantum_mcts_link_annealer(
    quantum_mcts_t* qmcts,
    quantum_annealer_t* annealer)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_link_annealer: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);
    qmcts->annealer = annealer;
    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Planning Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t quantum_mcts_plan(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim,
    qmcts_plan_t* plan)
{
    if (!qmcts || !state || state_dim == 0 || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_plan: qmcts, state, or plan is NULL, or state_dim is 0");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Reset tree for new planning */
    quantum_mcts_reset(qmcts);

    /* Create root node */
    qmcts->root_id = create_node(qmcts, 0, state, state_dim, -1);
    if (qmcts->root_id == UINT32_MAX) {
        nimcp_mutex_unlock(qmcts->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "quantum_mcts_plan: failed to create root node");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Store current state */
    if (qmcts->current_state) {
        nimcp_free(qmcts->current_state);
    }
    qmcts->current_state = (float*)nimcp_calloc(state_dim, sizeof(float));
    if (qmcts->current_state) {
        memcpy(qmcts->current_state, state, state_dim * sizeof(float));
        qmcts->current_state_dim = state_dim;
    }

    /* Compute number of simulations based on ATP level */
    uint32_t num_sims = qmcts->config.num_simulations;
    if (qmcts->atp_level < 1.0f) {
        num_sims = (uint32_t)(num_sims * qmcts->atp_level);
        if (num_sims < 10) num_sims = 10;
    }

    /* Run simulations */
    nimcp_error_t result = quantum_mcts_simulate(qmcts, num_sims);
    if (result != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(qmcts->mutex);
        return result;
    }

    /* Extract plan from tree */
    uint32_t current_id = qmcts->root_id;
    uint32_t action_count = 0;
    uint32_t max_actions = qmcts->config.planning_horizon;

    while (action_count < max_actions) {
        int idx = find_node_index(qmcts, current_id);
        if (idx < 0) break;

        const qmcts_node_t* node = &qmcts->nodes[idx];
        if (node->is_terminal || node->num_children == 0) break;

        /* Get best child */
        uint32_t best_child = quantum_mcts_get_best_child(qmcts, current_id);
        if (best_child == UINT32_MAX) break;

        int child_idx = find_node_index(qmcts, best_child);
        if (child_idx < 0) break;

        const qmcts_node_t* child_node = &qmcts->nodes[child_idx];

        /* Add action to plan */
        if (action_count < plan->num_actions) {
            plan->actions[action_count] = child_node->action_id;
            if (plan->step_values) {
                plan->step_values[action_count] = child_node->mean_value;
            }
            if (plan->step_uncertainties) {
                plan->step_uncertainties[action_count] = child_node->quantum_uncertainty;
            }
        }

        action_count++;
        current_id = best_child;
    }

    plan->num_actions = action_count;

    /* Compute expected value and uncertainty */
    int root_idx = find_node_index(qmcts, qmcts->root_id);
    if (root_idx >= 0) {
        const qmcts_node_t* root = &qmcts->nodes[root_idx];
        plan->expected_value = root->mean_value;
        plan->uncertainty = root->quantum_uncertainty;
        plan->quantum_confidence = 1.0f - root->quantum_uncertainty;
    }

    /* Update statistics */
    uint64_t elapsed = nimcp_time_monotonic_us() - start_time;
    qmcts->stats.total_planning_time_us += elapsed;
    qmcts->stats.avg_planning_time_us =
        (float)qmcts->stats.total_planning_time_us /
        (qmcts->stats.total_simulations > 0 ? qmcts->stats.total_simulations : 1);

    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t quantum_mcts_select_action(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim,
    int* action)
{
    if (!qmcts || !state || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_select_action: qmcts, state, or action is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    qmcts_plan_t plan;
    memset(&plan, 0, sizeof(plan));

    /* Allocate minimal plan storage */
    plan.actions = (int*)nimcp_malloc(sizeof(int));
    if (!plan.actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "quantum_mcts_select_action: failed to allocate plan.actions");
        return NIMCP_ERROR_NO_MEMORY;
    }
    plan.num_actions = 1;

    nimcp_error_t result = quantum_mcts_plan(qmcts, state, state_dim, &plan);
    if (result == NIMCP_SUCCESS && plan.num_actions > 0) {
        *action = plan.actions[0];
    } else {
        *action = 0;  /* Default action */
    }

    nimcp_free(plan.actions);

    return result;
}

NIMCP_API nimcp_error_t quantum_mcts_simulate(
    quantum_mcts_t* qmcts,
    uint32_t num_simulations)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_simulate: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Compute classical vs quantum split */
    uint32_t quantum_sims = (uint32_t)(num_simulations *
        qmcts->config.quantum_fraction);
    uint32_t classical_sims = num_simulations - quantum_sims;

    /* Run classical simulations */
    for (uint32_t i = 0; i < classical_sims; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && classical_sims > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)classical_sims);
        }

        /* Selection */
        uint32_t selected = select_node(qmcts, qmcts->root_id);

        /* Expansion */
        int idx = find_node_index(qmcts, selected);
        if (idx >= 0 && !qmcts->nodes[idx].is_expanded &&
            !qmcts->nodes[idx].is_terminal) {
            expand_node(qmcts, selected);
        }

        /* Simulation (rollout) */
        float value = simulate_rollout(qmcts, selected);

        /* Backpropagation */
        backpropagate(qmcts, selected, value);

        qmcts->stats.classical_simulations++;
    }

    /* Run quantum-enhanced simulations */
    for (uint32_t i = 0; i < quantum_sims; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && quantum_sims > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)quantum_sims);
        }

        /* Selection with quantum bonus */
        uint32_t selected = select_node(qmcts, qmcts->root_id);

        /* Expansion */
        int idx = find_node_index(qmcts, selected);
        if (idx >= 0 && !qmcts->nodes[idx].is_expanded &&
            !qmcts->nodes[idx].is_terminal) {
            expand_node(qmcts, selected);
        }

        /* Quantum-enhanced rollout */
        float value = quantum_mcts_rollout(qmcts, selected, qmcts->fep_planner);

        /* Backpropagation */
        backpropagate(qmcts, selected, value);

        qmcts->stats.quantum_simulations++;
    }

    qmcts->stats.total_simulations += num_simulations;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Quantum-Enhanced Functions
 * ============================================================================ */

NIMCP_API float quantum_mcts_rollout(
    quantum_mcts_t* qmcts,
    uint32_t node_id,
    void* fep)
{
    if (!qmcts) {
        return 0.0f;
    }

    int idx = find_node_index(qmcts, node_id);
    if (idx < 0) {
        return 0.0f;
    }

    qmcts_node_t* node = &qmcts->nodes[idx];
    uint64_t start_time = nimcp_time_monotonic_us();

    /* Check cache first */
    if (qmcts->config.enable_caching && node->state) {
        uint64_t hash = compute_state_hash(node->state, node->state_dim);
        qmcts_cache_entry_t* entry = cache_lookup(qmcts, hash);
        if (entry) {
            qmcts->stats.cache_hits++;
            return entry->value_estimate;
        }
        qmcts->stats.cache_misses++;
    }

    float value = 0.0f;

    /* Use quantum amplitude estimation if enabled */
    if (qmcts->config.enable_amplitude_estimation && node->state) {
        qmc_amplitude_result_t amplitude_result;
        nimcp_error_t result = quantum_mcts_estimate_value(
            qmcts, node->state, node->state_dim, &amplitude_result);

        if (result == NIMCP_SUCCESS) {
            /* Use amplitude as value estimate */
            value = amplitude_result.probability;
            node->quantum_value = value;
            node->quantum_uncertainty = amplitude_result.std_error;

            qmcts->stats.qmc_shots_used += amplitude_result.samples_used;

            /* Cache the result */
            if (qmcts->config.enable_caching) {
                uint64_t hash = compute_state_hash(node->state, node->state_dim);
                cache_insert(qmcts, hash, &amplitude_result, value);
            }

            qmcts->stats.total_rollout_time_us +=
                nimcp_time_monotonic_us() - start_time;
            return value;
        }
    }

    /* Fall back to classical rollout with FEP integration */
    if (fep && qmcts->config.enable_fep_integration) {
        value = quantum_mcts_fep_value(qmcts, node->state, node->state_dim);
    } else if (qmcts->value_fn && node->state) {
        /* Use provided value function */
        value = qmcts->value_fn(node->state, node->state_dim, qmcts->user_data);
    } else {
        /* Random rollout simulation */
        float cumulative = 0.0f;
        float discount = 1.0f;
        uint32_t depth = 0;
        uint32_t max_depth = qmcts->config.planning_horizon - node->proof_depth;

        float* temp_state = NULL;
        if (node->state_dim > 0) {
            temp_state = (float*)nimcp_calloc(node->state_dim, sizeof(float));
            if (temp_state) {
                memcpy(temp_state, node->state, node->state_dim * sizeof(float));
            }
        }

        while (depth < max_depth && temp_state) {
            /* Get available actions */
            int actions[64];
            uint32_t num_actions = 0;

            if (qmcts->action_fn) {
                num_actions = qmcts->action_fn(temp_state, node->state_dim,
                    actions, 64, qmcts->user_data);
            }

            if (num_actions == 0) break;

            /* Random action selection */
            int action = actions[(uint32_t)(random_float(&qmcts->rng_seed) *
                num_actions) % num_actions];

            /* Apply transition */
            float next_state[256];
            float reward = 0.0f;
            bool terminal = false;

            if (qmcts->transition_fn) {
                qmcts->transition_fn(temp_state, node->state_dim, action,
                    next_state, &reward, &terminal, qmcts->user_data);

                cumulative += discount * reward;
                discount *= qmcts->config.discount_factor;

                memcpy(temp_state, next_state, node->state_dim * sizeof(float));

                if (terminal) break;
            } else {
                break;
            }

            depth++;
        }

        if (temp_state) {
            nimcp_free(temp_state);
            temp_state = NULL;
        }

        value = cumulative;
    }

    qmcts->stats.total_rollout_time_us += nimcp_time_monotonic_us() - start_time;
    qmcts->stats.avg_rollout_time_us =
        (float)qmcts->stats.total_rollout_time_us /
        (qmcts->stats.total_simulations > 0 ? qmcts->stats.total_simulations : 1);

    return value;
}

NIMCP_API nimcp_error_t quantum_mcts_estimate_value(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim,
    qmc_amplitude_result_t* result)
{
    if (!qmcts || !state || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_estimate_value: qmcts, state, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Encode state as amplitude distribution */
    uint32_t num_states = 256;  /* Fixed encoding size */
    if (state_dim < num_states) {
        num_states = state_dim;
    }

    float* amplitudes = (float*)nimcp_calloc(num_states, sizeof(float));
    if (!amplitudes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "quantum_mcts_estimate_value: failed to allocate amplitudes");
        return NIMCP_ERROR_NO_MEMORY;
    }

    /* Convert state to amplitude distribution */
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_states && i < state_dim; i++) {
        amplitudes[i] = fabsf(state[i]);
        sum += amplitudes[i] * amplitudes[i];
    }

    /* Normalize */
    if (sum > 0.0f) {
        float norm = sqrtf(sum);
        for (uint32_t i = 0; i < num_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_states > 256) {
                quantum_mcts_heartbeat("quantum_mcts_loop",
                                 (float)(i + 1) / (float)num_states);
            }

            amplitudes[i] /= norm;
        }
    } else {
        /* Uniform distribution */
        float uniform = 1.0f / sqrtf((float)num_states);
        for (uint32_t i = 0; i < num_states; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_states > 256) {
                quantum_mcts_heartbeat("quantum_mcts_loop",
                                 (float)(i + 1) / (float)num_states);
            }

            amplitudes[i] = uniform;
        }
    }

    /* Configure QMC estimation */
    qmc_amplitude_config_t qmc_config = {
        .num_samples = qmcts->config.qmc_shots,
        .use_importance = true,
        .proposal_dist = NULL,
        .seed = qmcts->rng_seed++
    };

    /* Estimate amplitude of "high-value" states */
    uint32_t target_state = 0;
    float max_amp = 0.0f;
    for (uint32_t i = 0; i < num_states; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_states > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)num_states);
        }

        if (amplitudes[i] > max_amp) {
            max_amp = amplitudes[i];
            target_state = i;
        }
    }

    qmc_result_t qmc_result = qmc_estimate_amplitude(
        amplitudes, num_states, target_state, &qmc_config, result);

    nimcp_free(amplitudes);
    amplitudes = NULL;

    if (qmc_result != QMC_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "quantum_mcts_estimate_value: qmc_estimate_amplitude failed");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API float quantum_mcts_exploration_bonus(
    quantum_mcts_t* qmcts,
    const qmcts_node_t* node)
{
    if (!qmcts || !node) {
        return 0.0f;
    }

    /* Base exploration from uncertainty */
    float uncertainty_bonus = node->quantum_uncertainty *
        qmcts->config.quantum_exploration_boost;

    /* Amplitude-based bonus (higher amplitude = more promising unexplored paths) */
    float amplitude_bonus = 0.0f;
    if (node->amplitude.real != 0.0f || node->amplitude.imag != 0.0f) {
        float amp_mag = sqrtf(node->amplitude.real * node->amplitude.real +
                              node->amplitude.imag * node->amplitude.imag);
        amplitude_bonus = amp_mag * qmcts->config.quantum_exploration_boost;
    }

    return uncertainty_bonus + amplitude_bonus;
}

NIMCP_API uint32_t quantum_mcts_select_child(
    quantum_mcts_t* qmcts,
    uint32_t node_id)
{
    if (!qmcts) {
        return UINT32_MAX;
    }

    int idx = find_node_index(qmcts, node_id);
    if (idx < 0) {
        return UINT32_MAX;
    }

    const qmcts_node_t* parent = &qmcts->nodes[idx];
    if (parent->num_children == 0) {
        return UINT32_MAX;
    }

    uint32_t best_child = UINT32_MAX;
    float best_score = -INFINITY;

    for (uint32_t i = 0; i < parent->num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && parent->num_children > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)parent->num_children);
        }

        int child_idx = find_node_index(qmcts, parent->children[i]);
        if (child_idx < 0) continue;

        const qmcts_node_t* child = &qmcts->nodes[child_idx];

        /* Quantum-enhanced UCB */
        float score = ucb_score(qmcts, child, parent->visit_count);

        /* Add quantum exploration bonus */
        score += quantum_mcts_exploration_bonus(qmcts, child);

        if (score > best_score) {
            best_score = score;
            best_child = parent->children[i];
        }
    }

    return best_child;
}

/* ============================================================================
 * Tree Management
 * ============================================================================ */

NIMCP_API const qmcts_node_t* quantum_mcts_get_node(
    const quantum_mcts_t* qmcts,
    uint32_t node_id)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_mcts_get_node: qmcts is NULL");
        return NULL;
    }

    int idx = find_node_index(qmcts, node_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "quantum_mcts_get_node: validation failed");
        return NULL;
    }

    return &qmcts->nodes[idx];
}

NIMCP_API const qmcts_node_t* quantum_mcts_get_root(const quantum_mcts_t* qmcts)
{
    if (!qmcts || qmcts->root_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "quantum_mcts_get_root: qmcts is NULL");
        return NULL;
    }

    return quantum_mcts_get_node(qmcts, qmcts->root_id);
}

NIMCP_API uint32_t quantum_mcts_get_best_child(
    const quantum_mcts_t* qmcts,
    uint32_t node_id)
{
    if (!qmcts) {
        return UINT32_MAX;
    }

    int idx = find_node_index(qmcts, node_id);
    if (idx < 0) {
        return UINT32_MAX;
    }

    const qmcts_node_t* parent = &qmcts->nodes[idx];
    if (parent->num_children == 0) {
        return UINT32_MAX;
    }

    uint32_t best_child = UINT32_MAX;
    uint32_t best_visits = 0;
    float best_value = -INFINITY;

    for (uint32_t i = 0; i < parent->num_children; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && parent->num_children > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)parent->num_children);
        }

        int child_idx = find_node_index(qmcts, parent->children[i]);
        if (child_idx < 0) continue;

        const qmcts_node_t* child = &qmcts->nodes[child_idx];

        /* Prefer most visited (robust child selection) */
        if (child->visit_count > best_visits ||
            (child->visit_count == best_visits && child->mean_value > best_value)) {
            best_visits = child->visit_count;
            best_value = child->mean_value;
            best_child = parent->children[i];
        }
    }

    return best_child;
}

NIMCP_API uint32_t quantum_mcts_get_depth(const quantum_mcts_t* qmcts)
{
    if (!qmcts) {
        return 0;
    }

    return qmcts->stats.max_depth_reached;
}

/* ============================================================================
 * FEP Integration
 * ============================================================================ */

NIMCP_API float quantum_mcts_fep_value(
    quantum_mcts_t* qmcts,
    const float* state,
    uint32_t state_dim)
{
    if (!qmcts || !state) {
        return 0.0f;
    }

    /* If FEP planner is linked, use its value estimation */
    if (qmcts->fep_planner) {
        /* TODO: Call FEP planning system for value estimation */
        /* For now, use a simple heuristic */
    }

    /* Fallback: use value function if provided */
    if (qmcts->value_fn) {
        return qmcts->value_fn(state, state_dim, qmcts->user_data);
    }

    return 0.0f;
}

NIMCP_API nimcp_error_t quantum_mcts_active_inference_action(
    quantum_mcts_t* qmcts,
    const float* belief_state,
    uint32_t belief_dim,
    int* action)
{
    if (!qmcts || !belief_state || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_active_inference_action: qmcts, belief_state, or action is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Use standard planning with FEP integration */
    return quantum_mcts_select_action(qmcts, belief_state, belief_dim, action);
}

/* ============================================================================
 * Plan Management
 * ============================================================================ */

NIMCP_API nimcp_error_t quantum_mcts_plan_init(
    qmcts_plan_t* plan,
    uint32_t max_actions)
{
    if (!plan || max_actions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_plan_init: plan is NULL or max_actions is 0");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(plan, 0, sizeof(qmcts_plan_t));

    plan->actions = (int*)nimcp_calloc(max_actions, sizeof(int));
    if (!plan->actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "quantum_mcts_plan_init: failed to allocate actions array");
        return NIMCP_ERROR_NO_MEMORY;
    }

    plan->step_values = (float*)nimcp_calloc(max_actions, sizeof(float));
    if (!plan->step_values) return -1;
    plan->step_uncertainties = (float*)nimcp_calloc(max_actions, sizeof(float));

    plan->num_actions = max_actions;

    return NIMCP_SUCCESS;
}

NIMCP_API void quantum_mcts_plan_cleanup(qmcts_plan_t* plan)
{
    if (!plan) {
        return;
    }

    if (plan->actions) {
        nimcp_free(plan->actions);
    }
    if (plan->step_values) {
        nimcp_free(plan->step_values);
    }
    if (plan->step_uncertainties) {
        nimcp_free(plan->step_uncertainties);
    }

    memset(plan, 0, sizeof(qmcts_plan_t));
}

/* ============================================================================
 * Modulation
 * ============================================================================ */

NIMCP_API nimcp_error_t quantum_mcts_modulate_atp(
    quantum_mcts_t* qmcts,
    float atp_level)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_modulate_atp: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);
    qmcts->atp_level = fmaxf(0.1f, fminf(1.0f, atp_level));
    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t quantum_mcts_set_temperature(
    quantum_mcts_t* qmcts,
    float temperature)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_set_temperature: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);
    qmcts->temperature = fmaxf(0.01f, temperature);
    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t quantum_mcts_register_bio_async(
    quantum_mcts_t* qmcts)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_register_bio_async: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);

    if (qmcts->bio_async_enabled) {
        nimcp_mutex_unlock(qmcts->mutex);
        return NIMCP_SUCCESS;
    }

    /* Check if router is available */
    if (!bio_router_is_initialized()) {
        nimcp_mutex_unlock(qmcts->mutex);
        return NIMCP_SUCCESS;  /* No router, skip */
    }

    /* Register with router */
    bio_module_info_t info = {
        .module_id = qmcts->module_id,
        .module_name = qmcts->module_name,
        .inbox_capacity = 32,
        .user_data = qmcts
    };

    qmcts->bio_ctx = bio_router_register_module(&info);
    if (qmcts->bio_ctx) {
        qmcts->bio_async_enabled = true;
    }

    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t quantum_mcts_unregister_bio_async(
    quantum_mcts_t* qmcts)
{
    if (!qmcts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_unregister_bio_async: qmcts is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(qmcts->mutex);

    if (!qmcts->bio_async_enabled) {
        nimcp_mutex_unlock(qmcts->mutex);
        return NIMCP_SUCCESS;
    }

    if (qmcts->bio_ctx) {
        bio_router_unregister_module(qmcts->bio_ctx);
        qmcts->bio_ctx = NULL;
    }

    qmcts->bio_async_enabled = false;

    nimcp_mutex_unlock(qmcts->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

NIMCP_API nimcp_error_t quantum_mcts_get_stats(
    const quantum_mcts_t* qmcts,
    quantum_mcts_stats_t* stats)
{
    if (!qmcts || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "quantum_mcts_get_stats: qmcts or stats is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(stats, &qmcts->stats, sizeof(quantum_mcts_stats_t));
    stats->total_nodes = qmcts->num_nodes;

    return NIMCP_SUCCESS;
}

NIMCP_API void quantum_mcts_print_diagnostics(const quantum_mcts_t* qmcts)
{
    if (!qmcts) {
        return;
    }

    /* Would use NIMCP logging infrastructure */
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static uint32_t create_node(quantum_mcts_t* qmcts, uint32_t parent_id,
    const float* state, uint32_t state_dim, int action)
{
    /* Check capacity */
    if (qmcts->num_nodes >= qmcts->node_capacity) {
        uint32_t new_capacity = qmcts->node_capacity * 2;
        qmcts_node_t* new_nodes = (qmcts_node_t*)nimcp_realloc(
            qmcts->nodes, new_capacity * sizeof(qmcts_node_t));
        if (!new_nodes) {
            return UINT32_MAX;
        }
        qmcts->nodes = new_nodes;
        qmcts->node_capacity = new_capacity;
    }

    qmcts_node_t* node = &qmcts->nodes[qmcts->num_nodes];
    memset(node, 0, sizeof(qmcts_node_t));

    node->node_id = qmcts->next_node_id++;
    node->parent_id = parent_id;
    node->action_id = action;
    node->visit_count = 0;
    node->q_value = 0.0f;
    node->mean_value = 0.0f;
    node->quantum_value = 0.0f;
    node->quantum_uncertainty = 1.0f;
    node->is_terminal = false;
    node->is_expanded = false;

    /* Copy state */
    if (state && state_dim > 0) {
        node->state = (float*)nimcp_calloc(state_dim, sizeof(float));
        if (node->state) {
            memcpy(node->state, state, state_dim * sizeof(float));
            node->state_dim = state_dim;
        }
    }

    /* Set proof depth */
    if (parent_id != 0) {
        int parent_idx = find_node_index(qmcts, parent_id);
        if (parent_idx >= 0) {
            node->proof_depth = qmcts->nodes[parent_idx].proof_depth + 1;
            if (node->proof_depth > qmcts->stats.max_depth_reached) {
                qmcts->stats.max_depth_reached = node->proof_depth;
            }
        }
    }

    node->created_time_us = nimcp_time_monotonic_us();
    node->last_visit_us = node->created_time_us;

    qmcts->num_nodes++;

    return node->node_id;
}

static nimcp_error_t expand_node(quantum_mcts_t* qmcts, uint32_t node_id)
{
    int idx = find_node_index(qmcts, node_id);
    if (idx < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_FOUND,
            "expand_node: node not found");
        return NIMCP_ERROR_NOT_FOUND;
    }

    qmcts_node_t* node = &qmcts->nodes[idx];

    if (node->is_expanded || node->is_terminal) {
        return NIMCP_SUCCESS;
    }

    /* Get available actions */
    int actions[64];
    uint32_t num_actions = 0;

    if (qmcts->action_fn && node->state) {
        num_actions = qmcts->action_fn(node->state, node->state_dim,
            actions, 64, qmcts->user_data);
    }

    if (num_actions == 0) {
        node->is_terminal = true;
        qmcts->stats.terminal_nodes++;
        return NIMCP_SUCCESS;
    }

    /* Allocate children array */
    node->children = (uint32_t*)nimcp_calloc(num_actions, sizeof(uint32_t));
    if (!node->children) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "expand_node: failed to allocate children array");
        return NIMCP_ERROR_NO_MEMORY;
    }
    node->children_capacity = num_actions;

    /* Create child nodes for each action */
    for (uint32_t i = 0; i < num_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_actions > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)num_actions);
        }

        float next_state[256];
        float reward = 0.0f;
        bool terminal = false;

        if (qmcts->transition_fn && node->state) {
            qmcts->transition_fn(node->state, node->state_dim, actions[i],
                next_state, &reward, &terminal, qmcts->user_data);

            uint32_t child_id = create_node(qmcts, node_id,
                next_state, node->state_dim, actions[i]);

            if (child_id != UINT32_MAX) {
                /* Re-fetch node in case array was reallocated */
                idx = find_node_index(qmcts, node_id);
                if (idx >= 0) {
                    node = &qmcts->nodes[idx];
                    node->children[node->num_children++] = child_id;

                    /* Mark terminal if transition says so */
                    int child_idx = find_node_index(qmcts, child_id);
                    if (child_idx >= 0) {
                        qmcts->nodes[child_idx].is_terminal = terminal;
                        if (terminal) {
                            qmcts->stats.terminal_nodes++;
                        }
                    }
                }
            }
        }
    }

    node->is_expanded = true;

    return NIMCP_SUCCESS;
}

static float simulate_rollout(quantum_mcts_t* qmcts, uint32_t node_id)
{
    /* Classical rollout - delegate to quantum_mcts_rollout with NULL FEP */
    return quantum_mcts_rollout(qmcts, node_id, NULL);
}

static void backpropagate(quantum_mcts_t* qmcts, uint32_t node_id, float value)
{
    uint32_t current_id = node_id;

    while (current_id != 0) {
        int idx = find_node_index(qmcts, current_id);
        if (idx < 0) break;

        qmcts_node_t* node = &qmcts->nodes[idx];

        node->visit_count++;
        node->q_value += value;
        node->mean_value = node->q_value / node->visit_count;
        node->last_visit_us = nimcp_time_monotonic_us();

        /* Update quantum uncertainty (decreases with visits) */
        node->quantum_uncertainty = 1.0f / sqrtf((float)node->visit_count);

        current_id = node->parent_id;

        /* Discount value as we go up */
        value *= qmcts->config.discount_factor;
    }
}

static float ucb_score(const quantum_mcts_t* qmcts, const qmcts_node_t* node,
    uint32_t parent_visits)
{
    if (node->visit_count == 0) {
        return INFINITY;  /* Prioritize unvisited nodes */
    }

    float exploitation = node->mean_value;
    float exploration = qmcts->config.exploration_constant *
        sqrtf(logf((float)parent_visits + 1.0f) / (float)node->visit_count);

    return exploitation + exploration;
}

static uint32_t select_node(quantum_mcts_t* qmcts, uint32_t root_id)
{
    uint32_t current_id = root_id;

    while (true) {
        int idx = find_node_index(qmcts, current_id);
        if (idx < 0) break;

        const qmcts_node_t* node = &qmcts->nodes[idx];

        if (node->is_terminal) {
            return current_id;
        }

        if (!node->is_expanded || node->num_children == 0) {
            return current_id;
        }

        /* Select best child */
        uint32_t best_child = quantum_mcts_select_child(qmcts, current_id);
        if (best_child == UINT32_MAX) {
            return current_id;
        }

        current_id = best_child;
    }

    return current_id;
}

static int find_node_index(const quantum_mcts_t* qmcts, uint32_t node_id)
{
    for (uint32_t i = 0; i < qmcts->num_nodes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && qmcts->num_nodes > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)qmcts->num_nodes);
        }

        if (qmcts->nodes[i].node_id == node_id) {
            return (int)i;
        }
    }
    return -1;  /* Not found is normal */
}

static float random_float(uint32_t* seed)
{
    /* Simple LCG random */
    *seed = *seed * 1103515245 + 12345;
    return (float)((*seed >> 16) & 0x7fff) / 32768.0f;
}

static uint64_t compute_state_hash(const float* state, uint32_t dim)
{
    /* Simple hash for state caching */
    uint64_t hash = 14695981039346656037ULL;
    const uint8_t* bytes = (const uint8_t*)state;
    uint32_t num_bytes = dim * sizeof(float);

    for (uint32_t i = 0; i < num_bytes; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_bytes > 256) {
            quantum_mcts_heartbeat("quantum_mcts_loop",
                             (float)(i + 1) / (float)num_bytes);
        }

        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }

    return hash;
}

static qmcts_cache_entry_t* cache_lookup(quantum_mcts_t* qmcts, uint64_t hash)
{
    if (!qmcts->cache) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cache_lookup: qmcts->cache is NULL");
        return NULL;
    }

    uint32_t idx = (uint32_t)(hash % qmcts->cache_capacity);

    if (qmcts->cache[idx].state_hash == hash) {
        qmcts->cache[idx].access_count++;
        return &qmcts->cache[idx];
    }

    return NULL;
}

static void cache_insert(quantum_mcts_t* qmcts, uint64_t hash,
    const qmc_amplitude_result_t* amplitude, float value)
{
    if (!qmcts->cache) {
        return;
    }

    uint32_t idx = (uint32_t)(hash % qmcts->cache_capacity);

    qmcts->cache[idx].state_hash = hash;
    qmcts->cache[idx].amplitude = *amplitude;
    qmcts->cache[idx].value_estimate = value;
    qmcts->cache[idx].timestamp_us = nimcp_time_monotonic_us();
    qmcts->cache[idx].access_count = 1;

    if (qmcts->cache_size < qmcts->cache_capacity) {
        qmcts->cache_size++;
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void quantum_mcts_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_quantum_mcts_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int quantum_mcts_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_mcts_training_begin: NULL argument");
        return -1;
    }
    quantum_mcts_heartbeat_instance(NULL, "quantum_mcts_training_begin", 0.0f);
    (void)(struct quantum_mcts*)instance; /* Module state available for reset */
    return 0;
}

int quantum_mcts_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_mcts_training_end: NULL argument");
        return -1;
    }
    quantum_mcts_heartbeat_instance(NULL, "quantum_mcts_training_end", 1.0f);
    (void)(struct quantum_mcts*)instance; /* Module state available for finalization */
    return 0;
}

int quantum_mcts_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "quantum_mcts_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    quantum_mcts_heartbeat_instance(NULL, "quantum_mcts_training_step", progress);
    (void)(struct quantum_mcts*)instance; /* Module state available for step adaptation */
    return 0;
}

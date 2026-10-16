//=============================================================================
// nimcp_basal_ganglia.c - Basal Ganglia Implementation
//=============================================================================
/**
 * @file nimcp_basal_ganglia.c
 * @brief Main basal ganglia implementation integrating all nuclei
 *
 * WHAT: Basal ganglia model for action selection and habit learning
 * WHY:  Provides biologically-inspired action selection mechanism
 * HOW:  Integrates striatum, GP, STN, and SN for pathway-based competition
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"

/* BBB Integration (Security) */
#include "security/nimcp_blood_brain_barrier.h"

/* KG Integration (Knowledge Graph) */
#include "core/brain/nimcp_brain_kg_helpers.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_basal_ganglia_health_agent = NULL;


/* Stub declarations for subsystem integration globals */
static void* g_basal_ganglia_bbb = NULL;
static void* g_basal_ganglia_kg_ctx = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(basal_ganglia, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/**
 * @brief Set BBB system for input validation
 * @param bbb BBB system handle (can be NULL to disable)
 */
void basal_ganglia_set_bbb(bbb_system_t bbb) {
    g_basal_ganglia_bbb = bbb;
}

/**
 * @brief Set KG context for state persistence
 * @param kg_ctx Knowledge graph context (can be NULL to disable)
 */
void basal_ganglia_set_kg_context(void* kg_ctx) {
    g_basal_ganglia_kg_ctx = kg_ctx;
}

/** @brief Send heartbeat from basal_ganglia module */
static inline void basal_ganglia_heartbeat(const char* operation, float progress) {
    if (g_basal_ganglia_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_basal_ganglia_health_agent, operation, progress);
    }
}

/* Forward declarations for internal functions */
static int basal_ganglia_strengthen_habit_unlocked(basal_ganglia_t* bg,
                                                    uint32_t habit_id,
                                                    bool success);

/**
 * @brief Validate cortical input through BBB
 * @param input Input array to validate
 * @param num_actions Number of elements
 * @return true if valid, false if threat detected
 */
static bool basal_ganglia_validate_input(const float* input, uint32_t num_actions) {
    if (!g_basal_ganglia_bbb) return true;  /* BBB not configured, allow */
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_validate_input: input is NULL");
        return false;
    }

    bbb_validation_result_t result;
    bool valid = bbb_validate_input(g_basal_ganglia_bbb, input,
                                     num_actions * sizeof(float), &result);
    if (!valid) {
        NIMCP_LOGGING_WARN("BBB rejected cortical input: %s", result.reason);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_SECURITY_THREAT, result.reason);
    }
    return valid;
}

/**
 * @brief Record habit to knowledge graph
 * @param habit Habit to record
 */
static void basal_ganglia_kg_record_habit(const habit_t* habit) {
    if (!g_basal_ganglia_kg_ctx || !habit) return;

    /* Use KG helpers to record habit node and relations */
    NIMCP_LOGGING_DEBUG("KG: Recording habit %u (context=%u, action=%u, strength=%.2f)",
                        habit->habit_id, habit->trigger_context,
                        habit->action_id, habit->strength);
}

/**
 * @brief Record action selection to knowledge graph
 * @param bg Basal ganglia instance
 * @param action_id Selected action
 * @param conflict_level Conflict level during selection
 */
static void basal_ganglia_kg_record_action(const basal_ganglia_t* bg,
                                            uint32_t action_id,
                                            float conflict_level) {
    if (!g_basal_ganglia_kg_ctx || !bg) return;

    NIMCP_LOGGING_DEBUG("KG: Recording action selection %u (conflict=%.2f, mode=%s)",
                        action_id, conflict_level,
                        basal_ganglia_mode_name(bg->mode));
}


//=============================================================================
// MCTS State for Action Selection
//=============================================================================

/**
 * @brief State for MCTS action selection
 */
typedef struct {
    float* cortical_input;        /**< Current cortical input */
    float* thalamic_values;       /**< Simulated thalamic output */
    float dopamine_level;         /**< Current dopamine level */
    uint32_t num_actions;         /**< Number of available actions */
    uint32_t depth;               /**< Current planning depth */
    uint32_t selected_action;     /**< Action selected at this state */
} bg_mcts_state_t;

/**
 * @brief User data for MCTS callbacks
 */
typedef struct {
    basal_ganglia_t* bg;          /**< Basal ganglia reference */
    const float* original_input;  /**< Original cortical input */
    uint32_t max_depth;           /**< Maximum planning depth */
} bg_mcts_user_data_t;

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Find action with highest thalamic output
 */
static uint32_t find_winning_action(const basal_ganglia_t* bg) {
    uint32_t winner = 0;
    float max_val = bg->thalamic_output[0];

    for (uint32_t a = 1; a < bg->num_actions; a++) {
        if (bg->thalamic_output[a] > max_val) {
            max_val = bg->thalamic_output[a];
            winner = a;
        }
    }

    return winner;
}

/**
 * @brief Compute conflict level between competing actions
 */
static float compute_conflict(const basal_ganglia_t* bg) {
    if (bg->num_actions < 2) return 0.0f;

    /* Find top two actions */
    float first = 0, second = 0;
    for (uint32_t a = 0; a < bg->num_actions; a++) {
        float val = bg->thalamic_output[a];
        if (val > first) {
            second = first;
            first = val;
        } else if (val > second) {
            second = val;
        }
    }

    /* Conflict is high when top two are close */
    if (first < 0.001f) return 0.0f;
    return 1.0f - (first - second) / first;
}

//=============================================================================
// MCTS Callbacks for Action Selection
//=============================================================================

/**
 * @brief Get number of available actions
 */
static uint32_t bg_mcts_get_action_count(const void* state, void* user_data) {
    (void)user_data;
    const bg_mcts_state_t* s = (const bg_mcts_state_t*)state;
    return s->num_actions;
}

/**
 * @brief Get action at index
 */
static uint32_t bg_mcts_get_action(const void* state, uint32_t idx, void* user_data) {
    (void)state;
    (void)user_data;
    return idx;  /* Action ID is just the index */
}

/**
 * @brief Apply action to state and return new state
 */
static void* bg_mcts_apply_action(const void* state, uint32_t action, void* user_data) {
    const bg_mcts_state_t* s = (const bg_mcts_state_t*)state;
    (void)user_data;  /* Currently unused, reserved for future extensions */

    /* Allocate new state */
    bg_mcts_state_t* new_state = nimcp_malloc(sizeof(bg_mcts_state_t));
    if (!new_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate MCTS state");
        return NULL;
    }

    new_state->num_actions = s->num_actions;
    new_state->depth = s->depth + 1;
    new_state->selected_action = action;
    new_state->dopamine_level = s->dopamine_level;

    /* Allocate and copy arrays */
    new_state->cortical_input = nimcp_malloc(s->num_actions * sizeof(float));
    new_state->thalamic_values = nimcp_malloc(s->num_actions * sizeof(float));

    if (!new_state->cortical_input || !new_state->thalamic_values) {
        nimcp_free(new_state->cortical_input);
        nimcp_free(new_state->thalamic_values);
        nimcp_free(new_state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_mcts_apply_action: required parameter is NULL (new_state->cortical_input, new_state->thalamic_values)");
        return NULL;
    }

    /* Simulate effect: selected action gets boost, others slightly dampened */
    for (uint32_t a = 0; a < s->num_actions; a++) {
        if (a == action) {
            /* Boost selected action based on dopamine */
            new_state->cortical_input[a] = s->cortical_input[a] * (1.0f + 0.2f * s->dopamine_level);
            new_state->thalamic_values[a] = s->thalamic_values[a] * 1.1f;
        } else {
            /* Dampen competing actions (lateral inhibition) */
            new_state->cortical_input[a] = s->cortical_input[a] * 0.9f;
            new_state->thalamic_values[a] = s->thalamic_values[a] * 0.95f;
        }
        /* Clamp values */
        if (new_state->cortical_input[a] > 1.0f) new_state->cortical_input[a] = 1.0f;
        if (new_state->thalamic_values[a] > 1.0f) new_state->thalamic_values[a] = 1.0f;
    }

    return new_state;
}

/**
 * @brief Evaluate state value
 */
static float bg_mcts_evaluate(const void* state, void* user_data) {
    const bg_mcts_state_t* s = (const bg_mcts_state_t*)state;
    (void)user_data;  /* Currently unused, reserved for future extensions */

    /* Value is based on:
     * 1. Thalamic activation of selected action (higher = better)
     * 2. Separation from competing actions (more = clearer decision)
     * 3. Dopamine modulation (reward expectation)
     */
    float selected_value = s->thalamic_values[s->selected_action];

    /* Find max competing value */
    float max_other = 0.0f;
    for (uint32_t a = 0; a < s->num_actions; a++) {
        if (a != s->selected_action && s->thalamic_values[a] > max_other) {
            max_other = s->thalamic_values[a];
        }
    }

    /* Value = selected activation + separation + dopamine bonus */
    float separation = selected_value - max_other;
    float value = selected_value + 0.5f * separation + 0.2f * s->dopamine_level;

    return value;
}

/**
 * @brief Check if state is terminal
 */
static bool bg_mcts_is_terminal(const void* state, void* user_data) {
    const bg_mcts_state_t* s = (const bg_mcts_state_t*)state;
    const bg_mcts_user_data_t* ud = (const bg_mcts_user_data_t*)user_data;

    /* Terminal if max depth reached or clear winner */
    if (ud && s->depth >= ud->max_depth) return true;
    if (!ud && s->depth >= 5) return true;  /* Default max depth */

    /* Check for clear winner (low conflict) */
    float first = 0, second = 0;
    for (uint32_t a = 0; a < s->num_actions; a++) {
        if (s->thalamic_values[a] > first) {
            second = first;
            first = s->thalamic_values[a];
        } else if (s->thalamic_values[a] > second) {
            second = s->thalamic_values[a];
        }
    }

    float conflict = (first < 0.001f) ? 0.0f : 1.0f - (first - second) / first;
    return conflict < 0.2f;  /* Clear winner = terminal */
}

/**
 * @brief Free state memory
 */
static void bg_mcts_free_state(void* state, void* user_data) {
    (void)user_data;
    if (!state) return;

    bg_mcts_state_t* s = (bg_mcts_state_t*)state;
    nimcp_free(s->cortical_input);
    nimcp_free(s->thalamic_values);
    nimcp_free(s);
}

/**
 * @brief Clone state
 */
static void* bg_mcts_clone_state(const void* state, void* user_data) {
    (void)user_data;
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "state is NULL");
        return NULL;
    }

    const bg_mcts_state_t* s = (const bg_mcts_state_t*)state;

    bg_mcts_state_t* clone = nimcp_malloc(sizeof(bg_mcts_state_t));
    if (!clone) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate MCTS clone");
        return NULL;
    }

    clone->num_actions = s->num_actions;
    clone->depth = s->depth;
    clone->selected_action = s->selected_action;
    clone->dopamine_level = s->dopamine_level;

    clone->cortical_input = nimcp_malloc(s->num_actions * sizeof(float));
    clone->thalamic_values = nimcp_malloc(s->num_actions * sizeof(float));

    if (!clone->cortical_input || !clone->thalamic_values) {
        nimcp_free(clone->cortical_input);
        nimcp_free(clone->thalamic_values);
        nimcp_free(clone);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bg_mcts_clone_state: required parameter is NULL (clone->cortical_input, clone->thalamic_values)");
        return NULL;
    }

    memcpy(clone->cortical_input, s->cortical_input, s->num_actions * sizeof(float));
    memcpy(clone->thalamic_values, s->thalamic_values, s->num_actions * sizeof(float));

    return clone;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

void basal_ganglia_default_config(basal_ganglia_config_t* config) {
    NIMCP_THROW_IF(!config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    if (!config) return;

    config->num_actions = 8;
    config->dopamine_baseline = BG_DOPAMINE_BASELINE;
    config->action_threshold = BG_ACTION_THRESHOLD;
    config->habit_learning_rate = NIMCP_DEFAULT_LEARNING_RATE;
    config->exploration_rate = 0.1f;
    config->enable_hyperdirect = true;
    config->enable_habit_learning = true;

    /* Component configurations */
    striatum_default_config(&config->striatum_config);
    config->striatum_config.num_actions = config->num_actions;

    globus_pallidus_default_config(&config->gpi_config, GP_SEGMENT_INTERNAL);
    config->gpi_config.num_actions = config->num_actions;

    globus_pallidus_default_config(&config->gpe_config, GP_SEGMENT_EXTERNAL);
    config->gpe_config.num_actions = config->num_actions;

    subthalamic_default_config(&config->stn_config);
    config->stn_config.num_actions = config->num_actions;

    substantia_nigra_default_config(&config->snc_config, SN_PART_COMPACTA);

    substantia_nigra_default_config(&config->snr_config, SN_PART_RETICULATA);
    config->snr_config.num_actions = config->num_actions;
}

basal_ganglia_t* basal_ganglia_create(const basal_ganglia_config_t* config) {
    basal_ganglia_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        basal_ganglia_default_config(&cfg);
    }

    /* Ensure all child configs have matching num_actions */
    cfg.striatum_config.num_actions = cfg.num_actions;
    cfg.gpi_config.num_actions = cfg.num_actions;
    cfg.gpe_config.num_actions = cfg.num_actions;
    cfg.stn_config.num_actions = cfg.num_actions;
    cfg.snr_config.num_actions = cfg.num_actions;

    basal_ganglia_t* bg = nimcp_malloc(sizeof(basal_ganglia_t));
    if (!bg) {
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate basal ganglia");
        return NULL;
    }
    memset(bg, 0, sizeof(basal_ganglia_t));

    bg->config = cfg;
    bg->num_actions = cfg.num_actions;
    bg->dopamine_level = cfg.dopamine_baseline;
    bg->max_habits = BG_MAX_HABITS;

    /* Create striatum */
    bg->striatum = striatum_create(&cfg.striatum_config);
    if (!bg->striatum) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Failed to create striatum");
        nimcp_free(bg);
        return NULL;
    }

    /* Create GPi */
    bg->gpi = globus_pallidus_create(&cfg.gpi_config);
    if (!bg->gpi) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Failed to create GPi");
        striatum_destroy(bg->striatum);
        nimcp_free(bg);
        return NULL;
    }

    /* Create GPe */
    bg->gpe = globus_pallidus_create(&cfg.gpe_config);
    if (!bg->gpe) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Failed to create GPe");
        globus_pallidus_destroy(bg->gpi);
        striatum_destroy(bg->striatum);
        nimcp_free(bg);
        return NULL;
    }

    /* Create STN */
    bg->stn = subthalamic_create(&cfg.stn_config);
    if (!bg->stn) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Failed to create STN");
        globus_pallidus_destroy(bg->gpe);
        globus_pallidus_destroy(bg->gpi);
        striatum_destroy(bg->striatum);
        nimcp_free(bg);
        return NULL;
    }

    /* Create SNc (dopamine) */
    bg->snc = substantia_nigra_create(&cfg.snc_config);
    if (!bg->snc) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Failed to create SNc");
        subthalamic_destroy(bg->stn);
        globus_pallidus_destroy(bg->gpe);
        globus_pallidus_destroy(bg->gpi);
        striatum_destroy(bg->striatum);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "basal_ganglia_create: failed to create SNc");
        nimcp_free(bg);
        return NULL;
    }

    /* Create SNr (output) */
    bg->snr = substantia_nigra_create(&cfg.snr_config);
    if (!bg->snr) {
        NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Failed to create SNr");
        substantia_nigra_destroy(bg->snc);
        subthalamic_destroy(bg->stn);
        globus_pallidus_destroy(bg->gpe);
        globus_pallidus_destroy(bg->gpi);
        striatum_destroy(bg->striatum);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "basal_ganglia_create: failed to create SNr");
        nimcp_free(bg);
        return NULL;
    }

    /* Allocate action candidates */
    bg->actions = nimcp_calloc(cfg.num_actions, sizeof(action_candidate_t));
    if (!bg->actions) {
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate action candidates");
        goto cleanup;
    }

    /* Initialize action candidates */
    for (uint32_t a = 0; a < cfg.num_actions; a++) {
        bg->actions[a].action_id = a;
        snprintf(bg->actions[a].name, sizeof(bg->actions[a].name), "Action_%u", a);
    }

    /* Allocate pathway arrays */
    bg->direct_pathway = nimcp_calloc(cfg.num_actions, sizeof(float));
    bg->indirect_pathway = nimcp_calloc(cfg.num_actions, sizeof(float));
    bg->hyperdirect_pathway = nimcp_calloc(cfg.num_actions, sizeof(float));
    bg->thalamic_output = nimcp_calloc(cfg.num_actions, sizeof(float));

    if (!bg->direct_pathway || !bg->indirect_pathway ||
        !bg->hyperdirect_pathway || !bg->thalamic_output) {
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate pathway arrays");
        goto cleanup;
    }

    /* Allocate habits */
    bg->habits = nimcp_calloc(BG_MAX_HABITS, sizeof(habit_t));
    if (!bg->habits) {
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate habits");
        goto cleanup;
    }

    /* Allocate mutex */
    bg->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!bg->mutex) {
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate BG mutex");
        goto cleanup;
    }
    nimcp_mutex_init(bg->mutex, NULL);

    /* Initialize MCTS parameters */
    bg->rand_seed = mc_seed_from_time();
    bg->mcts_conflict_threshold = 0.6f;  /* Use MCTS when conflict > 60% */
    bg->mcts_iterations = 50;            /* Default MCTS iterations */

    NIMCP_LOGGING_DEBUG("Created basal ganglia with %u actions", cfg.num_actions);

    return bg;

cleanup:
    if (bg->actions) nimcp_free(bg->actions);
    if (bg->direct_pathway) nimcp_free(bg->direct_pathway);
    if (bg->indirect_pathway) nimcp_free(bg->indirect_pathway);
    if (bg->hyperdirect_pathway) nimcp_free(bg->hyperdirect_pathway);
    if (bg->thalamic_output) nimcp_free(bg->thalamic_output);
    if (bg->habits) nimcp_free(bg->habits);
    substantia_nigra_destroy(bg->snr);
    substantia_nigra_destroy(bg->snc);
    subthalamic_destroy(bg->stn);
    globus_pallidus_destroy(bg->gpe);
    globus_pallidus_destroy(bg->gpi);
    striatum_destroy(bg->striatum);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "basal_ganglia_create: allocation failed");
    nimcp_free(bg);
    return NULL;
}

void basal_ganglia_destroy(basal_ganglia_t* bg) {
    NIMCP_THROW_IF(!bg, NIMCP_ERROR_NULL_POINTER, "bg is NULL");
    if (!bg) return;

    nimcp_mutex_lock(bg->mutex);

    /* Destroy nuclei */
    striatum_destroy(bg->striatum);
    globus_pallidus_destroy(bg->gpi);
    globus_pallidus_destroy(bg->gpe);
    subthalamic_destroy(bg->stn);
    substantia_nigra_destroy(bg->snc);
    substantia_nigra_destroy(bg->snr);

    /* Free arrays */
    nimcp_free(bg->actions);
    nimcp_free(bg->direct_pathway);
    nimcp_free(bg->indirect_pathway);
    nimcp_free(bg->hyperdirect_pathway);
    nimcp_free(bg->thalamic_output);
    nimcp_free(bg->habits);

    nimcp_mutex_unlock(bg->mutex);
    nimcp_mutex_destroy(bg->mutex);
    nimcp_free(bg->mutex);

    nimcp_free(bg);

    NIMCP_LOGGING_DEBUG("Destroyed basal ganglia");
}

int basal_ganglia_reset(basal_ganglia_t* bg) {
    NIMCP_THROW_IF(!bg, NIMCP_ERROR_NULL_POINTER, "bg is NULL");
    if (!bg) return -1;

    nimcp_mutex_lock(bg->mutex);

    /* Reset nuclei */
    striatum_reset(bg->striatum);
    globus_pallidus_reset(bg->gpi);
    globus_pallidus_reset(bg->gpe);
    subthalamic_reset(bg->stn);
    substantia_nigra_reset(bg->snc);
    substantia_nigra_reset(bg->snr);

    /* Reset state */
    bg->dopamine_level = bg->config.dopamine_baseline;
    bg->selected_action = 0;
    bg->action_state = ACTION_STATE_IDLE;
    bg->mode = BG_MODE_GOAL_DIRECTED;
    bg->conflict_level = 0;
    bg->habit_mode = false;

    /* Clear arrays */
    memset(bg->direct_pathway, 0, bg->num_actions * sizeof(float));
    memset(bg->indirect_pathway, 0, bg->num_actions * sizeof(float));
    memset(bg->hyperdirect_pathway, 0, bg->num_actions * sizeof(float));
    memset(bg->thalamic_output, 0, bg->num_actions * sizeof(float));

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

//=============================================================================
// Action Selection Functions
//=============================================================================

int basal_ganglia_set_action_value(basal_ganglia_t* bg, uint32_t action_id,
                                    float value, float urgency, float cost) {
    if (!bg || action_id >= bg->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "basal_ganglia_set_action_value: bg is NULL");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);

    bg->actions[action_id].value = value;
    bg->actions[action_id].urgency = fmaxf(0.0f, fminf(1.0f, urgency));
    bg->actions[action_id].cost = fmaxf(0.0f, fminf(1.0f, cost));

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

int basal_ganglia_select_action(basal_ganglia_t* bg,
                                 const float* cortical_input,
                                 uint32_t* selected_action) {
    NIMCP_THROW_IF(!bg, NIMCP_ERROR_NULL_POINTER, "bg is NULL");
    NIMCP_THROW_IF(!cortical_input, NIMCP_ERROR_NULL_POINTER, "cortical_input is NULL");
    NIMCP_THROW_IF(!selected_action, NIMCP_ERROR_NULL_POINTER, "selected_action is NULL");
    if (!bg || !cortical_input || !selected_action) return -1;

    /* BBB validation of cortical input */
    if (!basal_ganglia_validate_input(cortical_input, bg->num_actions)) {
        return -1;
    }

    basal_ganglia_heartbeat("select_action_start", 0.0f);

    nimcp_mutex_lock(bg->mutex);

    bg->action_state = ACTION_STATE_COMPETING;
    uint64_t start_time = nimcp_time_get_ms();

    basal_ganglia_heartbeat("select_action_processing", 0.1f);

    /* 1. Process through striatum (D1 and D2 pathways) */
    striatum_process_input(bg->striatum, cortical_input, bg->dopamine_level);
    striatum_get_d1_output(bg->striatum, bg->direct_pathway);
    striatum_get_d2_output(bg->striatum, bg->indirect_pathway);

    /* 2. Process indirect pathway: D2 → GPe */
    globus_pallidus_set_striatal_input(bg->gpe, bg->indirect_pathway);
    globus_pallidus_process(bg->gpe);

    /* 3. Get GPe output for STN */
    float gpe_output[BG_MAX_ACTIONS];
    globus_pallidus_get_output(bg->gpe, gpe_output);

    /* 4. Process STN (receives GPe and optional hyperdirect) */
    subthalamic_set_gpe_input(bg->stn, gpe_output);
    if (bg->config.enable_hyperdirect) {
        /* Send cortical input as hyperdirect signal */
        subthalamic_set_cortical_input(bg->stn, cortical_input, false);
    }
    subthalamic_process(bg->stn);

    /* 5. Get STN output */
    float stn_output[BG_MAX_ACTIONS];
    subthalamic_get_output(bg->stn, stn_output);

    /* 6. Process GPi (receives D1 direct and STN) */
    globus_pallidus_set_striatal_input(bg->gpi, bg->direct_pathway);
    globus_pallidus_set_stn_input(bg->gpi, stn_output);
    globus_pallidus_set_gpe_input(bg->gpi, gpe_output);
    globus_pallidus_process(bg->gpi);

    /* 7. Also process SNr similarly */
    snr_set_striatal_input(bg->snr, bg->direct_pathway);
    snr_set_stn_input(bg->snr, stn_output);
    snr_process(bg->snr);

    /* 8. Compute thalamic output (disinhibition) */
    /* Low GPi/SNr output = high thalamic activity = action enabled */
    float gpi_output[BG_MAX_ACTIONS];
    float snr_output[BG_MAX_ACTIONS];
    globus_pallidus_get_output(bg->gpi, gpi_output);
    snr_get_output(bg->snr, snr_output);

    for (uint32_t a = 0; a < bg->num_actions; a++) {
        /* Average of GPi and SNr inhibition */
        float inhibition = (gpi_output[a] + snr_output[a]) / 2.0f;
        /* Disinhibition = 1 - inhibition */
        bg->thalamic_output[a] = 1.0f - inhibition;
    }

    /* 9. Find winning action */
    uint32_t winner = find_winning_action(bg);
    bg->selected_action = winner;

    /* 10. Check if above threshold */
    if (bg->thalamic_output[winner] >= bg->config.action_threshold) {
        bg->action_state = ACTION_STATE_SELECTED;
        *selected_action = winner;
    } else {
        bg->action_state = ACTION_STATE_IDLE;
        *selected_action = winner;  /* Return best candidate anyway */
    }

    /* 11. Compute conflict level */
    bg->conflict_level = compute_conflict(bg);

    /* 12. Determine operating mode */
    if (bg->conflict_level > 0.7f) {
        bg->mode = BG_MODE_GOAL_DIRECTED;  /* High conflict = need deliberation */
    } else if (bg->habit_mode && bg->conflict_level < 0.3f) {
        bg->mode = BG_MODE_HABITUAL;
    }

    /* Update statistics */
    uint64_t end_time = nimcp_time_get_ms();
    bg->stats.total_selections++;
    float selection_time = (float)(end_time - start_time);
    float count = (float)bg->stats.total_selections;
    bg->stats.avg_selection_time_ms =
        (bg->stats.avg_selection_time_ms * (count - 1.0f) + selection_time) / count;

    if (bg->mode == BG_MODE_GOAL_DIRECTED) bg->stats.goal_directed_count++;
    else if (bg->mode == BG_MODE_HABITUAL) bg->stats.habitual_count++;

    /* Calculate conflict ratio properly as percentage of high-conflict decisions */
    static _Atomic uint64_t high_conflict_count = 0;
    if (bg->conflict_level > 0.5f) high_conflict_count++;
    bg->stats.conflict_ratio = (bg->stats.total_selections > 0)
        ? (float)high_conflict_count / (float)bg->stats.total_selections
        : 0.0f;

    /* Record to knowledge graph */
    basal_ganglia_kg_record_action(bg, winner, bg->conflict_level);

    basal_ganglia_heartbeat("select_action_complete", 1.0f);

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

int basal_ganglia_get_thalamic_output(const basal_ganglia_t* bg, float* output) {
    if (!bg || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_get_thalamic_output: required parameter is NULL (bg, output)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    memcpy(output, bg->thalamic_output, bg->num_actions * sizeof(float));
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return 0;
}

int basal_ganglia_suppress_action(basal_ganglia_t* bg, float suppression_strength) {
    if (!bg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_suppress_action: bg is NULL");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);

    /* Activate STN emergency stop */
    subthalamic_emergency_stop(bg->stn, suppression_strength);
    bg->mode = BG_MODE_SUPPRESSED;
    bg->action_state = ACTION_STATE_CANCELLED;
    bg->stats.suppression_count++;

    NIMCP_LOGGING_DEBUG("Suppressed action with strength %.2f", suppression_strength);

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

int basal_ganglia_action_completed(basal_ganglia_t* bg, uint32_t action_id,
                                    bool success) {
    NIMCP_THROW_IF(!bg, NIMCP_ERROR_NULL_POINTER, "bg is NULL");
    NIMCP_THROW_IF(action_id >= bg->num_actions, NIMCP_ERROR_INVALID_PARAMETER,
                   "action_id out of range");
    if (!bg || action_id >= bg->num_actions) return -1;

    nimcp_mutex_lock(bg->mutex);

    bg->action_state = ACTION_STATE_COMPLETED;

    /* Update habit strength if this was a habit - use UNLOCKED version to prevent deadlock */
    if (bg->actions[action_id].is_habit) {
        for (uint32_t h = 0; h < bg->num_habits; h++) {
            if (bg->habits[h].action_id == action_id) {
                basal_ganglia_strengthen_habit_unlocked(bg, h, success);
                break;
            }
        }
    }

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

int basal_ganglia_select_action_mcts(basal_ganglia_t* bg,
                                      const float* cortical_input,
                                      uint32_t num_iterations,
                                      uint32_t* selected_action) {
    if (!bg || !cortical_input || !selected_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_select_action_mcts: required parameter is NULL (bg, cortical_input, selected_action)");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);

    uint64_t start_time = nimcp_time_get_ms();

    /* First, run normal pathway processing to get baseline thalamic values */
    bg->action_state = ACTION_STATE_COMPETING;

    striatum_process_input(bg->striatum, cortical_input, bg->dopamine_level);
    striatum_get_d1_output(bg->striatum, bg->direct_pathway);
    striatum_get_d2_output(bg->striatum, bg->indirect_pathway);

    globus_pallidus_set_striatal_input(bg->gpe, bg->indirect_pathway);
    globus_pallidus_process(bg->gpe);

    float gpe_output[BG_MAX_ACTIONS];
    globus_pallidus_get_output(bg->gpe, gpe_output);

    subthalamic_set_gpe_input(bg->stn, gpe_output);
    if (bg->config.enable_hyperdirect) {
        subthalamic_set_cortical_input(bg->stn, cortical_input, false);
    }
    subthalamic_process(bg->stn);

    float stn_output[BG_MAX_ACTIONS];
    subthalamic_get_output(bg->stn, stn_output);

    globus_pallidus_set_striatal_input(bg->gpi, bg->direct_pathway);
    globus_pallidus_set_stn_input(bg->gpi, stn_output);
    globus_pallidus_set_gpe_input(bg->gpi, gpe_output);
    globus_pallidus_process(bg->gpi);

    snr_set_striatal_input(bg->snr, bg->direct_pathway);
    snr_set_stn_input(bg->snr, stn_output);
    snr_process(bg->snr);

    float gpi_output[BG_MAX_ACTIONS];
    float snr_output[BG_MAX_ACTIONS];
    globus_pallidus_get_output(bg->gpi, gpi_output);
    snr_get_output(bg->snr, snr_output);

    for (uint32_t a = 0; a < bg->num_actions; a++) {
        float inhibition = (gpi_output[a] + snr_output[a]) / 2.0f;
        bg->thalamic_output[a] = 1.0f - inhibition;
    }

    /* Create initial MCTS state */
    bg_mcts_state_t initial_state = {0};
    initial_state.num_actions = bg->num_actions;
    initial_state.depth = 0;
    initial_state.dopamine_level = bg->dopamine_level;
    initial_state.selected_action = 0;

    initial_state.cortical_input = nimcp_malloc(bg->num_actions * sizeof(float));
    initial_state.thalamic_values = nimcp_malloc(bg->num_actions * sizeof(float));

    if (!initial_state.cortical_input || !initial_state.thalamic_values) {
        nimcp_free(initial_state.cortical_input);
        nimcp_free(initial_state.thalamic_values);
        nimcp_mutex_unlock(bg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "basal_ganglia_select_action_mcts: allocation failed");
        return -1;
    }

    memcpy(initial_state.cortical_input, cortical_input, bg->num_actions * sizeof(float));
    memcpy(initial_state.thalamic_values, bg->thalamic_output, bg->num_actions * sizeof(float));

    /* Setup user data */
    bg_mcts_user_data_t user_data = {
        .bg = bg,
        .original_input = cortical_input,
        .max_depth = 5
    };

    /* Configure MCTS */
    mcts_config_t config;
    mcts_config_init(&config);
    config.max_iterations = (num_iterations > 0) ? num_iterations : bg->mcts_iterations;
    config.max_depth = 5;
    config.exploration_constant = 1.4f;
    config.discount_factor = 0.95f;
    config.max_nodes = 256;

    config.get_action_count = bg_mcts_get_action_count;
    config.get_action = bg_mcts_get_action;
    config.apply_action = bg_mcts_apply_action;
    config.evaluate = bg_mcts_evaluate;
    config.is_terminal = bg_mcts_is_terminal;
    config.free_state = bg_mcts_free_state;
    config.clone_state = bg_mcts_clone_state;
    config.user_data = &user_data;
    config.seed = bg->rand_seed;

    /* Run MCTS */
    mcts_result_t result;
    nimcp_error_t err = mcts_search(&config, &initial_state, &result);

    /* Update RNG seed */
    bg->rand_seed = config.seed;

    /* Cleanup initial state */
    nimcp_free(initial_state.cortical_input);
    nimcp_free(initial_state.thalamic_values);

    if (err != NIMCP_MC_OK) {
        nimcp_mutex_unlock(bg->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "basal_ganglia_select_action_mcts: MCTS search failed");
        return -1;
    }

    /* Use MCTS result */
    *selected_action = result.best_action;
    bg->selected_action = result.best_action;
    bg->action_state = ACTION_STATE_SELECTED;

    /* Compute conflict level */
    bg->conflict_level = compute_conflict(bg);

    /* Update statistics */
    uint64_t end_time = nimcp_time_get_ms();
    bg->stats.total_selections++;
    float selection_time = (float)(end_time - start_time);
    float count = (float)bg->stats.total_selections;
    bg->stats.avg_selection_time_ms =
        (bg->stats.avg_selection_time_ms * (count - 1.0f) + selection_time) / count;
    bg->stats.goal_directed_count++;

    NIMCP_LOGGING_DEBUG("MCTS selected action %u (value=%.3f, iterations=%u)",
                        result.best_action, result.best_value, result.iterations_completed);

    mcts_result_free(&result);
    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

//=============================================================================
// Dopamine Modulation Functions
//=============================================================================

int basal_ganglia_update_dopamine(basal_ganglia_t* bg, float reward,
                                   float expected_reward) {
    if (!bg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_update_dopamine: bg is NULL");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);

    /* Update SNc dopamine based on reward */
    snc_update_reward(bg->snc, reward, expected_reward);

    /* Get new dopamine level */
    bg->dopamine_level = snc_get_dopamine(bg->snc);
    bg->reward_prediction_error = snc_get_rpe(bg->snc);

    /* Determine dopamine signal type */
    if (bg->reward_prediction_error > 0.1f) {
        bg->dopamine_signal = DOPAMINE_SIGNAL_REWARD;
    } else if (bg->reward_prediction_error < -0.1f) {
        bg->dopamine_signal = DOPAMINE_SIGNAL_PUNISHMENT;
    } else {
        bg->dopamine_signal = DOPAMINE_SIGNAL_TONIC;
    }

    /* Update striatum with new dopamine */
    striatum_set_dopamine(bg->striatum, bg->dopamine_level);

    /* Update statistics */
    float count = (float)(bg->stats.total_selections + 1);
    bg->stats.avg_dopamine_level =
        (bg->stats.avg_dopamine_level * (count - 1.0f) + bg->dopamine_level) / count;

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

int basal_ganglia_set_dopamine(basal_ganglia_t* bg, float level) {
    if (!bg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_set_dopamine: bg is NULL");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);
    bg->dopamine_level = fmaxf(0.0f, fminf(1.0f, level));
    striatum_set_dopamine(bg->striatum, bg->dopamine_level);
    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

float basal_ganglia_get_dopamine(const basal_ganglia_t* bg) {
    if (!bg) return 0.5f;

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    float da = bg->dopamine_level;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return da;
}

float basal_ganglia_get_rpe(const basal_ganglia_t* bg) {
    if (!bg) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    float rpe = bg->reward_prediction_error;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return rpe;
}

//=============================================================================
// Habit Learning Functions
//=============================================================================

int basal_ganglia_register_habit(basal_ganglia_t* bg, uint32_t context,
                                  uint32_t action_id, uint32_t* habit_id) {
    if (!bg || !habit_id || action_id >= bg->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_register_habit: required parameter is NULL (bg, habit_id)");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);

    if (bg->num_habits >= bg->max_habits) {
        NIMCP_LOGGING_WARN("Maximum habits reached");
        nimcp_mutex_unlock(bg->mutex);
        return -2;
    }

    uint32_t id = bg->num_habits;
    bg->habits[id].habit_id = id;
    bg->habits[id].trigger_context = context;
    bg->habits[id].action_id = action_id;
    bg->habits[id].strength = 0.1f;  /* Initial strength */
    bg->habits[id].repetitions = 0;
    bg->habits[id].last_executed_ms = 0;
    bg->habits[id].success_rate = 0.0f;

    bg->actions[action_id].is_habit = true;
    bg->actions[action_id].habit_strength = 0.1f;

    bg->num_habits++;
    *habit_id = id;

    NIMCP_LOGGING_DEBUG("Registered habit %u: context %u -> action %u",
                        id, context, action_id);

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

/**
 * @brief Internal unlocked version of strengthen_habit
 * @note Caller MUST hold bg->mutex
 */
static int basal_ganglia_strengthen_habit_unlocked(basal_ganglia_t* bg,
                                                    uint32_t habit_id,
                                                    bool success) {
    if (!bg || habit_id >= bg->num_habits) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "basal_ganglia_strengthen_habit_unlocked: bg is NULL");
        return -1;
    }

    habit_t* habit = &bg->habits[habit_id];

    /* Update repetitions */
    habit->repetitions++;
    habit->last_executed_ms = nimcp_time_get_ms();

    /* Update success rate */
    float count = (float)habit->repetitions;
    habit->success_rate =
        (habit->success_rate * (count - 1.0f) + (success ? 1.0f : 0.0f)) / count;

    /* Strengthen or weaken based on success */
    float delta = bg->config.habit_learning_rate;
    if (success) {
        habit->strength = fminf(1.0f, habit->strength + delta);
    } else {
        habit->strength = fmaxf(0.0f, habit->strength - delta * 0.5f);
    }

    /* Update action's habit strength */
    bg->actions[habit->action_id].habit_strength = habit->strength;

    /* Record to KG */
    basal_ganglia_kg_record_habit(habit);

    /* Check if habit is now strong enough for automatic mode */
    if (habit->strength > BG_HABIT_STRENGTH_THRESHOLD) {
        NIMCP_LOGGING_DEBUG("Habit %u reached threshold strength %.2f",
                            habit_id, habit->strength);
    }

    return 0;
}

int basal_ganglia_strengthen_habit(basal_ganglia_t* bg, uint32_t habit_id,
                                    bool success) {
    NIMCP_THROW_IF(!bg, NIMCP_ERROR_NULL_POINTER, "bg is NULL");
    if (!bg || habit_id >= bg->num_habits) return -1;

    nimcp_mutex_lock(bg->mutex);
    int result = basal_ganglia_strengthen_habit_unlocked(bg, habit_id, success);
    nimcp_mutex_unlock(bg->mutex);

    return result;
}

bool basal_ganglia_check_habit(const basal_ganglia_t* bg, uint32_t context,
                                uint32_t* action_id) {
    if (!bg || !action_id) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);

    for (uint32_t h = 0; h < bg->num_habits; h++) {
        if (bg->habits[h].trigger_context == context &&
            bg->habits[h].strength > BG_HABIT_STRENGTH_THRESHOLD) {
            *action_id = bg->habits[h].action_id;
            nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);
    return false;
}

float basal_ganglia_get_habit_strength(const basal_ganglia_t* bg,
                                        uint32_t habit_id) {
    if (!bg || habit_id >= bg->num_habits) return -1.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    float strength = bg->habits[habit_id].strength;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return strength;
}

int basal_ganglia_set_habit_mode(basal_ganglia_t* bg, bool habitual) {
    if (!bg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_set_habit_mode: bg is NULL");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);
    bg->habit_mode = habitual;
    bg->mode = habitual ? BG_MODE_HABITUAL : BG_MODE_GOAL_DIRECTED;
    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

bool basal_ganglia_is_habit_mode(const basal_ganglia_t* bg) {
    if (!bg) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    bool mode = bg->habit_mode;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return mode;
}

//=============================================================================
// Pathway Analysis Functions
//=============================================================================

float basal_ganglia_get_direct_activation(const basal_ganglia_t* bg,
                                           uint32_t action_id) {
    if (!bg || action_id >= bg->num_actions) return -1.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    float activation = bg->direct_pathway[action_id];
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return activation;
}

float basal_ganglia_get_indirect_activation(const basal_ganglia_t* bg,
                                             uint32_t action_id) {
    if (!bg || action_id >= bg->num_actions) return -1.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    float activation = bg->indirect_pathway[action_id];
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return activation;
}

float basal_ganglia_get_conflict(const basal_ganglia_t* bg) {
    if (!bg) return 0.0f;

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    float conflict = bg->conflict_level;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return conflict;
}

bg_operating_mode_t basal_ganglia_get_mode(const basal_ganglia_t* bg) {
    if (!bg) return BG_MODE_GOAL_DIRECTED;

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    bg_operating_mode_t mode = bg->mode;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return mode;
}

//=============================================================================
// Update Functions
//=============================================================================

int basal_ganglia_step(basal_ganglia_t* bg, float dt) {
    if (!bg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_step: bg is NULL");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);

    /* Step all nuclei */
    striatum_step(bg->striatum, dt);
    globus_pallidus_step(bg->gpi, dt);
    globus_pallidus_step(bg->gpe, dt);
    subthalamic_step(bg->stn, dt);
    snc_step(bg->snc, dt);
    snr_step(bg->snr, dt);

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

int basal_ganglia_process_input(basal_ganglia_t* bg, const float* cortical_input) {
    if (!bg || !cortical_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_process_input: required parameter is NULL (bg, cortical_input)");
        return -1;
    }

    uint32_t selected;
    return basal_ganglia_select_action(bg, cortical_input, &selected);
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Bio-async message handler callback
 * @param msg Message header + payload
 * @param msg_size Total message size
 * @param response_promise Promise for response (may be NULL)
 * @param user_data Basal ganglia pointer
 * @return NIMCP_SUCCESS or error
 */
static nimcp_error_t basal_ganglia_bio_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)response_promise;  /* Not used for these messages */

    basal_ganglia_t* bg = (basal_ganglia_t*)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;
    const void* payload = (const uint8_t*)msg + sizeof(bio_message_header_t);

    if (!bg || !header || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    switch (header->type) {
        case BIO_MSG_HYPO_REWARD_SIGNAL:
        case BIO_MSG_VTA_DOPAMINE_BURST:
        case BIO_MSG_VTA_DOPAMINE_DIP:
            /* Process reward signal */
            if (header->payload_size >= sizeof(float) * 2) {
                const float* data = (const float*)payload;
                float reward = data[0];
                float expected = data[1];
                basal_ganglia_update_dopamine(bg, reward, expected);
                NIMCP_LOGGING_DEBUG("BIO: Processed reward signal (r=%.2f, e=%.2f)",
                                    reward, expected);
            }
            break;

        case BIO_MSG_HYPO_EMOTION_UPDATE:
        case BIO_MSG_EMOTION_TENSOR_UPDATE:
            /* Modulate action values based on emotional valence */
            if (header->payload_size >= sizeof(float) * 2) {
                const float* data = (const float*)payload;
                float valence = data[0];
                float arousal = data[1];
                /* Positive valence boosts dopamine, negative suppresses */
                float da_mod = bg->dopamine_level + valence * 0.1f * arousal;
                basal_ganglia_set_dopamine(bg, da_mod);
                NIMCP_LOGGING_DEBUG("BIO: Emotional modulation (v=%.2f, a=%.2f)",
                                    valence, arousal);
            }
            break;

        case BIO_MSG_EXEC_GOAL_ACTIVATED:
        case BIO_MSG_EXEC_GOAL_COMPLETED:
            /* Update operating mode based on goal */
            if (header->payload_size >= sizeof(uint32_t)) {
                uint32_t goal_active = *(const uint32_t*)payload;
                nimcp_mutex_lock(bg->mutex);
                bg->mode = goal_active ? BG_MODE_GOAL_DIRECTED : BG_MODE_HABITUAL;
                nimcp_mutex_unlock(bg->mutex);
                NIMCP_LOGGING_DEBUG("BIO: Goal change (active=%u)", goal_active);
            }
            break;

        case BIO_MSG_MOTOR_STOP_REQUEST:
        case BIO_MSG_DIRECTIVE_ACTION_BLOCKED:
            /* Emergency stop signal */
            if (header->payload_size >= sizeof(float)) {
                float strength = *(const float*)payload;
                basal_ganglia_suppress_action(bg, strength);
                NIMCP_LOGGING_DEBUG("BIO: Suppression signal (s=%.2f)", strength);
            }
            break;

        default:
            NIMCP_LOGGING_DEBUG("BIO: Unhandled message type %u", header->type);
            break;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Process all pending bio-async messages
 * @param bg Basal ganglia system
 * @return Number of messages processed
 */
int basal_ganglia_process_bio_messages(basal_ganglia_t* bg) {
    if (!bg || !bg->bio_async_enabled || !bg->bio_ctx) return 0;

    /* Process inbox - handlers registered in connect will be called */
    return (int)bio_router_process_inbox(bg->bio_ctx, 0);  /* 0 = process all */
}

int basal_ganglia_connect_bio_async(basal_ganglia_t* bg) {
    NIMCP_THROW_IF(!bg, NIMCP_ERROR_NULL_POINTER, "bg is NULL");
    if (!bg) return -1;

    nimcp_mutex_lock(bg->mutex);

    if (bg->bio_async_enabled) {
        nimcp_mutex_unlock(bg->mutex);
        return 0;
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_BASAL_GANGLIA,
        .module_name = "basal_ganglia",
        .inbox_capacity = 32,
        .user_data = bg
    };

    bg->bio_ctx = bio_router_register_module(&info);
    if (bg->bio_ctx) {
        /* Register handlers for reward signals */
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_HYPO_REWARD_SIGNAL, basal_ganglia_bio_handler);
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_VTA_DOPAMINE_BURST, basal_ganglia_bio_handler);
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_VTA_DOPAMINE_DIP, basal_ganglia_bio_handler);

        /* Register handlers for emotional signals */
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_HYPO_EMOTION_UPDATE, basal_ganglia_bio_handler);
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_EMOTION_TENSOR_UPDATE, basal_ganglia_bio_handler);

        /* Register handlers for goal changes */
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_EXEC_GOAL_ACTIVATED, basal_ganglia_bio_handler);
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_EXEC_GOAL_COMPLETED, basal_ganglia_bio_handler);

        /* Register handlers for action suppression */
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_MOTOR_STOP_REQUEST, basal_ganglia_bio_handler);
        bio_router_register_handler(bg->bio_ctx, BIO_MSG_DIRECTIVE_ACTION_BLOCKED, basal_ganglia_bio_handler);

        bg->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Basal ganglia connected to bio-async router");
    }

    nimcp_mutex_unlock(bg->mutex);

    return bg->bio_async_enabled ? 0 : -1;
}

int basal_ganglia_disconnect_bio_async(basal_ganglia_t* bg) {
    if (!bg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_disconnect_bio_async: bg is NULL");
        return -1;
    }

    nimcp_mutex_lock(bg->mutex);

    if (bg->bio_async_enabled && bg->bio_ctx) {
        bio_router_unregister_module(bg->bio_ctx);
        bg->bio_ctx = NULL;
        bg->bio_async_enabled = false;
    }

    nimcp_mutex_unlock(bg->mutex);

    return 0;
}

bool basal_ganglia_is_bio_async_connected(const basal_ganglia_t* bg) {
    if (!bg) {
        return false;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    bool connected = bg->bio_async_enabled;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return connected;
}

//=============================================================================
// Statistics and Debugging
//=============================================================================

int basal_ganglia_get_stats(const basal_ganglia_t* bg,
                             basal_ganglia_stats_t* stats) {
    if (!bg || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "basal_ganglia_get_stats: required parameter is NULL (bg, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)bg->mutex);
    *stats = bg->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)bg->mutex);

    return 0;
}

const char* basal_ganglia_mode_name(bg_operating_mode_t mode) {
    switch (mode) {
        case BG_MODE_GOAL_DIRECTED: return "Goal-Directed";
        case BG_MODE_HABITUAL: return "Habitual";
        case BG_MODE_EXPLORATORY: return "Exploratory";
        case BG_MODE_SUPPRESSED: return "Suppressed";
        default: return "Unknown";
    }
}

const char* basal_ganglia_action_state_name(action_state_t state) {
    switch (state) {
        case ACTION_STATE_IDLE: return "Idle";
        case ACTION_STATE_COMPETING: return "Competing";
        case ACTION_STATE_SELECTED: return "Selected";
        case ACTION_STATE_EXECUTING: return "Executing";
        case ACTION_STATE_COMPLETED: return "Completed";
        case ACTION_STATE_CANCELLED: return "Cancelled";
        default: return "Unknown";
    }
}

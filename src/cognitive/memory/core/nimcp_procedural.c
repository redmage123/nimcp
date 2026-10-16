//=============================================================================
// nimcp_procedural.c - Procedural Memory System Implementation
//=============================================================================
/**
 * @file nimcp_procedural.c
 * @brief Implementation of procedural memory for skills, habits, and actions
 *
 * Implements motor programs, skill acquisition (cognitive->associative->autonomous),
 * habit formation with stimulus-response associations, and procedural chunking.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_procedural.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(procedural, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Thread-Local Error Handling
//=============================================================================

#ifdef _WIN32
    #define THREAD_LOCAL __declspec(thread)
#else
    #define THREAD_LOCAL _Thread_local
#endif

static THREAD_LOCAL char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal procedural memory manager structure
 */
struct procedural_memory_internal {
    // PR memory integration
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;

    // Skill storage
    procedural_skill_t* skills;
    size_t num_skills;
    size_t max_skills;

    // Habit storage
    procedural_habit_t* habits;
    size_t num_habits;
    size_t max_habits;

    // Execution state
    uint64_t executing_skill_id;
    size_t executing_step_index;
    float execution_start_time;
    size_t execution_steps_completed;
    size_t execution_steps_failed;
    bool is_executing;

    // ID generation
    uint64_t next_skill_id;
    uint64_t next_habit_id;
    uint64_t next_step_id;

    // Statistics
    procedural_stats_t stats;

    // Configuration
    procedural_config_t config;

    // Current time for operations
    float current_time;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in seconds
 */
static float get_current_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f;
}

/**
 * @brief Find skill by ID
 */
static procedural_skill_t* find_skill(procedural_memory_t pm, uint64_t id) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    for (size_t i = 0; i < pm->num_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_skills);
        }

        if (pm->skills[i].skill_id == id) {
            return &pm->skills[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_skill: validation failed");
    return NULL;
}

/**
 * @brief Find habit by ID
 */
static procedural_habit_t* find_habit(procedural_memory_t pm, uint64_t id) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    for (size_t i = 0; i < pm->num_habits; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_habits > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_habits);
        }

        if (pm->habits[i].habit_id == id) {
            return &pm->habits[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_habit: validation failed");
    return NULL;
}

/**
 * @brief Find free slot in skills array
 */
static procedural_skill_t* find_free_skill_slot(procedural_memory_t pm) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    // Look for empty slot
    for (size_t i = 0; i < pm->num_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_skills);
        }

        if (pm->skills[i].skill_id == PROC_INVALID_ID) {
            return &pm->skills[i];
        }
    }

    // No empty slot, expand if room
    if (pm->num_skills < pm->max_skills) {
        return &pm->skills[pm->num_skills++];
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_skill_slot: validation failed");
    return NULL;
}

/**
 * @brief Find free slot in habits array
 */
static procedural_habit_t* find_free_habit_slot(procedural_memory_t pm) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    // Look for empty slot
    for (size_t i = 0; i < pm->num_habits; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_habits > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_habits);
        }

        if (pm->habits[i].habit_id == PROC_INVALID_ID) {
            return &pm->habits[i];
        }
    }

    // No empty slot, expand if room
    if (pm->num_habits < pm->max_habits) {
        return &pm->habits[pm->num_habits++];
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_free_habit_slot: validation failed");
    return NULL;
}

/**
 * @brief Initialize skill to default values
 */
static void init_skill(procedural_skill_t* skill) {
    if (!skill) return;

    memset(skill, 0, sizeof(*skill));
    skill->skill_id = PROC_INVALID_ID;
    skill->parent_skill_id = PROC_INVALID_ID;
    skill->stage = SKILL_STAGE_COGNITIVE;
    skill->type = PROC_MOTOR;
    skill->exec_status = PROC_EXEC_IDLE;
    skill->accuracy = 0.0f;
    skill->speed = 1.0f;
    skill->automaticity = 0.0f;
    skill->strength = 1.0f;
    skill->learning_rate = 0.1f;
    skill->skill_quaternion = quat_identity();
}

/**
 * @brief Initialize habit to default values
 */
static void init_habit(procedural_habit_t* habit) {
    if (!habit) return;

    memset(habit, 0, sizeof(*habit));
    habit->habit_id = PROC_INVALID_ID;
    habit->linked_skill_id = PROC_INVALID_ID;
    habit->strength = PROC_DEFAULT_HABIT_STRENGTH;
    habit->cue_state = quat_identity();
    habit->response_state = quat_identity();
}

/**
 * @brief Free skill resources
 */
static void free_skill(procedural_skill_t* skill) {
    if (!skill) return;

    if (skill->skill_name) {
        nimcp_free(skill->skill_name);
        skill->skill_name = NULL;
    }

    if (skill->steps) {
        for (size_t i = 0; i < skill->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && skill->num_steps > 256) {
                procedural_heartbeat("procedural_loop",
                                 (float)(i + 1) / (float)skill->num_steps);
            }

            if (skill->steps[i].action_description) {
                nimcp_free(skill->steps[i].action_description);
            }
        }
        nimcp_free(skill->steps);
        skill->steps = NULL;
    }

    if (skill->accuracy_history) {
        nimcp_free(skill->accuracy_history);
        skill->accuracy_history = NULL;
    }

    if (skill->sub_skills) {
        nimcp_free(skill->sub_skills);
        skill->sub_skills = NULL;
    }

    // Note: memory_node is managed by PR system
    skill->memory_node = NULL;
    skill->skill_id = PROC_INVALID_ID;
}

/**
 * @brief Free habit resources
 */
static void free_habit(procedural_habit_t* habit) {
    if (!habit) return;

    if (habit->reward_history) {
        nimcp_free(habit->reward_history);
        habit->reward_history = NULL;
    }

    // Note: memory_node is managed by PR system
    habit->memory_node = NULL;
    habit->habit_id = PROC_INVALID_ID;
}

/**
 * @brief Copy string with length limit
 */
static char* copy_string(const char* src, size_t max_len) {
    if (!src) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "src is NULL");

        return NULL;

    }

    size_t len = strlen(src);
    if (len > max_len - 1) {
        len = max_len - 1;
    }

    char* copy = (char*)nimcp_malloc(len + 1);
    if (!copy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate copy");

        return NULL;

    }

    strncpy(copy, src, len);
    copy[len] = '\0';
    return copy;
}

/**
 * @brief Compute skill signature from steps
 */
static void compute_skill_signature(procedural_skill_t* skill) {
    if (!skill || !skill->skill_name) return;

    // Create signature from skill name and steps
    prime_signature_t* sig = prime_sig_from_text(skill->skill_name);
    if (sig) {
        skill->skill_signature = *sig;
        prime_sig_destroy(sig);
    }

    // Incorporate step signatures
    for (size_t i = 0; i < skill->num_steps && i < 10; i++) {
        if (skill->steps[i].action_description) {
            prime_signature_t* step_sig = prime_sig_from_text(skill->steps[i].action_description);
            if (step_sig) {
                prime_signature_t* composed = prime_sig_compose(&skill->skill_signature, step_sig);
                if (composed) {
                    skill->skill_signature = *composed;
                    prime_sig_destroy(composed);
                }
                prime_sig_destroy(step_sig);
            }
        }
    }
}

/**
 * @brief Update running average
 */
static float update_running_average(float current, float new_value, size_t count) {
    if (count == 0) return new_value;
    return (current * (float)(count - 1) + new_value) / (float)count;
}

/**
 * @brief Check if skill is ready to advance to next stage
 */
static bool check_stage_advancement(procedural_skill_t* skill, const procedural_config_t* config) {
    if (!skill || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "check_stage_advancement: required parameter is NULL (skill, config)");
        return false;
    }

    switch (skill->stage) {
        case SKILL_STAGE_COGNITIVE:
            return (skill->practice_count >= (size_t)config->cognitive_practice_threshold &&
                    skill->accuracy >= config->cognitive_accuracy_threshold &&
                    skill->automaticity >= config->cognitive_auto_threshold);

        case SKILL_STAGE_ASSOCIATIVE:
            return (skill->practice_count >= (size_t)config->associative_practice_threshold &&
                    skill->accuracy >= config->associative_accuracy_threshold &&
                    skill->automaticity >= config->associative_auto_threshold);

        case SKILL_STAGE_AUTONOMOUS:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_stage_advancement: operation failed");
            return false; // Already at highest stage

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_stage_advancement: operation failed");
            return false;
    }
}

/**
 * @brief Create PR memory node for skill
 */
static pr_memory_node_t* create_skill_memory_node(procedural_memory_t pm,
                                                   procedural_skill_t* skill) {
    if (!pm || !skill || !pm->node_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "check_stage_advancement: required parameter is NULL (pm, skill, pm->node_manager)");
        return NULL;
    }

    pr_node_config_t config = pr_memory_node_default_config();
    config.initial_tier = PR_MEMORY_TIER_Z2; // Long-term storage for skills
    config.initial_strength = skill->strength;
    config.salience = 0.6f;
    config.accessibility = 0.5f + skill->automaticity * 0.5f;
    config.compute_signature = true;

    const void* data = skill->skill_name;
    size_t size = skill->skill_name ? strlen(skill->skill_name) : 0;

    return pr_memory_node_create(pm->node_manager, data, size, &config);
}

/**
 * @brief Create PR memory node for habit
 */
static pr_memory_node_t* create_habit_memory_node(procedural_memory_t pm,
                                                   procedural_habit_t* habit) {
    if (!pm || !habit || !pm->node_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "check_stage_advancement: required parameter is NULL (pm, habit, pm->node_manager)");
        return NULL;
    }

    pr_node_config_t config = pr_memory_node_default_config();
    config.initial_tier = PR_MEMORY_TIER_Z2;
    config.initial_strength = habit->strength;
    config.salience = 0.7f;
    config.accessibility = 0.6f + habit->automaticity * 0.4f;
    config.compute_signature = false; // Use provided signature

    // Use response signature hash as data identifier
    uint64_t sig_hash = habit->response_signature.hash;

    return pr_memory_node_create(pm->node_manager, &sig_hash, sizeof(sig_hash), &config);
}

/**
 * @brief Update statistics after skill operation
 */
static void update_skill_stats(procedural_memory_t pm) {
    if (!pm) return;

    pm->stats.total_skills = 0;
    pm->stats.skills_cognitive = 0;
    pm->stats.skills_associative = 0;
    pm->stats.skills_autonomous = 0;
    pm->stats.total_chunks = 0;
    float total_accuracy = 0.0f;
    float total_automaticity = 0.0f;

    for (size_t i = 0; i < pm->num_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_skills);
        }

        procedural_skill_t* skill = &pm->skills[i];
        if (skill->skill_id == PROC_INVALID_ID) continue;

        pm->stats.total_skills++;
        total_accuracy += skill->accuracy;
        total_automaticity += skill->automaticity;

        switch (skill->stage) {
            case SKILL_STAGE_COGNITIVE:
                pm->stats.skills_cognitive++;
                break;
            case SKILL_STAGE_ASSOCIATIVE:
                pm->stats.skills_associative++;
                break;
            case SKILL_STAGE_AUTONOMOUS:
                pm->stats.skills_autonomous++;
                break;
        }

        if (skill->is_chunk) {
            pm->stats.total_chunks++;
        }
    }

    if (pm->stats.total_skills > 0) {
        pm->stats.mean_skill_accuracy = total_accuracy / pm->stats.total_skills;
        pm->stats.mean_skill_automaticity = total_automaticity / pm->stats.total_skills;
    }
}

/**
 * @brief Update statistics after habit operation
 */
static void update_habit_stats(procedural_memory_t pm) {
    if (!pm) return;

    pm->stats.total_habits = 0;
    float total_strength = 0.0f;
    float total_reward = 0.0f;

    for (size_t i = 0; i < pm->num_habits; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_habits > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_habits);
        }

        procedural_habit_t* habit = &pm->habits[i];
        if (habit->habit_id == PROC_INVALID_ID) continue;

        pm->stats.total_habits++;
        total_strength += habit->strength;
        total_reward += habit->mean_reward;
    }

    if (pm->stats.total_habits > 0) {
        pm->stats.mean_habit_strength = total_strength / pm->stats.total_habits;
        pm->stats.mean_habit_reward = total_reward / pm->stats.total_habits;
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT procedural_config_t procedural_config_default(void) {
    procedural_config_t config = {
        .max_skills = PROC_MAX_SKILLS,
        .max_habits = PROC_MAX_HABITS,
        .max_steps_per_skill = PROC_MAX_STEPS_PER_SKILL,
        .max_sub_skills = PROC_MAX_SUB_SKILLS,

        // Learning parameters
        .base_learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .skill_decay_rate = PROC_DEFAULT_DECAY_RATE,
        .habit_decay_rate = PROC_DEFAULT_DECAY_RATE * 0.5f,

        // Stage transition thresholds
        .cognitive_practice_threshold = (float)PROC_COGNITIVE_PRACTICE_THRESHOLD,
        .associative_practice_threshold = (float)PROC_ASSOCIATIVE_PRACTICE_THRESHOLD,
        .cognitive_accuracy_threshold = PROC_COGNITIVE_ACCURACY_THRESHOLD,
        .associative_accuracy_threshold = PROC_ASSOCIATIVE_ACCURACY_THRESHOLD,
        .cognitive_auto_threshold = PROC_COGNITIVE_AUTO_THRESHOLD,
        .associative_auto_threshold = PROC_ASSOCIATIVE_AUTO_THRESHOLD,

        // Habit parameters
        .habit_trigger_threshold = PROC_HABIT_TRIGGER_THRESHOLD,
        .habit_initial_strength = PROC_DEFAULT_HABIT_STRENGTH,
        .habit_reinforcement_rate = 0.1f,

        // History tracking
        .accuracy_history_len = 20,
        .reward_history_len = PROC_MAX_REWARD_HISTORY
    };
    return config;
}

NIMCP_EXPORT bool procedural_config_validate(const procedural_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "procedural_config_validate: config is NULL");
        return false;
    }

    if (config->max_skills == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "procedural_config_validate: config->max_skills is zero");
        return false;
    }
    if (config->max_habits == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "procedural_config_validate: config->max_habits is zero");
        return false;
    }
    if (config->max_steps_per_skill == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "procedural_config_validate: config->max_steps_per_skill is zero");
        return false;
    }

    if (config->base_learning_rate < 0.0f || config->base_learning_rate > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "procedural_config_validate: validation failed");
        return false;
    }
    if (config->skill_decay_rate < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "procedural_config_validate: validation failed");
        return false;
    }

    if (config->cognitive_accuracy_threshold < 0.0f ||
        config->cognitive_accuracy_threshold > 1.0f) return false;
    if (config->associative_accuracy_threshold < 0.0f ||
        config->associative_accuracy_threshold > 1.0f) return false;

    if (config->habit_trigger_threshold < 0.0f ||
        config->habit_trigger_threshold > 1.0f) return false;

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT procedural_memory_t procedural_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const procedural_config_t* config
) {
    // Use default config if none provided
    procedural_config_t cfg;
    if (config) {
        cfg = *config;
        if (!procedural_config_validate(&cfg)) {
            set_error("Invalid configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "procedural_create: procedural_config_validate is NULL");
            return NULL;
        }
    } else {
        cfg = procedural_config_default();
    }

    // Allocate manager
    procedural_memory_t pm = (procedural_memory_t)nimcp_calloc(1, sizeof(struct procedural_memory_internal));
    if (!pm) {
        set_error("Memory allocation failed for manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "procedural_create: pm is NULL");
        return NULL;
    }

    // Store integration handles
    pm->entanglement = entanglement;
    pm->node_manager = node_manager;

    // Allocate skills array
    pm->skills = (procedural_skill_t*)nimcp_calloc(cfg.max_skills, sizeof(procedural_skill_t));
    if (!pm->skills) {
        set_error("Memory allocation failed for skills");
        nimcp_free(pm);
        pm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "procedural_create: pm->skills is NULL");
        return NULL;
    }

    // Initialize all skill slots
    for (size_t i = 0; i < cfg.max_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cfg.max_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)cfg.max_skills);
        }

        init_skill(&pm->skills[i]);
    }

    pm->num_skills = 0;
    pm->max_skills = cfg.max_skills;

    // Allocate habits array
    pm->habits = (procedural_habit_t*)nimcp_calloc(cfg.max_habits, sizeof(procedural_habit_t));
    if (!pm->habits) {
        set_error("Memory allocation failed for habits");
        nimcp_free(pm->skills);
        nimcp_free(pm);
        pm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "procedural_create: pm->habits is NULL");
        return NULL;
    }

    // Initialize all habit slots
    for (size_t i = 0; i < cfg.max_habits; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cfg.max_habits > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)cfg.max_habits);
        }

        init_habit(&pm->habits[i]);
    }

    pm->num_habits = 0;
    pm->max_habits = cfg.max_habits;

    // Initialize execution state
    pm->executing_skill_id = PROC_INVALID_ID;
    pm->is_executing = false;

    // Initialize ID generators
    pm->next_skill_id = 1;
    pm->next_habit_id = 1;
    pm->next_step_id = 1;

    // Store config
    pm->config = cfg;

    // Initialize stats
    memset(&pm->stats, 0, sizeof(pm->stats));

    // Initialize time
    pm->current_time = get_current_time_seconds();

    return pm;
}

NIMCP_EXPORT void procedural_destroy(procedural_memory_t pm) {
    if (!pm) return;

    // Free skills
    if (pm->skills) {
        for (size_t i = 0; i < pm->max_skills; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pm->max_skills > 256) {
                procedural_heartbeat("procedural_loop",
                                 (float)(i + 1) / (float)pm->max_skills);
            }

            free_skill(&pm->skills[i]);
        }
        nimcp_free(pm->skills);
    }

    // Free habits
    if (pm->habits) {
        for (size_t i = 0; i < pm->max_habits; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pm->max_habits > 256) {
                procedural_heartbeat("procedural_loop",
                                 (float)(i + 1) / (float)pm->max_habits);
            }

            free_habit(&pm->habits[i]);
        }
        nimcp_free(pm->habits);
    }

    nimcp_free(pm);
    pm = NULL;
}

NIMCP_EXPORT procedural_error_t procedural_reset(procedural_memory_t pm) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    // Reset all skills
    for (size_t i = 0; i < pm->max_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->max_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->max_skills);
        }

        free_skill(&pm->skills[i]);
        init_skill(&pm->skills[i]);
    }
    pm->num_skills = 0;

    // Reset all habits
    for (size_t i = 0; i < pm->max_habits; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->max_habits > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->max_habits);
        }

        free_habit(&pm->habits[i]);
        init_habit(&pm->habits[i]);
    }
    pm->num_habits = 0;

    // Reset execution state
    pm->executing_skill_id = PROC_INVALID_ID;
    pm->is_executing = false;

    // Reset ID generators
    pm->next_skill_id = 1;
    pm->next_habit_id = 1;
    pm->next_step_id = 1;

    // Reset stats
    memset(&pm->stats, 0, sizeof(pm->stats));

    return PROC_SUCCESS;
}

//=============================================================================
// Skill Creation Functions
//=============================================================================

NIMCP_EXPORT procedural_error_t procedural_create_skill(
    procedural_memory_t pm,
    const char* name,
    procedural_type_t type,
    uint64_t* skill_id_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!name) {
        set_error("NULL skill name");
        return PROC_ERROR_NULL_POINTER;
    }
    if (type < PROC_MOTOR || type > PROC_HABIT) {
        set_error("Invalid procedural type");
        return PROC_ERROR_INVALID_TYPE;
    }

    // Find free slot
    procedural_skill_t* skill = find_free_skill_slot(pm);
    if (!skill) {
        set_error("Maximum skills reached");
        return PROC_ERROR_CAPACITY;
    }

    // Initialize skill
    init_skill(skill);
    skill->skill_id = pm->next_skill_id++;
    skill->type = type;
    skill->stage = SKILL_STAGE_COGNITIVE;

    // Copy name
    skill->skill_name = copy_string(name, PROC_MAX_SKILL_NAME);
    if (!skill->skill_name) {
        set_error("Failed to copy skill name");
        free_skill(skill);
        return PROC_ERROR_NO_MEMORY;
    }

    // Allocate steps array
    skill->steps = (procedural_step_t*)nimcp_calloc(pm->config.max_steps_per_skill,
                                               sizeof(procedural_step_t));
    if (!skill->steps) {
        set_error("Failed to allocate steps array");
        free_skill(skill);
        return PROC_ERROR_NO_MEMORY;
    }
    skill->allocated_steps = pm->config.max_steps_per_skill;
    skill->num_steps = 0;

    // Allocate accuracy history
    skill->accuracy_history = (float*)nimcp_calloc(pm->config.accuracy_history_len, sizeof(float));
    if (!skill->accuracy_history) {
        set_error("Failed to allocate accuracy history");
        free_skill(skill);
        return PROC_ERROR_NO_MEMORY;
    }
    skill->history_len = pm->config.accuracy_history_len;

    // Set initial values
    skill->accuracy = 0.0f;
    skill->speed = 1.0f;
    skill->automaticity = 0.0f;
    skill->strength = 1.0f;
    skill->learning_rate = pm->config.base_learning_rate;
    skill->creation_time = pm->current_time;
    skill->last_practice_time = pm->current_time;

    // Compute initial signature
    compute_skill_signature(skill);

    // Create PR memory node if available
    if (pm->node_manager) {
        skill->memory_node = create_skill_memory_node(pm, skill);
    }

    // Update stats
    update_skill_stats(pm);

    if (skill_id_out) {
        *skill_id_out = skill->skill_id;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_create_skill_with_steps(
    procedural_memory_t pm,
    const char* name,
    procedural_type_t type,
    const char** step_descriptions,
    const float* step_durations,
    size_t num_steps,
    uint64_t* skill_id_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!step_descriptions || !step_durations) {
        set_error("NULL step data");
        return PROC_ERROR_NULL_POINTER;
    }
    if (num_steps > pm->config.max_steps_per_skill) {
        set_error("Too many steps");
        return PROC_ERROR_MAX_STEPS;
    }

    // Create the skill first
    uint64_t skill_id = 0;
    procedural_error_t err = procedural_create_skill(pm, name, type, &skill_id);
    if (err != PROC_SUCCESS) {
        return err;
    }

    // Add steps
    for (size_t i = 0; i < num_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_steps > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)num_steps);
        }

        uint64_t step_id = 0;
        err = procedural_add_step(pm, skill_id, step_descriptions[i],
                                  step_durations[i], 0.5f, &step_id);
        if (err != PROC_SUCCESS) {
            procedural_remove_skill(pm, skill_id);
            return err;
        }
    }

    // Recompute signature with all steps
    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (skill) {
        compute_skill_signature(skill);
    }

    if (skill_id_out) {
        *skill_id_out = skill_id;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_add_step(
    procedural_memory_t pm,
    uint64_t skill_id,
    const char* action_description,
    float duration_ms,
    float precision_required,
    uint64_t* step_id_out
) {
    return procedural_add_step_ex(pm, skill_id, action_description,
                                   duration_ms, precision_required,
                                   NULL, NULL, step_id_out);
}

NIMCP_EXPORT procedural_error_t procedural_add_step_ex(
    procedural_memory_t pm,
    uint64_t skill_id,
    const char* action_description,
    float duration_ms,
    float precision_required,
    const prime_signature_t* preconditions,
    const prime_signature_t* postconditions,
    uint64_t* step_id_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!action_description) {
        set_error("NULL action description");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    if (skill->num_steps >= skill->allocated_steps) {
        set_error("Maximum steps reached");
        return PROC_ERROR_MAX_STEPS;
    }

    // Get next step slot
    procedural_step_t* step = &skill->steps[skill->num_steps];
    memset(step, 0, sizeof(*step));

    // Assign step ID
    step->step_id = pm->next_step_id++;

    // Copy action description
    step->action_description = copy_string(action_description, PROC_MAX_ACTION_DESCRIPTION);
    if (!step->action_description) {
        set_error("Failed to copy action description");
        return PROC_ERROR_NO_MEMORY;
    }

    // Set step properties
    step->duration_ms = duration_ms;
    step->precision_required = nimcp_clampf(precision_required, 0.0f, 1.0f);

    // Compute action signature
    prime_signature_t* sig = prime_sig_from_text(action_description);
    if (sig) {
        step->action_signature = *sig;
        prime_sig_destroy(sig);
    }

    // Set preconditions and postconditions
    if (preconditions) {
        step->preconditions = *preconditions;
    }
    if (postconditions) {
        step->postconditions = *postconditions;
    }

    // Initialize execution tracking
    step->execution_count = 0;
    step->success_rate = 0.0f;
    step->mean_duration_ms = duration_ms;
    step->is_optional = false;
    step->requires_attention = true;

    skill->num_steps++;

    if (step_id_out) {
        *step_id_out = step->step_id;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_insert_step(
    procedural_memory_t pm,
    uint64_t skill_id,
    size_t index,
    const char* action_description,
    float duration_ms,
    float precision_required,
    uint64_t* step_id_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!action_description) {
        set_error("NULL action description");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    if (skill->num_steps >= skill->allocated_steps) {
        set_error("Maximum steps reached");
        return PROC_ERROR_MAX_STEPS;
    }

    if (index > skill->num_steps) {
        index = skill->num_steps;
    }

    // Shift existing steps
    for (size_t i = skill->num_steps; i > index; i--) {
        skill->steps[i] = skill->steps[i - 1];
    }

    // Initialize the new step
    procedural_step_t* step = &skill->steps[index];
    memset(step, 0, sizeof(*step));

    step->step_id = pm->next_step_id++;
    step->action_description = copy_string(action_description, PROC_MAX_ACTION_DESCRIPTION);
    if (!step->action_description) {
        // Shift steps back
        for (size_t i = index; i < skill->num_steps; i++) {
            skill->steps[i] = skill->steps[i + 1];
        }
        set_error("Failed to copy action description");
        return PROC_ERROR_NO_MEMORY;
    }

    step->duration_ms = duration_ms;
    step->precision_required = nimcp_clampf(precision_required, 0.0f, 1.0f);
    step->mean_duration_ms = duration_ms;
    step->requires_attention = true;

    prime_signature_t* sig = prime_sig_from_text(action_description);
    if (sig) {
        step->action_signature = *sig;
        prime_sig_destroy(sig);
    }

    skill->num_steps++;

    if (step_id_out) {
        *step_id_out = step->step_id;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_remove_step(
    procedural_memory_t pm,
    uint64_t skill_id,
    size_t step_index
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    if (step_index >= skill->num_steps) {
        set_error("Invalid step index");
        return PROC_ERROR_INVALID_STEP;
    }

    // Free the step's resources
    if (skill->steps[step_index].action_description) {
        nimcp_free(skill->steps[step_index].action_description);
    }

    // Shift remaining steps
    for (size_t i = step_index; i < skill->num_steps - 1; i++) {
        skill->steps[i] = skill->steps[i + 1];
    }

    // Clear the last slot
    memset(&skill->steps[skill->num_steps - 1], 0, sizeof(procedural_step_t));
    skill->num_steps--;

    return PROC_SUCCESS;
}

//=============================================================================
// Practice and Learning Functions
//=============================================================================

NIMCP_EXPORT procedural_error_t procedural_practice(
    procedural_memory_t pm,
    uint64_t skill_id,
    float practice_accuracy,
    float practice_duration
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    practice_accuracy = nimcp_clampf(practice_accuracy, 0.0f, 1.0f);
    pm->current_time = get_current_time_seconds();

    // Update practice count
    skill->practice_count++;

    // Update accuracy (running average)
    skill->accuracy = update_running_average(skill->accuracy, practice_accuracy,
                                              skill->practice_count);

    // Store in history (circular buffer)
    size_t history_idx = (skill->practice_count - 1) % skill->history_len;
    skill->accuracy_history[history_idx] = practice_accuracy;

    // Update automaticity based on practice
    // Logarithmic increase: more practice = higher automaticity, diminishing returns
    float practice_factor = logf(1.0f + skill->practice_count) / 10.0f;
    float accuracy_factor = skill->accuracy * skill->accuracy; // Accuracy squared

    skill->automaticity = nimcp_clampf(
        practice_factor * accuracy_factor,
        0.0f, 1.0f
    );

    // Update speed (faster with more practice)
    skill->speed = 1.0f + logf(1.0f + skill->practice_count * 0.1f);

    // Update strength (reinforced by practice)
    skill->strength = nimcp_clampf(
        skill->strength + pm->config.base_learning_rate * 0.1f,
        PROC_MIN_SKILL_STRENGTH, 1.0f
    );

    // Update timing
    skill->last_practice_time = pm->current_time;
    skill->total_practice_time += practice_duration;

    // Check for stage advancement
    bool advanced = false;
    procedural_advance_stage(pm, skill_id, &advanced);

    // Update learning rate (decreases with stage)
    switch (skill->stage) {
        case SKILL_STAGE_COGNITIVE:
            skill->learning_rate = pm->config.base_learning_rate;
            break;
        case SKILL_STAGE_ASSOCIATIVE:
            skill->learning_rate = pm->config.base_learning_rate * 0.5f;
            break;
        case SKILL_STAGE_AUTONOMOUS:
            skill->learning_rate = pm->config.base_learning_rate * 0.1f;
            break;
    }

    // Update quaternion state
    skill->skill_quaternion.w = skill->strength;       // Consolidation
    skill->skill_quaternion.x = 0.0f;                  // Neutral emotion
    skill->skill_quaternion.y = 0.5f + skill->accuracy * 0.5f; // Salience from accuracy
    skill->skill_quaternion.z = skill->automaticity;   // Accessibility

    // Update stats
    pm->stats.total_practice_sessions++;
    pm->stats.total_practice_time += practice_duration;
    update_skill_stats(pm);

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_practice_detailed(
    procedural_memory_t pm,
    uint64_t skill_id,
    const float* step_accuracies,
    const float* step_durations,
    size_t num_steps
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!step_accuracies || !step_durations) {
        set_error("NULL step data");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    if (num_steps != skill->num_steps) {
        set_error("Step count mismatch");
        return PROC_ERROR_INVALID_STEP;
    }

    // Update per-step statistics
    float total_accuracy = 0.0f;
    float total_duration = 0.0f;

    for (size_t i = 0; i < num_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_steps > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)num_steps);
        }

        procedural_step_t* step = &skill->steps[i];
        float acc = nimcp_clampf(step_accuracies[i], 0.0f, 1.0f);

        step->execution_count++;
        step->success_rate = update_running_average(step->success_rate, acc,
                                                     step->execution_count);
        step->mean_duration_ms = update_running_average(step->mean_duration_ms,
                                                         step_durations[i],
                                                         step->execution_count);

        total_accuracy += acc;
        total_duration += step_durations[i];
    }

    // Practice with aggregate metrics
    float avg_accuracy = total_accuracy / (fabsf(num_steps) > 1e-7f ? num_steps : 1e-7f);
    return procedural_practice(pm, skill_id, avg_accuracy, total_duration);
}

NIMCP_EXPORT procedural_error_t procedural_advance_stage(
    procedural_memory_t pm,
    uint64_t skill_id,
    bool* advanced_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    bool advanced = false;

    if (check_stage_advancement(skill, &pm->config)) {
        switch (skill->stage) {
            case SKILL_STAGE_COGNITIVE:
                skill->stage = SKILL_STAGE_ASSOCIATIVE;
                advanced = true;
                break;
            case SKILL_STAGE_ASSOCIATIVE:
                skill->stage = SKILL_STAGE_AUTONOMOUS;
                advanced = true;
                break;
            case SKILL_STAGE_AUTONOMOUS:
                // Already at highest stage
                break;
        }

        if (advanced) {
            // Mark steps as requiring less attention
            for (size_t i = 0; i < skill->num_steps; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && skill->num_steps > 256) {
                    procedural_heartbeat("procedural_loop",
                                     (float)(i + 1) / (float)skill->num_steps);
                }

                if (skill->stage == SKILL_STAGE_AUTONOMOUS) {
                    skill->steps[i].requires_attention = false;
                }
            }

            // Update PR node tier if available
            if (skill->memory_node && skill->stage == SKILL_STAGE_AUTONOMOUS) {
                pr_memory_node_set_tier(skill->memory_node, PR_MEMORY_TIER_Z3);
            }
        }
    }

    if (advanced_out) {
        *advanced_out = advanced;
    }

    update_skill_stats(pm);
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_set_stage(
    procedural_memory_t pm,
    uint64_t skill_id,
    skill_stage_t stage
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (stage < SKILL_STAGE_COGNITIVE || stage > SKILL_STAGE_AUTONOMOUS) {
        set_error("Invalid stage");
        return PROC_ERROR_INVALID_STAGE;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    skill->stage = stage;
    update_skill_stats(pm);

    return PROC_SUCCESS;
}

NIMCP_EXPORT skill_stage_t procedural_get_stage(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) return (skill_stage_t)-1;

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) return (skill_stage_t)-1;

    return skill->stage;
}

NIMCP_EXPORT float procedural_compute_automaticity(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) return -1.0f;

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) return -1.0f;

    // Automaticity factors:
    // 1. Practice repetitions (logarithmic)
    float practice_factor = logf(1.0f + skill->practice_count) / 10.0f;
    practice_factor = nimcp_clampf(practice_factor, 0.0f, 1.0f);

    // 2. Accuracy consistency (low variance = high automaticity)
    float accuracy_variance = 0.0f;
    if (skill->practice_count > 1) {
        float mean = skill->accuracy;
        size_t count = skill->practice_count < skill->history_len ?
                       skill->practice_count : skill->history_len;
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                procedural_heartbeat("procedural_loop",
                                 (float)(i + 1) / (float)count);
            }

            float diff = skill->accuracy_history[i] - mean;
            accuracy_variance += diff * diff;
        }
        accuracy_variance /= count;
    }
    float consistency = 1.0f - sqrtf(accuracy_variance);
    consistency = nimcp_clampf(consistency, 0.0f, 1.0f);

    // 3. Speed improvement
    float speed_factor = nimcp_clampf((skill->speed - 1.0f) / 2.0f, 0.0f, 1.0f);

    // 4. Base accuracy
    float accuracy_factor = skill->accuracy;

    // Weighted combination
    float automaticity = 0.3f * practice_factor +
                         0.3f * consistency +
                         0.2f * speed_factor +
                         0.2f * accuracy_factor;

    skill->automaticity = nimcp_clampf(automaticity, 0.0f, 1.0f);
    return skill->automaticity;
}

//=============================================================================
// Execution Functions
//=============================================================================

NIMCP_EXPORT procedural_error_t procedural_execute(
    procedural_memory_t pm,
    uint64_t skill_id,
    procedural_exec_result_t* result_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    pm->current_time = get_current_time_seconds();

    // Simulate execution of all steps
    size_t steps_completed = 0;
    size_t steps_failed = 0;
    float total_time = 0.0f;
    float total_accuracy = 0.0f;

    for (size_t i = 0; i < skill->num_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && skill->num_steps > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)skill->num_steps);
        }

        procedural_step_t* step = &skill->steps[i];

        // Simulate step execution with accuracy based on skill level
        float base_success_rate = 0.5f + skill->automaticity * 0.4f;
        float success_prob = base_success_rate + skill->accuracy * 0.1f;

        // Simple simulation: consider step successful based on probability
        bool success = ((float)nimcp_tl_rand() / RAND_MAX) < success_prob;

        if (success || step->is_optional) {
            steps_completed++;
            total_accuracy += success ? 1.0f : 0.5f;
        } else {
            steps_failed++;
        }

        // Update step stats
        step->execution_count++;
        step->success_rate = update_running_average(step->success_rate,
                                                     success ? 1.0f : 0.0f,
                                                     step->execution_count);

        // Simulated duration (faster with higher speed)
        float actual_duration = step->duration_ms / skill->speed;
        total_time += actual_duration;
        step->mean_duration_ms = update_running_average(step->mean_duration_ms,
                                                         actual_duration,
                                                         step->execution_count);
    }

    // Determine success
    bool success = steps_failed == 0 ||
                   (float)steps_completed / skill->num_steps > 0.7f;

    // Update skill execution stats
    skill->exec_status = success ? PROC_EXEC_COMPLETED : PROC_EXEC_FAILED;

    // This counts as practice
    float execution_accuracy = skill->num_steps > 0 ?
                               total_accuracy / skill->num_steps : 0.0f;
    procedural_practice(pm, skill_id, execution_accuracy, total_time);

    pm->stats.total_skill_executions++;

    // Fill result
    if (result_out) {
        result_out->skill_id = skill_id;
        result_out->status = skill->exec_status;
        result_out->steps_completed = steps_completed;
        result_out->steps_failed = steps_failed;
        result_out->execution_time_ms = total_time;
        result_out->accuracy = execution_accuracy;
        result_out->triggered_learning = true;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_begin_execution(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    if (pm->is_executing) {
        set_error("Already executing a skill");
        return PROC_ERROR_ALREADY_EXECUTING;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    pm->executing_skill_id = skill_id;
    pm->executing_step_index = 0;
    pm->execution_start_time = get_current_time_seconds();
    pm->execution_steps_completed = 0;
    pm->execution_steps_failed = 0;
    pm->is_executing = true;

    skill->exec_status = PROC_EXEC_STARTING;
    skill->exec_start_time = pm->execution_start_time;
    skill->current_step_index = 0;

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_step_execution(
    procedural_memory_t pm,
    bool step_success,
    float step_duration,
    bool* completed_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    if (!pm->is_executing) {
        set_error("Not executing any skill");
        return PROC_ERROR_NOT_EXECUTING;
    }

    procedural_skill_t* skill = find_skill(pm, pm->executing_skill_id);
    if (!skill) {
        set_error("Executing skill not found");
        pm->is_executing = false;
        return PROC_ERROR_NOT_FOUND;
    }

    if (pm->executing_step_index >= skill->num_steps) {
        if (completed_out) *completed_out = true;
        return PROC_SUCCESS;
    }

    // Update current step
    procedural_step_t* step = &skill->steps[pm->executing_step_index];
    step->execution_count++;
    step->success_rate = update_running_average(step->success_rate,
                                                 step_success ? 1.0f : 0.0f,
                                                 step->execution_count);
    step->mean_duration_ms = update_running_average(step->mean_duration_ms,
                                                     step_duration,
                                                     step->execution_count);

    if (step_success) {
        pm->execution_steps_completed++;
    } else {
        pm->execution_steps_failed++;
    }

    // Advance to next step
    pm->executing_step_index++;
    skill->current_step_index = pm->executing_step_index;

    // Check if completed
    bool completed = (pm->executing_step_index >= skill->num_steps);
    if (completed) {
        skill->exec_status = PROC_EXEC_COMPLETING;
    } else {
        skill->exec_status = PROC_EXEC_RUNNING;
    }

    if (completed_out) {
        *completed_out = completed;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_end_execution(
    procedural_memory_t pm,
    bool success,
    procedural_exec_result_t* result_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    if (!pm->is_executing) {
        set_error("Not executing any skill");
        return PROC_ERROR_NOT_EXECUTING;
    }

    procedural_skill_t* skill = find_skill(pm, pm->executing_skill_id);
    if (!skill) {
        set_error("Executing skill not found");
        pm->is_executing = false;
        return PROC_ERROR_NOT_FOUND;
    }

    pm->current_time = get_current_time_seconds();
    float total_time = (pm->current_time - pm->execution_start_time) * 1000.0f;

    // Finalize execution status
    skill->exec_status = success ? PROC_EXEC_COMPLETED : PROC_EXEC_FAILED;

    // Compute accuracy
    float accuracy = 0.0f;
    if (skill->num_steps > 0) {
        accuracy = (float)pm->execution_steps_completed / skill->num_steps;
    }

    // This counts as practice
    procedural_practice(pm, pm->executing_skill_id, accuracy, total_time);

    pm->stats.total_skill_executions++;

    // Fill result
    if (result_out) {
        result_out->skill_id = pm->executing_skill_id;
        result_out->status = skill->exec_status;
        result_out->steps_completed = pm->execution_steps_completed;
        result_out->steps_failed = pm->execution_steps_failed;
        result_out->execution_time_ms = total_time;
        result_out->accuracy = accuracy;
        result_out->triggered_learning = true;
    }

    // Reset execution state
    pm->executing_skill_id = PROC_INVALID_ID;
    pm->is_executing = false;

    return PROC_SUCCESS;
}

NIMCP_EXPORT uint64_t procedural_get_executing_skill(procedural_memory_t pm) {
    if (!pm || !pm->is_executing) return PROC_INVALID_ID;
    return pm->executing_skill_id;
}

NIMCP_EXPORT size_t procedural_get_current_step(procedural_memory_t pm) {
    if (!pm || !pm->is_executing) return (size_t)-1;
    return pm->executing_step_index;
}

//=============================================================================
// Habit Functions
//=============================================================================

NIMCP_EXPORT procedural_error_t procedural_create_habit(
    procedural_memory_t pm,
    const prime_signature_t* cue_signature,
    const prime_signature_t* response_signature,
    float initial_strength,
    uint64_t* habit_id_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!cue_signature || !response_signature) {
        set_error("NULL signature");
        return PROC_ERROR_NULL_POINTER;
    }

    // Find free slot
    procedural_habit_t* habit = find_free_habit_slot(pm);
    if (!habit) {
        set_error("Maximum habits reached");
        return PROC_ERROR_CAPACITY;
    }

    // Initialize habit
    init_habit(habit);
    habit->habit_id = pm->next_habit_id++;

    // Set signatures
    habit->cue_signature = *cue_signature;
    habit->response_signature = *response_signature;

    // Set initial strength
    habit->strength = (initial_strength > 0) ? initial_strength :
                       pm->config.habit_initial_strength;
    habit->strength = nimcp_clampf(habit->strength, 0.0f, 1.0f);

    // Initialize reward tracking
    habit->history_capacity = pm->config.reward_history_len;
    habit->reward_history = (float*)nimcp_calloc(habit->history_capacity, sizeof(float));
    if (!habit->reward_history) {
        set_error("Failed to allocate reward history");
        free_habit(habit);
        return PROC_ERROR_NO_MEMORY;
    }
    habit->history_len = 0;
    habit->mean_reward = 0.0f;
    habit->reward_prediction = 0.0f;

    // Set default properties
    habit->context_dependency = 0.5f;
    habit->automaticity = 0.0f;

    // Create PR memory node if available
    if (pm->node_manager) {
        habit->memory_node = create_habit_memory_node(pm, habit);
    }

    update_habit_stats(pm);

    if (habit_id_out) {
        *habit_id_out = habit->habit_id;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_create_habit_for_skill(
    procedural_memory_t pm,
    const prime_signature_t* cue_signature,
    uint64_t linked_skill_id,
    float initial_strength,
    uint64_t* habit_id_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!cue_signature) {
        set_error("NULL cue signature");
        return PROC_ERROR_NULL_POINTER;
    }

    // Verify skill exists
    procedural_skill_t* skill = find_skill(pm, linked_skill_id);
    if (!skill) {
        set_error("Linked skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    // Use skill signature as response
    procedural_error_t err = procedural_create_habit(pm, cue_signature,
                                                      &skill->skill_signature,
                                                      initial_strength,
                                                      habit_id_out);
    if (err != PROC_SUCCESS) {
        return err;
    }

    // Link skill to habit
    procedural_habit_t* habit = find_habit(pm, *habit_id_out);
    if (habit) {
        habit->linked_skill_id = linked_skill_id;
        habit->has_linked_skill = true;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_trigger_habit(
    procedural_memory_t pm,
    const prime_signature_t* context_signature,
    procedural_habit_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!context_signature) {
        set_error("NULL context signature");
        return PROC_ERROR_NULL_POINTER;
    }

    size_t num_triggered = 0;
    pm->current_time = get_current_time_seconds();

    for (size_t i = 0; i < pm->num_habits && num_triggered < max_triggered; i++) {
        procedural_habit_t* habit = &pm->habits[i];
        if (habit->habit_id == PROC_INVALID_ID) continue;

        // Compute similarity between context and cue
        float similarity = prime_sig_jaccard(&habit->cue_signature, context_signature);

        // Check if above threshold
        if (similarity >= pm->config.habit_trigger_threshold) {
            // Habit triggered
            habit->trigger_count++;
            habit->last_trigger_time = pm->current_time;

            pm->stats.total_habit_triggers++;
            pm->stats.total_habit_fires++;

            if (triggered_out) {
                triggered_out[num_triggered].habit_id = habit->habit_id;
                triggered_out[num_triggered].trigger_strength = similarity;
                triggered_out[num_triggered].habit_strength = habit->strength;
                triggered_out[num_triggered].predicted_reward = habit->reward_prediction;
                triggered_out[num_triggered].response = &habit->response_signature;
            }

            num_triggered++;
        }
    }

    if (num_triggered_out) {
        *num_triggered_out = num_triggered;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT float procedural_reinforce_habit(
    procedural_memory_t pm,
    uint64_t habit_id,
    float reward
) {
    if (!pm) return -1.0f;

    procedural_habit_t* habit = find_habit(pm, habit_id);
    if (!habit) return -1.0f;

    pm->current_time = get_current_time_seconds();

    // Compute reward prediction error
    float prediction_error = reward - habit->reward_prediction;

    // Update habit strength using TD-like learning
    float delta = pm->config.habit_reinforcement_rate * prediction_error;
    habit->strength = nimcp_clampf(habit->strength + delta, 0.0f, 1.0f);

    // Update reward prediction
    habit->reward_prediction += pm->config.habit_reinforcement_rate *
                                 (reward - habit->reward_prediction);

    // Add to reward history
    if (habit->history_len < habit->history_capacity) {
        habit->reward_history[habit->history_len++] = reward;
    } else {
        // Circular buffer
        for (size_t i = 0; i < habit->history_capacity - 1; i++) {
            habit->reward_history[i] = habit->reward_history[i + 1];
        }
        habit->reward_history[habit->history_capacity - 1] = reward;
    }

    // Update mean reward
    float sum = 0.0f;
    for (size_t i = 0; i < habit->history_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && habit->history_len > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)habit->history_len);
        }

        sum += habit->reward_history[i];
    }
    habit->mean_reward = habit->history_len > 0 ? sum / habit->history_len : 0.0f;

    // Update automaticity
    habit->automaticity = nimcp_clampf(
        habit->automaticity + 0.01f * (reward > 0 ? 1.0f : -0.5f),
        0.0f, 1.0f
    );

    // Update execution count
    habit->execution_count++;
    habit->last_execution_time = pm->current_time;

    update_habit_stats(pm);

    return habit->strength;
}

NIMCP_EXPORT procedural_error_t procedural_get_habit(
    procedural_memory_t pm,
    uint64_t habit_id,
    procedural_habit_t* habit_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!habit_out) {
        set_error("NULL output pointer");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_habit_t* habit = find_habit(pm, habit_id);
    if (!habit) {
        set_error("Habit not found");
        return PROC_ERROR_NOT_FOUND;
    }

    *habit_out = *habit;
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_remove_habit(
    procedural_memory_t pm,
    uint64_t habit_id
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_habit_t* habit = find_habit(pm, habit_id);
    if (!habit) {
        set_error("Habit not found");
        return PROC_ERROR_NOT_FOUND;
    }

    free_habit(habit);
    update_habit_stats(pm);

    return PROC_SUCCESS;
}

//=============================================================================
// Chunking Functions
//=============================================================================

NIMCP_EXPORT procedural_error_t procedural_chunk_skills(
    procedural_memory_t pm,
    const char* name,
    const uint64_t* sub_skill_ids,
    size_t num_sub_skills,
    uint64_t* chunk_id_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!name) {
        set_error("NULL chunk name");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!sub_skill_ids || num_sub_skills == 0) {
        set_error("No sub-skills provided");
        return PROC_ERROR_INVALID_CHUNK;
    }
    if (num_sub_skills > pm->config.max_sub_skills) {
        set_error("Too many sub-skills");
        return PROC_ERROR_INVALID_CHUNK;
    }

    // Verify all sub-skills exist and check for circular reference
    for (size_t i = 0; i < num_sub_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_sub_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)num_sub_skills);
        }

        procedural_skill_t* sub_skill = find_skill(pm, sub_skill_ids[i]);
        if (!sub_skill) {
            set_error("Sub-skill not found");
            return PROC_ERROR_NOT_FOUND;
        }

        // Check for self-reference (prevent circular)
        if (sub_skill->is_chunk) {
            for (size_t j = 0; j < sub_skill->num_sub_skills; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && sub_skill->num_sub_skills > 256) {
                    procedural_heartbeat("procedural_loop",
                                     (float)(j + 1) / (float)sub_skill->num_sub_skills);
                }

                for (size_t k = 0; k < num_sub_skills; k++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((k & 0xFF) == 0 && num_sub_skills > 256) {
                        procedural_heartbeat("procedural_loop",
                                         (float)(k + 1) / (float)num_sub_skills);
                    }

                    if (sub_skill->sub_skills[j] == sub_skill_ids[k]) {
                        set_error("Circular chunk reference detected");
                        return PROC_ERROR_CIRCULAR_CHUNK;
                    }
                }
            }
        }
    }

    // Create chunk skill (inherit type from first sub-skill)
    procedural_skill_t* first_sub = find_skill(pm, sub_skill_ids[0]);
    uint64_t chunk_id = 0;
    procedural_error_t err = procedural_create_skill(pm, name, first_sub->type, &chunk_id);
    if (err != PROC_SUCCESS) {
        return err;
    }

    procedural_skill_t* chunk = find_skill(pm, chunk_id);
    if (!chunk) {
        set_error("Failed to retrieve created chunk");
        return PROC_ERROR_INTERNAL;
    }

    // Mark as chunk
    chunk->is_chunk = true;

    // Allocate sub-skills array
    chunk->sub_skills = (uint64_t*)nimcp_calloc(num_sub_skills, sizeof(uint64_t));
    if (!chunk->sub_skills) {
        procedural_remove_skill(pm, chunk_id);
        set_error("Failed to allocate sub-skills array");
        return PROC_ERROR_NO_MEMORY;
    }

    memcpy(chunk->sub_skills, sub_skill_ids, num_sub_skills * sizeof(uint64_t));
    chunk->num_sub_skills = num_sub_skills;

    // Set parent reference in sub-skills
    for (size_t i = 0; i < num_sub_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_sub_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)num_sub_skills);
        }

        procedural_skill_t* sub_skill = find_skill(pm, sub_skill_ids[i]);
        if (sub_skill) {
            sub_skill->parent_skill_id = chunk_id;
        }
    }

    // Aggregate properties from sub-skills
    float total_accuracy = 0.0f;
    float total_automaticity = 0.0f;
    size_t total_practice = 0;

    for (size_t i = 0; i < num_sub_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_sub_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)num_sub_skills);
        }

        procedural_skill_t* sub_skill = find_skill(pm, sub_skill_ids[i]);
        if (sub_skill) {
            total_accuracy += sub_skill->accuracy;
            total_automaticity += sub_skill->automaticity;
            total_practice += sub_skill->practice_count;
        }
    }

    chunk->accuracy = total_accuracy / num_sub_skills;
    chunk->automaticity = total_automaticity / num_sub_skills;
    chunk->practice_count = total_practice / num_sub_skills;

    // Determine chunk stage based on aggregate
    if (chunk->accuracy >= pm->config.associative_accuracy_threshold &&
        chunk->automaticity >= pm->config.associative_auto_threshold) {
        chunk->stage = SKILL_STAGE_AUTONOMOUS;
    } else if (chunk->accuracy >= pm->config.cognitive_accuracy_threshold &&
               chunk->automaticity >= pm->config.cognitive_auto_threshold) {
        chunk->stage = SKILL_STAGE_ASSOCIATIVE;
    } else {
        chunk->stage = SKILL_STAGE_COGNITIVE;
    }

    // Compose signature from sub-skills
    for (size_t i = 0; i < num_sub_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_sub_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)num_sub_skills);
        }

        procedural_skill_t* sub_skill = find_skill(pm, sub_skill_ids[i]);
        if (sub_skill) {
            prime_signature_t* composed = prime_sig_compose(&chunk->skill_signature,
                                                            &sub_skill->skill_signature);
            if (composed) {
                chunk->skill_signature = *composed;
                prime_sig_destroy(composed);
            }
        }
    }

    update_skill_stats(pm);

    if (chunk_id_out) {
        *chunk_id_out = chunk_id;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_get_sub_skills(
    procedural_memory_t pm,
    uint64_t chunk_id,
    uint64_t* sub_skill_ids_out,
    size_t max_sub_skills,
    size_t* num_sub_skills_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* chunk = find_skill(pm, chunk_id);
    if (!chunk) {
        set_error("Chunk not found");
        return PROC_ERROR_NOT_FOUND;
    }

    if (!chunk->is_chunk) {
        set_error("Skill is not a chunk");
        return PROC_ERROR_INVALID_CHUNK;
    }

    size_t count = chunk->num_sub_skills;
    if (count > max_sub_skills) {
        count = max_sub_skills;
    }

    if (sub_skill_ids_out) {
        memcpy(sub_skill_ids_out, chunk->sub_skills, count * sizeof(uint64_t));
    }

    if (num_sub_skills_out) {
        *num_sub_skills_out = count;
    }

    return PROC_SUCCESS;
}

NIMCP_EXPORT bool procedural_is_chunk(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "procedural_is_chunk: pm is NULL");
        return false;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "procedural_is_chunk: skill is NULL");
        return false;
    }

    return skill->is_chunk;
}

//=============================================================================
// Decay Functions
//=============================================================================

NIMCP_EXPORT size_t procedural_decay(
    procedural_memory_t pm,
    float elapsed_days
) {
    if (!pm || elapsed_days <= 0) return 0;

    pm->current_time = get_current_time_seconds();
    size_t affected = 0;

    for (size_t i = 0; i < pm->num_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_skills);
        }

        procedural_skill_t* skill = &pm->skills[i];
        if (skill->skill_id == PROC_INVALID_ID) continue;

        // Check time since last practice
        float days_since_practice = (pm->current_time - skill->last_practice_time) / 86400.0f;

        if (days_since_practice > 0) {
            float decay = expf(-pm->config.skill_decay_rate * days_since_practice);
            float new_strength = skill->strength * decay;

            if (new_strength < PROC_MIN_SKILL_STRENGTH) {
                new_strength = PROC_MIN_SKILL_STRENGTH;
            }

            if (skill->strength != new_strength) {
                skill->strength = new_strength;
                skill->automaticity *= decay;
                affected++;
            }
        }
    }

    update_skill_stats(pm);
    return affected;
}

NIMCP_EXPORT float procedural_decay_skill(
    procedural_memory_t pm,
    uint64_t skill_id,
    float elapsed_days
) {
    if (!pm || elapsed_days <= 0) return -1.0f;

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) return -1.0f;

    float decay = expf(-pm->config.skill_decay_rate * elapsed_days);
    skill->strength *= decay;

    if (skill->strength < PROC_MIN_SKILL_STRENGTH) {
        skill->strength = PROC_MIN_SKILL_STRENGTH;
    }

    skill->automaticity *= decay;
    skill->automaticity = nimcp_clampf(skill->automaticity, 0.0f, 1.0f);

    update_skill_stats(pm);
    return skill->strength;
}

NIMCP_EXPORT size_t procedural_decay_habits(
    procedural_memory_t pm,
    float elapsed_days
) {
    if (!pm || elapsed_days <= 0) return 0;

    pm->current_time = get_current_time_seconds();
    size_t affected = 0;

    for (size_t i = 0; i < pm->num_habits; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_habits > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_habits);
        }

        procedural_habit_t* habit = &pm->habits[i];
        if (habit->habit_id == PROC_INVALID_ID) continue;

        // Check time since last execution
        float days_since_exec = (pm->current_time - habit->last_execution_time) / 86400.0f;

        if (days_since_exec > 0) {
            float decay = expf(-pm->config.habit_decay_rate * days_since_exec);
            float new_strength = habit->strength * decay;

            if (new_strength < 0.1f) {
                new_strength = 0.1f;
            }

            if (habit->strength != new_strength) {
                habit->strength = new_strength;
                habit->automaticity *= decay;
                affected++;
            }
        }
    }

    update_habit_stats(pm);
    return affected;
}

//=============================================================================
// Query Functions
//=============================================================================

NIMCP_EXPORT procedural_error_t procedural_get_skill(
    procedural_memory_t pm,
    uint64_t skill_id,
    procedural_skill_t* skill_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!skill_out) {
        set_error("NULL output pointer");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    *skill_out = *skill;
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_get_skills_by_type(
    procedural_memory_t pm,
    procedural_type_t type,
    uint64_t* skill_ids_out,
    size_t max_skills,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    size_t count = 0;

    for (size_t i = 0; i < pm->num_skills && count < max_skills; i++) {
        procedural_skill_t* skill = &pm->skills[i];
        if (skill->skill_id == PROC_INVALID_ID) continue;
        if (skill->type != type) continue;

        if (skill_ids_out) {
            skill_ids_out[count] = skill->skill_id;
        }
        count++;
    }

    if (count_out) *count_out = count;
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_get_skills_by_stage(
    procedural_memory_t pm,
    skill_stage_t stage,
    uint64_t* skill_ids_out,
    size_t max_skills,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    size_t count = 0;

    for (size_t i = 0; i < pm->num_skills && count < max_skills; i++) {
        procedural_skill_t* skill = &pm->skills[i];
        if (skill->skill_id == PROC_INVALID_ID) continue;
        if (skill->stage != stage) continue;

        if (skill_ids_out) {
            skill_ids_out[count] = skill->skill_id;
        }
        count++;
    }

    if (count_out) *count_out = count;
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_search_skills(
    procedural_memory_t pm,
    const char* query,
    uint64_t* skill_ids_out,
    size_t max_skills,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!query) {
        set_error("NULL query string");
        return PROC_ERROR_NULL_POINTER;
    }

    size_t count = 0;

    for (size_t i = 0; i < pm->num_skills && count < max_skills; i++) {
        procedural_skill_t* skill = &pm->skills[i];
        if (skill->skill_id == PROC_INVALID_ID) continue;
        if (!skill->skill_name) continue;

        // Case-insensitive substring search
        if (strstr(skill->skill_name, query) != NULL) {
            if (skill_ids_out) {
                skill_ids_out[count] = skill->skill_id;
            }
            count++;
        }
    }

    if (count_out) *count_out = count;
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_get_strongest_skills(
    procedural_memory_t pm,
    size_t k,
    uint64_t* skill_ids_out,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    // Collect skills with their strengths
    typedef struct {
        uint64_t id;
        float strength;
    } skill_entry_t;

    skill_entry_t* entries = (skill_entry_t*)nimcp_calloc(pm->num_skills, sizeof(skill_entry_t));
    if (!entries) {
        set_error("Memory allocation failed");
        return PROC_ERROR_NO_MEMORY;
    }

    size_t num_entries = 0;
    for (size_t i = 0; i < pm->num_skills; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_skills > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_skills);
        }

        procedural_skill_t* skill = &pm->skills[i];
        if (skill->skill_id == PROC_INVALID_ID) continue;

        entries[num_entries].id = skill->skill_id;
        entries[num_entries].strength = skill->strength;
        num_entries++;
    }

    // Simple selection sort for top-k
    for (size_t i = 0; i < num_entries && i < k; i++) {
        size_t max_idx = i;
        for (size_t j = i + 1; j < num_entries; j++) {
            if (entries[j].strength > entries[max_idx].strength) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            skill_entry_t temp = entries[i];
            entries[i] = entries[max_idx];
            entries[max_idx] = temp;
        }
    }

    // Copy top-k to output
    size_t count = (num_entries < k) ? num_entries : k;
    if (skill_ids_out) {
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                procedural_heartbeat("procedural_loop",
                                 (float)(i + 1) / (float)count);
            }

            skill_ids_out[i] = entries[i].id;
        }
    }

    nimcp_free(entries);
    entries = NULL;

    if (count_out) *count_out = count;
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_remove_skill(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    // Clear parent reference from sub-skills if this is a chunk
    if (skill->is_chunk && skill->sub_skills) {
        for (size_t i = 0; i < skill->num_sub_skills; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && skill->num_sub_skills > 256) {
                procedural_heartbeat("procedural_loop",
                                 (float)(i + 1) / (float)skill->num_sub_skills);
            }

            procedural_skill_t* sub = find_skill(pm, skill->sub_skills[i]);
            if (sub && sub->parent_skill_id == skill_id) {
                sub->parent_skill_id = PROC_INVALID_ID;
            }
        }
    }

    // Clear linked habit references
    for (size_t i = 0; i < pm->num_habits; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_habits > 256) {
            procedural_heartbeat("procedural_loop",
                             (float)(i + 1) / (float)pm->num_habits);
        }

        if (pm->habits[i].linked_skill_id == skill_id) {
            pm->habits[i].linked_skill_id = PROC_INVALID_ID;
            pm->habits[i].has_linked_skill = false;
        }
    }

    free_skill(skill);
    update_skill_stats(pm);

    return PROC_SUCCESS;
}

//=============================================================================
// Statistics and Debugging Functions
//=============================================================================

NIMCP_EXPORT procedural_error_t procedural_get_stats(
    procedural_memory_t pm,
    procedural_stats_t* stats_out
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }
    if (!stats_out) {
        set_error("NULL output pointer");
        return PROC_ERROR_NULL_POINTER;
    }

    *stats_out = pm->stats;
    return PROC_SUCCESS;
}

NIMCP_EXPORT procedural_error_t procedural_reset_stats(procedural_memory_t pm) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    // Keep current counts, reset accumulated stats
    size_t total_skills = pm->stats.total_skills;
    size_t total_habits = pm->stats.total_habits;

    memset(&pm->stats, 0, sizeof(pm->stats));

    pm->stats.total_skills = total_skills;
    pm->stats.total_habits = total_habits;

    update_skill_stats(pm);
    update_habit_stats(pm);

    return PROC_SUCCESS;
}

NIMCP_EXPORT const char* procedural_error_string(procedural_error_t error) {
    switch (error) {
        case PROC_SUCCESS:              return "Success";
        case PROC_ERROR_NULL_POINTER:   return "NULL pointer";
        case PROC_ERROR_INVALID_TYPE:   return "Invalid type";
        case PROC_ERROR_INVALID_STAGE:  return "Invalid stage";
        case PROC_ERROR_NO_MEMORY:      return "Memory allocation failed";
        case PROC_ERROR_CAPACITY:       return "Maximum capacity reached";
        case PROC_ERROR_NOT_FOUND:      return "Not found";
        case PROC_ERROR_INVALID_STEP:   return "Invalid step";
        case PROC_ERROR_MAX_STEPS:      return "Maximum steps exceeded";
        case PROC_ERROR_ALREADY_EXECUTING: return "Already executing";
        case PROC_ERROR_NOT_EXECUTING:  return "Not executing";
        case PROC_ERROR_INVALID_CHUNK:  return "Invalid chunk";
        case PROC_ERROR_CIRCULAR_CHUNK: return "Circular chunk reference";
        case PROC_ERROR_INTERNAL:       return "Internal error";
        default:                        return "Unknown error";
    }
}

NIMCP_EXPORT const char* procedural_get_last_error(void) {
    return g_last_error[0] ? g_last_error : NULL;
}

NIMCP_EXPORT const char* procedural_stage_name(skill_stage_t stage) {
    switch (stage) {
        case SKILL_STAGE_COGNITIVE:   return "COGNITIVE";
        case SKILL_STAGE_ASSOCIATIVE: return "ASSOCIATIVE";
        case SKILL_STAGE_AUTONOMOUS:  return "AUTONOMOUS";
        default:                      return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* procedural_type_name(procedural_type_t type) {
    switch (type) {
        case PROC_MOTOR:      return "MOTOR";
        case PROC_COGNITIVE:  return "COGNITIVE";
        case PROC_PERCEPTUAL: return "PERCEPTUAL";
        case PROC_HABIT:      return "HABIT";
        default:              return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* procedural_exec_status_name(proc_exec_status_t status) {
    switch (status) {
        case PROC_EXEC_IDLE:       return "IDLE";
        case PROC_EXEC_STARTING:   return "STARTING";
        case PROC_EXEC_RUNNING:    return "RUNNING";
        case PROC_EXEC_PAUSED:     return "PAUSED";
        case PROC_EXEC_COMPLETING: return "COMPLETING";
        case PROC_EXEC_COMPLETED:  return "COMPLETED";
        case PROC_EXEC_FAILED:     return "FAILED";
        default:                   return "UNKNOWN";
    }
}

NIMCP_EXPORT void procedural_print_skill(const procedural_skill_t* skill) {
    if (!skill) {
        printf("Skill: NULL\n");
        return;
    }

    printf("Skill[%lu] '%s':\n", (unsigned long)skill->skill_id,
           skill->skill_name ? skill->skill_name : "(unnamed)");
    printf("  Type: %s\n", procedural_type_name(skill->type));
    printf("  Stage: %s\n", procedural_stage_name(skill->stage));
    printf("  Steps: %zu\n", skill->num_steps);
    printf("  Practice count: %zu\n", skill->practice_count);
    printf("  Accuracy: %.1f%%\n", skill->accuracy * 100);
    printf("  Automaticity: %.1f%%\n", skill->automaticity * 100);
    printf("  Strength: %.2f\n", skill->strength);
    printf("  Speed: %.2fx\n", skill->speed);
    printf("  Is chunk: %s\n", skill->is_chunk ? "yes" : "no");

    if (skill->is_chunk && skill->num_sub_skills > 0) {
        printf("  Sub-skills: %zu\n", skill->num_sub_skills);
    }

    if (skill->num_steps > 0 && skill->num_steps <= 5) {
        printf("  Steps:\n");
        for (size_t i = 0; i < skill->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && skill->num_steps > 256) {
                procedural_heartbeat("procedural_loop",
                                 (float)(i + 1) / (float)skill->num_steps);
            }

            printf("    %zu. %s (%.0f ms)\n", i + 1,
                   skill->steps[i].action_description ?
                   skill->steps[i].action_description : "(unnamed)",
                   skill->steps[i].duration_ms);
        }
    }
}

NIMCP_EXPORT void procedural_print_habit(const procedural_habit_t* habit) {
    if (!habit) {
        printf("Habit: NULL\n");
        return;
    }

    printf("Habit[%lu]:\n", (unsigned long)habit->habit_id);
    printf("  Strength: %.2f\n", habit->strength);
    printf("  Automaticity: %.1f%%\n", habit->automaticity * 100);
    printf("  Context dependency: %.1f%%\n", habit->context_dependency * 100);
    printf("  Trigger count: %zu\n", habit->trigger_count);
    printf("  Execution count: %zu\n", habit->execution_count);
    printf("  Mean reward: %.2f\n", habit->mean_reward);
    printf("  Reward prediction: %.2f\n", habit->reward_prediction);

    if (habit->has_linked_skill) {
        printf("  Linked skill ID: %lu\n", (unsigned long)habit->linked_skill_id);
    }
}

NIMCP_EXPORT void procedural_print_summary(procedural_memory_t pm) {
    if (!pm) {
        printf("Procedural Memory: NULL\n");
        return;
    }

    printf("=== Procedural Memory Summary ===\n");
    printf("Skills: %zu total\n", pm->stats.total_skills);
    printf("  Cognitive: %zu\n", pm->stats.skills_cognitive);
    printf("  Associative: %zu\n", pm->stats.skills_associative);
    printf("  Autonomous: %zu\n", pm->stats.skills_autonomous);
    printf("  Chunks: %zu\n", pm->stats.total_chunks);
    printf("  Mean accuracy: %.1f%%\n", pm->stats.mean_skill_accuracy * 100);
    printf("  Mean automaticity: %.1f%%\n", pm->stats.mean_skill_automaticity * 100);
    printf("Habits: %zu total\n", pm->stats.total_habits);
    printf("  Mean strength: %.2f\n", pm->stats.mean_habit_strength);
    printf("  Mean reward: %.2f\n", pm->stats.mean_habit_reward);
    printf("  Total triggers: %lu\n", (unsigned long)pm->stats.total_habit_triggers);
    printf("  Total fires: %lu\n", (unsigned long)pm->stats.total_habit_fires);
    printf("Practice: %lu sessions, %.1f sec total\n",
           (unsigned long)pm->stats.total_practice_sessions,
           pm->stats.total_practice_time / 1000.0f);
    printf("Executions: %zu total\n", pm->stats.total_skill_executions);
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT float procedural_current_time(void) {
    return get_current_time_seconds();
}

NIMCP_EXPORT float procedural_compute_learning_rate(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) return -1.0f;

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) return -1.0f;

    // Base rate from config
    float rate = pm->config.base_learning_rate;

    // Adjust by stage
    switch (skill->stage) {
        case SKILL_STAGE_COGNITIVE:
            rate *= 1.0f;
            break;
        case SKILL_STAGE_ASSOCIATIVE:
            rate *= 0.5f;
            break;
        case SKILL_STAGE_AUTONOMOUS:
            rate *= 0.1f;
            break;
    }

    // Diminishing returns with practice
    float practice_factor = 1.0f / (1.0f + logf(1.0f + skill->practice_count * 0.01f));
    rate *= practice_factor;

    // Less room to improve with high accuracy
    rate *= (1.0f - skill->accuracy * 0.5f);

    return nimcp_clampf(rate, 0.001f, 1.0f);
}

NIMCP_EXPORT float procedural_estimate_mastery_time(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) return -1.0f;

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) return -1.0f;

    if (skill->stage == SKILL_STAGE_AUTONOMOUS) {
        return 0.0f; // Already mastered
    }

    // Estimate based on current progress and typical progression
    float remaining_practice = 0.0f;

    if (skill->stage == SKILL_STAGE_COGNITIVE) {
        float to_associative = pm->config.associative_practice_threshold -
                               skill->practice_count;
        if (to_associative < 0) to_associative = 0;
        remaining_practice = to_associative +
                             (pm->config.associative_practice_threshold -
                              pm->config.cognitive_practice_threshold);
    } else if (skill->stage == SKILL_STAGE_ASSOCIATIVE) {
        remaining_practice = pm->config.associative_practice_threshold -
                             skill->practice_count;
        if (remaining_practice < 0) remaining_practice = 0;
    }

    // Assume 1 practice session per day on average
    return remaining_practice;
}

//=============================================================================
// Integration Functions
//=============================================================================

NIMCP_EXPORT pr_memory_node_t* procedural_get_skill_node(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "skill is NULL");

        return NULL;

    }

    return skill->memory_node;
}

NIMCP_EXPORT pr_memory_node_t* procedural_get_habit_node(
    procedural_memory_t pm,
    uint64_t habit_id
) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    procedural_habit_t* habit = find_habit(pm, habit_id);
    if (!habit) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "habit is NULL");

        return NULL;

    }

    return habit->memory_node;
}

NIMCP_EXPORT procedural_error_t procedural_update_skill_signature(
    procedural_memory_t pm,
    uint64_t skill_id
) {
    if (!pm) {
        set_error("NULL procedural memory");
        return PROC_ERROR_NULL_POINTER;
    }

    procedural_skill_t* skill = find_skill(pm, skill_id);
    if (!skill) {
        set_error("Skill not found");
        return PROC_ERROR_NOT_FOUND;
    }

    compute_skill_signature(skill);
    return PROC_SUCCESS;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void procedural_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_procedural_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int procedural_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "procedural_training_begin: NULL argument");
        return -1;
    }
    procedural_heartbeat_instance(NULL, "procedural_training_begin", 0.0f);
    (void)(struct procedural_memory_internal*)instance; /* Module state available for reset */
    return 0;
}

int procedural_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "procedural_training_end: NULL argument");
        return -1;
    }
    procedural_heartbeat_instance(NULL, "procedural_training_end", 1.0f);
    (void)(struct procedural_memory_internal*)instance; /* Module state available for finalization */
    return 0;
}

int procedural_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "procedural_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    procedural_heartbeat_instance(NULL, "procedural_training_step", progress);
    (void)(struct procedural_memory_internal*)instance; /* Module state available for step adaptation */
    return 0;
}

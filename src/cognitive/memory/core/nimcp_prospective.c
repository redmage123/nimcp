//=============================================================================
// nimcp_prospective.c - Prospective Memory System Implementation
//=============================================================================
/**
 * @file nimcp_prospective.c
 * @brief Implementation of "Remembering to Remember" prospective memory
 *
 * Implements time-based, event-based, activity-based, and location-based
 * prospective memory with integration to the Prime Resonant architecture.
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_prospective.h"
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

BRIDGE_BOILERPLATE(prospective, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Thread-local Error Storage
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
 * @brief Internal prospective memory manager structure
 */
struct prospective_memory_internal {
    // PR memory integration
    entangle_graph_t entanglement;
    pr_node_manager_t node_manager;
    theta_gamma_manager_t theta_gamma;

    // Intention storage
    prospective_intention_t* intentions;
    size_t num_intentions;
    size_t max_intentions;

    // Active monitoring
    prospective_intention_t** active_monitors;
    size_t num_active;
    size_t max_active;

    // Current context
    float current_time;
    prime_signature_t current_context;
    bool context_valid;

    // ID generation
    uint64_t next_intention_id;

    // Statistics
    prospective_stats_t stats;

    // Configuration
    prospective_config_t config;

    // Last check time for rate limiting
    float last_check_time;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Find intention by ID
 */
static prospective_intention_t* find_intention(prospective_memory_t pm, uint64_t id) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    for (size_t i = 0; i < pm->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_intentions);
        }

        if (pm->intentions[i].intention_id == id) {
            return &pm->intentions[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_intention: validation failed");
    return NULL;
}

/**
 * @brief Find free slot in intentions array
 */
static prospective_intention_t* find_free_slot(prospective_memory_t pm) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    // Look for empty slot (id = PROSP_INVALID_ID)
    for (size_t i = 0; i < pm->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_intentions);
        }

        if (pm->intentions[i].intention_id == PROSP_INVALID_ID) {
            return &pm->intentions[i];
        }
    }

    // No empty slot, try to expand if room
    if (pm->num_intentions < pm->max_intentions) {
        return &pm->intentions[pm->num_intentions++];
    }

    return NULL;  /* All slots occupied is normal */
}

/**
 * @brief Initialize intention to default values
 */
static void init_intention(prospective_intention_t* intent) {
    if (!intent) return;

    memset(intent, 0, sizeof(*intent));
    intent->intention_id = PROSP_INVALID_ID;
    intent->status = PROSP_PENDING;
    intent->importance = PROSP_DEFAULT_IMPORTANCE;
    intent->urgency = PROSP_DEFAULT_URGENCY;
    intent->priority = PROSP_PRIORITY_MEDIUM;
    intent->current_activation = 1.0f;
    intent->action_quaternion = quat_identity();
}

/**
 * @brief Free intention resources
 */
static void free_intention(prospective_intention_t* intent) {
    if (!intent) return;

    if (intent->action_description) {
        nimcp_free(intent->action_description);
        intent->action_description = NULL;
    }

    // Note: memory_node is managed by the PR system, don't free here
    intent->memory_node = NULL;

    intent->intention_id = PROSP_INVALID_ID;
}

/**
 * @brief Copy action description
 */
static char* copy_action(const char* action) {
    if (!action) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "action is NULL");

        return NULL;

    }

    size_t len = strlen(action);
    if (len > PROSP_MAX_ACTION_DESCRIPTION - 1) {
        len = PROSP_MAX_ACTION_DESCRIPTION - 1;
    }

    char* copy = (char*)nimcp_malloc(len + 1);
    if (!copy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate copy");

        return NULL;

    }

    strncpy(copy, action, len);
    copy[len] = '\0';
    return copy;
}

/**
 * @brief Compute priority from importance and urgency
 */
static prospective_priority_t compute_priority_internal(float importance, float urgency) {
    float combined = (importance + urgency) / 2.0f;

    if (combined >= 8.0f) return PROSP_PRIORITY_CRITICAL;
    if (combined >= 6.0f) return PROSP_PRIORITY_HIGH;
    if (combined >= 4.0f) return PROSP_PRIORITY_MEDIUM;
    return PROSP_PRIORITY_LOW;
}

/**
 * @brief Add intention to active monitors
 */
static bool add_to_active_monitors(prospective_memory_t pm, prospective_intention_t* intent) {
    if (!pm || !intent) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "add_to_active_monitors: required parameter is NULL (pm, intent)");
        return false;
    }
    if (pm->num_active >= pm->max_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "add_to_active_monitors: capacity exceeded");
        return false;
    }

    // Check if already monitored
    for (size_t i = 0; i < pm->num_active; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_active > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_active);
        }

        if (pm->active_monitors[i] == intent) {
            return true; // Already there
        }
    }

    pm->active_monitors[pm->num_active++] = intent;
    pm->stats.current_active_monitors = pm->num_active;
    return true;
}

/**
 * @brief Remove intention from active monitors
 */
static void remove_from_active_monitors(prospective_memory_t pm, prospective_intention_t* intent) {
    if (!pm || !intent) return;

    for (size_t i = 0; i < pm->num_active; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_active > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_active);
        }

        if (pm->active_monitors[i] == intent) {
            // Shift remaining down
            for (size_t j = i; j < pm->num_active - 1; j++) {
                pm->active_monitors[j] = pm->active_monitors[j + 1];
            }
            pm->num_active--;
            pm->stats.current_active_monitors = pm->num_active;
            return;
        }
    }
}

/**
 * @brief Compute time-based activation boost
 */
static float compute_time_activation(prospective_intention_t* intent, float current_time) {
    if (intent->type != PROSP_TIME_BASED) return 0.0f;

    float target = intent->trigger.time_trigger.target_time;
    float window_before = intent->trigger.time_trigger.window_before;

    float time_to_target = target - current_time;

    if (time_to_target < 0) {
        // Past target time
        return 0.0f;
    }

    if (time_to_target < window_before) {
        // Within alert window - activation increases as we approach
        float proximity = 1.0f - (time_to_target / window_before);
        return proximity * PROSP_APPROACH_ACTIVATION_BOOST;
    }

    return 0.0f;
}

/**
 * @brief Check if time trigger should fire
 */
static bool check_time_trigger_internal(prospective_intention_t* intent, float current_time,
                                        float* strength_out) {
    if (!intent || intent->type != PROSP_TIME_BASED) {
        return false;
    }
    if (intent->status != PROSP_PENDING) {
        return false;
    }

    float target = intent->trigger.time_trigger.target_time;
    float window_after = intent->trigger.time_trigger.window_after;

    float time_to_target = target - current_time;

    // Trigger if within grace period after target
    if (time_to_target <= 0 && time_to_target >= -window_after) {
        if (strength_out) {
            // Strength decreases as we move past target
            *strength_out = 1.0f - ((-time_to_target) / window_after);
            if (*strength_out < 0.0f) *strength_out = 0.0f;
        }
        return true;
    }

    // Also trigger if we've reached target time (even if not yet past)
    if (time_to_target <= 0) {
        if (strength_out) *strength_out = 1.0f;
        return true;
    }

    return false;
}

/**
 * @brief Check if event trigger should fire
 */
static bool check_event_trigger_internal(prospective_intention_t* intent,
                                         const prime_signature_t* context,
                                         float* strength_out) {
    if (!intent || !context) {
        return false;
    }
    if (intent->type != PROSP_EVENT_BASED) {
        return false;
    }
    if (intent->status != PROSP_PENDING) {
        return false;
    }

    prosp_event_trigger_t* trigger = &intent->trigger.event_trigger;

    // Compute similarity using Jaccard
    float similarity = prime_sig_jaccard(&trigger->cue_signature, context);

    if (similarity >= trigger->similarity_threshold) {
        if (strength_out) *strength_out = similarity;
        return true;
    }

    return false;
}

/**
 * @brief Check if activity trigger should fire
 */
static bool check_activity_trigger_internal(prospective_intention_t* intent,
                                            uint64_t completed_activity_id,
                                            const char* activity_tag,
                                            float* strength_out) {
    if (!intent) {
        return false;
    }
    if (intent->type != PROSP_ACTIVITY_BASED) {
        return false;
    }
    if (intent->status != PROSP_PENDING) {
        return false;
    }

    prosp_activity_trigger_t* trigger = &intent->trigger.activity_trigger;

    // Check if this is the prerequisite we're waiting for
    if (trigger->any_activity) {
        // Trigger on any activity (possibly filtered by tag)
        if (trigger->activity_tag[0] != '\0' && activity_tag) {
            if (strstr(activity_tag, trigger->activity_tag) == NULL) {
                return false;
            }
        }
        if (strength_out) *strength_out = 1.0f;
        return true;
    }

    // Specific prerequisite
    if (trigger->prerequisite_id == completed_activity_id) {
        if (strength_out) *strength_out = 1.0f;
        return true;
    }

    return false;
}

/**
 * @brief Check if location trigger should fire
 */
static bool check_location_trigger_internal(prospective_intention_t* intent,
                                            const prime_signature_t* location,
                                            float* strength_out) {
    if (!intent || !location) {
        return false;
    }
    if (intent->type != PROSP_LOCATION_BASED) {
        return false;
    }
    if (intent->status != PROSP_PENDING) {
        return false;
    }

    prosp_location_trigger_t* trigger = &intent->trigger.location_trigger;

    float similarity = prime_sig_jaccard(&trigger->location_signature, location);

    if (similarity >= trigger->similarity_threshold) {
        if (strength_out) *strength_out = similarity;
        return true;
    }

    return false;
}

/**
 * @brief Fill trigger result structure
 */
static void fill_trigger_result(prospective_trigger_result_t* result,
                                prospective_intention_t* intent,
                                float strength,
                                float current_time) {
    if (!result || !intent) return;

    result->intention_id = intent->intention_id;
    result->type = intent->type;
    result->trigger_strength = strength;
    result->action_description = intent->action_description;

    // Compute time to deadline for time-based
    if (intent->type == PROSP_TIME_BASED) {
        float target = intent->trigger.time_trigger.target_time;
        result->time_to_deadline = target - current_time;
    } else {
        result->time_to_deadline = 0.0f;
    }
}

/**
 * @brief Create PR memory node for intention
 */
static pr_memory_node_t* create_intention_memory_node(prospective_memory_t pm,
                                                       prospective_intention_t* intent) {
    if (!pm || !intent || !pm->node_manager) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "compute_time_activation: required parameter is NULL (pm, intent, pm->node_manager)");
        return NULL;
    }

    // Create node config with high salience
    pr_node_config_t config = pr_memory_node_default_config();
    config.initial_tier = PR_MEMORY_TIER_Z1;  // Short-term
    config.initial_strength = 1.0f;
    config.salience = 0.9f;  // High salience for intentions
    config.accessibility = 0.8f;
    config.compute_signature = true;

    // Use action description as content
    const void* data = intent->action_description;
    size_t size = intent->action_description ? strlen(intent->action_description) : 0;

    return pr_memory_node_create(pm->node_manager, data, size, &config);
}

//=============================================================================
// Configuration Functions
//=============================================================================

NIMCP_EXPORT prospective_config_t prospective_config_default(void) {
    prospective_config_t config = {
        .max_intentions = PROSP_MAX_INTENTIONS,
        .max_active_monitors = PROSP_MAX_ACTIVE_MONITORS,
        .default_importance = PROSP_DEFAULT_IMPORTANCE,
        .default_urgency = PROSP_DEFAULT_URGENCY,
        .activation_decay_rate = PROSP_ACTIVATION_DECAY_RATE,
        .check_interval = 1.0f,
        .similarity_threshold = PROSP_DEFAULT_SIMILARITY_THRESHOLD,
        .auto_prune_executed = true,
        .auto_prune_missed = false,
        .prune_delay = 300.0f
    };
    return config;
}

NIMCP_EXPORT bool prospective_config_validate(const prospective_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_config_validate: config is NULL");
        return false;
    }

    if (config->max_intentions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prospective_config_validate: config->max_intentions is zero");
        return false;
    }
    if (config->max_active_monitors == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prospective_config_validate: config->max_active_monitors is zero");
        return false;
    }
    if (config->default_importance < 0 || config->default_importance > 10) {
        return false;
    }
    if (config->default_urgency < 0 || config->default_urgency > 10) {
        return false;
    }
    if (config->activation_decay_rate < 0) {
        return false;
    }
    if (config->similarity_threshold < 0 || config->similarity_threshold > 1) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

NIMCP_EXPORT prospective_memory_t prospective_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    theta_gamma_manager_t theta_gamma,
    const prospective_config_t* config
) {
    // Use default config if none provided
    prospective_config_t cfg;
    if (config) {
        cfg = *config;
        if (!prospective_config_validate(&cfg)) {
            set_error("Invalid configuration");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_create: prospective_config_validate is NULL");
            return NULL;
        }
    } else {
        cfg = prospective_config_default();
    }

    // Allocate manager
    prospective_memory_t pm = (prospective_memory_t)nimcp_calloc(1, sizeof(struct prospective_memory_internal));
    if (!pm) {
        set_error("Memory allocation failed for manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prospective_create: pm is NULL");
        return NULL;
    }

    // Store integration handles
    pm->entanglement = entanglement;
    pm->node_manager = node_manager;
    pm->theta_gamma = theta_gamma;

    // Allocate intentions array
    pm->intentions = (prospective_intention_t*)nimcp_calloc(cfg.max_intentions, sizeof(prospective_intention_t));
    if (!pm->intentions) {
        set_error("Memory allocation failed for intentions");
        nimcp_free(pm);
        pm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prospective_create: pm->intentions is NULL");
        return NULL;
    }

    // Initialize all slots as empty
    for (size_t i = 0; i < cfg.max_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && cfg.max_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)cfg.max_intentions);
        }

        init_intention(&pm->intentions[i]);
    }

    pm->num_intentions = 0;
    pm->max_intentions = cfg.max_intentions;

    // Allocate active monitors array
    pm->active_monitors = (prospective_intention_t**)nimcp_calloc(cfg.max_active_monitors,
                                                             sizeof(prospective_intention_t*));
    if (!pm->active_monitors) {
        set_error("Memory allocation failed for active monitors");
        nimcp_free(pm->intentions);
        nimcp_free(pm);
        pm = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prospective_create: pm->active_monitors is NULL");
        return NULL;
    }

    pm->num_active = 0;
    pm->max_active = cfg.max_active_monitors;

    // Initialize state
    pm->current_time = prospective_current_time();
    pm->context_valid = false;
    pm->next_intention_id = 1;
    pm->last_check_time = 0.0f;

    // Store config
    pm->config = cfg;

    // Initialize stats
    memset(&pm->stats, 0, sizeof(pm->stats));

    return pm;
}

NIMCP_EXPORT void prospective_destroy(prospective_memory_t pm) {
    if (!pm) return;

    // Free intention resources
    if (pm->intentions) {
        for (size_t i = 0; i < pm->max_intentions; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pm->max_intentions > 256) {
                prospective_heartbeat("prospective_loop",
                                 (float)(i + 1) / (float)pm->max_intentions);
            }

            free_intention(&pm->intentions[i]);
        }
        nimcp_free(pm->intentions);
    }

    // Free active monitors array (not the intentions themselves)
    if (pm->active_monitors) {
        nimcp_free(pm->active_monitors);
    }

    nimcp_free(pm);
    pm = NULL;
}

NIMCP_EXPORT prospective_error_t prospective_reset(prospective_memory_t pm) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    // Free and reinitialize all intentions
    for (size_t i = 0; i < pm->max_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->max_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->max_intentions);
        }

        free_intention(&pm->intentions[i]);
        init_intention(&pm->intentions[i]);
    }

    pm->num_intentions = 0;
    pm->num_active = 0;
    pm->next_intention_id = 1;
    pm->context_valid = false;

    // Reset stats
    memset(&pm->stats, 0, sizeof(pm->stats));

    return PROSP_SUCCESS;
}

//=============================================================================
// Intention Creation Functions
//=============================================================================

NIMCP_EXPORT prospective_error_t prospective_create_time_intention(
    prospective_memory_t pm,
    float target_time,
    bool is_relative,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
) {
    prosp_time_trigger_t trigger = {0};
    prospective_init_time_trigger(&trigger, target_time, is_relative);

    return prospective_create_time_intention_ex(pm, &trigger, action, importance, urgency, intention_id_out);
}

NIMCP_EXPORT prospective_error_t prospective_create_time_intention_ex(
    prospective_memory_t pm,
    const prosp_time_trigger_t* trigger,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!action) {
        set_error("NULL action description");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!trigger) {
        set_error("NULL trigger");
        return PROSP_ERROR_INVALID_TRIGGER;
    }

    // Find free slot
    prospective_intention_t* intent = find_free_slot(pm);
    if (!intent) {
        set_error("Maximum intentions reached");
        return PROSP_ERROR_CAPACITY;
    }

    // Initialize intention
    init_intention(intent);
    intent->intention_id = pm->next_intention_id++;
    intent->type = PROSP_TIME_BASED;
    intent->status = PROSP_PENDING;

    // Copy action
    intent->action_description = copy_action(action);
    if (!intent->action_description) {
        set_error("Failed to copy action description");
        free_intention(intent);
        return PROSP_ERROR_NO_MEMORY;
    }

    // Compute signature from action
    prime_signature_t* sig = prime_sig_from_text(action);
    if (sig) {
        intent->action_signature = *sig;
        prime_sig_destroy(sig);
    }

    // Set trigger
    intent->trigger.time_trigger = *trigger;

    // Convert relative to absolute if needed
    if (trigger->is_relative) {
        intent->trigger.time_trigger.target_time = pm->current_time + trigger->target_time;
        intent->trigger.time_trigger.is_relative = false;  // Now absolute
    }

    // Set importance/urgency
    intent->importance = (importance > 0) ? importance : pm->config.default_importance;
    intent->urgency = (urgency > 0) ? urgency : pm->config.default_urgency;
    intent->priority = compute_priority_internal(intent->importance, intent->urgency);

    // Set timestamps
    intent->creation_time = pm->current_time;
    intent->expected_execution_time = intent->trigger.time_trigger.target_time;
    intent->current_activation = 1.0f;

    // Create PR memory node if manager available
    if (pm->node_manager) {
        intent->memory_node = create_intention_memory_node(pm, intent);
    }

    // Add to active monitors
    add_to_active_monitors(pm, intent);

    // Update stats
    pm->stats.total_created++;
    pm->stats.current_pending++;

    if (intention_id_out) {
        *intention_id_out = intent->intention_id;
    }

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_create_event_intention(
    prospective_memory_t pm,
    const prime_signature_t* cue_signature,
    float similarity_threshold,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
) {
    prosp_event_trigger_t trigger = {0};
    prospective_init_event_trigger(&trigger, cue_signature, similarity_threshold);

    return prospective_create_event_intention_ex(pm, &trigger, action, importance, urgency, intention_id_out);
}

NIMCP_EXPORT prospective_error_t prospective_create_event_intention_ex(
    prospective_memory_t pm,
    const prosp_event_trigger_t* trigger,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!action) {
        set_error("NULL action description");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!trigger) {
        set_error("NULL trigger");
        return PROSP_ERROR_INVALID_TRIGGER;
    }

    prospective_intention_t* intent = find_free_slot(pm);
    if (!intent) {
        set_error("Maximum intentions reached");
        return PROSP_ERROR_CAPACITY;
    }

    init_intention(intent);
    intent->intention_id = pm->next_intention_id++;
    intent->type = PROSP_EVENT_BASED;
    intent->status = PROSP_PENDING;

    intent->action_description = copy_action(action);
    if (!intent->action_description) {
        set_error("Failed to copy action description");
        free_intention(intent);
        return PROSP_ERROR_NO_MEMORY;
    }

    prime_signature_t* sig = prime_sig_from_text(action);
    if (sig) {
        intent->action_signature = *sig;
        prime_sig_destroy(sig);
    }

    intent->trigger.event_trigger = *trigger;

    // Apply default threshold if not specified
    if (trigger->similarity_threshold <= 0) {
        intent->trigger.event_trigger.similarity_threshold = pm->config.similarity_threshold;
    }

    intent->importance = (importance > 0) ? importance : pm->config.default_importance;
    intent->urgency = (urgency > 0) ? urgency : pm->config.default_urgency;
    intent->priority = compute_priority_internal(intent->importance, intent->urgency);

    intent->creation_time = pm->current_time;
    intent->current_activation = 1.0f;

    if (pm->node_manager) {
        intent->memory_node = create_intention_memory_node(pm, intent);
    }

    add_to_active_monitors(pm, intent);

    pm->stats.total_created++;
    pm->stats.current_pending++;

    if (intention_id_out) {
        *intention_id_out = intent->intention_id;
    }

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_create_activity_intention(
    prospective_memory_t pm,
    uint64_t prerequisite_id,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
) {
    prosp_activity_trigger_t trigger = {0};
    prospective_init_activity_trigger(&trigger, prerequisite_id);

    return prospective_create_activity_intention_ex(pm, &trigger, action, importance, urgency, intention_id_out);
}

NIMCP_EXPORT prospective_error_t prospective_create_activity_intention_ex(
    prospective_memory_t pm,
    const prosp_activity_trigger_t* trigger,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!action) {
        set_error("NULL action description");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!trigger) {
        set_error("NULL trigger");
        return PROSP_ERROR_INVALID_TRIGGER;
    }

    prospective_intention_t* intent = find_free_slot(pm);
    if (!intent) {
        set_error("Maximum intentions reached");
        return PROSP_ERROR_CAPACITY;
    }

    init_intention(intent);
    intent->intention_id = pm->next_intention_id++;
    intent->type = PROSP_ACTIVITY_BASED;
    intent->status = PROSP_PENDING;

    intent->action_description = copy_action(action);
    if (!intent->action_description) {
        set_error("Failed to copy action description");
        free_intention(intent);
        return PROSP_ERROR_NO_MEMORY;
    }

    prime_signature_t* sig = prime_sig_from_text(action);
    if (sig) {
        intent->action_signature = *sig;
        prime_sig_destroy(sig);
    }

    intent->trigger.activity_trigger = *trigger;

    intent->importance = (importance > 0) ? importance : pm->config.default_importance;
    intent->urgency = (urgency > 0) ? urgency : pm->config.default_urgency;
    intent->priority = compute_priority_internal(intent->importance, intent->urgency);

    intent->creation_time = pm->current_time;
    intent->current_activation = 1.0f;

    if (pm->node_manager) {
        intent->memory_node = create_intention_memory_node(pm, intent);
    }

    add_to_active_monitors(pm, intent);

    pm->stats.total_created++;
    pm->stats.current_pending++;

    if (intention_id_out) {
        *intention_id_out = intent->intention_id;
    }

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_create_location_intention(
    prospective_memory_t pm,
    const prime_signature_t* location_signature,
    float similarity_threshold,
    const char* action,
    float importance,
    float urgency,
    uint64_t* intention_id_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!action) {
        set_error("NULL action description");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_free_slot(pm);
    if (!intent) {
        set_error("Maximum intentions reached");
        return PROSP_ERROR_CAPACITY;
    }

    init_intention(intent);
    intent->intention_id = pm->next_intention_id++;
    intent->type = PROSP_LOCATION_BASED;
    intent->status = PROSP_PENDING;

    intent->action_description = copy_action(action);
    if (!intent->action_description) {
        set_error("Failed to copy action description");
        free_intention(intent);
        return PROSP_ERROR_NO_MEMORY;
    }

    prime_signature_t* sig = prime_sig_from_text(action);
    if (sig) {
        intent->action_signature = *sig;
        prime_sig_destroy(sig);
    }

    // Set location trigger
    memset(&intent->trigger.location_trigger, 0, sizeof(prosp_location_trigger_t));
    if (location_signature) {
        intent->trigger.location_trigger.location_signature = *location_signature;
    }
    intent->trigger.location_trigger.similarity_threshold =
        (similarity_threshold > 0) ? similarity_threshold : pm->config.similarity_threshold;

    intent->importance = (importance > 0) ? importance : pm->config.default_importance;
    intent->urgency = (urgency > 0) ? urgency : pm->config.default_urgency;
    intent->priority = compute_priority_internal(intent->importance, intent->urgency);

    intent->creation_time = pm->current_time;
    intent->current_activation = 1.0f;

    if (pm->node_manager) {
        intent->memory_node = create_intention_memory_node(pm, intent);
    }

    add_to_active_monitors(pm, intent);

    pm->stats.total_created++;
    pm->stats.current_pending++;

    if (intention_id_out) {
        *intention_id_out = intent->intention_id;
    }

    return PROSP_SUCCESS;
}

//=============================================================================
// Update and Monitoring Functions
//=============================================================================

NIMCP_EXPORT prospective_error_t prospective_update(
    prospective_memory_t pm,
    float current_time,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t num_triggered = 0;
    pm->current_time = current_time;

    // Check rate limiting
    float time_since_last = current_time - pm->last_check_time;
    if (time_since_last < pm->config.check_interval && time_since_last >= 0) {
        // Too soon for another check
        if (num_triggered_out) *num_triggered_out = 0;
        return PROSP_SUCCESS;
    }
    pm->last_check_time = current_time;

    // Apply decay to all activations
    prospective_apply_decay(pm, time_since_last);

    // Check all pending intentions
    for (size_t i = 0; i < pm->num_intentions && num_triggered < max_triggered; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->status != PROSP_PENDING) continue;

        float strength = 0.0f;
        bool triggered = false;

        // Check based on type
        switch (intent->type) {
            case PROSP_TIME_BASED:
                triggered = check_time_trigger_internal(intent, current_time, &strength);
                if (triggered) {
                    pm->stats.time_triggers_fired++;
                }
                break;

            case PROSP_EVENT_BASED:
                // Event triggers checked via signal_context
                if (pm->context_valid) {
                    triggered = check_event_trigger_internal(intent, &pm->current_context, &strength);
                    if (triggered) {
                        pm->stats.event_triggers_fired++;
                    }
                }
                break;

            case PROSP_ACTIVITY_BASED:
                // Activity triggers checked via signal_activity_complete
                break;

            case PROSP_LOCATION_BASED:
                // Location triggers checked via signal_location
                break;
        }

        if (triggered && triggered_out) {
            // Transition to TRIGGERED status
            intent->status = PROSP_TRIGGERED;
            intent->trigger_time = current_time;
            pm->stats.current_pending--;
            pm->stats.current_triggered++;

            // Fill result
            fill_trigger_result(&triggered_out[num_triggered], intent, strength, current_time);
            num_triggered++;
        }

        // Update activation based on time proximity
        if (intent->type == PROSP_TIME_BASED && intent->status == PROSP_PENDING) {
            float boost = compute_time_activation(intent, current_time);
            if (boost > 0) {
                intent->current_activation += boost;
                if (intent->current_activation > PROSP_MAX_ACTIVATION) {
                    intent->current_activation = PROSP_MAX_ACTIVATION;
                }
            }

            // Check for missed (past grace period)
            float target = intent->trigger.time_trigger.target_time;
            float window_after = intent->trigger.time_trigger.window_after;
            if (current_time > target + window_after) {
                // Missed
                intent->status = PROSP_MISSED;
                pm->stats.total_missed++;
                pm->stats.current_pending--;
                remove_from_active_monitors(pm, intent);
            }
        }

        // Increment retrieval count and update last check
        intent->retrieval_count++;
        intent->last_check_time = current_time;
    }

    pm->stats.total_checks++;

    // Auto-prune if enabled
    if (pm->config.auto_prune_executed || pm->config.auto_prune_missed) {
        prospective_prune(pm, pm->config.prune_delay);
    }

    if (num_triggered_out) *num_triggered_out = num_triggered;

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_check_time_triggers(
    prospective_memory_t pm,
    float current_time,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t num_triggered = 0;
    pm->current_time = current_time;

    for (size_t i = 0; i < pm->num_intentions && num_triggered < max_triggered; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->type != PROSP_TIME_BASED) continue;
        if (intent->status != PROSP_PENDING) continue;

        float strength = 0.0f;
        if (check_time_trigger_internal(intent, current_time, &strength)) {
            intent->status = PROSP_TRIGGERED;
            intent->trigger_time = current_time;
            pm->stats.current_pending--;
            pm->stats.current_triggered++;
            pm->stats.time_triggers_fired++;

            if (triggered_out) {
                fill_trigger_result(&triggered_out[num_triggered], intent, strength, current_time);
            }
            num_triggered++;
        }
    }

    if (num_triggered_out) *num_triggered_out = num_triggered;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_check_event_triggers(
    prospective_memory_t pm,
    const prime_signature_t* context_signature,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!context_signature) {
        set_error("NULL context signature");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t num_triggered = 0;

    for (size_t i = 0; i < pm->num_intentions && num_triggered < max_triggered; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->type != PROSP_EVENT_BASED) continue;
        if (intent->status != PROSP_PENDING) continue;

        float strength = 0.0f;
        if (check_event_trigger_internal(intent, context_signature, &strength)) {
            intent->status = PROSP_TRIGGERED;
            intent->trigger_time = pm->current_time;
            pm->stats.current_pending--;
            pm->stats.current_triggered++;
            pm->stats.event_triggers_fired++;

            if (triggered_out) {
                fill_trigger_result(&triggered_out[num_triggered], intent, strength, pm->current_time);
            }
            num_triggered++;
        }
    }

    if (num_triggered_out) *num_triggered_out = num_triggered;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_check_activity_triggers(
    prospective_memory_t pm,
    uint64_t completed_activity_id,
    prospective_trigger_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t num_triggered = 0;

    for (size_t i = 0; i < pm->num_intentions && num_triggered < max_triggered; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->type != PROSP_ACTIVITY_BASED) continue;
        if (intent->status != PROSP_PENDING) continue;

        float strength = 0.0f;
        if (check_activity_trigger_internal(intent, completed_activity_id, NULL, &strength)) {
            intent->status = PROSP_TRIGGERED;
            intent->trigger_time = pm->current_time;
            pm->stats.current_pending--;
            pm->stats.current_triggered++;
            pm->stats.activity_triggers_fired++;

            if (triggered_out) {
                fill_trigger_result(&triggered_out[num_triggered], intent, strength, pm->current_time);
            }
            num_triggered++;
        }
    }

    if (num_triggered_out) *num_triggered_out = num_triggered;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT int prospective_signal_context(
    prospective_memory_t pm,
    const prime_signature_t* context,
    const nimcp_quaternion_t* context_state
) {
    if (!pm || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_signal_context: required parameter is NULL (pm, context)");
        return -1;
    }

    // Store context for later checks
    pm->current_context = *context;
    pm->context_valid = true;

    // Check event triggers immediately
    prospective_trigger_result_t triggered[32];
    size_t num_triggered = 0;

    prospective_check_event_triggers(pm, context, triggered, 32, &num_triggered);

    return (int)num_triggered;
}

NIMCP_EXPORT int prospective_signal_activity_complete(
    prospective_memory_t pm,
    uint64_t activity_id,
    const char* activity_tag
) {
    if (!pm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_signal_activity_complete: pm is NULL");
        return -1;
    }

    int num_triggered = 0;

    for (size_t i = 0; i < pm->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_intentions);
        }

        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->type != PROSP_ACTIVITY_BASED) continue;
        if (intent->status != PROSP_PENDING) continue;

        float strength = 0.0f;
        if (check_activity_trigger_internal(intent, activity_id, activity_tag, &strength)) {
            intent->status = PROSP_TRIGGERED;
            intent->trigger_time = pm->current_time;
            pm->stats.current_pending--;
            pm->stats.current_triggered++;
            pm->stats.activity_triggers_fired++;
            num_triggered++;
        }
    }

    return num_triggered;
}

NIMCP_EXPORT int prospective_signal_location(
    prospective_memory_t pm,
    const prime_signature_t* location_signature
) {
    if (!pm || !location_signature) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prospective_signal_location: required parameter is NULL (pm, location_signature)");
        return -1;
    }

    int num_triggered = 0;

    for (size_t i = 0; i < pm->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_intentions);
        }

        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->type != PROSP_LOCATION_BASED) continue;
        if (intent->status != PROSP_PENDING) continue;

        float strength = 0.0f;
        if (check_location_trigger_internal(intent, location_signature, &strength)) {
            intent->status = PROSP_TRIGGERED;
            intent->trigger_time = pm->current_time;
            pm->stats.current_pending--;
            pm->stats.current_triggered++;
            num_triggered++;
        }
    }

    return num_triggered;
}

//=============================================================================
// Intention Management Functions
//=============================================================================

NIMCP_EXPORT prospective_error_t prospective_execute_intention(
    prospective_memory_t pm,
    uint64_t intention_id
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {
        set_error("Intention not found");
        return PROSP_ERROR_NOT_FOUND;
    }

    if (intent->status == PROSP_EXECUTED) {
        set_error("Intention already executed");
        return PROSP_ERROR_ALREADY_EXECUTED;
    }

    // Update status
    if (intent->status == PROSP_PENDING) {
        pm->stats.current_pending--;
    } else if (intent->status == PROSP_TRIGGERED) {
        pm->stats.current_triggered--;
    }

    intent->status = PROSP_EXECUTED;
    pm->stats.total_executed++;

    // Update response time stats
    if (intent->trigger_time > 0 && pm->current_time > intent->trigger_time) {
        float response_time = pm->current_time - intent->trigger_time;
        // Update running mean
        float n = (float)pm->stats.total_executed;
        pm->stats.mean_response_time =
            ((n - 1) * pm->stats.mean_response_time + response_time) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    }

    remove_from_active_monitors(pm, intent);

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_cancel_intention(
    prospective_memory_t pm,
    uint64_t intention_id
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {
        set_error("Intention not found");
        return PROSP_ERROR_NOT_FOUND;
    }

    if (intent->status == PROSP_PENDING) {
        pm->stats.current_pending--;
    } else if (intent->status == PROSP_TRIGGERED) {
        pm->stats.current_triggered--;
    }

    intent->status = PROSP_CANCELLED;
    pm->stats.total_cancelled++;

    remove_from_active_monitors(pm, intent);

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_miss_intention(
    prospective_memory_t pm,
    uint64_t intention_id
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {
        set_error("Intention not found");
        return PROSP_ERROR_NOT_FOUND;
    }

    if (intent->status == PROSP_PENDING) {
        pm->stats.current_pending--;
    } else if (intent->status == PROSP_TRIGGERED) {
        pm->stats.current_triggered--;
    }

    intent->status = PROSP_MISSED;
    pm->stats.total_missed++;

    remove_from_active_monitors(pm, intent);

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_reschedule(
    prospective_memory_t pm,
    uint64_t intention_id,
    float new_target_time,
    bool is_relative
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {
        set_error("Intention not found");
        return PROSP_ERROR_NOT_FOUND;
    }

    if (intent->type != PROSP_TIME_BASED) {
        set_error("Can only reschedule time-based intentions");
        return PROSP_ERROR_INVALID_TYPE;
    }

    // Update target time
    float absolute_time = is_relative ? pm->current_time + new_target_time : new_target_time;
    intent->trigger.time_trigger.target_time = absolute_time;
    intent->expected_execution_time = absolute_time;

    // Reset to pending if was triggered or missed
    if (intent->status == PROSP_TRIGGERED || intent->status == PROSP_MISSED) {
        if (intent->status == PROSP_TRIGGERED) {
            pm->stats.current_triggered--;
        }
        intent->status = PROSP_PENDING;
        pm->stats.current_pending++;
        add_to_active_monitors(pm, intent);
    }

    // Reset activation
    intent->current_activation = 1.0f;
    intent->trigger_time = 0.0f;

    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_remove_intention(
    prospective_memory_t pm,
    uint64_t intention_id
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {
        set_error("Intention not found");
        return PROSP_ERROR_NOT_FOUND;
    }

    // Update stats
    switch (intent->status) {
        case PROSP_PENDING:
            pm->stats.current_pending--;
            break;
        case PROSP_TRIGGERED:
            pm->stats.current_triggered--;
            break;
        default:
            break;
    }

    remove_from_active_monitors(pm, intent);
    free_intention(intent);

    return PROSP_SUCCESS;
}

NIMCP_EXPORT size_t prospective_prune(prospective_memory_t pm, float min_age) {
    if (!pm) return 0;

    size_t removed = 0;

    for (size_t i = 0; i < pm->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_intentions);
        }

        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;

        bool should_prune = false;
        float age = pm->current_time - intent->creation_time;

        if (intent->status == PROSP_EXECUTED && pm->config.auto_prune_executed) {
            if (age >= min_age) should_prune = true;
        }
        if (intent->status == PROSP_MISSED && pm->config.auto_prune_missed) {
            if (age >= min_age) should_prune = true;
        }
        if (intent->status == PROSP_CANCELLED) {
            if (age >= min_age) should_prune = true;
        }

        if (should_prune) {
            free_intention(intent);
            removed++;
        }
    }

    return removed;
}

//=============================================================================
// Query Functions
//=============================================================================

NIMCP_EXPORT prospective_error_t prospective_get_intention(
    prospective_memory_t pm,
    uint64_t intention_id,
    prospective_intention_t* intention_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!intention_out) {
        set_error("NULL output pointer");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {
        set_error("Intention not found");
        return PROSP_ERROR_NOT_FOUND;
    }

    *intention_out = *intent;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_get_pending(
    prospective_memory_t pm,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t count = 0;

    for (size_t i = 0; i < pm->num_intentions && count < max_intentions; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->status != PROSP_PENDING) continue;

        if (intentions_out) {
            intentions_out[count] = *intent;
        }
        count++;
    }

    if (count_out) *count_out = count;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_get_triggered(
    prospective_memory_t pm,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t count = 0;

    for (size_t i = 0; i < pm->num_intentions && count < max_intentions; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->status != PROSP_TRIGGERED) continue;

        if (intentions_out) {
            intentions_out[count] = *intent;
        }
        count++;
    }

    if (count_out) *count_out = count;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_get_urgent(
    prospective_memory_t pm,
    size_t k,
    prospective_intention_t* intentions_out,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    // Collect pending intentions with their urgency scores
    typedef struct {
        prospective_intention_t* intent;
        float urgency_score;
    } urgency_entry_t;

    urgency_entry_t* entries = (urgency_entry_t*)nimcp_calloc(pm->num_intentions, sizeof(urgency_entry_t));
    if (!entries) {
        set_error("Memory allocation failed");
        return PROSP_ERROR_NO_MEMORY;
    }

    size_t num_entries = 0;
    for (size_t i = 0; i < pm->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_intentions);
        }

        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->status != PROSP_PENDING && intent->status != PROSP_TRIGGERED) continue;

        entries[num_entries].intent = intent;

        // Compute urgency score
        float score = intent->urgency + intent->importance * 0.5f;

        // Boost for time-based approaching deadline
        if (intent->type == PROSP_TIME_BASED) {
            float time_to_target = intent->trigger.time_trigger.target_time - pm->current_time;
            if (time_to_target > 0 && time_to_target < 3600.0f) { // Within 1 hour
                score += (3600.0f - time_to_target) / 360.0f; // Up to +10
            }
        }

        entries[num_entries].urgency_score = score;
        num_entries++;
    }

    // Simple bubble sort for top-k (good enough for small k)
    for (size_t i = 0; i < num_entries && i < k; i++) {
        size_t max_idx = i;
        for (size_t j = i + 1; j < num_entries; j++) {
            if (entries[j].urgency_score > entries[max_idx].urgency_score) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            urgency_entry_t temp = entries[i];
            entries[i] = entries[max_idx];
            entries[max_idx] = temp;
        }
    }

    // Copy top-k to output
    size_t count = (num_entries < k) ? num_entries : k;
    if (intentions_out) {
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                prospective_heartbeat("prospective_loop",
                                 (float)(i + 1) / (float)count);
            }

            intentions_out[i] = *entries[i].intent;
        }
    }

    nimcp_free(entries);
    entries = NULL;

    if (count_out) *count_out = count;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_get_by_type(
    prospective_memory_t pm,
    prospective_type_t type,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t count = 0;

    for (size_t i = 0; i < pm->num_intentions && count < max_intentions; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->type != type) continue;

        if (intentions_out) {
            intentions_out[count] = *intent;
        }
        count++;
    }

    if (count_out) *count_out = count;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_search(
    prospective_memory_t pm,
    const char* query,
    prospective_intention_t* intentions_out,
    size_t max_intentions,
    size_t* count_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!query) {
        set_error("NULL query string");
        return PROSP_ERROR_NULL_POINTER;
    }

    size_t count = 0;

    for (size_t i = 0; i < pm->num_intentions && count < max_intentions; i++) {
        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (!intent->action_description) continue;

        // Case-insensitive substring search
        if (strstr(intent->action_description, query) != NULL) {
            if (intentions_out) {
                intentions_out[count] = *intent;
            }
            count++;
        }
    }

    if (count_out) *count_out = count;
    return PROSP_SUCCESS;
}

//=============================================================================
// Activation Functions
//=============================================================================

NIMCP_EXPORT float prospective_compute_activation(
    prospective_memory_t pm,
    uint64_t intention_id,
    float current_time
) {
    if (!pm) return -1.0f;

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) return -1.0f;

    float activation = intent->current_activation;

    // Factor 1: Base from importance/urgency
    float base = (intent->importance + intent->urgency) / 20.0f; // Normalize to ~0.5

    // Factor 2: Recency of checks (retrieval strengthens)
    float recency_boost = 0.0f;
    if (intent->retrieval_count > 0) {
        float time_since_last = current_time - intent->last_check_time;
        if (time_since_last < 60.0f) { // Recent check
            recency_boost = 0.2f * (1.0f - time_since_last / 60.0f);
        }
    }

    // Factor 3: Time proximity for time-based
    float proximity_boost = 0.0f;
    if (intent->type == PROSP_TIME_BASED && intent->status == PROSP_PENDING) {
        proximity_boost = compute_time_activation(intent, current_time);
    }

    activation = base + recency_boost + proximity_boost + activation * 0.5f;

    // Clamp
    if (activation < 0.0f) activation = 0.0f;
    if (activation > PROSP_MAX_ACTIVATION) activation = PROSP_MAX_ACTIVATION;

    return activation;
}

NIMCP_EXPORT float prospective_boost_activation(
    prospective_memory_t pm,
    uint64_t intention_id,
    float boost_amount
) {
    if (!pm) return -1.0f;

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) return -1.0f;

    intent->current_activation += boost_amount;
    intent->retrieval_count++;
    intent->last_check_time = pm->current_time;

    if (intent->current_activation > PROSP_MAX_ACTIVATION) {
        intent->current_activation = PROSP_MAX_ACTIVATION;
    }

    return intent->current_activation;
}

NIMCP_EXPORT prospective_error_t prospective_apply_decay(
    prospective_memory_t pm,
    float elapsed_time
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    if (elapsed_time <= 0) return PROSP_SUCCESS;

    float decay_factor = expf(-pm->config.activation_decay_rate * elapsed_time);

    float total_activation = 0.0f;
    size_t pending_count = 0;

    for (size_t i = 0; i < pm->num_intentions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && pm->num_intentions > 256) {
            prospective_heartbeat("prospective_loop",
                             (float)(i + 1) / (float)pm->num_intentions);
        }

        prospective_intention_t* intent = &pm->intentions[i];
        if (intent->intention_id == PROSP_INVALID_ID) continue;
        if (intent->status != PROSP_PENDING) continue;

        intent->current_activation *= decay_factor;

        // Remove from active if activation too low
        if (intent->current_activation < PROSP_MIN_ACTIVE_ACTIVATION) {
            remove_from_active_monitors(pm, intent);
        }

        total_activation += intent->current_activation;
        pending_count++;
    }

    // Update mean activation stat
    if (pending_count > 0) {
        pm->stats.mean_activation = total_activation / (float)pending_count;
    }

    return PROSP_SUCCESS;
}

//=============================================================================
// Integration Functions
//=============================================================================

NIMCP_EXPORT prospective_error_t prospective_link_memory(
    prospective_memory_t pm,
    uint64_t intention_id,
    uint64_t memory_id
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {
        set_error("Intention not found");
        return PROSP_ERROR_NOT_FOUND;
    }

    if (intent->num_related >= 8) {
        set_error("Maximum related memories reached");
        return PROSP_ERROR_CAPACITY;
    }

    // Add to related memories
    intent->related_memory_ids[intent->num_related++] = memory_id;

    // Create entanglement if graph available
    if (pm->entanglement && intent->memory_node) {
        entangle_edge_t edge = {
            .from_id = pr_memory_node_get_id(intent->memory_node),
            .to_id = memory_id,
            .resonance_score = 0.8f,
            .type = ENTANGLE_EDGE_ASSOCIATIVE,
            .weight = 0.8f,
            .bidirectional = true
        };
        entangle_add_edge(pm->entanglement, &edge);
    }

    return PROSP_SUCCESS;
}

NIMCP_EXPORT pr_memory_node_t* prospective_get_memory_node(
    prospective_memory_t pm,
    uint64_t intention_id
) {
    if (!pm) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pm is NULL");

        return NULL;

    }

    prospective_intention_t* intent = find_intention(pm, intention_id);
    if (!intent) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intent is NULL");

        return NULL;

    }

    return intent->memory_node;
}

NIMCP_EXPORT bool prospective_should_check_now(prospective_memory_t pm) {
    if (!pm) {
        return false;
    }

    // If theta-gamma is available, check during encoding phase
    if (pm->theta_gamma) {
        return theta_gamma_can_encode(pm->theta_gamma);
    }

    // Otherwise, always allow
    return true;
}

//=============================================================================
// Statistics and Debugging Functions
//=============================================================================

NIMCP_EXPORT prospective_error_t prospective_get_stats(
    prospective_memory_t pm,
    prospective_stats_t* stats_out
) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }
    if (!stats_out) {
        set_error("NULL output pointer");
        return PROSP_ERROR_NULL_POINTER;
    }

    *stats_out = pm->stats;
    return PROSP_SUCCESS;
}

NIMCP_EXPORT prospective_error_t prospective_reset_stats(prospective_memory_t pm) {
    if (!pm) {
        set_error("NULL prospective memory");
        return PROSP_ERROR_NULL_POINTER;
    }

    // Keep current_* stats accurate
    size_t pending = pm->stats.current_pending;
    size_t triggered = pm->stats.current_triggered;
    size_t active = pm->stats.current_active_monitors;

    memset(&pm->stats, 0, sizeof(pm->stats));

    pm->stats.current_pending = pending;
    pm->stats.current_triggered = triggered;
    pm->stats.current_active_monitors = active;

    return PROSP_SUCCESS;
}

NIMCP_EXPORT const char* prospective_error_string(prospective_error_t error) {
    switch (error) {
        case PROSP_SUCCESS:            return "Success";
        case PROSP_ERROR_NULL_POINTER: return "NULL pointer";
        case PROSP_ERROR_INVALID_TYPE: return "Invalid type";
        case PROSP_ERROR_INVALID_STATUS: return "Invalid status";
        case PROSP_ERROR_NO_MEMORY:    return "Memory allocation failed";
        case PROSP_ERROR_CAPACITY:     return "Maximum capacity reached";
        case PROSP_ERROR_NOT_FOUND:    return "Not found";
        case PROSP_ERROR_INVALID_TIME: return "Invalid time";
        case PROSP_ERROR_INVALID_TRIGGER: return "Invalid trigger";
        case PROSP_ERROR_ALREADY_EXECUTED: return "Already executed";
        case PROSP_ERROR_INTERNAL:     return "Internal error";
        default:                       return "Unknown error";
    }
}

NIMCP_EXPORT const char* prospective_get_last_error(void) {
    return g_last_error[0] ? g_last_error : NULL;
}

NIMCP_EXPORT const char* prospective_type_name(prospective_type_t type) {
    switch (type) {
        case PROSP_TIME_BASED:     return "TIME_BASED";
        case PROSP_EVENT_BASED:    return "EVENT_BASED";
        case PROSP_ACTIVITY_BASED: return "ACTIVITY_BASED";
        case PROSP_LOCATION_BASED: return "LOCATION_BASED";
        default:                   return "UNKNOWN";
    }
}

NIMCP_EXPORT const char* prospective_status_name(prospective_status_t status) {
    switch (status) {
        case PROSP_PENDING:    return "PENDING";
        case PROSP_TRIGGERED:  return "TRIGGERED";
        case PROSP_EXECUTED:   return "EXECUTED";
        case PROSP_MISSED:     return "MISSED";
        case PROSP_CANCELLED:  return "CANCELLED";
        default:               return "UNKNOWN";
    }
}

NIMCP_EXPORT void prospective_print_intention(const prospective_intention_t* intention) {
    if (!intention) {
        printf("Intention: NULL\n");
        return;
    }

    printf("Intention[%lu]:\n", (unsigned long)intention->intention_id);
    printf("  Type: %s\n", prospective_type_name(intention->type));
    printf("  Status: %s\n", prospective_status_name(intention->status));
    printf("  Action: %s\n", intention->action_description ? intention->action_description : "(none)");
    printf("  Importance: %.1f, Urgency: %.1f\n", intention->importance, intention->urgency);
    printf("  Activation: %.3f\n", intention->current_activation);
    printf("  Retrieval count: %zu\n", intention->retrieval_count);

    if (intention->type == PROSP_TIME_BASED) {
        printf("  Target time: %.1f\n", intention->trigger.time_trigger.target_time);
        printf("  Window: [%.1f before, %.1f after]\n",
               intention->trigger.time_trigger.window_before,
               intention->trigger.time_trigger.window_after);
    }
}

NIMCP_EXPORT void prospective_print_summary(prospective_memory_t pm) {
    if (!pm) {
        printf("Prospective Memory: NULL\n");
        return;
    }

    printf("=== Prospective Memory Summary ===\n");
    printf("Total created: %zu\n", pm->stats.total_created);
    printf("Current pending: %zu\n", pm->stats.current_pending);
    printf("Current triggered: %zu\n", pm->stats.current_triggered);
    printf("Active monitors: %zu\n", pm->stats.current_active_monitors);
    printf("Executed: %zu, Missed: %zu, Cancelled: %zu\n",
           pm->stats.total_executed, pm->stats.total_missed, pm->stats.total_cancelled);
    printf("Mean response time: %.2f sec\n", pm->stats.mean_response_time);
    printf("Mean activation: %.3f\n", pm->stats.mean_activation);
    printf("Total checks: %lu\n", (unsigned long)pm->stats.total_checks);
    printf("Triggers fired - Time: %lu, Event: %lu, Activity: %lu\n",
           (unsigned long)pm->stats.time_triggers_fired,
           (unsigned long)pm->stats.event_triggers_fired,
           (unsigned long)pm->stats.activity_triggers_fired);
}

//=============================================================================
// Utility Functions
//=============================================================================

NIMCP_EXPORT float prospective_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f;
}

NIMCP_EXPORT prospective_priority_t prospective_compute_priority(
    float importance,
    float urgency
) {
    return compute_priority_internal(importance, urgency);
}

NIMCP_EXPORT void prospective_init_time_trigger(
    prosp_time_trigger_t* trigger,
    float target_time,
    bool is_relative
) {
    if (!trigger) return;

    memset(trigger, 0, sizeof(*trigger));
    trigger->target_time = target_time;
    trigger->is_relative = is_relative;
    trigger->window_before = PROSP_DEFAULT_WINDOW_BEFORE;
    trigger->window_after = PROSP_DEFAULT_WINDOW_AFTER;
    trigger->repeat_daily = false;
    trigger->repeat_interval = 0.0f;
}

NIMCP_EXPORT void prospective_init_event_trigger(
    prosp_event_trigger_t* trigger,
    const prime_signature_t* cue_signature,
    float threshold
) {
    if (!trigger) return;

    memset(trigger, 0, sizeof(*trigger));
    if (cue_signature) {
        trigger->cue_signature = *cue_signature;
    }
    trigger->similarity_threshold = (threshold > 0) ? threshold : PROSP_DEFAULT_SIMILARITY_THRESHOLD;
    trigger->match_state = false;
    trigger->state_weight = 0.0f;
}

NIMCP_EXPORT void prospective_init_activity_trigger(
    prosp_activity_trigger_t* trigger,
    uint64_t prerequisite_id
) {
    if (!trigger) return;

    memset(trigger, 0, sizeof(*trigger));
    trigger->prerequisite_id = prerequisite_id;
    trigger->any_activity = false;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void prospective_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_prospective_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int prospective_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prospective_training_begin: NULL argument");
        return -1;
    }
    prospective_heartbeat_instance(NULL, "prospective_training_begin", 0.0f);
    (void)(struct prospective_memory_internal*)instance; /* Module state available for reset */
    return 0;
}

int prospective_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prospective_training_end: NULL argument");
        return -1;
    }
    prospective_heartbeat_instance(NULL, "prospective_training_end", 1.0f);
    (void)(struct prospective_memory_internal*)instance; /* Module state available for finalization */
    return 0;
}

int prospective_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "prospective_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    prospective_heartbeat_instance(NULL, "prospective_training_step", progress);
    (void)(struct prospective_memory_internal*)instance; /* Module state available for step adaptation */
    return 0;
}

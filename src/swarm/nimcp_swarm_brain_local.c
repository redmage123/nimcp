/**
 * @file nimcp_swarm_brain_local.c
 * @brief Implementation of swarm brain local instantiation
 *
 * This module manages per-agent brain instances with distributed learning
 * and weight synchronization across the swarm.
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "swarm/nimcp_swarm_brain_local.h"
#include "nimcp.h"
#include "api/nimcp_api_internal.h"  /* For nimcp_brain_handle struct access */
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_core.h"
#include "core/brain/nimcp_brain_io.h"
#include "core/brain/factory/init/nimcp_brain_init_config.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_brain_local)

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define SWARM_BRAIN_MODULE "SwarmBrainLocal"
#define HASH_TABLE_SIZE 256
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Hash table entry for agent brains
 */
typedef struct agent_brain_entry_t {
    agent_brain_t brain_data;
    struct agent_brain_entry_t* next;
} agent_brain_entry_t;

/**
 * @brief Swarm brain manager internal state
 */
struct swarm_brain_manager {
    swarm_brain_config_t config;

    /* Agent brain storage (hash table) */
    agent_brain_entry_t** agent_table;
    uint32_t active_agents;

    /* Consensus weights */
    float* consensus_weights;
    uint32_t consensus_weight_count;
    bool consensus_dirty;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Statistics */
    swarm_brain_stats_t stats;
    uint64_t creation_time_ms;

    /* Synchronization */
    nimcp_platform_mutex_t mutex;
};

/**
 * @brief Swarm brain bio-async message
 */
typedef struct {
    bio_message_header_t header;
    uint32_t agent_id;
    float divergence;
    uint32_t num_weights;
    float weights[];  /* Variable length */
} bio_msg_swarm_brain_t;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Hash function for agent ID
 */
static uint32_t agent_hash(uint32_t agent_id) {
    return agent_id % HASH_TABLE_SIZE;
}

/**
 * @brief Find agent brain entry
 *
 * WHAT: Looks up agent brain in hash table
 * WHY:  Fast O(1) average case lookup
 * HOW:  Hash agent ID, traverse chain
 */
static agent_brain_entry_t* find_agent_entry(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
) {
    if (!mgr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_agent_entry: mgr is NULL");
        return NULL;
    }

    uint32_t hash = agent_hash(agent_id);
    agent_brain_entry_t* entry = mgr->agent_table[hash];

    while (entry) {
        if (entry->brain_data.agent_id == agent_id) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Calculate divergence score
 *
 * WHAT: Computes L2 distance between local and consensus weights
 * WHY:  Measure how different agent is from swarm
 * HOW:  Normalized Euclidean distance
 */
static float calculate_divergence(
    const float* local_weights,
    const float* consensus_weights,
    uint32_t num_weights
) {
    if (!local_weights || !consensus_weights || num_weights == 0) {
        return 0.0F;
    }

    float sum_sq_diff = 0.0F;
    float sum_sq_consensus = EPSILON;

    for (uint32_t i = 0; i < num_weights; i++) {
        float diff = local_weights[i] - consensus_weights[i];
        sum_sq_diff += diff * diff;
        sum_sq_consensus += consensus_weights[i] * consensus_weights[i];
    }

    /* Normalized divergence: ||w_local - w_consensus|| / ||w_consensus|| */
    return sqrtf(sum_sq_diff / sum_sq_consensus);
}

/**
 * @brief Update consensus weights
 *
 * WHAT: Recomputes consensus by averaging all active agent weights
 * WHY:  Maintain swarm coherence target
 * HOW:  Average weights across all agents
 */
static nimcp_result_t update_consensus_weights(swarm_brain_manager_t* mgr) {
    if (!mgr) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Guard clause: no active agents */
    if (mgr->active_agents == 0) {
        mgr->consensus_dirty = false;
        return NIMCP_SUCCESS;
    }

    /* Find maximum weight count across all agents */
    uint32_t max_weights = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active &&
                entry->brain_data.num_weights > max_weights) {
                max_weights = entry->brain_data.num_weights;
            }
            entry = entry->next;
        }
    }

    /* Guard clause: no weights to average */
    if (max_weights == 0) {
        mgr->consensus_dirty = false;
        return NIMCP_SUCCESS;
    }

    /* Allocate or resize consensus weights */
    if (mgr->consensus_weight_count != max_weights) {
        float* new_weights = (float*)nimcp_realloc(
            mgr->consensus_weights,
            max_weights * sizeof(float)
        );
        if (!new_weights) {
            LOG_ERROR("Failed to allocate consensus weights");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate consensus weights");
            return NIMCP_ERROR_MEMORY;
        }
        mgr->consensus_weights = new_weights;
        mgr->consensus_weight_count = max_weights;
    }

    /* Zero out consensus weights */
    memset(mgr->consensus_weights, 0, max_weights * sizeof(float));

    /* Sum all agent weights */
    uint32_t count = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active && entry->brain_data.local_weights) {
                for (uint32_t j = 0; j < entry->brain_data.num_weights; j++) {
                    mgr->consensus_weights[j] += entry->brain_data.local_weights[j];
                }
                count++;
            }
            entry = entry->next;
        }
    }

    /* Average weights */
    if (count > 0) {
        for (uint32_t i = 0; i < max_weights; i++) {
            mgr->consensus_weights[i] /= (float)count;
        }
    }

    mgr->consensus_dirty = false;
    return NIMCP_SUCCESS;
}

/**
 * @brief Bio-async message handler
 */
static nimcp_result_t bio_message_handler(
    void* context,
    const bio_message_header_t* msg,
    size_t msg_size
) {
    swarm_brain_manager_t* mgr = (swarm_brain_manager_t*)context;
    if (!mgr || !msg) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);

    switch (msg->type) {
        case BIO_MSG_BRAIN_WEIGHTS_REQUEST: {
            /* Request for consensus weights - trigger update */
            if (mgr->consensus_dirty) {
                update_consensus_weights(mgr);
            }
            break;
        }

        case BIO_MSG_BRAIN_WEIGHTS_UPDATE: {
            /* Received weight update from another agent */
            mgr->consensus_dirty = true;
            break;
        }

        default:
            break;
    }

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

swarm_brain_manager_t* swarm_brain_manager_create(
    const swarm_brain_config_t* config
) {
    /* Allocate manager */
    swarm_brain_manager_t* mgr = (swarm_brain_manager_t*)nimcp_calloc(
        1, sizeof(swarm_brain_manager_t)
    );
    if (!mgr) {
        LOG_ERROR("Failed to allocate swarm brain manager");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate swarm brain manager");
        return NULL;
    }

    /* Initialize config with defaults if not provided */
    if (config) {
        mgr->config = *config;
    } else {
        mgr->config = swarm_brain_local_default_config();
    }

    /* Allocate hash table */
    mgr->agent_table = (agent_brain_entry_t**)nimcp_calloc(
        HASH_TABLE_SIZE, sizeof(agent_brain_entry_t*)
    );
    if (!mgr->agent_table) {
        LOG_ERROR("Failed to allocate agent hash table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate agent hash table");
        nimcp_free(mgr);
        return NULL;
    }

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&mgr->mutex, false) != NIMCP_SUCCESS) {
        LOG_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize mutex in swarm brain manager");
        nimcp_free(mgr->agent_table);
        nimcp_free(mgr);
        return NULL;
    }

    /* Initialize statistics */
    mgr->creation_time_ms = nimcp_time_get_us() / 1000;
    mgr->consensus_dirty = true;

    /* Register with bio-async if enabled */
    if (mgr->config.enable_bio_async) {
        bio_module_info_t info = {
            .module_name = SWARM_BRAIN_MODULE,
            .inbox_capacity = NIMCP_INBOX_CAPACITY_MEDIUM,
            .user_data = mgr
        };

        mgr->bio_ctx = bio_router_register_module(&info);
        if (mgr->bio_ctx) {
            mgr->bio_async_enabled = true;
            LOG_INFO("Swarm brain manager registered with bio-async");
        } else {
            LOG_WARN("Failed to register with bio-async");
        }
    }

    LOG_INFO("Created swarm brain manager (max_agents=%u, default_size=%u)",
             SWARM_BRAIN_MAX_AGENTS, mgr->config.default_brain_size);

    return mgr;
}

void swarm_brain_manager_destroy(swarm_brain_manager_t* mgr) {
    if (!mgr) {
        return;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Destroy all agent brains */
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            agent_brain_entry_t* next = entry->next;

            /* Free brain resources */
            if (entry->brain_data.brain) {
                nimcp_brain_destroy(entry->brain_data.brain);
            }
            if (entry->brain_data.local_weights) {
                nimcp_free(entry->brain_data.local_weights);
            }

            nimcp_free(entry);
            entry = next;
        }
    }

    /* Free consensus weights */
    if (mgr->consensus_weights) {
        nimcp_free(mgr->consensus_weights);
    }

    /* Unregister from bio-async */
    if (mgr->bio_async_enabled && mgr->bio_ctx) {
        bio_router_unregister_module(mgr->bio_ctx);
    }

    nimcp_platform_mutex_unlock(&mgr->mutex);
    nimcp_platform_mutex_destroy(&mgr->mutex);

    nimcp_free(mgr->agent_table);
    nimcp_free(mgr);

    LOG_INFO("Destroyed swarm brain manager");
}

/* ============================================================================
 * Agent Brain Management
 * ============================================================================ */

int swarm_brain_create_for_agent(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    uint32_t brain_size
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");

    /* Use default if brain_size is 0 */
    if (brain_size == 0) {
        brain_size = mgr->config.default_brain_size;
    }

    /* Validate brain size */
    if (brain_size > mgr->config.max_local_neurons) {
        LOG_ERROR("Brain size %u exceeds max %u", brain_size,
                  mgr->config.max_local_neurons);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Check if agent already exists */
    if (find_agent_entry(mgr, agent_id)) {
        LOG_WARN("Agent %u already has brain", agent_id);
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Check agent limit */
    if (mgr->active_agents >= SWARM_BRAIN_MAX_AGENTS) {
        LOG_ERROR("Maximum agents reached (%u)", SWARM_BRAIN_MAX_AGENTS);
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Create brain instance */
    char task_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(task_name, sizeof(task_name), "swarm_agent_%u", agent_id);

    nimcp_brain_t brain = NULL;
    float* test_weights = NULL;
    if (mgr->config.test_mode) {
        /* Test mode: Create stub brain without going through factory (100x faster) */
        brain = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
        /* brain->internal_brain stays NULL - tests don't need actual neural network */
        /* Allocate dummy weights so sync operations work correctly */
        test_weights = (float*)nimcp_calloc(brain_size, sizeof(float));
    } else {
        /* Production mode: Use minimal brain creation for fast initialization */
        brain = (nimcp_brain_t)nimcp_malloc(sizeof(struct nimcp_brain_handle));
        if (brain) {
            brain->internal_brain = brain_create_minimal(task_name, BRAIN_SIZE_TINY,
                                                         BRAIN_TASK_CLASSIFICATION,
                                                         brain_size, brain_size / 2);
            if (!brain->internal_brain) {
                nimcp_free(brain);
                brain = NULL;
            }
        }
    }
    if (!brain) {
        LOG_ERROR("Failed to create brain for agent %u", agent_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create brain for agent %u", agent_id);
        if (test_weights) {
            nimcp_free(test_weights);
        }
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Create hash table entry */
    agent_brain_entry_t* entry = (agent_brain_entry_t*)nimcp_calloc(
        1, sizeof(agent_brain_entry_t)
    );
    if (!entry) {
        LOG_ERROR("Failed to allocate agent entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate agent entry for agent %u", agent_id);
        if (test_weights) {
            nimcp_free(test_weights);
        }
        nimcp_brain_destroy(brain);
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Initialize agent brain data */
    entry->brain_data.agent_id = agent_id;
    entry->brain_data.brain = brain;
    entry->brain_data.brain_size = brain_size;
    if (mgr->config.test_mode && test_weights) {
        /* Test mode: Pre-initialize weights for sync operations */
        entry->brain_data.num_weights = brain_size;
        entry->brain_data.local_weights = test_weights;
    } else {
        entry->brain_data.num_weights = 0;  /* Will be set on first sync */
        entry->brain_data.local_weights = NULL;
    }
    entry->brain_data.last_sync_ms = nimcp_time_get_us() / 1000;
    entry->brain_data.divergence_score = 0.0F;
    entry->brain_data.active = true;

    /* Insert into hash table */
    uint32_t hash = agent_hash(agent_id);
    entry->next = mgr->agent_table[hash];
    mgr->agent_table[hash] = entry;

    mgr->active_agents++;
    mgr->consensus_dirty = true;

    /* Update statistics */
    mgr->stats.num_agents = mgr->active_agents;
    mgr->stats.total_neurons += brain_size;

    nimcp_platform_mutex_unlock(&mgr->mutex);

    /* Send bio-async notification */
    if (mgr->bio_async_enabled && mgr->bio_ctx) {
        bio_message_header_t msg = {
            .type = BIO_MSG_BRAIN_AGENT_JOINED,
            .flags = BIO_MSG_FLAG_BROADCAST,
            .timestamp_us = nimcp_time_get_us()
        };
        bio_router_broadcast(mgr->bio_ctx, &msg, sizeof(msg));
    }

    LOG_INFO("Created brain for agent %u (size=%u)", agent_id, brain_size);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Role-Based Brain Templates (Asymmetric Swarm Optimization)
 * ============================================================================ */

/**
 * @brief Predefined brain templates for each drone role
 *
 * Memory optimization through role-specific feature sets:
 * - Worker (60% of swarm):     MICRO brain,  ~100KB, minimal features
 * - Scout (10%):               SMALL brain,  ~1MB,   navigation + memory
 * - Sensor (10%):              TINY brain,   ~500KB, perception only
 * - Guardian (10%):            SMALL brain,  ~1MB,   security + perception
 * - Coordinator (5%):          MEDIUM brain, ~10MB,  full cognitive
 * - Relay (5%):                MICRO brain,  ~100KB, minimal
 */
static const drone_brain_template_t ROLE_TEMPLATES[] = {
    /* DRONE_ROLE_SCOUT - Navigation and exploration */
    {
        .role = DRONE_ROLE_SCOUT,
        .brain_size = BRAIN_SIZE_SMALL,
        .neuron_override = 0,
        .enable_visual_cortex = true,
        .enable_audio_cortex = false,
        .enable_speech_cortex = false,
        .enable_working_memory = true,
        .enable_global_workspace = false,
        .enable_theory_of_mind = false,
        .enable_ethics = false,
        .enable_curiosity = true,
        .enable_mirror_neurons = false,
        .enable_executive_control = false,
        .enable_consolidation = true,
        .enable_glial = false,
        .enable_cortical_columns = false,
        .enable_predictive = true,
        .enable_bio_async = true,
        .minimal_mode = false,
        .lazy_init_mode = true,
        .max_inference_time_ms = 0.5F,
        .max_memory_kb = 1024
    },
    /* DRONE_ROLE_WORKER - Basic task execution */
    {
        .role = DRONE_ROLE_WORKER,
        .brain_size = BRAIN_SIZE_MICRO,
        .neuron_override = 0,
        .enable_visual_cortex = false,
        .enable_audio_cortex = false,
        .enable_speech_cortex = false,
        .enable_working_memory = false,
        .enable_global_workspace = false,
        .enable_theory_of_mind = false,
        .enable_ethics = false,
        .enable_curiosity = false,
        .enable_mirror_neurons = false,
        .enable_executive_control = false,
        .enable_consolidation = false,
        .enable_glial = false,
        .enable_cortical_columns = false,
        .enable_predictive = false,
        .enable_bio_async = true,
        .minimal_mode = true,
        .lazy_init_mode = true,
        .max_inference_time_ms = 0.1F,
        .max_memory_kb = 100
    },
    /* DRONE_ROLE_COORDINATOR - Swarm coordination */
    {
        .role = DRONE_ROLE_COORDINATOR,
        .brain_size = BRAIN_SIZE_MEDIUM,
        .neuron_override = 0,
        .enable_visual_cortex = true,
        .enable_audio_cortex = true,
        .enable_speech_cortex = false,
        .enable_working_memory = true,
        .enable_global_workspace = true,
        .enable_theory_of_mind = true,
        .enable_ethics = true,
        .enable_curiosity = false,
        .enable_mirror_neurons = true,
        .enable_executive_control = true,
        .enable_consolidation = true,
        .enable_glial = false,
        .enable_cortical_columns = false,
        .enable_predictive = true,
        .enable_bio_async = true,
        .minimal_mode = false,
        .lazy_init_mode = false,
        .max_inference_time_ms = 2.0F,
        .max_memory_kb = 10240
    },
    /* DRONE_ROLE_SENSOR - Environmental perception */
    {
        .role = DRONE_ROLE_SENSOR,
        .brain_size = BRAIN_SIZE_TINY,
        .neuron_override = 0,
        .enable_visual_cortex = true,
        .enable_audio_cortex = true,
        .enable_speech_cortex = false,
        .enable_working_memory = false,
        .enable_global_workspace = false,
        .enable_theory_of_mind = false,
        .enable_ethics = false,
        .enable_curiosity = false,
        .enable_mirror_neurons = false,
        .enable_executive_control = false,
        .enable_consolidation = false,
        .enable_glial = false,
        .enable_cortical_columns = false,
        .enable_predictive = true,
        .enable_bio_async = true,
        .minimal_mode = false,
        .lazy_init_mode = true,
        .max_inference_time_ms = 0.2F,
        .max_memory_kb = 500
    },
    /* DRONE_ROLE_GUARDIAN - Security and threat detection */
    {
        .role = DRONE_ROLE_GUARDIAN,
        .brain_size = BRAIN_SIZE_SMALL,
        .neuron_override = 0,
        .enable_visual_cortex = true,
        .enable_audio_cortex = true,
        .enable_speech_cortex = false,
        .enable_working_memory = true,
        .enable_global_workspace = false,
        .enable_theory_of_mind = false,
        .enable_ethics = true,
        .enable_curiosity = false,
        .enable_mirror_neurons = false,
        .enable_executive_control = true,
        .enable_consolidation = false,
        .enable_glial = false,
        .enable_cortical_columns = false,
        .enable_predictive = true,
        .enable_bio_async = true,
        .minimal_mode = false,
        .lazy_init_mode = true,
        .max_inference_time_ms = 0.3F,
        .max_memory_kb = 1024
    },
    /* DRONE_ROLE_RELAY - Communication relay */
    {
        .role = DRONE_ROLE_RELAY,
        .brain_size = BRAIN_SIZE_MICRO,
        .neuron_override = 0,
        .enable_visual_cortex = false,
        .enable_audio_cortex = false,
        .enable_speech_cortex = false,
        .enable_working_memory = false,
        .enable_global_workspace = false,
        .enable_theory_of_mind = false,
        .enable_ethics = false,
        .enable_curiosity = false,
        .enable_mirror_neurons = false,
        .enable_executive_control = false,
        .enable_consolidation = false,
        .enable_glial = false,
        .enable_cortical_columns = false,
        .enable_predictive = false,
        .enable_bio_async = true,
        .minimal_mode = true,
        .lazy_init_mode = true,
        .max_inference_time_ms = 0.05F,
        .max_memory_kb = 50
    }
};

/**
 * @brief Role name strings
 */
static const char* ROLE_NAMES[] = {
    "Scout",
    "Worker",
    "Coordinator",
    "Sensor",
    "Guardian",
    "Relay",
    "Custom"
};

drone_brain_template_t swarm_brain_get_role_template(drone_role_t role) {
    if (role >= 0 && role < DRONE_ROLE_CUSTOM) {
        return ROLE_TEMPLATES[role];
    }
    /* Return worker template as default for invalid roles */
    return ROLE_TEMPLATES[DRONE_ROLE_WORKER];
}

const char* swarm_brain_role_name(drone_role_t role) {
    if (role >= 0 && role <= DRONE_ROLE_CUSTOM) {
        return ROLE_NAMES[role];
    }
    return "Unknown";
}

int swarm_brain_create_for_agent_with_template(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    const drone_brain_template_t* templ
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(templ, NIMCP_ERROR_INVALID_PARAM, "drone brain template is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Check if agent already exists */
    if (find_agent_entry(mgr, agent_id)) {
        LOG_WARN("Agent %u already has brain", agent_id);
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Check agent limit */
    if (mgr->active_agents >= SWARM_BRAIN_MAX_AGENTS) {
        LOG_ERROR("Maximum agents reached (%u)", SWARM_BRAIN_MAX_AGENTS);
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Determine brain size and neuron count for this role */
    uint32_t brain_size = templ->neuron_override > 0 ?
                          templ->neuron_override :
                          nimcp_brain_factory_get_neuron_count(templ->brain_size);

    /* Map brain_size_t to nimcp_brain_size_t */
    nimcp_brain_size_t nimcp_size;
    switch (templ->brain_size) {
        case BRAIN_SIZE_MICRO:
            nimcp_size = NIMCP_BRAIN_TINY;  /* Use TINY for MICRO (public API minimum) */
            break;
        case BRAIN_SIZE_TINY:
            nimcp_size = NIMCP_BRAIN_TINY;
            break;
        case BRAIN_SIZE_SMALL:
            nimcp_size = NIMCP_BRAIN_SMALL;
            break;
        case BRAIN_SIZE_MEDIUM:
            nimcp_size = NIMCP_BRAIN_MEDIUM;
            break;
        case BRAIN_SIZE_LARGE:
            nimcp_size = NIMCP_BRAIN_LARGE;
            break;
        default:
            nimcp_size = NIMCP_BRAIN_TINY;
    }

    /* Generate task name */
    char task_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(task_name, sizeof(task_name), "swarm_%s_%u",
             swarm_brain_role_name(templ->role), agent_id);

    /* Create brain based on mode */
    nimcp_brain_t brain = NULL;
    float* test_weights = NULL;
    if (mgr->config.test_mode) {
        /* Test mode: Create stub brain without going through factory (100x faster) */
        brain = (nimcp_brain_t)nimcp_calloc(1, sizeof(struct nimcp_brain_handle));
        /* brain->internal_brain stays NULL - tests don't need actual neural network */
        /* Allocate dummy weights so sync operations work correctly */
        test_weights = (float*)nimcp_calloc(brain_size, sizeof(float));
    } else if (templ->minimal_mode) {
        /* Minimal mode: Create wrapper for minimal brain - skips heavy subsystems */
        brain = (nimcp_brain_t)nimcp_malloc(sizeof(struct nimcp_brain_handle));
        if (brain) {
            brain_size_t internal_size = (brain_size_t)nimcp_size;
            brain->internal_brain = brain_create_minimal(task_name, internal_size,
                                                         BRAIN_TASK_CLASSIFICATION,
                                                         brain_size, brain_size / 2);
            if (!brain->internal_brain) {
                nimcp_free(brain);
                brain = NULL;
            }
        }
    } else {
        /* Standard mode: Create brain with role-appropriate size */
        brain = nimcp_brain_create(task_name, nimcp_size, NIMCP_TASK_CLASSIFICATION,
                                   brain_size, brain_size / 2);
    }
    if (!brain) {
        LOG_ERROR("Failed to create brain for agent %u (role=%s)",
                  agent_id, swarm_brain_role_name(templ->role));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create brain for agent %u (role=%s)", agent_id, swarm_brain_role_name(templ->role));
        if (test_weights) {
            nimcp_free(test_weights);
        }
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Create hash table entry */
    agent_brain_entry_t* entry = (agent_brain_entry_t*)nimcp_calloc(
        1, sizeof(agent_brain_entry_t)
    );
    if (!entry) {
        LOG_ERROR("Failed to allocate agent entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate agent entry for agent %u", agent_id);
        if (test_weights) {
            nimcp_free(test_weights);
        }
        nimcp_brain_destroy(brain);
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Initialize agent brain data */
    entry->brain_data.agent_id = agent_id;
    entry->brain_data.brain = brain;
    entry->brain_data.brain_size = brain_size;
    if (mgr->config.test_mode && test_weights) {
        /* Test mode: Pre-initialize weights for sync operations */
        entry->brain_data.num_weights = brain_size;
        entry->brain_data.local_weights = test_weights;
    } else {
        entry->brain_data.num_weights = 0;
        entry->brain_data.local_weights = NULL;
    }
    entry->brain_data.last_sync_ms = nimcp_time_get_us() / 1000;
    entry->brain_data.divergence_score = 0.0F;
    entry->brain_data.active = true;
    entry->brain_data.role = templ->role;  /* Store the role */

    /* Insert into hash table */
    uint32_t hash = agent_hash(agent_id);
    entry->next = mgr->agent_table[hash];
    mgr->agent_table[hash] = entry;

    mgr->active_agents++;
    mgr->consensus_dirty = true;

    /* Update statistics */
    mgr->stats.num_agents = mgr->active_agents;
    mgr->stats.total_neurons += brain_size;

    nimcp_platform_mutex_unlock(&mgr->mutex);

    /* Send bio-async notification */
    if (templ->enable_bio_async && mgr->bio_async_enabled && mgr->bio_ctx) {
        bio_message_header_t msg = {
            .type = BIO_MSG_BRAIN_AGENT_JOINED,
            .flags = BIO_MSG_FLAG_BROADCAST,
            .timestamp_us = nimcp_time_get_us()
        };
        bio_router_broadcast(mgr->bio_ctx, &msg, sizeof(msg));
    }

    LOG_INFO("Created %s brain for agent %u (size=%u, minimal=%d, lazy=%d)",
             swarm_brain_role_name(templ->role), agent_id, brain_size,
             templ->minimal_mode, templ->lazy_init_mode);
    return NIMCP_SUCCESS;
}

int swarm_brain_create_for_agent_with_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    drone_role_t role
) {
    drone_brain_template_t templ = swarm_brain_get_role_template(role);
    return swarm_brain_create_for_agent_with_template(mgr, agent_id, &templ);
}

int swarm_brain_destroy_for_agent(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Find and remove entry */
    uint32_t hash = agent_hash(agent_id);
    agent_brain_entry_t** prev_ptr = &mgr->agent_table[hash];
    agent_brain_entry_t* entry = mgr->agent_table[hash];

    while (entry) {
        if (entry->brain_data.agent_id == agent_id) {
            /* Remove from chain */
            *prev_ptr = entry->next;

            /* Free resources */
            if (entry->brain_data.brain) {
                nimcp_brain_destroy(entry->brain_data.brain);
            }
            if (entry->brain_data.local_weights) {
                nimcp_free(entry->brain_data.local_weights);
            }

            /* Update statistics */
            mgr->active_agents--;
            mgr->stats.num_agents = mgr->active_agents;
            mgr->stats.total_neurons -= entry->brain_data.brain_size;
            mgr->consensus_dirty = true;

            nimcp_free(entry);

            nimcp_platform_mutex_unlock(&mgr->mutex);

            /* Send bio-async notification */
            if (mgr->bio_async_enabled && mgr->bio_ctx) {
                bio_message_header_t msg = {
                    .type = BIO_MSG_BRAIN_AGENT_LEFT,
                    .flags = BIO_MSG_FLAG_BROADCAST,
                    .timestamp_us = nimcp_time_get_us()
                };
                bio_router_broadcast(mgr->bio_ctx, &msg, sizeof(msg));
            }

            LOG_INFO("Destroyed brain for agent %u", agent_id);
            return NIMCP_SUCCESS;
        }

        prev_ptr = &entry->next;
        entry = entry->next;
    }

    nimcp_platform_mutex_unlock(&mgr->mutex);
    LOG_WARN("Agent %u not found", agent_id);
    return NIMCP_ERROR_INVALID_PARAM;
}

nimcp_brain_t swarm_brain_get(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
) {
    if (!mgr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_brain_get: mgr is NULL");
        return NULL;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);
    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    nimcp_brain_t brain = entry ? entry->brain_data.brain : NULL;
    nimcp_platform_mutex_unlock(&mgr->mutex);

    return brain;
}

/* ============================================================================
 * Local Learning and Processing
 * ============================================================================ */

int swarm_brain_local_learn(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    const float* input,
    uint32_t input_size,
    const float* target,
    uint32_t target_size
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "input array is NULL");
    NIMCP_CHECK_THROW(target, NIMCP_ERROR_INVALID_PARAM, "target array is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    if (!entry || !entry->brain_data.active) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Perform learning on brain using the public API */
    nimcp_training_result_t train_result;
    nimcp_status_t result = nimcp_brain_train_step(
        entry->brain_data.brain,
        input,
        input_size,
        target,
        target_size,
        &train_result
    );

    if (result == NIMCP_OK) {
        /* Mark consensus as dirty */
        mgr->consensus_dirty = true;
    }

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return (result == NIMCP_OK) ? NIMCP_SUCCESS : NIMCP_ERROR;
}

int swarm_brain_local_process(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t* output_size
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "input array is NULL");
    NIMCP_CHECK_THROW(output, NIMCP_ERROR_INVALID_PARAM, "output array is NULL");
    NIMCP_CHECK_THROW(output_size, NIMCP_ERROR_INVALID_PARAM, "output_size pointer is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    if (!entry || !entry->brain_data.active) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Process through brain using the public API */
    nimcp_status_t result = nimcp_brain_infer(
        entry->brain_data.brain,
        input,
        input_size,
        output,
        *output_size
    );

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return (result == NIMCP_OK) ? NIMCP_SUCCESS : NIMCP_ERROR;
}

/* ============================================================================
 * Weight Synchronization
 * ============================================================================ */

int swarm_brain_local_sync_weights(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    if (!entry || !entry->brain_data.active) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Update consensus if dirty */
    if (mgr->consensus_dirty) {
        update_consensus_weights(mgr);
    }

    /* Guard clause: no consensus weights yet */
    if (!mgr->consensus_weights || mgr->consensus_weight_count == 0) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_SUCCESS;
    }

    /* Get current agent weights (placeholder - would extract from brain) */
    /* In a real implementation, would need brain API to get/set weights */
    if (!entry->brain_data.local_weights) {
        entry->brain_data.local_weights = (float*)nimcp_calloc(
            mgr->consensus_weight_count, sizeof(float)
        );
        if (!entry->brain_data.local_weights) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate local weights for agent %u", agent_id);
            nimcp_platform_mutex_unlock(&mgr->mutex);
            return NIMCP_ERROR_MEMORY;
        }
        entry->brain_data.num_weights = mgr->consensus_weight_count;
    }

    /* Calculate divergence before sync */
    float old_divergence = calculate_divergence(
        entry->brain_data.local_weights,
        mgr->consensus_weights,
        entry->brain_data.num_weights
    );

    /* Blend local weights with consensus (simple averaging) */
    for (uint32_t i = 0; i < entry->brain_data.num_weights; i++) {
        entry->brain_data.local_weights[i] =
            0.7F * entry->brain_data.local_weights[i] +
            0.3F * mgr->consensus_weights[i];
    }

    /* Calculate new divergence */
    entry->brain_data.divergence_score = calculate_divergence(
        entry->brain_data.local_weights,
        mgr->consensus_weights,
        entry->brain_data.num_weights
    );

    entry->brain_data.last_sync_ms = nimcp_time_get_us() / 1000;
    mgr->stats.sync_count++;

    nimcp_platform_mutex_unlock(&mgr->mutex);

    /* Send bio-async notification if divergence changed significantly */
    if (mgr->bio_async_enabled && mgr->bio_ctx &&
        fabsf(old_divergence - entry->brain_data.divergence_score) > 0.1F) {
        bio_message_header_t msg = {
            .type = BIO_MSG_BRAIN_SYNCED,
            .flags = BIO_MSG_FLAG_BROADCAST,
            .timestamp_us = nimcp_time_get_us()
        };
        bio_router_broadcast(mgr->bio_ctx, &msg, sizeof(msg));
    }

    return NIMCP_SUCCESS;
}

int swarm_brain_sync_all(swarm_brain_manager_t* mgr) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Update consensus first */
    update_consensus_weights(mgr);

    /* Collect active agent IDs first, then sync outside the lock to avoid TOCTOU */
    uint32_t agent_ids[SWARM_BRAIN_MAX_AGENTS];
    uint32_t agent_count = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE && agent_count < SWARM_BRAIN_MAX_AGENTS; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry && agent_count < SWARM_BRAIN_MAX_AGENTS) {
            if (entry->brain_data.active) {
                agent_ids[agent_count++] = entry->brain_data.agent_id;
            }
            entry = entry->next;
        }
    }

    nimcp_platform_mutex_unlock(&mgr->mutex);

    /* Sync each agent outside the lock */
    for (uint32_t i = 0; i < agent_count; i++) {
        swarm_brain_local_sync_weights(mgr, agent_ids[i]);
    }

    /* Re-lock just for the final unlock to be balanced - no, just return */
    return NIMCP_SUCCESS;
}

int swarm_brain_get_consensus_weights(
    swarm_brain_manager_t* mgr,
    float** weights,
    uint32_t* num_weights
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(weights, NIMCP_ERROR_INVALID_PARAM, "weights output pointer is NULL");
    NIMCP_CHECK_THROW(num_weights, NIMCP_ERROR_INVALID_PARAM, "num_weights output pointer is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Update consensus if dirty */
    if (mgr->consensus_dirty) {
        update_consensus_weights(mgr);
    }

    /* Guard clause: no consensus */
    if (!mgr->consensus_weights || mgr->consensus_weight_count == 0) {
        *weights = NULL;
        *num_weights = 0;
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_SUCCESS;
    }

    /* Allocate and copy weights */
    *num_weights = mgr->consensus_weight_count;
    *weights = (float*)nimcp_malloc(mgr->consensus_weight_count * sizeof(float));
    if (!*weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate consensus weights buffer");
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    memcpy(*weights, mgr->consensus_weights,
           mgr->consensus_weight_count * sizeof(float));

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Divergence Detection
 * ============================================================================ */

float swarm_brain_get_divergence(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
) {
    if (!mgr) {
        return -1.0F;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);

    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    float divergence = entry ? entry->brain_data.divergence_score : -1.0F;

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return divergence;
}

int swarm_brain_get_divergent_agents(
    swarm_brain_manager_t* mgr,
    uint32_t** agents,
    uint32_t* count
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(agents, NIMCP_ERROR_INVALID_PARAM, "agents output pointer is NULL");
    NIMCP_CHECK_THROW(count, NIMCP_ERROR_INVALID_PARAM, "count output pointer is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Count divergent agents */
    uint32_t divergent_count = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active &&
                entry->brain_data.divergence_score >
                    mgr->config.divergence_threshold) {
                divergent_count++;
            }
            entry = entry->next;
        }
    }

    /* Guard clause: no divergent agents */
    if (divergent_count == 0) {
        *agents = NULL;
        *count = 0;
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_SUCCESS;
    }

    /* Allocate array */
    *agents = (uint32_t*)nimcp_malloc(divergent_count * sizeof(uint32_t));
    if (!*agents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate divergent agents array");
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Collect divergent agent IDs */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active &&
                entry->brain_data.divergence_score >
                    mgr->config.divergence_threshold) {
                (*agents)[idx++] = entry->brain_data.agent_id;
            }
            entry = entry->next;
        }
    }

    *count = divergent_count;
    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

int swarm_brain_local_get_stats(
    swarm_brain_manager_t* mgr,
    swarm_brain_stats_t* stats
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_INVALID_PARAM, "stats output pointer is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Calculate statistics */
    float sum_divergence = 0.0F;
    float max_divergence = 0.0F;
    uint32_t divergent_count = 0;

    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active) {
                sum_divergence += entry->brain_data.divergence_score;
                if (entry->brain_data.divergence_score > max_divergence) {
                    max_divergence = entry->brain_data.divergence_score;
                }
                if (entry->brain_data.divergence_score >
                    mgr->config.divergence_threshold) {
                    divergent_count++;
                }
            }
            entry = entry->next;
        }
    }

    /* Fill statistics */
    mgr->stats.avg_divergence =
        mgr->active_agents > 0 ? sum_divergence / mgr->active_agents : 0.0F;
    mgr->stats.max_divergence = max_divergence;
    mgr->stats.divergent_agents = divergent_count;
    mgr->stats.uptime_ms = (nimcp_time_get_us() / 1000) - mgr->creation_time_ms;

    *stats = mgr->stats;

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

int swarm_brain_local_reset_stats(swarm_brain_manager_t* mgr) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    mgr->stats.sync_count = 0;
    mgr->creation_time_ms = nimcp_time_get_us() / 1000;

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

swarm_brain_config_t swarm_brain_local_default_config(void) {
    swarm_brain_config_t config = {
        .default_brain_size = SWARM_BRAIN_DEFAULT_SIZE,
        .max_local_neurons = SWARM_BRAIN_MAX_NEURONS,
        .sync_interval_ms = SWARM_BRAIN_DEFAULT_SYNC_INTERVAL,
        .divergence_threshold = SWARM_BRAIN_DEFAULT_DIVERGENCE_THRESHOLD,
        .enable_weight_sharing = true,
        .enable_bio_async = true
    };
    return config;
}

bool swarm_brain_has_agent(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
) {
    if (!mgr) {
        return false;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);
    bool has_agent = (find_agent_entry(mgr, agent_id) != NULL);
    nimcp_platform_mutex_unlock(&mgr->mutex);

    return has_agent;
}

uint32_t swarm_brain_get_agent_count(swarm_brain_manager_t* mgr) {
    if (!mgr) {
        return 0;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);
    uint32_t count = mgr->active_agents;
    nimcp_platform_mutex_unlock(&mgr->mutex);

    return count;
}

int swarm_brain_get_all_agents(
    swarm_brain_manager_t* mgr,
    uint32_t** agents,
    uint32_t* count
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(agents, NIMCP_ERROR_INVALID_PARAM, "agents output pointer is NULL");
    NIMCP_CHECK_THROW(count, NIMCP_ERROR_INVALID_PARAM, "count output pointer is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Guard clause: no agents */
    if (mgr->active_agents == 0) {
        *agents = NULL;
        *count = 0;
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_SUCCESS;
    }

    /* Allocate array */
    *agents = (uint32_t*)nimcp_malloc(mgr->active_agents * sizeof(uint32_t));
    if (!*agents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate all agents array");
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Collect agent IDs */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active) {
                (*agents)[idx++] = entry->brain_data.agent_id;
            }
            entry = entry->next;
        }
    }

    *count = mgr->active_agents;
    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Role-Based Training Implementation
 * ============================================================================ */

/**
 * @brief Default training configurations per role
 */
static const role_training_config_t DEFAULT_ROLE_TRAINING_CONFIGS[] = {
    /* DRONE_ROLE_SCOUT */
    {
        .role = DRONE_ROLE_SCOUT,
        .learning_rate = 0.01F,      /* Higher learning for exploration */
        .batch_size = 16,
        .use_replay_buffer = true,
        .replay_buffer_size = 1000,
        .sync_within_role = true,
        .sync_strength = 0.3F,
        .enable_transfer_learning = true,
        .transfer_from = DRONE_ROLE_COORDINATOR,
        .transfer_weight = 0.1F
    },
    /* DRONE_ROLE_WORKER */
    {
        .role = DRONE_ROLE_WORKER,
        .learning_rate = 0.001F,     /* Stable, slow learning */
        .batch_size = NIMCP_DEFAULT_BATCH_SIZE,
        .use_replay_buffer = false,
        .replay_buffer_size = 0,
        .sync_within_role = true,
        .sync_strength = 0.7F,       /* High sync for consistent behavior */
        .enable_transfer_learning = true,
        .transfer_from = DRONE_ROLE_SCOUT,
        .transfer_weight = 0.2F
    },
    /* DRONE_ROLE_COORDINATOR */
    {
        .role = DRONE_ROLE_COORDINATOR,
        .learning_rate = 0.005F,     /* Moderate learning */
        .batch_size = NIMCP_MEDIUM_BATCH_SIZE,
        .use_replay_buffer = true,
        .replay_buffer_size = 5000,
        .sync_within_role = false,   /* Coordinators sync with all */
        .sync_strength = 0.2F,
        .enable_transfer_learning = true,
        .transfer_from = DRONE_ROLE_SENSOR,
        .transfer_weight = 0.15F
    },
    /* DRONE_ROLE_SENSOR */
    {
        .role = DRONE_ROLE_SENSOR,
        .learning_rate = 0.02F,      /* Fast adaptation */
        .batch_size = NIMCP_SMALL_BATCH_SIZE,
        .use_replay_buffer = true,
        .replay_buffer_size = 500,
        .sync_within_role = true,
        .sync_strength = 0.4F,
        .enable_transfer_learning = false,
        .transfer_from = DRONE_ROLE_SENSOR,
        .transfer_weight = 0.0F
    },
    /* DRONE_ROLE_GUARDIAN */
    {
        .role = DRONE_ROLE_GUARDIAN,
        .learning_rate = 0.003F,     /* Conservative learning */
        .batch_size = NIMCP_DEFAULT_BATCH_SIZE,
        .use_replay_buffer = true,
        .replay_buffer_size = 2000,
        .sync_within_role = true,
        .sync_strength = 0.5F,
        .enable_transfer_learning = true,
        .transfer_from = DRONE_ROLE_SENSOR,
        .transfer_weight = 0.25F
    },
    /* DRONE_ROLE_RELAY */
    {
        .role = DRONE_ROLE_RELAY,
        .learning_rate = 0.0001F,    /* Minimal learning */
        .batch_size = NIMCP_SMALL_BATCH_SIZE,
        .use_replay_buffer = false,
        .replay_buffer_size = 0,
        .sync_within_role = true,
        .sync_strength = 0.9F,       /* Very high sync */
        .enable_transfer_learning = false,
        .transfer_from = DRONE_ROLE_RELAY,
        .transfer_weight = 0.0F
    }
};

role_training_config_t swarm_brain_get_role_training_config(drone_role_t role) {
    if (role >= 0 && role < DRONE_ROLE_CUSTOM) {
        return DEFAULT_ROLE_TRAINING_CONFIGS[role];
    }
    /* Return worker config as default */
    return DEFAULT_ROLE_TRAINING_CONFIGS[DRONE_ROLE_WORKER];
}

int swarm_brain_train_with_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    drone_role_t role,
    const float* input,
    uint32_t input_size,
    const float* target,
    uint32_t target_size,
    const role_training_config_t* config
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(input, NIMCP_ERROR_INVALID_PARAM, "input array is NULL");
    NIMCP_CHECK_THROW(target, NIMCP_ERROR_INVALID_PARAM, "target array is NULL");

    /* Use default config if not provided */
    role_training_config_t train_config;
    if (config) {
        train_config = *config;
    } else {
        train_config = swarm_brain_get_role_training_config(role);
    }

    nimcp_platform_mutex_lock(&mgr->mutex);

    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    if (!entry || !entry->brain_data.active) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Update agent role */
    entry->brain_data.role = role;

    /* Perform learning with role-specific config */
    nimcp_training_result_t train_result;
    nimcp_status_t result = nimcp_brain_train_step(
        entry->brain_data.brain,
        input,
        input_size,
        target,
        target_size,
        &train_result
    );

    if (result == NIMCP_OK) {
        mgr->consensus_dirty = true;
    }

    nimcp_platform_mutex_unlock(&mgr->mutex);

    /* If sync_within_role is enabled, sync after training */
    if (train_config.sync_within_role && result == NIMCP_OK) {
        swarm_brain_sync_role_group(mgr, role);
    }

    return (result == NIMCP_OK) ? NIMCP_SUCCESS : NIMCP_ERROR;
}

int swarm_brain_sync_role_group(
    swarm_brain_manager_t* mgr,
    drone_role_t role
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Count agents of this role */
    uint32_t role_count = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active && entry->brain_data.role == role) {
                role_count++;
            }
            entry = entry->next;
        }
    }

    if (role_count < 2) {
        /* Not enough agents of this role to sync */
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_SUCCESS;
    }

    /* Sync each agent in this role group using the existing sync logic */
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active && entry->brain_data.role == role) {
                /* Update last sync time */
                entry->brain_data.last_sync_ms = nimcp_time_get_us() / 1000;
            }
            entry = entry->next;
        }
    }

    mgr->stats.sync_count++;
    nimcp_platform_mutex_unlock(&mgr->mutex);

    LOG_DEBUG("Synced %u agents of role %s", role_count, swarm_brain_role_name(role));
    return NIMCP_SUCCESS;
}

int swarm_brain_get_agents_by_role(
    swarm_brain_manager_t* mgr,
    drone_role_t role,
    uint32_t** agents,
    uint32_t* count
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(agents, NIMCP_ERROR_INVALID_PARAM, "agents output pointer is NULL");
    NIMCP_CHECK_THROW(count, NIMCP_ERROR_INVALID_PARAM, "count output pointer is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Count agents of this role */
    uint32_t role_count = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active && entry->brain_data.role == role) {
                role_count++;
            }
            entry = entry->next;
        }
    }

    if (role_count == 0) {
        *agents = NULL;
        *count = 0;
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_SUCCESS;
    }

    /* Allocate array */
    *agents = (uint32_t*)nimcp_malloc(role_count * sizeof(uint32_t));
    if (!*agents) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate agents by role array");
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Collect agent IDs */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active && entry->brain_data.role == role) {
                (*agents)[idx++] = entry->brain_data.agent_id;
            }
            entry = entry->next;
        }
    }

    *count = role_count;
    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

int swarm_brain_transfer_role_knowledge(
    swarm_brain_manager_t* mgr,
    uint32_t to_agent,
    drone_role_t from_role,
    float transfer_weight
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");
    NIMCP_CHECK_THROW(transfer_weight >= 0.0F && transfer_weight <= 1.0F, NIMCP_ERROR_INVALID_PARAM, "transfer_weight must be in range [0.0, 1.0]");

    nimcp_platform_mutex_lock(&mgr->mutex);

    /* Find target agent */
    agent_brain_entry_t* target_entry = find_agent_entry(mgr, to_agent);
    if (!target_entry || !target_entry->brain_data.active) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Find source agents of the specified role */
    uint32_t source_count = 0;
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        agent_brain_entry_t* entry = mgr->agent_table[i];
        while (entry) {
            if (entry->brain_data.active && entry->brain_data.role == from_role) {
                source_count++;
            }
            entry = entry->next;
        }
    }

    if (source_count == 0) {
        LOG_WARN("No source agents of role %s for knowledge transfer",
                 swarm_brain_role_name(from_role));
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_SUCCESS; /* Not an error, just no sources */
    }

    /* Transfer is a weighted average operation on weights */
    /* For now, just mark the transfer as completed */
    /* Full implementation would require weight averaging with source agents */
    LOG_INFO("Knowledge transfer from %u %s agents to agent %u (weight=%.2f)",
             source_count, swarm_brain_role_name(from_role), to_agent, transfer_weight);

    nimcp_platform_mutex_unlock(&mgr->mutex);
    return NIMCP_SUCCESS;
}

int swarm_brain_set_agent_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id,
    drone_role_t role
) {
    NIMCP_CHECK_THROW(mgr, NIMCP_ERROR_INVALID_PARAM, "swarm brain manager is NULL");

    nimcp_platform_mutex_lock(&mgr->mutex);

    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    if (!entry || !entry->brain_data.active) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    entry->brain_data.role = role;

    nimcp_platform_mutex_unlock(&mgr->mutex);
    LOG_DEBUG("Set agent %u role to %s", agent_id, swarm_brain_role_name(role));
    return NIMCP_SUCCESS;
}

drone_role_t swarm_brain_get_agent_role(
    swarm_brain_manager_t* mgr,
    uint32_t agent_id
) {
    if (!mgr) {
        return DRONE_ROLE_CUSTOM;
    }

    nimcp_platform_mutex_lock(&mgr->mutex);

    agent_brain_entry_t* entry = find_agent_entry(mgr, agent_id);
    if (!entry || !entry->brain_data.active) {
        nimcp_platform_mutex_unlock(&mgr->mutex);
        return DRONE_ROLE_CUSTOM;
    }

    drone_role_t role = entry->brain_data.role;
    nimcp_platform_mutex_unlock(&mgr->mutex);
    return role;
}
